#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <orbis/Pad.h> // CORREÇÃO: Biblioteca dos botões adicionada!

#include "controle.h"
#include "controle_direcional.h"
#include "controle_musicas.h"
#include "controle_virtual.h"
#include "controle_explorar.h"
#include "controle_editar.h"
#include "controle_baixar.h"
#include "controle_root.h"
#include "menu_upload.h"

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

int cd = 0;
bool pCross = false, pCircle = false, pTri = false, pSquare = false, pL1 = false, pR1 = false, pL2 = false;

extern int selAudioOpcao, selOpcao, sel, off, totalItens;
extern bool showOpcoes, editMode, marcados[3000];
extern bool painelDuplo; extern int painelAtivo, selEsq, totalItensEsq, offEsq;
extern bool visualizandoMidiaImagem; extern float zoomMidia; extern bool fullscreenMidia;
extern bool visualizandoMidiaTexto; extern int textoMidiaScroll, totalLinhasTexto;
extern int estadoNotepad, linhaSelecionada, totalLinhasNotepad; extern bool notepadSomenteLeitura;
extern bool showUploadOpcoes; extern int selUploadOpcao;

extern int listOri;
extern int audioH, upH, discoH;

// VARIÁVEIS DE SCROLL COMO EXTERNAS PARA NÃO DAR DUPLICATE SYMBOL
extern int offAudioOpcao;
extern int offUploadOpcao;
extern int offOpcao;

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);
extern void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
extern void acaoCircle_Baixar(); extern void acaoTriangle_Baixar(); extern void acaoL2_Explorar();

void processarNavegacaoDPad(uint32_t botoes) {

    if (visualizandoMidiaImagem) {
        if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
            if (cd <= 0) {
                if (botoes & ORBIS_PAD_BUTTON_UP) { fullscreenMidia = false; zoomMidia += 0.5f; }
                else if (botoes & ORBIS_PAD_BUTTON_DOWN) { fullscreenMidia = false; zoomMidia -= 0.5f; if (zoomMidia < 0.1f) zoomMidia = 0.1f; }
                cd = 2;
            }
        }
        else cd = 0; if (cd > 0) cd--; return;
    }

    if (visualizandoMidiaTexto) {
        if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
            if (cd <= 0) {
                if (botoes & ORBIS_PAD_BUTTON_UP) { textoMidiaScroll -= 2; if (textoMidiaScroll < 0) textoMidiaScroll = 0; }
                else if (botoes & ORBIS_PAD_BUTTON_DOWN) { textoMidiaScroll += 2; if (textoMidiaScroll > totalLinhasTexto - 15) textoMidiaScroll = totalLinhasTexto - 15; if (textoMidiaScroll < 0) textoMidiaScroll = 0; }
                cd = 4;
            }
        }
        else cd = 0; if (cd > 0) cd--; return;
    }

    bool isHoriz = (listOri == 1 && !showOpcoes && !showUploadOpcoes && menuAtual != MENU_NOTEPAD && menuAtual != MENU_AUDIO_OPCOES);
    int btnNext = isHoriz ? ORBIS_PAD_BUTTON_RIGHT : ORBIS_PAD_BUTTON_DOWN;
    int btnPrev = isHoriz ? ORBIS_PAD_BUTTON_LEFT : ORBIS_PAD_BUTTON_UP;
    int btnPanNext = isHoriz ? ORBIS_PAD_BUTTON_DOWN : ORBIS_PAD_BUTTON_RIGHT;
    int btnPanPrev = isHoriz ? ORBIS_PAD_BUTTON_UP : ORBIS_PAD_BUTTON_LEFT;

    if (painelDuplo && !showOpcoes && (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME)) {
        if (botoes & (btnPanNext | btnPanPrev)) { if (cd <= 0) { painelAtivo = (painelAtivo == 0) ? 1 : 0; cd = 10; } return; }
    }

    if (botoes & (btnNext | btnPrev)) {
        if (cd <= 0) {
            if (menuAtual == MENU_NOTEPAD) {
                if (estadoNotepad == 0) {
                    if (botoes & btnNext) { if (linhaSelecionada < 4999) { linhaSelecionada++; if (linhaSelecionada >= totalLinhasNotepad && !notepadSomenteLeitura) totalLinhasNotepad = linhaSelecionada + 1; } }
                    else if (botoes & btnPrev && linhaSelecionada > 0) linhaSelecionada--;
                }
            }
            else if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) {
                int maxV = (upH - 50) / 45; if (maxV < 1) maxV = 1;
                if (botoes & btnNext) {
                    if (selUploadOpcao < 2) { selUploadOpcao++; if (selUploadOpcao >= offUploadOpcao + maxV) offUploadOpcao++; }
                    else { selUploadOpcao = 0; offUploadOpcao = 0; }
                }
                else if (botoes & btnPrev) {
                    if (selUploadOpcao > 0) { selUploadOpcao--; if (selUploadOpcao < offUploadOpcao) offUploadOpcao--; }
                    else { selUploadOpcao = 2; offUploadOpcao = 3 - maxV; if (offUploadOpcao < 0) offUploadOpcao = 0; }
                }
            }
            else if (showOpcoes) {
                if (menuAtual == MENU_AUDIO_OPCOES) {
                    int maxV = (audioH - 50) / 45; if (maxV < 1) maxV = 1;
                    if (botoes & btnNext) {
                        if (selAudioOpcao < 10) { selAudioOpcao++; if (selAudioOpcao >= offAudioOpcao + maxV) offAudioOpcao++; }
                        else { selAudioOpcao = 0; offAudioOpcao = 0; }
                    }
                    else if (botoes & btnPrev) {
                        if (selAudioOpcao > 0) { selAudioOpcao--; if (selAudioOpcao < offAudioOpcao) offAudioOpcao--; }
                        else { selAudioOpcao = 10; offAudioOpcao = 11 - maxV; if (offAudioOpcao < 0) offAudioOpcao = 0; }
                    }
                }
                else {
                    int maxV = (discoH - 80) / 45; if (maxV < 1) maxV = 1;
                    if (botoes & btnNext) {
                        if (selOpcao < 9) { selOpcao++; if (selOpcao >= offOpcao + maxV) offOpcao++; }
                        else { selOpcao = 0; offOpcao = 0; }
                    }
                    else if (botoes & btnPrev) {
                        if (selOpcao > 0) { selOpcao--; if (selOpcao < offOpcao) offOpcao--; }
                        else { selOpcao = 9; offOpcao = 10 - maxV; if (offOpcao < 0) offOpcao = 0; }
                    }
                }
            }
            else {
                bool ehEsq = (painelDuplo && painelAtivo == 0 && (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME));
                int* sAtual = ehEsq ? &selEsq : &sel; int* oAtual = ehEsq ? &offEsq : &off;
                int tItens = ehEsq ? totalItensEsq : totalItens;

                if (botoes & btnNext) {
                    if (*sAtual < (tItens - 1)) { (*sAtual)++; if (*sAtual >= (*oAtual + 6)) (*oAtual)++; }
                    else if (tItens > 0) { *sAtual = 0; *oAtual = 0; }
                }
                else if (botoes & btnPrev) {
                    if (*sAtual > 0) { (*sAtual)--; if (*sAtual < *oAtual) (*oAtual)--; }
                    else if (tItens > 0) { *sAtual = tItens - 1; *oAtual = tItens - 6; if (*oAtual < 0) *oAtual = 0; }
                }
            }
            cd = 10;
        }
    }
    else if (!(botoes & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT | ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN))) { cd = 0; }
    if (cd > 0) cd--;
}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (totalItens <= 0) { sel = 0; off = 0; }
    else if (sel >= totalItens) { sel = totalItens - 1; if (sel < off) off = sel; else if (sel >= off + 6) off = sel - 5; if (off < 0) off = 0; }
    if (painelDuplo) { if (totalItensEsq <= 0) { selEsq = 0; offEsq = 0; } else if (selEsq >= totalItensEsq) { selEsq = totalItensEsq - 1; if (selEsq < offEsq) offEsq = selEsq; else if (selEsq >= offEsq + 6) offEsq = selEsq - 5; if (offEsq < 0) offEsq = 0; } }

    if (editMode && menuAtual != MENU_NOTEPAD) { processarControlesEdicao(botoes); return; }
    processarNavegacaoDPad(botoes);

    if (botoes & ORBIS_PAD_BUTTON_L2) { if (!pL2) { if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoL2_Explorar(); pL2 = true; } }
    else pL2 = false;
    if (botoes & ORBIS_PAD_BUTTON_L1) { if (!pL1) { if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) marcados[sel] = !marcados[sel]; pL1 = true; } }
    else pL1 = false;
    if (botoes & ORBIS_PAD_BUTTON_R1) { if (!pR1) { if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) marcados[sel] = !marcados[sel]; else if (menuAtual == MENU_EXPLORAR) acaoR1_Explorar(); pR1 = true; } }
    else pR1 = false;
    if (botoes & ORBIS_PAD_BUTTON_CROSS) { if (!pCross) { if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) acaoCross_MenuUpload(); else if (menuAtual == ROOT || menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCross_Root(); else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCross_Musicas(); else if (menuAtual == MENU_NOTEPAD) { if (!notepadSomenteLeitura) { if (estadoNotepad == 0) acaoCross_Notepad(uId, imeSetting, imeTitle, linhasNotepad[linhaSelecionada]); else if (estadoNotepad == 1) acaoCross_Notepad(uId, imeSetting, imeTitle, nomeArquivo); } } else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCross_Explorar(); else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCross_Editar(); else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCross_Baixar(uId, imeSetting, imeTitle); pCross = true; } }
    else pCross = false;
    if (botoes & ORBIS_PAD_BUTTON_CIRCLE) { if (!pCircle) { if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) acaoCircle_MenuUpload(); else if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) showOpcoes = false; else { if (menuAtual == MENU_NOTEPAD) { if (notepadSomenteLeitura) { menuAtual = MENU_MIDIA; } else { if (estadoNotepad == 1) estadoNotepad = 0; else { menuAtual = MENU_EXPLORAR; if (painelDuplo && painelAtivo == 0) listarDiretorioEsq(pathExplorarEsq); else listarDiretorio(pathExplorar); } } } else if (menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCircle_Root(); else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCircle_Musicas(); else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCircle_Explorar(); else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCircle_Editar(); else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCircle_Baixar(); } pCircle = true; } }
    else pCircle = false;
    if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) { if (!pTri) { if (menuAtual == MENU_MUSICAS) acaoTriangle_Musicas(); else if (menuAtual == MENU_EXPLORAR) acaoTriangle_Explorar(); else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) acaoTriangle_MenuUpload(); pTri = true; } }
    else pTri = false;
    if (botoes & ORBIS_PAD_BUTTON_SQUARE) { if (!pSquare) { if (menuAtual == MENU_NOTEPAD) { if (!notepadSomenteLeitura) { if (estadoNotepad == 0) estadoNotepad = 1; else if (estadoNotepad == 1) { if (strlen(nomeArquivo) > 0) salvarArquivoNotepad(); else { snprintf(msgStatus, sizeof(msgStatus), "O nome do arquivo nao pode ser vazio!"); msgTimer = 120; } } } } pSquare = true; } }
    else pSquare = false;
}