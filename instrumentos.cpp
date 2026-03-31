#include "instrumentos.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <orbis/Pad.h>

extern int globalPadHandle;

void renderizarInstrumentos(uint32_t* p) {
    // Fundo escuro
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;

    desenharTexto(p, "PIANO VIRTUAL HYPER NEIVA", 40, 50, 50, 0xFF00AAFF);
    desenharTexto(p, "Pressione [OPTIONS] para Sair do Piano", 25, 50, 100, 0xFFAAAAAA);

    if (globalPadHandle < 0) {
        desenharTexto(p, "Controle nao detectado!", 30, 50, 200, 0xFF0000FF);
        return;
    }

    OrbisPadData data;
    scePadReadState(globalPadHandle, &data);
    uint32_t b = data.buttons;

    // Mapeamento de 13 notas (Uma Oitava Completa: Dó a Dó)
    // Brancas (7): Dó, Ré, Mi, Fá, Sol, Lá, Si, Dó
    // Pretas (5): Dó#, Ré#, Fá#, Sol#, Lá#

    bool keys[13] = { false };
    keys[0] = (b & ORBIS_PAD_BUTTON_L2);          // Dó (Branca)
    keys[1] = (b & ORBIS_PAD_BUTTON_L1);          // Dó# (Preta)
    keys[2] = (b & ORBIS_PAD_BUTTON_LEFT);        // Ré (Branca)
    keys[3] = (b & ORBIS_PAD_BUTTON_UP);          // Ré# (Preta)
    keys[4] = (b & ORBIS_PAD_BUTTON_RIGHT);       // Mi (Branca)
    keys[5] = (b & ORBIS_PAD_BUTTON_SQUARE);      // Fá (Branca)
    keys[6] = (b & ORBIS_PAD_BUTTON_TRIANGLE);    // Fá# (Preta)
    keys[7] = (b & ORBIS_PAD_BUTTON_CROSS);       // Sol (Branca)
    keys[8] = (b & ORBIS_PAD_BUTTON_TOUCH_PAD);   // Sol# (Preta)
    keys[9] = (b & ORBIS_PAD_BUTTON_CIRCLE);      // Lá (Branca)
    keys[10] = (b & ORBIS_PAD_BUTTON_R1);         // Lá# (Preta)
    keys[11] = (b & ORBIS_PAD_BUTTON_R2);         // Si (Branca)
    keys[12] = (b & ORBIS_PAD_BUTTON_R3);         // Dó Maior (Branca)

    int startX = 360;
    int startY = 300;
    int whiteW = 150;
    int whiteH = 500;
    int blackW = 80;
    int blackH = 300;

    // Array para saber quais índices do vetor keys[] são teclas brancas
    int whiteMap[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
    const char* whiteLabels[8] = { "[L2]", "[ESQ]", "[DIR]", "[QUAD]", "[ X ]", "[ BOLA ]", "[R2]", "[R3]" };
    const char* whiteNotes[8] = { "DO", "RE", "MI", "FA", "SOL", "LA", "SI", "DO" };

    // 1º DESENHAR AS TECLAS BRANCAS
    for (int i = 0; i < 8; i++) {
        int keyIndex = whiteMap[i];
        bool isPressed = keys[keyIndex];
        uint32_t color = isPressed ? 0xFF00AAFF : 0xFFEEEEEE; // Fica azul se pressionada
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

        // Letreiros na ponta da tecla branca
        desenharTexto(p, whiteNotes[i], 30, kX + 50, startY + whiteH - 80, isPressed ? 0xFFFFFFFF : 0xFF000000);
        desenharTexto(p, whiteLabels[i], 20, kX + 40, startY + whiteH - 30, isPressed ? 0xFFFFFFFF : 0xFF555555);
    }

    // Array para as teclas pretas
    int blackMap[5] = { 1, 3, 6, 8, 10 };
    int blackOffsets[5] = { 1, 2, 4, 5, 6 }; // Posições (após qual tecla branca ela entra)
    const char* blackLabels[5] = { "[L1]", "[CIMA]", "[TRI]", "[TOUCH]", "[R1]" };

    // 2º DESENHAR AS TECLAS PRETAS (Por cima das brancas)
    for (int i = 0; i < 5; i++) {
        int keyIndex = blackMap[i];
        bool isPressed = keys[keyIndex];
        uint32_t color = isPressed ? 0xFF00AAFF : 0xFF222222; // Fica azul se pressionada

        int kX = startX + (blackOffsets[i] * whiteW) - (blackW / 2);

        for (int y = 0; y < blackH; y++) {
            for (int x = 0; x < blackW; x++) {
                p[(startY + y) * 1920 + (kX + x)] = color;
            }
        }

        // Letreiros nas teclas pretas
        desenharTexto(p, blackLabels[i], 18, kX + 10, startY + blackH - 30, 0xFFFFFFFF);
    }

    desenharTexto(p, "Dica: Aperte os botoes indicados nas teclas para tocar!", 25, 600, 900, 0xFF00FF00);
}