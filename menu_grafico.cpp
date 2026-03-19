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

    // 0. DESENHAR IMAGEM EM TELA CHEIA SE ESTIVER VISUALIZANDO
    if (visualizandoMidiaImagem && imgMidia) {
        // Estica/desenha a imagem ocupando a tela inteira (1920x1080)
        desenharRedimensionado(p, imgMidia, wM, hM, 1920, 1080, 0, 0);

        // Fundo semitransparente escuro no cantinho para dar leitura ao botão "Voltar"
        for (int by = 0; by < 60; by++) {
            for (int bx = 0; bx < 250; bx++) {
                int px = 1650 + bx; int py = 980 + by;
                if (px < 1920 && py < 1080) p[py * 1920 + px] = 0xAA000000;
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
                int px = listX + bx; int py = yP + by; if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = corFundo;
            }
            desenharTexto(p, nomes[gIdx], 35, listX + 20, yP + 20, corTexto);
        }
    }

    // 2. DESENHAR O BLOCO DE NOTAS (NOTEPAD)
    if (menuAtual == MENU_NOTEPAD) {
        // Folha branca
        for (int by = 0; by < 700; by++) {
            for (int bx = 0; bx < 1400; bx++) {
                int px = 260 + bx; int py = 150 + by;
                if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = 0xFFEEEEEE;
            }
        }
        // Barra vermelha
        for (int by = 0; by < 60; by++) {
            for (int bx = 0; bx < 1400; bx++) {
                int px = 260 + bx; int py = 150 + by;
                if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = 0xFFD05050;
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
            int px = discoX + mx; int py = discoY - 100 + my; if (px < 1920 && py < 1080 && py >= 0) p[py * 1920 + px] = 0xEE111111;
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
                int px = listX + 600 + mx; int py = listY + my;
                if (px < 1920 && py < 1080 && py >= 0) p[py * 1920 + px] = 0xEE111111;
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