#ifndef EDITAR_H
#define EDITAR_H

#include <stdbool.h>

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH;
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark, backX, backY, backW, backH;
extern int fontAlign, fontScroll;

extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;

extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;

extern bool editMode;
extern int editTarget;
extern int editType;
extern int mapAcoes[50];

void salvarConfiguracao();
void carregarConfiguracao();
void preencherMenuEditar();
void preencherMenuEditTarget();
void processarControlesEdicao(unsigned int buttons);
void acaoCircle_Editar();

#endif