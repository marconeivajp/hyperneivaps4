#ifndef RADIO_H
#define RADIO_H

#include "menu.h"

void preencherMenuRadioCategorias();
void buscarEstacoesRadio(const char* query, bool isPodcast);
void carregarFavoritosRadio();
void alternarFavoritoRadio(int index);
void acaoTriangle_Radio();
void acaoCross_Radio();
void acaoCircle_Radio();

#endif