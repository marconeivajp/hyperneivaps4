#include "menu_grafico.h"
#include "menu.h"
#include "graphics.h"
#include "bloco_de_notas.h" 
#include "menu_audio.h" 
#include "menu_upload.h" 
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <dirent.h>
#include "stb_image.h"

extern bool editMode;
extern bool showOpcoes;
extern int selOpcao;
extern char pathExplorar[256];
extern bool marcados[3000];
extern const char* listaOpcoes[10];

extern char bufferTecladoC[128];
extern unsigned char* imgPreview;

// VARIÁVEIS DE ASSETS PADRÃO (Vindos do main.cpp)
extern unsigned char* defaultArtwork1;
extern unsigned char* defaultArtwork2;
extern int wDef1, hDef1;
extern int wDef2, hDef2;

extern int listX, listY, listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;
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


// =========================================================================
// FUNÇÃO QUE IGNORA MAIÚSCULAS/MINÚSCULAS APENAS PARA O NOME DO ARQUIVO/EXTENSÃO
// =========================================================================
unsigned char* carregarMediaCaseInsensitive(const char* pastaPath, const char* nomeProcurado, int* w, int* h, int* c) {
    DIR* d = opendir(pastaPath);
    if (!d) return NULL;

    struct dirent* dir;
    char caminhoEncontrado[1024] = "";
    bool achou = false;

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        const char* dot = strrchr(dir->d_name, '.');
        if (dot) {
            int lenNome = dot - dir->d_name;
            char nomeBase[256];
            if (lenNome >= sizeof(nomeBase)) lenNome = sizeof(nomeBase) - 1;
            strncpy(nomeBase, dir->d_name, lenNome);
            nomeBase[lenNome] = '\0';

            if (strcasecmp(nomeBase, nomeProcurado) == 0) {
                if (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) {
                    snprintf(caminhoEncontrado, sizeof(caminhoEncontrado), "%s/%s", pastaPath, dir->d_name);
                    achou = true;
                    break;
                }
            }
        }
    }
    closedir(d);

    if (achou) {
        return stbi_load(caminhoEncontrado, w, h, c, 4);
    }
    return NULL;
}
// =========================================================================


void desenharInterface(uint32_t* p) {

    // VARIÁVEIS DE MÍDIA DINÂMICA
    static char nomeItemAnterior[128] = "";
    static unsigned char* imgBgDinamico = NULL;
    static int dynBgW = 0, dynBgH = 0, dynBgC = 0;
    static unsigned char* imgCapaDinamica = NULL;
    static int dynCapaW = 0, dynCapaH = 0, dynCapaC = 0;
    static unsigned char* imgDiscoDinamico = NULL;
    static int dynDiscoW = 0, dynDiscoH = 0, dynDiscoC = 0;

    // Se mudou de item, atualiza as 3 imagens da memória!
    if (strcmp(nomeItemAnterior, nomes[sel]) != 0) {
        strcpy(nomeItemAnterior, nomes[sel]);

        if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
        if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
        if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }

        if (strlen(nomeItemAnterior) > 0) {
            imgBgDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Background", nomeItemAnterior, &dynBgW, &dynBgH, &dynBgC);
            imgCapaDinamica = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork1", nomeItemAnterior, &dynCapaW, &dynCapaH, &dynCapaC);
            imgDiscoDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork2", nomeItemAnterior, &dynDiscoW, &dynDiscoH, &dynDiscoC);
        }
    }

    // 0.0 DESENHAR BACKGROUND DINÂMICO SOBRE O PADRÃO (Se existir)
    if (menuAtual != MENU_NOTEPAD && imgBgDinamico) {
        // Redesenha um fundo fullscreen por cima do original
        desenharRedimensionado(p, imgBgDinamico, dynBgW, dynBgH, 1920, 1080, 0, 0);
    }

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
            int refPainel = painelDuplo ? painelIndex : 1;

            int sAtual = (refPainel == 0) ? selEsq : sel;
            int oAtual = (refPainel == 0) ? offEsq : off;
            int tItens = (refPainel == 0) ? totalItensEsq : totalItens;
            char (*nItems)[64] = (refPainel == 0) ? nomesEsq : nomes;
            bool* mItems = (refPainel == 0) ? marcadosEsq : marcados;
            MenuLevel mAtual = (refPainel == 0) ? menuAtualEsq : menuAtual;

            if (mAtual == MENU_EXPLORAR_HOME && tItens <= 0) {
                strcpy(nItems[0], "Hyper Neiva");
                strcpy(nItems[1], "Raiz");
                strcpy(nItems[2], "USB 0");
                strcpy(nItems[3], "USB 1");
                tItens = 4;
                if (refPainel == 0) totalItensEsq = 4; else totalItens = 4;
            }

            int larguraBarraDupla = 750;
            int posX;
            int larguraItem;

            if (painelDuplo) {
                if (refPainel == 0) {
                    posX = capaX;
                    larguraItem = larguraBarraDupla;
                }
                else {
                    posX = listX;
                    larguraItem = larguraBarraDupla;
                }
            }
            else {
                posX = listX;
                larguraItem = listW;
            }

            for (int i = 0; i < 6; i++) {
                int gIdx = i + oAtual; if (gIdx >= tItens) break;

                int yP = listY + (i * 120);

                bool isPainelAtivo = (!painelDuplo || painelAtivo == refPainel);
                uint32_t corFundo = isPainelAtivo ? 0xAA222222 : 0xAA111111;
                uint32_t corTexto = isPainelAtivo ? 0xFFFFFFFF : 0xFFAAAAAA;

                bool isMarcado = (mAtual == MENU_EXPLORAR || mAtual == MENU_BAIXAR_DROPBOX_LISTA || mAtual == MENU_BAIXAR_DROPBOX_UPLOAD) && mItems[gIdx];

                if (isMarcado) {
                    corFundo = isPainelAtivo ? 0xAAFFFF99 : 0xAA999933;
                }

                if (gIdx == sAtual) {
                    if (isPainelAtivo) {
                        if (isMarcado) corFundo = 0xFF00FF00;
                        else corFundo = 0xFF00AAFF;
                        corTexto = 0xFF000000;
                    }
                    else {
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

    // 3. BREADCRUMBS E IMAGENS (Artworks)
    if (menuAtual == SCRAPER_LIST && imgPreview) {
        desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
    }
    else if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
        if (!painelDuplo) {
            char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar);
            desenharTexto(p, bread, 30, listX, 1020, 0xFFFFFFFF);
        }
        else {
            char breadEsq[300]; sprintf(breadEsq, "ESQ: %s", pathExplorarEsq);
            desenharTexto(p, breadEsq, 25, capaX, 1020, (painelAtivo == 0) ? 0xFF00AAFF : 0xFFAAAAAA);

            char breadDir[300]; sprintf(breadDir, "DIR: %s", pathExplorar);
            desenharTexto(p, breadDir, 25, listX, 1020, (painelAtivo == 1) ? 0xFF00AAFF : 0xFFAAAAAA);
        }
    }
    else {
        // EXIBE A CAPINHA (Dinâmica ou Default 1)
        if (imgCapaDinamica) {
            desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY);
        }
        else if (menuAtual == JOGAR_XML || editMode) {
            if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY);
        }

        // EXIBE O DISCO (Dinâmico ou Default 2)
        if (imgDiscoDinamico) {
            desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY);
        }
        else if (menuAtual == JOGAR_XML || editMode) {
            if (defaultArtwork2) desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY);
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