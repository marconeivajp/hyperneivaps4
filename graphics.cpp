#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Headers do SDK do PS4 (OpenOrbis) necessários para o vídeo
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>

// INCLUSÃO DO LEITOR DE PDF E MANGÁ
#include "pdf.h"

// Instanciação das variáveis da fonte
stbtt_fontinfo font;
int temF = 0;

// --- VARIÁVEIS INTERNAS DE VÍDEO ---
// Ficam "escondidas" aqui, o main.cpp já não precisa de saber delas
int video = 0;
int bA = 0; // Buffer Ativo (0 ou 1 para o Double Buffering)
void* buffers[2];

// =========================================================================
// SISTEMA DE VÍDEO DO PS4
// =========================================================================

/**
 * Inicializa a saída de vídeo nativa do PS4.
 * Aloca memória direta ("Direct Memory") para a resolução 1920x1080 com Double Buffering.
 */
void inicializarVideo() {
    // Abre a porta de vídeo principal da consola
    video = sceVideoOutOpen(255, 0, 0, NULL);

    // Calcula o tamanho da memória para a resolução Full HD (1920x1080) a 32 bits (4 bytes por pixel)
    // A máscara com 0x1FFFFF garante o alinhamento de memória exigido pela placa gráfica do PS4
    size_t bSz = ((1920 * 1080 * 4) + 0x1FFFFF) & ~0x1FFFFF;
    off_t ph;

    // Aloca e mapeia a "Direct Memory" (Memória RAM partilhada com o GPU) para os dois buffers
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz * 2, 2097152, 2, &ph);
    void* vM = NULL;
    sceKernelMapDirectMemory(&vM, bSz * 2, 0x33, 0, ph, 2097152);

    // Atribui os dois buffers aos ponteiros (o segundo começa logo onde o primeiro acaba)
    buffers[0] = vM;
    buffers[1] = (void*)((uint8_t*)vM + bSz);

    // Regista a resolução e as características na saída de vídeo do Orbis OS
    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, 1920, 1080, 1920);
    sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr);
}

/**
 * Retorna o ponteiro de memória onde a interface vai pintar os píxeis neste exato frame.
 */
uint32_t* obterBufferVideo() {
    return (uint32_t*)buffers[bA];
}

/**
 * Envia o frame que acabou de ser desenhado para a TV (Flip)
 * e troca os buffers (Double Buffering) para pintar o próximo ecrã no fundo.
 */
void submeterTela() {
    sceVideoOutSubmitFlip(video, bA, 1, 0);
    bA = (bA + 1) % 2; // Alterna entre buffer 0 e 1 (Matemática circular)
    sceKernelUsleep(16000); // Pausa de 16ms (~60 FPS) para não fritar o processador do PS4
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

// =========================================================================
// FUNÇÃO PARA RENDERIZAR O PDF/MANGÁ NA TELA
// =========================================================================
void desenharPDFnaTela(uint32_t* pixels) {
    if (visualizandoPDF && imgPaginaAtual != NULL) {
        // Pinta um fundo Cinza Escuro sólido atrás do PDF
        for (int y = 0; y < 1080; y++) {
            for (int x = 0; x < 1920; x++) {
                pixels[y * 1920 + x] = 0xFF151515;
            }
        }

        // Calcula as dimensões finais com o nível de Zoom
        int dW = (int)(pdfImgW * pdfZoom);
        int dH = (int)(pdfImgH * pdfZoom);

        // Centraliza a imagem e aplica o offset das setas direcionais
        int posX = (1920 - dW) / 2 + pdfOffsetX;
        int posY = (1080 - dH) / 2 + pdfOffsetY;

        // Pinta a página renderizada na tela
        desenharRedimensionado(pixels, imgPaginaAtual, pdfImgW, pdfImgH, dW, dH, posX, posY);

        // Desenha a Interface do Leitor (Texto)
        char txtPagina[100];
        sprintf(txtPagina, "PAGINA: %d / %d   |   ZOOM: %d%%", pdfPaginaAtual, pdfTotalPaginas, (int)(pdfZoom * 100));

        // Sombra do Texto
        desenharTexto(pixels, txtPagina, 30, 52, 1022, 0xFF000000);
        desenharTexto(pixels, "[BOLINHA] Fechar | [L1] Voltar | [R1] Avancar | [L2]/[R2] Zoom | [SETAS] Mover", 24, 52, 1052, 0xFF000000);

        // Texto Branco Principal
        desenharTexto(pixels, txtPagina, 30, 50, 1020, 0xFFFFFFFF);
        desenharTexto(pixels, "[BOLINHA] Fechar | [L1] Voltar | [R1] Avancar | [L2]/[R2] Zoom | [SETAS] Mover", 24, 50, 1050, 0xFFAAAAAA);
    }
}