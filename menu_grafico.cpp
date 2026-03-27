#include "menu_grafico.h"
#include "menu.h"
#include "graphics.h"
#include "bloco_de_notas.h" 
#include "menu_audio.h" 
#include "menu_upload.h" 
#include "baixar.h" 
#include "elementos.h" 
#include "controle_elementos.h" 
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <math.h> 
#include "stb_image.h"
#include "elementos_animados_sprite_sheet.h"

extern bool editMode; extern int editTarget; extern int editType; extern bool showOpcoes; extern int selOpcao; extern char pathExplorar[256]; extern bool marcados[3000]; extern const char* listaOpcoes[150]; extern char bufferTecladoC[128]; extern unsigned char* imgPreview;
extern unsigned char* defaultArtwork1; extern unsigned char* defaultArtwork2; extern int wDef1, hDef1; extern int wDef2, hDef2;

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH;
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark, backX, backY, backW, backH; extern int wP, hP;
extern int fontAlign, fontScroll;

extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;

extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;

// Importa as novas variáveis de estilo do editar.cpp
extern int listStyle;
extern int fontAnim;
extern int listCurvature;
extern int listZoomCentro;

int offOpcao = 0;
int frameContadorGlobal = 0;

extern bool visualizandoMidiaImagem; extern unsigned char* imgMidia; extern int wM, hM; extern float zoomMidia; extern bool fullscreenMidia;
extern bool visualizandoMidiaTexto; extern char* textoMidiaBuffer; extern char* linhasTexto[5000]; extern int totalLinhasTexto; extern int textoMidiaScroll;
extern bool painelDuplo; extern int painelAtivo; extern char nomesEsq[3000][64]; extern bool marcadosEsq[3000]; extern char pathExplorarEsq[256]; extern int selEsq; extern int totalItensEsq; extern MenuLevel menuAtualEsq; extern int offEsq;

extern volatile bool downloadEmSegundoPlano;
extern volatile float progressoAtualDownload;
extern char msgDownloadBg[256];
extern volatile int totalFilaSessao;
extern volatile int baixadosFilaSessao;

extern int totalOpcoes;
extern char ipDoPS4[64];

extern stbtt_fontinfo font;
extern int temF;

uint32_t getSysColor(int index) {
    uint32_t sysColors[] = { 0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000, 0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933, 0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF };
    if (index < 0 || index > 14) return sysColors[0];
    return sysColors[index];
}

int medirLarguraTexto(const char* texto, int tam) {
    if (!temF || !texto) return 0;
    float s = stbtt_ScaleForPixelHeight(&font, (float)tam);
    int totalW = 0;
    for (int i = 0; texto[i]; ++i) {
        int adv, lsb; stbtt_GetCodepointHMetrics(&font, texto[i], &adv, &lsb);
        totalW += (int)(adv * s);
    } return totalW;
}

void desenharTextoClip(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor, int clipX, int clipW) {
    if (!temF || !texto || !pixels) return;
    float s = stbtt_ScaleForPixelHeight(&font, (float)tam);
    int asc; stbtt_GetFontVMetrics(&font, &asc, 0, 0); asc = (int)(asc * s);
    int curX = x;

    uint8_t baseAlpha = (cor >> 24) & 0xFF;
    if (baseAlpha < 10) return;

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
                if (pX >= clipX && pX < (clipX + clipW) && pY >= 0 && pY < 1080 && pX >= 0 && pX < 1920) {
                    uint8_t alpha = b[cy * w + cx];
                    uint8_t finalAlpha = (alpha * baseAlpha) / 255;
                    if (finalAlpha > 30) pixels[pY * 1920 + pX] = (finalAlpha << 24) | (cor & 0x00FFFFFF);
                }
            } free(b);
        } curX += (int)(adv * s);
    }
}

void desenharTextoAlinhadoAnimado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor, bool isSelected) {
    int espacoDisponivel = maxW - 40;
    int pxWidth = medirLarguraTexto(textoOriginal, fTam);
    int posX = xBase + 20;

    if (pxWidth <= espacoDisponivel) {
        if (fontAlign == 1) posX = xBase + (maxW / 2) - (pxWidth / 2);
        else if (fontAlign == 2) posX = xBase + maxW - pxWidth - 20;
    }

    if (pxWidth > espacoDisponivel) {
        if (fontScroll == 1) {
            if (isSelected) {
                int excessPx = pxWidth - espacoDisponivel;
                int cycle = 120;
                int pos = frameContadorGlobal % cycle;
                int pixelShift = 0;
                if (pos >= 60) {
                    float progress = (float)(pos - 60) / 60.0f;
                    pixelShift = (int)(progress * excessPx);
                }
                desenharTextoClip(p, textoOriginal, fTam, posX - pixelShift, y, cor, xBase + 20, espacoDisponivel);
            }
            else desenharTextoClip(p, textoOriginal, fTam, posX, y, cor, xBase + 20, espacoDisponivel);
        }
        else {
            int maxChars = espacoDisponivel / (fTam * 0.55f);
            if (maxChars < 4) maxChars = 4; char txtFinal[512];
            strncpy(txtFinal, textoOriginal, maxChars - 3); txtFinal[maxChars - 3] = '\0'; strcat(txtFinal, "...");
            desenharTexto(p, txtFinal, fTam, posX, y, cor);
        }
    }
    else { desenharTextoClip(p, textoOriginal, fTam, posX, y, cor, xBase, maxW); }
}

void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor) {
    desenharTextoAlinhadoAnimado(p, textoOriginal, fTam, xBase, y, maxW, cor, false);
}

unsigned char* carregarMediaCaseInsensitive(const char* pastaPath, const char* nomeProcurado, int* w, int* h, int* c) {
    DIR* d = opendir(pastaPath); if (!d) return NULL; struct dirent* dir; char caminhoEncontrado[1024] = ""; bool achou = false;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue; const char* dot = strrchr(dir->d_name, '.');
        if (dot) {
            int lenNome = dot - dir->d_name; char nomeBase[256]; if (lenNome >= sizeof(nomeBase)) lenNome = sizeof(nomeBase) - 1; strncpy(nomeBase, dir->d_name, lenNome); nomeBase[lenNome] = '\0';
            if (strcasecmp(nomeBase, nomeProcurado) == 0) { if (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) { snprintf(caminhoEncontrado, sizeof(caminhoEncontrado), "%s/%s", pastaPath, dir->d_name); achou = true; break; } }
        }
    } closedir(d); if (achou) return stbi_load(caminhoEncontrado, w, h, c, 4); return NULL;
}

void desenharInterface(uint32_t* p) {
    frameContadorGlobal++;

    static char nomeItemAnterior[128] = ""; static unsigned char* imgBgDinamico = NULL; static int dynBgW = 0, dynBgH = 0, dynBgC = 0; static unsigned char* imgCapaDinamica = NULL; static int dynCapaW = 0, dynCapaH = 0, dynCapaC = 0; static unsigned char* imgDiscoDinamico = NULL; static int dynDiscoW = 0, dynDiscoH = 0, dynDiscoC = 0;

    if (menuAtual == SCRAPER_LIST) {
        if (strcmp(nomes[sel], ultimoJogoCarregado) != 0) { char cp[512]; snprintf(cp, sizeof(cp), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]); FILE* fEx = fopen(cp, "rb"); if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); } else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } } strncpy(ultimoJogoCarregado, nomes[sel], 63); ultimoJogoCarregado[63] = '\0'; }
    }
    else if (strcmp(nomeItemAnterior, nomes[sel]) != 0) {
        strcpy(nomeItemAnterior, nomes[sel]); if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; } if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; } if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
        if (strlen(nomeItemAnterior) > 0) { imgBgDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Background", nomeItemAnterior, &dynBgW, &dynBgH, &dynBgC); imgCapaDinamica = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork1", nomeItemAnterior, &dynCapaW, &dynCapaH, &dynCapaC); imgDiscoDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork2", nomeItemAnterior, &dynDiscoW, &dynDiscoH, &dynDiscoC); }
    }

    if (menuAtual != MENU_NOTEPAD && imgBgDinamico) { desenharRedimensionado(p, imgBgDinamico, dynBgW, dynBgH, 1920, 1080, 0, 0); }
    if (menuAtual != MENU_NOTEPAD && !visualizandoMidiaImagem && !visualizandoMidiaTexto) { desenharElementoAnimado(p); }

    if (visualizandoMidiaTexto && textoMidiaBuffer) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515; for (int by = 0; by < 80; by++) for (int bx = 0; bx < 1920; bx++) p[by * 1920 + bx] = 0xFF303030; desenharTexto(p, "LEITOR DE ARQUIVOS", 35, 50, 25, 0xFF00AAFF);
        int maxLinhasVisiveis = 23; for (int i = 0; i < maxLinhasVisiveis; i++) { int indiceDaLinha = textoMidiaScroll + i; if (indiceDaLinha < totalLinhasTexto && linhasTexto[indiceDaLinha] != NULL) { desenharTexto(p, linhasTexto[indiceDaLinha], 30, 50, 120 + (i * 40), 0xFFDDDDDD); } }
        for (int by = 0; by < 60; by++) for (int bx = 0; bx < 1920; bx++) p[(1020 + by) * 1920 + bx] = 0xFF222222; char rodape[128]; sprintf(rodape, "[Setas] Rolar   |   [O] Voltar   |   Linha: %d / %d", textoMidiaScroll, totalLinhasTexto); desenharTexto(p, rodape, 25, 50, 1035, 0xFF00AAFF); return;
    }

    if (visualizandoMidiaImagem && imgMidia) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF000000; float propW = 1920.0f / wM; float propH = 1080.0f / hM; float propMax = (propW < propH) ? propW : propH; int drawW, drawH;
        if (fullscreenMidia) { drawW = (int)(wM * propMax); drawH = (int)(hM * propMax); }
        else { drawW = (int)(wM * zoomMidia); drawH = (int)(hM * zoomMidia); if (drawW > 1920 || drawH > 1080) { drawW = (int)(wM * propMax); drawH = (int)(hM * propMax); zoomMidia = propMax; } }
        int posX = (1920 - drawW) / 2; int posY = (1080 - drawH) / 2; desenharRedimensionado(p, imgMidia, wM, hM, drawW, drawH, posX, posY);
        for (int by = 0; by < 130; by++) { for (int bx = 0; bx < 400; bx++) { int pxX = 1480 + bx; int pyY = 930 + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xAA000000; } }
        desenharTexto(p, "[X] Tela Cheia", 25, 1500, 960, 0xFFFFFFFF); desenharTexto(p, "[Cima/Baixo] Zoom", 25, 1500, 1000, 0xFFFFFFFF); desenharTexto(p, "[O] Voltar", 25, 1500, 1040, 0xFFFFFFFF); return;
    }

    if (menuAtual != MENU_NOTEPAD) {
        int paineisDesenhar = painelDuplo ? 2 : 1; uint32_t corBasePainel = getSysColor(listBg);
        int curListX = (listOri == 0) ? listXV : listXH; int curListY = (listOri == 0) ? listYV : listYH; int curListSpc = (listOri == 0) ? listSpcV : listSpcH;

        for (int painelIndex = 0; painelIndex < paineisDesenhar; painelIndex++) {
            int refPainel = painelDuplo ? painelIndex : 1; int sAtual = (refPainel == 0) ? selEsq : sel; int oAtual = (refPainel == 0) ? offEsq : off;
            int tItens = (refPainel == 0) ? totalItensEsq : totalItens; char (*nItems)[64] = (refPainel == 0) ? nomesEsq : nomes; bool* mItems = (refPainel == 0) ? marcadosEsq : marcados; MenuLevel mAtual = (refPainel == 0) ? menuAtualEsq : menuAtual;

            if (mAtual == MENU_EXPLORAR_HOME && tItens <= 0) { strcpy(nItems[0], "Hyper Neiva"); strcpy(nItems[1], "Raiz"); strcpy(nItems[2], "USB 0"); strcpy(nItems[3], "USB 1"); tItens = 4; if (refPainel == 0) totalItensEsq = 4; else totalItens = 4; }

            int larguraBarraDupla = 750; int posX_Base = (painelDuplo) ? ((refPainel == 0) ? capaX : curListX) : curListX; int larguraItem = (painelDuplo) ? larguraBarraDupla : listW;
            int stepX = (listOri == 1) ? curListSpc : 0; int stepY = (listOri == 0) ? curListSpc : 0;

            static float smoothSelDir = 0.0f; static float smoothSelEsq = 0.0f; static MenuLevel lastMenu = ROOT;
            if (lastMenu != menuAtual) { smoothSelDir = sel; smoothSelEsq = selEsq; lastMenu = menuAtual; }
            float& smoothSel = (refPainel == 0) ? smoothSelEsq : smoothSelDir;

            if (fabs(smoothSel - sAtual) > 20.0f) smoothSel = sAtual;
            smoothSel += (sAtual - smoothSel) * 0.15f;

            int loopStart = 0; int loopEnd = 6;
            if (listStyle == 1) { loopStart = (int)smoothSel - 4; loopEnd = (int)smoothSel + 4; }

            for (int i = loopStart; i <= loopEnd; i++) {
                int gIdx = (listStyle == 1) ? i : (i + oAtual);

                if (gIdx < 0 || gIdx >= tItens) {
                    if (listStyle == 1) continue; else break;
                }

                int currentX = posX_Base; int currentY = curListY;
                int animOffsetX = 0; int animOffsetY = 0;
                int currentFontTam = fontTam;
                float opacityMulti = 1.0f;

                if (listStyle == 1) {
                    float dist = gIdx - smoothSel;

                    int centroX = posX_Base;
                    int centroY = curListY + (2 * stepY);
                    if (listOri == 1) centroX = posX_Base + (2 * stepX);

                    currentX = centroX + (int)(dist * stepX);
                    currentY = centroY + (int)(dist * stepY);

                    // APLICA O MULTIPLICADOR DE CURVATURA AQUI
                    int curva = (int)(dist * dist * (float)listCurvature);
                    if (listOri == 0) animOffsetX = curva;
                    else animOffsetY = curva;

                    float absDist = fabs(dist);
                    if (absDist < 1.0f && (!painelDuplo || painelAtivo == refPainel)) {
                        // APLICA O ZOOM CUSTOMIZADO AQUI
                        currentFontTam += (int)((1.0f - absDist) * (float)listZoomCentro);
                    }

                    if (absDist > 2.0f) {
                        opacityMulti = 1.0f - ((absDist - 2.0f) / 1.5f);
                        if (opacityMulti < 0.0f) opacityMulti = 0.0f;
                    }
                }
                else {
                    currentX = posX_Base + (i * stepX);
                    currentY = curListY + (i * stepY);

                    if (listStyle == 2 && gIdx == sAtual && (!painelDuplo || painelAtivo == refPainel)) {
                        if (listOri == 0) animOffsetX = 30; else animOffsetY = 30;
                        currentFontTam += (listZoomCentro / 2); // Metade do zoom para o estilo PS4
                    }
                    else if (listStyle == 3) {
                        if (gIdx == sAtual && (!painelDuplo || painelAtivo == refPainel)) currentFontTam += (listZoomCentro / 2); // Metade do zoom para PS3
                        else currentFontTam -= 6;
                    }
                }

                bool isPainelAtivo = (!painelDuplo || painelAtivo == refPainel);
                uint32_t corFundo = isPainelAtivo ? corBasePainel : 0xAA111111;
                uint32_t corTexto = isPainelAtivo ? 0xFFFFFFFF : 0xFFAAAAAA;

                bool isMarcado = (mAtual == MENU_EXPLORAR || mAtual == MENU_BAIXAR_DROPBOX_LISTA || mAtual == MENU_BAIXAR_DROPBOX_UPLOAD) && mItems[gIdx];
                if (isMarcado) { corFundo = isPainelAtivo ? getSysColor(listMark) : getSysColor(11); }

                bool isSelected = isPainelAtivo && (gIdx == sAtual);

                if (isSelected) {
                    if (isMarcado) corFundo = getSysColor(listHoverMark); else corFundo = getSysColor(10);
                    corTexto = 0xFF000000;

                    if (fontAnim == 1) {
                        currentFontTam += (int)(sin(frameContadorGlobal * 0.1f) * 6.0f);
                    }
                    else if (fontAnim == 2) {
                        uint8_t r = (uint8_t)((sin(frameContadorGlobal * 0.05f) + 1.0f) * 127.5f);
                        uint8_t g = (uint8_t)((sin(frameContadorGlobal * 0.05f + 2.0f) + 1.0f) * 127.5f);
                        uint8_t b = (uint8_t)((sin(frameContadorGlobal * 0.05f + 4.0f) + 1.0f) * 127.5f);
                        corTexto = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                    else if (fontAnim == 3) {
                        animOffsetY += (int)(sin(frameContadorGlobal * 0.2f) * 8.0f);
                    }
                }
                else if (!isPainelAtivo) {
                    corFundo = 0xAA555555;
                }

                uint8_t aF = (uint8_t)(((corFundo >> 24) & 0xFF) * opacityMulti);
                corFundo = (aF << 24) | (corFundo & 0x00FFFFFF);
                uint8_t aT = (uint8_t)(((corTexto >> 24) & 0xFF) * opacityMulti);
                corTexto = (aT << 24) | (corTexto & 0x00FFFFFF);

                int drawX = currentX + animOffsetX;
                int drawY = currentY + animOffsetY;

                if (aF > 10) {
                    for (int by = 0; by < listH; by++) {
                        for (int bx = 0; bx < larguraItem; bx++) {
                            int pxX = drawX + bx; int pyY = drawY + by;
                            if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = corFundo;
                        }
                    }
                }

                desenharTextoAlinhadoAnimado(p, nItems[gIdx], currentFontTam, drawX, drawY + (listH / 4), larguraItem, corTexto, isSelected);
            }
        }
    }

    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) renderizarNotepad(p);

    if (menuAtual == SCRAPER_LIST && imgPreview) { desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY); }
    else if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
        if (!painelDuplo) { char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, bread, 30, cX, 1020, 0xFFFFFFFF); }
        else { char breadEsq[300]; sprintf(breadEsq, "ESQ: %s", pathExplorarEsq); desenharTexto(p, breadEsq, 25, capaX, 1020, (painelAtivo == 0) ? 0xFF00AAFF : 0xFFAAAAAA); char breadDir[300]; sprintf(breadDir, "DIR: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, breadDir, 25, cX, 1020, (painelAtivo == 1) ? 0xFF00AAFF : 0xFFAAAAAA); }
    }
    else {
        bool isEditingBar = ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 4);
        bool isEditingAudio = ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 5);
        bool isEditingUp = ((menuAtual == MENU_EDIT_TARGET || editMode) && (editTarget == 6 || editTarget == 9));

        if (isEditingBar) {
            int bX = barX; int bY = barY; int bW = barW; int bH = barH;
            for (int y = bY; y < bY + bH; y++) { for (int x = bX; x < bX + bW; x++) { if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barBg); } }
            int fill = bW / 2; for (int y = bY; y < bY + bH; y++) { for (int x = bX; x < bX + fill; x++) { if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barFill); } }
            desenharTexto(p, "50%   -   1 / 1", 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
        }
        else if (isEditingAudio) {
            for (int my = 0; my < audioH; my++) { for (int mx = 0; mx < audioW; mx++) { int pxX = audioX + mx; int pyY = audioY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(listBg); } }
            int maxV = (audioH - 50) / 45; if (maxV < 1) maxV = 1;
            if (maxV > 0) desenharTextoAlinhado(p, "PLAY / PAUSE", fontTam, audioX, audioY + 50, audioW, 0xFFFFFF00);
            if (maxV > 1) desenharTextoAlinhado(p, "PARAR", fontTam, audioX, audioY + 95, audioW, 0xFFFFFFFF);
        }
        else if (isEditingUp) {
            for (int my = 0; my < upH; my++) { for (int mx = 0; mx < upW; mx++) { int pxX = upX + mx; int pyY = upY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(upBg); } }
            int maxV = (upH - 50) / 45; if (maxV < 1) maxV = 1;

            if (editTarget == 9) {
                if (maxV > 0) desenharTextoAlinhado(p, "Copiar", fontTam, upX, upY + 50, upW, getSysColor(upTextSel));
                if (maxV > 1) desenharTextoAlinhado(p, "Colar", fontTam, upX, upY + 95, upW, getSysColor(upTextNorm));
                if (maxV > 2) desenharTextoAlinhado(p, "Deletar", fontTam, upX, upY + 140, upW, getSysColor(upTextNorm));
                if (maxV > 3) desenharTextoAlinhado(p, "Renomear", fontTam, upX, upY + 185, upW, getSysColor(upTextNorm));
                if (maxV > 4) desenharTextoAlinhado(p, "Nova Pasta", fontTam, upX, upY + 230, upW, getSysColor(upTextNorm));
            }
            else {
                if (maxV > 0) desenharTextoAlinhado(p, "Selecionar", fontTam, upX, upY + 50, upW, getSysColor(upTextSel));
                if (maxV > 1) desenharTextoAlinhado(p, "Selecionar Tudo", fontTam, upX, upY + 95, upW, getSysColor(upTextNorm));
            }
        }
        else {
            if (imgCapaDinamica) { desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY); }
            else if (menuAtual == JOGAR_XML || (editMode && editTarget != 4 && editTarget != 5 && editTarget != 6 && editTarget != 9 && editTarget != 10 && editTarget != 11 && editTarget != 12 && editTarget != 14)) { if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }
            if (imgDiscoDinamico) { desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); }
            else if (menuAtual == JOGAR_XML || (editMode && editTarget != 4 && editTarget != 5 && editTarget != 6 && editTarget != 9 && editTarget != 10 && editTarget != 11 && editTarget != 12 && editTarget != 14)) { if (defaultArtwork2) desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
        }
    }

    if (editMode) {
        char txtPos[200]; int* tX, * tY, * tW, * tH;
        if (editTarget == 0) { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
        else if (editTarget == 1) { tX = &capaX; tY = &capaY; tW = &capaW; tH = &capaH; }
        else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
        else if (editTarget == 3) { tX = &backX; tY = &backY; tW = &backW; tH = &backH; }
        else if (editTarget == 4) { tX = &barX; tY = &barY; tW = &barW; tH = &barH; }
        else if (editTarget == 5) { tX = &audioX; tY = &audioY; tW = &audioW; tH = &audioH; }
        else if (editTarget == 6 || editTarget == 9) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
        else if (editTarget == 8) { tX = &msgX; tY = &msgY; tW = &msgTam; tH = &msgTam; }
        else if (editTarget == 10) { tX = &elem1X; tY = &elem1Y; tW = &elem1W; tH = &elem1H; }
        else if (editTarget == 11) { tX = &ctrl1X; tY = &ctrl1Y; tW = &ctrl1W; tH = &ctrl1H; }
        else if (editTarget == 12) { tX = &pont1X; tY = &pont1Y; tW = &pont1W; tH = &pont1H; }
        else if (editTarget == 14) { tX = &anim_posX; tY = &anim_posY; tW = &anim_colunas; tH = &anim_linhas; }
        else { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }

        if (editTarget == 9 && editType == 0) sprintf(txtPos, "MODO EDICAO - CORES DO EXPLORAR (USE SETAS ESQ/DIR)");
        else if (editType == 3) sprintf(txtPos, "MODO EDICAO - COR DE FUNDO (USE SETAS ESQ/DIR)");
        else if (editType == 8) sprintf(txtPos, "MODO EDICAO - COR PREENCHIMENTO (USE SETAS ESQ/DIR)");
        else if (editType == 4) sprintf(txtPos, "MODO EDICAO - ESPACAMENTO: %d", (listOri == 0) ? listSpcV : listSpcH);
        else if (editType == 5) sprintf(txtPos, "MODO EDICAO - ORIENTACAO: %s", listOri == 0 ? "VERTICAL" : "HORIZONTAL");
        else if (editType == 10) sprintf(txtPos, "MODO EDICAO - ALINHAMENTO: %s", fontAlign == 0 ? "ESQUERDA" : (fontAlign == 1 ? "CENTRO" : "DIREITA"));
        else if (editType == 11) sprintf(txtPos, "MODO EDICAO - LIMITES: %s", fontScroll == 0 ? "CORTAR (..)" : "ANIMACAO ROLAGEM");
        else if (editType == 12) { int stat = 0; if (editTarget == 10) stat = elem1On; else if (editTarget == 11) stat = ctrl1On; else if (editTarget == 12) stat = pont1On; sprintf(txtPos, "MODO EDICAO - LIGADO: %s (USE SETAS)", stat ? "SIM" : "NAO"); }
        else if (editType == 13) sprintf(txtPos, "MODO EDICAO - MODO PONTEIRO: %s (USE SETAS)", pont1Modo == 0 ? "ACOMPANHA" : "ESTATICO");
        else if (editType == 14) { const char* lds[] = { "ESQ", "DIR", "CIMA", "BAIXO" }; sprintf(txtPos, "MODO EDICAO - LADO PONTEIRO: %s (USE SETAS)", lds[pont1Lado]); }
        else if (editType == 15) sprintf(txtPos, "MODO EDICAO - EFEITOS SONOROS: %s (USE SETAS)", sfxLigado ? "LIGADO" : "DESLIGADO");
        else if (editType == 16) sprintf(txtPos, "MODO EDICAO - VOLUME EFEITOS: %d%% (USE SETAS)", sfxVolume);

        else if (editType == 45) sprintf(txtPos, "MODO EDICAO - ESTILO DA LISTA: %s (USE SETAS)", listStyle == 0 ? "NORMAL" : (listStyle == 1 ? "ROLETA HYPERSPIN" : (listStyle == 2 ? "PS4 FLOW" : "PS3 XMB")));
        else if (editType == 46) sprintf(txtPos, "MODO EDICAO - EFEITO DA FONTE: %s (USE SETAS)", fontAnim == 0 ? "NORMAL" : (fontAnim == 1 ? "PULSAR" : (fontAnim == 2 ? "ARCO-IRIS" : "ONDA")));
        else if (editType == 47) sprintf(txtPos, "MODO EDICAO - CURVATURA DA ROLETA: %d (USE SETAS)", listCurvature);
        else if (editType == 48) sprintf(txtPos, "MODO EDICAO - ZOOM DO CENTRO: %d (USE SETAS)", listZoomCentro);

        else if (editTarget == 11) sprintf(txtPos, "EDICAO CTRL - POSICAO ATUAL: X:%d Y:%d", ctrl1X, ctrl1Y);
        else if (editTarget == 7) sprintf(txtPos, "MODO EDICAO - TAMANHO DA FONTE: %d", fontTam);
        else if (editTarget == 8) sprintf(txtPos, "MODO EDICAO - NOTIFICACOES: X: %d  |  Y: %d  |  TAM: %d", msgX, msgY, msgTam);
        else sprintf(txtPos, "MODO EDICAO - X: %d  |  Y: %d  |  L: %d  |  A: %d", *tX, *tY, *tW, *tH);

        for (int by = 0; by < 40; by++) { for (int bx = 0; bx < 1920; bx++) { int pyY = 1040 + by; if (pyY < 1080) p[pyY * 1920 + bx] = 0xAA000000; } }
        desenharTexto(p, txtPos, 25, 50, 1045, 0xFF00FF00);
    }

    if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) {
        if (selOpcao >= totalOpcoes) selOpcao = 0;
        if (selOpcao < 0) selOpcao = totalOpcoes - 1;

        for (int my = 0; my < upH; my++) {
            for (int mx = 0; mx < upW; mx++) {
                int pxX = upX + mx;
                int pyY = upY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(upBg);
            }
        }

        int maxV = (upH - 50) / 45;
        if (maxV < 1) maxV = 1;

        if (selOpcao < offOpcao) offOpcao = selOpcao;
        if (selOpcao >= offOpcao + maxV) offOpcao = selOpcao - maxV + 1;

        for (int i = 0; i < maxV; i++) {
            int gIdx = i + offOpcao;
            if (gIdx >= totalOpcoes) break;

            bool isSelected = (gIdx == selOpcao);
            uint32_t corOp = isSelected ? getSysColor(upTextSel) : getSysColor(upTextNorm);
            desenharTextoAlinhadoAnimado(p, listaOpcoes[gIdx], fontTam, upX, upY + 50 + (i * 45), upW, corOp, isSelected);
        }
    }

    desenharMenuAudio(p); desenharMenuUpload(p);

    bool esconderElementos = (visualizandoMidiaImagem || visualizandoMidiaTexto || menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD);

    if (!esconderElementos) {
        int curListX = (listOri == 0) ? listXV : listXH;
        int curListY = (listOri == 0) ? listYV : listYH;
        int curListSpc = (listOri == 0) ? listSpcV : listSpcH;
        int stepX = (listOri == 1) ? curListSpc : 0;
        int stepY = (listOri == 0) ? curListSpc : 0;

        int refP = painelDuplo ? painelAtivo : 1;
        int sAt = (refP == 0) ? selEsq : sel;
        int oAt = (refP == 0) ? offEsq : off;
        int posX_B = (painelDuplo) ? ((refP == 0) ? capaX : curListX) : curListX;
        int wItem = (painelDuplo) ? 750 : listW;

        int selScreenX = 0; int selScreenY = 0;

        // O PONTEIRO ACOMPANHA AS NOVAS LISTAS (Ou fica fixo no Hyperspin)
        if (listStyle == 1) {
            selScreenX = posX_B;
            selScreenY = curListY + (2 * stepY);
            if (listOri == 1) selScreenX = posX_B + (2 * stepX);
        }
        else {
            int selAnimOffsetX = 0; int selAnimOffsetY = 0;
            if (listStyle == 2) {
                if (listOri == 0) selAnimOffsetX = 30; else selAnimOffsetY = 30;
            }
            if (fontAnim == 3) { selAnimOffsetY += (int)(sin(frameContadorGlobal * 0.2f) * 8.0f); }

            selScreenX = posX_B + ((sAt - oAt) * stepX) + selAnimOffsetX;
            selScreenY = curListY + ((sAt - oAt) * stepY) + selAnimOffsetY;
        }

        desenharElementos(p, selScreenX, selScreenY, wItem, listH);
    }

    if (msgTimer > 0) { desenharTexto(p, msgStatus, msgTam, msgX, msgY, 0xFFFFFFFF); msgTimer--; }
    else if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 8) { desenharTexto(p, "EXEMPLO DE NOTIFICACAO...", msgTam, msgX, msgY, 0xFF00FF00); }

    if (downloadEmSegundoPlano) {
        int bX = barX; int bY = barY; int bW = barW; int bH = barH;
        for (int y = bY; y < bY + bH; y++) { for (int x = bX; x < bX + bW; x++) { if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barBg); } }
        int fill = (int)(bW * progressoAtualDownload); if (fill > bW) fill = bW; if (fill < 0) fill = 0;
        for (int y = bY; y < bY + bH; y++) { for (int x = bX; x < bX + fill; x++) { if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barFill); } }
        char pctMsg[300]; sprintf(pctMsg, "%s", msgDownloadBg); desenharTexto(p, pctMsg, 25, bX, bY - 35, 0xFFFFFFFF);
        int porcentagem = (int)(progressoAtualDownload * 100.0f); if (porcentagem > 100) porcentagem = 100; if (porcentagem < 0) porcentagem = 0;
        char textoFila[128]; snprintf(textoFila, sizeof(textoFila), "%d%%   -   %d / %d", porcentagem, baixadosFilaSessao + 1, totalFilaSessao);
        desenharTexto(p, textoFila, 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
    }

    if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_UPLOAD || menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR) {
        char textoIP[128]; sprintf(textoIP, "IP DO PS4: %s", ipDoPS4); desenharTexto(p, textoIP, 25, 1550, 1020, 0xFF00FF00);
    }
}