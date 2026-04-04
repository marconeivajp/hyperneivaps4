#include "menu_grafico.h"
#include "menu_grafico_cache_grafico.h"
#include "menu_grafico_render_texto.h"
#include "menu_grafico_visualizadores.h"
#include "menu_grafico_layout.h"

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
#include <math.h> 
#include "stb_image.h"

#include "elementos_animados_sprite_sheet.h"

extern int mapAcoes[50];
extern int sel;

extern bool editMode; extern int editTarget; extern int editType; extern bool showOpcoes; extern int selOpcao; extern char pathExplorar[256]; extern bool marcados[3000]; extern const char* listaOpcoes[150]; extern char bufferTecladoC[128]; extern unsigned char* imgPreview;
extern unsigned char* defaultArtwork1; extern unsigned char* defaultArtwork2; extern int wDef1, hDef1; extern int wDef2, hDef2;

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH;
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark, backX, backY, backW, backH; extern int wP, hP;
extern int fontAlign, fontScroll;

extern int picX, picY, picW, picH;
extern int vidX, vidY, vidW, vidH;

extern unsigned char* imgPic1;
extern int wPic1, hPic1, cPic1;

extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On;
extern int pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;

extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;

extern int listStyle, fontAnim, listCurvature, listZoomCentro;

extern bool gridAtivo;
extern int gridX, gridY, gridItemW, gridItemH, gridCols, gridLins, gridSpcX, gridSpcY;

int offOpcao = 0;
int frameContadorGlobal = 0;

extern bool visualizandoMidiaImagem; extern unsigned char* imgMidia; extern int wM, hM; extern float zoomMidia; extern bool fullscreenMidia;
extern bool visualizandoMidiaTexto; extern char* textoMidiaBuffer; extern char* linhasTexto[5000]; extern int totalLinhasTexto; extern int textoMidiaScroll;
extern bool painelDuplo; extern int painelAtivo; extern char nomesEsq[3000][64]; extern bool marcadosEsq[3000]; extern char pathExplorarEsq[256]; extern int selEsq; extern int totalItensEsq; extern MenuLevel menuAtualEsq; extern int offEsq;
extern bool emApolloSaves;

extern volatile bool downloadEmSegundoPlano;
extern volatile float progressoAtualDownload;
extern char msgDownloadBg[256];
extern volatile int totalFilaSessao;
extern volatile int baixadosFilaSessao;

extern int totalOpcoes;
extern char ipDoPS4[64];

char nomeItemAnterior[128] = ""; unsigned char* imgBgDinamico = NULL; int dynBgW = 0, dynBgH = 0, dynBgC = 0; unsigned char* imgCapaDinamica = NULL; int dynCapaW = 0, dynCapaH = 0, dynCapaC = 0; unsigned char* imgDiscoDinamico = NULL; int dynDiscoW = 0, dynDiscoH = 0, dynDiscoC = 0;

extern bool isFirstFrameUI;
extern int uiW[10];
extern int uiH[10];
extern unsigned char* uiTextures[10];
extern unsigned char* prevUiTextures[10];
extern int prevUiW[10];
extern int prevUiH[10];
extern int uiAnimFrame;
extern int lastTelaId;
extern int prevTelaIdForOut;

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
extern int interfaceTelaAlvo;
extern int interfaceElementoAlvo;

extern char ultimoJogoCarregado[64]; extern int consoleAtual;
extern stbtt_fontinfo font; extern int temF;

extern Console listaConsoles[5];

void desenharInterface(uint32_t* p) {
    frameContadorGlobal++;

    if (menuAtual == MENU_CONTROLE_TESTE) {
        extern void renderizarControleTeste(uint32_t * p);
        renderizarControleTeste(p);
        return;
    }
    if (menuAtual == MENU_INSTRUMENTOS) {
        extern void renderizarInstrumentos(uint32_t * p);
        renderizarInstrumentos(p);
        return;
    }
    if (menuAtual == MENU_INFORMACAO) {
        extern void renderizarInformacao(uint32_t * p);
        renderizarInformacao(p);
        return;
    }

    if (!tentouVidE) {
        imgVidEdicao = stbi_load("/user/appmeta/CUSA18879/pic1.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/app/CUSA18879/sce_sys/pic1.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/appmeta/CUSA18879/icon0.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/app/CUSA18879/sce_sys/icon0.png", &wVidE, &hVidE, &cVidE, 4);
        tentouVidE = true;
    }

    int refP_Top = painelDuplo ? painelAtivo : 1;
    int sAtivo = (refP_Top == 0) ? selEsq : sel;
    MenuLevel mAtivo = (refP_Top == 0) ? menuAtualEsq : menuAtual;
    char* nAtivo = (refP_Top == 0) ? nomesEsq[sAtivo] : nomes[sAtivo];

    if (nAtivo == NULL || strlen(nAtivo) == 0 || strcmp(nAtivo, "..") == 0 || strstr(nAtivo, "Desconhecido") != NULL || strstr(nAtivo, "esconhecido") != NULL) {
        if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
        if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
        if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
        strcpy(nomeItemAnterior, ""); strcpy(ultimoJogoCarregado, "");
    }
    else {
        if (mAtivo == SCRAPER_LIST) {
            if (strcmp(nAtivo, ultimoJogoCarregado) != 0) {
                char cp[512]; const char* cName = (consoleAtual >= 0 && consoleAtual < 5) ? listaConsoles[consoleAtual].nome : "Unknown";
                snprintf(cp, sizeof(cp), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", cName, nAtivo);
                FILE* fEx = fopen(cp, "rb");
                if (!fEx) { snprintf(cp, sizeof(cp), "/user/app/meta/%s/icon0.png", nAtivo); fEx = fopen(cp, "rb"); }
                if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); }
                else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } }
                strncpy(ultimoJogoCarregado, nAtivo, 63); ultimoJogoCarregado[63] = '\0';
            }
        }
        else if (strcmp(nomeItemAnterior, nAtivo) != 0 && menuAtual != MENU_JOGAR_PS4) {
            strcpy(nomeItemAnterior, nAtivo);
            if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
            if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
            if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }

            if (strlen(nomeItemAnterior) > 0) {
                imgBgDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Background", nomeItemAnterior, &dynBgW, &dynBgH, &dynBgC);
                imgCapaDinamica = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork1", nomeItemAnterior, &dynCapaW, &dynCapaH, &dynCapaC);
                imgDiscoDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork2", nomeItemAnterior, &dynDiscoW, &dynDiscoH, &dynDiscoC);
            }
        }
    }

    if (menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD && imgBgDinamico) {
        desenharRedimensionado(p, imgBgDinamico, dynBgW, dynBgH, 1920, 1080, 0, 0);
    }

    if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 4) {
        for (int bx = 0; bx < backW; bx++) {
            for (int by = 0; by < backH; by++) {
                if (bx == 0 || bx == backW - 1 || by == 0 || by == backH - 1) {
                    int pxx = backX + bx; int pyy = backY + by;
                    if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xFF00FF00;
                }
            }
        }
    }

    if (menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD && !visualizandoMidiaImagem && !visualizandoMidiaTexto) {
        desenharElementoAnimado(p);
    }

    if (renderizarLeitorMidia(p)) return;

    int telaId = 0;
    if (menuAtual == ROOT || menuAtual == MENU_TIPO_JOGO) telaId = 0;
    else if (menuAtual == SCRAPER_LIST || menuAtual == JOGAR_XML || menuAtual == MENU_JOGAR_PS4) telaId = 1;
    else if (menuAtual == MENU_NOTEPAD || menuAtual == MENU_AUDIO_OPCOES || menuAtual == MENU_MIDIA || menuAtual == MENU_EXTRA || menuAtual == MENU_INFORMACAO) telaId = 2;
    else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_LOJAS || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR || menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_UPLOAD || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES) telaId = 3;
    else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) telaId = 4;
    else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) telaId = 5;

    if (menuAtual == MENU_EDIT_TARGET && (editTarget >= 16 && editTarget <= 18)) { telaId = interfaceTelaAlvo; }

    for (int i = 0; i < 10; i++) {
        if (customUI[telaId][i].ativo && uiTextures[i] == NULL && strlen(customUI[telaId][i].caminho) > 0) {
            uiTextures[i] = stbi_load(customUI[telaId][i].caminho, &uiW[i], &uiH[i], NULL, 4);
        }
    }

    if (lastTelaId != telaId || isFirstFrameUI) {
        for (int i = 0; i < 10; i++) {
            if (prevUiTextures[i]) stbi_image_free(prevUiTextures[i]);
            prevUiTextures[i] = uiTextures[i]; prevUiW[i] = uiW[i]; prevUiH[i] = uiH[i]; uiTextures[i] = NULL;
        }
        prevTelaIdForOut = lastTelaId;
        for (int i = 0; i < 10; i++) {
            if (customUI[telaId][i].ativo && strlen(customUI[telaId][i].caminho) > 0) {
                uiTextures[i] = stbi_load(customUI[telaId][i].caminho, &uiW[i], &uiH[i], NULL, 4);
            }
        }
        lastTelaId = telaId; uiAnimFrame = 0; isFirstFrameUI = false;
    }

    renderizarCustomUI(p, telaId);

    bool esconderElementos = (visualizandoMidiaImagem || visualizandoMidiaTexto || menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD || menuAtual == MENU_CONTROLE_TESTE || menuAtual == MENU_INFORMACAO || menuAtual == MENU_INSTRUMENTOS);

    if (!esconderElementos) {
        int numPaineis = painelDuplo ? 2 : 1;
        for (int iterPainel = 0; iterPainel < numPaineis; iterPainel++) {
            int currentPainel = painelDuplo ? iterPainel : 1;
            desenharListas(p, currentPainel);
        }

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

        if (listStyle == 1 && !painelDuplo && menuAtual != MENU_EXPLORAR && menuAtual != MENU_EXPLORAR_HOME) {
            selScreenX = posX_B + (listOri == 1 ? (2 * stepX) : 0);
            selScreenY = curListY + (2 * stepY);
        }

        if (selScreenX > -100 && listStyle != 4 && listStyle != 3) {
            desenharElementos(p, selScreenX, selScreenY, wItem, listH);
        }

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
        else if (menuAtual == MENU_EXPLORAR || (painelDuplo && menuAtualEsq == MENU_EXPLORAR)) {
            if (!painelDuplo) { char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, bread, 30, cX, 1020, 0xFFFFFFFF); }
            else { char breadEsq[300]; sprintf(breadEsq, "ESQ: %s", pathExplorarEsq); desenharTexto(p, breadEsq, 25, capaX, 1020, (painelAtivo == 0) ? 0xFF00AAFF : 0xFFAAAAAA); char breadDir[300]; sprintf(breadDir, "DIR: %s", pathExplorar); int cX = (listOri == 0) ? listXV : listXH; desenharTexto(p, breadDir, 25, cX, 1020, (painelAtivo == 1) ? 0xFF00AAFF : 0xFFAAAAAA); }
        }
        else if (!editMode || editTarget == 0 || editTarget == 1 || editTarget == 2 || editTarget == 3) {
            bool isMenuXML = (menuAtual == JOGAR_XML || menuAtual == SCRAPER_LIST);
            bool isMenuPS4 = (menuAtual == MENU_JOGAR_PS4);
            bool isEditingCapaCD = ((menuAtual == MENU_EDIT_TARGET || editMode) && (editTarget == 1 || editTarget == 2 || editTarget == 3));

            int currentRenderStyle = listStyle;
            if ((currentRenderStyle == 3 || currentRenderStyle == 4 || currentRenderStyle == 5) && !isMenuXML && !isMenuPS4) currentRenderStyle = 1;

            if (isMenuXML || isMenuPS4 || isEditingCapaCD) {
                if ((currentRenderStyle != 4 && currentRenderStyle != 5 && currentRenderStyle != 3) || (!isMenuXML && !isMenuPS4 && editTarget != 1)) {
                    if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 3) {
                        if (imgVidEdicao) desenharRedimensionado(p, imgVidEdicao, wVidE, hVidE, picW, picH, picX, picY);
                        else if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, picW, picH, picX, picY);
                    }
                    else if (isMenuPS4 || isMenuXML) {
                        if (imgPic1) desenharRedimensionado(p, imgPic1, wPic1, hPic1, picW, picH, picX, picY);
                    }

                    if (menuAtual == JOGAR_XML || isEditingCapaCD || isMenuPS4) {
                        if (imgCapaDinamica && !isMenuPS4 && !painelDuplo) { desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY); }
                        else if (imgPreview && !painelDuplo) { desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY); }
                        else if (defaultArtwork1 && !painelDuplo) { desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }

                        if (imgDiscoDinamico && !isMenuPS4 && !painelDuplo) { desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); }
                        else if (defaultArtwork2 && !painelDuplo) { desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
                    }
                }
            }
        }
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
            bool isSel = (gIdx == selOpcao);

            desenharTextoAlinhadoAnimado(p, listaOpcoes[gIdx], fontTam, upX, upY + 50 + (i * 45), upW, corOp, isSel);
        }
    }

    desenharMenuAudio(p);
    desenharMenuUpload(p);

    if (downloadEmSegundoPlano) {
        int bX = barX; int bY = barY; int bW = barW; int bH = barH;

        for (int y = bY; y < bY + bH; y++) {
            for (int x = bX; x < bX + bW; x++) {
                if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barBg);
            }
        }

        int fill = (int)(bW * progressoAtualDownload);
        if (fill > bW) fill = bW;
        if (fill < 0) fill = 0;

        for (int y = bY; y < bY + bH; y++) {
            for (int x = bX; x < bX + fill; x++) {
                if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = getSysColor(barFill);
            }
        }

        char pctMsg[300]; sprintf(pctMsg, "%s", msgDownloadBg);
        desenharTexto(p, pctMsg, 25, bX, bY - 35, 0xFFFFFFFF);

        int porcentagem = (int)(progressoAtualDownload * 100.0f);
        if (porcentagem > 100) porcentagem = 100;
        if (porcentagem < 0) porcentagem = 0;

        char textoFila[128]; snprintf(textoFila, sizeof(textoFila), "%d%%   -   %d / %d", porcentagem, baixadosFilaSessao + 1, totalFilaSessao);
        desenharTexto(p, textoFila, 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
    }

    if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_UPLOAD || menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR) {
        char textoIP[128]; sprintf(textoIP, "IP DO PS4: %s", ipDoPS4);
        desenharTexto(p, textoIP, 25, 1550, 1020, 0xFF00FF00);
    }

    if (editMode || menuAtual == MENU_EDIT_TARGET) {
        desenharRodapeEdicao(p);
    }

    if (msgTimer > 0) { desenharTexto(p, msgStatus, msgTam, msgX, msgY, 0xFFFFFFFF); msgTimer--; }
    else if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 9) { desenharTexto(p, "EXEMPLO DE NOTIFICACAO...", msgTam, msgX, msgY, 0xFF00FF00); }
}