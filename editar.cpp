#include "editar.h"
#include "explorar.h" 
#include "menu.h" 
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>

struct LayoutConfig {
    int lX, lY, lW, lH, cX, cY, cW, cH, dX, dY, dW, dH, bX, bY, bW, bH, brX, brY, brW, brH, aX, aY, aW, aH, uX, uY, uW, uH, fT, mX, mY, mTam, lSpcV, lOri, lBg, bBg, bFill, lMk, lHMk, lXH, lYH, lSpcH, fAl, fSc;
    int e1X, e1Y, e1W, e1H, e1O, c1X, c1Y, c1W, c1H, c1O, p1X, p1Y, p1W, p1H, p1O, p1M, p1L;
};

// AS NOVAS DIMENSÕES 321x378 FORAM APLICADAS AQUI NAS VARIÁVEIS dAudioW, dAudioH, dUpW, dUpH
const int dLXV = 1054, dLYV = 204, dListSpcV = 120, dLXH = 50, dLYH = 800, dListSpcH = 300, dLW = 550, dLH = 80, dCX = 150, dCY = 640, dCW = 300, dCH = 400, dDX = 555, dDY = 650, dDW = 300, dDH = 300, dBarX = 95, dBarY = 911, dBarW = 345, dBarH = 15, dAudioX = 545, dAudioY = 632, dAudioW = 321, dAudioH = 378, dUpX = 545, dUpY = 632, dUpW = 321, dUpH = 378, dFontTam = 35, dMsgX = 100, dMsgY = 970, dMsgTam = 40, dListOri = 0, dListBg = 0, dBarBg = 6, dBarFill = 7, dListMark = 8, dListHoverMark = 9, dFontAlign = 0, dFontScroll = 0;

// =========================================================================
// TODOS OS ELEMENTOS COMEÇAM DESLIGADOS (0) E NAS SUAS COORDENADAS PADRÃO!
const int dElem1X = 100, dElem1Y = 358, dElem1W = 200, dElem1H = 200, dElem1On = 0;
const int dCtrl1X = 724, dCtrl1Y = 361, dCtrl1W = 200, dCtrl1H = 200, dCtrl1On = 0;
const int dPont1X = 0, dPont1Y = 0, dPont1W = 50, dPont1H = 50, dPont1On = 0, dPont1Modo = 0, dPont1Lado = 0;
// =========================================================================

int listXV = dLXV, listYV = dLYV, listSpcV = dListSpcV, listXH = dLXH, listYH = dLYH, listSpcH = dListSpcH, listW = dLW, listH = dLH, capaX = dCX, capaY = dCY, capaW = dCW, capaH = dCH, discoX = dDX, discoY = dDY, discoW = dDW, discoH = dDH, backX = 0, backY = 0, backW = 1920, backH = 1080, barX = dBarX, barY = dBarY, barW = dBarW, barH = dBarH, audioX = dAudioX, audioY = dAudioY, audioW = dAudioW, audioH = dAudioH, upX = dUpX, upY = dUpY, upW = dUpW, upH = dUpH, fontTam = dFontTam, msgX = dMsgX, msgY = dMsgY, msgTam = dMsgTam, listOri = dListOri, listBg = dListBg, barBg = dBarBg, barFill = dBarFill, listMark = dListMark, listHoverMark = dListHoverMark, fontAlign = dFontAlign, fontScroll = dFontScroll;
int elem1X = dElem1X, elem1Y = dElem1Y, elem1W = dElem1W, elem1H = dElem1H, elem1On = dElem1On;
int ctrl1X = dCtrl1X, ctrl1Y = dCtrl1Y, ctrl1W = dCtrl1W, ctrl1H = dCtrl1H, ctrl1On = dCtrl1On;
int pont1X = dPont1X, pont1Y = dPont1Y, pont1W = dPont1W, pont1H = dPont1H, pont1On = dPont1On, pont1Modo = dPont1Modo, pont1Lado = dPont1Lado;

bool editMode = false; int editTarget = 0, editType = 0; int mapAcoes[15];

void salvarConfiguracao() {
    LayoutConfig cfg = {
        listXV, listYV, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH, barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH, fontTam, msgX, msgY, msgTam, listSpcV, listOri, listBg, barBg, barFill, listMark, listHoverMark, listXH, listYH, listSpcH, fontAlign, fontScroll,
        elem1X, elem1Y, elem1W, elem1H, elem1On, ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On, pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado
    };

    FILE* f = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "wb");
    if (f) { fwrite(&cfg, sizeof(LayoutConfig), 1, f); fclose(f); strcpy(msgStatus, "POSICOES SALVAS!"); }
    msgTimer = 90;
}

void carregarConfiguracao() {
    LayoutConfig cfg; FILE* f = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "rb");
    if (f) {
        elem1On = dElem1On; ctrl1On = dCtrl1On; pont1On = dPont1On;
        size_t lidos = fread(&cfg, 1, sizeof(LayoutConfig), f);
        if (lidos >= 1) {
            listXV = cfg.lX; listYV = cfg.lY; listW = cfg.lW; listH = cfg.lH; capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH; discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH; backX = cfg.bX; backY = cfg.bY; backW = cfg.bW; backH = cfg.bH; barX = cfg.brX; barY = cfg.brY; barW = cfg.brW; barH = cfg.brH; audioX = cfg.aX; audioY = cfg.aY; audioW = cfg.aW; audioH = cfg.aH; upX = cfg.uX; upY = cfg.uY; upW = cfg.uW; upH = cfg.uH; fontTam = cfg.fT; msgX = cfg.mX; msgY = cfg.mY; msgTam = cfg.mTam; listSpcV = cfg.lSpcV; listOri = cfg.lOri; listBg = cfg.lBg; barBg = cfg.bBg; barFill = cfg.bFill; listMark = cfg.lMk; listHoverMark = cfg.lHMk;
            if (lidos > 150) { listXH = cfg.lXH; listYH = cfg.lYH; listSpcH = cfg.lSpcH; fontAlign = cfg.fAl; fontScroll = cfg.fSc; }
            if (lidos == sizeof(LayoutConfig)) { elem1X = cfg.e1X; elem1Y = cfg.e1Y; elem1W = cfg.e1W; elem1H = cfg.e1H; elem1On = cfg.e1O; ctrl1X = cfg.c1X; ctrl1Y = cfg.c1Y; ctrl1W = cfg.c1W; ctrl1H = cfg.c1H; ctrl1On = cfg.c1O; pont1X = cfg.p1X; pont1Y = cfg.p1Y; pont1W = cfg.p1W; pont1H = cfg.p1H; pont1On = cfg.p1O; pont1Modo = cfg.p1M; pont1Lado = cfg.p1L; }
        } fclose(f);
    }
}

void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA"); strcpy(nomes[1], "CAPA"); strcpy(nomes[2], "DISCO"); strcpy(nomes[3], "FUNDO"); strcpy(nomes[4], "BARRA LOAD"); strcpy(nomes[5], "MENU AUDIO"); strcpy(nomes[6], "MENU UPLOAD"); strcpy(nomes[7], "FONTE (TEXTO)"); strcpy(nomes[8], "NOTIFICACOES"); strcpy(nomes[9], "EXPLORAR"); strcpy(nomes[10], "ELEMENTO FIXO"); strcpy(nomes[11], "ELEMENTO CTRL"); strcpy(nomes[12], "PONTEIRO"); strcpy(nomes[13], "RESETAR TUDO"); totalItens = 14; menuAtual = MENU_EDITAR;
}

void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0;
    if (editTarget == 9) { strcpy(nomes[totalItens], "COR MARCADO"); mapAcoes[totalItens++] = 6; strcpy(nomes[totalItens], "COR CURSOR+MARC"); mapAcoes[totalItens++] = 7; }
    else if (editTarget == 7) { strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ALINHAMENTO"); mapAcoes[totalItens++] = 10; strcpy(nomes[totalItens], "LIMITES"); mapAcoes[totalItens++] = 11; }
    else if (editTarget >= 10 && editTarget <= 12) {
        strcpy(nomes[totalItens], "POSICAO"); mapAcoes[totalItens++] = 0; strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ESTICAR"); mapAcoes[totalItens++] = 2; strcpy(nomes[totalItens], "LIGAR / DESLIGAR"); mapAcoes[totalItens++] = 12;
        if (editTarget == 12) { strcpy(nomes[totalItens], "MODO ACOMPANHAR/ESTATICO"); mapAcoes[totalItens++] = 13; strcpy(nomes[totalItens], "LADO (ESQ/DIR/CIMA/BAIXO)"); mapAcoes[totalItens++] = 14; }
    }
    else {
        strcpy(nomes[totalItens], "POSICAO"); mapAcoes[totalItens++] = 0;
        if (editTarget == 8) { strcpy(nomes[totalItens], "TAMANHO DA FONTE"); mapAcoes[totalItens++] = 1; }
        else { strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ESTICAR"); mapAcoes[totalItens++] = 2; }
        if (editTarget == 0) { strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3; strcpy(nomes[totalItens], "ESPACAMENTO"); mapAcoes[totalItens++] = 4; strcpy(nomes[totalItens], "ORIENTACAO"); mapAcoes[totalItens++] = 5; }
        else if (editTarget == 4) { strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3; strcpy(nomes[totalItens], "COR PREENCH"); mapAcoes[totalItens++] = 8; }
        else if (editTarget == 5 || editTarget == 6) { strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3; }
    }
    strcpy(nomes[totalItens], "RESETAR ESTE ITEM"); mapAcoes[totalItens++] = 9; menuAtual = MENU_EDIT_TARGET;
}

void processarControlesEdicao(unsigned int buttons) {
    static bool pCrossEdit = false; static bool pCircleEdit = false;
    int* tX = &listXV, * tY = &listYV, * tW = &listW, * tH = &listH;

    if (editTarget == 0) { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
    else if (editTarget == 1) { tX = &capaX; tY = &capaY; tW = &capaW; tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else if (editTarget == 3) { tX = &backX; tY = &backY; tW = &backW; tH = &backH; }
    else if (editTarget == 4) { tX = &barX; tY = &barY; tW = &barW; tH = &barH; }
    else if (editTarget == 5) { tX = &audioX; tY = &audioY; tW = &audioW; tH = &audioH; }
    else if (editTarget == 6) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
    else if (editTarget == 7) { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }
    else if (editTarget == 8) { tX = &msgX; tY = &msgY; tW = &msgTam; tH = &msgTam; }
    else if (editTarget == 10) { tX = &elem1X; tY = &elem1Y; tW = &elem1W; tH = &elem1H; }
    else if (editTarget == 11) { tX = &ctrl1X; tY = &ctrl1Y; tW = &ctrl1W; tH = &ctrl1H; }
    else if (editTarget == 12) { tX = &pont1X; tY = &pont1Y; tW = &pont1W; tH = &pont1H; }

    if (editType == 3) { if (editTarget == 4) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { barBg--; if (barBg < 0) barBg = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { barBg++; if (barBg > 14) barBg = 0; } } else { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listBg--; if (listBg < 0) listBg = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listBg++; if (listBg > 14) listBg = 0; } } }
    else if (editType == 4) { int* tSpc = (listOri == 0) ? &listSpcV : &listSpcH; if (buttons & ORBIS_PAD_BUTTON_UP) (*tSpc) -= 5; if (buttons & ORBIS_PAD_BUTTON_DOWN) (*tSpc) += 5; if (buttons & ORBIS_PAD_BUTTON_LEFT) (*tSpc) -= 1; if (buttons & ORBIS_PAD_BUTTON_RIGHT) (*tSpc) += 1; }
    else if (editType == 5) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) listOri = (listOri == 0) ? 1 : 0; }
    else if (editType == 6) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listMark--; if (listMark < 0) listMark = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listMark++; if (listMark > 14) listMark = 0; } }
    else if (editType == 7) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listHoverMark--; if (listHoverMark < 0) listHoverMark = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listHoverMark++; if (listHoverMark > 14) listHoverMark = 0; } }
    else if (editType == 8) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { barFill--; if (barFill < 0) barFill = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { barFill++; if (barFill > 14) barFill = 0; } }
    else if (editType == 10) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { fontAlign--; if (fontAlign < 0) fontAlign = 2; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { fontAlign++; if (fontAlign > 2) fontAlign = 0; } }
    else if (editType == 11) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) fontScroll = (fontScroll == 0) ? 1 : 0; }
    else if (editType == 12) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) { if (editTarget == 10) elem1On = (elem1On == 0) ? 1 : 0; else if (editTarget == 11) ctrl1On = (ctrl1On == 0) ? 1 : 0; else if (editTarget == 12) pont1On = (pont1On == 0) ? 1 : 0; } }
    else if (editType == 13) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) pont1Modo = (pont1Modo == 0) ? 1 : 0; }
    else if (editType == 14) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { pont1Lado--; if (pont1Lado < 0) pont1Lado = 3; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { pont1Lado++; if (pont1Lado > 3) pont1Lado = 0; } }
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