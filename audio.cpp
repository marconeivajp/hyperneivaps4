#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h> 

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list va_list
#endif
#endif

#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#include "audio.h"
#include "audio_radio.h"
#include "audio_musica.h"
#include "audio_emulador.h"
#include "explorar.h" 
#include "elementos_sonoros.h" 
#include "instrumentos.h"
#include "video.h"

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

volatile int modoReproducao = 0;

volatile int comandoBuscarSegundos = 0;
int volumeGeral = 100;
char musicaAtual[256] = "PARADO";
char ultimaMusicaTocada[256] = "";

char caminhosMusicasMenu[3000][256];
char caminhoNavegacaoMusicas[512] = "/data/HyperNeiva/Musicas";

volatile float audioTempoAtual = 0.0f;

// Emulator audio logic moved to audio_emu.cpp

static AudioType currentAudioType = AUDIO_NONE;

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
        int modoSalvar = (int)modoReproducao;
        fwrite(&modoSalvar, 1, sizeof(int), f);
        fwrite(ultimaMusicaTocada, 1, sizeof(ultimaMusicaTocada), f);

        int pausado = comandoPausar ? 1 : 0;
        fwrite(&pausado, 1, sizeof(int), f);

        fclose(f);
    }
}

void carregarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "rb");
    if (f) {
        if (fread(ultimaMusicaTocada, 1, sizeof(ultimaMusicaTocada), f) <= 0) {
            strcpy(ultimaMusicaTocada, "");
            if (strcmp(musicaAtual, "PARADO") != 0) strcpy(ultimaMusicaTocada, musicaAtual);
        }

        int pausado = 0;
        if (fread(&pausado, 1, sizeof(int), f) > 0) {
            comandoPausar = (pausado == 1);
        }
        else {
            comandoPausar = false;
        }

        fclose(f);
    }
    else {
        volumeGeral = 100;
        modoReproducao = 0;
        strcpy(ultimaMusicaTocada, "");
        comandoPausar = false;
    }
}

// Playlist and file selection logic moved to audio_playlist.cpp

// Funções de rádio movidas para audio_radio.cpp

static void* audioThreadFunc(void* argp) {
    if (!sistemaAudioIniciado) { sceAudioOutInit(); sistemaAudioIniciado = true; }
    int32_t userId;
    if (sceUserServiceGetInitialUser(&userId) < 0) userId = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
    audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (audioPort < 0) { audioRodando = false; return NULL; }

    drwav wav; drmp3 mp3;
    int16_t pSampleData[256 * 2];
    char caminhoAudio[256];
    uint64_t currentFrame = 0;

    if (!comandoPausar && prepararArquivoAudio(caminhoAudio)) {
        if (strncmp(caminhoAudio, "http", 4) == 0) {
            if (iniciarRadio(caminhoAudio)) {
                currentAudioType = AUDIO_STREAM;
            }
            else {
                extern char msgStatus[128]; extern int msgTimer; extern unsigned int msgStatusColor;
                snprintf(msgStatus, sizeof(msgStatus), "Erro ao conectar no servidor da Radio.");
                msgTimer = 400; msgStatusColor = 0xFF0000FF;
            }
        }
        else if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
            if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
        }
        else {
            if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
        }
    }

    while (audioRodando) {
        if (audioPort < 0) {
            audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        }

        // TELEMETRIA: Observa a Net a baixar e o FFmpeg a "comer"
        if (currentAudioType == AUDIO_STREAM && isRadioRodando()) {
            extern char msgStatus[128];
            extern int msgTimer;
            extern unsigned int msgStatusColor;

            obterTelemetriaRadio(msgStatus, sizeof(msgStatus));
            msgTimer = 100;
            msgStatusColor = 0xFF00FF00;
        }

        if (comandoBuscarSegundos != 0) {
            if (currentAudioType != AUDIO_NONE) {
                int sampleRate = (currentAudioType == AUDIO_WAV) ? wav.sampleRate : mp3.sampleRate;
                int64_t frameOffset = (int64_t)comandoBuscarSegundos * sampleRate;
                int64_t targetFrame = (int64_t)currentFrame + frameOffset;

                if (targetFrame < 0) targetFrame = 0;

                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, (uint64_t)targetFrame);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, (uint64_t)targetFrame);
                currentFrame = (uint64_t)targetFrame;

                if (currentAudioType == AUDIO_WAV) audioTempoAtual = (float)currentFrame / wav.sampleRate;
                else if (currentAudioType == AUDIO_MP3) audioTempoAtual = (float)currentFrame / mp3.sampleRate;
            }
            comandoBuscarSegundos = 0;
        }

        if (comandoTrocar) {
            comandoTrocar = false;
            if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
            else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
            else if (currentAudioType == AUDIO_STREAM) pararRadioStreaming();
            currentAudioType = AUDIO_NONE;

            if (prepararArquivoAudio(caminhoAudio)) {
                if (strncmp(caminhoAudio, "http", 4) == 0) {
                    if (iniciarRadio(caminhoAudio)) {
                        currentAudioType = AUDIO_STREAM;
                    }
                    else {
                        strcpy(musicaAtual, "PARADO");
                        comandoPausar = true;
                        pararRadioStreaming();
                    }
                }
                else if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
                    if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
                }
                else {
                    if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
                }
            }
            currentFrame = 0;
            audioTempoAtual = 0.0f;
        }

        if (comandoPausar || currentAudioType == AUDIO_NONE) {
            for (int i = 0; i < 256 * 2; i++) pSampleData[i] = 0;
            misturarEfeitosSonoros(pSampleData, 256);
            misturarAudioPiano(pSampleData, 256);
            misturarAudioVideo(pSampleData, 256);
            misturarAudioEmulador(pSampleData, 256);
            if (audioPort >= 0) sceAudioOutOutput(audioPort, pSampleData);
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
        else if (currentAudioType == AUDIO_STREAM) {
            framesLidos = lerFrameRadio(pSampleData, 256);
            if (framesLidos == 0 && !isRadioRodando()) {
                // Erro fatal real (EOF) ou desconexão
                strcpy(musicaAtual, "PARADO");
                comandoPausar = true;
            }
            currentChannels = 2;
        }

        if (framesLidos > 0 && currentChannels == 1) {
            for (int i = framesLidos - 1; i >= 0; i--) {
                pSampleData[i * 2] = pSampleData[i];
                pSampleData[i * 2 + 1] = pSampleData[i];
            }
        }

        currentFrame += framesLidos;

        if (currentAudioType == AUDIO_WAV) {
            audioTempoAtual = (float)currentFrame / wav.sampleRate;
        }
        else if (currentAudioType == AUDIO_MP3) {
            audioTempoAtual = (float)currentFrame / mp3.sampleRate;
        }

        if (framesLidos == 0) {
            if (modoReproducao == 1) {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
                audioTempoAtual = 0.0f;
            }
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
                        audioTempoAtual = 0.0f;
                        continue;
                    }
                }
            }
            else {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
                audioTempoAtual = 0.0f;
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

        misturarEfeitosSonoros(pSampleData, 256);
        misturarAudioPiano(pSampleData, 256);
        misturarAudioVideo(pSampleData, 256);
        misturarAudioEmulador(pSampleData, 256);
        if (audioPort >= 0) sceAudioOutOutput(audioPort, pSampleData);
    }

    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
    else if (currentAudioType == AUDIO_STREAM) pararRadioStreaming();
    sceAudioOutClose(audioPort);
    return NULL;
}

void inicializarAudio() {
    if (audioRodando) return;
    srand(time(NULL));
    // Note: avformat_network_init calls moved to audio_radio or handled globally

    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
    carregarConfiguracaoAudio();
    audioRodando = true;

    if (strcmp(musicaAtual, "PARADO") == 0) comandoPausar = true;

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
    if (strncmp(path, "http", 4) == 0) {
        extern char msgStatus[128];
        extern int msgTimer;
        extern unsigned int msgStatusColor;
        strcpy(msgStatus, "Conectando ao Servidor...");
        msgTimer = 200;
        msgStatusColor = 0xFF00FF00;
    }
    strcpy(musicaAtual, path);
    strcpy(ultimaMusicaTocada, path);
    salvarConfiguracaoAudio();
    comandoPausar = false;
    comandoTrocar = true;
}

void tocarProximaMusica() {
    bool estavaParado = false;
    if (strcmp(musicaAtual, "PARADO") == 0) {
        if (strlen(ultimaMusicaTocada) == 0) return;
        strcpy(musicaAtual, ultimaMusicaTocada);
        estavaParado = true;
    }

    char proxima[256];
    if (obterProximaMusica(proxima)) {
        tocarMusicaNova(proxima);
    }
    else if (estavaParado) {
        strcpy(musicaAtual, "PARADO");
    }
}

void tocarMusicaAnterior() {
    bool estavaParado = false;
    if (strcmp(musicaAtual, "PARADO") == 0) {
        if (strlen(ultimaMusicaTocada) == 0) return;
        strcpy(musicaAtual, ultimaMusicaTocada);
        estavaParado = true;
    }

    char anterior[256];
    if (obterMusicaAnterior(anterior)) {
        tocarMusicaNova(anterior);
    }
    else if (estavaParado) {
        strcpy(musicaAtual, "PARADO");
    }
}