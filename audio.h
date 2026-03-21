#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h"

// Tornamos as variáveis globais acessíveis
extern char musicaAtual[256];
extern volatile bool comandoPausar;
extern volatile bool modoRepetir;
extern int volumeGeral;

// Funções do módulo de áudio
void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void tocarProximaMusica();
void tocarMusicaAnterior();
void aumentarVolume();
void diminuirVolume();
void adiantarAudio();   // NOVA FUNÇÃO
void retrocederAudio(); // NOVA FUNÇÃO
void preencherMenuMusicas();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

#endif