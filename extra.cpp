#include "extra.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <orbis/Pad.h>

extern int sel;
extern MenuLevel menuAtual;
extern int totalItens;
extern char nomes[3000][64];

// PONTE: Lê o comando já aberto pelo main.cpp
extern int globalPadHandle;

// Matrizes para guardar o rastro (gráfico) contínuo
static bool trilhaL[256][256];
static bool trilhaR[256][256];
static bool trilhaTouch[576][282]; // Resolução do Touchpad escalonada

// Guarda a posição anterior para desenhar linhas perfeitas (Bresenham)
static int prevLx = 128, prevLy = 128;
static int prevRx = 128, prevRy = 128;
static int prevTx = -1, prevTy = -1;

void preencherMenuExtra() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Teste de Controle (Analise de Drift e Botoes)");
    strcpy(nomes[1], "Piano Virtual (Instrumento)");
    totalItens = 2;
    menuAtual = MENU_EXTRA;
    sel = 0;
}

void acaoCross_Extra() {
    if (sel == 0) {
        // Limpa os gráficos antes de entrar no Teste de Controle
        memset(trilhaL, 0, sizeof(trilhaL));
        memset(trilhaR, 0, sizeof(trilhaR));
        memset(trilhaTouch, 0, sizeof(trilhaTouch));
        prevLx = 128; prevLy = 128;
        prevRx = 128; prevRy = 128;
        prevTx = -1; prevTy = -1;
        menuAtual = MENU_CONTROLE_TESTE;
    }
    else if (sel == 1) {
        // Abre a interface do Piano Virtual
        menuAtual = MENU_INSTRUMENTOS;
    }
}

// =======================================================
// ALGORITMO BRESENHAM (Cria linhas contínuas geometricamente perfeitas)
// =======================================================
template<int W, int H>
void drawLineOnGrid(bool(&grid)[W][H], int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H) grid[x0][y0] = true;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void renderizarControleTeste(uint32_t* p) {
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;

    desenharTexto(p, "DIAGNOSTICO AVANCADO DO COMANDO", 40, 50, 40, 0xFF00AAFF);
    desenharTexto(p, "Pressione [OPTIONS] para Sair do Teste", 25, 50, 80, 0xFFAAAAAA);

    if (globalPadHandle < 0) {
        desenharTexto(p, "Controle nao inicializado pelo sistema!", 30, 50, 200, 0xFF0000FF);
        return;
    }

    OrbisPadData data;
    int res = scePadReadState(globalPadHandle, &data);
    if (res != 0) {
        desenharTexto(p, "Sinal do controle perdido. Reconecte o controle...", 30, 50, 200, 0xFF0000FF);
        return;
    }

    // =======================================================
    // LEITURA BRUTA (BLINDADA) DA MEMÓRIA DO HARDWARE DA PS4
    // Usa casting de bytes para evitar qualquer erro de nome no SDK
    // =======================================================
    uint8_t* raw = (uint8_t*)&data;

    uint32_t buttons = *(uint32_t*)(raw + 0x00);
    uint8_t lx = raw[0x04];
    uint8_t ly = raw[0x05];
    uint8_t rx = raw[0x06];
    uint8_t ry = raw[0x07];
    uint8_t l2 = raw[0x08];
    uint8_t r2 = raw[0x09];

    // O Touchpad possui 4 bytes em branco "reserved" antes do X e Y!
    // Esta é a magia que faz as coordenadas funcionarem perfeitamente e sem erros do compilador:
    uint8_t touchNum = raw[0x3C];
    uint16_t t0_x = 0; uint16_t t0_y = 0;
    uint16_t t1_x = 0; uint16_t t1_y = 0;

    if (touchNum > 0) {
        t0_x = *(uint16_t*)(raw + 0x44); // 0x44 é o endereço exato do X do dedo 1
        t0_y = *(uint16_t*)(raw + 0x46); // 0x46 é o endereço exato do Y do dedo 1
    }
    if (touchNum > 1) {
        t1_x = *(uint16_t*)(raw + 0x4C); // 0x4C é o endereço exato do X do dedo 2
        t1_y = *(uint16_t*)(raw + 0x4E); // 0x4E é o endereço exato do Y do dedo 2
    }

    // Funcao auxiliar para desenhar a base dos circulos
    auto drawCircle = [&](int cx, int cy, int r, uint32_t color, bool fill) {
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    if (fill || (x * x + y * y >= (r - 3) * (r - 3))) {
                        int px = cx + x; int py = cy + y;
                        if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = color;
                    }
                }
            }
        }
        };

    // =======================================================
    // PARTE 1: TOPO (BOTOES DIGITAIS TODOS ALINHADOS)
    // =======================================================
    int dX = 100, dY = 150;
    desenharTexto(p, "D-PAD", 30, dX, dY - 20, 0xFF00AAFF);
    desenharTexto(p, "[ CIMA ]", 35, dX, dY, (buttons & ORBIS_PAD_BUTTON_UP) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ BAIXO ]", 35, dX, dY + 45, (buttons & ORBIS_PAD_BUTTON_DOWN) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ ESQ ]", 35, dX, dY + 90, (buttons & ORBIS_PAD_BUTTON_LEFT) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ DIR ]", 35, dX, dY + 135, (buttons & ORBIS_PAD_BUTTON_RIGHT) ? 0xFFFFFFFF : 0xFF555555);

    int eX = 650, eY = 150;
    desenharTexto(p, "SISTEMA E OMBRO", 30, eX + 150, eY - 20, 0xFF00AAFF);
    desenharTexto(p, "[ L1 ]", 35, eX, eY, (buttons & ORBIS_PAD_BUTTON_L1) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ R1 ]", 35, eX, eY + 45, (buttons & ORBIS_PAD_BUTTON_R1) ? 0xFFFFFFFF : 0xFF555555);

    desenharTexto(p, "[ L3 CLICK ]", 35, eX + 200, eY, (buttons & ORBIS_PAD_BUTTON_L3) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ R3 CLICK ]", 35, eX + 200, eY + 45, (buttons & ORBIS_PAD_BUTTON_R3) ? 0xFFFFFFFF : 0xFF555555);

    desenharTexto(p, "[ TOUCH CLIQUE ]", 35, eX + 450, eY, (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ OPTIONS ] (SAIR)", 35, eX + 450, eY + 45, (buttons & ORBIS_PAD_BUTTON_OPTIONS) ? 0xFF0000FF : 0xFF555555);

    int aX = 1500, aY = 150;
    desenharTexto(p, "BOTOES DE ACAO", 30, aX, aY - 20, 0xFF00AAFF);
    desenharTexto(p, "[ TRIANGULO ]", 35, aX, aY, (buttons & ORBIS_PAD_BUTTON_TRIANGLE) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ BOLINHA ]", 35, aX, aY + 45, (buttons & ORBIS_PAD_BUTTON_CIRCLE) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ CRUZ (X) ]", 35, aX, aY + 90, (buttons & ORBIS_PAD_BUTTON_CROSS) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ QUADRADO ]", 35, aX, aY + 135, (buttons & ORBIS_PAD_BUTTON_SQUARE) ? 0xFFFFFFFF : 0xFF555555);

    // =======================================================
    // PARTE 2: CENTRO (L3 -> R3 -> TOUCHPAD)
    // =======================================================

    // Analógico L3
    if (lx >= 115 && lx <= 140 && ly >= 115 && ly <= 140) { memset(trilhaL, 0, sizeof(trilhaL)); prevLx = 128; prevLy = 128; }
    else { drawLineOnGrid(trilhaL, prevLx, prevLy, lx, ly); prevLx = lx; prevLy = ly; }

    int lCenterX = 300; int lCenterY = 600; int radius = 150;
    desenharTexto(p, "Centro Ideal: 128 | 128", 25, lCenterX - 130, lCenterY - 180, 0xFFFFFFFF);
    drawCircle(lCenterX, lCenterY, radius, 0xFF444444, false);
    drawCircle(lCenterX, lCenterY, 4, 0xFFFFFFFF, true);

    for (int tx = 0; tx < 256; tx++) {
        for (int ty = 0; ty < 256; ty++) {
            if (trilhaL[tx][ty]) {
                int px = lCenterX + ((tx - 128) * radius) / 128; int py = lCenterY + ((ty - 128) * radius) / 128;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int fpx = px + dx, fpy = py + dy;
                        if (fpx >= 0 && fpx < 1920 && fpy >= 0 && fpy < 1080) p[fpy * 1920 + fpx] = 0xFF00AAFF;
                    }
                }
            }
        }
    }

    int lxOff = ((lx - 128) * radius) / 128; int lyOff = ((ly - 128) * radius) / 128;
    drawCircle(lCenterX + lxOff, lCenterY + lyOff, 20, 0xFF00FF00, true);

    char txt[256]; sprintf(txt, "L3 X: %d | Y: %d", lx, ly);
    desenharTexto(p, txt, 30, lCenterX - 110, lCenterY + 180, 0xFFFFFFFF);

    // Analógico R3
    if (rx >= 115 && rx <= 140 && ry >= 115 && ry <= 140) { memset(trilhaR, 0, sizeof(trilhaR)); prevRx = 128; prevRy = 128; }
    else { drawLineOnGrid(trilhaR, prevRx, prevRy, rx, ry); prevRx = rx; prevRy = ry; }

    int rCenterX = 800; int rCenterY = 600;
    desenharTexto(p, "Centro Ideal: 128 | 128", 25, rCenterX - 130, rCenterY - 180, 0xFFFFFFFF);
    drawCircle(rCenterX, rCenterY, radius, 0xFF444444, false);
    drawCircle(rCenterX, rCenterY, 4, 0xFFFFFFFF, true);

    for (int tx = 0; tx < 256; tx++) {
        for (int ty = 0; ty < 256; ty++) {
            if (trilhaR[tx][ty]) {
                int px = rCenterX + ((tx - 128) * radius) / 128; int py = rCenterY + ((ty - 128) * radius) / 128;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int fpx = px + dx, fpy = py + dy;
                        if (fpx >= 0 && fpx < 1920 && fpy >= 0 && fpy < 1080) p[fpy * 1920 + fpx] = 0xFF00AAFF;
                    }
                }
            }
        }
    }

    int rxOff = ((rx - 128) * radius) / 128; int ryOff = ((ry - 128) * radius) / 128;
    drawCircle(rCenterX + rxOff, rCenterY + ryOff, 20, 0xFF00FF00, true);

    sprintf(txt, "R3 X: %d | Y: %d", rx, ry);
    desenharTexto(p, txt, 30, rCenterX - 110, rCenterY + 180, 0xFFFFFFFF);

    // -----------------------------------------
    // Renderizar Touchpad na Direita
    // -----------------------------------------
    int tpW = 576; int tpH = 282;
    int tpX = 1250; int tpY = 460;

    desenharTexto(p, "TOUCHPAD DA PS4 (TESTE DE RASTRO)", 25, tpX + 60, tpY - 30, 0xFF00AAFF);

    for (int y = 0; y < tpH; y++) { for (int x = 0; x < tpW; x++) { p[(tpY + y) * 1920 + (tpX + x)] = 0xFF222222; } }
    for (int y = 0; y < tpH; y++) { for (int x = 0; x < tpW; x++) { if (x == 0 || y == 0 || x == tpW - 1 || y == tpH - 1) p[(tpY + y) * 1920 + (tpX + x)] = 0xFFAAAAAA; } }

    if (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) { memset(trilhaTouch, 0, sizeof(trilhaTouch)); prevTx = -1; prevTy = -1; }

    if (touchNum > 0) {
        int tx = (t0_x * tpW) / 1919; int ty = (t0_y * tpH) / 941;
        if (prevTx == -1) { prevTx = tx; prevTy = ty; }
        drawLineOnGrid(trilhaTouch, prevTx, prevTy, tx, ty);
        prevTx = tx; prevTy = ty;
    }
    else {
        prevTx = -1; prevTy = -1;
    }

    for (int ty = 0; ty < tpH; ty++) {
        for (int tx = 0; tx < tpW; tx++) {
            if (trilhaTouch[tx][ty]) {
                int fpx = tpX + tx; int fpy = tpY + ty;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ppx = fpx + dx, ppy = fpy + dy;
                        if (ppx >= 0 && ppx < 1920 && ppy >= 0 && ppy < 1080) p[ppy * 1920 + ppx] = 0xFF00AAFF;
                    }
                }
            }
        }
    }

    if (touchNum > 0) {
        int dotX = tpX + (t0_x * tpW) / 1919; int dotY = tpY + (t0_y * tpH) / 941;
        drawCircle(dotX, dotY, 12, 0xFF00AAFF, true);
        sprintf(txt, "Dedo 1: X:%d Y:%d", t0_x, t0_y);
        desenharTexto(p, txt, 25, tpX + 10, tpY + tpH + 30, 0xFF00AAFF);
    }
    else { desenharTexto(p, "Sem toque detectado", 25, tpX + 170, tpY + tpH + 30, 0xFF555555); }

    if (touchNum > 1) {
        int dotX2 = tpX + (t1_x * tpW) / 1919; int dotY2 = tpY + (t1_y * tpH) / 941;
        drawCircle(dotX2, dotY2, 12, 0xFF00FF00, true);
        sprintf(txt, "Dedo 2: X:%d Y:%d", t1_x, t1_y);
        desenharTexto(p, txt, 25, tpX + 350, tpY + tpH + 30, 0xFF00FF00);
    }

    // =======================================================
    // PARTE 3: RODAPÉ (GATILHOS L2 E R2 DE 0 A 255)
    // =======================================================
    int l2X = 300, l2Y = 920;
    desenharTexto(p, "PRESSAO L2:", 30, l2X - 180, l2Y, 0xFFFFFFFF);
    for (int i = 0; i < 255; i++) {
        uint32_t c = (i < l2) ? 0xFF00AAFF : 0xFF333333;
        for (int y = 0; y < 40; y++) p[(l2Y + y) * 1920 + (l2X + i)] = c;
    }
    sprintf(txt, "%d / 255", l2); desenharTexto(p, txt, 25, l2X + 270, l2Y + 5, 0xFFFFFFFF);

    int r2X = 1150, r2Y = 920;
    desenharTexto(p, "PRESSAO R2:", 30, r2X - 180, r2Y, 0xFFFFFFFF);
    for (int i = 0; i < 255; i++) {
        uint32_t c = (i < r2) ? 0xFF00AAFF : 0xFF333333;
        for (int y = 0; y < 40; y++) p[(r2Y + y) * 1920 + (r2X + i)] = c;
    }
    sprintf(txt, "%d / 255", r2); desenharTexto(p, txt, 25, r2X + 270, r2Y + 5, 0xFFFFFFFF);
}