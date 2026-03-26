#include "editar.h"
#include "explorar.h" 
#include "menu.h" 
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>

#include "elementos_animados_sprite_sheet.h"

struct LayoutConfig {
    int lX, lY, lW, lH, cX, cY, cW, cH, dX, dY, dW, dH, bX, bY, bW, bH, brX, brY, brW, brH, aX, aY, aW, aH, uX, uY, uW, uH, fT, mX, mY, mTam, lSpcV, lOri, lBg, bBg, bFill, lMk, lHMk, lXH, lYH, lSpcH, fAl, fSc;
    int e1X, e1Y, e1W, e1H, e1O, c1X, c1Y, c1W, c1H, c1O, p1X, p1Y, p1W, p1H, p1O, p1M, p1L;
    int sfxOn, sfxVol;
    int uBg, uTn, uTs;

    bool a_ativo, a_ck;
    int a_x, a_y, a_col, a_lin, a_vel;
    float a_esc;
    uint8_t a_r, a_g, a_b;
    int a_offX, a_offY, a_fIni, a_fFim;
    bool a_ck2;
    uint8_t a_r2, a_g2, a_b2;
    bool a_autoCtr;

    int a_frameOffX[100];
    int a_frameOffY[100];
};

int listXV = 1054, listYV = 204, listSpcV = 120, listXH = 50, listYH = 800, listSpcH = 300, listW = 550, listH = 80, capaX = 150, capaY = 640, capaW = 300, capaH = 400, discoX = 555, discoY = 650, discoW = 300, discoH = 300, backX = 0, backY = 0, backW = 1920, backH = 1080, barX = 95, barY = 911, barW = 345, barH = 15, audioX = 545, audioY = 632, audioW = 321, audioH = 378, upX = 545, upY = 632, upW = 321, upH = 378, fontTam = 35, msgX = 100, msgY = 970, msgTam = 40, listOri = 0, listBg = 0, barBg = 6, barFill = 7, listMark = 8, listHoverMark = 9, fontAlign = 0, fontScroll = 0;
int elem1X = 100, elem1Y = 358, elem1W = 200, elem1H = 200, elem1On = 0;
int ctrl1X = 724, ctrl1Y = 361, ctrl1W = 200, ctrl1H = 200, ctrl1On = 0;
int pont1X = 0, pont1Y = 0, pont1W = 50, pont1H = 50, pont1On = 0, pont1Modo = 0, pont1Lado = 0;
int sfxLigado = 1, sfxVolume = 100;
int upBg = 0, upTextNorm = 12, upTextSel = 8;

bool editMode = false; int editTarget = 0, editType = 0; int mapAcoes[50]; // CORRIGIDO PARA 50 AQUI!

void salvarConfiguracao() {
    LayoutConfig cfg;
    memset(&cfg, 0, sizeof(LayoutConfig));
    cfg.lX = listXV; cfg.lY = listYV; cfg.lW = listW; cfg.lH = listH; cfg.cX = capaX; cfg.cY = capaY; cfg.cW = capaW; cfg.cH = capaH; cfg.dX = discoX; cfg.dY = discoY; cfg.dW = discoW; cfg.dH = discoH; cfg.bX = backX; cfg.bY = backY; cfg.bW = backW; cfg.bH = backH; cfg.brX = barX; cfg.brY = barY; cfg.brW = barW; cfg.brH = barH; cfg.aX = audioX; cfg.aY = audioY; cfg.aW = audioW; cfg.aH = audioH; cfg.uX = upX; cfg.uY = upY; cfg.uW = upW; cfg.uH = upH; cfg.fT = fontTam; cfg.mX = msgX; cfg.mY = msgY; cfg.mTam = msgTam; cfg.lSpcV = listSpcV; cfg.lOri = listOri; cfg.lBg = listBg; cfg.bBg = barBg; cfg.bFill = barFill; cfg.lMk = listMark; cfg.lHMk = listHoverMark; cfg.lXH = listXH; cfg.lYH = listYH; cfg.lSpcH = listSpcH; cfg.fAl = fontAlign; cfg.fSc = fontScroll;
    cfg.e1X = elem1X; cfg.e1Y = elem1Y; cfg.e1W = elem1W; cfg.e1H = elem1H; cfg.e1O = elem1On; cfg.c1X = ctrl1X; cfg.c1Y = ctrl1Y; cfg.c1W = ctrl1W; cfg.c1H = ctrl1H; cfg.c1O = ctrl1On; cfg.p1X = pont1X; cfg.p1Y = pont1Y; cfg.p1W = pont1W; cfg.p1H = pont1H; cfg.p1O = pont1On; cfg.p1M = pont1Modo; cfg.p1L = pont1Lado; cfg.sfxOn = sfxLigado; cfg.sfxVol = sfxVolume;
    cfg.uBg = upBg; cfg.uTn = upTextNorm; cfg.uTs = upTextSel;

    cfg.a_ativo = anim_ativo; cfg.a_ck = anim_usarColorKey; cfg.a_x = anim_posX; cfg.a_y = anim_posY; cfg.a_col = anim_colunas; cfg.a_lin = anim_linhas; cfg.a_vel = anim_velocidade; cfg.a_esc = anim_escala; cfg.a_r = anim_keyR; cfg.a_g = anim_keyG; cfg.a_b = anim_keyB;
    cfg.a_offX = anim_offsetX; cfg.a_offY = anim_offsetY; cfg.a_fIni = anim_frameInicial; cfg.a_fFim = anim_frameFinal;
    cfg.a_ck2 = anim_usarColorKey2; cfg.a_r2 = anim_keyR2; cfg.a_g2 = anim_keyG2; cfg.a_b2 = anim_keyB2;
    cfg.a_autoCtr = anim_autoCenter;

    for (int i = 0; i < 100; i++) {
        cfg.a_frameOffX[i] = anim_frameOffsetX[i];
        cfg.a_frameOffY[i] = anim_frameOffsetY[i];
    }

    FILE* f = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "wb");
    if (f) { fwrite(&cfg, sizeof(LayoutConfig), 1, f); fclose(f); strcpy(msgStatus, "POSICOES SALVAS!"); }
    msgTimer = 90;
}

void carregarConfiguracao() {
    LayoutConfig cfg; FILE* f = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "rb");

    elem1On = 0; ctrl1On = 0; pont1On = 0; sfxLigado = 1; sfxVolume = 100;
    upBg = 0; upTextNorm = 12; upTextSel = 8;
    anim_ativo = true; anim_usarColorKey = true; anim_posX = 445; anim_posY = -130;
    anim_colunas = 24; anim_linhas = 19; anim_velocidade = 100; anim_escala = 4.5f;
    anim_keyR = 55; anim_keyG = 39; anim_keyB = 130;
    anim_offsetX = 65; anim_offsetY = 0; anim_frameInicial = 0; anim_frameFinal = 7;
    anim_usarColorKey2 = true; anim_keyR2 = 0; anim_keyG2 = 0; anim_keyB2 = 255;
    anim_autoCenter = false;
    for (int i = 0; i < 100; i++) { anim_frameOffsetX[i] = 0; anim_frameOffsetY[i] = 0; }

    if (f) {
        memset(&cfg, 0, sizeof(LayoutConfig));
        size_t lidos = fread(&cfg, 1, sizeof(LayoutConfig), f);
        if (lidos >= 1) {
            listXV = cfg.lX; listYV = cfg.lY; listW = cfg.lW; listH = cfg.lH; capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH; discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH; backX = cfg.bX; backY = cfg.bY; backW = cfg.bW; backH = cfg.bH; barX = cfg.brX; barY = cfg.brY; barW = cfg.brW; barH = cfg.brH; audioX = cfg.aX; audioY = cfg.aY; audioW = cfg.aW; audioH = cfg.aH; upX = cfg.uX; upY = cfg.uY; upW = cfg.uW; upH = cfg.uH; fontTam = cfg.fT; msgX = cfg.mX; msgY = cfg.mY; msgTam = cfg.mTam; listSpcV = cfg.lSpcV; listOri = cfg.lOri; listBg = cfg.lBg; barBg = cfg.bBg; barFill = cfg.bFill; listMark = cfg.lMk; listHoverMark = cfg.lHMk;
            if (lidos > 150) { listXH = cfg.lXH; listYH = cfg.lYH; listSpcH = cfg.lSpcH; fontAlign = cfg.fAl; fontScroll = cfg.fSc; }
            if (lidos >= 196) {
                elem1X = cfg.e1X; elem1Y = cfg.e1Y; elem1W = cfg.e1W; elem1H = cfg.e1H; elem1On = cfg.e1O; ctrl1X = cfg.c1X; ctrl1Y = cfg.c1Y; ctrl1W = cfg.c1W; ctrl1H = cfg.c1H; ctrl1On = cfg.c1O; pont1X = cfg.p1X; pont1Y = cfg.p1Y; pont1W = cfg.p1W; pont1H = cfg.p1H; pont1On = cfg.p1O; pont1Modo = cfg.p1M; pont1Lado = cfg.p1L; sfxLigado = cfg.sfxOn; sfxVolume = cfg.sfxVol; upBg = cfg.uBg; upTextNorm = cfg.uTn; upTextSel = cfg.uTs;
            }
            if (lidos >= sizeof(LayoutConfig) - 805) {
                anim_ativo = cfg.a_ativo; anim_usarColorKey = cfg.a_ck; anim_posX = cfg.a_x; anim_posY = cfg.a_y;
                anim_colunas = cfg.a_col; anim_linhas = cfg.a_lin; anim_velocidade = cfg.a_vel; anim_escala = cfg.a_esc;
                anim_keyR = cfg.a_r; anim_keyG = cfg.a_g; anim_keyB = cfg.a_b;
                anim_offsetX = cfg.a_offX; anim_offsetY = cfg.a_offY; anim_frameInicial = cfg.a_fIni; anim_frameFinal = cfg.a_fFim;
                anim_usarColorKey2 = cfg.a_ck2; anim_keyR2 = cfg.a_r2; anim_keyG2 = cfg.a_g2; anim_keyB2 = cfg.a_b2;
            }
            if (lidos == sizeof(LayoutConfig)) {
                anim_autoCenter = cfg.a_autoCtr;
                for (int i = 0; i < 100; i++) {
                    anim_frameOffsetX[i] = cfg.a_frameOffX[i];
                    anim_frameOffsetY[i] = cfg.a_frameOffY[i];
                }
            }
        } fclose(f);
    }
}

void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA"); strcpy(nomes[1], "CAPA"); strcpy(nomes[2], "DISCO"); strcpy(nomes[3], "FUNDO"); strcpy(nomes[4], "BARRA LOAD"); strcpy(nomes[5], "MENU AUDIO"); strcpy(nomes[6], "MENU UPLOAD"); strcpy(nomes[7], "FONTE (TEXTO)"); strcpy(nomes[8], "NOTIFICACOES"); strcpy(nomes[9], "EXPLORAR"); strcpy(nomes[10], "ELEMENTO FIXO"); strcpy(nomes[11], "ELEMENTO CTRL"); strcpy(nomes[12], "PONTEIRO"); strcpy(nomes[13], "ELEMENTOS SONOROS");
    strcpy(nomes[14], "ANIMACAO (SPRITES)");
    strcpy(nomes[15], "RESETAR TUDO");
    totalItens = 16; menuAtual = MENU_EDITAR;
}

void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0;
    if (editTarget == 9) {
        strcpy(nomes[totalItens], "COR LISTA MARCADA"); mapAcoes[totalItens++] = 6;
        strcpy(nomes[totalItens], "COR LISTA CURSOR+MARC"); mapAcoes[totalItens++] = 7;
        strcpy(nomes[totalItens], "MENU OPCOES POSICAO"); mapAcoes[totalItens++] = 17;
        strcpy(nomes[totalItens], "MENU OPCOES TAMANHO"); mapAcoes[totalItens++] = 18;
        strcpy(nomes[totalItens], "MENU OPCOES ESTICAR"); mapAcoes[totalItens++] = 19;
        strcpy(nomes[totalItens], "MENU OPCOES COR FUNDO"); mapAcoes[totalItens++] = 20;
        strcpy(nomes[totalItens], "MENU OPCOES COR TEXTO"); mapAcoes[totalItens++] = 21;
        strcpy(nomes[totalItens], "MENU OPCOES COR TEXTO SEL"); mapAcoes[totalItens++] = 22;
    }
    else if (editTarget == 7) { strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ALINHAMENTO"); mapAcoes[totalItens++] = 10; strcpy(nomes[totalItens], "LIMITES"); mapAcoes[totalItens++] = 11; }
    else if (editTarget >= 10 && editTarget <= 12) {
        strcpy(nomes[totalItens], "POSICAO"); mapAcoes[totalItens++] = 0; strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ESTICAR"); mapAcoes[totalItens++] = 2; strcpy(nomes[totalItens], "LIGAR / DESLIGAR"); mapAcoes[totalItens++] = 12;
        if (editTarget == 12) { strcpy(nomes[totalItens], "MODO ACOMPANHAR/ESTATICO"); mapAcoes[totalItens++] = 13; strcpy(nomes[totalItens], "LADO (ESQ/DIR/CIMA/BAIXO)"); mapAcoes[totalItens++] = 14; }
    }
    else if (editTarget == 13) {
        strcpy(nomes[totalItens], "LIGAR / DESLIGAR"); mapAcoes[totalItens++] = 15;
        strcpy(nomes[totalItens], "VOLUME"); mapAcoes[totalItens++] = 16;
    }
    else if (editTarget == 14) {
        strcpy(nomes[totalItens], "POSICAO DA ANIMACAO"); mapAcoes[totalItens++] = 23;
        strcpy(nomes[totalItens], "ESCALA / TAMANHO"); mapAcoes[totalItens++] = 24;
        strcpy(nomes[totalItens], "VELOCIDADE"); mapAcoes[totalItens++] = 25;
        strcpy(nomes[totalItens], "GRADE (COLUNAS E LINHAS)"); mapAcoes[totalItens++] = 26;
        strcpy(nomes[totalItens], "DESLOCAMENTO INTERNO (X / Y)"); mapAcoes[totalItens++] = 27;
        strcpy(nomes[totalItens], "FRAME INICIAL E FINAL (LOOP)"); mapAcoes[totalItens++] = 28;
        strcpy(nomes[totalItens], "TESTAR SPRITE (EDITAR PIVOT)"); mapAcoes[totalItens++] = 29;
        strcpy(nomes[totalItens], "INICIAR ANIMACAO CONTINUA"); mapAcoes[totalItens++] = 30;
        strcpy(nomes[totalItens], "LIGAR / DESLIGAR VISUAL"); mapAcoes[totalItens++] = 31;
        strcpy(nomes[totalItens], "CHROMA KEY 1 - RED"); mapAcoes[totalItens++] = 32;
        strcpy(nomes[totalItens], "CHROMA KEY 1 - GREEN"); mapAcoes[totalItens++] = 33;
        strcpy(nomes[totalItens], "CHROMA KEY 1 - BLUE"); mapAcoes[totalItens++] = 34;

        strcpy(nomes[totalItens], "CHROMA KEY 2 (LIG/DESL)"); mapAcoes[totalItens++] = 36;
        strcpy(nomes[totalItens], "CHROMA KEY 2 - RED"); mapAcoes[totalItens++] = 37;
        strcpy(nomes[totalItens], "CHROMA KEY 2 - GREEN"); mapAcoes[totalItens++] = 38;
        strcpy(nomes[totalItens], "CHROMA KEY 2 - BLUE"); mapAcoes[totalItens++] = 39;

        strcpy(nomes[totalItens], "AUTO-CENTRALIZAR X (INTELIGENTE)"); mapAcoes[totalItens++] = 40;
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
    static bool pCrossEdit = false; static bool pCircleEdit = false; static bool pTriEdit = false;
    int* tX = &listXV, * tY = &listYV, * tW = &listW, * tH = &listH;

    if (editTarget == 0) { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
    else if (editTarget == 1) { tX = &capaX; tY = &capaY; tW = &capaW; tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else if (editTarget == 3) { tX = &backX; tY = &backY; tW = &backW; tH = &backH; }
    else if (editTarget == 4) { tX = &barX; tY = &barY; tW = &barW; tH = &barH; }
    else if (editTarget == 5) { tX = &audioX; tY = &audioY; tW = &audioW; tH = &audioH; }
    else if (editTarget == 6 || editTarget == 9) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
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
    else if (editType == 15) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) sfxLigado = (sfxLigado == 0) ? 1 : 0; }
    else if (editType == 16) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { sfxVolume -= 5; if (sfxVolume < 0) sfxVolume = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { sfxVolume += 5; if (sfxVolume > 100) sfxVolume = 100; } }
    else if (editType == 17) { if (buttons & ORBIS_PAD_BUTTON_UP) upY -= 2; if (buttons & ORBIS_PAD_BUTTON_DOWN) upY += 2; if (buttons & ORBIS_PAD_BUTTON_LEFT) upX -= 2; if (buttons & ORBIS_PAD_BUTTON_RIGHT) upX += 2; }
    else if (editType == 18) { if (buttons & ORBIS_PAD_BUTTON_UP) { upH -= 2; upW -= 2; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { upH += 2; upW += 2; } }
    else if (editType == 19) { if (buttons & ORBIS_PAD_BUTTON_UP) upH -= 2; if (buttons & ORBIS_PAD_BUTTON_DOWN) upH += 2; if (buttons & ORBIS_PAD_BUTTON_LEFT) upW -= 2; if (buttons & ORBIS_PAD_BUTTON_RIGHT) upW += 2; }
    else if (editType == 20) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { upBg--; if (upBg < 0) upBg = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { upBg++; if (upBg > 14) upBg = 0; } }
    else if (editType == 21) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { upTextNorm--; if (upTextNorm < 0) upTextNorm = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { upTextNorm++; if (upTextNorm > 14) upTextNorm = 0; } }
    else if (editType == 22) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { upTextSel--; if (upTextSel < 0) upTextSel = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { upTextSel++; if (upTextSel > 14) upTextSel = 0; } }

    else if (editType == 23) { if (buttons & ORBIS_PAD_BUTTON_UP) anim_posY -= 5; if (buttons & ORBIS_PAD_BUTTON_DOWN) anim_posY += 5; if (buttons & ORBIS_PAD_BUTTON_LEFT) anim_posX -= 5; if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_posX += 5; }
    else if (editType == 24) { if (buttons & ORBIS_PAD_BUTTON_UP) anim_escala += 0.1f; if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_escala -= 0.1f; if (anim_escala < 0.1f) anim_escala = 0.1f; } }
    else if (editType == 25) { if (buttons & ORBIS_PAD_BUTTON_LEFT) anim_velocidade -= 5; if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_velocidade += 5; if (buttons & ORBIS_PAD_BUTTON_UP) anim_velocidade += 20; if (buttons & ORBIS_PAD_BUTTON_DOWN) anim_velocidade -= 20; if (anim_velocidade < 1) anim_velocidade = 1; }

    else if (editType == 26) {
        static int bDel1 = 0; if (bDel1 > 0) bDel1--;
        if (bDel1 == 0) {
            if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_colunas -= 1; bDel1 = 4; }
            if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_colunas += 1; bDel1 = 4; }
            if (buttons & ORBIS_PAD_BUTTON_UP) { anim_linhas -= 1; bDel1 = 4; }
            if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_linhas += 1; bDel1 = 4; }
        }
        if (anim_colunas < 1) anim_colunas = 1; if (anim_linhas < 1) anim_linhas = 1;
    }
    else if (editType == 27) {
        if (buttons & ORBIS_PAD_BUTTON_LEFT) anim_offsetX -= 1;
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_offsetX += 1;
        if (buttons & ORBIS_PAD_BUTTON_UP) anim_offsetY -= 1;
        if (buttons & ORBIS_PAD_BUTTON_DOWN) anim_offsetY += 1;
    }
    else if (editType == 28) {
        static int bDel2 = 0; if (bDel2 > 0) bDel2--;
        if (bDel2 == 0) {
            if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_frameInicial -= 1; bDel2 = 5; }
            if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_frameInicial += 1; bDel2 = 5; }
            if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_frameFinal -= 1; bDel2 = 5; }
            if (buttons & ORBIS_PAD_BUTTON_UP) { anim_frameFinal += 1; bDel2 = 5; }
        }
        if (anim_frameInicial < 0) anim_frameInicial = 0;
        if (anim_frameFinal < 0) anim_frameFinal = 0;
    }

    else if (editType == 29) {
        anim_modoTeste = true;
        static int fDelay = 0; if (fDelay > 0) fDelay--;

        if (fDelay == 0) {
            if (buttons & ORBIS_PAD_BUTTON_R1) {
                anim_frameAtual += 1;
                if (anim_frameAtual > anim_frameFinal) anim_frameAtual = anim_frameInicial;
                fDelay = 10;
            }
            if (buttons & ORBIS_PAD_BUTTON_L1) {
                anim_frameAtual -= 1;
                if (anim_frameAtual < anim_frameInicial) anim_frameAtual = anim_frameFinal;
                fDelay = 10;
            }

            if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_frameOffsetX[anim_frameAtual] -= 1; fDelay = 2; }
            if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_frameOffsetX[anim_frameAtual] += 1; fDelay = 2; }
            if (buttons & ORBIS_PAD_BUTTON_UP) { anim_frameOffsetY[anim_frameAtual] -= 1; fDelay = 2; }
            if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_frameOffsetY[anim_frameAtual] += 1; fDelay = 2; }
        }

        if (buttons & ORBIS_PAD_BUTTON_TRIANGLE) {
            if (!pTriEdit) {
                autoCentralizarFrameAtual();
                pTriEdit = true;
            }
        }
        else {
            pTriEdit = false;
        }
    }

    else if (editType == 30) { anim_modoTeste = false; }
    else if (editType == 31) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) anim_ativo = !anim_ativo; }
    else if (editType == 32) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyR -= 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyR += 5; } }
    else if (editType == 33) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyG -= 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyG += 5; } }
    else if (editType == 34) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyB -= 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyB += 5; } }

    else if (editType == 36) {
        static int ckDel = 0; if (ckDel > 0) ckDel--;
        if (ckDel == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) {
            anim_usarColorKey2 = !anim_usarColorKey2;
            ckDel = 10;
        }
    }
    else if (editType == 37) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyR2 -= 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyR2 += 5; } }
    else if (editType == 38) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyG2 -= 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyG2 += 5; } }
    else if (editType == 39) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyB2 -= 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyB2 += 5; } }

    else if (editType == 40) {
        static int acDel = 0; if (acDel > 0) acDel--;
        if (acDel == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) {
            anim_autoCenter = !anim_autoCenter;
            acDel = 10;
        }
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