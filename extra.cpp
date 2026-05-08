#include "extra.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/AudioIn.h>

extern int sel;
extern MenuLevel menuAtual;
extern int totalItens;
extern char nomes[3000][64];

// PONTE: Lê o comando já aberto pelo main.cpp
extern int globalPadHandle;

// SFX EXTERNOS (Elementos Sonoros)
extern int16_t* sfxUpData; extern size_t sfxUpLen;
extern int16_t* sfxDownData; extern size_t sfxDownLen;
extern int16_t* sfxCrossData; extern size_t sfxCrossLen;
extern int16_t* sfxCircleData; extern size_t sfxCircleLen;

// Matrizes para guardar o rastro (gráfico) contínuo
static bool trilhaL[256][256];
static bool trilhaR[256][256];

// AGORA TEMOS DUAS MATRIZES DE RASTRO (Uma para cada dedo)
static bool trilhaTouch1[576][282];
static bool trilhaTouch2[576][282];

// Guarda a posição anterior para desenhar linhas perfeitas (Bresenham)
static int prevLx = 128, prevLy = 128;
static int prevRx = 128, prevRy = 128;

// Posições anteriores independentes para os dois dedos
static int prevT1x = -1, prevT1y = -1;
static int prevT2x = -1, prevT2y = -1;

// CONTROLE DE HARDWARE (Som e Luz)
static int padAudioPort = -1;
static int padMicPort = -1;
static int micLevelDisplay = 0;
static uint32_t prevButtonsTest = 0;

struct ColorPreset { const char* name; uint8_t r, g, b; uint32_t btn; };
static ColorPreset colors[] = {
    {"BRANCO (CIMA)", 255, 255, 255, ORBIS_PAD_BUTTON_UP},
    {"AMARELO (BAIXO)", 255, 255, 0, ORBIS_PAD_BUTTON_DOWN},
    {"CYAN (ESQUERDA)", 0, 255, 255, ORBIS_PAD_BUTTON_LEFT},
    {"LIMA (DIREITA)", 128, 255, 0, ORBIS_PAD_BUTTON_RIGHT},
    {"VERDE (TRIAN.)", 0, 255, 0, ORBIS_PAD_BUTTON_TRIANGLE},
    {"VERMELHO (BOLA)", 255, 0, 0, ORBIS_PAD_BUTTON_CIRCLE},
    {"AZUL (CRUZ)", 0, 0, 255, ORBIS_PAD_BUTTON_CROSS},
    {"MAGENTA (QUADRADO)", 255, 0, 255, ORBIS_PAD_BUTTON_SQUARE}
};

void preencherMenuExtra() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Teste de Controle (Analise de Drift e Botoes)");
    strcpy(nomes[1], "Piano Virtual (Instrumento)");
    totalItens = 2;
    menuAtual = MENU_EXTRA;
    sel = 0;
}

void finalizarControleTeste() {
    if (padAudioPort >= 0) {
        sceAudioOutClose(padAudioPort);
        padAudioPort = -1;
    }
    if (padMicPort >= 0) {
        // No OpenOrbis sceAudioInClose as vezes nao recebe handle ou usa global
        ((void(*)())sceAudioInClose)(); 
        padMicPort = -1;
    }
    // Reseta a luz para o padrão (Desligado ou cor neutra)
    OrbisPadColor color = { 0, 0, 0 };
    scePadSetLightBar(globalPadHandle, &color);
    prevButtonsTest = 0;
}

void acaoCross_Extra() {
    if (sel == 0) {
        // Limpa os gráficos antes de entrar no Teste de Controle
        memset(trilhaL, 0, sizeof(trilhaL));
        memset(trilhaR, 0, sizeof(trilhaR));
        memset(trilhaTouch1, 0, sizeof(trilhaTouch1));
        memset(trilhaTouch2, 0, sizeof(trilhaTouch2));
        prevLx = 128; prevLy = 128;
        prevRx = 128; prevRy = 128;
        prevT1x = -1; prevT1y = -1;
        prevT2x = -1; prevT2y = -1;
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

    desenharTexto(p, "GERENCIADOR DE COMANDO (V1.00)", 40, 50, 40, 0xFF00AAFF);
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
    // LEITURA DOS SENSORES (MEMÓRIA BRUTA)
    // =======================================================
    uint8_t* raw = (uint8_t*)&data;
    uint32_t buttons = *(uint32_t*)(raw + 0x00);
    uint8_t lx = raw[0x04];
    uint8_t ly = raw[0x05];
    uint8_t rx = raw[0x06];
    uint8_t ry = raw[0x07];
    uint8_t l2 = raw[0x08];
    uint8_t r2 = raw[0x09];

    // GIROSCOPIO (Offset 0x12) - ACELEROMETRO (Offset 0x18)
    int16_t gx = *(int16_t*)(raw + 0x12);
    int16_t gy = *(int16_t*)(raw + 0x14);
    int16_t gz = *(int16_t*)(raw + 0x16);
    int16_t ax = *(int16_t*)(raw + 0x18);
    int16_t ay = *(int16_t*)(raw + 0x1A);
    int16_t az = *(int16_t*)(raw + 0x1C);

    // =======================================================
    // FEEDBACK DE HARDWARE (SOM, LUZ E MICROFONE)
    // =======================================================
    extern int32_t global_uId;
    if (padAudioPort < 0) {
        // Tipo 4 = BGM/Pad (Comum em drivers PS4) ou fallback 6
        padAudioPort = sceAudioOutOpen(global_uId, (OrbisAudioOutPort)4, 0, 1024, 48000, 1);
        ((void(*)(int, int))sceAudioOutSetMixLevelPadSpk)(padAudioPort, 255);
    }
    if (padMicPort < 0) {
        padMicPort = 1; // Usado como flag de "aberto"
        sceAudioInInit();
        ((void(*)())sceAudioInOpen)(); 
    }

    // Brilho dinâmico via Gatilhos (Base de 30% + Escala de 70%)
    float intensity = 0.3f + 0.7f * ((l2 > r2 ? l2 : r2) / 255.0f);

    bool anyButtonPressed = false;
    for (int i = 0; i < 8; i++) {
        uint32_t btn = colors[i].btn;
        if (buttons & btn) {
            anyButtonPressed = true;
            OrbisPadColor color = { (uint8_t)(colors[i].r * intensity), (uint8_t)(colors[i].g * intensity), (uint8_t)(colors[i].b * intensity) };
            scePadSetLightBar(globalPadHandle, &color);

            // Som apenas no primeiro pressionamento (Setas e Botões Principais)
            if (!(prevButtonsTest & btn)) {
                int16_t* sfx = NULL;
                if (btn == ORBIS_PAD_BUTTON_UP || btn == ORBIS_PAD_BUTTON_LEFT || btn == ORBIS_PAD_BUTTON_TRIANGLE) sfx = sfxUpData;
                else if (btn == ORBIS_PAD_BUTTON_DOWN || btn == ORBIS_PAD_BUTTON_RIGHT || btn == ORBIS_PAD_BUTTON_SQUARE) sfx = sfxDownData;
                else if (btn == ORBIS_PAD_BUTTON_CROSS) sfx = sfxCrossData;
                else if (btn == ORBIS_PAD_BUTTON_CIRCLE) sfx = sfxCircleData;
                if (sfx && padAudioPort >= 0) sceAudioOutOutput(padAudioPort, sfx);
            }
        }
    }
    if (!anyButtonPressed) {
        OrbisPadColor color = { 0, 0, 0 };
        scePadSetLightBar(globalPadHandle, &color);
    }

    prevButtonsTest = buttons;

    uint8_t touchNum = raw[0x34];
    uint16_t t0_x = 0; uint16_t t0_y = 0;
    uint16_t t1_x = 0; uint16_t t1_y = 0;

    if (touchNum > 0) {
        t0_x = *(uint16_t*)(raw + 0x3C);
        t0_y = *(uint16_t*)(raw + 0x3E);
    }
    if (touchNum > 1) {
        t1_x = *(uint16_t*)(raw + 0x44);
        t1_y = *(uint16_t*)(raw + 0x46);
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

    int lCenterX = 220; int lCenterY = 600; int radius = 150;
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

    int rCenterX = 560; int rCenterY = 600;
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
    // Renderizar Sensores (Giroscópio / Acelerômetro)
    // -----------------------------------------
    int sX = 1350; int sY = 460;
    desenharTexto(p, "SENSORES DE MOVIMENTO", 30, sX, sY - 40, 0xFF00AAFF);

    auto drawSensorBar = [&](int x, int y, const char* label, int16_t value, uint32_t color) {
        desenharTexto(p, label, 25, x, y, 0xFFFFFFFF);
        int barWidth = 200; int barHeight = 20;
        int fill = (value * barWidth) / 32768;
        if (fill > barWidth) fill = barWidth; if (fill < -barWidth) fill = -barWidth;

        for (int h = 0; h < barHeight; h++) {
            for (int w = 0; w < barWidth * 2; w++) {
                p[(y + 5 + h) * 1920 + (x + 100 + w)] = 0xFF333333;
            }
            if (fill >= 0) {
                for (int w = 0; w < fill; w++) p[(y + 5 + h) * 1920 + (x + 100 + barWidth + w)] = color;
            } else {
                for (int w = 0; w < -fill; w++) p[(y + 5 + h) * 1920 + (x + 100 + barWidth - w)] = color;
            }
        }
        char valStr[32]; sprintf(valStr, "%d", value);
        desenharTexto(p, valStr, 22, x + 100 + barWidth * 2 + 10, y, 0xFFFFFFFF);
    };

    drawSensorBar(sX, sY, "GYRO X:", gx, 0xFF00AAFF);
    drawSensorBar(sX, sY + 40, "GYRO Y:", gy, 0xFF00AAFF);
    drawSensorBar(sX, sY + 80, "GYRO Z:", gz, 0xFF00AAFF);

    drawSensorBar(sX, sY + 140, "ACCEL X:", ax, 0xFF00FF00);
    drawSensorBar(sX, sY + 180, "ACCEL Y:", ay, 0xFF00FF00);
    drawSensorBar(sX, sY + 220, "ACCEL Z:", az, 0xFF00FF00);

    // -----------------------------------------
    // Renderizar Touchpad (Multi-Touch)
    // -----------------------------------------
    int tpW = 350; int tpH = 200;
    int tpX = 850; int tpY = 500;

    desenharTexto(p, "TOUCHPAD PS4", 25, tpX + 60, tpY - 30, 0xFF00AAFF);

    for (int y = 0; y < tpH; y++) { for (int x = 0; x < tpW; x++) { p[(tpY + y) * 1920 + (tpX + x)] = 0xFF222222; } }
    for (int y = 0; y < tpH; y++) { for (int x = 0; x < tpW; x++) { if (x == 0 || y == 0 || x == tpW - 1 || y == tpH - 1) p[(tpY + y) * 1920 + (tpX + x)] = 0xFFAAAAAA; } }

    // Limpa a tela do touch se pressionar o "Click" central
    if (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) {
        memset(trilhaTouch1, 0, sizeof(trilhaTouch1));
        memset(trilhaTouch2, 0, sizeof(trilhaTouch2));
        prevT1x = -1; prevT1y = -1;
        prevT2x = -1; prevT2y = -1;
    }

    if (touchNum > 0) {
        int tx = (t0_x * tpW) / 1919; int ty = (t0_y * tpH) / 941;
        if (prevT1x == -1) { prevT1x = tx; prevT1y = ty; }
        drawLineOnGrid(trilhaTouch1, prevT1x, prevT1y, tx, ty);
        prevT1x = tx; prevT1y = ty;
    } else { prevT1x = -1; prevT1y = -1; }

    if (touchNum > 1) {
        int tx2 = (t1_x * tpW) / 1919; int ty2 = (t1_y * tpH) / 941;
        if (prevT2x == -1) { prevT2x = tx2; prevT2y = ty2; }
        drawLineOnGrid(trilhaTouch2, prevT2x, prevT2y, tx2, ty2);
        prevT2x = tx2; prevT2y = ty2;
    } else { prevT2x = -1; prevT2y = -1; }

    // Renderiza as trilhas
    for (int ty = 0; ty < tpH; ty++) {
        for (int tx = 0; tx < tpW; tx++) {
            if (trilhaTouch1[tx][ty]) {
                int fpx = tpX + tx; int fpy = tpY + ty;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ppx = fpx+dx, ppy = fpy+dy;
                        if (ppx >= 0 && ppx < 1920 && ppy >= 0 && ppy < 1080) p[ppy * 1920 + ppx] = 0xFF00AAFF;
                    }
                }
            }
            if (trilhaTouch2[tx][ty]) {
                int fpx = tpX + tx; int fpy = tpY + ty;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ppx = fpx+dx, ppy = fpy+dy;
                        if (ppx >= 0 && ppx < 1920 && ppy >= 0 && ppy < 1080) p[ppy * 1920 + ppx] = 0xFF00FF00;
                    }
                }
            }
        }
    }

    if (touchNum > 0) {
        int dotX = tpX + (t0_x * tpW) / 1919; int dotY = tpY + (t0_y * tpH) / 941;
        drawCircle(dotX, dotY, 12, 0xFF00AAFF, true);
        sprintf(txt, "DEDO 1: [%d, %d]", t0_x, t0_y);
        desenharTexto(p, txt, 25, tpX - 80, tpY + tpH + 30, 0xFF00AAFF);
    }
    if (touchNum > 1) {
        int dotX2 = tpX + (t1_x * tpW) / 1919; int dotY2 = tpY + (t1_y * tpH) / 941;
        drawCircle(dotX2, dotY2, 12, 0xFF00FF00, true);
        sprintf(txt, "DEDO 2: [%d, %d]", t1_x, t1_y);
        desenharTexto(p, txt, 25, tpX + 210, tpY + tpH + 30, 0xFF00FF00); // Movido mais para a esquerda
    }

    // -----------------------------------------
    // Renderizar Lista de Cores (Dividida em 2 Colunas)
    // -----------------------------------------
    int cXLabel = sX - 40; int cYLabel = sY + 300;
    desenharTexto(p, "LISTA DE CORES (LED)", 30, cXLabel, cYLabel - 40, 0xFF00AAFF);
    
    for (int i = 0; i < 8; i++) {
        int col = (i < 4) ? 0 : 1;
        int row = (i < 4) ? i : i - 4;
        int curX = cXLabel + (col * 320);
        int curY = cYLabel + (row * 35);

        uint32_t c = (buttons & colors[i].btn) ? 0xFFFFFFFF : 0xFF555555;
        uint32_t previewCol = 0xFF000000 | (colors[i].r << 16) | (colors[i].g << 8) | colors[i].b;
        
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) p[(curY + y) * 1920 + (curX + x)] = previewCol;
        }
        desenharTexto(p, colors[i].name, 22, curX + 30, curY, c);
    }

    char briTxt[64]; sprintf(briTxt, "BRILHO LED: %.0f%%", intensity * 100.0f);
    desenharTexto(p, briTxt, 25, cXLabel, cYLabel + 160, 0xFFFFFFFF);

    // -----------------------------------------
    // Renderizar Microfone (AudioIn)
    // -----------------------------------------
    int mX = tpX; int mY = tpY + 300;
    desenharTexto(p, "MICROFONE (INPUT)", 30, mX, mY - 30, 0xFF00AAFF);
    
    // Tenta ler amostras do microfone para o HUD
    static int16_t micBuffer[512];
    if (padMicPort >= 0) {
        ((int(*)(int, void*))sceAudioInInput)(0, micBuffer); 
        int peak = 0;
        for(int i=0; i<512; i++) { if(abs(micBuffer[i]) > peak) peak = abs(micBuffer[i]); }
        micLevelDisplay = peak;
    }

    int mBarW = 350; int mBarH = 20;
    for (int y = 0; y < mBarH; y++) {
        for (int x = 0; x < mBarW; x++) {
            uint32_t c = (x < (micLevelDisplay * mBarW) / 32768) ? 0xFF00FF00 : 0xFF333333;
            p[(mY + y) * 1920 + (mX + x)] = c;
        }
    }
    desenharTexto(p, "Fale no controle/headset para testar", 20, mX, mY + 30, 0xFFAAAAAA);

    // ===================================
    // PARTE 3: RODAPÉ (GATILHOS L2 E R2 EMPILHADOS)
    // =======================================================
    int l2X = 150, l2Y = 850;
    desenharTexto(p, "L2:", 30, l2X - 100, l2Y, 0xFFFFFFFF);
    for (int i = 0; i < 255; i++) {
        uint32_t c = (i < l2) ? 0xFF00AAFF : 0xFF333333;
        for (int y = 0; y < 40; y++) p[(l2Y + y) * 1920 + (l2X + i)] = c;
    }
    sprintf(txt, "%d / 255", l2); desenharTexto(p, txt, 25, l2X + 270, l2Y + 5, 0xFFFFFFFF);

    int r2X = 150, r2Y = 920;
    desenharTexto(p, "R2:", 30, r2X - 100, r2Y, 0xFFFFFFFF);
    for (int i = 0; i < 255; i++) {
        uint32_t c = (i < r2) ? 0xFF00AAFF : 0xFF333333;
        for (int y = 0; y < 40; y++) p[(r2Y + y) * 1920 + (r2X + i)] = c;
    }
    sprintf(txt, "%d / 255", r2); desenharTexto(p, txt, 25, r2X + 270, r2Y + 5, 0xFFFFFFFF);
}