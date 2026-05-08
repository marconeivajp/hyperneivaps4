#include "menu_grafico_render_texto.h"
#include "graphics.h" 
#include <string.h>
#include <stdlib.h>
#include "stb_truetype.h"

extern stbtt_fontinfo font;
extern int temF;
extern int fontAlign;
extern int frameContadorGlobal;

uint32_t getSysColor(int index) {
    uint32_t sysColors[] = { 0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000, 0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933, 0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF };
    if (index < 0 || index > 14) return sysColors[0];
    return sysColors[index];
}

static int advance_utf8(const char* str, int num_chars) {
    int idx = 0;
    for (int i = 0; i < num_chars; i++) {
        if (str[idx] == '\0') break;
        idx++;
        while (str[idx] != '\0' && (str[idx] & 0xC0) == 0x80) {
            idx++;
        }
    }
    return idx;
}

int medirLarguraTexto(const char* texto, int tam) {
    if (!temF || !texto) return 0; float s = stbtt_ScaleForPixelHeight(&font, (float)tam); int totalW = 0;
    for (int i = 0; texto[i]; ++i) { int adv, lsb; stbtt_GetCodepointHMetrics(&font, texto[i], &adv, &lsb); totalW += (int)(adv * s); } return totalW;
}

void desenharTextoClip(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor, int clipX, int clipW) {
    if (!temF || !texto || !pixels) return; float s = stbtt_ScaleForPixelHeight(&font, (float)tam); int asc; stbtt_GetFontVMetrics(&font, &asc, 0, 0); asc = (int)(asc * s); int curX = x; uint8_t baseAlpha = (cor >> 24) & 0xFF; if (baseAlpha < 10) return;
    for (int i = 0; texto[i]; ++i) {
        int adv, lsb, x0, y0, x1, y1; stbtt_GetCodepointHMetrics(&font, texto[i], &adv, &lsb); stbtt_GetCodepointBitmapBox(&font, texto[i], s, s, &x0, &y0, &x1, &y1); int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            unsigned char* b = (unsigned char*)malloc(w * h); stbtt_MakeCodepointBitmap(&font, b, w, h, w, s, s, texto[i]);
            for (int cy = 0; cy < h; ++cy) for (int cx = 0; cx < w; ++cx) {
                int pX = curX + x0 + cx; int pY = y + asc + y0 + cy;
                if (pX >= clipX && pX < (clipX + clipW) && pY >= 0 && pY < 1080 && pX >= 0 && pX < 1920) {
                    uint8_t alpha = b[cy * w + cx]; uint8_t finalAlpha = (alpha * baseAlpha) / 255; if (finalAlpha > 30) pixels[pY * 1920 + pX] = (finalAlpha << 24) | (cor & 0x00FFFFFF);
                }
            } free(b);
        } curX += (int)(adv * s);
    }
}

void desenharTextoAlinhadoAnimado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor, bool isSelected) {
    int espacoDisponivel = maxW - 40;
    int pxWidth = medirLarguraTexto(textoOriginal, fTam);
    int posX = xBase + 20;

    if (pxWidth <= espacoDisponivel) {
        if (fontAlign == 1) posX = xBase + (maxW / 2) - (pxWidth / 2);
        else if (fontAlign == 2) posX = xBase + maxW - pxWidth - 20;
        desenharTextoClip(p, textoOriginal, fTam, posX, y, cor, xBase, maxW);
    }
    else {
        if (isSelected) {
            int excessPx = pxWidth - espacoDisponivel + 40;
            int cycle = 300;
            int pos = frameContadorGlobal % cycle;
            int pixelShift = 0;

            if (pos > 60 && pos <= 150) {
                float progress = (float)(pos - 60) / 90.0f;
                progress = progress * progress * (3.0f - 2.0f * progress);
                pixelShift = (int)(progress * excessPx);
            }
            else if (pos > 150 && pos <= 210) { pixelShift = excessPx; }
            else if (pos > 210) {
                float progress = (float)(pos - 210) / 90.0f;
                progress = progress * progress * (3.0f - 2.0f * progress);
                pixelShift = (int)((1.0f - progress) * excessPx);
            }
            desenharTextoClip(p, textoOriginal, fTam, posX - pixelShift, y, cor, xBase + 20, espacoDisponivel);
        }
        else {
            int maxChars = (int)(espacoDisponivel / (fTam * 0.55f));
            if (maxChars < 4) maxChars = 4;
            char txtFinal[512];
            strncpy(txtFinal, textoOriginal, maxChars - 3);
            txtFinal[maxChars - 3] = '\0';
            strcat(txtFinal, "...");
            desenharTexto(p, txtFinal, fTam, posX, y, cor);
        }
    }
}

void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor) {
    desenharTextoAlinhadoAnimado(p, textoOriginal, fTam, xBase, y, maxW, cor, false);
}