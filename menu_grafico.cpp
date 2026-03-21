#include "menu_grafico.h"
#include "menu.h"
#include "graphics.h"
#include "bloco_de_notas.h" 
#include "menu_audio.h" 
#include "menu_upload.h" 
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

// --- VARIÁVEIS DO PAINEL DUPLO ---
extern bool painelDuplo;
extern int painelAtivo;
extern char nomesEsq[3000][64];
extern bool marcadosEsq[3000];
extern char pathExplorarEsq[256];
extern int selEsq;
extern int totalItensEsq;
extern MenuLevel menuAtualEsq;
extern int offEsq;


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

        int paineisDesenhar = painelDuplo ? 2 : 1;

        for (int painelIndex = 0; painelIndex < paineisDesenhar; painelIndex++) {
            int refPainel = painelDuplo ? painelIndex : 1; // 0 = Esq, 1 = Dir

            int sAtual = (refPainel == 0) ? selEsq : sel;
            int oAtual = (refPainel == 0) ? offEsq : off;
            int tItens = (refPainel == 0) ? totalItensEsq : totalItens;
            char (*nItems)[64] = (refPainel == 0) ? nomesEsq : nomes;
            bool* mItems = (refPainel == 0) ? marcadosEsq : marcados;
            MenuLevel mAtual = (refPainel == 0) ? menuAtualEsq : menuAtual;

            // --- GARANTIA DE PREENCHIMENTO DO MENU HOME ---
            if (mAtual == MENU_EXPLORAR_HOME && tItens <= 0) {
                strcpy(nItems[0], "Hyper Neiva");
                strcpy(nItems[1], "Raiz");
                strcpy(nItems[2], "USB 0");
                strcpy(nItems[3], "USB 1");
                tItens = 4;
                if (refPainel == 0) totalItensEsq = 4; else totalItens = 4;
            }

            // --- CÁLCULO DAS POSIÇÕES CORRETAS ---
            int posX;
            int larguraItem;

            if (painelDuplo) {
                if (refPainel == 0) { // PAINEL ESQUERDO (Abre onde era a capinha)
                    posX = capaX; // Vai para a esquerda
                    larguraItem = listX - capaX - 40; // Estica até 40 pixels antes de encostar na lista principal
                    if (larguraItem < 100) larguraItem = 100; // Segurança mínima
                }
                else { // PAINEL DIREITO (O menu original)
                    posX = listX; // Fica INTACTO na posição original!
                    larguraItem = listW; // Fica com a largura original
                }
            }
            else { // MODO NORMAL (TELA ÚNICA)
                posX = listX;
                larguraItem = listW;
            }

            for (int i = 0; i < 6; i++) {
                int gIdx = i + oAtual; if (gIdx >= tItens) break;

                int yP = listY + (i * 120);

                // Painel inativo fica mais escuro
                bool isPainelAtivo = (!painelDuplo || painelAtivo == refPainel);
                uint32_t corFundo = isPainelAtivo ? 0xAA222222 : 0xAA111111;
                uint32_t corTexto = isPainelAtivo ? 0xFFFFFFFF : 0xFFAAAAAA;

                bool isMarcado = (mAtual == MENU_EXPLORAR || mAtual == MENU_BAIXAR_DROPBOX_LISTA || mAtual == MENU_BAIXAR_DROPBOX_UPLOAD) && mItems[gIdx];

                if (isMarcado) {
                    corFundo = isPainelAtivo ? 0xAAFFFF99 : 0xAA999933; // Amarelo marcado
                }

                if (gIdx == sAtual) {
                    if (isPainelAtivo) {
                        if (isMarcado) corFundo = 0xFF00FF00; // Verde
                        else corFundo = 0xFF00AAFF; // Azul Normal
                        corTexto = 0xFF000000;
                    }
                    else {
                        // Cursor no painel inativo
                        corFundo = 0xAA555555;
                    }
                }

                for (int by = 0; by < listH; by++) for (int bx = 0; bx < larguraItem; bx++) {
                    int pxX = posX + bx; int pyY = yP + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = corFundo;
                }
                desenharTexto(p, nItems[gIdx], 35, posX + 20, yP + 20, corTexto);
            }
        }
    }

    // 2. DESENHAR O BLOCO DE NOTAS (NOTEPAD)
    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) {
        renderizarNotepad(p);
    }

    // 3. DESENHAR AS IMAGENS E BREADCRUMBS
    if (menuAtual == JOGAR_XML || editMode) {
        int idx = sel % 6;
        if (capasAssets[idx]) desenharRedimensionado(p, capasAssets[idx], wC[idx], hC[idx], capaW, capaH, capaX, capaY);
        if (discosAssets[idx]) desenharDiscoRedondo(p, discosAssets[idx], wD[idx], hD[idx], discoW, discoH, discoX, discoY);
    }
    else if (menuAtual == SCRAPER_LIST && imgPreview) {
        desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
    }
    else if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
        if (!painelDuplo) {
            char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar);
            desenharTexto(p, bread, 30, listX, 1020, 0xFFFFFFFF);
        }
        else {
            // Ajustamos a posição do texto com os caminhos para bater com a nova lógica
            char breadEsq[300]; sprintf(breadEsq, "ESQ: %s", pathExplorarEsq);
            desenharTexto(p, breadEsq, 25, capaX, 1020, (painelAtivo == 0) ? 0xFF00AAFF : 0xFFAAAAAA); // Debaixo do esquerdo

            char breadDir[300]; sprintf(breadDir, "DIR: %s", pathExplorar);
            desenharTexto(p, breadDir, 25, listX, 1020, (painelAtivo == 1) ? 0xFF00AAFF : 0xFFAAAAAA); // Debaixo do principal
        }
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

    // 6. DESENHAR MENU SUSPENSO (OPÇÕES DE UPLOAD) 
    desenharMenuUpload(p);

    if (msgTimer > 0) {
        desenharTexto(p, msgStatus, 40, 100, 950, 0xFFFFFFFF);
        msgTimer--;
    }
}