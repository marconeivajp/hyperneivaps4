// 1. Bibliotecas Padrão C/C++
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>

// 2. CORREÇÃO DE INTELLISENSE PARA VISUAL STUDIO (Remove linhas vermelhas e erros de sintaxe)
#ifdef __INTELLISENSE__
#undef __builtin_va_list
#define __builtin_va_list int
#define __builtin_va_start(a,b)
#define __builtin_va_end(a)
#define __builtin_va_arg(a,b) ((b)0)
#define __attribute__(x)
#define __inline__ inline
typedef int OrbisSysModuleInternal;
typedef int OrbisSysModule;
typedef int OrbisImeType;
typedef int OrbisButtonLabel;
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
#include "menu.h"
#include "controle.h"
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"

// 5. Definições de Fallback para Menus (Necessário para a renderização)
#ifndef MENU_MUSICAS
#define MENU_MUSICAS ((MenuLevel)11)
#endif
#ifndef MENU_AUDIO_OPCOES
#define MENU_AUDIO_OPCOES ((MenuLevel)12)
#endif
#ifndef MENU_NOTEPAD
#define MENU_NOTEPAD ((MenuLevel)13)
#endif

// Implementação das bibliotecas Single-Header (STB)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// --- VARIÁVEIS GLOBAIS DE SISTEMA (Gestão de Hardware) ---
int video = 0, bA = 0;
bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL;
uint16_t* pImeTitle = NULL;
char bufferTecladoC[128] = "";

// Assets Visuais
unsigned char* capasAssets[6], * discosAssets[6], * backImg = NULL;
int wC[6], hC[6], cC[6], wD[6], hD[6], cD[6], wB, hB, cB;

// --- DECLARAÇÕES EXTERNAS (Resolvendo símbolos de outros módulos) ---
extern unsigned char* imgPreview;
extern int wP, hP, cP;

// CORREÇÃO DE LINKER: Padronizado para 64 conforme explorar.h para evitar erro de redeclaração
extern char ultimoJogoCarregado[64];

// Variáveis e funções do módulo de Áudio (Exibição no HUD)
extern int selAudioOpcao;
extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

// --- FUNÇÕES DE INICIALIZAÇÃO ---

void inicializarPastas() {
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/xml", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
}

int main(void) {
    // Inicialização da infraestrutura básica
    initNetwork();
    inicializarAudio();

    // Carregamento de Módulos de Sistema (Common Dialog e IME)
    sceSysmoduleLoadModuleInternal((OrbisSysModuleInternal)0x0080);
    sceSysmoduleLoadModule((OrbisSysModule)0x00A2);
    sceCommonDialogInitialize();

    // Inicialização de Utilizador e Pad (Controle)
    sceUserServiceInitialize(NULL);
    int32_t uId;
    sceUserServiceGetInitialUser(&uId);
    scePadInit();
    int pad = scePadOpen(uId, 0, 0, NULL);

    // Abertura da saída de vídeo
    video = sceVideoOutOpen(255, 0, 0, NULL);

    // Configuração de Double Buffering em Memória Direta
    size_t bSz = ((1920 * 1080 * 4) + 0x1FFFFF) & ~0x1FFFFF;
    off_t ph;
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz * 2, 2097152, 2, &ph);
    void* vM = NULL;
    sceKernelMapDirectMemory(&vM, bSz * 2, 0x33, 0, ph, 2097152);
    void* buffers[2] = { vM, (void*)((uint8_t*)vM + bSz) };

    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, 1920, 1080, 1920);
    sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr);

    // Configuração de Memória Direta para o Teclado (IME)
    off_t imePh; void* imeVm = NULL;
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 2097152, 2097152, 2, &imePh);
    sceKernelMapDirectMemory(&imeVm, 2097152, 0x33, 0, imePh, 2097152);
    bufferTecladoW = (uint16_t*)((uint8_t*)imeVm + 1024);
    pImeTitle = (uint16_t*)((uint8_t*)bufferTecladoW + 1024);

    // Título nativo do teclado em UTF-16
    uint16_t t[] = { 'E','s','c','r','e','v','a',' ','n','a',' ','F','o','l','h','a','\0' };
    memcpy(pImeTitle, t, sizeof(t));

    // Preparação do sistema de pastas e carregamento de configs
    inicializarPastas();
    carregarConfiguracao();

    // Carregamento de Fontes TrueType
    int fd = sceKernelOpen("/app0/assets/fonts/font.ttf", 0, 0);
    if (fd >= 0) {
        off_t fs = sceKernelLseek(fd, 0, 2); sceKernelLseek(fd, 0, 0);
        size_t mSz = (fs + 0x1FFFFF) & ~0x1FFFFF; off_t pFont;
        unsigned char* ttf = NULL;
        sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), mSz, 2097152, 2, &pFont);
        sceKernelMapDirectMemory((void**)&ttf, mSz, 0x01 | 0x02, 0, pFont, 2097152);
        sceKernelRead(fd, ttf, fs); sceKernelClose(fd);
        temF = stbtt_InitFont(&font, ttf, 0);
    }

    // Carregamento de Imagens Assets
    backImg = stbi_load("/app0/assets/images/background.png", &wB, &hB, &cB, 4);
    for (int i = 0; i < 6; i++) {
        char pC[128], pD[128];
        sprintf(pC, "/app0/assets/images/capa%d.png", i + 1);
        sprintf(pD, "/app0/assets/images/disco%d.png", i + 1);
        capasAssets[i] = stbi_load(pC, &wC[i], &hC[i], &cC[i], 4);
        discosAssets[i] = stbi_load(pD, &wD[i], &hD[i], &cD[i], 4);
    }

    // Define o menu inicial como Root
    preencherRoot();

    // --- LOOP PRINCIPAL DE FRAME ---
    for (;;) {
        OrbisPadData pData;
        scePadReadState(pad, &pData);
        uint32_t* pFrame = (uint32_t*)buffers[bA];

        // 1. Limpeza do Frame e Desenho do Fundo
        for (int i = 0; i < 1920 * 1080; i++) pFrame[i] = 0xFF121212;
        if (backImg) desenharRedimensionado(pFrame, backImg, wB, hB, backW, backH, backX, backY);

        // 2. PROCESSAR ENTRADAS DO COMANDO (Módulo controle.cpp)
        processarComando(pData.buttons, uId);

        // 3. TRATAMENTO DO TECLADO VIRTUAL (IME)
        if (tecladoAtivo) {
            int stat = (int)sceImeDialogGetStatus();
            if (stat == 3 /* FINISHED */) {
                if (bufferTecladoW[0] != 0) {
                    for (int i = 0; i < 127; i++) {
                        bufferTecladoC[i] = (char)bufferTecladoW[i];
                        if (bufferTecladoW[i] == 0) break;
                    }
                    bufferTecladoC[127] = '\0';
                    sprintf(msgStatus, "TEXTO ATUALIZADO!"); msgTimer = 120;
                }
                sceImeDialogTerm(); tecladoAtivo = false;
            }
            goto SUBMIT_FRAME;
        }

        // 4. RENDERIZAÇÃO DA INTERFACE DO UTILIZADOR
        if (menuAtual == MENU_NOTEPAD) {
            // Renderização do Bloco de Notas
            for (int by = 150; by < 850; by++) for (int bx = 260; bx < 1660; bx++) pFrame[by * 1920 + bx] = 0xFFEEEEEE;
            for (int by = 150; by < 210; by++) for (int bx = 260; bx < 1660; bx++) pFrame[by * 1920 + bx] = 0xFFD05050;
            desenharTexto(pFrame, "BLOCO DE NOTAS", 40, 280, 160, 0xFFFFFFFF);
            desenharTexto(pFrame, "[X] Escrever   [O] Voltar", 30, 1200, 160, 0xFFFFFFFF);
            desenharTexto(pFrame, bufferTecladoC, 40, 280, 260, 0xFF000000);
        }
        else {
            // Renderização das Listas Genéricas
            for (int i = 0; i < 6; i++) {
                int gIdx = i + off; if (gIdx >= totalItens) break;
                int yP = listY + (i * 120);

                uint32_t corFundo = (menuAtual == MENU_EXPLORAR && marcados[gIdx]) ? 0xAAFFFF99 : 0xAA222222;
                uint32_t corTexto = (gIdx == sel) ? 0xFF000000 : 0xFFFFFFFF;
                if (gIdx == sel) corFundo = 0xFF00AAFF;

                for (int by = 0; by < listH; by++) for (int bx = 0; bx < listW; bx++) {
                    int px = listX + bx; int py = yP + by; if (px >= 0 && px < 1920 && py >= 0 && py < 1080) pFrame[py * 1920 + px] = corFundo;
                }
                desenharTexto(pFrame, nomes[gIdx], 35, listX + 20, yP + 20, corTexto);
            }
        }

        // 5. RENDERIZAÇÃO DE ASSETS DINÂMICOS (Capas, Discos e Scraper)
        if (menuAtual == JOGAR_XML || editMode) {
            int idx = sel % 6;
            if (capasAssets[idx]) desenharRedimensionado(pFrame, capasAssets[idx], wC[idx], hC[idx], capaW, capaH, capaX, capaY);
            if (discosAssets[idx]) desenharDiscoRedondo(pFrame, discosAssets[idx], wD[idx], hD[idx], discoW, discoH, discoX, discoY);
        }
        else if (menuAtual == SCRAPER_LIST && imgPreview) {
            desenharRedimensionado(pFrame, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
        }
        else if (menuAtual == MENU_EXPLORAR) {
            char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar); desenharTexto(pFrame, bread, 30, listX, 1020, 0xFFFFFFFF);
        }

        // 6. MENU DE OPÇÕES CONTEXTUAL (HUD)
        if (showOpcoes) {
            int ox = (menuAtual == MENU_AUDIO_OPCOES) ? listX + 600 : discoX;
            int oy = (menuAtual == MENU_AUDIO_OPCOES) ? listY : discoY - 100;
            int count = (menuAtual == MENU_AUDIO_OPCOES) ? 11 : 10;

            for (int my = 0; my < count * 50; my++) for (int mx = 0; mx < 350; mx++) {
                int px = ox + mx; int py = oy + my; if (px < 1920 && py < 1080 && py >= 0) pFrame[py * 1920 + px] = 0xEE111111;
            }
            for (int i = 0; i < count; i++) {
                uint32_t corOp = (i == (menuAtual == MENU_AUDIO_OPCOES ? selAudioOpcao : selOpcao)) ? 0xFFFFFF00 : 0xFFFFFFFF;
                const char* txt = (menuAtual == MENU_AUDIO_OPCOES) ? listaOpcoesAudio[i] : listaOpcoes[i];
                desenharTexto(pFrame, txt, 30, ox + 20, oy + 20 + (i * 45), corOp);
            }
        }

        // Exibição de Mensagens de Status (Toasts)
        if (msgTimer > 0) { desenharTexto(pFrame, msgStatus, 40, 100, 950, 0xFFFFFFFF); msgTimer--; }

    SUBMIT_FRAME:
        // Envio do buffer e troca para o próximo
        sceVideoOutSubmitFlip(video, bA, 1, 0); bA = (bA + 1) % 2; sceKernelUsleep(16000);
    }
}