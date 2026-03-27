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

extern int picX, picY, picW, picH;

extern unsigned char* imgPic1;
extern int wPic1, hPic1, cPic1;

extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;

extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;

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

static unsigned char* imgVidEdicao = NULL;
static int wVidE = 0, hVidE = 0, cVidE = 0;
static bool tentouVidE = false;

uint32_t getSysColor(int index) {
    uint32_t sysColors[] = { 0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000, 0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933, 0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF };
    if (index < 0 || index > 14) return sysColors[0];
    return sysColors[index];
}

static bool isTextoAnimado = false;

void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor) {
    int maxChars = (maxW - 40) / (fTam * 0.55f);
    if (maxChars < 1) maxChars = 1;

    int len = strlen(textoOriginal); char txtFinal[512];

    if (len > maxChars) {
        if (isTextoAnimado && fontScroll == 1) {
            static char ultimoTextoAnimado[512] = "";
            static int frameInicioAnim = 0;

            if (strcmp(ultimoTextoAnimado, textoOriginal) != 0) {
                strcpy(ultimoTextoAnimado, textoOriginal);
                frameInicioAnim = frameContadorGlobal;
            }

            int framesAtivos = frameContadorGlobal - frameInicioAnim;
            int delayInicio = 90;
            int vel = 6;
            int delayFim = 30;

            int excess = len - maxChars;
            int tempoAndando = excess * vel;
            int cicloTotal = delayInicio + tempoAndando + delayFim;

            int pos = framesAtivos % cicloTotal;
            int shift = 0;

            if (pos < delayInicio) { shift = 0; }
            else if (pos < delayInicio + tempoAndando) { shift = (pos - delayInicio) / vel; }
            else { shift = excess; }

            if (shift < 0) shift = 0;
            if (shift > excess) shift = excess;

            strncpy(txtFinal, textoOriginal + shift, maxChars);
            txtFinal[maxChars] = '\0';
        }
        else {
            if (maxChars > 3) { strncpy(txtFinal, textoOriginal, maxChars - 3); txtFinal[maxChars - 3] = '\0'; strcat(txtFinal, "..."); }
            else { strncpy(txtFinal, textoOriginal, maxChars); txtFinal[maxChars] = '\0'; }
        }
    }
    else { strcpy(txtFinal, textoOriginal); }

    int lenFinal = strlen(txtFinal); int pxWidth = lenFinal * (fTam * 0.55f);
    int posX = xBase + 20; if (fontAlign == 1) posX = xBase + (maxW / 2) - (pxWidth / 2); else if (fontAlign == 2) posX = xBase + maxW - pxWidth - 20;
    desenharTexto(p, txtFinal, fTam, posX, y, cor);

    isTextoAnimado = false;
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

    if (!tentouVidE) {
        imgVidEdicao = stbi_load("/user/appmeta/CUSA18879/pic1.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/app/CUSA18879/sce_sys/pic1.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/appmeta/CUSA18879/icon0.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/app/CUSA18879/sce_sys/icon0.png", &wVidE, &hVidE, &cVidE, 4);
        tentouVidE = true;
    }

    static char nomeItemAnterior[128] = ""; static unsigned char* imgBgDinamico = NULL; static int dynBgW = 0, dynBgH = 0, dynBgC = 0; static unsigned char* imgCapaDinamica = NULL; static int dynCapaW = 0, dynCapaH = 0, dynCapaC = 0; static unsigned char* imgDiscoDinamico = NULL; static int dynDiscoW = 0, dynDiscoH = 0, dynDiscoC = 0;

    if (menuAtual == SCRAPER_LIST) {
        extern int consoleAtual;
        if (strcmp(nomeItemAnterior, nomes[sel]) != 0) {
            char cp[512];
            snprintf(cp, sizeof(cp), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]);
            FILE* fEx = fopen(cp, "rb");
            if (fEx) {
                fclose(fEx);
                if (imgPreview) stbi_image_free(imgPreview);
                imgPreview = stbi_load(cp, &wP, &hP, &cP, 4);
            }
            else {
                if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
            }
            strncpy(nomeItemAnterior, nomes[sel], 63);
            nomeItemAnterior[63] = '\0';
        }
    }
    else if (strcmp(nomeItemAnterior, nomes[sel]) != 0 && menuAtual != MENU_JOGAR_PS4) {
        strcpy(nomeItemAnterior, nomes[sel]); if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; } if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; } if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
        if (strlen(nomeItemAnterior) > 0) { imgBgDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Background", nomeItemAnterior, &dynBgW, &dynBgH, &dynBgC); imgCapaDinamica = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork1", nomeItemAnterior, &dynCapaW, &dynCapaH, &dynCapaC); imgDiscoDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork2", nomeItemAnterior, &dynDiscoW, &dynDiscoH, &dynDiscoC); }
    }

    if (menuAtual != MENU_NOTEPAD && imgBgDinamico) { desenharRedimensionado(p, imgBgDinamico, dynBgW, dynBgH, 1920, 1080, 0, 0); }

    if (menuAtual != MENU_NOTEPAD && !visualizandoMidiaImagem && !visualizandoMidiaTexto) {
        desenharElementoAnimado(p);
    }

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

            for (int i = 0; i < 6; i++) {
                int gIdx = i + oAtual; if (gIdx >= tItens) break;
                int currentX = posX_Base + (i * stepX); int currentY = curListY + (i * stepY);
                bool isPainelAtivo = (!painelDuplo || painelAtivo == refPainel);
                uint32_t corFundo = isPainelAtivo ? corBasePainel : 0xAA111111; uint32_t corTexto = isPainelAtivo ? 0xFFFFFFFF : 0xFFAAAAAA;

                bool isMarcado = (mAtual == MENU_EXPLORAR || mAtual == MENU_BAIXAR_DROPBOX_LISTA || mAtual == MENU_BAIXAR_DROPBOX_UPLOAD) && mItems[gIdx];
                if (isMarcado) { corFundo = isPainelAtivo ? getSysColor(listMark) : getSysColor(11); }

                if (gIdx == sAtual) {
                    if (isPainelAtivo) {
                        if (isMarcado) corFundo = getSysColor(listHoverMark); else corFundo = getSysColor(10);
                        corTexto = 0xFF000000;
                        isTextoAnimado = true;
                    }
                    else corFundo = 0xAA555555;
                }
                for (int by = 0; by < listH; by++) { for (int bx = 0; bx < larguraItem; bx++) { int pxX = currentX + bx; int pyY = currentY + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = corFundo; } }

                desenharTextoAlinhado(p, nItems[gIdx], fontTam, currentX, currentY + (listH / 4), larguraItem, corTexto);
            }
        }
    }

    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) renderizarNotepad(p);

    // =========================================================
    // DESENHISTA INTELIGENTE DE SAVES
    // =========================================================
    bool isSavePath = false;
    if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
        char* pRef = (painelDuplo && painelAtivo == 0) ? pathExplorarEsq : pathExplorar;
        if (strstr(pRef, "savedata") || strstr(pRef, "SAVEDATA") || strstr(pRef, "apollo") || strstr(pRef, "exported")) {
            isSavePath = true;
        }
    }

    if (isSavePath && imgPreview) {
        desenharDiscoRedondo(p, imgPreview, wP, hP, discoW, discoH, discoX, discoY);
    }
    else if ((menuAtual == SCRAPER_LIST || menuAtual == MENU_JOGAR_PS4) && imgPreview) {
        if (menuAtual == MENU_JOGAR_PS4 && imgPic1) {
            desenharRedimensionado(p, imgPic1, wPic1, hPic1, picW, picH, picX, picY);
        }
        desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
    }

    else if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
        if (!painelDuplo) { char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, bread, 30, cX, 1020, 0xFFFFFFFF); }
        else { char breadEsq[300]; sprintf(breadEsq, "ESQ: %s", pathExplorarEsq); desenharTexto(p, breadEsq, 25, capaX, 1020, (painelAtivo == 0) ? 0xFF00AAFF : 0xFFAAAAAA); char breadDir[300]; sprintf(breadDir, "DIR: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, breadDir, 25, cX, 1020, (painelAtivo == 1) ? 0xFF00AAFF : 0xFFAAAAAA); }
    }
    else if (!editMode) {
        bool isEditingBar = ((menuAtual == MENU_EDIT_TARGET) && editTarget == 5);
        bool isEditingAudio = ((menuAtual == MENU_EDIT_TARGET) && editTarget == 6);
        bool isEditingUp = ((menuAtual == MENU_EDIT_TARGET) && (editTarget == 7 || editTarget == 10));

        if (menuAtual == MENU_EDIT_TARGET && editTarget == 3) {
            if (imgVidEdicao) desenharRedimensionado(p, imgVidEdicao, wVidE, hVidE, picW, picH, picX, picY);
            else if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, picW, picH, picX, picY);
        }
        else if (menuAtual == MENU_EDIT_TARGET && editTarget == 1) {
            if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY);
        }
        else if (menuAtual == MENU_EDIT_TARGET && editTarget == 2) {
            if (defaultArtwork2) desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY);
        }

        if (isEditingBar) {
            int bX = barX; int bY = barY; int bW = barW; int bH = barH;
            for (int y = bY; y < bY + bH; y++) { for (int x = bX; x < bX + bW; x++) { if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barBg); } }
            int fill = bW / 2; for (int y = bY; y < bY + bH; y++) { for (int x = bX; x < bX + fill; x++) { if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barFill); } }
            desenharTexto(p, "50%   -   1 / 1", 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
        }
        else if (isEditingAudio) {
            for (int my = 0; my < audioH; my++) { for (int mx = 0; mx < audioW; mx++) { int pxX = audioX + mx; int pyY = audioY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(listBg); } }
            int maxV = (audioH - 50) / 45; if (maxV < 1) maxV = 1;
            if (maxV > 0) desenharTextoAlinhado(p, "PLAY / PAUSE", fontTam, audioX, audioY + 50, audioW, 0xFFFFFF00); if (maxV > 1) desenharTextoAlinhado(p, "PARAR", fontTam, audioX, audioY + 95, audioW, 0xFFFFFFFF);
        }
        else if (isEditingUp) {
            for (int my = 0; my < upH; my++) { for (int mx = 0; mx < upW; mx++) { int pxX = upX + mx; int pyY = upY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(upBg); } }
            int maxV = (upH - 50) / 45; if (maxV < 1) maxV = 1;

            if (editTarget == 10) {
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
            else if (menuAtual == JOGAR_XML) { if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }

            if (imgDiscoDinamico) { desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); }
            else if (menuAtual == JOGAR_XML) { if (defaultArtwork2) desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
        }
    }

    if (editMode) {
        if (editTarget == 3) {
            if (imgVidEdicao) desenharRedimensionado(p, imgVidEdicao, wVidE, hVidE, picW, picH, picX, picY);
            else if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, picW, picH, picX, picY);
        }

        char txtPos[200]; int* tX, * tY, * tW, * tH;
        if (editTarget == 0) { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
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
        else if (editTarget == 15) { tX = &anim_posX; tY = &anim_posY; tW = &anim_colunas; tH = &anim_linhas; }
        else { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }

        if (editTarget == 10 && editType == 0) sprintf(txtPos, "MODO EDICAO - CORES DO EXPLORAR (USE SETAS ESQ/DIR)");
        else if (editType == 3) sprintf(txtPos, "MODO EDICAO - COR DE FUNDO (USE SETAS ESQ/DIR)");
        else if (editType == 8) sprintf(txtPos, "MODO EDICAO - COR PREENCHIMENTO (USE SETAS ESQ/DIR)");
        else if (editType == 4) sprintf(txtPos, "MODO EDICAO - ESPACAMENTO: %d", (listOri == 0) ? listSpcV : listSpcH);
        else if (editType == 5) sprintf(txtPos, "MODO EDICAO - ORIENTACAO: %s", listOri == 0 ? "VERTICAL" : "HORIZONTAL");
        else if (editType == 10) sprintf(txtPos, "MODO EDICAO - ALINHAMENTO: %s", fontAlign == 0 ? "ESQUERDA" : (fontAlign == 1 ? "CENTRO" : "DIREITA"));
        else if (editType == 11) sprintf(txtPos, "MODO EDICAO - LIMITES: %s", fontScroll == 0 ? "CORTAR (..)" : "ANIMACAO ROLAGEM");
        else if (editType == 12) { int stat = 0; if (editTarget == 11) stat = elem1On; else if (editTarget == 12) stat = ctrl1On; else if (editTarget == 13) stat = pont1On; sprintf(txtPos, "MODO EDICAO - LIGADO: %s (USE SETAS ESQ/DIR)", stat ? "SIM" : "NAO"); }
        else if (editType == 13) sprintf(txtPos, "MODO EDICAO - MODO PONTEIRO: %s (USE SETAS ESQ/DIR)", pont1Modo == 0 ? "ACOMPANHA" : "ESTATICO");
        else if (editType == 14) { const char* lds[] = { "ESQUERDA", "DIREITA", "CIMA", "BAIXO" }; sprintf(txtPos, "MODO EDICAO - LADO PONTEIRO: %s (USE SETAS)", lds[pont1Lado]); }
        else if (editType == 15) sprintf(txtPos, "MODO EDICAO - EFEITOS SONOROS: %s (USE SETAS)", sfxLigado ? "LIGADO" : "DESLIGADO");
        else if (editType == 16) sprintf(txtPos, "MODO EDICAO - VOLUME EFEITOS: %d%% (USE SETAS)", sfxVolume);
        else if (editType == 17) sprintf(txtPos, "MODO EDICAO - MENU OPCOES POSICAO: X:%d Y:%d", upX, upY);
        else if (editType == 18) sprintf(txtPos, "MODO EDICAO - MENU OPCOES TAMANHO: LARGURA:%d ALTURA:%d", upW, upH);
        else if (editType == 19) sprintf(txtPos, "MODO EDICAO - MENU OPCOES ESTICAR: LARGURA:%d", upW);
        else if (editType == 20) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR FUNDO (USE SETAS)");
        else if (editType == 21) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR DO TEXTO (USE SETAS)");
        else if (editType == 22) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR TEXTO SELECIONADO (USE SETAS)");

        else if (editType == 23) sprintf(txtPos, "MODO EDICAO - ANIMACAO: POSICAO X:%d Y:%d (USE SETAS)", anim_posX, anim_posY);
        else if (editType == 24) sprintf(txtPos, "MODO EDICAO - ANIMACAO: ESCALA: %.1f (USE CIMA/BAIXO)", anim_escala);
        else if (editType == 25) sprintf(txtPos, "MODO EDICAO - ANIMACAO: VELOCIDADE: %d (USE SETAS)", anim_velocidade);
        else if (editType == 26) sprintf(txtPos, "MODO EDICAO - ANIMACAO GRADE: COLUNAS:%d LINHAS:%d (USE SETAS)", anim_colunas, anim_linhas);
        else if (editType == 27) sprintf(txtPos, "MODO EDICAO - ANIMACAO: DESLOCAMENTO GLOBAL DA GRADE X:%d Y:%d (SETAS)", anim_offsetX, anim_offsetY);
        else if (editType == 28) sprintf(txtPos, "MODO EDICAO - ANIMACAO: LOOP - INICIO [%d] FIM [%d] (USE ESQ/DIR/UP/DOWN)", anim_frameInicial, anim_frameFinal);
        else if (editType == 29) sprintf(txtPos, "MODO TESTE UNITY - FRAME [%d] | OFFSET X:%d Y:%d | L1/R1: Muda Frame | SETAS: Move | TRIANGULO: Auto-Centro", anim_frameAtual, anim_frameOffsetX[anim_frameAtual], anim_frameOffsetY[anim_frameAtual]);
        else if (editType == 30) sprintf(txtPos, "MODO EDICAO - ANIMACAO CONTINUA ATIVADA! (APERTE O PARA VOLTAR)");
        else if (editType == 31) sprintf(txtPos, "MODO EDICAO - ANIMACAO: VISUAL LIGADO: %s", anim_ativo ? "SIM" : "NAO");
        else if (editType == 32) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 1: RED (VERMELHO): %d", anim_keyR);
        else if (editType == 33) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 1: GREEN (VERDE): %d", anim_keyG);
        else if (editType == 34) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 1: BLUE (AZUL): %d", anim_keyB);
        else if (editType == 36) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: LIGADO: %s", anim_usarColorKey2 ? "SIM" : "NAO");
        else if (editType == 37) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: RED (VERMELHO): %d", anim_keyR2);
        else if (editType == 38) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: GREEN (VERDE): %d", anim_keyG2);
        else if (editType == 39) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: BLUE (AZUL): %d", anim_keyB2);
        else if (editType == 40) sprintf(txtPos, "MODO EDICAO - AUTO-CENTRO GLOBAL (SEMPRE LIGADO): %s", anim_autoCenter ? "LIGADO" : "DESLIGADO");

        else if (editTarget == 12) sprintf(txtPos, "EDICAO CTRL - POSICAO ATUAL: X:%d Y:%d", ctrl1X, ctrl1Y);
        else if (editTarget == 8) sprintf(txtPos, "MODO EDICAO - TAMANHO DA FONTE: %d", fontTam);
        else if (editTarget == 9) sprintf(txtPos, "MODO EDICAO - NOTIFICACOES: X: %d  |  Y: %d  |  TAMANHO: %d", msgX, msgY, msgTam);
        else sprintf(txtPos, "MODO EDICAO - X: %d  |  Y: %d  |  LARGURA: %d  |  ALTURA: %d", *tX, *tY, *tW, *tH);

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
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080)
                    p[pyY * 1920 + pxX] = getSysColor(upBg);
            }
        }

        int maxV = (upH - 50) / 45;
        if (maxV < 1) maxV = 1;

        if (selOpcao < offOpcao) offOpcao = selOpcao;
        if (selOpcao >= offOpcao + maxV) offOpcao = selOpcao - maxV + 1;

        for (int i = 0; i < maxV; i++) {
            int gIdx = i + offOpcao;
            if (gIdx >= totalOpcoes) break;

            uint32_t corOp = (gIdx == selOpcao) ? getSysColor(upTextSel) : getSysColor(upTextNorm);
            if (gIdx == selOpcao) isTextoAnimado = true;
            desenharTextoAlinhado(p, listaOpcoes[gIdx], fontTam, upX, upY + 50 + (i * 45), upW, corOp);
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

        int selScreenX = posX_B + ((sAt - oAt) * stepX);
        int selScreenY = curListY + ((sAt - oAt) * stepY);

        desenharElementos(p, selScreenX, selScreenY, wItem, listH);
    }

    if (msgTimer > 0) { desenharTexto(p, msgStatus, msgTam, msgX, msgY, 0xFFFFFFFF); msgTimer--; }
    else if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 9) { desenharTexto(p, "EXEMPLO DE NOTIFICACAO...", msgTam, msgX, msgY, 0xFF00FF00); }

    if (downloadEmSegundoPlano) {
        int bX = barX;
        int bY = barY;
        int bW = barW;
        int bH = barH;

        for (int y = bY; y < bY + bH; y++) {
            for (int x = bX; x < bX + bW; x++) {
                if (x >= 0 && x < 1920 && y >= 0 && y < 1080) {
                    p[y * 1920 + x] = getSysColor(barBg);
                }
            }
        }

        int fill = (int)(bW * progressoAtualDownload);
        if (fill > bW) fill = bW;
        if (fill < 0) fill = 0;

        for (int y = bY; y < bY + bH; y++) {
            for (int x = bX; x < bX + fill; x++) {
                if (x >= 0 && x < 1920 && y >= 0 && y < 1080) {
                    p[y * 1920 + x] = getSysColor(barFill);
                }
            }
        }

        char pctMsg[300];
        sprintf(pctMsg, "%s", msgDownloadBg);
        desenharTexto(p, pctMsg, 25, bX, bY - 35, 0xFFFFFFFF);

        int porcentagem = (int)(progressoAtualDownload * 100.0f);
        if (porcentagem > 100) porcentagem = 100;
        if (porcentagem < 0) porcentagem = 0;

        char textoFila[128];
        snprintf(textoFila, sizeof(textoFila), "%d%%   -   %d / %d", porcentagem, baixadosFilaSessao + 1, totalFilaSessao);
        desenharTexto(p, textoFila, 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
    }

    if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_UPLOAD || menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR) {
        char textoIP[128];
        sprintf(textoIP, "IP DO PS4: %s", ipDoPS4);
        desenharTexto(p, textoIP, 25, 1550, 1020, 0xFF00FF00);
    }
}