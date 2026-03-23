#ifndef ELEMENTOS_SONOROS_H
#define ELEMENTOS_SONOROS_H

#include <stdint.h>
#include <stddef.h>

enum SfxType {
    SFX_UP,
    SFX_DOWN,
    SFX_CROSS,
    SFX_CIRCLE
};

void inicializarElementosSonoros();
void tocarSom(SfxType tipo);

// Nova função mágica que mistura o áudio na música!
void misturarEfeitosSonoros(int16_t* bufferAudio, size_t frames);

#endif