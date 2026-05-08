#include "explorar_conversor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void obterNomeJogoSFO(const char* cusa, char* nomeSaida) {
    strcpy(nomeSaida, "Desconhecido");
    char pathSfo[512];
    sprintf(pathSfo, "/user/appmeta/%s/param.sfo", cusa);
    FILE* f = fopen(pathSfo, "rb");
    if (!f) return;

    unsigned int magic; fread(&magic, 4, 1, f);
    if (magic != 0x46535000) { fclose(f); return; }

    fseek(f, 0x08, SEEK_SET);
    unsigned int keyOffset = 0, dataOffset = 0, entries = 0;
    fread(&keyOffset, 4, 1, f); fread(&dataOffset, 4, 1, f); fread(&entries, 4, 1, f);

    for (unsigned int i = 0; i < entries; i++) {
        fseek(f, 0x14 + (i * 16), SEEK_SET);
        unsigned short kOff; fread(&kOff, 2, 1, f);
        fseek(f, 2, SEEK_CUR);
        unsigned int dLen, dMax, dOff;
        fread(&dLen, 4, 1, f); fread(&dMax, 4, 1, f); fread(&dOff, 4, 1, f);

        fseek(f, keyOffset + kOff, SEEK_SET);
        char key[64]; memset(key, 0, 64); fread(key, 1, 63, f);

        if (strcmp(key, "TITLE") == 0) {
            fseek(f, dataOffset + dOff, SEEK_SET);
            int realLen = dLen < 63 ? dLen : 63;
            fread(nomeSaida, 1, realLen, f);
            nomeSaida[realLen] = '\0';
            for (int c = 0; c < realLen; c++) { if (nomeSaida[c] == '\n' || nomeSaida[c] == '\r') nomeSaida[c] = '\0'; }
            break;
        }
    } fclose(f);
}

void redimensionarImagem(unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int srcX = (x * sw) / dw;
            int srcY = (y * sh) / dh;
            int srcIndex = (srcY * sw + srcX) * 4;
            int dstIndex = (y * dw + x) * 4;
            dst[dstIndex] = src[srcIndex];
            dst[dstIndex + 1] = src[srcIndex + 1];
            dst[dstIndex + 2] = src[srcIndex + 2];
            dst[dstIndex + 3] = src[srcIndex + 3];
        }
    }
}

static unsigned short colorTo565(int r, int g, int b) {
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static int colorDistance(int r1, int g1, int b1, int r2, int g2, int b2) {
    return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

void salvarComoDDS(const char* filepath, unsigned char* img, int w, int h) {
    FILE* f = fopen(filepath, "wb");
    if (!f) return;

    unsigned int header[32];
    memset(header, 0, sizeof(header));
    header[0] = 0x20534444;
    header[1] = 124;
    header[2] = 0x00081007;
    header[3] = h;
    header[4] = w;
    header[5] = (w > 4 ? w : 4) * (h > 4 ? h : 4) / 2;
    header[19] = 32;
    header[20] = 0x00000004;
    header[21] = 0x31545844;
    header[27] = 0x1000;

    fwrite(header, 1, 128, f);

    for (int by = 0; by < h / 4; by++) {
        for (int bx = 0; bx < w / 4; bx++) {
            int minR = 255, minG = 255, minB = 255;
            int maxR = 0, maxG = 0, maxB = 0;
            unsigned char block[16][3];

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = bx * 4 + x;
                    int py = by * 4 + y;
                    int idx = (py * w + px) * 4;
                    unsigned char r = img[idx];
                    unsigned char g = img[idx + 1];
                    unsigned char b = img[idx + 2];
                    block[y * 4 + x][0] = r;
                    block[y * 4 + x][1] = g;
                    block[y * 4 + x][2] = b;

                    if (r < minR) minR = r; if (g < minG) minG = g; if (b < minB) minB = b;
                    if (r > maxR) maxR = r; if (g > maxG) maxG = g; if (b > maxB) maxB = b;
                }
            }

            unsigned short c0 = colorTo565(maxR, maxG, maxB);
            unsigned short c1 = colorTo565(minR, minG, minB);

            if (c0 < c1) {
                unsigned short temp = c0; c0 = c1; c1 = temp;
                int tr = maxR, tg = maxG, tb = maxB;
                maxR = minR; maxG = minG; maxB = minB;
                minR = tr; minG = tg; minB = tb;
            }
            else if (c0 == c1) {
                if (c0 > 0) c1 = c0 - 1; else c0 = 1;
            }

            unsigned int indices = 0;
            for (int i = 0; i < 16; i++) {
                int d0 = colorDistance(block[i][0], block[i][1], block[i][2], maxR, maxG, maxB);
                int d1 = colorDistance(block[i][0], block[i][1], block[i][2], minR, minG, minB);
                int d2 = colorDistance(block[i][0], block[i][1], block[i][2], (2 * maxR + minR) / 3, (2 * maxG + minG) / 3, (2 * maxB + minB) / 3);
                int d3 = colorDistance(block[i][0], block[i][1], block[i][2], (maxR + 2 * minR) / 3, (maxG + 2 * minG) / 3, (maxB + 2 * minB) / 3);

                unsigned int idx = 0; int minD = d0;
                if (d1 < minD) { minD = d1; idx = 1; }
                if (d2 < minD) { minD = d2; idx = 2; }
                if (d3 < minD) { minD = d3; idx = 3; }
                indices |= (idx << (i * 2));
            }

            fwrite(&c0, 2, 1, f);
            fwrite(&c1, 2, 1, f);
            fwrite(&indices, 4, 1, f);
        }
    }
    fclose(f);
}