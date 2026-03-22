#include "menu_grafico.h"
#include "menu.h"
#include "graphics.h"
#include "bloco_de_notas.h" 
#include "menu_audio.h" 
#include "menu_upload.h" 
#include "baixar.h" 
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <dirent.h>
#include "stb_image.h"

extern bool editMode; extern int editTarget; extern int editType; extern bool showOpcoes; extern int selOpcao; extern char pathExplorar[256]; extern bool marcados[3000]; extern const char* listaOpcoes[10]; extern char bufferTecladoC[128]; extern unsigned char* imgPreview;
extern unsigned char* defaultArtwork1; extern unsigned char* defaultArtwork2; extern int wDef1, hDef1; extern int wDef2, hDef2;

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH; // AS 6 VARIÁVEIS DA LISTA
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark, backX, backY, backW, backH; extern int wP, hP;

extern bool visualizandoMidiaImagem; extern unsigned char* imgMidia; extern int wM, hM; extern float zoomMidia; extern bool fullscreenMidia;
extern bool visualizandoMidiaTexto; extern char* textoMidiaBuffer; extern char* linhasTexto[5000]; extern int totalLinhasTexto; extern int textoMidiaScroll;
extern bool painelDuplo; extern int painelAtivo; extern char nomesEsq[3000][64]; extern bool marcadosEsq[3000]; extern char pathExplorarEsq[256]; extern int selEsq; extern int totalItensEsq; extern MenuLevel menuAtualEsq; extern int offEsq;

uint32_t getSysColor(int index) {
    uint32_t sysColors[] = { 0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000, 0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933, 0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF };
    if (index < 0 || index > 14) return sysColors[0]; return sysColors[index];
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
    static char nomeItemAnterior[128] = ""; static unsigned char* imgBgDinamico = NULL; static int dynBgW = 0, dynBgH = 0, dynBgC = 0; static unsigned char* imgCapaDinamica = NULL; static int dynCapaW = 0, dynCapaH = 0, dynCapaC = 0; static unsigned char* imgDiscoDinamico = NULL; static int dynDiscoW = 0, dynDiscoH = 0, dynDiscoC = 0;

    if (menuAtual == SCRAPER_LIST) {
        if (strcmp(nomes[sel], ultimoJogoCarregado) != 0) { char cp[512]; snprintf(cp, sizeof(cp), "/data/HyperNeiva/baixado/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]); FILE* fEx = fopen(cp, "rb"); if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); } else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } } strncpy(ultimoJogoCarregado, nomes[sel], 63); ultimoJogoCarregado[63] = '\0'; }
    }
    else if (strcmp(nomeItemAnterior, nomes[sel]) != 0) {
        strcpy(nomeItemAnterior, nomes[sel]); if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; } if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; } if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
        if (strlen(nomeItemAnterior) > 0) { imgBgDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Background", nomeItemAnterior, &dynBgW, &dynBgH, &dynBgC); imgCapaDinamica = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork1", nomeItemAnterior, &dynCapaW, &dynCapaH, &dynCapaC); imgDiscoDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork2", nomeItemAnterior, &dynDiscoW, &dynDiscoH, &dynDiscoC); }
    }

    if (menuAtual != MENU_NOTEPAD && imgBgDinamico) desenharRedimensionado(p, imgBgDinamico, dynBgW, dynBgH, 1920, 1080, 0, 0);
    if (visualizandoMidiaTexto && textoMidiaBuffer) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515; for (int by = 0; by < 80; by++) for (int bx = 0; bx < 1920; bx++) p[by * 1920 + bx] = 0xFF303030; desenharTexto(p, "LEITOR DE ARQUIVOS", 35, 50, 25, 0xFF00AAFF);
        int maxLinhasVisiveis = 23; for (int i = 0; i < maxLinhasVisiveis; i++) { int indiceDaLinha = textoMidiaScroll + i; if (indiceDaLinha < totalLinhasTexto && linhasTexto[indiceDaLinha] != NULL) desenharTexto(p, linhasTexto[indiceDaLinha], 30, 50, 120 + (i * 40), 0xFFDDDDDD); }
        for (int by = 0; by < 60; by++) for (int bx = 0; bx < 1920; bx++) p[(1020 + by) * 1920 + bx] = 0xFF222222; char rodape[128]; sprintf(rodape, "[Setas] Rolar   |   [O] Voltar   |   Linha: %d / %d", textoMidiaScroll, totalLinhasTexto); desenharTexto(p, rodape, 25, 50, 1035, 0xFF00AAFF); return;
    }

    if (visualizandoMidiaImagem && imgMidia) {
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF000000; float propW = 1920.0f / wM; float propH = 1080.0f / hM; float propMax = (propW < propH) ? propW : propH; int drawW, drawH;
        if (fullscreenMidia) { drawW = (int)(wM * propMax); drawH = (int)(hM * propMax); }
        else { drawW = (int)(wM * zoomMidia); drawH = (int)(hM * zoomMidia); if (drawW > 1920 || drawH > 1080) { drawW = (int)(wM * propMax); drawH = (int)(hM * propMax); zoomMidia = propMax; } }
        int posX = (1920 - drawW) / 2; int posY = (1080 - drawH) / 2; desenharRedimensionado(p, imgMidia, wM, hM, drawW, drawH, posX, posY);
        for (int by = 0; by < 130; by++) for (int bx = 0; bx < 400; bx++) { int pxX = 1480 + bx; int pyY = 930 + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = 0xAA000000; }
        desenharTexto(p, "[X] Tela Cheia", 25, 1500, 960, 0xFFFFFFFF); desenharTexto(p, "[Cima/Baixo] Zoom", 25, 1500, 1000, 0xFFFFFFFF); desenharTexto(p, "[O] Voltar", 25, 1500, 1040, 0xFFFFFFFF); return;
    }

    if (menuAtual != MENU_NOTEPAD) {
        int paineisDesenhar = painelDuplo ? 2 : 1; uint32_t corBasePainel = getSysColor(listBg);
        int curListX = (listOri == 0) ? listXV : listXH; // CALCULA POSICAO ATUAL BASEADO NA ORIENTACAO
        int curListY = (listOri == 0) ? listYV : listYH;
        int curListSpc = (listOri == 0) ? listSpcV : listSpcH;

        for (int painelIndex = 0; painelIndex < paineisDesenhar; painelIndex++) {
            int refPainel = painelDuplo ? painelIndex : 1;
            int sAtual = (refPainel == 0) ? selEsq : sel; int oAtual = (refPainel == 0) ? offEsq : off;
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
                if (isMarcado) corFundo = isPainelAtivo ? getSysColor(listMark) : getSysColor(11);

                if (gIdx == sAtual) {
                    if (isPainelAtivo) { if (isMarcado) corFundo = getSysColor(listHoverMark); else corFundo = getSysColor(10); corTexto = 0xFF000000; }
                    else corFundo = 0xAA555555;
                }
                for (int by = 0; by < listH; by++) for (int bx = 0; bx < larguraItem; bx++) { int pxX = currentX + bx; int pyY = currentY + by; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = corFundo; }
                desenharTexto(p, nItems[gIdx], fontTam, currentX + 20, currentY + (listH / 4), corTexto);
            }
        }
    }

    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) renderizarNotepad(p);
    if (menuAtual == SCRAPER_LIST && imgPreview) desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
    else if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
        if (!painelDuplo) { char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, bread, 30, cX, 1020, 0xFFFFFFFF); }
        else { char breadEsq[300]; sprintf(breadEsq, "ESQ: %s", pathExplorarEsq); desenharTexto(p, breadEsq, 25, capaX, 1020, (painelAtivo == 0) ? 0xFF00AAFF : 0xFFAAAAAA); char breadDir[300]; sprintf(breadDir, "DIR: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, breadDir, 25, cX, 1020, (painelAtivo == 1) ? 0xFF00AAFF : 0xFFAAAAAA); }
    }
    else {
        bool isEditingBar = ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 4); bool isEditingAudio = ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 5); bool isEditingUp = ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 6);

        if (isEditingBar) {
            int bX = barX; int bY = barY; int bW = barW; int bH = barH;
            for (int y = bY; y < bY + bH; y++) for (int x = bX; x < bX + bW; x++) if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barBg);
            int fill = bW / 2; for (int y = bY; y < bY + bH; y++) for (int x = bX; x < bX + fill; x++) if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barFill);
            desenharTexto(p, "50%   -   1 / 1", 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
        }
        else if (isEditingAudio) {
            for (int my = 0; my < audioH; my++) for (int mx = 0; mx < audioW; mx++) { int pxX = audioX + mx; int pyY = audioY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(listBg); }
            desenharTexto(p, "PLAY / PAUSE", fontTam, audioX + 20, audioY + 50, 0xFFFFFF00); desenharTexto(p, "PARAR", fontTam, audioX + 20, audioY + 95, 0xFFFFFFFF);
        }
        else if (isEditingUp) {
            for (int my = 0; my < upH; my++) for (int mx = 0; mx < upW; mx++) { int pxX = upX + mx; int pyY = upY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(listBg); }
            desenharTexto(p, "Selecionar", fontTam, upX + 20, upY + 50, 0xFFFFFF00); desenharTexto(p, "Selecionar Tudo", fontTam, upX + 20, upY + 95, 0xFFFFFFFF);
        }
        else {
            if (imgCapaDinamica) desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY); else if (menuAtual == JOGAR_XML || (editMode && editTarget != 4 && editTarget != 5 && editTarget != 6)) { if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }
            if (imgDiscoDinamico) desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); else if (menuAtual == JOGAR_XML || (editMode && editTarget != 4 && editTarget != 5 && editTarget != 6)) { if (defaultArtwork2) desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
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
        else if (editTarget == 6) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
        else if (editTarget == 8) { tX = &msgX; tY = &msgY; tW = &msgTam; tH = &msgTam; }
        else { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }

        if (editTarget == 9) sprintf(txtPos, "MODO EDICAO - CORES DO EXPLORAR (USE SETAS ESQ/DIR)");
        else if (editType == 3) sprintf(txtPos, "MODO EDICAO - COR DE FUNDO (USE SETAS ESQ/DIR)");
        else if (editType == 8) sprintf(txtPos, "MODO EDICAO - COR PREENCHIMENTO (USE SETAS ESQ/DIR)");
        else if (editType == 4) sprintf(txtPos, "MODO EDICAO - ESPACAMENTO: %d", (listOri == 0) ? listSpcV : listSpcH);
        else if (editType == 5) sprintf(txtPos, "MODO EDICAO - ORIENTACAO: %s", listOri == 0 ? "VERTICAL" : "HORIZONTAL");
        else if (editTarget == 7) sprintf(txtPos, "MODO EDICAO - TAMANHO DA FONTE: %d", fontTam);
        else if (editTarget == 8) sprintf(txtPos, "MODO EDICAO - NOTIFICACOES: X: %d  |  Y: %d  |  TAMANHO: %d", msgX, msgY, msgTam);
        else sprintf(txtPos, "MODO EDICAO - X: %d  |  Y: %d  |  LARGURA: %d  |  ALTURA: %d", *tX, *tY, *tW, *tH);

        for (int by = 0; by < 40; by++) for (int bx = 0; bx < 1920; bx++) { int pyY = 1040 + by; if (pyY < 1080) p[pyY * 1920 + bx] = 0xAA000000; }
        desenharTexto(p, txtPos, 25, 50, 1045, 0xFF00FF00);
    }

    if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) {
        for (int my = 0; my < 500; my++) for (int mx = 0; mx < 350; mx++) { int pxX = discoX + mx; int pyY = discoY - 100 + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(listBg); }
        for (int i = 0; i < 10; i++) { uint32_t corOp = (i == selOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF; desenharTexto(p, listaOpcoes[i], fontTam, discoX + 20, discoY - 80 + (i * 45), corOp); }
    }

    desenharMenuAudio(p); desenharMenuUpload(p);

    if (msgTimer > 0) { desenharTexto(p, msgStatus, msgTam, msgX, msgY, 0xFFFFFFFF); msgTimer--; }
    else if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 8) { desenharTexto(p, "EXEMPLO DE NOTIFICACAO...", msgTam, msgX, msgY, 0xFF00FF00); }
}