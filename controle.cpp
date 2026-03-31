#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <orbis/Pad.h> 

#include "controle.h"
#include "controle_direcional.h"
#include "controle_musicas.h"
#include "controle_virtual.h"
#include "controle_explorar.h"
#include "controle_editar.h"
#include "controle_baixar.h"
#include "controle_root.h"
#include "menu_upload.h"
#include "controle_elementos.h" 
#include "elementos_sonoros.h" 

#include "stb_image.h"
#include "menu.h"
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"
#include "bloco_de_notas.h"
#include "ftp.h" 

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

int cd = 0;
bool pCross = false, pCircle = false, pTri = false, pSquare = false, pL1 = false, pR1 = false, pL2 = false;

extern int selAudioOpcao, selOpcao, sel, off, totalItens;
extern bool showOpcoes, editMode, marcados[3000];
extern bool painelDuplo; extern int painelAtivo, selEsq, totalItensEsq, offEsq;
extern bool visualizandoMidiaImagem; extern float zoomMidia; extern bool fullscreenMidia;
extern bool visualizandoMidiaTexto; extern int textoMidiaScroll, totalLinhasTexto;
extern int estadoNotepad, linhaSelecionada, totalLinhasNotepad; extern bool notepadSomenteLeitura;
extern bool showUploadOpcoes; extern int selUploadOpcao;

extern int listOri, audioH, upH, discoH;
extern int offAudioOpcao, offUploadOpcao, offOpcao;

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);
extern void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
extern void acaoCircle_Baixar(); extern void acaoTriangle_Baixar(); extern void acaoL2_Explorar();
extern void abrirMenuAudioOpcoes();

MenuLevel menuAntesDoAudio = ROOT; bool veioDeOutroMenuParaAudio = false;

int globalUserId = -1;

extern void carregarCapaLoja(int index);
int timeToLoadCapa = 0;

extern unsigned char* imgPreview;
extern int wP, hP, cP;
extern int ftpL2State;
extern void acaoL2_FTP();

extern int totalOpcoes;
unsigned char* imgPic1 = NULL;
int wPic1 = 0, hPic1 = 0, cPic1 = 0;

extern bool isFirstFrameUI;
extern char nomes[3000][64];
extern char nomesEsq[3000][64];
extern char pathExplorarEsq[256];
extern char pathExplorar[256];

extern bool crossReleaseRequired;
extern bool circleReleaseRequired;
extern bool selecionandoMidiaElemento;

#define MAX_NAV_STACK 100
struct EstadoNavegacao {
    MenuLevel menu; char path[256]; int sel; int off;
    MenuLevel menuEsq; char pathEsq[256]; int selEsq; int offEsq;
};
EstadoNavegacao pilhaNav[MAX_NAV_STACK]; int navTopo = 0;

void carregarPreviewLocal(const char* caminhoAbsoluto) {
    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
    char temp[512]; strcpy(temp, caminhoAbsoluto);
    for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
    if (strstr(temp, ".png") || strstr(temp, ".jpg") || strstr(temp, ".jpeg") || strstr(temp, ".bmp")) { imgPreview = stbi_load(caminhoAbsoluto, &wP, &hP, &cP, 4); }
}

void processarNavegacaoDPad(uint32_t botoes) {
    if (visualizandoMidiaImagem) { if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) { if (cd <= 0) { if (botoes & ORBIS_PAD_BUTTON_UP) { fullscreenMidia = false; zoomMidia += 0.5f; } else if (botoes & ORBIS_PAD_BUTTON_DOWN) { fullscreenMidia = false; zoomMidia -= 0.5f; if (zoomMidia < 0.1f) zoomMidia = 0.1f; } cd = 2; } } else cd = 0; if (cd > 0) cd--; return; }
    if (visualizandoMidiaTexto) { if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) { if (cd <= 0) { if (botoes & ORBIS_PAD_BUTTON_UP) { textoMidiaScroll -= 2; if (textoMidiaScroll < 0) textoMidiaScroll = 0; } else if (botoes & ORBIS_PAD_BUTTON_DOWN) { textoMidiaScroll += 2; if (textoMidiaScroll > totalLinhasTexto - 15) textoMidiaScroll = totalLinhasTexto - 15; if (textoMidiaScroll < 0) textoMidiaScroll = 0; } cd = 4; } } else cd = 0; if (cd > 0) cd--; return; }

    bool isHoriz = false;
    if (listOri == 1 && !showOpcoes && !showUploadOpcoes && menuAtual != MENU_NOTEPAD && menuAtual != MENU_AUDIO_OPCOES) isHoriz = true;

    int btnNext = isHoriz ? ORBIS_PAD_BUTTON_RIGHT : ORBIS_PAD_BUTTON_DOWN;
    int btnPrev = isHoriz ? ORBIS_PAD_BUTTON_LEFT : ORBIS_PAD_BUTTON_UP;
    int btnPanNext = isHoriz ? ORBIS_PAD_BUTTON_DOWN : ORBIS_PAD_BUTTON_RIGHT;
    int btnPanPrev = isHoriz ? ORBIS_PAD_BUTTON_UP : ORBIS_PAD_BUTTON_LEFT;

    if (painelDuplo && !showOpcoes && (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME || menuAtual == MENU_BAIXAR_FTP_LISTA)) {
        if (botoes & (btnPanNext | btnPanPrev)) {
            if (cd <= 0) { painelAtivo = (painelAtivo == 0) ? 1 : 0; cd = 10; }
            return;
        }
    }

    if (botoes & (btnNext | btnPrev | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
        if (cd <= 0) {
            bool movNext = (botoes & btnNext) || (!isHoriz && (botoes & ORBIS_PAD_BUTTON_DOWN));
            bool movPrev = (botoes & btnPrev) || (!isHoriz && (botoes & ORBIS_PAD_BUTTON_UP));

            if (movNext) tocarSom(SFX_DOWN);
            else if (movPrev) tocarSom(SFX_UP);

            if (menuAtual == MENU_NOTEPAD) {
                if (estadoNotepad == 0) {
                    if (movNext) { if (linhaSelecionada < 4999) { linhaSelecionada++; if (linhaSelecionada >= totalLinhasNotepad && !notepadSomenteLeitura) totalLinhasNotepad = linhaSelecionada + 1; } }
                    else if (movPrev && linhaSelecionada > 0) linhaSelecionada--;
                }
            }
            else if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) {
                int maxV = (upH - 50) / 45; if (maxV < 1) maxV = 1;
                if (movNext) { if (selUploadOpcao < 2) { selUploadOpcao++; if (selUploadOpcao >= offUploadOpcao + maxV) offUploadOpcao++; } else { selUploadOpcao = 0; offUploadOpcao = 0; } }
                else if (movPrev) { if (selUploadOpcao > 0) { selUploadOpcao--; if (selUploadOpcao < offUploadOpcao) offUploadOpcao--; } else { selUploadOpcao = 2; offUploadOpcao = 3 - maxV; if (offUploadOpcao < 0) offUploadOpcao = 0; } }
            }
            else if (showOpcoes) {
                if (menuAtual == MENU_AUDIO_OPCOES) {
                    int maxV = (audioH - 50) / 45; if (maxV < 1) maxV = 1;
                    if (movNext) { if (selAudioOpcao < 10) { selAudioOpcao++; if (selAudioOpcao >= offAudioOpcao + maxV) offAudioOpcao++; } else { selAudioOpcao = 0; offAudioOpcao = 0; } }
                    else if (movPrev) { if (selAudioOpcao > 0) { selAudioOpcao--; if (selAudioOpcao < offAudioOpcao) offAudioOpcao--; } else { selAudioOpcao = 10; offAudioOpcao = 11 - maxV; if (offAudioOpcao < 0) offAudioOpcao = 0; } }
                }
                else {
                    int maxV = (discoH - 80) / 45; if (maxV < 1) maxV = 1;
                    if (movNext) { if (selOpcao < totalOpcoes - 1) { selOpcao++; if (selOpcao >= offOpcao + maxV) offOpcao++; } else { selOpcao = 0; offOpcao = 0; } }
                    else if (movPrev) { if (selOpcao > 0) { selOpcao--; if (selOpcao < offOpcao) offOpcao--; } else { selOpcao = totalOpcoes - 1; offOpcao = totalOpcoes - maxV; if (offOpcao < 0) offOpcao = 0; } }
                }
            }
            else {
                bool ehEsq = (painelDuplo && painelAtivo == 0 && (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME || menuAtual == MENU_BAIXAR_FTP_LISTA));
                int* sAtual = ehEsq ? &selEsq : &sel; int* oAtual = ehEsq ? &offEsq : &off; int tItens = ehEsq ? totalItensEsq : totalItens;

                if (movNext) {
                    if (*sAtual < (tItens - 1)) { (*sAtual)++; if (*sAtual >= (*oAtual + 6)) (*oAtual)++; }
                    else if (tItens > 0) { *sAtual = 0; *oAtual = 0; }
                }
                else if (movPrev) {
                    if (*sAtual > 0) { (*sAtual)--; if (*sAtual < *oAtual) (*oAtual)--; }
                    else if (tItens > 0) { *sAtual = tItens - 1; *oAtual = tItens - 6; if (*oAtual < 0) *oAtual = 0; }
                }
                timeToLoadCapa = 10;
            }
            cd = 10;
        }
    }
    else if (!(botoes & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT | ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN))) { cd = 0; }
    if (cd > 0) cd--;

    if (timeToLoadCapa > 0) {
        timeToLoadCapa--;
        if (timeToLoadCapa == 0) {
            bool ehEsq = (painelDuplo && painelAtivo == 0);
            if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA && !ehEsq) carregarCapaLoja(sel);

            else if (menuAtual == MENU_JOGAR_PS4) {
                extern char linksAtuais[3000][1024];
                if (strlen(linksAtuais[sel]) > 0) {
                    char iconPath[512]; char picPath[512];

                    sprintf(iconPath, "/user/appmeta/%s/icon0.png", linksAtuais[sel]);
                    FILE* f = fopen(iconPath, "rb");
                    if (f) { fclose(f); carregarPreviewLocal(iconPath); }
                    else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } }

                    sprintf(picPath, "/user/appmeta/%s/pic1.png", linksAtuais[sel]);
                    FILE* fp = fopen(picPath, "rb");
                    if (fp) {
                        fclose(fp);
                        if (imgPic1) stbi_image_free(imgPic1);
                        imgPic1 = stbi_load(picPath, &wPic1, &hPic1, &cPic1, 4);
                    }
                    else { if (imgPic1) { stbi_image_free(imgPic1); imgPic1 = NULL; } }
                }
                else {
                    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
                    if (imgPic1) { stbi_image_free(imgPic1); imgPic1 = NULL; }
                }
            }

            else if (menuAtual == MENU_EXPLORAR || (painelDuplo && (menuAtualEsq == MENU_EXPLORAR || ftpL2State == 2))) {
                MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
                if (mAtual == MENU_EXPLORAR) {
                    int sAtual = ehEsq ? selEsq : sel; char (*nItems)[64] = ehEsq ? nomesEsq : nomes;
                    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;
                    if (nItems[sAtual][0] != '[') {
                        char caminhoAbs[512]; sprintf(caminhoAbs, "%s/%s", pExplorar, nItems[sAtual]);
                        if (strstr(caminhoAbs, "savedata") || strstr(caminhoAbs, "SAVEDATA") || strstr(caminhoAbs, "apollo") || strstr(caminhoAbs, "exported")) {
                            char saveIcon[512];
                            sprintf(saveIcon, "%s/sce_sys/icon0.png", caminhoAbs);
                            FILE* fs = fopen(saveIcon, "rb");
                            if (!fs) { sprintf(saveIcon, "%s/icon0.png", caminhoAbs); fs = fopen(saveIcon, "rb"); }
                            if (fs) { fclose(fs); carregarPreviewLocal(saveIcon); }
                            else { carregarPreviewLocal(caminhoAbs); }
                        }
                        else { carregarPreviewLocal(caminhoAbs); }
                    }
                    else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } }
                }
            }
        }
    }
}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    globalUserId = uId;
    if (totalItens <= 0) { sel = 0; off = 0; }
    else if (sel >= totalItens) { sel = totalItens - 1; if (sel < off) off = sel; else if (sel >= off + 6) off = sel - 5; if (off < 0) off = 0; }
    if (painelDuplo) { if (totalItensEsq <= 0) { selEsq = 0; offEsq = 0; } else if (selEsq >= totalItensEsq) { selEsq = totalItensEsq - 1; if (selEsq < offEsq) offEsq = selEsq; else if (selEsq >= offEsq + 6) offEsq = selEsq - 5; if (offEsq < 0) offEsq = 0; } }

    if (editMode && menuAtual != MENU_NOTEPAD) { processarControlesEdicao(botoes); return; }
    processarNavegacaoDPad(botoes);

    bool inExplorar = (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME || (painelDuplo && (menuAtualEsq == MENU_EXPLORAR || menuAtualEsq == MENU_EXPLORAR_HOME)));

    if (botoes & ORBIS_PAD_BUTTON_L2) { if (!pL2) { if (inExplorar) acaoL2_Explorar(); else if (menuAtual == MENU_BAIXAR_FTP_LISTA) acaoL2_FTP(); pL2 = true; } }
    else pL2 = false;
    if (botoes & ORBIS_PAD_BUTTON_L1) { if (!pL1) { pL1 = true; } }
    else pL1 = false;

    if (botoes & ORBIS_PAD_BUTTON_R1) {
        if (!pR1) {
            if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_FTP_LISTA) marcados[sel] = !marcados[sel];
            else if (inExplorar) { extern void acaoR1_Explorar(); acaoR1_Explorar(); } pR1 = true;
        }
    }
    else pR1 = false;

    if (botoes & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCross) {
            tocarSom(SFX_CROSS);

            MenuLevel mAntes = menuAtual; MenuLevel mEsqAntes = menuAtualEsq;
            char pAntes[256]; strcpy(pAntes, pathExplorar);
            char pEsqAntes[256]; strcpy(pEsqAntes, pathExplorarEsq);
            int sAntes = sel; int oAntes = off; int sEsqAntes = selEsq; int oEsqAntes = offEsq;

            if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) { extern void acaoCross_MenuUpload(); acaoCross_MenuUpload(); }
            else if (menuAtual == MENU_AUDIO_OPCOES && veioDeOutroMenuParaAudio && selAudioOpcao == 10) { menuAtual = menuAntesDoAudio; showOpcoes = false; veioDeOutroMenuParaAudio = false; }
            else if (menuAtual == ROOT || menuAtual == MENU_TIPO_JOGO || menuAtual == MENU_JOGAR_PS4 || menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCross_Root();
            else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCross_Musicas();
            else if (menuAtual == MENU_NOTEPAD) { if (!notepadSomenteLeitura) { if (estadoNotepad == 0) acaoCross_Notepad(uId, imeSetting, imeTitle, linhasNotepad[linhaSelecionada]); else if (estadoNotepad == 1) acaoCross_Notepad(uId, imeSetting, imeTitle, nomeArquivo); } }
            else if (inExplorar) {

                bool ehEsq = (painelDuplo && painelAtivo == 0);
                int sAtual = ehEsq ? selEsq : sel;
                char* nItem = ehEsq ? nomesEsq[sAtual] : nomes[sAtual];

                bool isImage = false;
                const char* dot = strrchr(nItem, '.');
                if (dot && (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)) {
                    isImage = true;
                }

                if (selecionandoMidiaElemento && isImage) {
                    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

                    extern CustomElementDef customUI[6][10];
                    extern int interfaceTelaAlvo;
                    extern int interfaceElementoAlvo;

                    CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo];

                    if (strcmp(pExplorar, "/") == 0) sprintf(el->caminho, "/%s", nItem);
                    else sprintf(el->caminho, "%s/%s", pExplorar, nItem);

                    el->ativo = true;

                    if (el->pW <= 0 || el->pH <= 0) {
                        el->pX = 960; el->pY = 540; el->pW = 300; el->pH = 300;
                        el->inX = -300; el->inY = 540;
                        el->outX = 2200; el->outY = 540;
                        el->animInAtiva = true; el->animOutAtiva = true;
                        el->velIn = 6; el->velOut = 6;
                    }

                    extern void salvarCustomUI(); salvarCustomUI();
                    selecionandoMidiaElemento = false;
                    isFirstFrameUI = true;

                    menuAtual = MENU_EDIT_TARGET;
                    editTarget = 18;
                    extern void preencherMenuEditTarget(); preencherMenuEditTarget();
                    sel = 1; off = 0;

                    editType = 62;
                    editMode = true;
                    crossReleaseRequired = true;
                    circleReleaseRequired = true;

                    extern char msgStatus[128]; extern int msgTimer;
                    strcpy(msgStatus, "IMAGEM CARREGADA! USE AS SETAS PARA POSICIONAR.");
                    msgTimer = 180;
                }
                else if (selecionandoMidiaElemento && !isImage) {
                    selecionandoMidiaElemento = false;
                    extern void acaoCross_Explorar(); acaoCross_Explorar();
                    selecionandoMidiaElemento = true;
                }
                else {
                    extern void acaoCross_Explorar(); acaoCross_Explorar();
                }
            }
            else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCross_Editar();
            else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_LOJAS || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_UPLOAD || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCross_Baixar(uId, imeSetting, imeTitle);

            if (menuAtual != mAntes || strcmp(pathExplorar, pAntes) != 0 || menuAtualEsq != mEsqAntes || strcmp(pathExplorarEsq, pEsqAntes) != 0) {
                if (navTopo < MAX_NAV_STACK) {
                    pilhaNav[navTopo].menu = mAntes; strcpy(pilhaNav[navTopo].path, pAntes); pilhaNav[navTopo].sel = sAntes; pilhaNav[navTopo].off = oAntes;
                    pilhaNav[navTopo].menuEsq = mEsqAntes; strcpy(pilhaNav[navTopo].pathEsq, pEsqAntes); pilhaNav[navTopo].selEsq = sEsqAntes; pilhaNav[navTopo].offEsq = oEsqAntes;
                    navTopo++;
                }
                if (menuAtual != mAntes || strcmp(pathExplorar, pAntes) != 0) { sel = 0; off = 0; }
                if (menuAtualEsq != mEsqAntes || strcmp(pathExplorarEsq, pEsqAntes) != 0) { selEsq = 0; offEsq = 0; }
            } pCross = true;
        }
    }
    else pCross = false;

    if (botoes & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircle) {
            tocarSom(SFX_CIRCLE);

            MenuLevel mAntes = menuAtual; MenuLevel mEsqAntes = menuAtualEsq;
            char pAntes[256]; strcpy(pAntes, pathExplorar);
            char pEsqAntes[256]; strcpy(pEsqAntes, pathExplorarEsq);

            if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) { extern void acaoCircle_MenuUpload(); acaoCircle_MenuUpload(); }
            else if (menuAtual == MENU_AUDIO_OPCOES && veioDeOutroMenuParaAudio) { menuAtual = menuAntesDoAudio; showOpcoes = false; veioDeOutroMenuParaAudio = false; }
            else if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) showOpcoes = false;
            else {
                if (menuAtual == MENU_NOTEPAD) { if (notepadSomenteLeitura) menuAtual = MENU_MIDIA; else { if (estadoNotepad == 1) estadoNotepad = 0; else { menuAtual = MENU_EXPLORAR; if (painelDuplo && painelAtivo == 0) { extern void listarDiretorioEsq(const char*); listarDiretorioEsq(pathExplorarEsq); } else { extern void listarDiretorio(const char*); listarDiretorio(pathExplorar); } } } }
                else if (menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA || menuAtual == MENU_TIPO_JOGO || menuAtual == MENU_JOGAR_PS4) acaoCircle_Root();
                else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCircle_Musicas();
                else if (inExplorar) {
                    if (selecionandoMidiaElemento) {
                        selecionandoMidiaElemento = false;
                        extern void acaoCircle_Explorar(); acaoCircle_Explorar();
                        if (menuAtual != MENU_EXPLORAR && menuAtual != MENU_EXPLORAR_HOME && menuAtualEsq != MENU_EXPLORAR && menuAtualEsq != MENU_EXPLORAR_HOME) {
                            menuAtual = MENU_EDIT_TARGET;
                            editTarget = 17;
                            extern void preencherMenuEditTarget(); preencherMenuEditTarget();
                            sel = 0; off = 0;
                            extern char msgStatus[128]; extern int msgTimer;
                            strcpy(msgStatus, "CRIACAO CANCELADA.");
                            msgTimer = 90;
                        }
                        else {
                            selecionandoMidiaElemento = true;
                        }
                    }
                    else {
                        extern void acaoCircle_Explorar(); acaoCircle_Explorar();
                    }
                }
                else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCircle_Editar();
                else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_LOJAS || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_UPLOAD || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCircle_Baixar();
            }
            if (menuAtual != mAntes || strcmp(pathExplorar, pAntes) != 0 || menuAtualEsq != mEsqAntes || strcmp(pathExplorarEsq, pEsqAntes) != 0) {
                if (navTopo > 0) {
                    navTopo--;
                    if (menuAtual != mAntes || strcmp(pathExplorar, pAntes) != 0) { sel = pilhaNav[navTopo].sel; off = pilhaNav[navTopo].off; }
                    if (menuAtualEsq != mEsqAntes || strcmp(pathExplorarEsq, pEsqAntes) != 0) { selEsq = pilhaNav[navTopo].selEsq; offEsq = pilhaNav[navTopo].offEsq; }
                }
                else { sel = 0; off = 0; selEsq = 0; offEsq = 0; }
            } pCircle = true;
        }
    }
    else pCircle = false;

    if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (!pTri) {
            if (botoes & ORBIS_PAD_BUTTON_L2) { if (menuAtual != MENU_AUDIO_OPCOES) { menuAntesDoAudio = menuAtual; veioDeOutroMenuParaAudio = true; abrirMenuAudioOpcoes(); } else if (veioDeOutroMenuParaAudio) { menuAtual = menuAntesDoAudio; showOpcoes = false; veioDeOutroMenuParaAudio = false; } }
            else {
                if (menuAtual == MENU_MUSICAS) acaoTriangle_Musicas();
                else if (inExplorar) { extern void acaoTriangle_Explorar(); acaoTriangle_Explorar(); }
                else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) { extern void acaoTriangle_MenuUpload(); acaoTriangle_MenuUpload(); }
                else if (menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_LISTA || menuAtual == MENU_BAIXAR_FTP_UPLOAD) acaoTriangle_Baixar();
            } pTri = true;
        }
    }
    else pTri = false;

    if (botoes & ORBIS_PAD_BUTTON_SQUARE) { if (!pSquare) { if (menuAtual == MENU_NOTEPAD) { if (!notepadSomenteLeitura) { if (estadoNotepad == 0) estadoNotepad = 1; else if (estadoNotepad == 1) { if (strlen(nomeArquivo) > 0) salvarArquivoNotepad(); else { snprintf(msgStatus, sizeof(msgStatus), "O nome do arquivo nao pode ser vazio!"); msgTimer = 120; } } } } pSquare = true; } }
    else pSquare = false;

    if (menuAtual == ROOT && navTopo > 0) navTopo = 0;
}