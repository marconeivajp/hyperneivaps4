#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h"

// Tornamos a musicaAtual e o comandoPausar acessíveis para o menu de áudio
extern char musicaAtual[256];
extern volatile bool comandoPausar;

// Funções do módulo de áudio
void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void tocarProximaMusica();   // NOVA FUNÇÃO
void tocarMusicaAnterior();  // NOVA FUNÇÃO
void preencherMenuMusicas();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

#endif