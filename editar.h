#ifndef EDITAR_H
#define EDITAR_H

#include <stdbool.h>

// Valores padrão
extern const int dLXV, dLYV, dListSpcV;
extern const int dLXH, dLYH, dListSpcH;
extern const int dLW, dLH;
extern const int dCX, dCY, dCW, dCH;
extern const int dDX, dDY, dDW, dDH;
extern const int dBarX, dBarY, dBarW, dBarH;
extern const int dAudioX, dAudioY, dAudioW, dAudioH;
extern const int dUpX, dUpY, dUpW, dUpH;
extern const int dFontTam, dMsgX, dMsgY, dMsgTam;
extern const int dListOri, dListBg;
extern const int dBarBg, dBarFill, dListMark, dListHoverMark;
extern const int dFontAlign, dFontScroll; // NOVOS PADRÕES DA FONTE

// Variáveis ativas do Layout
extern int listXV, listYV, listSpcV;
extern int listXH, listYH, listSpcH;
extern int listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;
extern int backX, backY, backW, backH;
extern int barX, barY, barW, barH;
extern int audioX, audioY, audioW, audioH;
extern int upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam;
extern int listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark;
extern int fontAlign, fontScroll; // NOVAS VARIÁVEIS DA FONTE

// Estados do modo de edição
extern bool editMode;
extern int editTarget;
extern int editType;
extern int mapAcoes[15];

// Funções de Edição
void salvarConfiguracao();
void carregarConfiguracao();
void preencherMenuEditar();
void preencherMenuEditTarget();
void processarControlesEdicao(unsigned int buttons);

#endif