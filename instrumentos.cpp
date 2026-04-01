#include "instrumentos.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <math.h>     
#include <orbis/Pad.h>

extern int globalPadHandle;
extern MenuLevel menuAtual;

// ==========================================
// VARIÁVEIS DO SINTETIZADOR DO PIANO
// ==========================================
// Agora são globais para a Thread de Áudio alimentar a Tela
static volatile bool pianoKeysPressionadas[13] = { false };
static volatile int oitavaOffset = 0; // 0 = Padrão, -1 = Grave, +1 = Agudo

static float pianoFases[13] = { 0.0f };
static float volumeTecla[13] = { 0.0f }; // Sistema Anti-Click e Responsividade

// Frequências exatas das 13 notas base de Dó (C4) a Dó Maior (C5)
static const float pianoFreqsBase[13] = {
    261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f,
    369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f, 523.25f
};

void renderizarInstrumentos(uint32_t* p) {
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;

    desenharTexto(p, "PIANO VIRTUAL HYPER NEIVA", 40, 50, 50, 0xFF00AAFF);
    desenharTexto(p, "Pressione [OPTIONS] para Sair do Piano", 25, 50, 100, 0xFFAAAAAA);

    if (globalPadHandle < 0) {
        desenharTexto(p, "Controle nao detectado!", 30, 50, 200, 0xFF0000FF);
        return;
    }

    // Mostra na tela a escala musical atual
    char txtOitava[128];
    sprintf(txtOitava, "Escala Musical Atual: Oitava %d (Mova o Analogico L3 Esq/Dir para mudar)", 4 + oitavaOffset);
    desenharTexto(p, txtOitava, 30, 50, 170, 0xFF00FF00);

    // NOTA: A leitura do controle (scePadReadState) foi removida daqui!
    // A Thread de Áudio faz a leitura em tempo real para ter ZERO LATÊNCIA.
    // Nós apenas lemos o array pianoKeysPressionadas[] para desenhar a UI.

    int startX = 360;
    int startY = 300;
    int whiteW = 150;
    int whiteH = 500;
    int blackW = 80;
    int blackH = 300;

    int whiteMap[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
    const char* whiteLabels[8] = { "[ESQ]", "[BAIXO]", "[DIR]", "[CIMA]", "[QUAD]", "[ X ]", "[BOLA]", "[R3]" };
    const char* whiteNotes[8] = { "DO", "RE", "MI", "FA", "SOL", "LA", "SI", "DO" };

    // 1º DESENHAR AS TECLAS BRANCAS
    for (int i = 0; i < 8; i++) {
        int keyIndex = whiteMap[i];
        bool isPressed = pianoKeysPressionadas[keyIndex];
        uint32_t color = isPressed ? 0xFF00AAFF : 0xFFEEEEEE;
        uint32_t borderColor = 0xFF555555;

        int kX = startX + (i * whiteW);

        for (int y = 0; y < whiteH; y++) {
            for (int x = 0; x < whiteW; x++) {
                if (x == 0 || x == whiteW - 1 || y == 0 || y == whiteH - 1) {
                    p[(startY + y) * 1920 + (kX + x)] = borderColor;
                }
                else {
                    p[(startY + y) * 1920 + (kX + x)] = color;
                }
            }
        }

        desenharTexto(p, whiteNotes[i], 30, kX + 50, startY + whiteH - 80, isPressed ? 0xFFFFFFFF : 0xFF000000);
        desenharTexto(p, whiteLabels[i], 20, kX + 40, startY + whiteH - 30, isPressed ? 0xFFFFFFFF : 0xFF555555);
    }

    int blackMap[5] = { 1, 3, 6, 8, 10 };
    int blackOffsets[5] = { 1, 2, 4, 5, 6 };
    const char* blackLabels[5] = { "[L2]", "[L1]", "[R1]", "[R2]", "[TRI]" };

    // 2º DESENHAR AS TECLAS PRETAS
    for (int i = 0; i < 5; i++) {
        int keyIndex = blackMap[i];
        bool isPressed = pianoKeysPressionadas[keyIndex];
        uint32_t color = isPressed ? 0xFF00AAFF : 0xFF222222;

        int kX = startX + (blackOffsets[i] * whiteW) - (blackW / 2);

        for (int y = 0; y < blackH; y++) {
            for (int x = 0; x < blackW; x++) {
                p[(startY + y) * 1920 + (kX + x)] = color;
            }
        }

        desenharTexto(p, blackLabels[i], 18, kX + 10, startY + blackH - 30, 0xFFFFFFFF);
    }

    desenharTexto(p, "Dica: Aperte os botoes indicados nas teclas para tocar!", 25, 600, 900, 0xFF00FF00);
}

// =======================================================
// GERADOR DE ÁUDIO DO PIANO VIRTUAL (Thread Independente)
// =======================================================
void misturarAudioPiano(int16_t* bufferAudio, size_t frames) {
    if (menuAtual != MENU_INSTRUMENTOS) {
        for (int k = 0; k < 13; k++) {
            pianoKeysPressionadas[k] = false;
            volumeTecla[k] = 0.0f;
            pianoFases[k] = 0.0f;
        }
        return;
    }

    // LEITURA DIRETA DO HARDWARE NA THREAD DE ÁUDIO (ZERO LATÊNCIA)
    if (globalPadHandle >= 0) {
        OrbisPadData data;
        if (scePadReadState(globalPadHandle, &data) == 0) {
            uint32_t b = data.buttons;

            pianoKeysPressionadas[0] = (b & ORBIS_PAD_BUTTON_LEFT);
            pianoKeysPressionadas[1] = (b & ORBIS_PAD_BUTTON_L2);
            pianoKeysPressionadas[2] = (b & ORBIS_PAD_BUTTON_DOWN);
            pianoKeysPressionadas[3] = (b & ORBIS_PAD_BUTTON_L1);
            pianoKeysPressionadas[4] = (b & ORBIS_PAD_BUTTON_RIGHT);
            pianoKeysPressionadas[5] = (b & ORBIS_PAD_BUTTON_UP);
            pianoKeysPressionadas[6] = (b & ORBIS_PAD_BUTTON_R1);
            pianoKeysPressionadas[7] = (b & ORBIS_PAD_BUTTON_SQUARE);
            pianoKeysPressionadas[8] = (b & ORBIS_PAD_BUTTON_R2);
            pianoKeysPressionadas[9] = (b & ORBIS_PAD_BUTTON_CROSS);
            pianoKeysPressionadas[10] = (b & ORBIS_PAD_BUTTON_TRIANGLE);
            pianoKeysPressionadas[11] = (b & ORBIS_PAD_BUTTON_CIRCLE);
            pianoKeysPressionadas[12] = (b & ORBIS_PAD_BUTTON_R3);

            // Leitura bruta da memória do Analógico L3 para as Oitavas
            uint8_t* raw = (uint8_t*)&data;
            uint8_t lx = raw[0x04];

            // Cooldown para a troca de oitava não enlouquecer com a rapidez da Thread
            static int cooldownAnalogo = 0;
            if (cooldownAnalogo > 0) {
                cooldownAnalogo--;
            }
            else {
                if (lx < 50 && oitavaOffset > -2) { // Vai até a oitava 2
                    oitavaOffset--;
                    cooldownAnalogo = 40; // Trava a mudança por 1/4 de segundo
                }
                else if (lx > 200 && oitavaOffset < 3) { // Vai até a oitava 7
                    oitavaOffset++;
                    cooldownAnalogo = 40;
                }
            }
        }
    }

    // Calcula a frequência real baseada na Escala Musical escolhida
    float multiplicadorEscala = powf(2.0f, (float)oitavaOffset);

    for (size_t i = 0; i < frames; i++) {
        float samplePiano = 0.0f;

        for (int k = 0; k < 13; k++) {

            // SISTEMA DE ENVELOPE (Attack / Release) 
            // Resolve o engasgo e estalos de apertar rápido demais
            if (pianoKeysPressionadas[k]) {
                volumeTecla[k] += 0.005f; // Sobe rápido (Attack perfeito)
                if (volumeTecla[k] > 1.0f) volumeTecla[k] = 1.0f;
            }
            else {
                volumeTecla[k] -= 0.002f; // Desce suave (Release sem cortes abruptos)
                if (volumeTecla[k] < 0.0f) {
                    volumeTecla[k] = 0.0f;
                    pianoFases[k] = 0.0f; // Zera a onda quando o som morre totalmente
                }
            }

            if (volumeTecla[k] > 0.0f) {
                float freqAtual = pianoFreqsBase[k] * multiplicadorEscala;

                // Aplica o volume suave na onda
                samplePiano += sinf(pianoFases[k]) * (4000.0f * volumeTecla[k]);

                // Avança a onda matemática no tempo
                pianoFases[k] += (2.0f * 3.1415926535f * freqAtual) / 48000.0f;
                if (pianoFases[k] > 2.0f * 3.1415926535f) {
                    pianoFases[k] -= 2.0f * 3.1415926535f;
                }
            }
        }

        if (samplePiano != 0.0f) {
            int32_t mixedL = bufferAudio[i * 2] + (int32_t)samplePiano;
            int32_t mixedR = bufferAudio[i * 2 + 1] + (int32_t)samplePiano;

            // Limitador final 16-bits (Anti-Distorção)
            if (mixedL > 32767) mixedL = 32767;
            else if (mixedL < -32768) mixedL = -32768;
            if (mixedR > 32767) mixedR = 32767;
            else if (mixedR < -32768) mixedR = -32768;

            bufferAudio[i * 2] = (int16_t)mixedL;
            bufferAudio[i * 2 + 1] = (int16_t)mixedR;
        }
    }
}