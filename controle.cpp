// --- INÍCIO DO ARQUIVO controle.cpp ---
#include "controle.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Correção para IntelliSense não reclamar do va_list
#ifdef __INTELLISENSE__
#define __builtin_va_list va_list
#endif

// Inclui o stb_image para podermos usar stbi_load e stbi_image_free no preview do scraper
#include "stb_image.h"

// Aqui nós NÃO incluímos globals.h nem menu.h diretamente para evitar o erro de redefinição,
// pois eles já vêm "de carona" com os includes abaixo:

#include "menu.h"
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"



// Globais definidas aqui para o linker achar
int cd = 0;
bool pCross = false;
bool pCircle = false;
bool pTri = false;

// 2. Variáveis externas
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
extern bool tecladoAtivo;
extern uint16_t* bufferTecladoW;
extern char bufferTecladoC[128];
extern char msgStatus[128];
extern int msgTimer;

// 3. Funções do main.cpp e audio.cpp que o controle precisa chamar
extern void preencherRoot();
extern void preencherExplorerHome();
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

// Funções stub para caso outros arquivos as chamem
void executarAcaoX() {
    // Lógica específica do X em outros arquivos
}

void executarAcaoBolinha() {
    // Lógica específica da Bolinha em outros arquivos
}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (editMode) {
        processarControlesEdicao(botoes);
    }
    else {
        // D-PAD Cima/Baixo
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

        if (menuAtual == SCRAPER_LIST && strcmp(nomes[sel], ultimoJogoCarregado) != 0) {
            char cp[256]; sprintf(cp, "/data/HyperNeiva/baixado/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]);
            FILE* fEx = fopen(cp, "rb");
            if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); strcpy(ultimoJogoCarregado, nomes[sel]); }
            else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } strcpy(ultimoJogoCarregado, ""); }
        }

        // CROSS (Confirmar)
        if (botoes & ORBIS_PAD_BUTTON_CROSS) {
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
                    else if (sel == 5) {
                        menuAtual = MENU_NOTEPAD;
                        memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
                    }
                }
                else if (menuAtual == MENU_NOTEPAD) {
                    memset(imeSetting, 0, sizeof(OrbisImeDialogSetting));
                    imeSetting->userId = uId;
                    imeSetting->type = (OrbisImeType)0;
                    imeSetting->maxTextLength = 127;

                    memset(bufferTecladoW, 0, 1024);
                    for (int i = 0; i < 127; i++) {
                        bufferTecladoW[i] = (uint16_t)bufferTecladoC[i];
                        if (bufferTecladoC[i] == '\0') break;
                    }

                    imeSetting->inputTextBuffer = (wchar_t*)bufferTecladoW;
                    imeSetting->title = (wchar_t*)imeTitle;

                    if (sceImeDialogInit(imeSetting, NULL) >= 0) {
                        tecladoAtivo = true;
                    }
                    else {
                        sprintf(msgStatus, "ERRO AO ABRIR TECLADO");
                        msgTimer = 90;
                    }
                }
                else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) carregarXML("/app0/assets/sp.xml");
                else if (menuAtual == MENU_BAIXAR) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
                else if (menuAtual == MENU_CAPAS) { memset(nomes, 0, sizeof(nomes)); for (int i = 0;i < 5;i++) strcpy(nomes[i], listaConsoles[i].nome); totalItens = 5; menuAtual = MENU_CONSOLES; }
                else if (menuAtual == MENU_CONSOLES) { consoleAtual = sel; acaoRede(NULL, true, false); }
                else if (menuAtual == MENU_EDITAR) {
                    if (sel == 3) { listX = dLX; listY = dLY; listW = dLW; listH = dLH; capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH; discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH; salvarConfiguracao(); }
                    else { editType = sel; preencherMenuEditTarget(); }
                }
                else if (menuAtual == MENU_EDIT_TARGET) {
                    if (sel == 4) { if (editType == 0) { listX = dLX;listY = dLY; } else if (editType == 1) { listW = dLW;listH = dLH; } salvarConfiguracao(); preencherMenuEditar(); }
                    else { editTarget = sel; editMode = true; }
                }
                else if (menuAtual == SCRAPER_LIST) { acaoRede(nomes[sel], false, true); }
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
                    if (sel == 0) { tocarMusicaNova("PARADO"); }
                    else {
                        char mPath[256];
                        sprintf(mPath, "/data/HyperNeiva/Musicas/%s", nomes[sel]);
                        tocarMusicaNova(mPath);
                    }
                }

                if (!editMode && !showOpcoes && menuAtual != SCRAPER_LIST && menuAtual != JOGAR_XML && menuAtual != MENU_NOTEPAD) { sel = 0; off = 0; }
                executarAcaoX();
                pCross = true;
            }
        }
        else pCross = false;

        // CIRCLE (Voltar)
        if (botoes & ORBIS_PAD_BUTTON_CIRCLE) {
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
                    if (strcmp(pathExplorar, baseRaiz) == 0) { preencherExplorerHome(); }
                    else { char* last = strrchr(pathExplorar, '/'); if (last) { if (last == pathExplorar) strcpy(pathExplorar, "/"); else *last = '\0'; listarDiretorio(pathExplorar); } }
                }
                if (!showOpcoes) { sel = 0; off = 0; }
                executarAcaoBolinha();
                pCircle = true;
            }
        }
        else pCircle = false;

        // TRIANGLE E R1
        if (menuAtual == MENU_EXPLORAR) {
            if (botoes & ORBIS_PAD_BUTTON_R1) { if (cd <= 0) { marcados[sel] = !marcados[sel]; cd = 12; } }
            if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) { if (!pTri) { showOpcoes = !showOpcoes; selOpcao = 0; pTri = true; } }
            else pTri = false;
        }
        else if (menuAtual == MENU_MUSICAS) {
            if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) {
                if (!pTri) { abrirMenuAudioOpcoes(); pTri = true; }
            }
            else pTri = false;
        }
        else if (menuAtual != MENU_AUDIO_OPCOES) {
            showOpcoes = false;
        }
    }
}
// --- FIM DO ARQUIVO controle.cpp ---