#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" // Necessário para acessar o enum MenuLevel
#include "menu.h"

// Truque para manter o explorar.h intacto: definimos o MENU_MUSICAS aqui
#ifndef MENU_MUSICAS
#define MENU_MUSICAS ((MenuLevel)11)
#endif

// Tornamos a musicaAtual acessível (global) para todo o sistema ler
extern char musicaAtual[256];

// Funções do módulo de áudio
void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void preencherMenuMusicas();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

#endif