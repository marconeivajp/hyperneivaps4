#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h"

// Tornamos as variáveis globais acessíveis
extern char musicaAtual[256];
extern volatile bool comandoPausar;
extern volatile bool modoRepetir;
extern int volumeGeral;

// NOVA VARIÁVEL: Guarda o caminho absoluto das músicas no menu
extern char caminhosMusicasMenu[3000][256];

// Funções do módulo de áudio
void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void tocarProximaMusica();
void tocarMusicaAnterior();
void aumentarVolume();
void diminuirVolume();
void adiantarAudio();
void retrocederAudio();
void preencherMenuMusicas();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

#endif