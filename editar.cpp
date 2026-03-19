#include "editar.h"
#include "explorar.h" // Importa as variáveis globais compartilhadas (nomes, menuAtual, etc)
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>

#include <stdarg.h>
#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

struct LayoutConfig { int lX, lY, lW, lH, cX, cY, cW, cH, dX, dY, dW, dH, bX, bY, bW, bH; };

// Valores padrão
const int dLX = 1054, dLY = 204, dLW = 550, dLH = 80;
const int dCX = 136, dCY = 628, dCW = 300, dCH = 400;
const int dDX = 528, dDY = 652, dDW = 300, dDH = 300;

// Variáveis ativas
int listX = dLX, listY = dLY, listW = dLW, listH = dLH;
int capaX = dCX, capaY = dCY, capaW = dCW, capaH = dCH;
int discoX = dDX, discoY = dDY, discoW = dDW, discoH = dDH;
int backX = 0, backY = 0, backW = 1920, backH = 1080;

// Estados
bool editMode = false;
int editTarget = 0, editType = 0;

void salvarConfiguracao() {
    LayoutConfig cfg = { listX, listY, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH };
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings.bin", "wb");
    if (f) {
        fwrite(&cfg, sizeof(LayoutConfig), 1, f);
        fclose(f);
        strcpy(msgStatus, "POSICOES SALVAS!");
    }
    msgTimer = 90;
}

void carregarConfiguracao() {
    LayoutConfig cfg;
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings.bin", "rb");
    if (f) {
        if (fread(&cfg, sizeof(LayoutConfig), 1, f) == 1) {
            listX = cfg.lX; listY = cfg.lY; listW = cfg.lW; listH = cfg.lH;
            capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH;
            discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH;
        }
        fclose(f);
    }
}

void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "POSICAO"); strcpy(nomes[1], "TAMANHO");
    strcpy(nomes[2], "ESTICAR"); strcpy(nomes[3], "RESETAR TUDO");
    totalItens = 4; menuAtual = MENU_EDITAR;
}

void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA"); strcpy(nomes[1], "CAPA");
    strcpy(nomes[2], "DISCO"); strcpy(nomes[3], "FUNDO");
    strcpy(nomes[4], "RESETAR");
    totalItens = 5; menuAtual = MENU_EDIT_TARGET;
}

void processarControlesEdicao(unsigned int buttons) {
    int* tX, * tY, * tW, * tH;

    if (editTarget == 0) { tX = &listX;  tY = &listY;  tW = &listW;  tH = &listH; }
    else if (editTarget == 1) { tX = &capaX;  tY = &capaY;  tW = &capaW;  tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else { tX = &backX;  tY = &backY;  tW = &backW;  tH = &backH; }

    if (buttons & ORBIS_PAD_BUTTON_UP) { if (editType == 1) { (*tH)--; (*tW)--; } else if (editType == 2) (*tH)--; else (*tY)--; }
    if (buttons & ORBIS_PAD_BUTTON_DOWN) { if (editType == 1) { (*tH)++; (*tW)++; } else if (editType == 2) (*tH)++; else (*tY)++; }
    if (buttons & ORBIS_PAD_BUTTON_LEFT) { if (editType == 2) (*tW)--; else if (editType == 0) (*tX)--; }
    if (buttons & ORBIS_PAD_BUTTON_RIGHT) { if (editType == 2) (*tW)++; else if (editType == 0) (*tX)++; }

    if (buttons & (ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_CIRCLE)) {
        editMode = false;
        salvarConfiguracao();
        preencherMenuEditar();
    }
}