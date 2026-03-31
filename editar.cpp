#include "editar.h"
#include "explorar.h" 
#include "menu.h" 
#include "controle_editar.h"
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>
#include "elementos_animados_sprite_sheet.h"

extern int wSprite, hSprite;
extern char msgStatus[128];
extern int msgTimer;

// VARIÁVEIS GLOBAIS DA ENGINE GRÁFICA (Resolve os erros de undefined symbol)
bool isFirstFrameUI = true;
int uiW[10] = { 0 };
int uiH[10] = { 0 };
unsigned char* uiTextures[10] = { NULL };
unsigned char* prevUiTextures[10] = { NULL };
int prevUiW[10] = { 0 }, prevUiH[10] = { 0 };
int uiAnimFrame = 0;
int lastTelaId = 0;
int prevTelaIdForOut = 0;

#ifndef CUSTOM_UI_DEF
#define CUSTOM_UI_DEF
struct CustomElementDef {
    bool ativo;
    char caminho[256];
    int pX, pY, pW, pH;
    bool animInAtiva;
    int inX, inY;
    int velIn;
    bool animOutAtiva;
    int outX, outY;
    int velOut;
};
#endif

CustomElementDef customUI[6][10];
int interfaceTelaAlvo = 0;
int interfaceElementoAlvo = 0;
bool selecionandoMidiaElemento = false;

struct LayoutConfig {
    int lX, lY, lW, lH, cX, cY, cW, cH, dX, dY, dW, dH, bX, bY, bW, bH, brX, brY, brW, brH, aX, aY, aW, aH, uX, uY, uW, uH, fT, mX, mY, mTam, lSpcV, lOri, lBg, bBg, bFill, lMk, lHMk, lXH, lYH, lSpcH, fAl, fSc;
    int e1X, e1Y, e1W, e1H, e1O, c1X, c1Y, c1W, c1H, c1O, p1X, p1Y, p1W, p1H, p1O, p1M, p1L;
    int sfxOn, sfxVol, uBg, uTn, uTs;
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
    int a_tol;
    int lSt, fAn, lCurv, lZmCtr;
    int picX, picY, picW, picH;
};

int listXV = 1054, listYV = 204, listSpcV = 120, listXH = 50, listYH = 800, listSpcH = 300, listW = 550, listH = 80, capaX = 150, capaY = 640, capaW = 300, capaH = 400, discoX = 555, discoY = 650, discoW = 300, discoH = 300, backX = 0, backY = 0, backW = 1920, backH = 1080, barX = 95, barY = 911, barW = 345, barH = 15, audioX = 545, audioY = 632, audioW = 321, audioH = 378, upX = 545, upY = 632, upW = 321, upH = 378, fontTam = 35, msgX = 100, msgY = 970, msgTam = 40, listOri = 0, listBg = 0, barBg = 6, barFill = 7, listMark = 8, listHoverMark = 9, fontAlign = 0, fontScroll = 0;
int elem1X = 100, elem1Y = 358, elem1W = 200, elem1H = 200, elem1On = 0;
int ctrl1X = 724, ctrl1Y = 361, ctrl1W = 200, ctrl1H = 200, ctrl1On = 0;
int pont1X = 0, pont1Y = 0, pont1W = 50, pont1H = 50, pont1On = 0, pont1Modo = 0, pont1Lado = 0;
int sfxLigado = 1, sfxVolume = 100, upBg = 0, upTextNorm = 12, upTextSel = 8;
int listStyle = 0, fontAnim = 0, listCurvature = 15, listZoomCentro = 15;
int picX = 150, picY = 100, picW = 730, picH = 410;

bool editMode = false; int editTarget = 0, editType = 0; int mapAcoes[50];

void carregarCustomUI() {
    memset(customUI, 0, sizeof(customUI)); FILE* f = fopen("/data/HyperNeiva/configuracao/settings/custom_ui.bin", "rb");
    if (f) { fread(customUI, sizeof(CustomElementDef), 60, f); fclose(f); }
    for (int t = 0; t < 6; t++) { for (int i = 0; i < 10; i++) { if (customUI[t][i].ativo) { if (customUI[t][i].velIn <= 0) customUI[t][i].velIn = 6; if (customUI[t][i].velOut <= 0) customUI[t][i].velOut = 6; } } }
}
void salvarCustomUI() { FILE* f = fopen("/data/HyperNeiva/configuracao/settings/custom_ui.bin", "wb"); if (f) { fwrite(customUI, sizeof(CustomElementDef), 60, f); fclose(f); } }

void salvarConfiguracao() {
    LayoutConfig cfg; memset(&cfg, 0, sizeof(LayoutConfig));
    cfg.lX = listXV; cfg.lY = listYV; cfg.lW = listW; cfg.lH = listH; cfg.cX = capaX; cfg.cY = capaY; cfg.cW = capaW; cfg.cH = capaH; cfg.dX = discoX; cfg.dY = discoY; cfg.dW = discoW; cfg.dH = discoH; cfg.bX = backX; cfg.bY = backY; cfg.bW = backW; cfg.bH = backH; cfg.brX = barX; cfg.brY = barY; cfg.brW = barW; cfg.brH = barH; cfg.aX = audioX; cfg.aY = audioY; cfg.aW = audioW; cfg.aH = audioH; cfg.uX = upX; cfg.uY = upY; cfg.uW = upW; cfg.uH = upH; cfg.fT = fontTam; cfg.mX = msgX; cfg.mY = msgY; cfg.mTam = msgTam; cfg.lSpcV = listSpcV; cfg.lOri = listOri; cfg.lBg = listBg; cfg.bBg = barBg; cfg.bFill = barFill; cfg.lMk = listMark; cfg.lHMk = listHoverMark; cfg.lXH = listXH; cfg.lYH = listYH; cfg.lSpcH = listSpcH; cfg.fAl = fontAlign; cfg.fSc = fontScroll;
    cfg.e1X = elem1X; cfg.e1Y = elem1Y; cfg.e1W = elem1W; cfg.e1H = elem1H; cfg.e1O = elem1On; cfg.c1X = ctrl1X; cfg.c1Y = ctrl1Y; cfg.c1W = ctrl1W; cfg.c1H = ctrl1H; cfg.c1O = ctrl1On; cfg.p1X = pont1X; cfg.p1Y = pont1Y; cfg.p1W = pont1W; cfg.p1H = pont1H; cfg.p1O = pont1On; cfg.p1M = pont1Modo; cfg.p1L = pont1Lado; cfg.sfxOn = sfxLigado; cfg.sfxVol = sfxVolume;
    cfg.uBg = upBg; cfg.uTn = upTextNorm; cfg.uTs = upTextSel;
    cfg.a_ativo = anim_ativo; cfg.a_ck = anim_usarColorKey; cfg.a_x = anim_posX; cfg.a_y = anim_posY; cfg.a_col = anim_colunas; cfg.a_lin = anim_linhas; cfg.a_vel = anim_velocidade; cfg.a_esc = anim_escala; cfg.a_r = anim_keyR; cfg.a_g = anim_keyG; cfg.a_b = anim_keyB; cfg.a_offX = anim_offsetX; cfg.a_offY = anim_offsetY; cfg.a_fIni = anim_frameInicial; cfg.a_fFim = anim_frameFinal; cfg.a_ck2 = anim_usarColorKey2; cfg.a_r2 = anim_keyR2; cfg.a_g2 = anim_keyG2; cfg.a_b2 = anim_keyB2; cfg.a_autoCtr = anim_autoCenter; cfg.a_tol = anim_tolerancia;
    cfg.lSt = listStyle; cfg.fAn = fontAnim; cfg.lCurv = listCurvature; cfg.lZmCtr = listZoomCentro; cfg.picX = picX; cfg.picY = picY; cfg.picW = picW; cfg.picH = picH;
    for (int i = 0; i < 100; i++) { cfg.a_frameOffX[i] = anim_frameOffsetX[i]; cfg.a_frameOffY[i] = anim_frameOffsetY[i]; }
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "wb");
    if (f) { fwrite(&cfg, sizeof(LayoutConfig), 1, f); fclose(f); strcpy(msgStatus, "POSICOES SALVAS!"); } msgTimer = 90;
    salvarCustomUI();
}

void salvarAnimacaoComNome(const char* nomeArquivo) {
    char caminho[256]; sprintf(caminho, "/data/HyperNeiva/configuracao/%s.bin", nomeArquivo); LayoutConfig cfg; memset(&cfg, 0, sizeof(LayoutConfig));
    FILE* fBase = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "rb"); if (fBase) { fread(&cfg, 1, sizeof(LayoutConfig), fBase); fclose(fBase); }
    cfg.a_ativo = anim_ativo; cfg.a_ck = anim_usarColorKey; cfg.a_x = anim_posX; cfg.a_y = anim_posY; cfg.a_col = anim_colunas; cfg.a_lin = anim_linhas; cfg.a_vel = anim_velocidade; cfg.a_esc = anim_escala; cfg.a_r = anim_keyR; cfg.a_g = anim_keyG; cfg.a_b = anim_keyB; cfg.a_offX = anim_offsetX; cfg.a_offY = anim_offsetY; cfg.a_fIni = anim_frameInicial; cfg.a_fFim = anim_frameFinal; cfg.a_ck2 = anim_usarColorKey2; cfg.a_r2 = anim_keyR2; cfg.a_g2 = anim_keyG2; cfg.a_b2 = anim_keyB2; cfg.a_autoCtr = anim_autoCenter; cfg.a_tol = anim_tolerancia; cfg.lCurv = listCurvature; cfg.lZmCtr = listZoomCentro;
    for (int i = 0; i < 100; i++) { cfg.a_frameOffX[i] = anim_frameOffsetX[i]; cfg.a_frameOffY[i] = anim_frameOffsetY[i]; }
    FILE* f = fopen(caminho, "wb"); if (f) { fwrite(&cfg, sizeof(LayoutConfig), 1, f); fclose(f); }
}

void salvarAnimacaoUnity(const char* nomeArquivo) {
    char caminho[256]; sprintf(caminho, "/data/HyperNeiva/configuracao/%s.png.meta", nomeArquivo); FILE* f = fopen(caminho, "w"); if (!f) return;
    fprintf(f, "fileFormatVersion: 2\nguid: 0123456789abcdef0123456789abcdef\nTextureImporter:\n  spriteMode: 2\n  spriteSheet:\n    serializedVersion: 2\n    sprites:\n");
    int frameW = wSprite / (anim_colunas > 0 ? anim_colunas : 1); int frameH = hSprite / (anim_linhas > 0 ? anim_linhas : 1); int totalFrames = anim_colunas * anim_linhas;
    for (int i = 0; i < totalFrames; i++) {
        int c = i % anim_colunas; int r = (i / anim_colunas) % anim_linhas;
        int unityX = anim_offsetX + (c * frameW); int unityY = hSprite - (anim_offsetY + (r * frameH)) - frameH;
        float pX = 0.5f + ((float)anim_frameOffsetX[i] / (float)frameW); float pY = 0.5f - ((float)anim_frameOffsetY[i] / (float)frameH);
        fprintf(f, "    - serializedVersion: 2\n      name: %s_%d\n      rect:\n        serializedVersion: 2\n        x: %d\n        y: %d\n        width: %d\n        height: %d\n      alignment: 9\n      pivot: {x: %f, y: %f}\n", nomeArquivo, i, unityX, unityY, frameW, frameH, pX, pY);
    } fclose(f);
}

void carregarConfiguracao() {
    LayoutConfig cfg; FILE* f = fopen("/data/HyperNeiva/configuracao/settings/settings.bin", "rb");
    elem1On = 0; ctrl1On = 0; pont1On = 0; sfxLigado = 1; sfxVolume = 100; upBg = 0; upTextNorm = 12; upTextSel = 8;
    anim_ativo = false; anim_usarColorKey = true; anim_posX = 445; anim_posY = -130; anim_colunas = 24; anim_linhas = 19; anim_velocidade = 100; anim_escala = 4.5f; anim_keyR = 55; anim_keyG = 39; anim_keyB = 130; anim_offsetX = 65; anim_offsetY = 0; anim_frameInicial = 0; anim_frameFinal = 7; anim_usarColorKey2 = true; anim_keyR2 = 0; anim_keyG2 = 0; anim_keyB2 = 255; anim_autoCenter = false; anim_tolerancia = 100;
    listStyle = 0; fontAnim = 0; listCurvature = 15; listZoomCentro = 15; picX = 150; picY = 100; picW = 730; picH = 410;
    for (int i = 0; i < 100; i++) { anim_frameOffsetX[i] = 0; anim_frameOffsetY[i] = 0; }

    if (f) {
        memset(&cfg, 0, sizeof(LayoutConfig)); size_t lidos = fread(&cfg, 1, sizeof(LayoutConfig), f);
        if (lidos >= 1) {
            listXV = cfg.lX; listYV = cfg.lY; listW = cfg.lW; listH = cfg.lH; capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH; discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH; backX = cfg.bX; backY = cfg.bY; backW = cfg.bW; backH = cfg.bH; barX = cfg.brX; barY = cfg.brY; barW = cfg.brW; barH = cfg.brH; audioX = cfg.aX; audioY = cfg.aY; audioW = cfg.aW; audioH = cfg.aH; upX = cfg.uX; upY = cfg.uY; upW = cfg.uW; upH = cfg.uH; fontTam = cfg.fT; msgX = cfg.mX; msgY = cfg.mY; msgTam = cfg.mTam; listSpcV = cfg.lSpcV; listOri = cfg.lOri; listBg = cfg.lBg; barBg = cfg.bBg; barFill = cfg.bFill; listMark = cfg.lMk; listHoverMark = cfg.lHMk;
            if (lidos > 150) { listXH = cfg.lXH; listYH = cfg.lYH; listSpcH = cfg.lSpcH; fontAlign = cfg.fAl; fontScroll = cfg.fSc; }
            if (lidos >= 196) { elem1X = cfg.e1X; elem1Y = cfg.e1Y; elem1W = cfg.e1W; elem1H = cfg.e1H; elem1On = cfg.e1O; ctrl1X = cfg.c1X; ctrl1Y = cfg.c1Y; ctrl1W = cfg.c1W; ctrl1H = cfg.c1H; ctrl1On = cfg.c1O; pont1X = cfg.p1X; pont1Y = cfg.p1Y; pont1W = cfg.p1W; pont1H = cfg.p1H; pont1On = cfg.p1O; pont1Modo = cfg.p1M; pont1Lado = cfg.p1L; sfxLigado = cfg.sfxOn; sfxVolume = cfg.sfxVol; upBg = cfg.uBg; upTextNorm = cfg.uTn; upTextSel = cfg.uTs; }
            if (lidos >= sizeof(LayoutConfig) - 810) { anim_ativo = cfg.a_ativo; anim_usarColorKey = cfg.a_ck; anim_posX = cfg.a_x; anim_posY = cfg.a_y; anim_colunas = cfg.a_col; anim_linhas = cfg.a_lin; anim_velocidade = cfg.a_vel; anim_escala = cfg.a_esc; anim_keyR = cfg.a_r; anim_keyG = cfg.a_g; anim_keyB = cfg.a_b; anim_offsetX = cfg.a_offX; anim_offsetY = cfg.a_offY; anim_frameInicial = cfg.a_fIni; anim_frameFinal = cfg.a_fFim; anim_usarColorKey2 = cfg.a_ck2; anim_keyR2 = cfg.a_r2; anim_keyG2 = cfg.a_g2; anim_keyB2 = cfg.a_b2; anim_autoCenter = cfg.a_autoCtr; }
            if (lidos >= sizeof(LayoutConfig) - 4) { if (cfg.a_tol >= 0 && cfg.a_tol < 200000) anim_tolerancia = cfg.a_tol; }
            listStyle = cfg.lSt; fontAnim = cfg.fAn; if (cfg.lCurv > 0 || lidos == sizeof(LayoutConfig)) listCurvature = cfg.lCurv; if (cfg.lZmCtr > 0 || lidos == sizeof(LayoutConfig)) listZoomCentro = cfg.lZmCtr;
            if (listCurvature <= 0 && lidos < sizeof(LayoutConfig)) listCurvature = 15; if (listZoomCentro <= 0 && lidos < sizeof(LayoutConfig)) listZoomCentro = 15;
            if (lidos >= sizeof(LayoutConfig) - 32) { if (cfg.picW > 0) { picX = cfg.picX; picY = cfg.picY; picW = cfg.picW; picH = cfg.picH; } }
            for (int i = 0; i < 100; i++) { anim_frameOffsetX[i] = cfg.a_frameOffX[i]; anim_frameOffsetY[i] = cfg.a_frameOffY[i]; }
        } fclose(f);
    } carregarCustomUI();
}

void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA"); strcpy(nomes[1], "CAPA"); strcpy(nomes[2], "DISCO");
    strcpy(nomes[3], "CAPA DE VIDEO");
    strcpy(nomes[4], "FUNDO"); strcpy(nomes[5], "BARRA LOAD"); strcpy(nomes[6], "MENU AUDIO"); strcpy(nomes[7], "MENU UPLOAD"); strcpy(nomes[8], "FONTE (TEXTO)"); strcpy(nomes[9], "NOTIFICACOES"); strcpy(nomes[10], "EXPLORAR"); strcpy(nomes[11], "ELEMENTO FIXO"); strcpy(nomes[12], "ELEMENTO CTRL"); strcpy(nomes[13], "PONTEIRO"); strcpy(nomes[14], "ELEMENTOS SONOROS");
    strcpy(nomes[15], "ANIMACAO (SPRITES)");
    strcpy(nomes[16], "INTERFACE ELEMENTOS (CUSTOM UI)");
    strcpy(nomes[17], "RESETAR TUDO");
    totalItens = 18; menuAtual = MENU_EDITAR;
}

void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0;

    if (editTarget == 16) {
        strcpy(nomes[totalItens], "TELA ROOT (MENU PRINCIPAL)"); mapAcoes[totalItens++] = 100;
        strcpy(nomes[totalItens], "TELA JOGAR (LISTA DE JOGOS)"); mapAcoes[totalItens++] = 101;
        strcpy(nomes[totalItens], "TELA MIDIA (IMAGENS E TEXTO)"); mapAcoes[totalItens++] = 102;
        strcpy(nomes[totalItens], "TELA BAIXAR (DOWNLOADS E FTP)"); mapAcoes[totalItens++] = 103;
        strcpy(nomes[totalItens], "TELA EDITAR (CONFIGURACOES)"); mapAcoes[totalItens++] = 104;
        strcpy(nomes[totalItens], "TELA EXPLORAR (GERENCIADOR ARQUIVOS)"); mapAcoes[totalItens++] = 105;
    }
    else if (editTarget == 17) {
        strcpy(nomes[totalItens], "+ CRIAR NOVO ELEMENTO (PROCURAR IMAGEM)"); mapAcoes[totalItens++] = 51;
        for (int i = 0; i < 10; i++) {
            if (customUI[interfaceTelaAlvo][i].ativo) {
                sprintf(nomes[totalItens], "EDITAR ELEMENTO %d", i + 1); mapAcoes[totalItens++] = 52 + i;
            }
        }
    }
    else if (editTarget == 18) {
        CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo];

        strcpy(nomes[totalItens], "1. PROCURAR / TROCAR IMAGEM (EXPLORAR)"); mapAcoes[totalItens++] = 51;
        strcpy(nomes[totalItens], "2. POSICAO FINAL (ONDE FICA NA TELA)"); mapAcoes[totalItens++] = 62;
        strcpy(nomes[totalItens], "3. TAMANHO (MANTEM A PROPORCAO ORIGINAL)"); mapAcoes[totalItens++] = 63;
        strcpy(nomes[totalItens], "4. ESTICAR (DEFORMAR LARGURA E ALTURA)"); mapAcoes[totalItens++] = 64;
        sprintf(nomes[totalItens], "5. ANIMACAO DE ENTRADA: %s", el->animInAtiva ? "[LIGADA]" : "[DESLIGADA]"); mapAcoes[totalItens++] = 67;
        strcpy(nomes[totalItens], "6. POSICAO DE NASCIMENTO (DE ONDE VEM)"); mapAcoes[totalItens++] = 65;
        sprintf(nomes[totalItens], "7. VELOCIDADE DA ENTRADA: %d (1 A 10)", el->velIn); mapAcoes[totalItens++] = 68;
        sprintf(nomes[totalItens], "8. ANIMACAO DE SAIDA: %s", el->animOutAtiva ? "[LIGADA]" : "[DESLIGADA]"); mapAcoes[totalItens++] = 69;
        strcpy(nomes[totalItens], "9. FADE OUT (PARA ONDE VAI QUANDO SAIR)"); mapAcoes[totalItens++] = 70;
        sprintf(nomes[totalItens], "10. VELOCIDADE DA SAIDA: %d (1 A 10)", el->velOut); mapAcoes[totalItens++] = 71;
        strcpy(nomes[totalItens], "X. DELETAR ESTE ELEMENTO"); mapAcoes[totalItens++] = 66;
    }
    else if (editTarget == 10) {
        strcpy(nomes[totalItens], "COR LISTA MARCADA"); mapAcoes[totalItens++] = 6; strcpy(nomes[totalItens], "COR LISTA CURSOR+MARC"); mapAcoes[totalItens++] = 7; strcpy(nomes[totalItens], "MENU OPCOES POSICAO"); mapAcoes[totalItens++] = 17; strcpy(nomes[totalItens], "MENU OPCOES TAMANHO"); mapAcoes[totalItens++] = 18; strcpy(nomes[totalItens], "MENU OPCOES ESTICAR"); mapAcoes[totalItens++] = 19; strcpy(nomes[totalItens], "MENU OPCOES COR FUNDO"); mapAcoes[totalItens++] = 20; strcpy(nomes[totalItens], "MENU OPCOES COR TEXTO"); mapAcoes[totalItens++] = 21; strcpy(nomes[totalItens], "MENU OPCOES COR TEXTO SEL"); mapAcoes[totalItens++] = 22;
    }
    else if (editTarget == 8) {
        strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ALINHAMENTO"); mapAcoes[totalItens++] = 10; strcpy(nomes[totalItens], "LIMITES (ROLAGEM)"); mapAcoes[totalItens++] = 11; strcpy(nomes[totalItens], "ANIMACAO DA FONTE"); mapAcoes[totalItens++] = 46;
    }
    else if (editTarget >= 11 && editTarget <= 13) {
        strcpy(nomes[totalItens], "POSICAO"); mapAcoes[totalItens++] = 0; strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ESTICAR"); mapAcoes[totalItens++] = 2; strcpy(nomes[totalItens], "LIGAR / DESLIGAR"); mapAcoes[totalItens++] = 12;
        if (editTarget == 13) { strcpy(nomes[totalItens], "MODO ACOMPANHAR/ESTATICO"); mapAcoes[totalItens++] = 13; strcpy(nomes[totalItens], "LADO (ESQ/DIR/CIMA/BAIXO)"); mapAcoes[totalItens++] = 14; }
    }
    else if (editTarget == 14) {
        strcpy(nomes[totalItens], "LIGAR / DESLIGAR"); mapAcoes[totalItens++] = 15; strcpy(nomes[totalItens], "VOLUME"); mapAcoes[totalItens++] = 16;
    }
    else if (editTarget == 15) {
        strcpy(nomes[totalItens], "POSICAO DA ANIMACAO"); mapAcoes[totalItens++] = 23; strcpy(nomes[totalItens], "ESCALA / TAMANHO"); mapAcoes[totalItens++] = 24; strcpy(nomes[totalItens], "VELOCIDADE"); mapAcoes[totalItens++] = 25; strcpy(nomes[totalItens], "GRADE (COLUNAS E LINHAS)"); mapAcoes[totalItens++] = 26; strcpy(nomes[totalItens], "DESLOCAMENTO INTERNO (X / Y)"); mapAcoes[totalItens++] = 27; strcpy(nomes[totalItens], "FRAME INICIAL E FINAL (LOOP)"); mapAcoes[totalItens++] = 28; strcpy(nomes[totalItens], "TESTAR SPRITE E AJUSTAR PIVOT"); mapAcoes[totalItens++] = 29;
        strcpy(nomes[totalItens], "CHROMA KEY 1 - RED (VERMELHO)"); mapAcoes[totalItens++] = 32; strcpy(nomes[totalItens], "CHROMA KEY 1 - GREEN (VERDE)"); mapAcoes[totalItens++] = 33; strcpy(nomes[totalItens], "CHROMA KEY 1 - BLUE (AZUL)"); mapAcoes[totalItens++] = 34;
        strcpy(nomes[totalItens], "CHROMA KEY 2 LIGADO"); mapAcoes[totalItens++] = 36; strcpy(nomes[totalItens], "CHROMA KEY 2 - RED (VERMELHO)"); mapAcoes[totalItens++] = 37; strcpy(nomes[totalItens], "CHROMA KEY 2 - GREEN (VERDE)"); mapAcoes[totalItens++] = 38; strcpy(nomes[totalItens], "CHROMA KEY 2 - BLUE (AZUL)"); mapAcoes[totalItens++] = 39;
        strcpy(nomes[totalItens], "SELECIONAR COR PARA EXCLUIR (MIRA)"); mapAcoes[totalItens++] = 44; strcpy(nomes[totalItens], "INICIAR ANIMACAO CONTINUA"); mapAcoes[totalItens++] = 30; strcpy(nomes[totalItens], "LIGAR / DESLIGAR VISUAL"); mapAcoes[totalItens++] = 31; strcpy(nomes[totalItens], "TOLERANCIA DE EXCLUSAO DA COR"); mapAcoes[totalItens++] = 42; strcpy(nomes[totalItens], "AUTO-CENTRALIZAR X (INTELIGENTE)"); mapAcoes[totalItens++] = 40; strcpy(nomes[totalItens], "SALVAR ANIMACAO E EXPORTAR UNITY"); mapAcoes[totalItens++] = 41;
    }
    else {
        strcpy(nomes[totalItens], "POSICAO"); mapAcoes[totalItens++] = 0;
        if (editTarget == 9) { strcpy(nomes[totalItens], "TAMANHO DA FONTE"); mapAcoes[totalItens++] = 1; }
        else { strcpy(nomes[totalItens], "TAMANHO"); mapAcoes[totalItens++] = 1; strcpy(nomes[totalItens], "ESTICAR"); mapAcoes[totalItens++] = 2; }

        if (editTarget == 0) {
            strcpy(nomes[totalItens], "ESTILO DE LISTA (ROLETA/PS4/PS3)"); mapAcoes[totalItens++] = 45; strcpy(nomes[totalItens], "CURVATURA DA ROLETA"); mapAcoes[totalItens++] = 47; strcpy(nomes[totalItens], "ZOOM DO SELETOR"); mapAcoes[totalItens++] = 48; strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3; strcpy(nomes[totalItens], "ESPACAMENTO"); mapAcoes[totalItens++] = 4; strcpy(nomes[totalItens], "ORIENTACAO"); mapAcoes[totalItens++] = 5;
        }
        else if (editTarget == 4 || editTarget == 5) { strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3; strcpy(nomes[totalItens], "COR PREENCH"); mapAcoes[totalItens++] = 8; }
        else if (editTarget == 6 || editTarget == 7) { strcpy(nomes[totalItens], "COR DE FUNDO"); mapAcoes[totalItens++] = 3; }
    }
    strcpy(nomes[totalItens], "RESETAR ESTE ITEM"); mapAcoes[totalItens++] = 9; menuAtual = MENU_EDIT_TARGET;
}