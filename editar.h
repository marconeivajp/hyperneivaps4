#ifndef EDITAR_H
#define EDITAR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool ativo;
    char caminho[256];
    int pX, pY, pW, pH;
    bool animInAtiva;
    int inX, inY;
    int velIn;
    bool animOutAtiva;
    int outX, outY;
    int velOut;
} CustomElementDef;

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
    int gX, gY, gIW, gIH, gCols, gLins, gSpcX, gSpcY;
    int numSlotsPadrao;
    int corSelecao;
    int msgColIndex;
    int fontColIndex;
    int p1Double;
    int listLoop;
    int listFocus;
    int barTextCol;
};

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH;
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark, backX, backY, backW, backH;
extern int fontAlign, fontScroll;
extern int numSlotsPadrao;
extern int corSelecao;
extern int msgColIndex, fontColIndex;
extern int p1Double, listLoop, listFocus, barTextCol;

// --- CONFIGURAÇÃO DA CAPA DE VÍDEO (FUNDO/PIC1) ---
extern int picX, picY, picW, picH;

extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;

extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;
extern int numSlotsPadrao;

// --- VARIÁVEIS DE ESTILO E ANIMAÇÃO ---
extern int listStyle;
extern int fontAnim;
extern int listCurvature;
extern int listZoomCentro;

extern bool editMode;
extern int editTarget;
extern int editType;
extern int mapAcoes[50];

void salvarConfiguracao();
void salvarAnimacaoComNome(const char* nomeArquivo);
void salvarAnimacaoUnity(const char* nomeArquivo);
void carregarConfiguracao();
void preencherMenuEditar();
void preencherMenuEditTarget();
void processarControlesEdicao(unsigned int buttons);
void acaoCircle_Editar();

#endif	