#include "editar.h"
#include "explorar.h" 
#include "menu.h" 
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>

// Struct aumentada e preparada para não quebrar saves antigos!
struct LayoutConfig {
    int lX, lY, lW, lH, cX, cY, cW, cH, dX, dY, dW, dH, bX, bY, bW, bH;
    int brX, brY, brW, brH, aX, aY, aW, aH, uX, uY, uW, uH, fT;
    int mX, mY, mTam, lSpcV, lOri, lBg;
    int bBg, bFill, lMk, lHMk;
    int lXH, lYH, lSpcH; // NOVOS - Exclusivos para Horizontal
};

// Padrões de Layout (Verticais e Horizontais Separados)
const int dLXV = 1054, dLYV = 204, dListSpcV = 120;
const int dLXH = 50, dLYH = 800, dListSpcH = 300; // Padrão Horizontal (Esquerda e mais espaçado)
const int dLW = 550, dLH = 80;
const int dCX = 150, dCY = 640, dCW = 300, dCH = 400;
const int dDX = 555, dDY = 650, dDW = 300, dDH = 300;
const int dBarX = 95, dBarY = 911, dBarW = 345, dBarH = 15;
const int dAudioX = 545, dAudioY = 632, dAudioW = 350, dAudioH = 550;
const int dUpX = 545, dUpY = 632, dUpW = 480, dUpH = 190;
const int dFontTam = 35, dMsgX = 100, dMsgY = 970, dMsgTam = 40;
const int dListOri = 0, dListBg = 0;
const int dBarBg = 6, dBarFill = 7, dListMark = 8, dListHoverMark = 9;

// Variáveis ativas
int listXV = dLXV, listYV = dLYV, listSpcV = dListSpcV;
int listXH = dLXH, listYH = dLYH, listSpcH = dListSpcH;
int listW = dLW, listH = dLH;
int capaX = dCX, capaY = dCY, capaW = dCW, capaH = dCH;
int discoX = dDX, discoY = dDY, discoW = dDW, discoH = dDH;
int backX = 0, backY = 0, backW = 1920, backH = 1080;
int barX = dBarX, barY = dBarY, barW = dBarW, barH = dBarH;
int audioX = dAudioX, audioY = dAudioY, audioW = dAudioW, audioH = dAudioH;
int upX = dUpX, upY = dUpY, upW = dUpW, upH = dUpH;
int fontTam = dFontTam, msgX = dMsgX, msgY = dMsgY, msgTam = dMsgTam;
int listOri = dListOri, listBg = dListBg;
int barBg = dBarBg, barFill = dBarFill, listMark = dListMark, listHoverMark = dListHoverMark;

bool editMode = false; int editTarget = 0, editType = 0; int mapAcoes[15];

void salvarConfiguracao() {
    LayoutConfig cfg = {
        listXV, listYV, listW, listH, capaX, capaY, capaW, capaH,
        discoX, discoY, discoW, discoH, backX, backY, backW, backH,
        barX, barY, barW, barH, audioX, audioY, audioW, audioH,
        upX, upY, upW, upH, fontTam,
        msgX, msgY, msgTam, listSpcV, listOri, listBg,
        barBg, barFill, listMark, listHoverMark,
        listXH, listYH, listSpcH
    };
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings.bin", "wb");
    if (f) { fwrite(&cfg, sizeof(LayoutConfig), 1, f); fclose(f); strcpy(msgStatus, "POSICOES SALVAS!"); }
    msgTimer = 90;
}

void carregarConfiguracao() {
    LayoutConfig cfg;
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings.bin", "rb");
    if (f) {
        listXH = dLXH; listYH = dLYH; listSpcH = dListSpcH; // Previne erro com save antigo
        size_t lidos = fread(&cfg, 1, sizeof(LayoutConfig), f);
        if (lidos >= 1) {
            listXV = cfg.lX; listYV = cfg.lY; listW = cfg.lW; listH = cfg.lH;
            capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH;
            discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH;
            backX = cfg.bX; backY = cfg.bY; backW = cfg.bW; backH = cfg.bH;
            barX = cfg.brX; barY = cfg.brY; barW = cfg.brW; barH = cfg.brH;
            audioX = cfg.aX; audioY = cfg.aY; audioW = cfg.aW; audioH = cfg.aH;
            upX = cfg.uX; upY = cfg.uY; upW = cfg.uW; upH = cfg.uH; fontTam = cfg.fT;
            msgX = cfg.mX; msgY = cfg.mY; msgTam = cfg.mTam;
            listSpcV = cfg.lSpcV; listOri = cfg.lOri; listBg = cfg.lBg;
            barBg = cfg.bBg; barFill = cfg.bFill; listMark = cfg.lMk; listHoverMark = cfg.lHMk;
            if (lidos == sizeof(LayoutConfig)) {
                listXH = cfg.lXH; listYH = cfg.lYH; listSpcH = cfg.lSpcH;
            }
        }
        fclose(f);
    }
}

void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA"); strcpy(nomes[1], "CAPA"); strcpy(nomes[2], "DISCO"); strcpy(nomes[3], "FUNDO");
    strcpy(nomes[4], "BARRA LOAD"); strcpy(nomes[5], "MENU AUDIO"); strcpy(nomes[6], "MENU UPLOAD");
    strcpy(nomes[7], "TAMANHO FONTE"); strcpy(nomes[8], "NOTIFICACOES"); strcpy(nomes[9], "EXPLORAR");
    strcpy(nomes[10], "RESETAR TUDO"); totalItens = 11; menuAtual = MENU_EDITAR;
}

void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0;
    if (editTarget == 9) {
        strcpy(nomes[totalItens], "COR MARCADO"); mapAcoes[totalItens++] = 6;
        strcpy(nomes[totalItens], "COR CURSOR+MARC"); mapAcoes[totalItens++] = 7;
    }
    else if (editTarget == 7) {
        strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1;
    }
    else {
        strcpy(nomes[totalItens], "POSICAO"); mapAcoes[totalItens++] = 0;
        if (editTarget == 8) { strcpy(nomes[totalItens], "TAMANHO DA FONTE"); mapAcoes[totalItens++] = 1; }
        else { strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ESTICAR"); mapAcoes[totalItens++] = 2; }

        if (editTarget == 0) {
            strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3;
            strcpy(nomes[totalItens], "ESPACAMENTO"); mapAcoes[totalItens++] = 4;
            strcpy(nomes[totalItens], "ORIENTACAO"); mapAcoes[totalItens++] = 5;
        }
        else if (editTarget == 4) {
            strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3;
            strcpy(nomes[totalItens], "COR PREENCHIMENTO"); mapAcoes[totalItens++] = 8;
        }
        else if (editTarget == 5 || editTarget == 6) {
            strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3;
        }
    }
    strcpy(nomes[totalItens], "RESETAR ESTE ITEM"); mapAcoes[totalItens++] = 9;
    menuAtual = MENU_EDIT_TARGET;
}

void processarControlesEdicao(unsigned int buttons) {
    static bool pCrossEdit = false; static bool pCircleEdit = false;
    int* tX = &listXV, * tY = &listYV, * tW = &listW, * tH = &listH; // Backup Pointer

    if (editTarget == 0) { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
    else if (editTarget == 1) { tX = &capaX; tY = &capaY; tW = &capaW; tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else if (editTarget == 3) { tX = &backX; tY = &backY; tW = &backW; tH = &backH; }
    else if (editTarget == 4) { tX = &barX; tY = &barY; tW = &barW; tH = &barH; }
    else if (editTarget == 5) { tX = &audioX; tY = &audioY; tW = &audioW; tH = &audioH; }
    else if (editTarget == 6) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
    else if (editTarget == 7) { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }
    else if (editTarget == 8) { tX = &msgX; tY = &msgY; tW = &msgTam; tH = &msgTam; }

    if (editType == 3) {
        if (editTarget == 4) {
            if (buttons & ORBIS_PAD_BUTTON_LEFT) { barBg--; if (barBg < 0) barBg = 14; }
            if (buttons & ORBIS_PAD_BUTTON_RIGHT) { barBg++; if (barBg > 14) barBg = 0; }
        }
        else {
            if (buttons & ORBIS_PAD_BUTTON_LEFT) { listBg--; if (listBg < 0) listBg = 14; }
            if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listBg++; if (listBg > 14) listBg = 0; }
        }
    }
    else if (editType == 4) { // ESPAÇAMENTO INDEPENDENTE!
        int* tSpc = (listOri == 0) ? &listSpcV : &listSpcH;
        if (buttons & ORBIS_PAD_BUTTON_UP) (*tSpc) -= 5;
        if (buttons & ORBIS_PAD_BUTTON_DOWN) (*tSpc) += 5;
        if (buttons & ORBIS_PAD_BUTTON_LEFT) (*tSpc) -= 1;
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) (*tSpc) += 1;
    }
    else if (editType == 5) {
        if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) listOri = (listOri == 0) ? 1 : 0;
    }
    else if (editType == 6) {
        if (buttons & ORBIS_PAD_BUTTON_LEFT) { listMark--; if (listMark < 0) listMark = 14; }
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listMark++; if (listMark > 14) listMark = 0; }
    }
    else if (editType == 7) {
        if (buttons & ORBIS_PAD_BUTTON_LEFT) { listHoverMark--; if (listHoverMark < 0) listHoverMark = 14; }
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listHoverMark++; if (listHoverMark > 14) listHoverMark = 0; }
    }
    else if (editType == 8) {
        if (buttons & ORBIS_PAD_BUTTON_LEFT) { barFill--; if (barFill < 0) barFill = 14; }
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) { barFill++; if (barFill > 14) barFill = 0; }
    }
    else {
        if (buttons & ORBIS_PAD_BUTTON_UP) { if (editType == 1) { (*tH)--; if (tW != tH) (*tW)--; } else if (editType == 2) (*tH)--; else (*tY)--; }
        if (buttons & ORBIS_PAD_BUTTON_DOWN) { if (editType == 1) { (*tH)++; if (tW != tH) (*tW)++; } else if (editType == 2) (*tH)++; else (*tY)++; }
        if (buttons & ORBIS_PAD_BUTTON_LEFT) { if (editType == 2) (*tW)--; else if (editType == 0) (*tX)--; }
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) { if (editType == 2) (*tW)++; else if (editType == 0) (*tX)++; }
    }

    if (buttons & ORBIS_PAD_BUTTON_CROSS) { if (!pCrossEdit) { salvarConfiguracao(); editMode = false; preencherMenuEditTarget(); pCrossEdit = true; } }
    else pCrossEdit = false;
    if (buttons & ORBIS_PAD_BUTTON_CIRCLE) { if (!pCircleEdit) { carregarConfiguracao(); strcpy(msgStatus, "ALTERACOES CANCELADAS!"); msgTimer = 90; editMode = false; preencherMenuEditTarget(); pCircleEdit = true; } }
    else pCircleEdit = false;
}