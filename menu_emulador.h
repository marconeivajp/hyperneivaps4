#ifndef MENU_EMULADOR_H
#define MENU_EMULADOR_H

#include <stdint.h>

extern int menuEmuSelecao;
extern bool menuEmuladorAtivo;

void atualizarMenuEmulador(uint32_t buttons, uint32_t ultimos);
void desenharMenuEmulador(uint32_t* pixels);

#endif
