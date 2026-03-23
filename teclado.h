#ifndef TECLADO_H
#define TECLADO_H

#include <stdint.h>
#include <stdbool.h>

// Funções de controlo
void inicializarTecladoMouse();
void fecharTecladoMouse();
unsigned int atualizarTecladoMouse(unsigned int botoesPadrao);
void desenharMouse(uint32_t* p);

#endif