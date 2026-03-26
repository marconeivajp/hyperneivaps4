#ifndef ELEMENTOS_ANIMADOS_SPRITE_SHEET_H
#define ELEMENTOS_ANIMADOS_SPRITE_SHEET_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ANIM_FRAMES 100

extern bool anim_ativo;
extern int anim_posX;
extern int anim_posY;
extern float anim_escala;

extern int anim_colunas;
extern int anim_linhas;
extern int anim_velocidade;

extern bool anim_usarColorKey;
extern uint8_t anim_keyR;
extern uint8_t anim_keyG;
extern uint8_t anim_keyB;

extern bool anim_usarColorKey2;
extern uint8_t anim_keyR2;
extern uint8_t anim_keyG2;
extern uint8_t anim_keyB2;

extern int anim_offsetX;
extern int anim_offsetY;
extern int anim_frameInicial;
extern int anim_frameFinal;

extern bool anim_modoTeste;
extern int anim_frameAtual;

// ==========================================
// DECLARAÇÃO DO AUTO-CENTRO (Obrigatório para o Linker)
// ==========================================
extern bool anim_autoCenter;

extern int anim_frameOffsetX[MAX_ANIM_FRAMES];
extern int anim_frameOffsetY[MAX_ANIM_FRAMES];

void carregarSpriteSheetAnimada();
void desenharElementoAnimado(uint32_t* buffer);
void autoCentralizarFrameAtual();

void desenharSpriteFrameEscala(uint32_t* pixels, unsigned char* img, int imgW, int imgH,
    int frameX, int frameY, int frameW, int frameH,
    int posX, int posY, float escala,
    uint8_t keyR, uint8_t keyG, uint8_t keyB, bool usarColorKey,
    uint8_t keyR2, uint8_t keyG2, uint8_t keyB2, bool usarColorKey2);

void desenharAnimacaoGrid(uint32_t* pixels, unsigned char* img, int imgW, int imgH,
    int colunas, int linhas, int frameAtual,
    int posX, int posY, float escala,
    uint8_t keyR, uint8_t keyG, uint8_t keyB, bool usarColorKey,
    uint8_t keyR2, uint8_t keyG2, uint8_t keyB2, bool usarColorKey2,
    int offsetX, int offsetY);

#endif