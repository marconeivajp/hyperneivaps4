// 1. Bibliotecas Padrão C/C++
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list va_list
#endif
#endif

// 3. Headers do SDK do PS4
#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

// 4. Headers Locais do Projeto
#include "audio.h"
#include "explorar.h" 

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

static int audioPort = -1;
static volatile bool audioRodando = false;
static pthread_t audioThreadId;
static bool sistemaAudioIniciado = false;

static volatile bool comandoTrocar = false;
volatile bool comandoPausar = false;
volatile bool modoRepetir = false;

volatile int comandoBuscarSegundos = 0;
int volumeGeral = 100;
char musicaAtual[256] = "PARADO";

// GLOBAL: Mapeia o caminho absoluto de cada música do menu
char caminhosMusicasMenu[3000][256];

enum AudioType { AUDIO_NONE, AUDIO_WAV, AUDIO_MP3 };

// ==========================================
// FUNÇÕES DE CONTROLO DE TEMPO / VOLUME
// ==========================================
void adiantarAudio() { comandoBuscarSegundos = 10; }
void retrocederAudio() { comandoBuscarSegundos = -10; }

void aumentarVolume() {
    volumeGeral += 10;
    if (volumeGeral > 100) volumeGeral = 100;
    salvarConfiguracaoAudio();
}

void diminuirVolume() {
    volumeGeral -= 10;
    if (volumeGeral < 0) volumeGeral = 0;
    salvarConfiguracaoAudio();
}

void salvarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "wb");
    if (f) {
        fwrite(musicaAtual, 1, sizeof(musicaAtual), f);
        fwrite(&volumeGeral, 1, sizeof(int), f);
        fclose(f);
    }
}

void carregarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "rb");
    if (f) {
        fread(musicaAtual, 1, sizeof(musicaAtual), f);
        if (fread(&volumeGeral, 1, sizeof(int), f) <= 0) volumeGeral = 100;
        fclose(f);
    }
    else {
        volumeGeral = 100;
    }
}

// ==========================================
// FUNÇÕES DE BUSCA RECURSIVA DE MÚSICAS
// ==========================================
#define MAX_PLAYLIST 2000

// Busca as músicas para a Playlist de fundo
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
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") ||
                strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                strncpy(lista[*total], fullPath, 255);
                lista[*total][255] = '\0';
                (*total)++;
            }
        }
    }
    closedir(d);
}

static bool obterProximaMusica(char* proximaMusicaPath) {
    // Usa malloc para não estourar a memória RAM da thread no PS4
    char (*listaAudios)[256] = (char (*)[256])malloc(MAX_PLAYLIST * 256);
    if (!listaAudios) return false;

    int totalAudios = 0;
    scanPlaylistRecursivo("/data/HyperNeiva/Musicas", listaAudios, &totalAudios);

    if (totalAudios == 0) {
        free(listaAudios);
        return false;
    }

    // Ordena alfabeticamente para seguir a mesma lógica de pastas
    for (int i = 0; i < totalAudios - 1; i++) {
        for (int j = i + 1; j < totalAudios; j++) {
            if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                char temp[256];
                strcpy(temp, listaAudios[i]);
                strcpy(listaAudios[i], listaAudios[j]);
                strcpy(listaAudios[j], temp);
            }
        }
    }

    int idx = -1;
    for (int i = 0; i < totalAudios; i++) {
        // Agora compara com o caminho inteiro!
        if (strcmp(listaAudios[i], musicaAtual) == 0) { idx = i; break; }
    }

    if (idx != -1 && idx + 1 < totalAudios) {
        strcpy(proximaMusicaPath, listaAudios[idx + 1]);
    }
    else {
        strcpy(proximaMusicaPath, listaAudios[0]);
    }

    free(listaAudios);
    return true;
}

static bool obterMusicaAnterior(char* musicaAnteriorPath) {
    char (*listaAudios)[256] = (char (*)[256])malloc(MAX_PLAYLIST * 256);
    if (!listaAudios) return false;

    int totalAudios = 0;
    scanPlaylistRecursivo("/data/HyperNeiva/Musicas", listaAudios, &totalAudios);

    if (totalAudios == 0) {
        free(listaAudios);
        return false;
    }

    for (int i = 0; i < totalAudios - 1; i++) {
        for (int j = i + 1; j < totalAudios; j++) {
            if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                char temp[256];
                strcpy(temp, listaAudios[i]);
                strcpy(listaAudios[i], listaAudios[j]);
                strcpy(listaAudios[j], temp);
            }
        }
    }

    int idx = -1;
    for (int i = 0; i < totalAudios; i++) {
        if (strcmp(listaAudios[i], musicaAtual) == 0) { idx = i; break; }
    }

    if (idx != -1 && idx - 1 >= 0) {
        strcpy(musicaAnteriorPath, listaAudios[idx - 1]);
    }
    else {
        strcpy(musicaAnteriorPath, listaAudios[totalAudios - 1]);
    }

    free(listaAudios);
    return true;
}

static bool prepararArquivoAudio(char* caminhoFinal) {
    if (strcmp(musicaAtual, "PARADO") == 0) return false;
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

static void* audioThreadFunc(void* argp) {
    if (!sistemaAudioIniciado) { sceAudioOutInit(); sistemaAudioIniciado = true; }
    int32_t userId;
    if (sceUserServiceGetInitialUser(&userId) < 0) userId = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
    audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (audioPort < 0) { audioRodando = false; return NULL; }

    drwav wav; drmp3 mp3; AudioType currentAudioType = AUDIO_NONE;
    int16_t pSampleData[256 * 2];
    char caminhoAudio[256];
    uint64_t currentFrame = 0;

    if (!comandoPausar && prepararArquivoAudio(caminhoAudio)) {
        if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
            if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
        }
        else {
            if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
        }
    }

    while (audioRodando) {

        if (comandoBuscarSegundos != 0) {
            if (currentAudioType != AUDIO_NONE) {
                int sampleRate = (currentAudioType == AUDIO_WAV) ? wav.sampleRate : mp3.sampleRate;
                int64_t frameOffset = (int64_t)comandoBuscarSegundos * sampleRate;
                int64_t targetFrame = (int64_t)currentFrame + frameOffset;

                if (targetFrame < 0) targetFrame = 0;

                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, (uint64_t)targetFrame);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, (uint64_t)targetFrame);
                currentFrame = (uint64_t)targetFrame;
            }
            comandoBuscarSegundos = 0;
        }

        if (comandoTrocar) {
            comandoTrocar = false;
            if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
            else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
            currentAudioType = AUDIO_NONE;

            if (prepararArquivoAudio(caminhoAudio)) {
                if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
                    if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
                }
                else {
                    if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
                }
            }
            currentFrame = 0;
        }

        if (comandoPausar || currentAudioType == AUDIO_NONE) {
            for (int i = 0; i < 256 * 2; i++) pSampleData[i] = 0;
            sceAudioOutOutput(audioPort, pSampleData);
            continue;
        }

        size_t framesLidos = 0;
        uint32_t currentChannels = 2;

        if (currentAudioType == AUDIO_WAV) {
            framesLidos = drwav_read_pcm_frames_s16(&wav, 256, pSampleData);
            currentChannels = wav.channels;
        }
        else if (currentAudioType == AUDIO_MP3) {
            framesLidos = drmp3_read_pcm_frames_s16(&mp3, 256, pSampleData);
            currentChannels = mp3.channels;
        }

        if (framesLidos > 0 && currentChannels == 1) {
            for (int i = framesLidos - 1; i >= 0; i--) {
                pSampleData[i * 2] = pSampleData[i];
                pSampleData[i * 2 + 1] = pSampleData[i];
            }
        }

        currentFrame += framesLidos;

        if (framesLidos == 0) {
            if (modoRepetir) {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
            }
            // Checa se é arquivo do HyperNeiva pra avançar a playlist
            else if (strstr(musicaAtual, "/data/HyperNeiva/Musicas/") != NULL) {
                char proxima[256];
                if (obterProximaMusica(proxima)) {
                    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
                    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
                    currentAudioType = AUDIO_NONE;

                    strcpy(musicaAtual, proxima);
                    salvarConfiguracaoAudio();

                    bool sucesso = false;
                    if (strstr(musicaAtual, ".mp3") || strstr(musicaAtual, ".MP3")) {
                        if (drmp3_init_file(&mp3, musicaAtual, NULL)) { currentAudioType = AUDIO_MP3; sucesso = true; }
                    }
                    else {
                        if (drwav_init_file(&wav, musicaAtual, NULL)) { currentAudioType = AUDIO_WAV; sucesso = true; }
                    }
                    if (sucesso) {
                        currentFrame = 0;
                        continue;
                    }
                }
            }
            else {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
            }
            continue;
        }

        if (framesLidos < 256) {
            for (size_t i = framesLidos * 2; i < 256 * 2; i++) pSampleData[i] = 0;
        }

        if (volumeGeral < 100) {
            float fatorVolume = volumeGeral / 100.0f;
            for (int i = 0; i < 256 * 2; i++) {
                pSampleData[i] = (int16_t)(pSampleData[i] * fatorVolume);
            }
        }

        sceAudioOutOutput(audioPort, pSampleData);
    }

    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
    sceAudioOutClose(audioPort);
    return NULL;
}

void inicializarAudio() {
    if (audioRodando) return;
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
    carregarConfiguracaoAudio();
    audioRodando = true;
    if (strcmp(musicaAtual, "PARADO") == 0) comandoPausar = true; else comandoPausar = false;
    comandoTrocar = false;
    if (pthread_create(&audioThreadId, NULL, audioThreadFunc, NULL) != 0) audioRodando = false;
}

void pararAudio() {
    if (!audioRodando) return;
    audioRodando = false;
    pthread_join(audioThreadId, NULL);
}

void tocarMusicaNova(const char* path) {
    if (strcmp(path, "PARADO") == 0) {
        strcpy(musicaAtual, "PARADO");
        salvarConfiguracaoAudio();
        comandoPausar = true;
        comandoTrocar = false;
        return;
    }
    // O controle agora já manda o caminho completo!
    strcpy(musicaAtual, path);

    salvarConfiguracaoAudio();
    comandoPausar = false;
    comandoTrocar = true;
}

void tocarProximaMusica() {
    if (strcmp(musicaAtual, "PARADO") == 0 || strlen(musicaAtual) == 0) return;
    char proxima[256];
    if (obterProximaMusica(proxima)) {
        tocarMusicaNova(proxima);
    }
}

void tocarMusicaAnterior() {
    if (strcmp(musicaAtual, "PARADO") == 0 || strlen(musicaAtual) == 0) return;
    char anterior[256];
    if (obterMusicaAnterior(anterior)) {
        tocarMusicaNova(anterior);
    }
}

// Busca as músicas para o Menu do Player de forma recursiva
void coletarMusicasMenuRecursivo(const char* basePath) {
    DIR* d = opendir(basePath);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && totalItens < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, dir->d_name);

        struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
            coletarMusicasMenuRecursivo(fullPath);
        }
        else {
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") ||
                strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {

                // Salva só o nome para o menu
                strncpy(nomes[totalItens], dir->d_name, 63);
                nomes[totalItens][63] = '\0';

                // Salva o caminho oculto na nova variável global
                strncpy(caminhosMusicasMenu[totalItens], fullPath, 255);
                caminhosMusicasMenu[totalItens][255] = '\0';

                totalItens++;
            }
        }
    }
    closedir(d);
}

void preencherMenuMusicas() {
    memset(nomes, 0, sizeof(nomes));
    memset(caminhosMusicasMenu, 0, sizeof(caminhosMusicasMenu));
    totalItens = 0;

    strcpy(nomes[totalItens], "PARAR MUSICA");
    strcpy(caminhosMusicasMenu[totalItens], "PARADO");
    totalItens++;

    coletarMusicasMenuRecursivo("/data/HyperNeiva/Musicas");

    // Ordenar o menu (ignorando a opção 0 que é o PARAR MUSICA)
    for (int i = 1; i < totalItens - 1; i++) {
        for (int j = i + 1; j < totalItens; j++) {
            if (strcasecmp(nomes[i], nomes[j]) > 0) {
                char tempNome[64];
                char tempPath[256];

                strcpy(tempNome, nomes[i]);
                strcpy(nomes[i], nomes[j]);
                strcpy(nomes[j], tempNome);

                strcpy(tempPath, caminhosMusicasMenu[i]);
                strcpy(caminhosMusicasMenu[i], caminhosMusicasMenu[j]);
                strcpy(caminhosMusicasMenu[j], tempPath);
            }
        }
    }

    menuAtual = MENU_MUSICAS;
}