#ifndef AUDIO_H
#define AUDIO_H

#include "explorar.h" // Necessário para acessar o enum MenuLevel

// Definição do nível de menu para músicas
#ifndef MENU_MUSICAS
#define MENU_MUSICAS ((MenuLevel)11)
#endif
#ifndef MENU_AUDIO_OPCOES
#define MENU_AUDIO_OPCOES ((MenuLevel)12)
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
void abrirMenuAudioOpcoes();
void tratarSelecaoAudio(int op);

#endif