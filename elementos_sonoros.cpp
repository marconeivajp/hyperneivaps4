#include "elementos_sonoros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

#include "dr_wav.h"

int16_t* sfxUpData = NULL; size_t sfxUpLen = 0;
int16_t* sfxDownData = NULL; size_t sfxDownLen = 0;
int16_t* sfxCrossData = NULL; size_t sfxCrossLen = 0;
int16_t* sfxCircleData = NULL; size_t sfxCircleLen = 0;

volatile int16_t* currentSfx = NULL;
volatile size_t currentSfxFrames = 0;
volatile size_t currentSfxPos = 0;

static int sfxPort = -1;
static pthread_t sfxThreadId;
static bool sfxRodando = false;

int16_t* carregarWavMemoria(const char* path, size_t* totalFrames) {
    unsigned int channels, sampleRate;
    drwav_uint64 totalPCMFrameCount;
    int16_t* pSampleData = drwav_open_file_and_read_pcm_frames_s16(path, &channels, &sampleRate, &totalPCMFrameCount, NULL);
    if (pSampleData == NULL) return NULL;

    // Converte para Stereo se o arquivo for Mono (exigência de estabilidade do PS4)
    if (channels == 1) {
        int16_t* stereoData = (int16_t*)malloc(totalPCMFrameCount * 2 * sizeof(int16_t));
        for (size_t i = 0; i < totalPCMFrameCount; i++) {
            stereoData[i * 2] = pSampleData[i];
            stereoData[i * 2 + 1] = pSampleData[i];
        }
        free(pSampleData);
        pSampleData = stereoData;
    }
    *totalFrames = totalPCMFrameCount;
    return pSampleData;
}

void* sfxThreadFunc(void* arg) {
    int32_t userId;
    if (sceUserServiceGetInitialUser(&userId) < 0) userId = ORBIS_USER_SERVICE_USER_ID_SYSTEM;

    // Porta 1 para SFX (A Música BGM está na Porta 0)
    sfxPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 1, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (sfxPort < 0) { sfxRodando = false; return NULL; }

    int16_t silentBuffer[256 * 2];
    memset(silentBuffer, 0, sizeof(silentBuffer));

    while (sfxRodando) {
        if (currentSfx != NULL && currentSfxPos < currentSfxFrames) {
            int16_t outBuf[256 * 2];
            size_t framesToRead = 256;
            if (currentSfxPos + framesToRead > currentSfxFrames) {
                framesToRead = currentSfxFrames - currentSfxPos;
            }

            memset(outBuf, 0, sizeof(outBuf));
            for (size_t i = 0; i < framesToRead * 2; i++) {
                outBuf[i] = currentSfx[currentSfxPos * 2 + i];
            }

            currentSfxPos += framesToRead;
            sceAudioOutOutput(sfxPort, outBuf);

            // Quando o som acaba, reseta para silêncio e evita ruidos!
            if (currentSfxPos >= currentSfxFrames) {
                currentSfx = NULL;
            }
        }
        else {
            sceAudioOutOutput(sfxPort, silentBuffer);
        }
    }

    sceAudioOutClose(sfxPort);
    return NULL;
}

void inicializarElementosSonoros() {
    if (sfxRodando) return;

    // Os arquivos já foram movidos pelo criar_pastas.cpp, agora é só carregar na memória (RAM)
    sfxUpData = carregarWavMemoria("/data/HyperNeiva/configuracao/audio/0_Defalt_direcinal_cima.wav", &sfxUpLen);
    sfxDownData = carregarWavMemoria("/data/HyperNeiva/configuracao/audio/0_Defalt_direcional_baixo.wav", &sfxDownLen);
    sfxCrossData = carregarWavMemoria("/data/HyperNeiva/configuracao/audio/0_Defalt_x.wav", &sfxCrossLen);
    sfxCircleData = carregarWavMemoria("/data/HyperNeiva/configuracao/audio/0_Defalt_bolinha.wav", &sfxCircleLen);

    sfxRodando = true;
    pthread_create(&sfxThreadId, NULL, sfxThreadFunc, NULL);
}

void tocarSom(SfxType tipo) {
    if (!sfxRodando) return;

    switch (tipo) {
    case SFX_UP:
        if (sfxUpData) { currentSfx = sfxUpData; currentSfxFrames = sfxUpLen; currentSfxPos = 0; }
        break;
    case SFX_DOWN:
        if (sfxDownData) { currentSfx = sfxDownData; currentSfxFrames = sfxDownLen; currentSfxPos = 0; }
        break;
    case SFX_CROSS:
        if (sfxCrossData) { currentSfx = sfxCrossData; currentSfxFrames = sfxCrossLen; currentSfxPos = 0; }
        break;
    case SFX_CIRCLE:
        if (sfxCircleData) { currentSfx = sfxCircleData; currentSfxFrames = sfxCircleLen; currentSfxPos = 0; }
        break;
    }
}