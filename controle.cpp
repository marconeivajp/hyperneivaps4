// --- INÍCIO DO ARQUIVO controle.cpp ---
#include "controle.h"
#include "controle_musicas.h"
#include "controle_virtual.h"
#include "controle_explorar.h"
#include "controle_editar.h"
#include "controle_baixar.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h> // <-- CORREÇÃO: Faltava essa linha para o __builtin_va_list funcionar!

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

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

extern MenuLevel menuAtual;
extern bool editMode;
extern bool showOpcoes;
extern int selAudioOpcao;
extern int selOpcao;
extern int sel;
extern int off;
extern int totalItens;
extern char nomes[3000][64];
extern char ultimoJogoCarregado[64];
extern char bufferTecladoC[128];
extern unsigned char* imgPreview;
extern int consoleAtual;
extern int wP, hP, cP;

extern void preencherRoot();
extern void preencherExplorerHome();
extern void preencherMenuMusicas();
extern void preencherMenuBaixar();
extern void preencherMenuEditar();

void executarAcaoX() {}
void executarAcaoBolinha() {}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (editMode) {
        processarControlesEdicao(botoes);
    }
    else {
        // --- D-PAD CIMA/BAIXO (NAVEGAÇÃO COMUM) --- 
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

        // PREVIEW DE IMAGENS DO SCRAPER
        if (menuAtual == SCRAPER_LIST && strcmp(nomes[sel], ultimoJogoCarregado) != 0) {
            char cp[256]; sprintf(cp, "/data/HyperNeiva/baixado/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]);
            FILE* fEx = fopen(cp, "rb");
            if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); strcpy(ultimoJogoCarregado, nomes[sel]); }
            else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } strcpy(ultimoJogoCarregado, ""); }
        }

        // --- BOTÃO X (CONFIRMAR) --- 
        if (botoes & ORBIS_PAD_BUTTON_CROSS) {
            if (!pCross) {

                // 1. DELEGAÇÃO AUTOMÁTICA PARA OS MÓDULOS
                if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCross_Musicas();
                else if (menuAtual == MENU_NOTEPAD) acaoCross_Notepad(uId, imeSetting, imeTitle);
                else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCross_Explorar();
                else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCross_Editar();
                else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCross_Baixar();

                // 2. ROOT E JOGOS (Ficaram no arquivo pai)
                else if (menuAtual == ROOT) {
                    if (sel == 0) carregarXML("/app0/assets/lista.xml");
                    else if (sel == 1) { preencherMenuBaixar(); sel = 0; off = 0; }
                    else if (sel == 2) { preencherMenuEditar(); sel = 0; off = 0; }
                    else if (sel == 3) { preencherExplorerHome(); sel = 0; off = 0; }
                    else if (sel == 4) {
                        if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
                        strcpy(ultimoJogoCarregado, ""); preencherMenuMusicas(); sel = 0; off = 0;
                    }
                    else if (sel == 5) {
                        menuAtual = MENU_NOTEPAD;
                        memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
                    }
                }
                else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) carregarXML("/app0/assets/sp.xml");

                if (!editMode && !showOpcoes && menuAtual != SCRAPER_LIST && menuAtual != JOGAR_XML && menuAtual != MENU_NOTEPAD && menuAtual != MENU_MUSICAS && menuAtual != MENU_BAIXAR_LINKS) {
                    sel = 0; off = 0;
                }
                executarAcaoX();
                pCross = true;
            }
        }
        else pCross = false;

        // --- BOTÃO CÍRCULO (VOLTAR) ---
        if (botoes & ORBIS_PAD_BUTTON_CIRCLE) {
            if (!pCircle) {
                if (showOpcoes) { showOpcoes = false; }
                else {
                    // DELEGAÇÃO DO VOLTAR
                    if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCircle_Musicas();
                    else if (menuAtual == MENU_NOTEPAD) acaoCircle_Notepad();
                    else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCircle_Explorar();
                    else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCircle_Editar();
                    else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCircle_Baixar();

                    else if (menuAtual == JOGAR_XML) { if (strstr(xmlCaminhoAtual, "sp.xml")) carregarXML("/app0/assets/lista.xml"); else preencherRoot(); }

                    if (menuAtual != MENU_AUDIO_OPCOES && menuAtual != MENU_EXPLORAR && menuAtual != MENU_NOTEPAD) { sel = 0; off = 0; }
                }
                executarAcaoBolinha();
                pCircle = true;
            }
        }
        else pCircle = false;

        // --- BOTÃO TRIÂNGULO E R1 ---
        if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) {
            if (!pTri) {
                if (menuAtual == MENU_MUSICAS) acaoTriangle_Musicas();
                else if (menuAtual == MENU_EXPLORAR) acaoTriangle_Explorar();
                pTri = true;
            }
        }
        else pTri = false;

        if (botoes & ORBIS_PAD_BUTTON_R1) {
            if (menuAtual == MENU_EXPLORAR) acaoR1_Explorar();
        }

        // --- PROTEÇÃO DE ESTADO VISUAL ---
        if (menuAtual != MENU_EXPLORAR && menuAtual != MENU_AUDIO_OPCOES) {
            showOpcoes = false;
        }
    }
}
// --- FIM DO ARQUIVO controle.cpp ---