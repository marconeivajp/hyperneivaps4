// --- INÍCIO DO ARQUIVO main.cpp ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int selAudioOpcao = 0;
extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL;
char bufferTecladoC[128] = "";

unsigned char* capasAssets[6], * discosAssets[6], * backImg = NULL;
int wC[6], hC[6], cC[6], wD[6], hD[6], cD[6], wB, hB, cB;

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
    uint16_t t[] = { 'E','s','c','r','e','v','a',' ','a','q','u','i','\0' };
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
    backImg = stbi_load("/app0/assets/images/background.png", &wB, &hB, &cB, 4);
    for (int i = 0; i < 6; i++) {
        char pC[128], pD[128]; sprintf(pC, "/app0/assets/images/capa%d.png", i + 1); sprintf(pD, "/app0/assets/images/disco%d.png", i + 1);
        capasAssets[i] = stbi_load(pC, &wC[i], &hC[i], &cC[i], 4); discosAssets[i] = stbi_load(pD, &wD[i], &hD[i], &cD[i], 4);
    }

    preencherRoot();

    for (;;) {
        OrbisPadData pData; scePadReadState(pad, &pData);
        uint32_t* p = obterBufferVideo();
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;
        if (backImg) desenharRedimensionado(p, backImg, wB, hB, backW, backH, backX, backY);

        // --- LÓGICA DO TECLADO (VERSÃO SEM TRAVAMENTO) ---
        if (tecladoAtivo) {
            int stat = (int)sceImeDialogGetStatus();

            // Se o status mudar de "RODANDO", a gente assume que acabou
            if (stat != 1) { // 1 costuma ser o status de "Aberto/Ativo"
                OrbisDialogResult res;
                memset(&res, 0, sizeof(res));
                sceImeDialogGetResult(&res);

                if (res.endstatus == 0) { // Confirmou no R2
                    memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
                    for (int i = 0; i < 127; i++) {
                        if (bufferTecladoW[i] == 0) break;
                        bufferTecladoC[i] = (char)(bufferTecladoW[i] & 0xFF);
                    }
                    strcpy(msgStatus, "TEXTO SALVO!");
                }
                else {
                    strcpy(msgStatus, "CANCELADO!");
                }

                msgTimer = 120;
                sceImeDialogTerm();
                tecladoAtivo = false; // LIBERA O APP
            }
            // Não damos "goto" aqui! Deixamos o app desenhar a interface por trás
        }
        else {
            // Só processa botões se o teclado não estiver na tela
            processarControles(pData.buttons, uId, imeSetting, imeTitle);
        }

        // --- SEMPRE DESENHA A INTERFACE (Para a notificação aparecer!) ---
        desenharInterface(p);
        submeterTela();
    }
}