#ifndef EDITAR_H
#define EDITAR_H

#include <stdbool.h>

// Valores padrão
extern const int dLX, dLY, dLW, dLH;
extern const int dCX, dCY, dCW, dCH;
extern const int dDX, dDY, dDW, dDH;
extern const int dBarX, dBarY, dBarW, dBarH; // NOVA: Padrões da Barra

// Variáveis ativas do Layout
extern int listX, listY, listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;
extern int backX, backY, backW, backH;
extern int barX, barY, barW, barH; // NOVA: Variáveis da Barra

// Estados do modo de edição
extern bool editMode;
extern int editTarget;
extern int editType;

// Funções de Edição
void salvarConfiguracao();
void carregarConfiguracao();
void preencherMenuEditar();
void preencherMenuEditTarget();
void processarControlesEdicao(unsigned int buttons);

#endif