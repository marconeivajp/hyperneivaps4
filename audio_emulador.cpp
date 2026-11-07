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

static double emuAudioPasso = 1.0, emuAudioFracao = 0.0;

// Limpa o que sobrou do jogo anterior e guarda a taxa do núcleo; a saída é sempre 48 kHz.
void definirTaxaAudioEmulador(double taxa) {
    emuAudioReadIdx = 0;
    emuAudioWriteIdx = 0;
    emuAudioFracao = 0.0;
    emuAudioPasso = taxa > 1000.0 ? taxa / 48000.0 : 1.0;
}

static inline int emuAudioDisponivel() {
    int n = emuAudioWriteIdx - emuAudioReadIdx;
    return n < 0 ? n + EMU_AUDIO_BUFFER_SIZE : n;
}

void misturarAudioEmulador(int16_t* pSamples, size_t numFrames) {
    // Atraso acumulado vira som atrasado da imagem: descarta o excesso.
    int sobra = emuAudioDisponivel() - EMU_AUDIO_BUFFER_SIZE * 3 / 4;
    if (sobra > 0) emuAudioReadIdx = (emuAudioReadIdx + sobra) % EMU_AUDIO_BUFFER_SIZE;
    for (size_t i = 0; i < numFrames; i++) {
        if (emuAudioDisponivel() < 2) break;
        int atual = emuAudioReadIdx, proxima = (atual + 1) % EMU_AUDIO_BUFFER_SIZE;
        double f = emuAudioFracao;
        int32_t eL = (int32_t)(emuAudioBuffer[atual * 2] + (emuAudioBuffer[proxima * 2] - emuAudioBuffer[atual * 2]) * f);
        int32_t eR = (int32_t)(emuAudioBuffer[atual * 2 + 1] + (emuAudioBuffer[proxima * 2 + 1] - emuAudioBuffer[atual * 2 + 1]) * f);

        int32_t mixL = pSamples[i * 2] + eL;
        int32_t mixR = pSamples[i * 2 + 1] + eR;
        if (mixL > 32767) mixL = 32767; else if (mixL < -32768) mixL = -32768;
        if (mixR > 32767) mixR = 32767; else if (mixR < -32768) mixR = -32768;
        pSamples[i * 2] = (int16_t)mixL;
        pSamples[i * 2 + 1] = (int16_t)mixR;

        emuAudioFracao += emuAudioPasso;
        while (emuAudioFracao >= 1.0 && emuAudioDisponivel() > 0) {
            emuAudioFracao -= 1.0;
            emuAudioReadIdx = (emuAudioReadIdx + 1) % EMU_AUDIO_BUFFER_SIZE;
        }
    }
}
