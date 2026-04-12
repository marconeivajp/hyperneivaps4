#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Headers do SDK do PS4 (OpenOrbis) necessários para o vídeo
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>

// INCLUSÃO DO LEITOR DE PDF E MANGÁ
#include "pdf.h"
#include "libretro_bridge.h"

extern bool menuEmuladorAtivo;

// Instanciação das variáveis da fonte
stbtt_fontinfo font;
int temF = 0;

// --- VARIÁVEIS INTERNAS DE VÍDEO ---
int video = 0;
int bA = 0; 
void* buffers[2];

// V39: Backup para o "Frozen Frame" (Global para V58)
uint32_t* backupEmuFrame = NULL;

// =========================================================================
// SISTEMA DE VÍDEO DO PS4
// =========================================================================

void inicializarVideo() {
    video = sceVideoOutOpen(255, 0, 0, NULL);

    size_t bSz = ((1920 * 1080 * 4) + 0x1FFFFF) & ~0x1FFFFF;
    off_t ph;

    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz * 2, 2097152, 2, &ph);
    void* vM = NULL;
    sceKernelMapDirectMemory(&vM, bSz * 2, 0x33, 0, ph, 2097152);

    buffers[0] = vM;
    buffers[1] = (void*)((uint8_t*)vM + bSz);

    backupEmuFrame = (uint32_t*)malloc(1920 * 1080 * 4);
    if (backupEmuFrame) memset(backupEmuFrame, 0, 1920 * 1080 * 4);

    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, 1920, 1080, 1920);
    sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr);
}

uint32_t* obterBufferVideo() {
    return (uint32_t*)buffers[bA];
}

void submeterTela() {
    sceVideoOutSubmitFlip(video, bA, 1, 0);
    bA = (bA + 1) % 2; 
    sceKernelUsleep(16000); 
}

void submeterTelaSemPausa() {
    sceVideoOutSubmitFlip(video, bA, 1, 0);
    bA = (bA + 1) % 2;
    // V52.2: Sincronização inteligente: só atrasa no menu/pausa para evitar flicker
    if (bridge_esta_pausado() || menuEmuladorAtivo) sceKernelUsleep(16000); 
}

// =========================================================================
// FUNÇÕES DE DESENHO ORIGINAIS
// =========================================================================

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

void desenharPDFnaTela(uint32_t* pixels) {
    if (visualizandoPDF && imgPaginaAtual != NULL) {
        for (int y = 0; y < 1080; y++) for (int x = 0; x < 1920; x++) pixels[y * 1920 + x] = 0xFF000000;
        int dW = (int)(pdfImgW * pdfZoom); int dH = (int)(pdfImgH * pdfZoom);
        int posX = (1920 - dW) / 2 + pdfOffsetX; int posY = (1080 - dH) / 2 + pdfOffsetY;
        desenharRedimensionado(pixels, imgPaginaAtual, pdfImgW, pdfImgH, dW, dH, posX, posY);
        char txtPagina[100]; sprintf(txtPagina, "PAGINA: %d / %d   |   ZOOM: %d%%", pdfPaginaAtual, pdfTotalPaginas, (int)(pdfZoom * 100));
        desenharTexto(pixels, txtPagina, 30, 52, 1022, 0xFF000000);
        desenharTexto(pixels, "[BOLINHA] Fechar | [L1] Voltar | [R1] Avancar | [L2]/[R2] Zoom | [SETAS] Mover", 24, 52, 1052, 0xFF000000);
        desenharTexto(pixels, txtPagina, 30, 50, 1020, 0xFFFFFFFF);
        desenharTexto(pixels, "[BOLINHA] Fechar | [L1] Voltar | [R1] Avancar | [L2]/[R2] Zoom | [SETAS] Mover", 24, 50, 1050, 0xFFAAAAAA);
    }
}

// =========================================================================
// NOVO: SUPORTE PARA O EMULADOR (LIBRETRO) - OTIMIZADO V32/V39/V40
// =========================================================================
extern int gEmuPixelFormat;

void desenharBufferEmulador(const void* data, unsigned width, unsigned height, size_t pitch) {
    uint32_t* pixels = obterBufferVideo();
    if (!pixels || !data) return;

    const int dW = 1920; const int dH = 1080;
    uint32_t stepX = (width << 16) / dW;
    uint32_t stepY = (height << 16) / dH;
    uint32_t currY = 0;

    if (gEmuPixelFormat == 1) { // 32-BIT
        uint32_t* src = (uint32_t*)data;
        for (int y = 0; y < dH; y++) {
            uint32_t srcY = currY >> 16;
            uint32_t* srcRow = (uint32_t*)((uint8_t*)src + (srcY * pitch));
            uint32_t* dstRow = pixels + (y * dW);
            uint32_t currX = 0;
            for (int x = 0; x < dW; x++) {
                dstRow[x] = 0xFF000000 | srcRow[currX >> 16];
                currX += stepX;
            }
            currY += stepY;
        }
    } else { // 16-BIT
        uint16_t* src = (uint16_t*)data;
        for (int y = 0; y < dH; y++) {
            uint32_t srcY = currY >> 16;
            uint16_t* srcRow = (uint16_t*)((uint8_t*)src + (srcY * pitch));
            uint32_t* dstRow = pixels + (y * dW);
            uint32_t currX = 0;
            for (int x = 0; x < dW; x++) {
                uint16_t c565 = srcRow[currX >> 16];
                uint32_t r = (c565 & 0xF800) << 8;
                uint32_t g = (c565 & 0x07E0) << 5;
                uint32_t b = (c565 & 0x001F) << 3;
                r |= (r >> 5) & 0xFF0000; g |= (g >> 6) & 0x00FF00; b |= (b >> 5);
                dstRow[x] = 0xFF000000 | r | g | b;
                currX += stepX;
            }
            currY += stepY;
        }
    }
}

// V40: Captura o frame apenas uma vez no momento da pausa
void capturarUltimoFrame() {
    uint32_t* pixels = obterBufferVideo();
    if (pixels && backupEmuFrame) memcpy(backupEmuFrame, pixels, 1920 * 1080 * 4);
}

// =========================================================================
// NOVO: MENU DE PAUSA DO EMULADOR (V33/V39/V40/V41)
// =========================================================================
void desenharMenuEmulador(int selecao) {
    uint32_t* pixels = obterBufferVideo();
    if (!pixels) return;

    // 1. Restaura o frame congelado do backup
    if (backupEmuFrame) memcpy(pixels, backupEmuFrame, 1920 * 1080 * 4);

    // 2. Caixa do Menu (Escurecimento para destaque)
    int boxW = 500; int boxH = 450;
    int boxX = (1920 - boxW) / 2; int boxY = (1080 - boxH) / 2;
    for (int y = boxY; y < boxY + boxH; y++) {
        uint32_t* row = pixels + (y * 1920);
        for (int x = boxX; x < boxX + boxW; x++) {
            uint32_t bg = row[x];
            uint8_t r = (((bg >> 16) & 0xFF) * 5) / 10; // 50% brilho
            uint8_t g = (((bg >> 8) & 0xFF) * 5) / 10;
            uint8_t b = ((bg & 0xFF) * 5) / 10;
            row[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    uint32_t* p = obterBufferVideo();
    if (p && backupEmuFrame) memcpy(backupEmuFrame, p, 1920 * 1080 * 4);
}