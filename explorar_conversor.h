#ifndef EXPLORAR_CONVERSOR_H
#define EXPLORAR_CONVERSOR_H

void obterNomeJogoSFO(const char* cusa, char* nomeSaida);
void redimensionarImagem(unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh);
void salvarComoDDS(const char* filepath, unsigned char* img, int w, int h);

#endif