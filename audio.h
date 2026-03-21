#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h"

// Tornamos as variáveis globais acessíveis
extern char musicaAtual[256];
extern volatile bool comandoPausar;
extern volatile bool modoRepetir;
extern int volumeGeral; // NOVA VARIÁVEL DE VOLUME

// Funções do módulo de áudio
void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void tocarProximaMusica();
void tocarMusicaAnterior();
void aumentarVolume(); // NOVA FUNÇÃO
void diminuirVolume(); // NOVA FUNÇÃO
void preencherMenuMusicas();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

#endif