#include "audio_emulador.h"

#define EMU_AUDIO_BUFFER_SIZE 4096
static int16_t emuAudioBuffer[EMU_AUDIO_BUFFER_SIZE * 2];
static volatile int emuAudioReadIdx = 0;
static volatile int emuAudioWriteIdx = 0;

void enviarAmostraAudio(int16_t L, int16_t R) {
    int nextWrite = (emuAudioWriteIdx + 1) % EMU_AUDIO_BUFFER_SIZE;
    if (nextWrite != emuAudioReadIdx) {
        emuAudioBuffer[emuAudioWriteIdx * 2] = L;
        emuAudioBuffer[emuAudioWriteIdx * 2 + 1] = R;
        emuAudioWriteIdx = nextWrite;
    }
}

void misturarAudioEmulador(int16_t* pSamples, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        if (emuAudioReadIdx != emuAudioWriteIdx) {
            int16_t eL = emuAudioBuffer[emuAudioReadIdx * 2];
            int16_t eR = emuAudioBuffer[emuAudioReadIdx * 2 + 1];

            int32_t mixL = pSamples[i * 2] + eL;
            int32_t mixR = pSamples[i * 2 + 1] + eR;

            if (mixL > 32767) mixL = 32767; else if (mixL < -32768) mixL = -32768;
            if (mixR > 32767) mixR = 32767; else if (mixR < -32768) mixR = -32768;

            pSamples[i * 2] = (int16_t)mixL;
            pSamples[i * 2 + 1] = (int16_t)mixR;

            emuAudioReadIdx = (emuAudioReadIdx + 1) % EMU_AUDIO_BUFFER_SIZE;
        }
    }
}
