#include "audio_musica.h"
#include "audio.h"
#include "explorar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

char caminhosMusicasMenu[3000][256];
char caminhoNavegacaoMusicas[512] = "/data/HyperNeiva/Musicas";

#define MAX_PLAYLIST 2000

void scanPlaylistRecursivo(const char* basePath, char (*lista)[256], int* total) {
    DIR* d = opendir(basePath);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && *total < MAX_PLAYLIST) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, dir->d_name);
        struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
            scanPlaylistRecursivo(fullPath, lista, total);
        }
        else {
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") || strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                strncpy(lista[*total], fullPath, 255); lista[*total][255] = '\0'; (*total)++;
            }
        }
    }
    closedir(d);
}

void scanPastaSimples(const char* basePath, char (*lista)[256], int* total) {
    DIR* d = opendir(basePath);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && *total < MAX_PLAYLIST) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, dir->d_name);
        struct stat st;
        if (dir->d_type != DT_DIR && (dir->d_type != DT_UNKNOWN || (stat(fullPath, &st) == 0 && !S_ISDIR(st.st_mode)))) {
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") || strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                strncpy(lista[*total], fullPath, 255); lista[*total][255] = '\0'; (*total)++;
            }
        }
    }
    closedir(d);
}

bool obterProximaMusica(char* proximaMusicaPath) {
    char (*listaAudios)[256] = (char (*)[256])malloc(MAX_PLAYLIST * 256);
    if (!listaAudios) return false;
    int totalAudios = 0;
    if (modoReproducao == 2 || modoReproducao == 3) {
        char pastaAtual[512]; strcpy(pastaAtual, musicaAtual);
        char* lastSlash = strrchr(pastaAtual, '/');
        if (lastSlash) { *lastSlash = '\0'; scanPastaSimples(pastaAtual, listaAudios, &totalAudios); }
    }
    if (totalAudios == 0) scanPlaylistRecursivo("/data/HyperNeiva/Musicas", listaAudios, &totalAudios);
    if (totalAudios == 0) { free(listaAudios); return false; }
    if (modoReproducao == 3 || modoReproducao == 4) {
        int r = rand() % totalAudios;
        if (totalAudios > 1 && strcmp(listaAudios[r], musicaAtual) == 0) r = (r + 1) % totalAudios;
        strcpy(proximaMusicaPath, listaAudios[r]);
    }
    else {
        for (int i = 0; i < totalAudios - 1; i++) {
            for (int j = i + 1; j < totalAudios; j++) {
                if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                    char temp[256]; strcpy(temp, listaAudios[i]); strcpy(listaAudios[i], listaAudios[j]); strcpy(listaAudios[j], temp);
                }
            }
        }
        int idx = -1;
        for (int i = 0; i < totalAudios; i++) { if (strcmp(listaAudios[i], musicaAtual) == 0) { idx = i; break; } }
        if (idx != -1 && idx + 1 < totalAudios) strcpy(proximaMusicaPath, listaAudios[idx + 1]);
        else strcpy(proximaMusicaPath, listaAudios[0]);
    }
    free(listaAudios); return true;
}

bool obterMusicaAnterior(char* musicaAnteriorPath) {
    char (*listaAudios)[256] = (char (*)[256])malloc(MAX_PLAYLIST * 256);
    if (!listaAudios) return false;
    int totalAudios = 0;
    if (modoReproducao == 2 || modoReproducao == 3) {
        char pastaAtual[512]; strcpy(pastaAtual, musicaAtual);
        char* lastSlash = strrchr(pastaAtual, '/');
        if (lastSlash) { *lastSlash = '\0'; scanPastaSimples(pastaAtual, listaAudios, &totalAudios); }
    }
    if (totalAudios == 0) scanPlaylistRecursivo("/data/HyperNeiva/Musicas", listaAudios, &totalAudios);
    if (totalAudios == 0) { free(listaAudios); return false; }
    if (modoReproducao == 3 || modoReproducao == 4) {
        int r = rand() % totalAudios;
        if (totalAudios > 1 && strcmp(listaAudios[r], musicaAtual) == 0) { r = (r + 1) % totalAudios; }
        strcpy(musicaAnteriorPath, listaAudios[r]);
    }
    else {
        for (int i = 0; i < totalAudios - 1; i++) {
            for (int j = i + 1; j < totalAudios; j++) {
                if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                    char temp[256]; strcpy(temp, listaAudios[i]); strcpy(listaAudios[i], listaAudios[j]); strcpy(listaAudios[j], temp);
                }
            }
        }
        int idx = -1;
        for (int i = 0; i < totalAudios; i++) { if (strcmp(listaAudios[i], musicaAtual) == 0) { idx = i; break; } }
        if (idx != -1 && idx - 1 >= 0) strcpy(musicaAnteriorPath, listaAudios[idx - 1]);
        else strcpy(musicaAnteriorPath, listaAudios[totalAudios - 1]);
    }
    free(listaAudios); return true;
}

bool prepararArquivoAudio(char* caminhoFinal) {
    if (strcmp(musicaAtual, "PARADO") == 0) return false;

    if (strncmp(musicaAtual, "http", 4) == 0) {
        strcpy(caminhoFinal, musicaAtual);
        return true;
    }

    if (strlen(musicaAtual) > 0) {
        FILE* fCustom = fopen(musicaAtual, "rb");
        if (fCustom) { fclose(fCustom); strcpy(caminhoFinal, musicaAtual); return true; }
    }
    const char* pathHD = "/data/HyperNeiva/configuracao/bgm.wav";
    FILE* fHD = fopen(pathHD, "rb");
    if (fHD) { fclose(fHD); strcpy(caminhoFinal, pathHD); return true; }

    const char* pathInterno = "/app0/assets/audio/bgm.wav";
    FILE* fInt = fopen(pathInterno, "rb");
    if (!fInt) return false;

    FILE* fOut = fopen(pathHD, "wb");
    if (!fOut) { fclose(fInt); strcpy(caminhoFinal, pathInterno); return true; }

    char buffer[8192];
    size_t bytesLidos;
    while ((bytesLidos = fread(buffer, 1, sizeof(buffer), fInt)) > 0) fwrite(buffer, 1, bytesLidos, fOut);
    fclose(fInt); fclose(fOut);

    strcpy(caminhoFinal, pathHD);
    return true;
}

struct ItemAudioTemp {
    char nome[64];
    char path[256];
    bool ehPasta;
};

void preencherMenuMusicas() {
    memset(nomes, 0, sizeof(nomes));
    memset(caminhosMusicasMenu, 0, sizeof(caminhosMusicasMenu));
    totalItens = 0;

    if (strcmp(caminhoNavegacaoMusicas, "/data/HyperNeiva/Musicas") == 0) {
        strcpy(nomes[totalItens], "PARAR MUSICA");
        strcpy(caminhosMusicasMenu[totalItens], "PARADO");
        totalItens++;
    }

    struct ItemAudioTemp temp[3000];
    int count = 0;

    DIR* d = opendir(caminhoNavegacaoMusicas);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && count < 2900) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", caminhoNavegacaoMusicas, dir->d_name);

            struct stat st;

            if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
                strncpy(temp[count].nome, dir->d_name, 63);
                temp[count].nome[63] = '\0';
                temp[count].ehPasta = true;
                snprintf(temp[count].path, 255, "%s", fullPath);
                count++;
            }
            else {
                if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") ||
                    strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                    strncpy(temp[count].nome, dir->d_name, 63);
                    temp[count].nome[63] = '\0';
                    temp[count].ehPasta = false;
                    snprintf(temp[count].path, 255, "%s", fullPath);
                    count++;
                }
            }
        }
        closedir(d);
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            bool trocar = false;
            if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
            else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
            if (trocar) { ItemAudioTemp aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
        }
    }

    for (int i = 0; i < count; i++) {
        if (temp[i].ehPasta) snprintf(nomes[totalItens], 64, "[%s]", temp[i].nome);
        else strcpy(nomes[totalItens], temp[i].nome);
        strcpy(caminhosMusicasMenu[totalItens], temp[i].path);
        totalItens++;
    }

    menuAtual = MENU_MUSICAS;
}
