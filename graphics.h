#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>
#include "stb_truetype.h"

// Variáveis da fonte partilhadas
extern stbtt_fontinfo font;
extern int temF;

// --- FUNÇÕES DE VÍDEO DO PS4 (NOVO) ---
void inicializarVideo();
uint32_t* obterBufferVideo();
void submeterTela();
void submeterTelaSemPausa(); // Sem o usleep de 16ms

// Funções de desenho
void desenharRedimensionado(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY);
void desenharDiscoRedondo(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY);
void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);

// Nova função para o emulador
void desenharBufferEmulador(const void* data, unsigned width, unsigned height, size_t pitch);
void capturarUltimoFrame();

#endif