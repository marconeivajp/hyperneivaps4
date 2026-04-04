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
        float propW = 1920.0f / wM, propH = 1080.0f / hM, propMax = (propW < propH) ? propW : propH;
        int drawW, drawH;
        if (fullscreenMidia) { drawW = (int)(wM * propMax); drawH = (int)(hM * propMax); }
        else { drawW = (int)(wM * zoomMidia); drawH = (int)(hM * zoomMidia); if (drawW > 1920 || drawH > 1080) { drawW = (int)(wM * propMax); drawH = (int)(hM * propMax); zoomMidia = propMax; } }
        int posX = (1920 - drawW) / 2, posY = (1080 - drawH) / 2;
        desenharRedimensionado(p, imgMidia, wM, hM, drawW, drawH, posX, posY);
        for (int by = 0; by < 130; by++) for (int bx = 0; bx < 400; bx++) { int pxX = 1480 + bx, pyY = 930 + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xAA000000; }
        desenharTexto(p, "[X] Tela Cheia", 25, 1500, 960, 0xFFFFFFFF); desenharTexto(p, "[Cima/Baixo] Zoom", 25, 1500, 1000, 0xFFFFFFFF); desenharTexto(p, "[O] Voltar", 25, 1500, 1040, 0xFFFFFFFF);
        return true;
    }
    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) { renderizarNotepad(p); return true; }
    return false;
}