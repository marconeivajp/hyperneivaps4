// --- INÍCIO DO ARQUIVO menu_grafico.cpp ---
#include "menu_grafico.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>

// --- VARIÁVEIS EXTERNAS (Vindas do main e de outros ficheiros) ---
extern bool editMode;
extern bool showOpcoes;
extern int selOpcao;
extern int selAudioOpcao;
extern char pathExplorar[256];
extern bool marcados[3000];
extern const char* listaOpcoes[10];
extern const char* listaOpcoesAudio[11];

extern char bufferTecladoC[128];
extern unsigned char* capasAssets[6];
extern unsigned char* discosAssets[6];
extern unsigned char* imgPreview;

extern int listX, listY, listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;
extern int wC[6], hC[6], wD[6], hD[6];
extern int wP, hP;

// --- VARIÁVEIS PARA O VISUALIZADOR DE IMAGENS ---
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;

// --- FUNÇÃO PRINCIPAL DE RENDERIZAÇÃO DA INTERFACE ---
void desenharInterface(uint32_t* p) {

    // 0. DESENHAR IMAGEM CENTRALIZADA E NO TAMANHO ORIGINAL
    if (visualizandoMidiaImagem && imgMidia) {

        // Pinta o fundo de preto para destacar a foto (efeito cinema/lightbox)
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF000000;

        int drawW = wM;
        int drawH = hM;

        // Proteção: Se a foto for maior que a tela da TV (ex: 4K), 
        // diminui ela mantendo a proporção correta para não estourar a memória.
        if (drawW > 1920 || drawH > 1080) {
            float propW = 1920.0f / drawW;
            float propH = 1080.0f / drawH;
            float prop = (propW < propH) ? propW : propH;
            drawW = (int)(drawW * prop);
            drawH = (int)(drawH * prop);
        }

        // Calcula a posição X e Y para a foto ficar exatamente no meio da tela
        int posX = (1920 - drawW) / 2;
        int posY = (1080 - drawH) / 2;

        // Desenha a imagem na tela usando a nova posição e escala
        desenharRedimensionado(p, imgMidia, wM, hM, drawW, drawH, posX, posY);

        // Fundo semitransparente escuro no cantinho para dar leitura ao botão "Voltar"
        for (int by = 0; by < 60; by++) {
            for (int bx = 0; bx < 250; bx++) {
                int pxX = 1650 + bx; int pyY = 980 + by;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xAA000000;
            }
        }
        // Mostra qual botão aperta para fechar a foto
        desenharTexto(p, "[O] Voltar", 30, 1670, 1020, 0xFFFFFFFF);

        // Retorna aqui para IMPEDIR que o menu clássico seja desenhado por cima da foto
        return;
    }

    // 1. DESENHAR LISTA DE ITENS (Tudo exceto Notepad)
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
        // Folha branca
        for (int by = 0; by < 700; by++) {
            for (int bx = 0; bx < 1400; bx++) {
                int pxX = 260 + bx; int pyY = 150 + by;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xFFEEEEEE;
            }
        }
        // Barra vermelha
        for (int by = 0; by < 60; by++) {
            for (int bx = 0; bx < 1400; bx++) {
                int pxX = 260 + bx; int pyY = 150 + by;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xFFD05050;
            }
        }
        desenharTexto(p, "BLOCO DE NOTAS", 40, 280, 160, 0xFFFFFFFF);
        desenharTexto(p, "[X] Escrever   [O] Voltar", 30, 1200, 160, 0xFFFFFFFF);
        desenharTexto(p, bufferTecladoC, 40, 280, 260, 0xFF000000);
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
    if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
        for (int my = 0; my < 550; my++) {
            for (int mx = 0; mx < 350; mx++) {
                int pxX = listX + 600 + mx; int pyY = listY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xEE111111;
            }
        }
        for (int i = 0; i < 11; i++) {
            uint32_t corOp = (i == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
            desenharTexto(p, listaOpcoesAudio[i], 30, listX + 620, listY + 50 + (i * 45), corOp);
        }
    }

    // 6. DESENHAR MENSAGEM DE STATUS
    if (msgTimer > 0) {
        desenharTexto(p, msgStatus, 40, 100, 950, 0xFFFFFFFF);
        msgTimer--;
    }
}
// --- FIM DO ARQUIVO menu_grafico.cpp ---