#ifndef ELEMENTOS_SONOROS_H
#define ELEMENTOS_SONOROS_H

enum SfxType {
    SFX_UP,
    SFX_DOWN,
    SFX_CROSS,
    SFX_CIRCLE
};

void inicializarElementosSonoros();
void tocarSom(SfxType tipo);

#endif