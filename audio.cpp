// 1. BLOCO DE COMPATIBILIDADE INTELLISENSE (Silencia erros no Visual Studio)
#ifdef __INTELLISENSE__
#include <stdarg.h>
#include <stddef.h>
#undef __builtin_va_list
#define __builtin_va_list va_list
#define __builtin_va_start(a,b)
#define __builtin_va_end(a)
#define __builtin_va_arg(a,b) ((b)0)
#define __attribute__(x)
#define __inline__ inline
#define __restrict
// Impede que o IntelliSense tente redeclarar funções de memória do SDK
#define _STDLIB_H_
#define _MALLOC_H_
#define _SYS_MALLOC_H_
#define _SYS_CDEFS_H_
#endif

// 2. Bibliotecas Padrão C/C++
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>

// 3. Headers do SDK do PS4
#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

// 4. Headers Locais do Projeto
#include "audio.h"
#include "explorar.h" 

// 5. Implementações Single-Header
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

static int audioPort = -1;
static volatile bool audioRodando = false;
static pthread_t audioThreadId;
static bool sistemaAudioIniciado = false;

static volatile bool comandoTrocar = false;
static volatile bool comandoPausar = false;

char musicaAtual[256] = "PARADO";

enum AudioType { AUDIO_NONE, AUDIO_WAV, AUDIO_MP3 };

void salvarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "wb");
    if (f) {
        fwrite(musicaAtual, 1, sizeof(musicaAtual), f);
        fclose(f);
    }
}

void carregarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "rb");
    if (f) {
        fread(musicaAtual, 1, sizeof(musicaAtual), f);
        fclose(f);
    }
}

static bool obterProximaMusica(char* proximaMusicaPath) {
    DIR* d = opendir("/data/HyperNeiva/Musicas");
    if (!d) return false;

    char listaAudios[100][128];
    int totalAudios = 0;

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") ||
            strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
            strncpy(listaAudios[totalAudios], dir->d_name, 127);
            totalAudios++;
            if (totalAudios >= 100) break;
        }
    }
    closedir(d);

    if (totalAudios == 0) return false;

    for (int i = 0; i < totalAudios - 1; i++) {
        for (int j = i + 1; j < totalAudios; j++) {
            if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                char temp[128];
                strcpy(temp, listaAudios[i]);
                strcpy(listaAudios[i], listaAudios[j]);
                strcpy(listaAudios[j], temp);
            }
        }
    }

    char* nomeAtual = strrchr(musicaAtual, '/');
    if (nomeAtual) nomeAtual++; else nomeAtual = musicaAtual;

    int idx = -1;
    for (int i = 0; i < totalAudios; i++) {
        if (strcmp(listaAudios[i], nomeAtual) == 0) {
            idx = i;
            break;
        }
    }

    if (idx != -1 && idx + 1 < totalAudios) {
        sprintf(proximaMusicaPath, "/data/HyperNeiva/Musicas/%s", listaAudios[idx + 1]);
    }
    else {
        sprintf(proximaMusicaPath, "/data/HyperNeiva/Musicas/%s", listaAudios[0]);
    }
    return true;
}

static bool prepararArquivoAudio(char* caminhoFinal) {
    if (strcmp(musicaAtual, "PARADO") == 0) return false;

    if (strlen(musicaAtual) > 0) {
        FILE* fCustom = fopen(musicaAtual, "rb");
        if (fCustom) {
            fclose(fCustom);
            strcpy(caminhoFinal, musicaAtual);
            return true;
        }
    }

    const char* pathHD = "/data/HyperNeiva/configuracao/bgm.wav";
    FILE* fHD = fopen(pathHD, "rb");
    if (fHD) {
        fclose(fHD);
        strcpy(caminhoFinal, pathHD);
        return true;
    }

    const char* pathInterno = "/app0/assets/audio/bgm.wav";
    FILE* fInt = fopen(pathInterno, "rb");
    if (!fInt) return false;

    FILE* fOut = fopen(pathHD, "wb");
    if (!fOut) {
        fclose(fInt);
        strcpy(caminhoFinal, pathInterno);
        return true;
    }

    char buffer[8192];
    size_t bytesLidos;
    while ((bytesLidos = fread(buffer, 1, sizeof(buffer), fInt)) > 0) {
        fwrite(buffer, 1, bytesLidos, fOut);
    }
    fclose(fInt); fclose(fOut);

    strcpy(caminhoFinal, pathHD);
    return true;
}

static void* audioThreadFunc(void* argp) {
    if (!sistemaAudioIniciado) {
        sceAudioOutInit();
        sistemaAudioIniciado = true;
    }

    int32_t userId;
    if (sceUserServiceGetInitialUser(&userId) < 0) {
        userId = 0xFF;
    }

    audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (audioPort < 0) {
        audioRodando = false;
        return NULL;
    }

    drwav wav;
    drmp3 mp3;
    AudioType currentAudioType = AUDIO_NONE;
    int16_t pSampleData[256 * 2];
    char caminhoAudio[256];

    if (!comandoPausar && prepararArquivoAudio(caminhoAudio)) {
        if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
            if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
        }
        else {
            if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
        }
    }

    while (audioRodando) {
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
        }

        if (comandoPausar || currentAudioType == AUDIO_NONE) {
            memset(pSampleData, 0, sizeof(pSampleData));
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
            for (int i = (int)framesLidos - 1; i >= 0; i--) {
                pSampleData[i * 2] = pSampleData[i];
                pSampleData[i * 2 + 1] = pSampleData[i];
            }
        }

        if (framesLidos == 0) {
            if (strstr(musicaAtual, "/data/HyperNeiva/Musicas/") != NULL) {
                char proxima[256];
                if (obterProximaMusica(proxima)) {
                    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
                    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
                    currentAudioType = AUDIO_NONE;

                    strcpy(musicaAtual, proxima);
                    salvarConfiguracaoAudio();

                    if (strstr(musicaAtual, ".mp3") || strstr(musicaAtual, ".MP3")) {
                        if (drmp3_init_file(&mp3, musicaAtual, NULL)) currentAudioType = AUDIO_MP3;
                    }
                    else {
                        if (drwav_init_file(&wav, musicaAtual, NULL)) currentAudioType = AUDIO_WAV;
                    }
                    continue;
                }
            }
            else {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
            }
            continue;
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
    comandoPausar = (strcmp(musicaAtual, "PARADO") == 0);
    comandoTrocar = false;
    pthread_create(&audioThreadId, NULL, audioThreadFunc, NULL);
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
    if (strstr(path, "/data/") != NULL) strcpy(musicaAtual, path);
    else sprintf(musicaAtual, "/data/HyperNeiva/Musicas/%s", path);
    salvarConfiguracaoAudio();
    comandoPausar = false;
    comandoTrocar = true;
}

void preencherMenuMusicas() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    strcpy(nomes[totalItens++], "PARAR MUSICA");
    DIR* d = opendir("/data/HyperNeiva/Musicas");
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && totalItens < 3000) {
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".mp3")) {
                strncpy(nomes[totalItens++], dir->d_name, 63);
            }
        }
        closedir(d);
    }
    menuAtual = MENU_MUSICAS;
}

extern int selAudioOpcao;
extern bool showOpcoes;
const char* listaOpcoesAudio[11] = { "PLAY / PAUSE", "PARAR", "PROXIMA FAIXA", "FAIXA ANTERIOR", "VOLUME +", "VOLUME -", "ADIANTAR 10s", "RETROCEDER 10s", "REPETIR", "---", "VOLTAR" };

void abrirMenuAudioOpcoes() {
    menuAtual = MENU_AUDIO_OPCOES;
    selAudioOpcao = 0;
    showOpcoes = true;
}

void tratarSelecaoAudio(int op) {
    if (totalItens <= 0 && menuAtual == MENU_MUSICAS) return;
    switch (op) {
    case 0: comandoPausar = !comandoPausar; break;
    case 1: tocarMusicaNova("PARADO"); break;
    case 10: showOpcoes = false; break;
    default: sprintf(msgStatus, "FUNCAO EM DESENVOLVIMENTO"); msgTimer = 60; break;
    }
}