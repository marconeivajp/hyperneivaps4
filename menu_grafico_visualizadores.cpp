#include "menu_grafico_visualizadores.h"
#include "graphics.h"
#include "bloco_de_notas.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>

extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;
extern int panMidiaX, panMidiaY;
extern bool visualizandoMidiaTexto;
extern char* textoMidiaBuffer;
extern char* linhasTexto[5000];
extern int totalLinhasTexto, textoMidiaScroll;
extern MenuLevel menuAtual;
extern MenuLevel menuAtualEsq;

bool renderizarLeitorMidia(uint32_t* p) {
    if (visualizandoMidiaTexto && textoMidiaBuffer) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;
        for (int by = 0; by < 80; by++) for (int bx = 0; bx < 1920; bx++) p[by * 1920 + bx] = 0xFF303030;
        desenharTexto(p, "LEITOR DE ARQUIVOS", 35, 50, 25, 0xFF00AAFF);
        int maxLinhasVisiveis = 23;
        for (int i = 0; i < maxLinhasVisiveis; i++) {
            int indiceDaLinha = textoMidiaScroll + i;
            if (indiceDaLinha < totalLinhasTexto && linhasTexto[indiceDaLinha] != NULL) {
                desenharTexto(p, linhasTexto[indiceDaLinha], 30, 50, 120 + (i * 40), 0xFFDDDDDD);
            }
        }
        for (int by = 0; by < 60; by++) for (int bx = 0; bx < 1920; bx++) p[(1020 + by) * 1920 + bx] = 0xFF222222;
        char rodape[128]; sprintf(rodape, "[Setas] Rolar   |   [O] Voltar   |   Linha: %d / %d", textoMidiaScroll, totalLinhasTexto);
        desenharTexto(p, rodape, 25, 50, 1035, 0xFF00AAFF);
        return true;
    }
    if (visualizandoMidiaImagem && imgMidia) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF000000;
        if (wM <= 0 || hM <= 0) return true;
        float propW = 1920.0f / (float)wM, propH = 1080.0f / (float)hM;
        float fit = (propW < propH) ? propW : propH;
        float z = fullscreenMidia ? fit : zoomMidia;
        if (z < 0.02f) z = 0.02f;
        if (z > 20.0f) z = 20.0f;
        int drawW = (int)(wM * z), drawH = (int)(hM * z);
        if (drawW < 1) drawW = 1;
        if (drawH < 1) drawH = 1;
        int limX = (drawW > 1920) ? (drawW - 1920) / 2 : 0;
        int limY = (drawH > 1080) ? (drawH - 1080) / 2 : 0;
        if (fullscreenMidia) { panMidiaX = 0; panMidiaY = 0; }
        if (panMidiaX > limX) panMidiaX = limX;
        if (panMidiaX < -limX) panMidiaX = -limX;
        if (panMidiaY > limY) panMidiaY = limY;
        if (panMidiaY < -limY) panMidiaY = -limY;
        int posX = (1920 - drawW) / 2 + panMidiaX, posY = (1080 - drawH) / 2 + panMidiaY;
        int x0 = posX < 0 ? 0 : posX;
        int y0 = posY < 0 ? 0 : posY;
        int x1 = posX + drawW; if (x1 > 1920) x1 = 1920;
        int y1 = posY + drawH; if (y1 > 1080) y1 = 1080;
        for (int py = y0; py < y1; py++) {
            int sy = (int)(((long long)(py - posY) * hM) / drawH);
            if (sy < 0) sy = 0;
            if (sy >= hM) sy = hM - 1;
            int rowBase = sy * wM;
            for (int px = x0; px < x1; px++) {
                int sx = (int)(((long long)(px - posX) * wM) / drawW);
                if (sx < 0) sx = 0;
                if (sx >= wM) sx = wM - 1;
                int idx = (rowBase + sx) * 4;
                unsigned char a = imgMidia[idx + 3];
                if (a > 0) p[py * 1920 + px] = ((unsigned int)a << 24) | ((unsigned int)imgMidia[idx] << 16) | ((unsigned int)imgMidia[idx + 1] << 8) | (unsigned int)imgMidia[idx + 2];
            }
        }
        for (int by = 0; by < 130; by++) for (int bx = 0; bx < 400; bx++) { int pxX = 1480 + bx, pyY = 930 + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xAA000000; }
        desenharTexto(p, "[X] Tela Cheia", 25, 1500, 920, 0xFFFFFFFF); desenharTexto(p, "[Cima/Baixo] Zoom", 25, 1500, 960, 0xFFFFFFFF); desenharTexto(p, "[Analogico] Mover", 25, 1500, 1000, 0xFFFFFFFF); desenharTexto(p, "[O] Voltar", 25, 1500, 1040, 0xFFFFFFFF);
        return true;
    }
    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) { renderizarNotepad(p); return true; }
    return false;
}