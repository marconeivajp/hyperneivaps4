#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h" // Traz a lista correta de menus

// Tornamos a musicaAtual, comandoPausar e modoRepetir acessíveis
extern char musicaAtual[256];
extern volatile bool comandoPausar;
extern volatile bool modoRepetir;

// Funções do módulo de áudio
void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void tocarProximaMusica();
void tocarMusicaAnterior();
void preencherMenuMusicas();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

#endif