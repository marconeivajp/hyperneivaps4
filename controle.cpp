// 1. Correção de IntelliSense para Visual Studio (DEVE SER O PRIMEIRO BLOCO)
#ifdef __INTELLISENSE__
#include <stdarg.h>
#define __builtin_va_list va_list
#define __attribute__(x)
#endif

// 2. Bibliotecas Padrão C/C++
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 3. Headers do SDK do PS4
#include <orbis/ImeDialog.h>
#include <orbis/Pad.h>

// 4. Headers Locais do Projeto
#include "controle.h"
#include "menu.h"
#include "audio.h"
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "jogar.h"
#include "baixar.h"

// --- VARIÁVEIS EXTERNAS ---
extern bool tecladoAtivo;
extern uint16_t* bufferTecladoW;
extern char bufferTecladoC[128];
extern uint16_t* pImeTitle;
extern char ultimoJogoCarregado[64]; // Padronizado para 64 conforme explorar.h
extern int selAudioOpcao;           // Certifique-se de definir esta variável no audio.cpp ou main.cpp
extern bool showOpcoes;
extern int selOpcao;
extern bool editMode;

// Funções de Áudio externas
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

void processarComando(uint32_t buttons, int32_t userId) {
    static int cd = 0;
    static bool pCross = false, pCircle = false, pTri = false;

    // Se o teclado virtual estiver na tela, o sistema processa os inputs
    if (tecladoAtivo) return;

    // 1. Processamento no Modo de Edição
    if (editMode) {
        processarControlesEdicao(buttons);
        return;
    }

    // 2. Processamento de Navegação (D-PAD Cima/Baixo)
    if (buttons & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
        if (cd <= 0) {
            if (showOpcoes) {
                // Navegação dentro dos menus contextuais (Opções de Áudio ou Arquivos)
                if (menuAtual == MENU_AUDIO_OPCOES) {
                    if (buttons & ORBIS_PAD_BUTTON_DOWN && selAudioOpcao < 10) selAudioOpcao++;
                    else if (buttons & ORBIS_PAD_BUTTON_UP && selAudioOpcao > 0) selAudioOpcao--;
                }
                else {
                    if (buttons & ORBIS_PAD_BUTTON_DOWN && selOpcao < 9) selOpcao++;
                    else if (buttons & ORBIS_PAD_BUTTON_UP && selOpcao > 0) selOpcao--;
                }
            }
            else {
                // Navegação na lista principal
                if (buttons & ORBIS_PAD_BUTTON_DOWN && sel < (totalItens - 1)) {
                    sel++;
                    if (sel >= (off + 6)) off++;
                }
                else if (buttons & ORBIS_PAD_BUTTON_UP && sel > 0) {
                    sel--;
                    if (sel < off) off--;
                }
            }
            cd = 10;
        }
    }
    else cd = 0;
    if (cd > 0) cd--;

    // 3. Botão CROSS (Confirmar/Ação)
    if (buttons & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCross) {
            if (showOpcoes) {
                if (menuAtual == MENU_AUDIO_OPCOES) tratarSelecaoAudio(selAudioOpcao);
                else acaoArquivo(selOpcao);
            }
            else if (menuAtual == ROOT) {
                if (sel == 0) carregarXML("/app0/assets/lista.xml");
                else if (sel == 1) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "CAPAS"); totalItens = 1; menuAtual = MENU_BAIXAR; }
                else if (sel == 2) preencherMenuEditar();
                else if (sel == 3) preencherExplorerHome();
                else if (sel == 4) preencherMenuMusicas();
                else if (sel == 5) { menuAtual = MENU_NOTEPAD; memset(bufferTecladoC, 0, sizeof(bufferTecladoC)); }
            }
            else if (menuAtual == MENU_NOTEPAD) {
                // Inicialização do teclado virtual para o Bloco de Notas
                OrbisImeDialogSetting ime; memset(&ime, 0, sizeof(ime));
                ime.userId = userId;
                ime.type = (OrbisImeType)0;
                ime.maxTextLength = 127;

                // Sincroniza o texto atual para o buffer de memória direta
                memset(bufferTecladoW, 0, 1024);
                for (int i = 0; i < 127; i++) {
                    bufferTecladoW[i] = (uint16_t)bufferTecladoC[i];
                    if (bufferTecladoC[i] == '\0') break;
                }

                ime.inputTextBuffer = (wchar_t*)bufferTecladoW;
                ime.title = (wchar_t*)pImeTitle;

                if (sceImeDialogInit(&ime, NULL) >= 0) tecladoAtivo = true;
            }
            else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) carregarXML("/app0/assets/sp.xml");
            else if (menuAtual == MENU_BAIXAR) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
            else if (menuAtual == MENU_CAPAS) { memset(nomes, 0, sizeof(nomes)); for (int i = 0;i < 5;i++) strcpy(nomes[i], listaConsoles[i].nome); totalItens = 5; menuAtual = MENU_CONSOLES; }
            else if (menuAtual == MENU_CONSOLES) { consoleAtual = sel; acaoRede(NULL, true, false); }
            else if (menuAtual == MENU_EDITAR) { if (sel == 3) salvarConfiguracao(); else { editType = sel; preencherMenuEditTarget(); } }
            else if (menuAtual == MENU_EDIT_TARGET) { if (sel == 4) { salvarConfiguracao(); preencherMenuEditar(); } else { editTarget = sel; editMode = true; } }
            else if (menuAtual == SCRAPER_LIST) acaoRede(nomes[sel], false, true);
            else if (menuAtual == MENU_EXPLORAR_HOME) {
                if (sel == 0) { strcpy(baseRaiz, "/data/HyperNeiva"); listarDiretorio(baseRaiz); }
                else if (sel == 1) { strcpy(baseRaiz, "/"); listarDiretorio(baseRaiz); }
                else if (sel == 2) { strcpy(baseRaiz, "/mnt/usb0"); listarDiretorio(baseRaiz); }
                else if (sel == 3) { strcpy(baseRaiz, "/mnt/usb1"); listarDiretorio(baseRaiz); }
            }
            else if (menuAtual == MENU_EXPLORAR) {
                if (nomes[sel][0] == '[') {
                    char pL[128]; strncpy(pL, &nomes[sel][1], strlen(nomes[sel]) - 2); pL[strlen(nomes[sel]) - 2] = '\0';
                    char nP[256]; sprintf(nP, "%s/%s", pathExplorar, pL); listarDiretorio(nP);
                }
            }
            else if (menuAtual == MENU_MUSICAS) {
                if (sel == 0) tocarMusicaNova("PARADO");
                else { char mPath[256]; sprintf(mPath, "/data/HyperNeiva/Musicas/%s", nomes[sel]); tocarMusicaNova(mPath); }
            }
            if (!editMode && !showOpcoes && menuAtual != SCRAPER_LIST && menuAtual != JOGAR_XML && menuAtual != MENU_NOTEPAD) { sel = 0; off = 0; }
            pCross = true;
        }
    }
    else pCross = false;

    // 4. Botão CIRCLE (Voltar)
    if (buttons & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircle) {
            if (showOpcoes) {
                showOpcoes = false;
                if (menuAtual == MENU_AUDIO_OPCOES) menuAtual = MENU_MUSICAS;
            }
            else if (menuAtual == MENU_NOTEPAD) preencherRoot();
            else if (menuAtual == JOGAR_XML) { if (strstr(xmlCaminhoAtual, "sp.xml")) carregarXML("/app0/assets/lista.xml"); else preencherRoot(); }
            else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_EDITAR || menuAtual == MENU_EXPLORAR_HOME || menuAtual == MENU_MUSICAS) preencherRoot();
            else if (menuAtual == MENU_CAPAS) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "CAPAS"); totalItens = 1; menuAtual = MENU_BAIXAR; }
            else if (menuAtual == MENU_CONSOLES) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
            else if (menuAtual == SCRAPER_LIST) { memset(nomes, 0, sizeof(nomes)); for (int i = 0;i < 5;i++) strcpy(nomes[i], listaConsoles[i].nome); totalItens = 5; menuAtual = MENU_CONSOLES; }
            else if (menuAtual == MENU_EDIT_TARGET) preencherMenuEditar();
            else if (menuAtual == MENU_EXPLORAR) {
                if (strcmp(pathExplorar, baseRaiz) == 0) preencherExplorerHome();
                else { char* last = strrchr(pathExplorar, '/'); if (last) { if (last == pathExplorar) strcpy(pathExplorar, "/"); else *last = '\0'; listarDiretorio(pathExplorar); } }
            }
            if (!showOpcoes) { sel = 0; off = 0; }
            pCircle = true;
        }
    }
    else pCircle = false;

    // 5. Botões Contextuais (Triangle e R1)
    if (menuAtual == MENU_EXPLORAR) {
        if (buttons & ORBIS_PAD_BUTTON_R1) { if (cd <= 0) { marcados[sel] = !marcados[sel]; cd = 12; } }
        if (buttons & ORBIS_PAD_BUTTON_TRIANGLE) { if (!pTri) { showOpcoes = !showOpcoes; selOpcao = 0; pTri = true; } }
        else pTri = false;
    }
    else if (menuAtual == MENU_MUSICAS) {
        if (buttons & ORBIS_PAD_BUTTON_TRIANGLE) { if (!pTri) { abrirMenuAudioOpcoes(); pTri = true; } }
        else pTri = false;
    }
}