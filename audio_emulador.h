#ifndef AUDIO_EMU_H
#define AUDIO_EMU_H

#include <stdint.h>
#include <stddef.h>

void enviarAmostraAudio(int16_t L, int16_t R);
void misturarAudioEmulador(int16_t* pSamples, size_t numFrames);

#endif
