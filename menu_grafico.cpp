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
static unsigned char* imgVidEdicao = NULL; static int wVidE = 0, hVidE = 0, cVidE = 0; static bool tentouVidE = false;

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

// =========================================================================
// O NOVO CACHE DE MEMORIA (IMPEDE O PS4 DE TRAVAR)
// =========================================================================
#define MAX_GRID_CACHE 64 
struct CoverCache {
    int index;
    unsigned char* img;
    int w, h, c;
};
static CoverCache coverCache[MAX_GRID_CACHE];
static bool cacheIniciado = false;

void initCoverCache() {
    if (cacheIniciado) return;
    for (int i = 0; i < MAX_GRID_CACHE; i++) { coverCache[i].index = -1; coverCache[i].img = NULL; }
    cacheIniciado = true;
}

void clearCoverCache() {
    for (int i = 0; i < MAX_GRID_CACHE; i++) {
        if (coverCache[i].img) { stbi_image_free(coverCache[i].img); coverCache[i].img = NULL; }
        coverCache[i].index = -1;
    }
}

void cleanOldCache(int startIdx, int endIdx) {
    for (int i = 0; i < MAX_GRID_CACHE; i++) {
        if (coverCache[i].index != -1 && (coverCache[i].index < startIdx || coverCache[i].index > endIdx)) {
            stbi_image_free(coverCache[i].img);
            coverCache[i].img = NULL;
            coverCache[i].index = -1;
        }
    }
}

int getCachedImage(int targetIdx) {
    for (int i = 0; i < MAX_GRID_CACHE; i++) if (coverCache[i].index == targetIdx) return i;
    return -1;
}

int getFreeCacheSlot() {
    for (int i = 0; i < MAX_GRID_CACHE; i++) if (coverCache[i].index == -1) return i;
    return -1;
}

void limparCacheGrafico() {
    clearCoverCache();
    if (imgVidEdicao) { stbi_image_free(imgVidEdicao); imgVidEdicao = NULL; }
    if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
    if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
    if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
    strcpy(nomeItemAnterior, ""); tentouVidE = false;
    for (int i = 0; i < 10; i++) {
        if (uiTextures[i]) { stbi_image_free(uiTextures[i]); uiTextures[i] = NULL; }
        if (prevUiTextures[i]) { stbi_image_free(prevUiTextures[i]); prevUiTextures[i] = NULL; }
    }
    lastTelaId = 0; isFirstFrameUI = true;
    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
    if (imgPic1) { stbi_image_free(imgPic1); imgPic1 = NULL; }
    if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; visualizandoMidiaImagem = false; }
}

uint32_t getSysColor(int index) {
    uint32_t sysColors[] = { 0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000, 0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933, 0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF };
    if (index < 0 || index > 14) return sysColors[0];
    return sysColors[index];
}

static int advance_utf8(const char* str, int num_chars) {
    int idx = 0;
    for (int i = 0; i < num_chars; i++) {
        if (str[idx] == '\0') break;
        idx++;
        while (str[idx] != '\0' && (str[idx] & 0xC0) == 0x80) {
            idx++;
        }
    }
    return idx;
}

static bool isTextoAnimado = false;

int medirLarguraTexto(const char* texto, int tam) {
    if (!temF || !texto) return 0; float s = stbtt_ScaleForPixelHeight(&font, (float)tam); int totalW = 0;
    for (int i = 0; texto[i]; ++i) { int adv, lsb; stbtt_GetCodepointHMetrics(&font, texto[i], &adv, &lsb); totalW += (int)(adv * s); } return totalW;
}

void desenharTextoClip(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor, int clipX, int clipW) {
    if (!temF || !texto || !pixels) return; float s = stbtt_ScaleForPixelHeight(&font, (float)tam); int asc; stbtt_GetFontVMetrics(&font, &asc, 0, 0); asc = (int)(asc * s); int curX = x; uint8_t baseAlpha = (cor >> 24) & 0xFF; if (baseAlpha < 10) return;
    for (int i = 0; texto[i]; ++i) {
        int adv, lsb, x0, y0, x1, y1; stbtt_GetCodepointHMetrics(&font, texto[i], &adv, &lsb); stbtt_GetCodepointBitmapBox(&font, texto[i], s, s, &x0, &y0, &x1, &y1); int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            unsigned char* b = (unsigned char*)malloc(w * h); stbtt_MakeCodepointBitmap(&font, b, w, h, w, s, s, texto[i]);
            for (int cy = 0; cy < h; ++cy) for (int cx = 0; cx < w; ++cx) {
                int pX = curX + x0 + cx; int pY = y + asc + y0 + cy;
                if (pX >= clipX && pX < (clipX + clipW) && pY >= 0 && pY < 1080 && pX >= 0 && pX < 1920) {
                    uint8_t alpha = b[cy * w + cx]; uint8_t finalAlpha = (alpha * baseAlpha) / 255; if (finalAlpha > 30) pixels[pY * 1920 + pX] = (finalAlpha << 24) | (cor & 0x00FFFFFF);
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
        desenharTextoClip(p, textoOriginal, fTam, posX, y, cor, xBase, maxW);
    }
    else {
        if (isSelected) {
            int excessPx = pxWidth - espacoDisponivel + 40;
            int cycle = 300;
            int pos = frameContadorGlobal % cycle;
            int pixelShift = 0;

            if (pos > 60 && pos <= 150) {
                float progress = (float)(pos - 60) / 90.0f;
                progress = progress * progress * (3.0f - 2.0f * progress);
                pixelShift = (int)(progress * excessPx);
            }
            else if (pos > 150 && pos <= 210) { pixelShift = excessPx; }
            else if (pos > 210) {
                float progress = (float)(pos - 210) / 90.0f;
                progress = progress * progress * (3.0f - 2.0f * progress);
                pixelShift = (int)((1.0f - progress) * excessPx);
            }
            desenharTextoClip(p, textoOriginal, fTam, posX - pixelShift, y, cor, xBase + 20, espacoDisponivel);
        }
        else {
            int maxChars = espacoDisponivel / (fTam * 0.55f);
            if (maxChars < 4) maxChars = 4;
            char txtFinal[512];
            strncpy(txtFinal, textoOriginal, maxChars - 3);
            txtFinal[maxChars - 3] = '\0';
            strcat(txtFinal, "...");
            desenharTexto(p, txtFinal, fTam, posX, y, cor);
        }
    }
}

void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor) {
    desenharTextoAlinhadoAnimado(p, textoOriginal, fTam, xBase, y, maxW, cor, false);
}

unsigned char* carregarMediaCaseInsensitive(const char* pastaPath, const char* nomeProcurado, int* w, int* h, int* c) {
    if (!pastaPath || !nomeProcurado || strlen(nomeProcurado) == 0 || strcmp(nomeProcurado, "..") == 0) return NULL;
    DIR* d = opendir(pastaPath); if (!d) return NULL; struct dirent* dir; char caminhoEncontrado[1024] = ""; bool achou = false;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.') continue; const char* dot = strrchr(dir->d_name, '.');
        if (dot) {
            int lenNome = dot - dir->d_name; char nomeBase[256]; if (lenNome >= sizeof(nomeBase)) lenNome = sizeof(nomeBase) - 1; strncpy(nomeBase, dir->d_name, lenNome); nomeBase[lenNome] = '\0';
            if (strcasecmp(nomeBase, nomeProcurado) == 0) { if (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) { snprintf(caminhoEncontrado, sizeof(caminhoEncontrado), "%s/%s", pastaPath, dir->d_name); achou = true; break; } }
        }
    } closedir(d); if (achou) return stbi_load(caminhoEncontrado, w, h, c, 4); return NULL;
}

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

    int refP = painelDuplo ? painelAtivo : 1;
    int sAtivo = (refP == 0) ? selEsq : sel;
    MenuLevel mAtivo = (refP == 0) ? menuAtualEsq : menuAtual;
    char* nAtivo = (refP == 0) ? nomesEsq[sAtivo] : nomes[sAtivo];

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

    if (menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD) {
        renderizarNotepad(p);
        return;
    }

    bool esconderElementos = (visualizandoMidiaImagem || visualizandoMidiaTexto || menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD || menuAtual == MENU_CONTROLE_TESTE || menuAtual == MENU_INFORMACAO || menuAtual == MENU_INSTRUMENTOS);

    if (!esconderElementos) {

        int refPainel = painelDuplo ? painelAtivo : 1;
        int sAtual = (refPainel == 0) ? selEsq : sel; int oAtual = (refPainel == 0) ? offEsq : off;
        int tItens = (refPainel == 0) ? totalItensEsq : totalItens; char (*nItems)[64] = (refPainel == 0) ? nomesEsq : nomes; bool* mItems = (refPainel == 0) ? marcadosEsq : marcados; MenuLevel mAtual = (refPainel == 0) ? menuAtualEsq : menuAtual;

        int currentRenderStyle = listStyle;
        bool isGameMenu = (mAtual == MENU_JOGAR_PS4 || mAtual == JOGAR_XML || mAtual == SCRAPER_LIST);

        if ((currentRenderStyle == 3 || currentRenderStyle == 4 || currentRenderStyle == 5) && !isGameMenu) currentRenderStyle = 1;

        uint32_t corBasePainel = getSysColor(listBg);
        int curListX = (listOri == 0) ? listXV : listXH; int curListY = (listOri == 0) ? listYV : listYH; int curListSpc = (listOri == 0) ? listSpcV : listSpcH;
        int larguraBarraDupla = 750; int posX_Base = (painelDuplo) ? ((refPainel == 0) ? capaX : curListX) : curListX; int larguraItem = (painelDuplo) ? larguraBarraDupla : listW;
        int stepX = (listOri == 1) ? curListSpc : 0; int stepY = (listOri == 0) ? curListSpc : 0;

        static float smoothSelDir = 0.0f; static float smoothSelEsq = 0.0f; static MenuLevel lastMenu2 = ROOT;
        if (lastMenu2 != menuAtual) { smoothSelDir = (float)sel; smoothSelEsq = (float)selEsq; lastMenu2 = menuAtual; }
        float& smoothSel = (refPainel == 0) ? smoothSelEsq : smoothSelDir;

        if (fabs(smoothSel - sAtual) > 20.0f) smoothSel = (float)sAtual;
        smoothSel += (sAtual - smoothSel) * 0.15f;

        int loadsThisFrame = 0;
        initCoverCache();

        // ====================================================================
        // GESTÃO DA GRADE (MODOS 4 E 5)
        // ====================================================================
        if (currentRenderStyle == 4 || currentRenderStyle == 5) {
            int cols = (currentRenderStyle == 5) ? 3 : gridCols;
            if (cols < 1) cols = 1;
            int itemW = gridItemW;
            int itemH = gridItemH;
            int paddingX = gridSpcX;
            int paddingY = gridSpcY;
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
                if (cacheIdx == -1 && loadsThisFrame < 1) {
                    int freeSlot = getFreeCacheSlot();
                    if (freeSlot != -1) {
                        char cPath[512] = "";
                        if (mAtual == MENU_JOGAR_PS4) {
                            sprintf(cPath, "/user/appmeta/%s/icon0.png", linksAtuais[i]);
                            FILE* f = fopen(cPath, "rb"); if (!f) sprintf(cPath, "/user/app/%s/sce_sys/icon0.png", linksAtuais[i]); else fclose(f);
                        }
                        else {
                            const char* cName = (consoleAtual >= 0 && consoleAtual < 5) ? listaConsoles[consoleAtual].nome : "Unknown";
                            snprintf(cPath, sizeof(cPath), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", cName, nItems[i]);
                        }
                        coverCache[freeSlot].index = i;
                        coverCache[freeSlot].img = stbi_load(cPath, &coverCache[freeSlot].w, &coverCache[freeSlot].h, &coverCache[freeSlot].c, 4);
                        cacheIdx = freeSlot;
                        loadsThisFrame++;
                    }
                }

                if (cacheIdx != -1 && coverCache[cacheIdx].img) {
                    desenharRedimensionado(p, coverCache[cacheIdx].img, coverCache[cacheIdx].w, coverCache[cacheIdx].h, itemW, itemH, drawX, drawY);
                }
                else {
                    for (int by = 0; by < itemH; by++) for (int bx = 0; bx < itemW; bx++) { int pyy = drawY + by, pxx = drawX + bx; if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xAA222222; }
                    desenharTextoAlinhado(p, nItems[i], 30, drawX + 10, drawY + itemH / 2, itemW - 20, 0xFFFFFFFF);
                }

                if (i == sAtual) {
                    for (int by = -5; by < itemH + 5; by++) for (int bx = -5; bx < itemW + 5; bx++) {
                        if (by < 0 || by >= itemH || bx < 0 || bx >= itemW) {
                            int pyy = drawY + by, pxx = drawX + bx;
                            if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xFF00FF00;
                        }
                    }
                    if (currentRenderStyle == 4) {
                        desenharTextoAlinhadoAnimado(p, nItems[i], 40, drawX - 50, drawY + itemH + 30, itemW + 100, 0xFFFFFFFF, true);
                    }
                }
            }
        }
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
                    if (cacheIdx == -1 && loadsThisFrame < 1) {
                        int freeSlot = getFreeCacheSlot();
                        if (freeSlot != -1) {
                            char cPath[512] = "";
                            if (mAtual == MENU_JOGAR_PS4) {
                                sprintf(cPath, "/user/appmeta/%s/icon0.png", linksAtuais[i]);
                                FILE* f = fopen(cPath, "rb"); if (!f) sprintf(cPath, "/user/app/%s/sce_sys/icon0.png", linksAtuais[i]); else fclose(f);
                            }
                            else {
                                const char* cName = (consoleAtual >= 0 && consoleAtual < 5) ? listaConsoles[consoleAtual].nome : "Unknown";
                                snprintf(cPath, sizeof(cPath), "/data/HyperNeiva/baixado/capas/%s/Named_Boxarts/%s.png", cName, nItems[i]);
                            }
                            coverCache[freeSlot].index = i;
                            coverCache[freeSlot].img = stbi_load(cPath, &coverCache[freeSlot].w, &coverCache[freeSlot].h, &coverCache[freeSlot].c, 4);
                            cacheIdx = freeSlot;
                            loadsThisFrame++;
                        }
                    }

                    if (cacheIdx != -1 && coverCache[cacheIdx].img) {
                        desenharRedimensionado(p, coverCache[cacheIdx].img, coverCache[cacheIdx].w, coverCache[cacheIdx].h, drawW, drawH, drawX, drawY);
                    }
                    else {
                        for (int by = 0; by < drawH; by++) for (int bx = 0; bx < drawW; bx++) { int pyy = drawY + by, pxx = drawX + bx; if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xAA222222; }
                    }

                    if (offset == 0) {
                        for (int by = -6; by < drawH + 6; by++) for (int bx = -6; bx < drawW + 6; bx++) {
                            if (by < 0 || by >= drawH || bx < 0 || bx >= drawW) {
                                int pyy = drawY + by, pxx = drawX + bx;
                                if (pxx >= 0 && pxx < 1920 && pyy >= 0 && pyy < 1080) p[pyy * 1920 + pxx] = 0xFF00FFFF;
                            }
                        }
                        desenharTextoAlinhadoAnimado(p, nItems[i], 40, centerX - 200, centerY + centerH + 50, centerW + 400, 0xFFFFFFFF, true);
                    }
                }
            }
        }

        // ====================================================================
        // LISTAS CLÁSSICAS (SOMEM NO MODO 4 E 3)
        // ====================================================================
        bool showClassicList = true;
        if (currentRenderStyle == 4 && isGameMenu) showClassicList = false;
        if (currentRenderStyle == 3 && isGameMenu) showClassicList = false;

        if (showClassicList) {
            int loopStart = 0; int loopEnd = 6;
            if (currentRenderStyle == 1) { loopStart = (int)smoothSel - 4; loopEnd = (int)smoothSel + 4; }

            for (int i = loopStart; i <= loopEnd; i++) {
                int gIdx = (currentRenderStyle == 1) ? i : (i + oAtual);

                if (gIdx < 0 || gIdx >= tItens) {
                    if (currentRenderStyle == 1) continue; else break;
                }

                int currentX = posX_Base; int currentY = curListY;
                int animOffsetX = 0; int animOffsetY = 0;
                int currentFontTam = fontTam;
                float opacityMulti = 1.0f;

                if (currentRenderStyle == 1) {
                    float dist = (float)gIdx - smoothSel;

                    int centroX = posX_Base;
                    int centroY = curListY + (2 * stepY);
                    if (listOri == 1) centroX = posX_Base + (2 * stepX);

                    currentX = centroX + (int)(dist * stepX);
                    currentY = centroY + (int)(dist * stepY);

                    int curva = (int)(dist * dist * (float)listCurvature);
                    if (listOri == 0) animOffsetX = curva;
                    else animOffsetY = curva;

                    float absDist = fabs(dist);
                    if (absDist < 1.0f && (!painelDuplo || painelAtivo == refPainel)) {
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
                uint32_t corTexto = isPainelAtivo ? 0xFFFFFFFF : 0xFFAAAAAA;

                bool isMarcado = (mAtual == MENU_EXPLORAR || mAtual == MENU_BAIXAR_DROPBOX_LISTA || mAtual == MENU_BAIXAR_DROPBOX_UPLOAD) && mItems[gIdx];
                if (isMarcado) { corFundo = isPainelAtivo ? getSysColor(listMark) : getSysColor(11); }

                bool isSelected = isPainelAtivo && (gIdx == sAtual);

                if (isSelected) {
                    if (isMarcado) corFundo = getSysColor(listHoverMark); else corFundo = getSysColor(10);
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

                desenharTextoAlinhadoAnimado(p, nItems[gIdx], currentFontTam, drawX, drawY + (listH / 4), larguraItem, corTexto, isSelected);
            }

            int curListX2 = (listOri == 0) ? listXV : listXH;
            int curListY2 = (listOri == 0) ? listYV : listYH;
            int curListSpc2 = (listOri == 0) ? listSpcV : listSpcH;
            int stepX2 = (listOri == 1) ? curListSpc2 : 0;
            int stepY2 = (listOri == 0) ? curListSpc2 : 0;
            int posX_B2 = (painelDuplo) ? ((refPainel == 0) ? capaX : curListX2) : curListX2;
            int wItem2 = (painelDuplo) ? 750 : listW;
            int selScreenX = posX_B2 + ((sAtual - oAtual) * stepX2);
            int selScreenY = curListY2 + ((sAtual - oAtual) * stepY2);

            if (currentRenderStyle == 1) {
                selScreenX = posX_B2 + (listOri == 1 ? (2 * stepX2) : 0);
                selScreenY = curListY2 + (2 * stepY2);
            }
            if (selScreenX > -100 && currentRenderStyle != 4 && currentRenderStyle != 3) {
                desenharElementos(p, selScreenX, selScreenY, wItem2, listH);
            }
        }

        // ====================================================================
        // RENDERIZAÇÃO DA CAPA ÚNICA NORMAL
        // ====================================================================
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
                        if (imgCapaDinamica && !isMenuPS4) { desenharRedimensionado(p, imgCapaDinamica, dynCapaW, dynCapaH, capaW, capaH, capaX, capaY); }
                        else if (imgPreview) { desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY); }
                        else if (defaultArtwork1) { desenharRedimensionado(p, defaultArtwork1, wDef1, hDef1, capaW, capaH, capaX, capaY); }

                        if (imgDiscoDinamico && !isMenuPS4) { desenharDiscoRedondo(p, imgDiscoDinamico, dynDiscoW, dynDiscoH, discoW, discoH, discoX, discoY); }
                        else if (defaultArtwork2) { desenharDiscoRedondo(p, defaultArtwork2, wDef2, hDef2, discoW, discoH, discoX, discoY); }
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
            if (isSel) isTextoAnimado = true;

            desenharTextoAlinhadoAnimado(p, listaOpcoes[gIdx], fontTam, upX, upY + 50 + (i * 45), upW, corOp, isSel);
        }
    }

    desenharMenuAudio(p); desenharMenuUpload(p);

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

    // ===============================================================
    // RODAPÉ DO MODO DE EDIÇÃO PERFEITAMENTE RESTAURADO!
    // ===============================================================
    bool showEditBottomBar = false;
    if (editMode || menuAtual == MENU_EDIT_TARGET) showEditBottomBar = true;

    if (showEditBottomBar) {
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
        else if (editType == 3) sprintf(txtPos, "MODO EDICAO - COR DE FUNDO (USE SETAS ESQ/DIR)");
        else if (editType == 8) sprintf(txtPos, "MODO EDICAO - COR PREENCHIMENTO (USE SETAS ESQ/DIR)");
        else if (editType == 4) sprintf(txtPos, "MODO EDICAO - ESPACAMENTO: %d", (listOri == 0) ? listSpcV : listSpcH);
        else if (editType == 5) sprintf(txtPos, "MODO EDICAO - ORIENTACAO: %s", listOri == 0 ? "VERTICAL" : "HORIZONTAL");
        else if (editType == 10) sprintf(txtPos, "MODO EDICAO - ALINHAMENTO: %s", fontAlign == 0 ? "ESQ" : (fontAlign == 1 ? "CENTRO" : "DIR"));
        else if (editType == 11) sprintf(txtPos, "MODO EDICAO - LIMITES: %s", fontScroll == 0 ? "CORTAR (..)" : "ANIMACAO ROLAGEM");
        else if (editType == 12) { int stat = 0; if (editTarget == 11) stat = elem1On; else if (editTarget == 12) stat = ctrl1On; else if (editTarget == 13) stat = pont1On; sprintf(txtPos, "MODO EDICAO - LIGADO: %s", stat ? "SIM" : "NAO"); }
        else if (editType == 13) sprintf(txtPos, "MODO EDICAO - MODO PONTEIRO: %s", pont1Modo == 0 ? "ACOMPANHA" : "ESTATICO");
        else if (editType == 14) { const char* lds[] = { "ESQ", "DIR", "CIMA", "BAIXO" }; sprintf(txtPos, "MODO EDICAO - LADO PONTEIRO: %s", lds[pont1Lado]); }
        else if (editType == 15) sprintf(txtPos, "MODO EDICAO - EFEITOS SONOROS: %s", sfxLigado ? "LIGADO" : "DESLIGADO");
        else if (editType == 16) sprintf(txtPos, "MODO EDICAO - VOLUME EFEITOS: %d%%", sfxVolume);
        else if (editType == 17) sprintf(txtPos, "MODO EDICAO - MENU OPCOES POSICAO: X:%d Y:%d", upX, upY);
        else if (editType == 18) sprintf(txtPos, "MODO EDICAO - MENU OPCOES TAMANHO: W:%d H:%d", upW, upH);
        else if (editType == 19) sprintf(txtPos, "MODO EDICAO - MENU OPCOES ESTICAR: W:%d", upW);
        else if (editType == 20) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR FUNDO");
        else if (editType == 21) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR DO TEXTO");
        else if (editType == 22) sprintf(txtPos, "MODO EDICAO - MENU OPCOES COR TEXTO SEL");
        else if (editType == 23) sprintf(txtPos, "MODO EDICAO - ANIMACAO: POSICAO X:%d Y:%d", anim_posX, anim_posY);
        else if (editType == 24) sprintf(txtPos, "MODO EDICAO - ANIMACAO: ESCALA: %.1f", anim_escala);
        else if (editType == 25) sprintf(txtPos, "MODO EDICAO - ANIMACAO: VELOCIDADE: %d", anim_velocidade);
        else if (editType == 26) sprintf(txtPos, "MODO EDICAO - ANIMACAO GRADE: COLS:%d LINS:%d", anim_colunas, anim_linhas);
        else if (editType == 27) sprintf(txtPos, "MODO EDICAO - ANIMACAO: OFFSET GRADE X:%d Y:%d", anim_offsetX, anim_offsetY);
        else if (editType == 28) sprintf(txtPos, "MODO EDICAO - ANIMACAO: LOOP INICIO [%d] FIM [%d]", anim_frameInicial, anim_frameFinal);
        else if (editType == 29) sprintf(txtPos, "MODO TESTE - FRAME [%d] | OFF X:%d Y:%d", anim_frameAtual, anim_frameOffsetX[anim_frameAtual], anim_frameOffsetY[anim_frameAtual]);
        else if (editType == 30) sprintf(txtPos, "MODO EDICAO - ANIMACAO CONTINUA ATIVADA!");
        else if (editType == 31) sprintf(txtPos, "MODO EDICAO - ANIMACAO: VISUAL LIGADO: %s", anim_ativo ? "SIM" : "NAO");
        else if (editType == 32) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 1: RED: %d", anim_keyR);
        else if (editType == 33) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 1: GREEN: %d", anim_keyG);
        else if (editType == 34) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 1: BLUE: %d", anim_keyB);
        else if (editType == 36) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: LIGADO: %s", anim_usarColorKey2 ? "SIM" : "NAO");
        else if (editType == 37) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: RED: %d", anim_keyR2);
        else if (editType == 38) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: GREEN: %d", anim_keyG2);
        else if (editType == 39) sprintf(txtPos, "MODO EDICAO - CHROMA KEY 2: BLUE: %d", anim_keyB2);
        else if (editType == 40) sprintf(txtPos, "MODO EDICAO - AUTO-CENTRO GLOBAL: %s", anim_autoCenter ? "LIGADO" : "DESLIGADO");
        else if (editType == 42) sprintf(txtPos, "MODO EDICAO - TOLERANCIA: %d", anim_tolerancia);
        else if (editType == 44) sprintf(txtPos, "USE AS SETAS E QUADRADO P/ COR 1, OU X P/ COR 2");
        else if (editType == 45) {
            const char* nEst[] = { "0. ORIGINAL", "1. ROLETA", "2. HORIZONTAL", "3. N-SWITCH", "4. GRADE PURA", "5. GRADE + LISTA" };
            sprintf(txtPos, "MODO EDICAO - ESTILO DA LISTA: %s (USE SETAS)", nEst[listStyle]);
        }
        else if (editType == 77) sprintf(txtPos, "MODO EDICAO - GRADE COLUNAS:%d LINHAS:%d", gridCols, gridLins);
        else if (editType == 78) sprintf(txtPos, "MODO EDICAO - GRADE ESPACAMENTO X:%d Y:%d", gridSpcX, gridSpcY);
        else if (editType == 75) sprintf(txtPos, "MODO EDICAO - POSICAO DA GRADE X:%d Y:%d", gridX, gridY);
        else if (editType == 76) sprintf(txtPos, "MODO EDICAO - TAMANHO DA CAPA NA GRADE W:%d H:%d", gridItemW, gridItemH);
        else if (editTarget < 52) sprintf(txtPos, "MODO EDICAO - X: %d  |  Y: %d  |  LARGURA: %d  |  ALTURA: %d", *tX, *tY, *tW, *tH);

        for (int by = 0; by < 40; by++) { for (int bx = 0; bx < 1920; bx++) { int pyY = 1040 + by; if (pyY < 1080) p[pyY * 1920 + bx] = 0xAA000000; } }
        desenharTexto(p, txtPos, 25, 50, 1045, 0xFF00FF00);
    }

    if (msgTimer > 0) { desenharTexto(p, msgStatus, msgTam, msgX, msgY, 0xFFFFFFFF); msgTimer--; }
    else if ((menuAtual == MENU_EDIT_TARGET || editMode) && editTarget == 9) { desenharTexto(p, "EXEMPLO DE NOTIFICACAO...", msgTam, msgX, msgY, 0xFF00FF00); }
}