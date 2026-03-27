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
extern int anim_tolerancia;

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

extern bool anim_autoCenter;

extern int anim_frameOffsetX[MAX_ANIM_FRAMES];
extern int anim_frameOffsetY[MAX_ANIM_FRAMES];

extern unsigned char* imgSpriteSheet;
extern int wSprite, hSprite, cSprite;

// --- VARIÁVEIS DO CURSOR E PIVÔ ---
extern int anim_cursorX;
extern int anim_cursorY;
extern bool anim_mostrarCruz;
extern bool anim_editandoPivoVisual;
extern int anim_pivoCursorX;
extern int anim_pivoCursorY;

void carregarSpriteSheetAnimada();
void desenharElementoAnimado(uint32_t* buffer);
void autoCentralizarFrameAtual();

// --- FUNÇÕES DE CAPTURA VISUAL ---
void pegarCorNoCursor(bool setColor2);

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