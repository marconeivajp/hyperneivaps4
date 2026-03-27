#include "elementos_animados_sprite_sheet.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void desenharTexto(uint32_t* p, const char* texto, int tamanho, int x, int y, uint32_t cor);
extern bool editMode;
extern int editTarget;
extern int editType;

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
int anim_tolerancia = 100;

bool anim_usarColorKey2 = false;
uint8_t anim_keyR2 = 0;
uint8_t anim_keyG2 = 0;
uint8_t anim_keyB2 = 0;

int anim_offsetX = 65;
int anim_offsetY = 0;
int anim_frameInicial = 0;
int anim_frameFinal = 7;

bool anim_modoTeste = false;
bool anim_autoCenter = false;

int anim_frameOffsetX[MAX_ANIM_FRAMES] = { 0 };
int anim_frameOffsetY[MAX_ANIM_FRAMES] = { 0 };

unsigned char* imgSpriteSheet = NULL;
int wSprite, hSprite, cSprite;
int anim_frameAtual = 0;
int anim_timer = 0;

int anim_cursorX = 960;
int anim_cursorY = 540;
bool anim_mostrarCruz = false;
bool anim_editandoPivoVisual = false;
int anim_pivoCursorX = 0;
int anim_pivoCursorY = 0;

void pegarCorNoCursor(bool setColor2) {
    if (!imgSpriteSheet || anim_colunas <= 0 || anim_linhas <= 0) return;

    int frameW = wSprite / anim_colunas;
    int frameH = hSprite / anim_linhas;

    int srcX = (anim_cursorX - anim_posX) / anim_escala;
    int srcY = (anim_cursorY - anim_posY) / anim_escala;

    if (srcX < 0 || srcX >= frameW || srcY < 0 || srcY >= frameH) return;

    int frameCol = anim_frameAtual % anim_colunas;
    int frameRow = (anim_frameAtual / anim_colunas) % anim_linhas;
    int frameX = anim_offsetX + (frameCol * frameW) + anim_frameOffsetX[anim_frameAtual];
    int frameY = anim_offsetY + (frameRow * frameH) + anim_frameOffsetY[anim_frameAtual];

    int imgX = frameX + srcX;
    int imgY = frameY + srcY;

    if (imgX < 0 || imgX >= wSprite || imgY < 0 || imgY >= hSprite) return;

    int idx = (imgY * wSprite + imgX) * 4;
    uint8_t r = imgSpriteSheet[idx + 0];
    uint8_t g = imgSpriteSheet[idx + 1];
    uint8_t b = imgSpriteSheet[idx + 2];
    uint8_t a = imgSpriteSheet[idx + 3];

    if (a == 0) return;

    if (setColor2) {
        anim_keyR2 = r; anim_keyG2 = g; anim_keyB2 = b;
        anim_usarColorKey2 = true;
    }
    else {
        anim_keyR = r; anim_keyG = g; anim_keyB = b;
        anim_usarColorKey = true;
    }
}

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
                if (dist <= anim_tolerancia) isBg = true;
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

    if (editMode && editTarget == 14) {
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

        // TUDO AQUI PARA BAIXO SÓ RENDERIZA SE ESTIVER NA ABA DA ANIMAÇÃO E NO MODO DE TESTE
        if (editMode && editTarget == 14) {
            int fw = wSprite / (anim_colunas > 0 ? anim_colunas : 1);
            int fh = hSprite / (anim_linhas > 0 ? anim_linhas : 1);
            int centroBoxX = anim_posX + (int)((fw * anim_escala) / 2.0f);
            int centroBoxY = anim_posY + (int)((fh * anim_escala) / 2.0f);

            // CRUZ GIGANTE VERDE DA TELA
            if (anim_mostrarCruz) {
                for (int i = 0; i < 1920; i++) {
                    if (centroBoxY >= 0 && centroBoxY < 1080) buffer[centroBoxY * 1920 + i] = 0xFF00FF00;
                }
                for (int j = 0; j < 1080; j++) {
                    if (centroBoxX >= 0 && centroBoxX < 1920) buffer[j * 1920 + centroBoxX] = 0xFF00FF00;
                }
            }

            // BOLINHA VERDE DO PIVÔ (SEMPRE VISÍVEL)
            for (int i = -5; i <= 5; i++) {
                for (int j = -5; j <= 5; j++) {
                    if (i * i + j * j <= 25) {
                        int px = centroBoxX + i;
                        int py = centroBoxY + j;
                        if (px >= 0 && px < 1920 && py >= 0 && py < 1080) buffer[py * 1920 + px] = 0xFF00FF00;
                    }
                }
            }

            // MODO 29: DEFINIR PIVÔ (MIRA MÓVEL)
            if (editType == 29 && anim_editandoPivoVisual) {
                for (int i = -5; i <= 5; i++) {
                    for (int j = -5; j <= 5; j++) {
                        if (i * i + j * j <= 25) {
                            int px = anim_pivoCursorX + i; int py = anim_pivoCursorY + j;
                            if (px >= 0 && px < 1920 && py >= 0 && py < 1080) buffer[py * 1920 + px] = 0xFF00FF00;
                        }
                    }
                }
                char txt[64];
                sprintf(txt, "NOVO PIVO: X:%d  Y:%d", anim_pivoCursorX - centroBoxX, anim_pivoCursorY - centroBoxY);
                desenharTexto(buffer, txt, 25, anim_pivoCursorX + 15, anim_pivoCursorY - 20, 0xFFFFFFFF);
            }

            // MODO 44: SELETOR DE COR PARA EXCLUIR (QUADRADO CHEIO VERDE)
            if (editType == 44) {
                for (int i = -6; i <= 6; i++) {
                    for (int j = -6; j <= 6; j++) {
                        int px = anim_cursorX + i; int py = anim_cursorY + j;
                        if (px >= 0 && px < 1920 && py >= 0 && py < 1080) buffer[py * 1920 + px] = 0xFF00FF00;
                    }
                }
                char txt[64];
                sprintf(txt, "POS: X:%d  Y:%d", anim_cursorX - anim_posX, anim_cursorY - anim_posY);
                desenharTexto(buffer, txt, 25, anim_cursorX + 15, anim_cursorY - 20, 0xFFFFFFFF);
            }

            // TIMELINE
            int startX = 50;
            int startY = 920;
            int size = 80;
            int numFrames = anim_frameFinal - anim_frameInicial + 1;

            if (numFrames > 0 && anim_colunas > 0 && anim_linhas > 0) {
                for (int i = 0; i < numFrames; i++) {
                    int frameReal = anim_frameInicial + i;
                    int pX = startX + (i * (size + 15));

                    if (pX + size > 1900) break;

                    uint32_t corBorda = (frameReal == anim_frameAtual) ? 0xFF00FFFF : 0xFF888888;
                    for (int bx = -4; bx <= size + 4; bx++) {
                        for (int by = -4; by <= size + 4; by++) {
                            int dx = pX + bx; int dy = startY + by;
                            if (dx >= 0 && dx < 1920 && dy >= 0 && dy < 1080) {
                                if (bx < 0 || bx > size || by < 0 || by > size) buffer[dy * 1920 + dx] = corBorda;
                            }
                        }
                    }

                    int fW = wSprite / anim_colunas; int fH = hSprite / anim_linhas;
                    if (fW == 0) fW = 1; if (fH == 0) fH = 1;
                    float escalaThumb = (float)size / (fW > fH ? fW : fH);

                    desenharAnimacaoGrid(buffer, imgSpriteSheet, wSprite, hSprite,
                        anim_colunas, anim_linhas, frameReal,
                        pX, startY, escalaThumb,
                        anim_keyR, anim_keyG, anim_keyB, anim_usarColorKey,
                        anim_keyR2, anim_keyG2, anim_keyB2, anim_usarColorKey2,
                        anim_offsetX, anim_offsetY);
                }
            }
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
                    int dist = (r - keyR) * (r - keyR) + (g - keyG) * (g - keyG) + (b - keyB) * (b - keyB);
                    if (dist <= anim_tolerancia) isBackground = true;
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
                if (dist <= anim_tolerancia) continue;
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