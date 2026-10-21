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

// Estrutura para Combos de Sons Customizados (V62/V63)
struct SfxCombo {
    uint32_t buttons;
    char path[256];
    bool active;
};

// Nova função mágica que mistura o áudio na música!
void misturarEfeitosSonoros(int16_t* bufferAudio, size_t frames);
void verificarSfxCombos(uint32_t buttons);
void carregarSfxCombos();
void salvarSfxCombos();

#endif