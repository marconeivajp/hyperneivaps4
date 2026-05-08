#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" 
#include "menu.h"
#include "audio_musica.h"
#include "audio_emulador.h"
#include "audio_radio.h"

extern char musicaAtual[256];
extern char ultimaMusicaTocada[256]; 
extern volatile bool comandoPausar;
extern volatile int modoReproducao;
extern int volumeGeral;

// --- VARIÁVEL DO VISUALIZADOR DE ACORDES ---
extern volatile float audioTempoAtual;

void inicializarAudio();
void pararAudio();
void tocarMusicaNova(const char* path);
void tocarProximaMusica();
void tocarMusicaAnterior();
void aumentarVolume();
void diminuirVolume();
void adiantarAudio();
void retrocederAudio();
void salvarConfiguracaoAudio();
void carregarConfiguracaoAudio();

// Tipos de áudio suportados
enum AudioType { AUDIO_NONE, AUDIO_WAV, AUDIO_MP3, AUDIO_STREAM };

#endif