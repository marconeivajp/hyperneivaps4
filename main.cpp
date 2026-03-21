#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
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
#include "bloco_de_notas.h" 

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int selAudioOpcao = 0;
bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL;
char bufferTecladoC[128] = "";

unsigned char* capasAssets[6], * discosAssets[6], * backImg = NULL;
int wC[6], hC[6], cC[6], wD[6], hD[6], cD[6], wB, hB, cB;

extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

int main(void) {
    initNetwork();
    inicializarAudio();

    // Inicialização dos Diálogos de Sistema
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    sceCommonDialogInitialize();

    sceUserServiceInitialize(NULL); int32_t uId; sceUserServiceGetInitialUser(&uId);
    scePadInit(); int pad = scePadOpen(uId, 0, 0, NULL);

    inicializarVideo();

    // Memória para o ImeDialog padrão
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

    // Carregamento de fontes e imagens
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

    // LOOP PRINCIPAL
    for (;;) {
        OrbisPadData pData; scePadReadState(pad, &pData);
        uint32_t* p = obterBufferVideo();
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;
        if (backImg) desenharRedimensionado(p, backImg, wB, hB, backW, backH, backX, backY);

        // --- ADICIONADO: VERIFICAÇÃO DO TECLADO DO EXPLORADOR ---
        // Esta função processa a criação da pasta e desbloqueia os controlos
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
            // Apenas processa controlos se NÃO estiver a aguardar o nome da pasta
            if (!esperandoNomePasta) {
                processarControles(pData.buttons, uId, imeSetting, imeTitle);
            }
        }

        desenharInterface(p);
        submeterTela();
    }
}