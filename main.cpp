// 1. Bibliotecas Padrão C/C++
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>

// 2. Correção de IntelliSense / Analisadores de Código (Sempre APÓS as libs padrão)
// O #ifdef __INTELLISENSE__ garante que o Clang (PS4) ignore isto e não dê erro de sintaxe.
#ifdef __INTELLISENSE__
#define __builtin_va_list va_list
#endif

// 3. Headers do SDK do PS4 (OpenOrbis)
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>

// 4. Headers Locais do Projeto
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"

// 5. Definições de fallback para Menus
#ifndef MENU_MUSICAS
#define MENU_MUSICAS ((MenuLevel)11)
#endif
#ifndef MENU_AUDIO_OPCOES
#define MENU_AUDIO_OPCOES ((MenuLevel)12)
#endif
#ifndef MENU_NOTEPAD
#define MENU_NOTEPAD ((MenuLevel)13)
#endif

// Implementação das bibliotecas STB
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// --- DECLARAÇÕES DO MENU DE ÁUDIO (Link com audio.cpp) ---
int selAudioOpcao = 0;
extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

// --- VARIÁVEIS GLOBAIS ---
MenuLevel menuAtual = ROOT;

char nomes[3000][64];
int totalItens = 0, sel = 0, off = 0, bA = 0, video = 0;
char msgStatus[128] = "SISTEMA PRONTO"; int msgTimer = 0;

// Variáveis Seguras para o Teclado do PS4 (Partilhadas com o Sistema Operacional)
bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL; // Ponteiro para a Memória Direta segura
char bufferTecladoC[128] = "";   // Buffer normal para desenhar na tela depois

// Assets
unsigned char* capasAssets[6], * discosAssets[6], * backImg = NULL;
int wC[6], hC[6], cC[6], wD[6], hD[6], cD[6], wB, hB, cB;

// --- FUNÇÕES DE SISTEMA ---
void inicializarPastas() {
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/xml", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
}

void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "JOGAR"); strcpy(nomes[1], "BAIXAR");
    strcpy(nomes[2], "EDITAR"); strcpy(nomes[3], "EXPLORAR");
    strcpy(nomes[4], "MUSICAS"); strcpy(nomes[5], "CRIAR"); // Adicionado a opção CRIAR (Notepad)
    totalItens = 6; menuAtual = ROOT;
}

void preencherExplorerHome() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva"); strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB0"); strcpy(nomes[3], "USB1");
    totalItens = 4; menuAtual = MENU_EXPLORAR_HOME;
}

// --- MAIN LOOP ---
int main(void) {
    // Inicialização do Sistema Orbis e Rede
    initNetwork();
    inicializarAudio();

    // Inicializa o sistema de Dialogs do PS4 e Teclado
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);

    sceCommonDialogInitialize();

    sceUserServiceInitialize(NULL); int32_t uId; sceUserServiceGetInitialUser(&uId);
    scePadInit(); int pad = scePadOpen(uId, 0, 0, NULL);
    video = sceVideoOutOpen(255, 0, 0, NULL);

    size_t bSz = ((1920 * 1080 * 4) + 0x1FFFFF) & ~0x1FFFFF;
    off_t ph; sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz * 2, 2097152, 2, &ph);
    void* vM = NULL; sceKernelMapDirectMemory(&vM, bSz * 2, 0x33, 0, ph, 2097152);
    void* buffers[2] = { vM, (void*)((uint8_t*)vM + bSz) };
    OrbisVideoOutBufferAttribute attr; memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, 1920, 1080, 1920);
    sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr);

    // =========================================================================
    // LÓGICA DE MEMÓRIA DIRETA PARA O TECLADO (Resolve o Crash da Tecla)
    // =========================================================================
    off_t imePh;
    void* imeVm = NULL;
    // Alocamos 2MB de memória protegida e partilhável
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 2097152, 2097152, 2, &imePh);
    sceKernelMapDirectMemory(&imeVm, 2097152, 0x33, 0, imePh, 2097152);

    OrbisImeDialogSetting* imeSetting = (OrbisImeDialogSetting*)imeVm;
    bufferTecladoW = (uint16_t*)((uint8_t*)imeVm + 1024); // Buffer do teclado dentro da memória segura
    uint16_t* imeTitle = (uint16_t*)((uint8_t*)bufferTecladoW + 1024);

    // Escrevemos o título nativamente em UTF-16
    uint16_t t[] = { 'E','s','c','r','e','v','a',' ','n','a',' ','F','o','l','h','a','\0' };
    memcpy(imeTitle, t, sizeof(t));
    // =========================================================================

    inicializarPastas(); carregarConfiguracao();

    // Carregamento de Fonte e Imagens
    int fd = sceKernelOpen("/app0/assets/fonts/font.ttf", 0, 0);
    if (fd >= 0) {
        off_t fs = sceKernelLseek(fd, 0, 2); sceKernelLseek(fd, 0, 0);
        size_t mSz = (fs + 0x1FFFFF) & ~0x1FFFFF; off_t p;
        unsigned char* ttf = NULL;
        sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), mSz, 2097152, 2, &p);
        sceKernelMapDirectMemory((void**)&ttf, mSz, 0x01 | 0x02, 0, p, 2097152);
        sceKernelRead(fd, ttf, fs); sceKernelClose(fd);
        temF = stbtt_InitFont(&font, ttf, 0);
    }
    backImg = stbi_load("/app0/assets/images/background.png", &wB, &hB, &cB, 4);
    for (int i = 0; i < 6; i++) {
        char pC[128], pD[128]; sprintf(pC, "/app0/assets/images/capa%d.png", i + 1); sprintf(pD, "/app0/assets/images/disco%d.png", i + 1);
        capasAssets[i] = stbi_load(pC, &wC[i], &hC[i], &cC[i], 4); discosAssets[i] = stbi_load(pD, &wD[i], &hD[i], &cD[i], 4);
    }

    preencherRoot();
    int cd = 0; bool pCross = false, pCircle = false, pTri = false;

    // Loop de Frame
    for (;;) {
        OrbisPadData pData; scePadReadState(pad, &pData);
        uint32_t* p = (uint32_t*)buffers[bA];
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212; // Clear
        if (backImg) desenharRedimensionado(p, backImg, wB, hB, backW, backH, backX, backY); // Draw BG

        // --- LÓGICA CORRIGIDA E BLINDADA DO TECLADO NATIVO DO PS4 ---
        if (tecladoAtivo) {
            int stat = (int)sceImeDialogGetStatus();

            if (stat == ORBIS_COMMON_DIALOG_STATUS_FINISHED) {
                // Lemos diretamente o bufferTecladoW preenchido pelo OS da consola de forma segura
                if (bufferTecladoW[0] != 0) {
                    // Conversão manual 100% segura de UTF-16 do PS4 para ASCII C
                    for (int i = 0; i < 127; i++) {
                        bufferTecladoC[i] = (char)bufferTecladoW[i];
                        if (bufferTecladoW[i] == 0) break; // Fim da string
                    }
                    bufferTecladoC[127] = '\0';

                    sprintf(msgStatus, "TEXTO ATUALIZADO!");
                    msgTimer = 120;
                }
                else {
                    memset(bufferTecladoC, 0, sizeof(bufferTecladoC)); // Limpa a tela se apagaram tudo
                    sprintf(msgStatus, "CANCELADO / VAZIO");
                    msgTimer = 90;
                }

                sceImeDialogTerm();
                tecladoAtivo = false;
            }

            // Pula a leitura normal do controle enquanto o teclado estiver na tela
            goto FIM_CONTROLES;
        }

        // 1. Processamento de Controles (Modo Edição)
        if (editMode) {
            processarControlesEdicao(pData.buttons);
        }
        // 2. Processamento de Controles (Navegação Padrão)
        else {
            // D-PAD Cima/Baixo
            if (pData.buttons & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
                if (cd <= 0) {
                    if (showOpcoes) {
                        if (menuAtual == MENU_AUDIO_OPCOES) {
                            if (pData.buttons & ORBIS_PAD_BUTTON_DOWN && selAudioOpcao < 10) selAudioOpcao++;
                            else if (pData.buttons & ORBIS_PAD_BUTTON_UP && selAudioOpcao > 0) selAudioOpcao--;
                        }
                        else {
                            if (pData.buttons & ORBIS_PAD_BUTTON_DOWN && selOpcao < 9) selOpcao++;
                            else if (pData.buttons & ORBIS_PAD_BUTTON_UP && selOpcao > 0) selOpcao--;
                        }
                    }
                    else {
                        if (pData.buttons & ORBIS_PAD_BUTTON_DOWN && sel < (totalItens - 1)) { sel++; if (sel >= (off + 6)) off++; }
                        else if (pData.buttons & ORBIS_PAD_BUTTON_UP && sel > 0) { sel--; if (sel < off) off--; }
                    }
                    cd = 10;
                }
            }
            else cd = 0;
            if (cd > 0) cd--;

            // Lógica de Scraper (Carregar imagem preview em background)
            if (menuAtual == SCRAPER_LIST && strcmp(nomes[sel], ultimoJogoCarregado) != 0) {
                char cp[256]; sprintf(cp, "/data/HyperNeiva/baixado/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].nome, nomes[sel]);
                FILE* fEx = fopen(cp, "rb");
                if (fEx) { fclose(fEx); if (imgPreview) stbi_image_free(imgPreview); imgPreview = stbi_load(cp, &wP, &hP, &cP, 4); strcpy(ultimoJogoCarregado, nomes[sel]); }
                else { if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; } strcpy(ultimoJogoCarregado, ""); }
            }

            // CROSS (Confirmar)
            if (pData.buttons & ORBIS_PAD_BUTTON_CROSS) {
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
                            // Abre o Menu "Bloco de Notas"
                            menuAtual = MENU_NOTEPAD;
                            memset(bufferTecladoC, 0, sizeof(bufferTecladoC)); // Inicia folha limpa
                        }
                    }
                    // NOVA LÓGICA DO BLOCO DE NOTAS (NOTEPAD) - Abrir Teclado
                    else if (menuAtual == MENU_NOTEPAD) {
                        memset(imeSetting, 0, sizeof(OrbisImeDialogSetting));

                        imeSetting->userId = uId;
                        imeSetting->type = (OrbisImeType)0; // ORBIS_TYPE_DEFAULT equivalente
                        imeSetting->maxTextLength = 127;

                        // Sincroniza o texto atual do bloco de notas de volta para o teclado para edição contínua
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
                    pCross = true;
                }
            }
            else pCross = false;

            // CIRCLE (Voltar)
            if (pData.buttons & ORBIS_PAD_BUTTON_CIRCLE) {
                if (!pCircle) {
                    if (showOpcoes) {
                        showOpcoes = false;
                        if (menuAtual == MENU_AUDIO_OPCOES) menuAtual = MENU_MUSICAS;
                    }
                    else if (menuAtual == MENU_NOTEPAD) {
                        // Sair do bloco de notas para o menu principal
                        preencherRoot();
                    }
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
                    pCircle = true;
                }
            }
            else pCircle = false;

            // TRIANGLE E R1 (Menus de Opções Contextuais)
            if (menuAtual == MENU_EXPLORAR) {
                if (pData.buttons & ORBIS_PAD_BUTTON_R1) { if (cd <= 0) { marcados[sel] = !marcados[sel]; cd = 12; } }
                if (pData.buttons & ORBIS_PAD_BUTTON_TRIANGLE) { if (!pTri) { showOpcoes = !showOpcoes; selOpcao = 0; pTri = true; } }
                else pTri = false;
            }
            else if (menuAtual == MENU_MUSICAS) { // Abrir Menu de Áudio com Triângulo
                if (pData.buttons & ORBIS_PAD_BUTTON_TRIANGLE) {
                    if (!pTri) { abrirMenuAudioOpcoes(); pTri = true; }
                }
                else pTri = false;
            }
            else if (menuAtual != MENU_AUDIO_OPCOES) {
                showOpcoes = false;
            }
        }

    FIM_CONTROLES:
        // 3. Renderização da Lista e Elementos Visuais (Apenas fora do Notepad)
        if (menuAtual != MENU_NOTEPAD) {
            for (int i = 0; i < 6; i++) {
                int gIdx = i + off; if (gIdx >= totalItens) break;
                int yP = listY + (i * 120);

                // Cores dinâmicas combinadas (Seleção Padrão vs Marcação do Explorador)
                uint32_t corFundo = 0xAA222222;
                uint32_t corTexto = 0xFFFFFFFF;

                if (menuAtual == MENU_EXPLORAR && marcados[gIdx]) { corFundo = 0xAAFFFF99; }
                if (gIdx == sel) { corFundo = 0xFF00AAFF; corTexto = 0xFF000000; } // Selecionado sobrepõe marcação

                for (int by = 0; by < listH; by++) for (int bx = 0; bx < listW; bx++) {
                    int px = listX + bx; int py = yP + by; if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = corFundo;
                }
                desenharTexto(p, nomes[gIdx], 35, listX + 20, yP + 20, corTexto);
            }
        }

        // ==========================================
        // UI DO BLOCO DE NOTAS (NOTEPAD)
        // ==========================================
        if (menuAtual == MENU_NOTEPAD) {
            // Desenha folha de papel branca/cinza claro
            for (int by = 0; by < 700; by++) {
                for (int bx = 0; bx < 1400; bx++) {
                    int px = 260 + bx;
                    int py = 150 + by;
                    if (px >= 0 && px < 1920 && py >= 0 && py < 1080) {
                        p[py * 1920 + px] = 0xFFEEEEEE;
                    }
                }
            }

            // Desenha a barra superior decorativa (Azul escuro)
            for (int by = 0; by < 60; by++) {
                for (int bx = 0; bx < 1400; bx++) {
                    int px = 260 + bx;
                    int py = 150 + by;
                    if (px >= 0 && px < 1920 && py >= 0 && py < 1080) {
                        p[py * 1920 + px] = 0xFFD05050; // Formato ABGR -> Azul D0, Verde 50, Vermelho 50
                    }
                }
            }

            // Textos de instrução do Notepad (Ajustado para Y = 160)
            desenharTexto(p, "BLOCO DE NOTAS", 40, 280, 160, 0xFFFFFFFF);
            desenharTexto(p, "[X] Escrever   [O] Voltar", 30, 1200, 160, 0xFFFFFFFF);

            // Texto escrito pelo usuário (Renderizado em preto)
            desenharTexto(p, bufferTecladoC, 40, 280, 260, 0xFF000000);
        }

        // Imagens do Menu (Jogos/Capa/Disco)
        if (menuAtual == JOGAR_XML || editMode) {
            int idx = sel % 6;
            if (capasAssets[idx]) desenharRedimensionado(p, capasAssets[idx], wC[idx], hC[idx], capaW, capaH, capaX, capaY);
            if (discosAssets[idx]) desenharDiscoRedondo(p, discosAssets[idx], wD[idx], hD[idx], discoW, discoH, discoX, discoY);
        }
        else if (menuAtual == SCRAPER_LIST && imgPreview) {
            desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
        }
        // Breadcrumbs do Explorador
        else if (menuAtual == MENU_EXPLORAR) {
            char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar);
            desenharTexto(p, bread, 30, listX, 1020, 0xFFFFFFFF);
        }

        // Menu de Opções Suspenso do Explorador
        if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) {
            for (int my = 0; my < 500; my++) for (int mx = 0; mx < 350; mx++) {
                int px = discoX + mx; int py = discoY - 100 + my; if (px < 1920 && py < 1080 && py >= 0) p[py * 1920 + px] = 0xEE111111;
            }
            for (int i = 0; i < 10; i++) {
                uint32_t corOp = (i == selOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
                desenharTexto(p, listaOpcoes[i], 30, discoX + 20, discoY - 80 + (i * 45), corOp);
            }
        }

        // Menu de Opções Suspenso do Áudio
        if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
            for (int my = 0; my < 550; my++) {
                for (int mx = 0; mx < 350; mx++) {
                    int px = listX + 600 + mx; // Posição X 
                    int py = listY + my;       // Posição Y 
                    if (px < 1920 && py < 1080 && py >= 0) p[py * 1920 + px] = 0xEE111111;
                }
            }
            for (int i = 0; i < 11; i++) {
                uint32_t corOp = (i == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
                desenharTexto(p, listaOpcoesAudio[i], 30, listX + 620, listY + 50 + (i * 45), corOp);
            }
        }

        // Status
        if (msgTimer > 0) { desenharTexto(p, msgStatus, 40, 100, 950, 0xFFFFFFFF); msgTimer--; }

        sceVideoOutSubmitFlip(video, bA, 1, 0); bA = (bA + 1) % 2; sceKernelUsleep(16000);
    }
}