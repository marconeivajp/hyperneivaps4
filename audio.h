#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h"

extern char musicaAtual[256];
extern volatile bool comandoPausar;
extern volatile bool modoRepetir;
extern int volumeGeral;

extern char caminhosMusicasMenu[3000][256];
extern char caminhoNavegacaoMusicas[512]; // Variável que gerencia as pastas no Player

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