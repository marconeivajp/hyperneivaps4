#include "elementos_sonoros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_wav.h"
#include "dr_mp3.h"

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

// Universal Audio Loader: Handles WAV/MP3 and Mono-to-Stereo conversion
int16_t* carregarAudioMemoria(const char* path, size_t* totalFrames) {
    if (!path || strlen(path) == 0) return NULL;
    
    unsigned int channels, sampleRate;
    drwav_uint64 wavFrames;
    drmp3_uint64 mp3Frames;
    int16_t* pSampleData = NULL;

    const char* ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".mp3") == 0)) {
        drmp3_config config;
        pSampleData = drmp3_open_file_and_read_pcm_frames_s16(path, &config, &mp3Frames, NULL);
        channels = config.channels;
        sampleRate = config.sampleRate;
        *totalFrames = (size_t)mp3Frames;
    } else {
        pSampleData = drwav_open_file_and_read_pcm_frames_s16(path, &channels, &sampleRate, &wavFrames, NULL);
        *totalFrames = (size_t)wavFrames;
    }

    if (pSampleData == NULL) return NULL;

    // Converte para Stereo 48kHz se necessário (simplificado: herda do sistema de áudio principal)
    if (channels == 1) {
        int16_t* stereoData = (int16_t*)malloc((*totalFrames) * 2 * sizeof(int16_t));
        for (size_t i = 0; i < (*totalFrames); i++) {
            stereoData[i * 2] = pSampleData[i];
            stereoData[i * 2 + 1] = pSampleData[i];
        }
        free(pSampleData);
        pSampleData = stereoData;
    }
    return pSampleData;
}

// Suporte para Combos de Sons Customizados (V62/V63)
SfxCombo combosSFX[20];
int16_t* sfxComboData[20] = {0};
size_t sfxComboLen[20] = {0};

void limparCacheSfxCombos() {
    for (int i = 0; i < 20; i++) {
        if (sfxComboData[i]) {
            free(sfxComboData[i]);
            sfxComboData[i] = NULL;
            sfxComboLen[i] = 0;
        }
    }
}

void carregarSfxCombos() {
    limparCacheSfxCombos();
    memset(combosSFX, 0, sizeof(combosSFX));
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings/sfx_combos.bin", "rb");
    if (f) {
        fread(combosSFX, sizeof(SfxCombo), 20, f);
        fclose(f);
        
        // Pré-carrega todos os ativos na RAM (Cache)
        for (int i = 0; i < 20; i++) {
            if (combosSFX[i].active && strlen(combosSFX[i].path) > 0) {
                sfxComboData[i] = carregarAudioMemoria(combosSFX[i].path, &sfxComboLen[i]);
            }
        }
    }
}

void salvarSfxCombos() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings/sfx_combos.bin", "wb");
    if (f) {
        fwrite(combosSFX, sizeof(SfxCombo), 20, f);
        fclose(f);
    }
    // Após salvar, recarregamos para atualizar o cache na memória
    carregarSfxCombos();
}

void inicializarElementosSonoros() {
    sfxUpData = carregarAudioMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_direcinal_cima.wav", &sfxUpLen);
    if (!sfxUpData) sfxUpData = carregarAudioMemoria("/app0/assets/audio/0_Defalt_direcinal_cima.wav", &sfxUpLen);
    
    sfxDownData = carregarAudioMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_direcional_baixo.wav", &sfxDownLen);
    if (!sfxDownData) sfxDownData = carregarAudioMemoria("/app0/assets/audio/0_Defalt_direcional_baixo.wav", &sfxDownLen);
    
    sfxCrossData = carregarAudioMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_x.wav", &sfxCrossLen);
    if (!sfxCrossData) sfxCrossData = carregarAudioMemoria("/app0/assets/audio/0_Defalt_x.wav", &sfxCrossLen);
    
    sfxCircleData = carregarAudioMemoria("/data/HyperNeiva/configuracao/audios/0_Defalt_bolinha.wav", &sfxCircleLen);
    if (!sfxCircleData) sfxCircleData = carregarAudioMemoria("/app0/assets/audio/0_Defalt_bolinha.wav", &sfxCircleLen);
    
    carregarSfxCombos();
}

void tocarSom(SfxType tipo) {
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
    if (!sfxLigado) return;

    if (currentSfx != NULL && currentSfxPos < currentSfxFrames) {
        size_t framesToMix = frames;
        if (currentSfxPos + framesToMix > currentSfxFrames) {
            framesToMix = currentSfxFrames - currentSfxPos;
        }

        float fatorVolume = sfxVolume / 100.0f;

        for (size_t i = 0; i < framesToMix * 2; i++) {
            int32_t sampleSfx = (int32_t)(currentSfx[currentSfxPos * 2 + i] * fatorVolume);
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

void verificarSfxCombos(uint32_t buttons) {
    if (!sfxLigado || buttons == 0) return;
    
    int melhorIndice = -1;
    int maxBotoes = 0;

    for (int i = 0; i < 20; i++) {
        if (combosSFX[i].active && (buttons & combosSFX[i].buttons) == combosSFX[i].buttons) {
            int count = 0; uint32_t b = combosSFX[i].buttons;
            while (b) { count += (b & 1); b >>= 1; }
            if (count > maxBotoes) {
                maxBotoes = count;
                melhorIndice = i;
            }
        }
    }

    if (melhorIndice != -1 && sfxComboData[melhorIndice]) {
        currentSfxFrames = sfxComboLen[melhorIndice];
        currentSfxPos = 0;
        currentSfx = sfxComboData[melhorIndice];
    }
}

#include <orbis/Pad.h>
const char* getSfxButtonName(uint32_t buttons) {
    if (buttons == 0) return "NENHUM";
    static char buf[256]; buf[0] = '\0'; bool first = true;
    auto add = [&](const char* name) { if (!first) strcat(buf, " + "); strcat(buf, name); first = false; };
    if (buttons & ORBIS_PAD_BUTTON_L1) add("L1");
    if (buttons & ORBIS_PAD_BUTTON_R1) add("R1");
    if (buttons & ORBIS_PAD_BUTTON_L2) add("L2");
    if (buttons & ORBIS_PAD_BUTTON_R2) add("R2");
    if (buttons & ORBIS_PAD_BUTTON_TRIANGLE) add("TRIANGULO");
    if (buttons & ORBIS_PAD_BUTTON_CIRCLE) add("BOLINHA");
    if (buttons & ORBIS_PAD_BUTTON_CROSS) add("X");
    if (buttons & ORBIS_PAD_BUTTON_SQUARE) add("QUADRADO");
    if (buttons & ORBIS_PAD_BUTTON_UP) add("CIMA");
    if (buttons & ORBIS_PAD_BUTTON_DOWN) add("BAIXO");
    if (buttons & ORBIS_PAD_BUTTON_LEFT) add("ESQUERDA");
    if (buttons & ORBIS_PAD_BUTTON_RIGHT) add("DIREITA");
    if (buttons & ORBIS_PAD_BUTTON_OPTIONS) add("OPTIONS");
    if (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) add("TOUCHPAD");
    return buf;
}