#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include "controle.h"
#include "controle_musicas.h"
#include "controle_virtual.h"
#include "controle_explorar.h"
#include "controle_editar.h"
#include "controle_baixar.h"
#include "controle_root.h"

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
bool pCross = false;
bool pCircle = false;
bool pTri = false;
bool pSquare = false;

extern int selAudioOpcao;
extern int selOpcao;
extern int sel;
extern int off;
extern int totalItens;
extern bool showOpcoes;
extern bool editMode;

extern void acaoCircle_Notepad();
extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);
extern void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
extern void acaoCircle_Baixar();
extern void acaoTriangle_Baixar();

void processarNavegacaoDPad(uint32_t botoes) {
    if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
        if (cd <= 0) {
            if (menuAtual == MENU_NOTEPAD) {
                if (estadoNotepad == 0) {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN) {
                        if (linhaSelecionada < MAX_LINHAS - 1) {
                            linhaSelecionada++;
                            if (linhaSelecionada >= totalLinhasNotepad && !notepadSomenteLeitura) totalLinhasNotepad = linhaSelecionada + 1;
                        }
                    }
                    else if (botoes & ORBIS_PAD_BUTTON_UP && linhaSelecionada > 0) {
                        linhaSelecionada--;
                    }
                }
                else if (estadoNotepad == 1) {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && pastaSelecionada < totalPastasNotepad - 1) pastaSelecionada++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && pastaSelecionada > 0) pastaSelecionada--;
                }
            }
            else if (showOpcoes) {
                if (menuAtual == MENU_AUDIO_OPCOES) {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && selAudioOpcao < 10) selAudioOpcao++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && selAudioOpcao > 0) selAudioOpcao--;
                }
                else {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && selOpcao < 9) selOpcao++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && selOpcao > 0) selOpcao--;
                }
            }
            else {
                if (botoes & ORBIS_PAD_BUTTON_DOWN && sel < (totalItens - 1)) { sel++; if (sel >= (off + 6)) off++; }
                else if (botoes & ORBIS_PAD_BUTTON_UP && sel > 0) { sel--; if (sel < off) off--; }
            }
            cd = 10;
        }
    }
    else cd = 0;
    if (cd > 0) cd--;
}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (editMode && menuAtual != MENU_NOTEPAD) { processarControlesEdicao(botoes); return; }

    processarNavegacaoDPad(botoes);

    if (botoes & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCross) {
            if (menuAtual == ROOT || menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCross_Root();
            else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCross_Musicas();
            else if (menuAtual == MENU_NOTEPAD) {
                if (!notepadSomenteLeitura) { // TRAVA: X NÃO FAZ NADA EM MODO LEITURA
                    if (estadoNotepad == 0) acaoCross_Notepad(uId, imeSetting, imeTitle, linhasNotepad[linhaSelecionada]);
                    else if (estadoNotepad == 1) {
                        if (strcmp(pastaAtualNotepad, "ATALHOS_RAIZ") == 0) lerDiretorioNotepad(pastasNotepad[pastaSelecionada]);
                        else if (strcmp(pastasNotepad[pastaSelecionada], "[PASTA VAZIA]") != 0) {
                            char novoCaminho[1024]; int len = strlen(pastaAtualNotepad);
                            const char* sep = (pastaAtualNotepad[len - 1] == '/') ? "" : "/";
                            snprintf(novoCaminho, sizeof(novoCaminho), "%s%s%s", pastaAtualNotepad, sep, pastasNotepad[pastaSelecionada]);
                            lerDiretorioNotepad(novoCaminho);
                        }
                    }
                    else if (estadoNotepad == 2) acaoCross_Notepad(uId, imeSetting, imeTitle, nomeArquivo);
                }
            }
            else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCross_Explorar();
            else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCross_Editar();
            else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCross_Baixar(uId, imeSetting, imeTitle);

            pCross = true;
        }
    }
    else pCross = false;

    if (botoes & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircle) {
            if (showOpcoes) showOpcoes = false;
            else {
                if (menuAtual == MENU_NOTEPAD) {
                    if (notepadSomenteLeitura) {
                        menuAtual = MENU_MIDIA; // Devolve para a tela de mídia/arquivos
                    }
                    else {
                        if (estadoNotepad == 2) estadoNotepad = 1;
                        else if (estadoNotepad == 1) {
                            if (strcmp(pastaAtualNotepad, "ATALHOS_RAIZ") == 0) estadoNotepad = 0;
                            else {
                                char* ultimaBarra = strrchr(pastaAtualNotepad, '/');
                                if (ultimaBarra != NULL) {
                                    if (ultimaBarra == pastaAtualNotepad + strlen(pastaAtualNotepad) - 1) { *ultimaBarra = '\0'; ultimaBarra = strrchr(pastaAtualNotepad, '/'); }
                                    if (ultimaBarra != NULL && ultimaBarra != pastaAtualNotepad) { *(ultimaBarra + 1) = '\0'; lerDiretorioNotepad(pastaAtualNotepad); }
                                    else carregarAtalhosNotepad();
                                }
                                else carregarAtalhosNotepad();
                            }
                        }
                        else acaoCircle_Notepad();
                    }
                }
                else if (menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCircle_Root();
                else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCircle_Musicas();
                else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCircle_Explorar();
                else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCircle_Editar();
                else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCircle_Baixar();
            }
            pCircle = true;
        }
    }
    else pCircle = false;

    if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (!pTri) {
            if (menuAtual == MENU_MUSICAS) acaoTriangle_Musicas();
            else if (menuAtual == MENU_EXPLORAR) acaoTriangle_Explorar();
            else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) acaoTriangle_Baixar();

            pTri = true;
        }
    }
    else pTri = false;

    if (botoes & ORBIS_PAD_BUTTON_SQUARE) {
        if (!pSquare) {
            if (menuAtual == MENU_NOTEPAD) {
                if (!notepadSomenteLeitura) { // TRAVA: QUADRADO NÃO FAZ NADA EM MODO LEITURA
                    if (estadoNotepad == 0) { estadoNotepad = 1; carregarAtalhosNotepad(); }
                    else if (estadoNotepad == 1) {
                        if (strcmp(pastaAtualNotepad, "ATALHOS_RAIZ") == 0) strcpy(pastaDestinoFinal, pastasNotepad[pastaSelecionada]);
                        else if (strcmp(pastasNotepad[pastaSelecionada], "[PASTA VAZIA]") == 0) strcpy(pastaDestinoFinal, pastaAtualNotepad);
                        else {
                            int len = strlen(pastaAtualNotepad); const char* sep = (pastaAtualNotepad[len - 1] == '/') ? "" : "/";
                            snprintf(pastaDestinoFinal, sizeof(pastaDestinoFinal), "%s%s%s", pastaAtualNotepad, sep, pastasNotepad[pastaSelecionada]);
                        }
                        estadoNotepad = 2;
                    }
                    else if (estadoNotepad == 2) {
                        if (strlen(nomeArquivo) > 0) salvarArquivoNotepad();
                        else { snprintf(msgStatus, sizeof(msgStatus), "O nome do arquivo nao pode ser vazio!"); msgTimer = 120; }
                    }
                }
            }
            pSquare = true;
        }
    }
    else pSquare = false;

    if (botoes & ORBIS_PAD_BUTTON_R1) {
        if (menuAtual == MENU_EXPLORAR) acaoR1_Explorar();
    }
}