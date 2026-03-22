#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>

#include "menu.h" 
#include "menu_grafico.h" 
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"
#include "controle.h"
#include "criar_pastas.h"
#include "bloco_de_notas.h" 

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int selAudioOpcao = 0;
bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL;
char bufferTecladoC[128] = "";

// VARIÁVEIS DE MÍDIA PADRÃO
unsigned char* backImg = NULL;
unsigned char* defaultArtwork1 = NULL;
unsigned char* defaultArtwork2 = NULL;

int wB, hB, cB;
int wDef1, hDef1, cDef1;
int wDef2, hDef2, cDef2;

extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

int main(void) {
    initNetwork();
    inicializarAudio();

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    sceCommonDialogInitialize();

    sceUserServiceInitialize(NULL); int32_t uId; sceUserServiceGetInitialUser(&uId);
    scePadInit(); int pad = scePadOpen(uId, 0, 0, NULL);

    inicializarVideo();

    off_t imePh;
    void* imeVm = NULL;
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 2097152, 2097152, 2, &imePh);
    sceKernelMapDirectMemory(&imeVm, 2097152, 0x33, 0, imePh, 2097152);

    OrbisImeDialogSetting* imeSetting = (OrbisImeDialogSetting*)imeVm;
    bufferTecladoW = (uint16_t*)((uint8_t*)imeVm + 1024);
    uint16_t* imeTitle = (uint16_t*)((uint8_t*)bufferTecladoW + 1024);
    uint16_t t[] = { 'D','i','g','i','t','e',' ','o',' ','L','i','n','k','\0' };
    memcpy(imeTitle, t, sizeof(t));

    inicializarPastas();
    carregarConfiguracao();

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

    // CARREGANDO O BACKGROUND E ARTWORKS FIXOS TUDO EM PNG
    backImg = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Background.png", &wB, &hB, &cB, 4);
    if (!backImg) backImg = stbi_load("/app0/assets/images/0_Defalt_Background.png", &wB, &hB, &cB, 4);

    defaultArtwork1 = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Artwork1.png", &wDef1, &hDef1, &cDef1, 4);
    if (!defaultArtwork1) defaultArtwork1 = stbi_load("/app0/assets/images/0_Defalt_Artwork1.png", &wDef1, &hDef1, &cDef1, 4);

    defaultArtwork2 = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Artwork2.png", &wDef2, &hDef2, &cDef2, 4);
    if (!defaultArtwork2) defaultArtwork2 = stbi_load("/app0/assets/images/0_Defalt_Artwork2.png", &wDef2, &hDef2, &cDef2, 4);

    preencherRoot();

    // LOOP PRINCIPAL
    for (;;) {
        OrbisPadData pData; scePadReadState(pad, &pData);
        uint32_t* p = obterBufferVideo();
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;

        // Desenha Background sempre 1920x1080
        if (backImg) desenharRedimensionado(p, backImg, wB, hB, 1920, 1080, 0, 0);

        atualizarImePasta();

        if (tecladoAtivo) {
            int stat = (int)sceImeDialogGetStatus();
            if (stat != 1) {
                OrbisDialogResult res;
                memset(&res, 0, sizeof(res));
                sceImeDialogGetResult(&res);

                if (res.endstatus == 0) {
                    memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
                    for (int i = 0; i < 127; i++) {
                        if (bufferTecladoW[i] == 0) break;
                        bufferTecladoC[i] = (char)(bufferTecladoW[i] & 0xFF);
                    }

                    if (menuAtual == MENU_BAIXAR_LINK_DIRETO) {
                        iniciarDownload(bufferTecladoC);
                        menuAtual = MENU_BAIXAR;
                    }
                    else if (menuAtual == MENU_NOTEPAD) {
                        aplicarTextoNotepad(bufferTecladoC);
                    }
                }
                else {
                    if (menuAtual == MENU_BAIXAR_LINK_DIRETO) {
                        menuAtual = MENU_BAIXAR;
                    }
                }

                msgTimer = 120;
                sceImeDialogTerm();
                tecladoAtivo = false;
            }
        }
        else {
            if (!esperandoNomePasta && !esperandoRenomear) {
                processarControles(pData.buttons, uId, imeSetting, imeTitle);
            }
        }

        desenharInterface(p);
        submeterTela();
    }
}