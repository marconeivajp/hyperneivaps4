#ifndef MENU_GRAFICO_LAYOUT_H
#define MENU_GRAFICO_LAYOUT_H

#include <stdint.h>
#include "menu.h"

void renderizarCustomUI(uint32_t* p, int telaId);
void desenharListas(uint32_t* p, int refPainel);
void desenharRodapeEdicao(uint32_t* p);

#endif