#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "stb_truetype.h"

// Variáveis da fonte partilhadas
extern stbtt_fontinfo font;
extern int temF;

// Funções de desenho
void desenharRedimensionado(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY);
void desenharDiscoRedondo(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY);
void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);

#endif