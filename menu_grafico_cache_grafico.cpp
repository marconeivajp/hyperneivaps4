#include "menu_grafico_cache_grafico.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include "stb_image.h"

CoverCache coverCache[MAX_GRID_CACHE];
bool cacheIniciado = false;

// Instanciadas aqui para o resto do sistema conseguir ler sem erro "Undefined Symbol"
unsigned char* imgVidEdicao = NULL;
int wVidE = 0, hVidE = 0, cVidE = 0;
bool tentouVidE = false;

extern unsigned char* imgBgDinamico;
extern unsigned char* imgCapaDinamica;
extern unsigned char* imgDiscoDinamico;
extern char nomeItemAnterior[128];
extern unsigned char* uiTextures[10];
extern unsigned char* prevUiTextures[10];
extern int lastTelaId;
extern bool isFirstFrameUI;
extern unsigned char* imgPreview;
extern unsigned char* imgPic1;
extern unsigned char* imgMidia;
extern bool visualizandoMidiaImagem;

void initCoverCache() {
    if (cacheIniciado) return;
    for (int i = 0; i < MAX_GRID_CACHE; i++) { coverCache[i].index = -1; coverCache[i].img = NULL; }
    cacheIniciado = true;
}

void clearCoverCache() {
    for (int i = 0; i < MAX_GRID_CACHE; i++) {
        if (coverCache[i].img) { stbi_image_free(coverCache[i].img); coverCache[i].img = NULL; }
        coverCache[i].index = -1;
    }
}

void cleanOldCache(int startIdx, int endIdx) {
    for (int i = 0; i < MAX_GRID_CACHE; i++) {
        if (coverCache[i].index != -1 && (coverCache[i].index < startIdx || coverCache[i].index > endIdx)) {
            stbi_image_free(coverCache[i].img);
            coverCache[i].img = NULL;
            coverCache[i].index = -1;
        }
    }
}

int getCachedImage(int targetIdx) {
    for (int i = 0; i < MAX_GRID_CACHE; i++) if (coverCache[i].index == targetIdx) return i;
    return -1;
}

int getFreeCacheSlot() {
    for (int i = 0; i < MAX_GRID_CACHE; i++) if (coverCache[i].index == -1) return i;
    return -1;
}

void limparCacheGrafico() {
    clearCoverCache();
    if (imgVidEdicao) { stbi_image_free(imgVidEdicao); imgVidEdicao = NULL; }
    if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
    if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
    if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
    strcpy(nomeItemAnterior, ""); tentouVidE = false;
    for (int i = 0; i < 10; i++) {
        if (uiTextures[i]) { stbi_image_free(uiTextures[i]); uiTextures[i] = NULL; }
        if (prevUiTextures[i]) { stbi_image_free(prevUiTextures[i]); prevUiTextures[i] = NULL; }
    }
    lastTelaId = 0; isFirstFrameUI = true;
    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
    if (imgPic1) { stbi_image_free(imgPic1); imgPic1 = NULL; }
    if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; visualizandoMidiaImagem = false; }
}

unsigned char* carregarMediaCaseInsensitive(const char* pastaPath, const char* nomeProcurado, int* w, int* h, int* c) {
    if (!pastaPath || !nomeProcurado || strlen(nomeProcurado) == 0 || strcmp(nomeProcurado, "..") == 0) return NULL;
    DIR* d = opendir(pastaPath); if (!d) return NULL; struct dirent* dir; char caminhoEncontrado[1024] = ""; bool achou = false;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.') continue; const char* dot = strrchr(dir->d_name, '.');
        if (dot) {
            int lenNome = (int)(dot - dir->d_name); char nomeBase[256]; if (lenNome >= sizeof(nomeBase)) lenNome = sizeof(nomeBase) - 1; strncpy(nomeBase, dir->d_name, lenNome); nomeBase[lenNome] = '\0';
            if (strcasecmp(nomeBase, nomeProcurado) == 0) { if (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) { snprintf(caminhoEncontrado, sizeof(caminhoEncontrado), "%s/%s", pastaPath, dir->d_name); achou = true; break; } }
        }
    } closedir(d); if (achou) return stbi_load(caminhoEncontrado, w, h, c, 4); return NULL;
}