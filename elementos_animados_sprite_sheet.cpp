#include "elementos_animados_sprite_sheet.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

extern void desenharTexto(uint32_t* p, const char* texto, int tamanho, int x, int y, uint32_t cor);
extern bool editMode;

bool anim_ativo = true;
int anim_posX = 445;
int anim_posY = -130;
float anim_escala = 4.5f;

int anim_colunas = 24;
int anim_linhas = 19;
int anim_velocidade = 100;

bool anim_usarColorKey = true;
uint8_t anim_keyR = 55;
uint8_t anim_keyG = 39;
uint8_t anim_keyB = 130;

bool anim_usarColorKey2 = false;
uint8_t anim_keyR2 = 0;
uint8_t anim_keyG2 = 0;
uint8_t anim_keyB2 = 0;

int anim_offsetX = 65;
int anim_offsetY = 0;
int anim_frameInicial = 0;
int anim_frameFinal = 7;

bool anim_modoTeste = false;

// ==========================================
// DEFINIÇÃO DO AUTO-CENTRO (É isto que resolve o erro)
// ==========================================
bool anim_autoCenter = false;

int anim_frameOffsetX[MAX_ANIM_FRAMES] = { 0 };
int anim_frameOffsetY[MAX_ANIM_FRAMES] = { 0 };

unsigned char* imgSpriteSheet = NULL;
int wSprite, hSprite, cSprite;
int anim_frameAtual = 0;
int anim_timer = 0;

void carregarSpriteSheetAnimada() {
    if (!imgSpriteSheet) {
        imgSpriteSheet = stbi_load("/app0/assets/images/elementos_animados_sprite_sheet.png", &wSprite, &hSprite, &cSprite, 4);
        if (!imgSpriteSheet) imgSpriteSheet = stbi_load("/app0/assets/images/elementos_animados_sprite_sheet.jpg", &wSprite, &hSprite, &cSprite, 4);
        if (!imgSpriteSheet) imgSpriteSheet = stbi_load("/app0/assets/images/zero.jpg", &wSprite, &hSprite, &cSprite, 4);
        if (!imgSpriteSheet) imgSpriteSheet = stbi_load("/app0/assets/images/zero.png", &wSprite, &hSprite, &cSprite, 4);
    }
}

void autoCentralizarFrameAtual() {
    if (!imgSpriteSheet || anim_colunas <= 0 || anim_linhas <= 0) return;

    int frameW = wSprite / anim_colunas;
    int frameH = hSprite / anim_linhas;
    int frameCol = anim_frameAtual % anim_colunas;
    int frameRow = (anim_frameAtual / anim_colunas) % anim_linhas;

    int baseX = anim_offsetX + (frameCol * frameW);
    int baseY = anim_offsetY + (frameRow * frameH);

    int minX = frameW, maxX = -1;
    int minY = frameH, maxY = -1;

    for (int y = 0; y < frameH; y++) {
        for (int x = 0; x < frameW; x++) {
            int oX = baseX + x;
            int oY = baseY + y;
            if (oX >= wSprite || oY >= hSprite || oX < 0 || oY < 0) continue;

            int idx = (oY * wSprite + oX) * 4;
            uint8_t r = imgSpriteSheet[idx + 0];
            uint8_t g = imgSpriteSheet[idx + 1];
            uint8_t b = imgSpriteSheet[idx + 2];
            uint8_t a = imgSpriteSheet[idx + 3];

            if (a == 0) continue;

            bool isBg = false;
            if (anim_usarColorKey) {
                int dist = (r - anim_keyR) * (r - anim_keyR) + (g - anim_keyG) * (g - anim_keyG) + (b - anim_keyB) * (b - anim_keyB);
                if (dist < 6000) isBg = true;
            }
            if (!isBg && anim_usarColorKey2) {
                int dist2 = (r - anim_keyR2) * (r - anim_keyR2) + (g - anim_keyG2) * (g - anim_keyG2) + (b - anim_keyB2) * (b - anim_keyB2);
                if (dist2 < 6000) isBg = true;
            }

            if (!isBg) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX >= minX && maxY >= minY) {
        int centroCorpoX = (minX + maxX) / 2;
        int centroCorpoY = (minY + maxY) / 2;
        int centroCaixaX = frameW / 2;
        int centroCaixaY = frameH / 2;
        anim_frameOffsetX[anim_frameAtual] = centroCorpoX - centroCaixaX;
        anim_frameOffsetY[anim_frameAtual] = centroCorpoY - centroCaixaY;
    }
}

void desenharElementoAnimado(uint32_t* buffer) {
    if (!anim_ativo) return;
    if (!imgSpriteSheet) carregarSpriteSheetAnimada();

    if (!imgSpriteSheet) {
        desenharTexto(buffer, "[!] ERRO DA SPRITE SHEET: IMAGEM NAO ENCONTRADA!", 35, 50, 150, 0xFFFF0000);
        return;
    }

    if (editMode) {
        for (int i = -5; i < 5; i++) {
            for (int j = -5; j < 5; j++) {
                int px = anim_posX + i; int py = anim_posY + j;
                if (px >= 0 && px < 1920 && py >= 0 && py < 1080) buffer[py * 1920 + px] = 0xFFFF0000;
            }
        }

        if (anim_colunas > 0 && anim_linhas > 0) {
            int drawW = (int)((wSprite / anim_colunas) * anim_escala);
            int drawH = (int)((hSprite / anim_linhas) * anim_escala);

            for (int x = 0; x <= drawW; x++) {
                int px = anim_posX + x; int pyTop = anim_posY; int pyBot = anim_posY + drawH;
                if (px >= 0 && px < 1920 && pyTop >= 0 && pyTop < 1080) buffer[pyTop * 1920 + px] = 0xFF00FF00;
                if (px >= 0 && px < 1920 && pyBot >= 0 && pyBot < 1080) buffer[pyBot * 1920 + px] = 0xFF00FF00;
            }
            for (int y = 0; y <= drawH; y++) {
                int py = anim_posY + y; int pxLeft = anim_posX; int pxRight = anim_posX + drawW;
                if (pxLeft >= 0 && pxLeft < 1920 && py >= 0 && py < 1080) buffer[py * 1920 + pxLeft] = 0xFF00FF00;
                if (pxRight >= 0 && pxRight < 1920 && py >= 0 && py < 1080) buffer[py * 1920 + pxRight] = 0xFF00FF00;
            }
        }
    }

    int totalFrames = anim_colunas * anim_linhas;
    if (totalFrames <= 0) totalFrames = 1;

    if (anim_frameInicial < 0) anim_frameInicial = 0;
    if (anim_frameFinal < anim_frameInicial) anim_frameFinal = anim_frameInicial;
    if (anim_frameFinal >= totalFrames) anim_frameFinal = totalFrames - 1;

    if (anim_modoTeste) {
        if (anim_frameAtual < anim_frameInicial || anim_frameAtual > anim_frameFinal) {
            anim_frameAtual = anim_frameInicial;
        }
    }
    else {
        if (anim_frameAtual < anim_frameInicial || anim_frameAtual > anim_frameFinal) {
            anim_frameAtual = anim_frameInicial;
        }

        anim_timer += anim_velocidade;
        while (anim_timer >= 100) {
            anim_frameAtual++;
            anim_timer -= 100;
            if (anim_frameAtual > anim_frameFinal) {
                anim_frameAtual = anim_frameInicial;
            }
        }
    }

    desenharAnimacaoGrid(buffer, imgSpriteSheet, wSprite, hSprite,
        anim_colunas, anim_linhas, anim_frameAtual,
        anim_posX, anim_posY, anim_escala,
        anim_keyR, anim_keyG, anim_keyB, anim_usarColorKey,
        anim_keyR2, anim_keyG2, anim_keyB2, anim_usarColorKey2,
        anim_offsetX, anim_offsetY);
}

void desenharSpriteFrameEscala(uint32_t* pixels, unsigned char* img, int imgW, int imgH,
    int frameX, int frameY, int frameW, int frameH,
    int posX, int posY, float escala,
    uint8_t keyR, uint8_t keyG, uint8_t keyB, bool usarColorKey,
    uint8_t keyR2, uint8_t keyG2, uint8_t keyB2, bool usarColorKey2) {

    if (!img || !pixels || frameW <= 0 || frameH <= 0 || escala <= 0.0f) return;

    int autoShiftX = 0;

    if (anim_autoCenter && usarColorKey) {
        int minX = frameW; int maxX = -1;
        for (int y = 0; y < frameH; y++) {
            for (int x = 0; x < frameW; x++) {
                int oX = frameX + x; int oY = frameY + y;
                if (oX >= imgW || oY >= imgH || oX < 0 || oY < 0) continue;
                int idx = (oY * imgW + oX) * 4;
                uint8_t r = img[idx + 0]; uint8_t g = img[idx + 1]; uint8_t b = img[idx + 2]; uint8_t a = img[idx + 3];
                if (a == 0) continue;
                bool isBackground = false;
                if (usarColorKey) {
                    int dist1 = (r - keyR) * (r - keyR) + (g - keyG) * (g - keyG) + (b - keyB) * (b - keyB);
                    if (dist1 < 6000) isBackground = true;
                }
                if (!isBackground && usarColorKey2) {
                    int dist2 = (r - keyR2) * (r - keyR2) + (g - keyG2) * (g - keyG2) + (b - keyB2) * (b - keyB2);
                    if (dist2 < 6000) isBackground = true;
                }
                if (!isBackground) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                }
            }
        }
        if (maxX >= minX) {
            int centroDoCorpo = (minX + maxX) / 2;
            int centroDoQuadrado = frameW / 2;
            autoShiftX = (int)((centroDoQuadrado - centroDoCorpo) * escala);
        }
    }

    int finalW = (int)(frameW * escala);
    int finalH = (int)(frameH * escala);

    for (int y = 0; y < finalH; y++) {
        int pY = posY + y;
        if (pY < 0 || pY >= 1080) continue;

        for (int x = 0; x < finalW; x++) {
            int pX = posX + x + autoShiftX;
            if (pX < 0 || pX >= 1920) continue;

            int srcX = (int)(x / escala);
            int srcY = (int)(y / escala);

            int oX = frameX + srcX;
            int oY = frameY + srcY;

            if (oX >= imgW || oY >= imgH || oX < 0 || oY < 0) continue;

            int idx = (oY * imgW + oX) * 4;
            uint8_t r = img[idx + 0]; uint8_t g = img[idx + 1];
            uint8_t b = img[idx + 2]; uint8_t a = img[idx + 3];

            if (a == 0) continue;

            if (usarColorKey) {
                int dist = (r - keyR) * (r - keyR) + (g - keyG) * (g - keyG) + (b - keyB) * (b - keyB);
                if (dist < 6000) continue;
            }
            if (usarColorKey2) {
                int dist2 = (r - keyR2) * (r - keyR2) + (g - keyG2) * (g - keyG2) + (b - keyB2) * (b - keyB2);
                if (dist2 < 6000) continue;
            }

            pixels[pY * 1920 + pX] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void desenharAnimacaoGrid(uint32_t* pixels, unsigned char* img, int imgW, int imgH,
    int colunas, int linhas, int frameAtual,
    int posX, int posY, float escala,
    uint8_t keyR, uint8_t keyG, uint8_t keyB, bool usarColorKey,
    uint8_t keyR2, uint8_t keyG2, uint8_t keyB2, bool usarColorKey2,
    int offsetX, int offsetY) {

    if (colunas <= 0 || linhas <= 0) return;

    int frameW = imgW / colunas;
    int frameH = imgH / linhas;

    int frameCol = frameAtual % colunas;
    int frameRow = (frameAtual / colunas) % linhas;

    int frameX = offsetX + (frameCol * frameW) + anim_frameOffsetX[frameAtual];
    int frameY = offsetY + (frameRow * frameH) + anim_frameOffsetY[frameAtual];

    desenharSpriteFrameEscala(pixels, img, imgW, imgH, frameX, frameY, frameW, frameH,
        posX, posY, escala,
        keyR, keyG, keyB, usarColorKey,
        keyR2, keyG2, keyB2, usarColorKey2);
}