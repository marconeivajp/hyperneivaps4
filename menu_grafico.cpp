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
#include "ImeDialog.h"
#include "CommonDialog.h"

extern int cursX, cursY, cursW, cursH;
extern void desenharElementos(uint32_t* p, int selX, int selY, int selW, int selH);

extern void renderizarControleTeste(uint32_t* p);
extern void renderizarInstrumentos(uint32_t* p);
extern void renderizarInformacao(uint32_t* p);

extern int mapAcoes[50];
extern int sel, off;

extern bool editMode; extern int editTarget; extern int editType; extern bool showOpcoes; extern int selOpcao; extern char pathExplorar[256]; extern bool marcados[3000]; extern const char* listaOpcoes[150]; extern char bufferTecladoC[128]; extern unsigned char* imgPreview;
extern unsigned char* defaultArtwork1; extern unsigned char* defaultArtwork2; extern int wDef1, hDef1; extern int wDef2, hDef2;

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH;
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark, backX, backY, backW, backH; extern int wP, hP, cP;
extern int fontAlign, fontScroll;

extern int picX, picY, picW, picH;
extern int vidX, vidY, vidW, vidH;

extern unsigned char* imgPic1;
extern int wPic1, hPic1, cPic1;

extern int elem1X, elem1Y, elem1W, elem1H, elem1On;
extern int ctrl1X, ctrl1Y, ctrl1W, ctrl1On;
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

char nomeItemAnterior[128] = ""; 
unsigned char* imgBgDinamico = NULL; int dynBgW = 0, dynBgH = 0, dynBgC = 0; 
unsigned char* imgCapaDinamica = NULL; int dynCapaW = 0, dynCapaH = 0, dynCapaC = 0; 
unsigned char* imgDiscoDinamico = NULL; int dynDiscoW = 0, dynDiscoH = 0, dynDiscoC = 0;

extern bool emSubmenuDropbox;
extern bool emSubmenuFTP;

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

unsigned char* imgVidEdicao = NULL; int wVidE, hVidE, cVidE; bool tentouVidE = false;
unsigned char* imgCapaEdicao = NULL; int wCapaE, hCapaE, cCapaE; bool tentouCapaE = false;
unsigned char* imgDiscoEdicao = NULL; int wDiscoE, hDiscoE, cDiscoE; bool tentouDiscoE = false;
extern char ultimoJogoCarregado[64]; extern int consoleAtual;
extern Console listaConsoles[5];

extern bool videoRodando, video_minimizado;
extern void verificarTrocaDeVideo(), atualizarVideoFFmpeg(uint32_t* p);
extern bool renderizarLeitorMidia(uint32_t* p);

void desenharInterface(uint32_t* p) {
    frameContadorGlobal++;

    if (menuAtual == MENU_CONTROLE_TESTE) {
        renderizarControleTeste(p);
        return;
    }
    if (menuAtual == MENU_INSTRUMENTOS) {
        renderizarInstrumentos(p);
        return;
    }
    if (menuAtual == MENU_INFORMACAO) {
        renderizarInformacao(p);
        return;
    }

    if (renderizarLeitorMidia(p)) return;

    // Logic for Previews in Edit Mode
    if (!tentouVidE) {
        imgVidEdicao = stbi_load("/user/appmeta/CUSA18879/pic1.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/app/CUSA18879/sce_sys/pic1.png", &wVidE, &hVidE, &cVidE, 4);
        if (!imgVidEdicao) imgVidEdicao = stbi_load("/user/appmeta/CUSA18879/icon0.png", &wVidE, &hVidE, &cVidE, 4);
        tentouVidE = true;
    }
    if (!tentouCapaE) {
        imgCapaEdicao = stbi_load("/data/HyperNeiva/configuracao/imagens/0_Defalt_Artwork1.png", &wCapaE, &hCapaE, &cCapaE, 4);
        if (!imgCapaEdicao) imgCapaEdicao = stbi_load("/app0/sce_sys/icon0.png", &wCapaE, &hCapaE, &cCapaE, 4);
        if (!imgCapaEdicao) imgCapaEdicao = stbi_load("/user/appmeta/CUSA18879/icon0.png", &wCapaE, &hCapaE, &cCapaE, 4);
        if (!imgCapaEdicao) imgCapaEdicao = stbi_load("/user/app/CUSA18879/sce_sys/icon0.png", &wCapaE, &hCapaE, &cCapaE, 4);
        if (!imgCapaEdicao) imgCapaEdicao = stbi_load("/app0/assets/images/0_Defalt_Background.png", &wCapaE, &hCapaE, &cCapaE, 4);
        tentouCapaE = true;
    }
    if (!tentouDiscoE) {
        imgDiscoEdicao = stbi_load("/data/HyperNeiva/configuracao/imagens/0_Defalt_Artwork2.png", &wDiscoE, &hDiscoE, &cDiscoE, 4);
        if (!imgDiscoEdicao) imgDiscoEdicao = stbi_load("/app0/assets/images/disco1.png", &wDiscoE, &hDiscoE, &cDiscoE, 4);
        if (!imgDiscoEdicao) imgDiscoEdicao = stbi_load("/app0/assets/images/disco.png", &wDiscoE, &hDiscoE, &cDiscoE, 4);
        if (!imgDiscoEdicao) imgDiscoEdicao = stbi_load("/user/appmeta/CUSA18879/icon0.png", &wDiscoE, &hDiscoE, &cDiscoE, 4);
        tentouDiscoE = true;
    }

    // Dynamic Media Loading Logic
    int refP_Top = painelDuplo ? painelAtivo : 1;
    int sAtivo = (refP_Top == 0) ? selEsq : sel;
    MenuLevel mAtivo = (refP_Top == 0) ? menuAtualEsq : menuAtual;
    char* nAtivo = (refP_Top == 0) ? nomesEsq[sAtivo] : nomes[sAtivo];

    if (!editMode && (nAtivo == NULL || strlen(nAtivo) == 0 || strcmp(nAtivo, "..") == 0 || strstr(nAtivo, "Desconhecido") != NULL || strstr(nAtivo, "esconhecido") != NULL)) {
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
        else if (strcmp(nomeItemAnterior, nAtivo) != 0) {
            strcpy(nomeItemAnterior, nAtivo);
            if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
            if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
            if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
            if (imgPic1) { stbi_image_free(imgPic1); imgPic1 = NULL; } 

            if (strlen(nomeItemAnterior) > 0) {
                if (menuAtual == MENU_JOGAR_PS4) {
                    // LOGICA PS4: Busca ícone e fundo nativo (Diversas tentativas)
                    char cp[512];
                    snprintf(cp, sizeof(cp), "/user/appmeta/%s/icon0.png", nomeItemAnterior);
                    imgCapaDinamica = stbi_load(cp, &dynCapaW, &dynCapaH, &dynCapaC, 4);
                    if (!imgCapaDinamica) {
                        snprintf(cp, sizeof(cp), "/user/app/meta/%s/icon0.png", nomeItemAnterior);
                        imgCapaDinamica = stbi_load(cp, &dynCapaW, &dynCapaH, &dynCapaC, 4);
                    }
                    if (!imgCapaDinamica) {
                         snprintf(cp, sizeof(cp), "/user/app/%s/sce_sys/icon0.png", nomeItemAnterior);
                         imgCapaDinamica = stbi_load(cp, &dynCapaW, &dynCapaH, &dynCapaC, 4);
                    }
                    
                    snprintf(cp, sizeof(cp), "/user/appmeta/%s/pic1.png", nomeItemAnterior);
                    imgPic1 = stbi_load(cp, &wPic1, &hPic1, &cPic1, 4);
                    if (!imgPic1) {
                         snprintf(cp, sizeof(cp), "/user/app/%s/sce_sys/pic1.png", nomeItemAnterior);
                         imgPic1 = stbi_load(cp, &wPic1, &hPic1, &cPic1, 4);
                    }
                } else {
                    imgBgDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Background", nomeItemAnterior, &dynBgW, &dynBgH, &dynBgC);
                    imgCapaDinamica = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork1", nomeItemAnterior, &dynCapaW, &dynCapaH, &dynCapaC);
                    imgDiscoDinamico = carregarMediaCaseInsensitive("/data/HyperNeiva/midia/imagens/Games/Artwork2", nomeItemAnterior, &dynDiscoW, &dynDiscoH, &dynDiscoC);
                }
            }
        }
    }

    // Render Background
    if (menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD && imgBgDinamico) {
        desenharRedimensionado(p, imgBgDinamico, dynBgW, dynBgH, 1920, 1080, 0, 0);
    }

    // Sprite Animation (Restored)
    if (menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD && !visualizandoMidiaImagem && !visualizandoMidiaTexto) {
        desenharElementoAnimado(p);
    }

    // Custom UI Logic (v47)
    int telaId = 0;
    if (menuAtual == ROOT || menuAtual == MENU_TIPO_JOGO) telaId = 0;
    else if (menuAtual == SCRAPER_LIST || menuAtual == JOGAR_XML || menuAtual == MENU_JOGAR_PS4) telaId = 1;
    else if (menuAtual == MENU_NOTEPAD || menuAtual == MENU_AUDIO_OPCOES || menuAtual == MENU_MIDIA || menuAtual == MENU_EXTRA || menuAtual == MENU_INFORMACAO) telaId = 2;
    else if (menuAtual == MENU_BAIXAR) telaId = 3;
    else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) telaId = 4;
    else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) telaId = 5;

    if (menuAtual == MENU_EDIT_TARGET && (editTarget >= 16 && editTarget <= 18)) { telaId = interfaceTelaAlvo; }
    renderizarCustomUI(p, telaId);

    // Visibility Check
    bool esconderElementos = (visualizandoMidiaImagem || visualizandoMidiaTexto || menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD || menuAtual == MENU_CONTROLE_TESTE || menuAtual == MENU_INFORMACAO || menuAtual == MENU_INSTRUMENTOS);
    
    // NO MODO EDIÇÃO, NÃO ESCONDER LISTAS E PONTEIROS SE ESTIVERMOS EDITANDO ELES OU O EXPLORAR
    if (editMode && (editTarget == 0 || editTarget == 10 || editTarget == 13)) esconderElementos = false;

    if (!esconderElementos) {
        // Draw Lists
        if (!painelDuplo) {
            desenharListas(p, 1);
        } else {
            desenharListas(p, 0);
            desenharListas(p, 1);
        }

        // Draw selection elements (pointers)
        desenharElementos(p, cursX, cursY, cursW, cursH);
    }

    // Draw Covers, Discs and previews (Fixed visibility for Edit Mode - AS PER REFERENCE)
    bool isEditingCapaCD = ((menuAtual == MENU_EDIT_TARGET || editMode) && (editTarget == 1 || editTarget == 2 || editTarget == 3));
    bool isMenuPS4 = (menuAtual == MENU_JOGAR_PS4);
    bool isMenuXML = (menuAtual == JOGAR_XML || menuAtual == SCRAPER_LIST);

    if (!esconderElementos || editMode) {
        // --- VIDEO PREVIEW (PIC / CAPA DE VIDEO) ---
        if (editMode && editTarget == 3) {
            if (imgVidEdicao) desenharRedimensionado(p, imgVidEdicao, wVidE, hVidE, picW, picH, picX, picY);
            else if (defaultArtwork1) desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, picW, picH, picX, picY);
        } 
        else if (isMenuPS4 || isMenuXML || editMode) {
            if (imgPic1) desenharRedimensionado(p, imgPic1, wPic1, hPic1, picW, picH, picX, picY);
        }

        // --- COVERS (CAPA) ---
        if (editMode && editTarget == 1) {
            if (imgCapaDinamica) { desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY); }
            else if (imgPreview) { desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY); }
            else if (imgCapaEdicao) { desenharRedimensionado(p, imgCapaEdicao, wCapaE, hCapaE, capaW, capaH, capaX, capaY); }
            else if (defaultArtwork1) { desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }
        } 
        else if (isMenuXML || isMenuPS4 || editMode) {
            if (imgCapaDinamica) { desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY); }
            else if (imgPreview) { desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY); }
            else if (imgCapaEdicao) { desenharRedimensionado(p, imgCapaEdicao, wCapaE, hCapaE, capaW, capaH, capaX, capaY); }
            else if (defaultArtwork1) { desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }
        }

        // --- DISCS (DISCO) ---
        if (editMode && editTarget == 2) {
            if (imgDiscoDinamico) { desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); }
            else if (imgDiscoEdicao) { desenharDiscoRedondo(p, imgDiscoEdicao, wDiscoE, hDiscoE, discoW, discoH, discoX, discoY); }
            else if (defaultArtwork2) { desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
        } 
        else if (isMenuXML || isMenuPS4 || editMode) {
            if (imgDiscoDinamico) { desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); }
            else if (imgDiscoEdicao) { desenharDiscoRedondo(p, imgDiscoEdicao, wDiscoE, hDiscoE, discoW, discoH, discoX, discoY); }
            else if (defaultArtwork2) { desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
        }
    }

    // --- OVERLAYS DE UI (Path e IP) ---
    if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) {
        char txtPath[512]; snprintf(txtPath, sizeof(txtPath), "PATH: %s", pathExplorar);
        int wPath = medirLarguraTexto(txtPath, 28);
        desenharTexto(p, txtPath, 28, 1880 - wPath, 1000, 0xFFFFFFFF); // Branco denovo, conforme pedido
    }
    if ((menuAtual >= MENU_BAIXAR && menuAtual <= MENU_BAIXAR_LINK_DIRETO) || emSubmenuDropbox || emSubmenuFTP) {
        extern char ipDoPS4[64];
        char txtIP[128]; snprintf(txtIP, sizeof(txtIP), "PS4 IP: %s", ipDoPS4);
        desenharTexto(p, txtIP, 32, 50, 40, 0xFFFFFFFF); // Ajustado para ser visivel e premium
    }
     // --- MENUS ESPECIAIS (AUDIO, UPLOAD, EXPLORAR) ---
        desenharMenuAudio(p);
        desenharMenuUpload(p);

        if (editMode && editTarget == 10) {
            for (int my = 0; my < upH; my++) for (int mx = 0; mx < upW; mx++) {
                int pxX = upX + mx; int pyY = upY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(listBg);
            }
            desenharTextoAlinhado(p, "[EXPLORADOR]", fontTam, upX, upY + 10, upW, 0xFFFFFFFF);
            desenharTextoAlinhado(p, "  [Pasta de Exemplo]", fontTam, upX, upY + 55, upW, 0xFFFFFFFF);
            desenharTextoAlinhado(p, "  Arquivo_de_Teste.pkg", fontTam, upX, upY + 100, upW, 0xFFFFFFFF);
        }

        // --- OPTION MENUS (TRIANGLE) ---
        if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) {
            if (selOpcao >= totalOpcoes) selOpcao = 0; if (selOpcao < 0) selOpcao = totalOpcoes - 1;
            for (int my = 0; my < upH; my++) for (int mx = 0; mx < upW; mx++) { int pxX = upX + mx; int pyY = upY + my; if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) p[pyY * 1920 + pxX] = getSysColor(upBg); }
            int maxV = (upH - 50) / 45; if (maxV < 1) maxV = 1;
            if (selOpcao < offOpcao) offOpcao = selOpcao; if (selOpcao >= offOpcao + maxV) offOpcao = selOpcao - maxV + 1;
            for (int i = 0; i < maxV; i++) {
                int gIdx = i + offOpcao; if (gIdx >= totalOpcoes) break;
                uint32_t corOp = (gIdx == selOpcao) ? getSysColor(upTextSel) : getSysColor(upTextNorm);
                bool isSel = (gIdx == selOpcao);
                desenharTextoAlinhadoAnimado(p, listaOpcoes[gIdx], fontTam, upX, upY + 50 + (i * 45), upW, corOp, isSel);
            }
        }
    // Download Bar
    if (downloadEmSegundoPlano || (editMode && editTarget == 5)) {
        float prog = downloadEmSegundoPlano ? progressoAtualDownload : 0.5f;
        int fill = (int)(barW * prog); if (fill > barW) fill = barW; if (fill < 0) fill = 0;
        for (int y = barY; y < barY + barH; y++) for (int x = barX; x < barX + barW; x++) if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = (x < barX + fill) ? getSysColor(barFill) : getSysColor(barBg);
    }

    // FFmpeg Video Updates
    if (videoRodando && video_minimizado) {
        verificarTrocaDeVideo();
        atualizarVideoFFmpeg(p);
    }

    // Status Messages
    if (msgTimer > 0) { 
        extern uint32_t msgStatusColor;
        if (menuAtual == MENU_ERRO_CRITICO) {
            for (int y = 300; y < 850; y++) for (int x = 100; x < 1820; x++) { p[y * 1920 + x] = (p[y * 1920 + x] & 0x00FFFFFF) | 0xEE000000; if (y == 300 || y == 849 || x == 100 || x == 1819) p[y * 1920 + x] = msgStatusColor; }
            desenharTexto(p, "FALHA NO NUCLEO", 40, 600, 350, msgStatusColor);
            desenharTexto(p, msgStatus, 26, 130, 430, 0xFFFFFFFF);
        } else {
            desenharTexto(p, msgStatus, msgTam, msgX, msgY, msgStatusColor); 
            msgTimer--; 
        }
    }

    if (editMode) desenharRodapeEdicao(p);
}

void atualizarImePasta() {
    extern bool tecladoAtivo;
    extern int tecladoTipo;
    extern uint16_t* bufferTecladoW;
    extern char bufferTecladoC[128];

    if (tecladoAtivo) {
        OrbisDialogStatus status = sceImeDialogGetStatus();
        if (status == 2) { // ORBIS_DIALOG_STATUS_FINISHED
            OrbisDialogResult result;
            memset(&result, 0, sizeof(result));
            sceImeDialogGetResult(&result);
            int32_t buttonId = *(int32_t*)&result;

            if (buttonId == 0) { // User pressed OK
                for (int i = 0; i < 127; i++) {
                    bufferTecladoC[i] = (char)bufferTecladoW[i];
                    if (bufferTecladoW[i] == 0) break;
                }
                
                if (tecladoTipo == 10) { // RADIO_SEARCH
                    extern void buscarEstacoesRadio(const char* query, bool isPodcast);
                    buscarEstacoesRadio(bufferTecladoC, false);
                }
            }
            sceImeDialogTerm();
            tecladoAtivo = false;
        }
    }
}