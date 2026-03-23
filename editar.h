#ifndef EDITAR_H
#define EDITAR_H
#include <stdbool.h>

extern const int dLXV, dLYV, dListSpcV;
extern const int dLXH, dLYH, dListSpcH;
extern const int dLW, dLH, dCX, dCY, dCW, dCH, dDX, dDY, dDW, dDH;
extern const int dBarX, dBarY, dBarW, dBarH, dAudioX, dAudioY, dAudioW, dAudioH, dUpX, dUpY, dUpW, dUpH;
extern const int dFontTam, dMsgX, dMsgY, dMsgTam, dListOri, dListBg, dBarBg, dBarFill, dListMark, dListHoverMark;
extern const int dFontAlign, dFontScroll;

// PADRÕES DE ELEMENTOS
extern const int dElem1X, dElem1Y, dElem1W, dElem1H, dElem1On;
extern const int dCtrl1X, dCtrl1Y, dCtrl1W, dCtrl1H, dCtrl1On;
extern const int dPont1X, dPont1Y, dPont1W, dPont1H, dPont1On, dPont1Modo, dPont1Lado;

// PADRÕES DE ELEMENTOS SONOROS (Volume e Estado)
extern const int dSfxLigado, dSfxVolume;

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH, barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH, fontTam, msgX, msgY, msgTam, listOri, listBg, barBg, barFill, listMark, listHoverMark, fontAlign, fontScroll;

// VARIÁVEIS DE ELEMENTOS
extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;

// VARIÁVEIS DE ELEMENTOS SONOROS
extern int sfxLigado, sfxVolume;

// VARIÁVEIS DO MENU DE EDIÇÃO (Corrigido para 20)
extern bool editMode; extern int editTarget; extern int editType; extern int mapAcoes[20];

void salvarConfiguracao(); void carregarConfiguracao(); void preencherMenuEditar(); void preencherMenuEditTarget(); void processarControlesEdicao(unsigned int buttons);

#endif