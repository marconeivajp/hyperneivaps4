#include "instrumentos.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   // Para o rand() do Violão
#include <math.h>     
#include <orbis/Pad.h>

extern int globalPadHandle;
extern MenuLevel menuAtual;

// ==========================================
// VARIÁVEIS COMPARTILHADAS (Troca de Modo)
// ==========================================
// 0 = Piano Virtual | 1 = Violão The Last of Us
static volatile int modoInstrumento = 0;
static bool l3PressionadoAnteriormente = false;

// ==========================================
// VARIÁVEIS DO SINTETIZADOR DO PIANO
// ==========================================
static volatile bool pianoKeysPressionadas[13] = { false };
static volatile int oitavaOffset = 0;
static float pianoFases[13] = { 0.0f };
static float volumeTecla[13] = { 0.0f };
static const float pianoFreqsBase[13] = {
    261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f,
    369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f, 523.25f
};

// ==========================================
// VARIÁVEIS DO SINTETIZADOR DO VIOLÃO (KARPLUS-STRONG)
// ==========================================
static volatile int acordeAtual = 0; // Padrão: 0 (Em)

// As mesmas frequências reais de um violão afinado em E Standard para cada Acorde de TLOU2
// Matriz: [Acorde][Corda de 6 a 1 (Grave para Agudo)]
static const float frequenciasViolao[6][6] = {
    { 82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f },  // 0: Em (Mi Menor)
    { 130.81f, 164.81f, 196.00f, 261.63f, 329.63f, 392.00f }, // 1: C (Dó Maior)
    { 98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f },  // 2: G (Sol Maior)
    { 110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f }, // 3: D (Ré Maior)
    { 110.00f, 164.81f, 220.00f, 261.63f, 329.63f, 440.00f }, // 4: Am (Lá Menor)
    { 123.47f, 185.00f, 246.94f, 293.66f, 369.99f, 493.88f }  // 5: Bm (Si Menor)
};
static const char* nomeAcordes[6] = { "Em", "C", "G", "D", "Am", "Bm" };

// Linhas de atraso físico para a vibração das 6 cordas
static float ks_delayLines[6][2048];
static int ks_delayPtr[6] = { 0 };
static int ks_delayLength[6] = { 0 };
static volatile int ks_strumTimer[6] = { -1, -1, -1, -1, -1, -1 };


void renderizarInstrumentos(uint32_t* p) {
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;

    desenharTexto(p, "SISTEMA DE INSTRUMENTOS VIRTUAIS", 40, 50, 50, 0xFF00AAFF);
    desenharTexto(p, "[L3] Alternar Piano/Violao  |  [OPTIONS] Sair", 25, 50, 100, 0xFFAAAAAA);

    if (globalPadHandle < 0) {
        desenharTexto(p, "Controle nao detectado!", 30, 50, 200, 0xFF0000FF);
        return;
    }

    // =========================================================================
    // MODO PIANO
    // =========================================================================
    if (modoInstrumento == 0) {
        char txtOitava[128];
        sprintf(txtOitava, "Escala Musical Atual: Oitava %d (Mova o Analogico Esquerdo para mudar)", 4 + oitavaOffset);
        desenharTexto(p, txtOitava, 30, 50, 170, 0xFF00FF00);

        int startX = 360; int startY = 300;
        int whiteW = 150; int whiteH = 500; int blackW = 80; int blackH = 300;

        int whiteMap[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
        const char* whiteLabels[8] = { "[ESQ]", "[BAIXO]", "[DIR]", "[CIMA]", "[QUAD]", "[ X ]", "[BOLA]", "[R3]" };
        const char* whiteNotes[8] = { "DO", "RE", "MI", "FA", "SOL", "LA", "SI", "DO" };

        for (int i = 0; i < 8; i++) {
            int keyIndex = whiteMap[i];
            bool isPressed = pianoKeysPressionadas[keyIndex];
            uint32_t color = isPressed ? 0xFF00AAFF : 0xFFEEEEEE;
            uint32_t borderColor = 0xFF555555;
            int kX = startX + (i * whiteW);

            for (int y = 0; y < whiteH; y++) {
                for (int x = 0; x < whiteW; x++) {
                    if (x == 0 || x == whiteW - 1 || y == 0 || y == whiteH - 1) p[(startY + y) * 1920 + (kX + x)] = borderColor;
                    else p[(startY + y) * 1920 + (kX + x)] = color;
                }
            }
            desenharTexto(p, whiteNotes[i], 30, kX + 50, startY + whiteH - 80, isPressed ? 0xFFFFFFFF : 0xFF000000);
            desenharTexto(p, whiteLabels[i], 20, kX + 40, startY + whiteH - 30, isPressed ? 0xFFFFFFFF : 0xFF555555);
        }

        int blackMap[5] = { 1, 3, 6, 8, 10 };
        int blackOffsets[5] = { 1, 2, 4, 5, 6 };
        const char* blackLabels[5] = { "[L2]", "[L1]", "[R1]", "[R2]", "[TRI]" };

        for (int i = 0; i < 5; i++) {
            int keyIndex = blackMap[i];
            bool isPressed = pianoKeysPressionadas[keyIndex];
            uint32_t color = isPressed ? 0xFF00AAFF : 0xFF222222;
            int kX = startX + (blackOffsets[i] * whiteW) - (blackW / 2);

            for (int y = 0; y < blackH; y++) {
                for (int x = 0; x < blackW; x++) p[(startY + y) * 1920 + (kX + x)] = color;
            }
            desenharTexto(p, blackLabels[i], 18, kX + 10, startY + blackH - 30, 0xFFFFFFFF);
        }
    }
    // =========================================================================
    // MODO VIOLÃO (THE LAST OF US)
    // =========================================================================
    else {
        desenharTexto(p, "MAO ESQUERDA: Analogico (L3) escolhe o Acorde", 25, 100, 200, 0xFF00FF00);

        // Desenhar a Roda de Acordes Mapeada do TLOU2
        int cx = 350; int cy = 550; int radius = 220;
        int boxW = 120; int boxH = 60;

        int boxX[6] = {
            cx - boxW / 2,                                // Em (Topo)
            cx + (int)(radius * 0.866f) - boxW / 2,       // C (Topo-Direita)
            cx + (int)(radius * 0.866f) - boxW / 2,       // G (Baixo-Direita)
            cx - boxW / 2,                                // D (Baixo)
            cx - (int)(radius * 0.866f) - boxW / 2,       // Am (Baixo-Esquerda)
            cx - (int)(radius * 0.866f) - boxW / 2        // Bm (Topo-Esquerda)
        };
        int boxY[6] = {
            cy - radius - boxH / 2,                       // Em
            cy - (int)(radius * 0.5f) - boxH / 2,         // C
            cy + (int)(radius * 0.5f) - boxH / 2,         // G
            cy + radius - boxH / 2,                       // D
            cy + (int)(radius * 0.5f) - boxH / 2,         // Am
            cy - (int)(radius * 0.5f) - boxH / 2          // Bm
        };

        for (int i = 0; i < 6; i++) {
            uint32_t color = (acordeAtual == i) ? 0xFF00AAFF : 0xFF333333; // Fica Azul se selecionado
            for (int y = 0; y < boxH; y++) {
                for (int x = 0; x < boxW; x++) {
                    p[(boxY[i] + y) * 1920 + (boxX[i] + x)] = color;
                }
            }
            desenharTexto(p, nomeAcordes[i], 30, boxX[i] + 35, boxY[i] + 40, 0xFFFFFFFF);
        }

        desenharTexto(p, "MAO DIREITA: Palhetada e Dedilhado", 30, 1000, 250, 0xFF00AAFF);
        desenharTexto(p, "Mova o Analogico Direito (R3) para Cima/Baixo para palhetar!", 20, 1000, 300, 0xFFFFFFFF);

        desenharTexto(p, "Para dedilhar cordas individuais:", 25, 1000, 400, 0xFF00AAFF);
        desenharTexto(p, "[ L2 ]       - Corda 6 (Grave)", 20, 1000, 450, 0xFFEEEEEE);
        desenharTexto(p, "[ L1 ]       - Corda 5", 20, 1000, 500, 0xFFEEEEEE);
        desenharTexto(p, "[ R1 ]       - Corda 4", 20, 1000, 550, 0xFFEEEEEE);
        desenharTexto(p, "[ R2 ]       - Corda 3", 20, 1000, 600, 0xFFEEEEEE);
        desenharTexto(p, "[QUADRADO]   - Corda 2", 20, 1000, 650, 0xFFEEEEEE);
        desenharTexto(p, "[  X  ]      - Corda 1 (Aguda)", 20, 1000, 700, 0xFFEEEEEE);
    }
}

// =======================================================
// GERADOR DE ÁUDIO UNIFICADO (Thread Independente)
// =======================================================
void misturarAudioPiano(int16_t* bufferAudio, size_t frames) {
    if (menuAtual != MENU_INSTRUMENTOS) {
        for (int k = 0; k < 13; k++) {
            pianoKeysPressionadas[k] = false; volumeTecla[k] = 0.0f; pianoFases[k] = 0.0f;
        }
        for (int k = 0; k < 6; k++) ks_delayLength[k] = 0;
        return;
    }

    // LÊ O CONTROLE COM ZERO LATÊNCIA NA THREAD DE ÁUDIO
    if (globalPadHandle >= 0) {
        OrbisPadData data;
        if (scePadReadState(globalPadHandle, &data) == 0) {
            uint32_t b = data.buttons;

            // Troca de Instrumento usando L3
            bool l3Atual = (b & ORBIS_PAD_BUTTON_L3);
            if (l3Atual && !l3PressionadoAnteriormente) {
                modoInstrumento = (modoInstrumento == 0) ? 1 : 0;
            }
            l3PressionadoAnteriormente = l3Atual;

            // Leitura bruta dos Analógicos (Mesmo padrão para todo DualShock 4)
            uint8_t* raw = (uint8_t*)&data;
            uint8_t lx = raw[0x04]; uint8_t ly = raw[0x05];
            uint8_t rx = raw[0x06]; uint8_t ry = raw[0x07];

            if (modoInstrumento == 0) {
                // ========================== LEITURA DO PIANO ==========================
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

                static int cooldownAnalogo = 0;
                if (cooldownAnalogo > 0) cooldownAnalogo--;
                else {
                    if (lx < 50 && oitavaOffset > -2) { oitavaOffset--; cooldownAnalogo = 40; }
                    else if (lx > 200 && oitavaOffset < 3) { oitavaOffset++; cooldownAnalogo = 40; }
                }
            }
            else {
                // ========================== LEITURA DO VIOLÃO ==========================
                // 1. Mão Esquerda (Roda de Acordes no L3)
                int dx = lx - 128; int dy = ly - 128;
                if (dx * dx + dy * dy > 2500) { // Só muda o acorde se inclinar o analógico além do centro
                    float angle = atan2f((float)dy, (float)dx) * 180.0f / 3.14159f;
                    if (angle > -120 && angle <= -60) acordeAtual = 0;      // Topo
                    else if (angle > -60 && angle <= 0) acordeAtual = 1;    // Topo-Dir
                    else if (angle > 0 && angle <= 60) acordeAtual = 2;     // Baixo-Dir
                    else if (angle > 60 && angle <= 120) acordeAtual = 3;   // Baixo
                    else if (angle > 120 && angle <= 180) acordeAtual = 4;  // Baixo-Esq
                    else acordeAtual = 5;                                   // Topo-Esq
                }

                // 2. Mão Direita: Palhetada Completa (R3)
                static uint8_t last_ry = 128;
                if (ry > 180 && last_ry <= 180) {
                    // Palhetada para Baixo (Strumming) - Atraso de 5ms entre cada corda
                    for (int i = 0; i < 6; i++) ks_strumTimer[i] = i * 250;
                }
                else if (ry < 70 && last_ry >= 70) {
                    // Palhetada para Cima (De baixo pra cima)
                    for (int i = 0; i < 6; i++) ks_strumTimer[5 - i] = i * 250;
                }
                last_ry = ry;

                // 3. Mão Direita: Dedilhado Individual (Botões)
                // CORREÇÃO APLICADA AQUI: Cast implícito evitado com "!= 0"
                static bool last_fp_keys[6] = { false };
                bool fp_keys[6] = {
                    (b & ORBIS_PAD_BUTTON_L2) != 0,
                    (b & ORBIS_PAD_BUTTON_L1) != 0,
                    (b & ORBIS_PAD_BUTTON_R1) != 0,
                    (b & ORBIS_PAD_BUTTON_R2) != 0,
                    (b & ORBIS_PAD_BUTTON_SQUARE) != 0,
                    (b & ORBIS_PAD_BUTTON_CROSS) != 0
                };
                for (int i = 0; i < 6; i++) {
                    if (fp_keys[i] && !last_fp_keys[i]) {
                        ks_strumTimer[i] = 1; // Dispara a corda instantaneamente
                    }
                    last_fp_keys[i] = fp_keys[i];
                }
            }
        }
    }

    // =========================================================================
    // INJETANDO O SOM NO BUFFER
    // =========================================================================
    float multiplicadorEscalaPiano = powf(2.0f, (float)oitavaOffset);

    for (size_t i = 0; i < frames; i++) {
        float mixTotal = 0.0f;

        if (modoInstrumento == 0) {
            // MATEMÁTICA DO PIANO
            for (int k = 0; k < 13; k++) {
                if (pianoKeysPressionadas[k]) {
                    volumeTecla[k] += 0.005f; if (volumeTecla[k] > 1.0f) volumeTecla[k] = 1.0f;
                }
                else {
                    volumeTecla[k] -= 0.002f; if (volumeTecla[k] < 0.0f) { volumeTecla[k] = 0.0f; pianoFases[k] = 0.0f; }
                }
                if (volumeTecla[k] > 0.0f) {
                    float freqAtual = pianoFreqsBase[k] * multiplicadorEscalaPiano;
                    mixTotal += sinf(pianoFases[k]) * (4000.0f * volumeTecla[k]);
                    pianoFases[k] += (2.0f * 3.1415926535f * freqAtual) / 48000.0f;
                    if (pianoFases[k] > 2.0f * 3.1415926535f) pianoFases[k] -= 2.0f * 3.1415926535f;
                }
            }
        }
        else {
            // MATEMÁTICA DO VIOLÃO (KARPLUS-STRONG ALGORITHM)
            for (int k = 0; k < 6; k++) {

                // Dispara a corda (Batida da Palheta)
                if (ks_strumTimer[k] > 0) {
                    ks_strumTimer[k]--;
                    if (ks_strumTimer[k] == 0) {
                        int len = (int)(48000.0f / frequenciasViolao[acordeAtual][k]);
                        if (len > 2047) len = 2047;
                        ks_delayLength[k] = len;
                        ks_delayPtr[k] = 0;
                        // Cria a vibração inicial injetando ruído branco puríssimo
                        for (int j = 0; j < len; j++) {
                            ks_delayLines[k][j] = ((rand() % 10000) / 5000.0f - 1.0f);
                        }
                    }
                }

                // Ecoa e Filtra o Som (Simula a vibração orgânica da corda perdendo força)
                if (ks_delayLength[k] > 0) {
                    int ptr = ks_delayPtr[k];
                    int prevPtr = (ptr - 1 < 0) ? (ks_delayLength[k] - 1) : (ptr - 1);

                    float current = ks_delayLines[k][ptr];
                    float prev = ks_delayLines[k][prevPtr];

                    // Decaimento natural da corda (Filter)
                    float newVal = 0.998f * 0.5f * (current + prev);

                    ks_delayLines[k][ptr] = newVal;
                    ks_delayPtr[k] = (ptr + 1) % ks_delayLength[k];

                    mixTotal += newVal * 2500.0f; // Amplifica o som final
                }
            }
        }

        if (mixTotal != 0.0f) {
            int32_t mixedL = bufferAudio[i * 2] + (int32_t)mixTotal;
            int32_t mixedR = bufferAudio[i * 2 + 1] + (int32_t)mixTotal;

            if (mixedL > 32767) mixedL = 32767; else if (mixedL < -32768) mixedL = -32768;
            if (mixedR > 32767) mixedR = 32767; else if (mixedR < -32768) mixedR = -32768;

            bufferAudio[i * 2] = (int16_t)mixedL;
            bufferAudio[i * 2 + 1] = (int16_t)mixedR;
        }
    }
}