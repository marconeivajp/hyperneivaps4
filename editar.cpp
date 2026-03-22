#include "editar.h"
#include "explorar.h" 
#include "menu.h" // Para termos acesso ao sel e off
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>

#include <stdarg.h>
#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

// Struct aumentada para caber a barra
struct LayoutConfig { int lX, lY, lW, lH, cX, cY, cW, cH, dX, dY, dW, dH, bX, bY, bW, bH, brX, brY, brW, brH; };

// Valores padrão
const int dLX = 1054, dLY = 204, dLW = 550, dLH = 80;
const int dCX = 136, dCY = 628, dCW = 300, dCH = 400;
const int dDX = 528, dDY = 652, dDW = 300, dDH = 300;
const int dBarX = 50, dBarY = 940, dBarW = 400, dBarH = 15;

// Variáveis ativas
int listX = dLX, listY = dLY, listW = dLW, listH = dLH;
int capaX = dCX, capaY = dCY, capaW = dCW, capaH = dCH;
int discoX = dDX, discoY = dDY, discoW = dDW, discoH = dDH;
int backX = 0, backY = 0, backW = 1920, backH = 1080;
int barX = dBarX, barY = dBarY, barW = dBarW, barH = dBarH;

// Estados
bool editMode = false;
int editTarget = 0, editType = 0;

void salvarConfiguracao() {
    LayoutConfig cfg = { listX, listY, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH, barX, barY, barW, barH };
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
        barX = dBarX; barY = dBarY; barW = dBarW; barH = dBarH;

        size_t lidos = fread(&cfg, 1, sizeof(LayoutConfig), f);
        if (lidos >= sizeof(LayoutConfig) - 4 * sizeof(int)) {
            listX = cfg.lX; listY = cfg.lY; listW = cfg.lW; listH = cfg.lH;
            capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH;
            discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH;
            backX = cfg.bX; backY = cfg.bY; backW = cfg.bW; backH = cfg.bH;

            if (lidos == sizeof(LayoutConfig)) {
                barX = cfg.brX; barY = cfg.brY; barW = cfg.brW; barH = cfg.brH;
            }
        }
        fclose(f);
    }
}

// 1º Menu: O QUE editar (Alvos)
void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA"); strcpy(nomes[1], "CAPA");
    strcpy(nomes[2], "DISCO"); strcpy(nomes[3], "FUNDO");
    strcpy(nomes[4], "BARRA LOAD"); strcpy(nomes[5], "RESETAR TUDO");
    totalItens = 6; menuAtual = MENU_EDITAR;
}

// 2º Menu: O QUE FAZER (Ações)
void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "POSICAO"); strcpy(nomes[1], "TAMANHO");
    strcpy(nomes[2], "ESTICAR"); strcpy(nomes[3], "RESETAR ESTE ITEM");
    totalItens = 4; menuAtual = MENU_EDIT_TARGET;
}

void processarControlesEdicao(unsigned int buttons) {
    static bool pCrossEdit = false;
    static bool pCircleEdit = false;

    int* tX, * tY, * tW, * tH;

    if (editTarget == 0) { tX = &listX;  tY = &listY;  tW = &listW;  tH = &listH; }
    else if (editTarget == 1) { tX = &capaX;  tY = &capaY;  tW = &capaW;  tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else if (editTarget == 3) { tX = &backX;  tY = &backY;  tW = &backW;  tH = &backH; }
    else { tX = &barX;  tY = &barY;  tW = &barW;  tH = &barH; }

    if (buttons & ORBIS_PAD_BUTTON_UP) { if (editType == 1) { (*tH)--; (*tW)--; } else if (editType == 2) (*tH)--; else (*tY)--; }
    if (buttons & ORBIS_PAD_BUTTON_DOWN) { if (editType == 1) { (*tH)++; (*tW)++; } else if (editType == 2) (*tH)++; else (*tY)++; }
    if (buttons & ORBIS_PAD_BUTTON_LEFT) { if (editType == 2) (*tW)--; else if (editType == 0) (*tX)--; }
    if (buttons & ORBIS_PAD_BUTTON_RIGHT) { if (editType == 2) (*tW)++; else if (editType == 0) (*tX)++; }

    // BOTÃO X: SALVA AS MODIFICAÇÕES E VOLTA AO MENU DE AÇÕES (Posição, Tamanho...)
    if (buttons & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCrossEdit) {
            salvarConfiguracao();
            editMode = false;
            preencherMenuEditTarget();
            pCrossEdit = true;
        }
    }
    else {
        pCrossEdit = false;
    }

    // BOTÃO O (BOLA): CANCELA AS MODIFICAÇÕES E VOLTA AO MENU DE AÇÕES
    if (buttons & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircleEdit) {
            carregarConfiguracao(); // Cancela recarregando a posição original do arquivo
            strcpy(msgStatus, "ALTERACOES CANCELADAS!"); // Dá um aviso visual
            msgTimer = 90;
            editMode = false;
            preencherMenuEditTarget();
            pCircleEdit = true;
        }
    }
    else {
        pCircleEdit = false;
    }
}