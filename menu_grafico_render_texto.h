#ifndef RENDER_TEXTO_H
#define RENDER_TEXTO_H

#include <stdint.h>
#include <stdbool.h>

uint32_t getSysColor(int index);
int medirLarguraTexto(const char* texto, int tam);
void desenharTextoClip(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor, int clipX, int clipW);
void desenharTextoAlinhadoAnimado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor, bool isSelected);
void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor);

#endif