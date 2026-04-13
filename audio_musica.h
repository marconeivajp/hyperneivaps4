#ifndef AUDIO_PLAYLIST_H
#define AUDIO_PLAYLIST_H

#include <stdbool.h>

extern char caminhosMusicasMenu[3000][256];
extern char caminhoNavegacaoMusicas[512];

void preencherMenuMusicas();
bool prepararArquivoAudio(char* caminhoFinal);
bool obterProximaMusica(char* proximaMusicaPath);
bool obterMusicaAnterior(char* musicaAnteriorPath);

// Funções internas de scan (podem ser úteis externamente)
void scanPlaylistRecursivo(const char* basePath, char (*lista)[256], int* total);
void scanPastaSimples(const char* basePath, char (*lista)[256], int* total);

#endif
