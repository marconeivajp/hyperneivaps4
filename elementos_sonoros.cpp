#include "elementos_sonoros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// IMPORTANTE: Incluimos a biblioteca SEM "IMPLEMENTATION" para não chocar com o audio.cpp!
#include "dr_wav.h"

// PUXANDO AS VARIÁVEIS DO MENU EDITAR
extern int sfxLigado;
extern int sfxVolume;

int16_t* sfxUpData = NULL; size_t sfxUpLen = 0;
int16_t* sfxDownData = NULL; size_t sfxDownLen = 0;
int16_t* sfxCrossData = NULL; size_t sfxCrossLen = 0;
int16_t* sfxCircleData = NULL; size_t sfxCircleLen = 0;

volatile int16_t* currentSfx = NULL;
volatile size_t currentSfxFrames = 0;
volatile size_t currentSfxPos = 0;

int16_t* carregarWavMemoria(const char* pathPrincipal, const char* pathAlternativo, size_t* totalFrames) {
    unsigned int channels, sampleRate;
    drwav_uint64 totalPCMFrameCount;

    int16_t* pSampleData = drwav_open_file_and_read_pcm_frames_s16(pathPrincipal, &channels, &sampleRate, &totalPCMFrameCount, NULL);

    if (pSampleData == NULL) {
        pSampleData = drwav_open_file_and_read_pcm_frames_s16(pathAlternativo, &channels, &sampleRate, &totalPCMFrameCount, NULL);
        if (pSampleData == NULL) return NULL;
    }

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

void inicializarElementosSonoros() {
    sfxUpData = carregarWavMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_direcinal_cima.wav", "/app0/assets/audio/0_Defalt_direcinal_cima.wav", &sfxUpLen);
    sfxDownData = carregarWavMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_direcional_baixo.wav", "/app0/assets/audio/0_Defalt_direcional_baixo.wav", &sfxDownLen);
    sfxCrossData = carregarWavMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_x.wav", "/app0/assets/audio/0_Defalt_x.wav", &sfxCrossLen);
    sfxCircleData = carregarWavMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_bolinha.wav", "/app0/assets/audio/0_Defalt_bolinha.wav", &sfxCircleLen);
}

void tocarSom(SfxType tipo) {
    // Se o som estiver desligado no menu Editar, nem aciona!
    if (!sfxLigado) return;

    switch (tipo) {
    case SFX_UP:
        if (sfxUpData) { currentSfxFrames = sfxUpLen; currentSfxPos = 0; currentSfx = sfxUpData; }
        break;
    case SFX_DOWN:
        if (sfxDownData) { currentSfxFrames = sfxDownLen; currentSfxPos = 0; currentSfx = sfxDownData; }
        break;
    case SFX_CROSS:
        if (sfxCrossData) { currentSfxFrames = sfxCrossLen; currentSfxPos = 0; currentSfx = sfxCrossData; }
        break;
    case SFX_CIRCLE:
        if (sfxCircleData) { currentSfxFrames = sfxCircleLen; currentSfxPos = 0; currentSfx = sfxCircleData; }
        break;
    }
}

void misturarEfeitosSonoros(int16_t* bufferAudio, size_t frames) {
    // Segunda barreira: se desligaram durante o som tocar, corta ele.
    if (!sfxLigado) return;

    if (currentSfx != NULL && currentSfxPos < currentSfxFrames) {
        size_t framesToMix = frames;
        if (currentSfxPos + framesToMix > currentSfxFrames) {
            framesToMix = currentSfxFrames - currentSfxPos;
        }

        // Calcula o volume como uma porcentagem de 0.00 até 1.00
        float fatorVolume = sfxVolume / 100.0f;

        for (size_t i = 0; i < framesToMix * 2; i++) {
            // Pega o sample do efeito sonoro e aplica o multiplicador de volume
            int32_t sampleSfx = (int32_t)(currentSfx[currentSfxPos * 2 + i] * fatorVolume);

            // Soma a Música de Fundo com o Efeito Sonoro reduzido/aumentado
            int32_t sample = bufferAudio[i] + sampleSfx;

            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;

            bufferAudio[i] = (int16_t)sample;
        }
        currentSfxPos += framesToMix;
        if (currentSfxPos >= currentSfxFrames) {
            currentSfx = NULL;
        }
    }
}