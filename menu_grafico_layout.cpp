#include "menu_grafico_layout.h"
#include "menu_grafico_cache_grafico.h"
#include "menu_grafico_render_texto.h"
#include "graphics.h"
#include "elementos.h"
#include "stb_image.h"
#include "menu.h"
#include "baixar.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "temas.h"

int cursX = 0, cursY = 0, cursW = 0, cursH = 0;

extern Console listaConsoles[5];
extern int consoleAtual;
extern char linksAtuais[3000][1024];

extern int sel, off, totalItens;
extern char nomes[3000][256];
extern bool marcados[3000];
extern MenuLevel menuAtual;

extern int selEsq, offEsq, totalItensEsq;
extern char nomesEsq[3000][256];
extern bool marcadosEsq[3000];
extern MenuLevel menuAtualEsq;

extern bool painelDuplo;
extern int painelAtivo;

extern bool editMode;
extern int editTarget, editType;
extern int listStyle, listOri, fontTam, listBg, barFill, listMark, listHoverMark, listCurvature, listZoomCentro, fontAnim, fontAlign, fontScroll;
extern int frameContadorGlobal;

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH, listW, listH;
extern int gridX, gridY, gridItemW, gridItemH, gridCols, gridLins, gridSpcX, gridSpcY;
extern int capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, picX, picY, picW, picH;
extern int backX, backY, backW, backH, barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int msgX, msgY, msgTam;
extern int upBg, upTextNorm, upTextSel;
extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;
extern int sfxLigado, sfxVolume;
extern int numSlotsPadrao;
extern int p1Double, listLoop, listFocus;
extern int corSelecao;
extern int msgColIndex, fontColIndex;

extern int anim_posX, anim_posY, anim_velocidade, anim_colunas, anim_linhas, anim_offsetX, anim_offsetY, anim_frameInicial, anim_frameFinal, anim_frameAtual, anim_tolerancia, anim_keyR, anim_keyG, anim_keyB, anim_keyR2, anim_keyG2, anim_keyB2;
extern float anim_escala; extern bool anim_ativo, anim_usarColorKey2, anim_autoCenter;
extern int anim_frameOffsetX[100], anim_frameOffsetY[100];

extern char pathExplorar[256];
extern char pathExplorarEsq[256];
extern unsigned char* imgVidEdicao;
extern int wVidE, hVidE, cVidE;
extern unsigned char* defaultArtwork1;
extern int wDef1, hDef1;
extern unsigned char* defaultArtwork2;
extern int wDef2, hDef2;
extern unsigned char* imgPic1;
extern int wPic1, hPic1, cPic1;
extern unsigned char* imgCapaDinamica;
extern int dynCapaW, dynCapaH;
extern unsigned char* imgDiscoDinamico;
extern int dynDiscoW, dynDiscoH;
extern unsigned char* imgPreview;
extern int wP, hP, cP;
extern char msgStatus[128];
extern int msgTimer;

#ifndef CUSTOM_UI_DEF
#define CUSTOM_UI_DEF
struct CustomElementDef {
    bool ativo;
    char caminho[256];
    int pX, pY, pW, pH;
    bool animInAtiva;
    int inX, inY;
    int velIn;
    bool animOutAtiva;
    int outX, outY;
    int velOut;
};
#endif
extern CustomElementDef customUI[6][10];
extern unsigned char* uiTextures[10];
extern unsigned char* prevUiTextures[10];
extern int uiW[10], uiH[10], prevUiW[10], prevUiH[10];
extern int uiAnimFrame, prevTelaIdForOut;
extern int interfaceTelaAlvo, interfaceElementoAlvo;

// ==========================================
// RENDERIZAR CUSTOM UI
// ==========================================
void renderizarCustomUI(uint32_t* p, int telaId) {
    if (!editMode && editTarget != 17 && editTarget != 18) uiAnimFrame++;

    for (int i = 0; i < 10; i++) {
        if (customUI[prevTelaIdForOut][i].ativo && prevUiTextures[i]) {
            CustomElementDef* el = &customUI[prevTelaIdForOut][i];
            if (!el->animOutAtiva) continue;

            int maxFrames = 120 - (el->velOut * 10);
            if (maxFrames < 10) maxFrames = 10;

            if (uiAnimFrame < maxFrames && !editMode && editTarget != 18) {
                float outProgress = (float)uiAnimFrame / maxFrames;
                outProgress = outProgress * outProgress;
                int curX = el->pX + (int)((el->outX - el->pX) * outProgress);
                int curY = el->pY + (int)((el->outY - el->pY) * outProgress);
                desenharRedimensionado(p, prevUiTextures[i], prevUiW[i], prevUiH[i], el->pW, el->pH, curX, curY);
            }
        }
    }

    for (int i = 0; i < 10; i++) {
        if (customUI[telaId][i].ativo && uiTextures[i]) {
            CustomElementDef* el = &customUI[telaId][i];
            int curX = el->pX;
            int curY = el->pY;

            bool isEditingThis = (menuAtual == MENU_EDIT_TARGET && telaId == interfaceTelaAlvo && i == interfaceElementoAlvo && (editTarget == 18 || editTarget == 17));

            if (!isEditingThis || !editMode) {
                if (el->animInAtiva) {
                    int maxFrames = 120 - (el->velIn * 10);
                    if (maxFrames < 10) maxFrames = 10;

                    if (uiAnimFrame < maxFrames && !editMode && editTarget != 18) {
                        float inProgress = (float)uiAnimFrame / maxFrames;
                        inProgress = 1.0f - pow(1.0f - inProgress, 3.0f);
                        curX = el->inX + (int)((el->pX - el->inX) * inProgress);
                        curY = el->inY + (int)((el->pY - el->inY) * inProgress);
                    }
                }
            }
            else {
                if (editMode) {
                    if (editType == 65) { curX = el->inX; curY = el->inY; }
                    else if (editType == 70) { curX = el->outX; curY = el->outY; }
                }
            }

            desenharRedimensionado(p, uiTextures[i], uiW[i], uiH[i], el->pW, el->pH, curX, curY);

            if (isEditingThis) {
                uint32_t corBorda = 0xFF00FF00;
                if (editMode && (editType == 65 || editType == 70)) corBorda = 0xFFFF0000;
                for (int bx = 0; bx < el->pW; bx++) {
                    for (int by = 0; by < el->pH; by++) {
                        if (bx == 0 || bx == el->pW - 1 || by == 0 || by == el->pH - 1) {
                            int pxx = curX + bx; int pyy = curY + by;
                            if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = corBorda;
                        }
                    }
                }
            }
        }
    }
}

// ==========================================
// DESENHAR LISTAS (JOGOS X EXPLORADOR)
// ==========================================
extern int idIcone(const char* nome);
extern void desenharIcone(uint32_t* tela, int id, int tamanho, int posX, int posY);

static int iconeArquivo(const char* extensao) {
    static const char* apelidos[][2] = {
        { "mkv", "mp4" }, { "webm", "mp4" }, { "wmv", "mp4" }, { "m4v", "mp4" }, { "ts", "mp4" }, { "3gp", "mp4" }, { "mpeg", "mpg" }, { "mov", "qt" },
        { "jpeg", "jpg" }, { "webp", "png" }, { "flac", "wav" }, { "ogg", "mp3" }, { "m4a", "aac" }, { "7z", "zip" }, { "gz", "tgz" }, { "cbz", "zip" },
        { "pkg", "iso" }, { "bin", "dat" }, { "json", "js" }, { "yaml", "yml" }, { "htm", "html" }, { "docx", "doc" }, { "pptx", "ppt" }, { "log", "txt" }, { "ini", "txt" }, { "cfg", "txt" }
    };
    static const char* jogos[] = { "gba", "gb", "gbc", "nes", "sfc", "smc", "md", "gen", "smd", "sms", "gg", "n64", "z64", "cue", "chd" };

    char ext[12]; int n = 0;
    for (; extensao[n] && n < 11; n++) ext[n] = (char)tolower((unsigned char)extensao[n]);
    ext[n] = '\0';
    for (unsigned int i = 0; i < sizeof(jogos) / sizeof(jogos[0]); i++) if (strcmp(ext, jogos[i]) == 0) return idIcone("m_jogar");
    for (unsigned int i = 0; i < sizeof(apelidos) / sizeof(apelidos[0]); i++) if (strcmp(ext, apelidos[i][0]) == 0) { strcpy(ext, apelidos[i][1]); break; }

    char nome[20]; snprintf(nome, sizeof(nome), "f_%s", ext);
    int id = idIcone(nome);
    return id >= 0 ? id : idIcone("f_page");
}

static int iconeDoItem(const char* nome, MenuLevel menu) {
    static const char* palavras[][2] = {
        { "pesquisar", "m_lupa" }, { "github", "m_caixa" }, { "adicionar", "m_mais" }, { "criar", "m_mais" }, { "[+]", "m_mais" }, { "radio", "m_radio" }, { "podcast", "m_radio" }, { "upload", "m_enviar" }, { "download", "m_baixar" }, { "baixar", "m_baixar" },
        { "restaurar", "m_nuvem" }, { "backup", "m_nuvem" }, { "dropbox", "m_nuvem" }, { "ftp", "m_servidor" }, { "servidor", "m_servidor" },
        { "loja", "m_loja" }, { "hb store", "m_loja" }, { "repositorio", "m_caixa" }, { "link", "m_link" }, { "xml", "m_lista" },
        { "jog", "m_jogar" }, { "ps4", "m_jogar" }, { "retroarch", "m_jogar" }, { "nintendo", "m_jogar" }, { "sega", "m_jogar" }, { "sony", "m_jogar" }, { "controle", "m_jogar" },
        { "midia", "m_midia" }, { "video", "m_video" }, { "filme", "m_video" }, { "serie", "m_video" }, { "anime", "m_video" },
        { "audio", "m_nota" }, { "musica", "m_nota" }, { "piano", "m_nota" }, { "imag", "m_imagem" }, { "leitura", "m_livro" }, { "livro", "m_livro" }, { "manga", "m_livro" },
        { "arquivo", "m_arquivos" }, { "editar", "m_editar" }, { "explorar", "m_explorar" }, { "extra", "m_extra" }, { "conquista", "m_extra" }, { "favorit", "m_extra" }, { "informac", "m_info" },
        { "hyper neiva", "m_casa" }, { "raiz", "m_disco" }, { "usb", "m_usb" }, { "pesquisar", "m_lupa" },
        { "erro", "m_aviso" }, { "nenhum", "m_aviso" }, { "vazia", "m_aviso" }, { "offline", "m_aviso" }, { "verifique", "m_aviso" }
    };

    if (nome[0] == '[' && nome[1] != ' ' && nome[1] != '+') return idIcone("f_pasta");

    const char* ponto = strrchr(nome, '.');
    if (ponto && ponto[1] && strlen(ponto) <= 6 && !strchr(ponto, ' ') && !strchr(ponto, ')')) return iconeArquivo(ponto + 1);

    char minusculo[256]; int n = 0;
    for (; nome[n] && n < 255; n++) minusculo[n] = (char)tolower((unsigned char)nome[n]);
    minusculo[n] = '\0';

    if (strstr(minusculo, "ponteiro")) return idIcone("m_ponteiro");
    if (menu == MENU_RADIO_LISTA || menu == MENU_RADIO_FAVORITOS || menu == MENU_RADIO_CATEGORIA) return idIcone("m_radio");
    if (menu == MENU_BAIXAR_GITHUB_REPOS || (menu == MENU_BAIXAR_GITHUB && strcmp(nome, "Pesquisar no GitHub") != 0 && strcmp(nome, "Pasta vazia") != 0)) return idIcone("m_caixa");
    if (menu == MENU_BAIXAR_GITHUB_RELEASES) return idIcone("m_arquivos");
    if (menu == MENU_EDITAR || menu == MENU_EDIT_TARGET) return idIcone("m_engrenagem");
    if (menu == MENU_JOGAR_PS4 || menu == JOGAR_XML || menu == SCRAPER_LIST) return idIcone("m_jogar");

    for (unsigned int i = 0; i < sizeof(palavras) / sizeof(palavras[0]); i++) {
        if (strstr(minusculo, palavras[i][0])) return idIcone(palavras[i][1]);
    }
    return idIcone(menu == MENU_MIDIA ? "f_pasta" : "m_item");
}

static void drawSimpleIcon(uint32_t* p, const char* name, MenuLevel menu, int drawX, int drawY, int iconSize) {
    desenharIcone(p, iconeDoItem(name, menu), iconSize, drawX + 4, drawY + (listH - iconSize) / 2);
}
void desenharListas(uint32_t* p, int refPainel) {
    int sAtual = (refPainel == 0) ? selEsq : sel;
    int oAtual = (refPainel == 0) ? offEsq : off;
    int tItens = (refPainel == 0) ? totalItensEsq : totalItens;
    char (*nItems)[256] = (refPainel == 0) ? nomesEsq : nomes;
    bool* mItems = (refPainel == 0) ? marcadosEsq : marcados;
    MenuLevel mAtual = (refPainel == 0) ? menuAtualEsq : menuAtual;

    // --- CORREÇÃO: POVOANDO A TELA INICIAL DO EXPLORADOR NO L2 ---
    if (mAtual == MENU_EXPLORAR_HOME && tItens <= 0) {
        strcpy(nItems[0], "Hyper Neiva"); strcpy(nItems[1], "Raiz");
        strcpy(nItems[2], "USB 0"); strcpy(nItems[3], "USB 1");
        tItens = 4;
        if (refPainel == 0) totalItensEsq = 4; else totalItens = 4;
    }

    int currentRenderStyle = listStyle;
    bool isGameMenu = (mAtual == MENU_JOGAR_PS4 || mAtual == JOGAR_XML || mAtual == SCRAPER_LIST);

    // DETERMINA SE É UM MENU DE SISTEMA (Pode precisar de fallback visual)
    bool isSystemMenu = (mAtual == ROOT || mAtual == MENU_EXPLORAR || mAtual == MENU_EXPLORAR_HOME || mAtual == MENU_EDITAR || mAtual == MENU_EDIT_TARGET || mAtual == MENU_BAIXAR_FTP_LISTA);

    uint32_t corBasePainel = getSysColor(listBg);
    int curListX = (listOri == 0) ? listXV : listXH;
    int curListY = (listOri == 0) ? listYV : listYH;
    int curListSpc = (listOri == 0) ? listSpcV : listSpcH;

    int larguraBarraDupla = 750;
    int posX_Base = (painelDuplo) ? ((refPainel == 0) ? capaX : curListX) : curListX;
    int larguraItem = (painelDuplo) ? larguraBarraDupla : listW;
    int stepX = (listOri == 1) ? curListSpc : 0;
    int stepY = (listOri == 0) ? curListSpc : 0;

    static float smoothSelDir = 0.0f; static float smoothSelEsq = 0.0f; static MenuLevel lastMenu2 = ROOT;
    if (lastMenu2 != menuAtual) { smoothSelDir = (float)sel; smoothSelEsq = (float)selEsq; lastMenu2 = menuAtual; initCoverCache(); clearCoverCache(); }
    float& smoothSel = (refPainel == 0) ? smoothSelEsq : smoothSelDir;

    if (fabs(smoothSel - sAtual) > 20.0f) smoothSel = (float)sAtual;
    smoothSel += (sAtual - smoothSel) * 0.15f;

    initCoverCache();

    // ----------------------------------------
    // MODO GRADE (4 ou 5)
    // ----------------------------------------
    if (currentRenderStyle == 4 || currentRenderStyle == 5) {
        int cols = (currentRenderStyle == 5) ? 3 : gridCols;
        if (cols < 1) cols = 1;
        int itemW = gridItemW; int itemH = gridItemH;
        int paddingX = gridSpcX; int paddingY = gridSpcY;
        int startX = (currentRenderStyle == 5) ? 50 : gridX;
        int startY = gridY;

        int currentRow = (int)smoothSel / cols;
        int startIdx = (currentRow > 4 ? currentRow - 4 : 0) * cols;
        int endIdx = startIdx + (cols * (gridLins + 6));
        if (endIdx >= tItens) endIdx = tItens - 1;

        cleanOldCache(startIdx, endIdx);

        float smoothCameraY = (smoothSel / cols) * (itemH + paddingY) - startY;
        if (smoothCameraY < -200) smoothCameraY = -200;

        for (int i = startIdx; i <= endIdx; i++) {
            int c = i % cols;
            float globalY = (i / cols) * (itemH + paddingY);
            int drawX = startX + c * (itemW + paddingX);
            int drawY = startY + (int)(globalY - smoothCameraY);

            if (drawY > 1280 || drawY + itemH < -200) continue;

            int cacheIdx = getCachedImage(i);
            if (cacheIdx == -1) {
                int freeSlot = getFreeCacheSlot();
                if (freeSlot != -1) {
                    char cPath[512] = "";
                    if (mAtual == MENU_JOGAR_PS4) {
                        sprintf(cPath, "/user/appmeta/%s/icon0.png", linksAtuais[i]);
                    }
                    else {
                        const char* cName = (consoleAtual >= 0 && consoleAtual < 5) ? listaConsoles[consoleAtual].nome : "Unknown";
                        snprintf(cPath, sizeof(cPath), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", cName, nItems[i]);
                    }
                    coverCache[freeSlot].index = i;
                    coverCache[freeSlot].img = stbi_load(cPath, &coverCache[freeSlot].w, &coverCache[freeSlot].h, &coverCache[freeSlot].c, 4);
                    cacheIdx = freeSlot;
                }
            }

            if (cacheIdx != -1 && coverCache[cacheIdx].img) {
                desenharRedimensionado(p, coverCache[cacheIdx].img, coverCache[cacheIdx].w, coverCache[cacheIdx].h, itemW, itemH, drawX, drawY);
            }
            else {
                // FALLBACK GRADE PARA MENUS DE SISTEMA
                uint32_t corF = (i == sAtual) ? 0xFF0055AA : 0xAA222222;
                for (int by = 0; by < itemH; by++) for (int bx = 0; bx < itemW; bx++) { int pyy = drawY + by, pxx = drawX + bx; if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = corF; }
                desenharTextoAlinhado(p, nItems[i], 28, drawX + 10, drawY + (itemH / 2) - 15, itemW - 20, 0xFFFFFFFF);
            }

            if (i == sAtual) {
                for (int by = -5; by < itemH + 5; by++) for (int bx = -5; bx < itemW + 5; bx++) {
                    if (by < 0 || by >= itemH || bx < 0 || bx >= itemW) {
                        int pyy = drawY + by, pxx = drawX + bx;
                        if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xFF00FF00;
                    }
                }
                if (currentRenderStyle == 4) desenharTextoAlinhadoAnimado(p, nItems[i], 40, drawX - 50, drawY + itemH + 30, itemW + 100, getSysColor(fontColIndex), true);

                if (refPainel == 1 || !painelDuplo) {
                    cursX = drawX; cursY = drawY; cursW = itemW; cursH = itemH;
                }
            }
        }
    }
    // ----------------------------------------
    // MODO ROLETA (3)
    // ----------------------------------------
    else if (currentRenderStyle == 3) {
        int centerW = 400; int centerH = 400;
        int sideW = 250; int sideH = 250;
        int centerY = 350; int centerX = 1920 / 2 - centerW / 2;

        int startIdx = sAtual - 4; if (startIdx < 0) startIdx = 0;
        int endIdx = sAtual + 4; if (endIdx >= tItens) endIdx = tItens - 1;
        cleanOldCache(startIdx, endIdx);

        for (int offset = 4; offset >= 0; offset--) {
            for (int dir = -1; dir <= 1; dir += 2) {
                if (offset == 0 && dir == 1) continue;
                int i = sAtual + (offset * dir);
                if (i < 0 || i >= tItens) continue;

                int drawW = (offset == 0) ? centerW : sideW;
                int drawH = (offset == 0) ? centerH : sideH;

                float dist = smoothSel - i;
                int drawX = centerX - (int)(dist * (sideW + 50));
                if (offset > 0) {
                    if (i > sAtual) drawX += (centerW - sideW) / 2 + 50;
                    else drawX -= (centerW - sideW) / 2 + 50;
                }

                int drawY = centerY + (centerH - drawH) / 2;
                if (drawX > 1920 || drawX + drawW < 0) continue;

                int cacheIdx = getCachedImage(i);
                if (cacheIdx == -1) {
                    int freeSlot = getFreeCacheSlot();
                    if (freeSlot != -1) {
                        char cPath[512] = "";
                        if (mAtual == MENU_JOGAR_PS4) {
                            sprintf(cPath, "/user/appmeta/%s/icon0.png", linksAtuais[i]);
                        }
                        else {
                            const char* cName = (consoleAtual >= 0 && consoleAtual < 5) ? listaConsoles[consoleAtual].nome : "Unknown";
                            snprintf(cPath, sizeof(cPath), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", cName, nItems[i]);
                        }
                        coverCache[freeSlot].index = i;
                        coverCache[freeSlot].img = stbi_load(cPath, &coverCache[freeSlot].w, &coverCache[freeSlot].h, &coverCache[freeSlot].c, 4);
                        cacheIdx = freeSlot;
                    }
                }

                if (cacheIdx != -1 && coverCache[cacheIdx].img) {
                    desenharRedimensionado(p, coverCache[cacheIdx].img, coverCache[cacheIdx].w, coverCache[cacheIdx].h, drawW, drawH, drawX, drawY);
                }
                else {
                    // FALLBACK ROLETA PARA MENUS DE SISTEMA
                    uint32_t corF = (offset == 0) ? 0xFF0077CC : 0xAA222222;
                    for (int by = 0; by < drawH; by++) for (int bx = 0; bx < drawW; bx++) { int pyy = drawY + by, pxx = drawX + bx; if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = corF; }
                    if (offset < 2) desenharTextoAlinhado(p, nItems[i], 24, drawX + 10, drawY + (drawH / 2) - 12, drawW - 20, 0xFFFFFFFF);
                }

                if (offset == 0) {
                    for (int by = -6; by < drawH + 6; by++) for (int bx = -6; bx < drawW + 6; bx++) {
                        if (by < 0 || by >= drawH || bx < 0 || bx >= drawW) {
                            int pyy = drawY + by, pxx = drawX + bx;
                            if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xFF00FFFF;
                        }
                    }
                    desenharTextoAlinhadoAnimado(p, nItems[i], 40, centerX - 200, centerY + centerH + 50, centerW + 400, getSysColor(fontColIndex), true);

                    if (refPainel == 1 || !painelDuplo) {
                        cursX = drawX; cursY = drawY; cursW = drawW; cursH = drawH;
                    }
                }
            }
        }
    }
    // ----------------------------------------
    // MODO LISTA CLÁSSICA E PAINEL EXPLORADOR
    // ----------------------------------------
    else {
        for (int i = 0; i < numSlotsPadrao; i++) {
            int gIdx;
            bool loopVisual = listLoop && (tItens > numSlotsPadrao);
            if (loopVisual && tItens > 0) { gIdx = (oAtual + i) % tItens; if (gIdx < 0) gIdx += tItens; }
            else { gIdx = i + oAtual; if (gIdx >= tItens || gIdx < 0) continue; }

            int currentX = posX_Base; int currentY = curListY;
            int animOffsetX = 0; int animOffsetY = 0;
            int currentFontTam = fontTam;
            float opacityMulti = 1.0f;

            if (currentRenderStyle == 1) {
                float dist = (float)gIdx - smoothSel;
                int centroX = posX_Base; int centroY = curListY + (2 * stepY);
                if (listOri == 1) centroX = posX_Base + (2 * stepX);

                currentX = centroX + (int)(dist * stepX);
                currentY = centroY + (int)(dist * stepY);

                int curva = (int)(dist * dist * (float)listCurvature);
                if (listOri == 0) animOffsetX = curva; else animOffsetY = curva;

                float absDist = fabs(dist);
                if (absDist < 1.0f && (!painelDuplo || painelAtivo == refPainel)) currentFontTam += (int)((1.0f - absDist) * (float)listZoomCentro);
                if (absDist > 2.0f) { opacityMulti = 1.0f - ((absDist - 2.0f) / 1.5f); if (opacityMulti < 0.0f) opacityMulti = 0.0f; }
            }
            else {
                currentX = posX_Base + (i * stepX);
                currentY = curListY + (i * stepY);

                if (currentRenderStyle == 2 && gIdx == sAtual && (!painelDuplo || painelAtivo == refPainel)) {
                    if (listOri == 0) animOffsetX = 30; else animOffsetY = 30;
                    currentFontTam += (listZoomCentro / 2);
                }
                else if (currentRenderStyle == 0 || currentRenderStyle == 5) {
                    if (gIdx == sAtual && (!painelDuplo || painelAtivo == refPainel)) currentFontTam += (listZoomCentro / 2);
                    else currentFontTam -= 6;
                }
            }

            bool isPainelAtivo = (!painelDuplo || painelAtivo == refPainel);
            uint32_t corFundo = isPainelAtivo ? corBasePainel : 0xAA111111;
            uint32_t corTexto = isPainelAtivo ? getSysColor(fontColIndex) : 0xFFAAAAAA;

            bool isMarcado = (mAtual == MENU_EXPLORAR || mAtual == MENU_BAIXAR_DROPBOX_LISTA || mAtual == MENU_BAIXAR_DROPBOX_UPLOAD || mAtual == MENU_BAIXAR_FTP_LISTA) && mItems[gIdx];
            if (isMarcado) { corFundo = isPainelAtivo ? getSysColor(listMark) : getSysColor(11); }

            bool isSelected = isPainelAtivo && (gIdx == sAtual);
            if (isSelected) {
                if (isMarcado) corFundo = getSysColor(listHoverMark); else corFundo = getSysColor(corSelecao);
                corTexto = 0xFF000000;

                if (fontAnim == 1) { currentFontTam += (int)(sin(frameContadorGlobal * 0.1f) * 6.0f); }
                else if (fontAnim == 2) { uint8_t r = (uint8_t)((sin(frameContadorGlobal * 0.05f) + 1.0f) * 127.5f); uint8_t g = (uint8_t)((sin(frameContadorGlobal * 0.05f + 2.0f) + 1.0f) * 127.5f); uint8_t b = (uint8_t)((sin(frameContadorGlobal * 0.05f + 4.0f) + 1.0f) * 127.5f); corTexto = 0xFF000000 | (r << 16) | (g << 8) | b; }
                else if (fontAnim == 3) { animOffsetY += (int)(sin(frameContadorGlobal * 0.2f) * 8.0f); }
            }
            else if (!isPainelAtivo) { corFundo = 0xAA555555; }

            uint8_t aF = (uint8_t)(((corFundo >> 24) & 0xFF) * opacityMulti); corFundo = (aF << 24) | (corFundo & 0x00FFFFFF);
            uint8_t aT = (uint8_t)(((corTexto >> 24) & 0xFF) * opacityMulti); corTexto = (aT << 24) | (corTexto & 0x00FFFFFF);

            int drawX = currentX + animOffsetX; int drawY = currentY + animOffsetY;

            if (aF > 10) { for (int by = 0; by < listH; by++) { for (int bx = 0; bx < larguraItem; bx++) { int pxX = drawX + bx; int pyY = drawY + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = corFundo; } } }

            { int iconSize = (listH > 48) ? 40 : listH - 4; if (iconSize < 16) iconSize = 16; drawSimpleIcon(p, nItems[gIdx], mAtual, drawX, drawY, iconSize); desenharTextoAlinhadoAnimado(p, nItems[gIdx], currentFontTam, drawX + 48, drawY + (listH / 4), larguraItem - 48, corTexto, isSelected); }

            if (isSelected) {
                if (refPainel == 1 || !painelDuplo) {
                    cursX = drawX; cursY = drawY; cursW = larguraItem; cursH = listH;
                }
            }
        }
    }
}

const char* getColorName(int index) {
    static const char* colorNames[] = {
        "CINZA ESCURO", "PRETO", "AZUL ESCURO", "VERMELHO ESCURO", "VERDE ESCURO",
        "TRANSPARENTE", "CINZA", "VERDE LIMA", "BEGE", "VERDE BRILHO",
        "AZUL CLARO", "OLIVA", "BRANCO", "VERMELHO", "AZUL"
    };
    if (index >= 0 && index <= 14) return colorNames[index];
    return "DESCONHECIDO";
}

// ==========================================
// DESENHAR RODAPÉ DE EDIÇÃO
// ==========================================
void desenharRodapeEdicao(uint32_t* p) {
    char txtPos[256] = "";
    int* tX = &listXV, * tY = &listYV, * tW = &listW, * tH = &listH;

    if (editTarget == 0) {
        if (listStyle == 4 || listStyle == 5) { tX = &gridX; tY = &gridY; tW = &gridItemW; tH = &gridItemH; }
        else { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
    }
    else if (editTarget == 1) { tX = &capaX; tY = &capaY; tW = &capaW; tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else if (editTarget == 3) { tX = &picX; tY = &picY; tW = &picW; tH = &picH; }
    else if (editTarget == 4) { tX = &backX; tY = &backY; tW = &backW; tH = &backH; }
    else if (editTarget == 5) { tX = &barX; tY = &barY; tW = &barW; tH = &barH; }
    else if (editTarget == 6) { tX = &audioX; tY = &audioY; tW = &audioW; tH = &audioH; }
    else if (editTarget == 7 || editTarget == 10) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
    else if (editTarget == 8) { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }
    else if (editTarget == 9) { tX = &msgX; tY = &msgY; tW = &msgTam; tH = &msgTam; }
    else if (editTarget == 11) { tX = &elem1X; tY = &elem1Y; tW = &elem1W; tH = &elem1H; }
    else if (editTarget == 12) { tX = &ctrl1X; tY = &ctrl1Y; tW = &ctrl1W; tH = &ctrl1H; }
    else if (editTarget == 13) { tX = &pont1X; tY = &pont1Y; tW = &pont1W; tH = &pont1H; }

    if (editTarget == 16) {
        const char* tn[] = { "ROOT", "JOGAR", "MIDIA", "BAIXAR", "EDITAR", "EXPLORAR" };
        sprintf(txtPos, "TELA ALVO SELECIONADA: %s", tn[interfaceTelaAlvo]);
    }
    else if (editTarget == 17 || editTarget == 18) {
        CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo];
        int dX = el->pX; int dY = el->pY;
        if (editType == 65) { dX = el->inX; dY = el->inY; }
        else if (editType == 70) { dX = el->outX; dY = el->outY; }

        if (editMode) {
            if (editType == 62) sprintf(txtPos, "CUSTOM UI - POSICAO X:%d Y:%d (SETAS E R1)", dX, dY);
            else if (editType == 63) sprintf(txtPos, "CUSTOM UI - TAMANHO PROPORCIONAL LARGURA:%d ALTURA:%d (SETAS E R1)", el->pW, el->pH);
            else if (editType == 64) sprintf(txtPos, "CUSTOM UI - ESTICAR LARGURA:%d ALTURA:%d (SETAS E R1)", el->pW, el->pH);
            else if (editType == 67) sprintf(txtPos, "CUSTOM UI - ANIM ENTRADA: %s (ESQ/DIR MUDAR)", el->animInAtiva ? "LIGADA" : "DESLIGADA");
            else if (editType == 65) sprintf(txtPos, "CUSTOM UI - NASCIMENTO X:%d Y:%d (SETAS E R1)", dX, dY);
            else if (editType == 68) sprintf(txtPos, "CUSTOM UI - VELOCIDADE ENTRADA: %d (ESQ/DIR MUDAR)", el->velIn);
            else if (editType == 69) sprintf(txtPos, "CUSTOM UI - ANIM SAIDA: %s (ESQ/DIR MUDAR)", el->animOutAtiva ? "LIGADA" : "DESLIGADA");
            else if (editType == 70) sprintf(txtPos, "CUSTOM UI - SAIDA FADE OUT X:%d Y:%d (SETAS E R1)", dX, dY);
            else if (editType == 71) sprintf(txtPos, "CUSTOM UI - VELOCIDADE SAIDA: %d (ESQ/DIR MUDAR)", el->velOut);
            else sprintf(txtPos, "CUSTOM UI - EDITANDO (USE SETAS E R1)");
        }
        else sprintf(txtPos, "CUSTOM UI - ELEMENTO %d DA TELA %d", interfaceElementoAlvo + 1, interfaceTelaAlvo);
    }
    else if (editTarget == 10 && editType == 0) sprintf(txtPos, "MODO EDICAO - CORES DO EXPLORAR (USE SETAS ESQ/DIR)");
    else if (editType == 3) sprintf(txtPos, "MODO EDICAO - COR DE FUNDO: %s (ESQ/DIR)", getColorName(listBg));
    else if (editType == 8) sprintf(txtPos, "MODO EDICAO - COR PREENCHIMENTO: %s (ESQ/DIR)", getColorName(barFill));
    else if (editType == 4) sprintf(txtPos, "MODO EDICAO - ESPACAMENTO: %d", (listOri == 0) ? listSpcV : listSpcH);
    else if (editType == 5) sprintf(txtPos, "MODO EDICAO - ORIENTACAO: %s", listOri == 0 ? "VERTICAL" : "HORIZONTAL");
    else if (editType == 6) sprintf(txtPos, "MODO EDICAO - COR LISTA MARCADA: %s", getColorName(listMark));
    else if (editType == 7) sprintf(txtPos, "MODO EDICAO - COR LISTA CURSOR: %s", getColorName(listHoverMark));
    else if (editType == 10) sprintf(txtPos, "MODO EDICAO - ALINHAMENTO: %s", fontAlign == 0 ? "ESQ" : (fontAlign == 1 ? "CENTRO" : "DIR"));
    else if (editType == 11) sprintf(txtPos, "MODO EDICAO - LIMITES: %s", fontScroll == 0 ? "CORTAR (..)" : "ANIMACAO ROLAGEM");
    else if (editType == 12) { int stat = 0; if (editTarget == 11) stat = elem1On; else if (editTarget == 12) stat = ctrl1On; else if (editTarget == 13) stat = pont1On; sprintf(txtPos, "MODO EDICAO - LIGADO: %s", stat ? "SIM" : "NAO"); }
    else if (editType == 13) sprintf(txtPos, "MODO EDICAO - MODO PONTEIRO: %s", pont1Modo == 0 ? "ACOMPANHA" : "ESTATICO");
    else if (editType == 14) { const char* lds[] = { "ESQ", "DIR", "CIMA", "BAIXO" }; sprintf(txtPos, "MODO EDICAO - LADO PONTEIRO: %s", lds[pont1Lado]); }
    else if (editType == 15) sprintf(txtPos, "MODO EDICAO - EFEITOS SONOROS: %s", sfxLigado ? "LIGADO" : "DESLIGADO");
    else if (editType == 16) sprintf(txtPos, "MODO EDICAO - VOLUME EFEITOS: %d%%", sfxVolume);
    else if (editType == 301) { extern int volumeGeral; sprintf(txtPos, "MODO EDICAO - VOLUME DA MUSICA: %d%%", volumeGeral); }
    else if (editType == 17) sprintf(txtPos, "MODO EDICAO - MENU OPCOES POSICAO: X:%d Y:%d", upX, upY);
    else if (editType == 18) sprintf(txtPos, "MODO EDICAO - MENU OPCOES TAMANHO: W:%d H:%d", upW, upH);
    else if (editType == 19) sprintf(txtPos, "MODO EDICAO - MENU OPCOES ESTICAR: W:%d", upW);
    else if (editType == 20) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR FUNDO: %s", getColorName(upBg));
    else if (editType == 21) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR DO TEXTO: %s", getColorName(upTextNorm));
    else if (editType == 22) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR TEXT SEL: %s", getColorName(upTextSel));
    else if (editType == 35) sprintf(txtPos, "MODO EDICAO - COR SELECAO: %s (ESQ/DIR)", getColorName(corSelecao));
    else if (editType == 80) sprintf(txtPos, "MODO EDICAO - COR DA NOTIFICACAO: %s (ESQ/DIR)", getColorName(msgColIndex));
    else if (editType == 81) sprintf(txtPos, "MODO EDICAO - COR DA FONTE (MENU): %s (ESQ/DIR)", getColorName(fontColIndex));
    else if (editType == 82) sprintf(txtPos, "MODO EDICAO - PONTEIROS (DUPLO): %s (QUALQUER SETA)", p1Double ? "SIM" : "NAO");
    else if (editType == 83) sprintf(txtPos, "MODO EDICAO - LISTA CICLICA (LOOP): %s (QUALQUER SETA)", listLoop ? "SIM" : "NAO");
    else if (editType == 84) sprintf(txtPos, "MODO EDICAO - FOCO CENTRALIZADO: %s (QUALQUER SETA)", listFocus ? "SIM" : "NAO");
    else if (editType == 45) {
        const char* nEst[] = { "0. ORIGINAL", "1. ROLETA", "2. HORIZONTAL", "3. N-SWITCH", "4. GRADE PURA", "5. GRADE + LISTA" };
        sprintf(txtPos, "MODO EDICAO - ESTILO DA LISTA: %s (USE SETAS)", nEst[listStyle]);
    }
    else if (editType == 77) sprintf(txtPos, "MODO EDICAO - GRADE COLUNAS:%d LINHAS:%d", gridCols, gridLins);
    else if (editType == 78) sprintf(txtPos, "MODO EDICAO - GRADE ESPACAMENTO X:%d Y:%d", gridSpcX, gridSpcY);
    else if (editType == 75) sprintf(txtPos, "MODO EDICAO - POSICAO DA GRADE X:%d Y:%d", gridX, gridY);
    else if (editType == 76) sprintf(txtPos, "MODO EDICAO - TAMANHO DA CAPA NA GRADE W:%d H:%d", gridItemW, gridItemH);
    else if (editType == 49) sprintf(txtPos, "MODO EDICAO - SLOTS VISIVEIS: %d", numSlotsPadrao);
    else if (editTarget < 52) sprintf(txtPos, "MODO EDICAO - X: %d  |  Y: %d  |  LARGURA: %d  |  ALTURA: %d", *tX, *tY, *tW, *tH);

    for (int by = 0; by < 40; by++) { for (int bx = 0; bx < 1920; bx++) { int pyY = 1040 + by; if (pyY < 1080) p[pyY * 1920 + bx] = 0xAA000000; } }
    desenharTexto(p, txtPos, 25, 50, 1045, 0xFF00FF00);
}

// ===== TEMAS =====
#define ARQUIVO_TEMA "/data/HyperNeiva/configuracao/tema.txt"
static int temaInterface = -1;

int temaAtual() {
    if (temaInterface >= 0) return temaInterface;
    temaInterface = TEMA_PADRAO;
    FILE* f = fopen(ARQUIVO_TEMA, "r");
    if (f) {
        int lido = 0;
        if (fscanf(f, "%d", &lido) == 1 && lido >= 0 && lido < TEMA_TOTAL) temaInterface = lido;
        fclose(f);
    }
    return temaInterface;
}

void definirTema(int tema) {
    extern void conquistaGanhar(const char* chave);
    if (tema < 0 || tema >= TEMA_TOTAL) return;
    temaInterface = tema;
    conquistaGanhar("tema");
    FILE* f = fopen(ARQUIVO_TEMA, "w");
    if (f) { fprintf(f, "%d\n", tema); fclose(f); }
}

static bool temaMenuInicio(MenuLevel m) { return m == ROOT || m == MENU_TIPO_JOGO; }
static bool temaMenuJogos(MenuLevel m) { return m == MENU_JOGAR_PS4 || m == JOGAR_XML || m == SCRAPER_LIST; }

// Só telas que são uma lista simples: o resto segue com o desenho próprio.
bool temaCobreMenu() {
    extern bool visualizandoMidiaImagem, visualizandoMidiaTexto, visualizandoPDF, editMode;
    if (editMode || painelDuplo || visualizandoMidiaImagem || visualizandoMidiaTexto || visualizandoPDF) return false;
    if (menuAtual == MENU_EMULADOR) return true;   // a biblioteca tem tela própria em qualquer tema
    if (temaAtual() == TEMA_PADRAO) return false;
    switch (menuAtual) {
    case ROOT: case MENU_TIPO_JOGO: case MENU_JOGAR_PS4: case JOGAR_XML: case SCRAPER_LIST:
    case MENU_MIDIA: case MENU_BAIXAR: case MENU_EDITAR: case MENU_EDIT_TARGET: case MENU_EXPLORAR: case MENU_EXPLORAR_HOME:
    case MENU_EXTRA: case MENU_BAIXAR_TORRENT: case MENU_MUSICAS: case MENU_BAIXAR_GITHUB: case MENU_BAIXAR_GITHUB_REPOS:
    case MENU_BAIXAR_GITHUB_RELEASES: case MENU_BAIXAR_GITHUB_ARQUIVOS: case MENU_BAIXAR_DROPBOX_LISTA: case MENU_BAIXAR_FTP_LISTA:
    case MENU_RADIO_CATEGORIA: case MENU_RADIO_LISTA: case MENU_RADIO_FAVORITOS: case MENU_CONSOLES:
        return true;
    default:
        return false;
    }
}

// Nestes menus as setas esquerda e direita andam na fila, como nos cartões do tema.
bool temaHorizontal() {
    if (!temaCobreMenu()) return false;
    if (menuAtual == MENU_EMULADOR) return true;
    int tema = temaAtual();
    if (temaMenuInicio(menuAtual)) return tema != TEMA_OPL;
    return temaMenuJogos(menuAtual) && (tema == TEMA_PS4 || tema == TEMA_PS5 || tema == TEMA_NETFLIX);
}

void temaIcone(uint32_t* p, const char* nome, int tipo, int tam, int x, int y) {
    desenharIcone(p, iconeDoItem(nome, tipo == CENA_LISTA ? menuAtual : ROOT), tam, x, y);
}

static bool temaCapaCarregada = false;

bool temaFavorito(const char* nome) {
    extern bool bibliotecaFavorito(const char* nome);
    return menuAtual == MENU_EMULADOR && bibliotecaFavorito(nome);
}

bool temaCapa(int indice, const char* nome, unsigned char** img, int* w, int* h) {
    int cache = getCachedImage(indice);
    if (cache == -1) {
        // Abrir PNG é demorado: um por quadro, para o carrossel não engasgar.
        if (temaCapaCarregada) { tmRedesenhar = true; tmCapaPendente = true; return false; }
        int livre = getFreeCacheSlot();
        if (livre == -1) return false;
        temaCapaCarregada = true;
        tmRedesenhar = true;
        char caminho[512] = "";
        if (menuAtual == MENU_EMULADOR) {
            extern bool bibliotecaCaminhoCapa(const char* jogo, char* saida, size_t tamanho);
            bibliotecaCaminhoCapa(nome, caminho, sizeof(caminho));
        }
        else if (menuAtual == MENU_JOGAR_PS4) snprintf(caminho, sizeof(caminho), "/user/appmeta/%s/icon0.png", linksAtuais[indice]);
        else {
            const char* console = (consoleAtual >= 0 && consoleAtual < 5) ? listaConsoles[consoleAtual].nome : "Unknown";
            snprintf(caminho, sizeof(caminho), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", console, nome);
        }
        coverCache[livre].index = indice;
        coverCache[livre].img = stbi_load(caminho, &coverCache[livre].w, &coverCache[livre].h, &coverCache[livre].c, 4);
        cache = livre;
    }
    if (!coverCache[cache].img) return false;
    *img = coverCache[cache].img;
    *w = coverCache[cache].w;
    *h = coverCache[cache].h;
    return true;
}

void temaDesenharAtual(uint32_t* p) {
    static char titulo[256] = "HYPER NEIVA";
    static unsigned int quadro = 0;
    if (temaMenuInicio(menuAtual) && totalItens > 0) snprintf(titulo, sizeof(titulo), "%s", nomes[sel]);
    TemaCena cena;
    cena.nomes = nomes;
    cena.marcados = marcados;
    cena.total = totalItens;
    cena.sel = sel;
    cena.tipo = temaMenuInicio(menuAtual) ? CENA_INICIO : temaMenuJogos(menuAtual) ? CENA_JOGOS : CENA_LISTA;
    cena.titulo = titulo;
    cena.quadro = quadro++;
    cena.subtitulo = "";
    cena.opcoes = NULL;
    cena.totalOpcoes = 0;
    cena.selOpcao = 0;
    cena.aba = 0;
    cena.tituloPainel = "";
    cena.previa = NULL;
    cena.previaW = 0;
    cena.previaH = 0;
    temaCapaCarregada = false;
    // A capa fica guardada pela posição na lista: sem limpar, o ícone de um app da lista
    // do PS4 aparecia na mesma posição do XML.
    static MenuLevel menuDasCapas = ROOT;
    if (menuDasCapas != menuAtual) {
        menuDasCapas = menuAtual;
        initCoverCache();
        clearCoverCache();
    }
    int tema = temaAtual();
    if (menuAtual == MENU_EMULADOR) {
        extern const char* bibliotecaSubtitulo();
        extern const char* bibliotecaTituloPainel();
        extern int bibliotecaAba();
        extern bool showOpcoes;
        extern const char* listaOpcoes[150];
        extern int totalOpcoes, selOpcao;
        tema = TEMA_BIBLIOTECA;
        cena.tipo = CENA_BIBLIOTECA;
        cena.subtitulo = bibliotecaSubtitulo();
        cena.aba = bibliotecaAba();
        extern unsigned char* bibliotecaPrevia(int* w, int* h);
        cena.previa = bibliotecaPrevia(&cena.previaW, &cena.previaH);
        if (showOpcoes) {
            cena.opcoes = listaOpcoes;
            cena.totalOpcoes = totalOpcoes;
            cena.selOpcao = selOpcao;
            cena.tituloPainel = bibliotecaTituloPainel();
        }
    }
    if (cena.tipo == CENA_JOGOS || cena.tipo == CENA_BIBLIOTECA) {
        initCoverCache();
        cleanOldCache(sel > 6 ? sel - 6 : 0, sel + 10);
    }
    temaDesenhar(p, tema, &cena);
}
