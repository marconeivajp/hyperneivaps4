// --- INÍCIO DO ARQUIVO controle.cpp ---
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

int cd = 0;
bool pCross = false;
bool pCircle = false;
bool pTri = false;

// Garantia de Declaração de Variáveis e Funções Externas
extern int selAudioOpcao;
extern int selOpcao;
extern int sel;
extern int off;
extern int totalItens;
extern bool showOpcoes;
extern bool editMode;

extern void acaoCircle_Notepad();
extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
extern void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
extern void acaoCircle_Baixar();
extern void acaoTriangle_Baixar();

void processarNavegacaoDPad(uint32_t botoes) {
    if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
        if (cd <= 0) {
            if (showOpcoes) {
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

void processarPreviewScraper() {
    if (menuAtual == SCRAPER_LIST && strcmp(nomes[sel], ultimoJogoCarregado) != 0) {
        char cp[256]; sprintf(cp, "/data/HyperNeiva/baixado/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]);
        FILE* fEx = fopen(cp, "rb");
        if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); strcpy(ultimoJogoCarregado, nomes[sel]); }
        else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } strcpy(ultimoJogoCarregado, ""); }
    }
}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (editMode) { processarControlesEdicao(botoes); return; }

    processarNavegacaoDPad(botoes);
    processarPreviewScraper();

    if (botoes & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCross) {
            if (menuAtual == ROOT || menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCross_Root();
            else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCross_Musicas();
            else if (menuAtual == MENU_NOTEPAD) acaoCross_Notepad(uId, imeSetting, imeTitle);
            else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCross_Explorar();
            else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCross_Editar();
            else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_URL || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCross_Baixar(uId, imeSetting, imeTitle);

            pCross = true;
        }
    }
    else pCross = false;

    if (botoes & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircle) {
            if (showOpcoes) showOpcoes = false;
            else {
                if (menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCircle_Root();
                else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCircle_Musicas();
                else if (menuAtual == MENU_NOTEPAD) acaoCircle_Notepad();
                else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCircle_Explorar();
                else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCircle_Editar();
                else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_URL || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCircle_Baixar();
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

    if (botoes & ORBIS_PAD_BUTTON_R1) {
        if (menuAtual == MENU_EXPLORAR) acaoR1_Explorar();
    }
}
// --- FIM DO ARQUIVO controle.cpp ---