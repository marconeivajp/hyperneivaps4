#include "graphics.h"
#include <stdlib.h>

// Instanciação das variáveis da fonte
stbtt_fontinfo font;
int temF = 0;

void desenharRedimensionado(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY) {
    if (!img || !pixels || dW <= 0 || dH <= 0) return;
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; if (pY < 0 || pY >= 1080) continue;
        for (int x = 0; x < dW; x++) {
            int pX = posX + x; if (pX < 0 || pX >= 1920) continue;
            int oX = (x * imgW) / dW; int oY = (y * imgH) / dH;
            int idx = (oY * imgW + oX) * 4; uint8_t a = img[idx + 3];
            if (a > 0) pixels[pY * 1920 + pX] = (a << 24) | (img[idx + 0] << 16) | (img[idx + 1] << 8) | img[idx + 2];
        }
    }
}

void desenharDiscoRedondo(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY) {
    if (!img || !pixels || dW <= 0 || dH <= 0) return;
    float r = dW / 2.0f;
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; if (pY < 0 || pY >= 1080) continue;
        for (int x = 0; x < dW; x++) {
            int pX = posX + x; if (pX < 0 || pX >= 1920) continue;
            float dx = x - r; float dy = y - r;
            if ((dx * dx + dy * dy) <= (r * r)) {
                int idx = ((y * imgH / dH) * imgW + (x * imgW / dW)) * 4; uint8_t a = img[idx + 3];
                if (a > 0) pixels[pY * 1920 + pX] = (a << 24) | (img[idx + 0] << 16) | (img[idx + 1] << 8) | img[idx + 2];
            }
        }
    }
}

void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor) {
    if (!temF || !texto || !pixels) return;
    float s = stbtt_ScaleForPixelHeight(&font, (float)tam);
    int asc; stbtt_GetFontVMetrics(&font, &asc, 0, 0); asc = (int)(asc * s);
    int curX = x;
    for (int i = 0; texto[i]; ++i) {
        int adv, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&font, texto[i], &adv, &lsb);
        stbtt_GetCodepointBitmapBox(&font, texto[i], s, s, &x0, &y0, &x1, &y1);
        int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            unsigned char* b = (unsigned char*)malloc(w * h);
            stbtt_MakeCodepointBitmap(&font, b, w, h, w, s, s, texto[i]);
            for (int cy = 0; cy < h; ++cy) for (int cx = 0; cx < w; ++cx) {
                int pX = curX + x0 + cx; int pY = y + asc + y0 + cy;
                if (pX >= 0 && pX < 1920 && pY >= 0 && pY < 1080) {
                    uint8_t alpha = b[cy * w + cx];
                    if (alpha > 30) pixels[pY * 1920 + pX] = cor;
                }
            }
            free(b);
        }
        curX += (int)(adv * s);
    }
}