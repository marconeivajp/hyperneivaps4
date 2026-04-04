#ifndef MENU_GRAFICO_CACHE_GRAFICO_H
#define MENU_GRAFICO_CACHE_GRAFICO_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_GRID_CACHE 64 

struct CoverCache {
    int index;
    unsigned char* img;
    int w, h, c;
};

extern CoverCache coverCache[MAX_GRID_CACHE];

// Variaveis de memoria garantidas para o Linker achar
extern unsigned char* imgVidEdicao;
extern int wVidE, hVidE, cVidE;
extern bool tentouVidE;

void initCoverCache();
void clearCoverCache();
void cleanOldCache(int startIdx, int endIdx);
int getCachedImage(int targetIdx);
int getFreeCacheSlot();
void limparCacheGrafico();
unsigned char* carregarMediaCaseInsensitive(const char* pastaPath, const char* nomeProcurado, int* w, int* h, int* c);

#endif