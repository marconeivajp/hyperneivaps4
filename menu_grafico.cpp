#include "menu_grafico.h"
#include "menu.h"
#include "graphics.h"
#include "bloco_de_notas.h" 
#include "menu_audio.h" 
#include "menu_upload.h" // <-- AQUI INCLUÍMOS O DESENHO DO MENU DE UPLOAD
#include <string.h>
#include <stdio.h>

extern bool editMode;
extern bool showOpcoes;
extern int selOpcao;
extern char pathExplorar[256];
extern bool marcados[3000];
extern const char* listaOpcoes[10];

extern char bufferTecladoC[128];
extern unsigned char* capasAssets[6];
extern unsigned char* discosAssets[6];
extern unsigned char* imgPreview;

extern int listX, listY, listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;
extern int wC[6], hC[6], wD[6], hD[6];
extern int wP, hP;

// VARIÁVEIS DA IMAGEM
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;

// VARIÁVEIS DO TEXTO
extern bool visualizandoMidiaTexto;
extern char* textoMidiaBuffer;
extern char* linhasTexto[5000];
extern int totalLinhasTexto;
extern int textoMidiaScroll;

void desenharInterface(uint32_t* p) {

    // 0.1 DESENHAR LEITOR DE TEXTO E CÓDIGO
    if (visualizandoMidiaTexto && textoMidiaBuffer) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;
        for (int by = 0; by < 80; by++) {
            for (int bx = 0; bx < 1920; bx++) p[by * 1920 + bx] = 0xFF303030;
        }
        desenharTexto(p, "LEITOR DE ARQUIVOS (TXT, XML, INI, JSON, CPP...)", 35, 50, 25, 0xFF00AAFF);

        int maxLinhasVisiveis = 23;
        for (int i = 0; i < maxLinhasVisiveis; i++) {
            int indiceDaLinha = textoMidiaScroll + i;
            if (indiceDaLinha < totalLinhasTexto && linhasTexto[indiceDaLinha] != NULL) {
                desenharTexto(p, linhasTexto[indiceDaLinha], 30, 50, 120 + (i * 40), 0xFFDDDDDD);
            }
        }

        for (int by = 0; by < 60; by++) {
            for (int bx = 0; bx < 1920; bx++) p[(1020 + by) * 1920 + bx] = 0xFF222222;
        }
        char rodape[128];
        sprintf(rodape, "[Setas] Rolar Texto   |   [O] Voltar   |   Linha: %d / %d", textoMidiaScroll, totalLinhasTexto);
        desenharTexto(p, rodape, 25, 50, 1035, 0xFF00AAFF);

        return;
    }

    // 0.2 DESENHAR IMAGEM
    if (visualizandoMidiaImagem && imgMidia) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF000000;

        float propW = 1920.0f / wM;
        float propH = 1080.0f / hM;
        float propMax = (propW < propH) ? propW : propH;

        int drawW, drawH;
        if (fullscreenMidia) {
            drawW = (int)(wM * propMax);
            drawH = (int)(hM * propMax);
        }
        else {
            drawW = (int)(wM * zoomMidia);
            drawH = (int)(hM * zoomMidia);
            if (drawW > 1920 || drawH > 1080) {
                drawW = (int)(wM * propMax);
                drawH = (int)(hM * propMax);
                zoomMidia = propMax;
            }
        }

        int posX = (1920 - drawW) / 2;
        int posY = (1080 - drawH) / 2;

        desenharRedimensionado(p, imgMidia, wM, hM, drawW, drawH, posX, posY);

        for (int by = 0; by < 130; by++) {
            for (int bx = 0; bx < 400; bx++) {
                int pxX = 1480 + bx; int pyY = 930 + by;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xAA000000;
            }
        }

        desenharTexto(p, "[X] Tela Cheia / Normal", 25, 1500, 960, 0xFFFFFFFF);
        desenharTexto(p, "[Cima/Baixo] Zoom", 25, 1500, 1000, 0xFFFFFFFF);
        desenharTexto(p, "[O] Voltar", 25, 1500, 1040, 0xFFFFFFFF);

        return;
    }

    // 1. DESENHAR LISTA DE ITENS
    if (menuAtual != MENU_NOTEPAD) {
        for (int i = 0; i < 6; i++) {
            int gIdx = i + off; if (gIdx >= totalItens) break;
            int yP = listY + (i * 120);

            uint32_t corFundo = 0xAA222222;
            uint32_t corTexto = 0xFFFFFFFF;

            if (menuAtual == MENU_EXPLORAR && marcados[gIdx]) corFundo = 0xAAFFFF99;
            if (gIdx == sel) { corFundo = 0xFF00AAFF; corTexto = 0xFF000000; }

            for (int by = 0; by < listH; by++) for (int bx = 0; bx < listW; bx++) {
                int pxX = listX + bx; int pyY = yP + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = corFundo;
            }
            desenharTexto(p, nomes[gIdx], 35, listX + 20, yP + 20, corTexto);
        }
    }

    // 2. DESENHAR O BLOCO DE NOTAS (NOTEPAD)
    if (menuAtual == MENU_NOTEPAD) {
        renderizarNotepad(p);
    }

    // 3. DESENHAR AS IMAGENS (CAPAS, DISCOS E SCRAPER)
    if (menuAtual == JOGAR_XML || editMode) {
        int idx = sel % 6;
        if (capasAssets[idx]) desenharRedimensionado(p, capasAssets[idx], wC[idx], hC[idx], capaW, capaH, capaX, capaY);
        if (discosAssets[idx]) desenharDiscoRedondo(p, discosAssets[idx], wD[idx], hD[idx], discoW, discoH, discoX, discoY);
    }
    else if (menuAtual == SCRAPER_LIST && imgPreview) {
        desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
    }
    else if (menuAtual == MENU_EXPLORAR) {
        char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar);
        desenharTexto(p, bread, 30, listX, 1020, 0xFFFFFFFF);
    }

    // 4. DESENHAR MENU SUSPENSO (OPÇÕES DO EXPLORADOR)
    if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) {
        for (int my = 0; my < 500; my++) for (int mx = 0; mx < 350; mx++) {
            int pxX = discoX + mx; int pyY = discoY - 100 + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xEE111111;
        }
        for (int i = 0; i < 10; i++) {
            uint32_t corOp = (i == selOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
            desenharTexto(p, listaOpcoes[i], 30, discoX + 20, discoY - 80 + (i * 45), corOp);
        }
    }

    // 5. DESENHAR MENU SUSPENSO (OPÇÕES DE ÁUDIO)
    desenharMenuAudio(p);

    // 6. DESENHAR MENU SUSPENSO (OPÇÕES DE UPLOAD) <-- AGORA VAI APARECER NA TELA!
    desenharMenuUpload(p);

    if (msgTimer > 0) {
        desenharTexto(p, msgStatus, 40, 100, 950, 0xFFFFFFFF);
        msgTimer--;
    }
}