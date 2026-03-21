#pragma once
#ifndef MENU_AUDIO_H
#define MENU_AUDIO_H

#include <stdint.h>
#include "menu.h" // Isso já traz a sua lista (enum) completa e oficial do sistema!

// Disponibiliza a lista de nomes para quem quiser usar
extern const char* listaOpcoesAudio[11];

void desenharMenuAudio(uint32_t* p);
void abrirMenuAudioOpcoes();
void tratarSelecaoAudio(int op);

#endif