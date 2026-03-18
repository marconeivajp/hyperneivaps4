#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>

#include "menu.h"

#ifdef __INTELLISENSE__
#define __builtin_va_list va_list
#endif

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>

#include "menu.h" // <-- NOSSO NOVO GERENCIADOR DE MENUS!
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"
#include "controle.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int selAudioOpcao = 0;
extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);

// Apenas variáveis exclusivas da main sobraram aqui!
int bA = 0, video = 0;

bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL;
char bufferTecladoC[128] = "";

unsigned char* capasAssets[6], * discosAssets[6], * backImg = NULL;
int wC[6], hC[6], cC[6], wD[6], hD[6], cD[6], wB, hB, cB;

void inicializarPastas() {
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/xml", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
}

int main(void) {
    initNetwork();
    inicializarAudio();

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

    off_t imePh;
    void* imeVm = NULL;
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 2097152, 2097152, 2, &imePh);
    sceKernelMapDirectMemory(&imeVm, 2097152, 0x33, 0, imePh, 2097152);

    OrbisImeDialogSetting* imeSetting = (OrbisImeDialogSetting*)imeVm;
    bufferTecladoW = (uint16_t*)((uint8_t*)imeVm + 1024);
    uint16_t* imeTitle = (uint16_t*)((uint8_t*)bufferTecladoW + 1024);
    uint16_t t[] = { 'E','s','c','r','e','v','a',' ','n','a',' ','F','o','l','h','a','\0' };
    memcpy(imeTitle, t, sizeof(t));

    inicializarPastas(); carregarConfiguracao();

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
        uint32_t* p = (uint32_t*)buffers[bA];
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;
        if (backImg) desenharRedimensionado(p, backImg, wB, hB, backW, backH, backX, backY);

        if (tecladoAtivo) {
            int stat = (int)sceImeDialogGetStatus();
            if (stat == ORBIS_COMMON_DIALOG_STATUS_FINISHED) {
                if (bufferTecladoW[0] != 0) {
                    for (int i = 0; i < 127; i++) {
                        bufferTecladoC[i] = (char)bufferTecladoW[i];
                        if (bufferTecladoW[i] == 0) break;
                    }
                    bufferTecladoC[127] = '\0';
                    sprintf(msgStatus, "TEXTO ATUALIZADO!"); msgTimer = 120;
                }
                else {
                    memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
                    sprintf(msgStatus, "CANCELADO / VAZIO"); msgTimer = 90;
                }
                sceImeDialogTerm();
                tecladoAtivo = false;
            }
            goto FIM_CONTROLES;
        }

        processarControles(pData.buttons, uId, imeSetting, imeTitle);

    FIM_CONTROLES:
        if (menuAtual != MENU_NOTEPAD) {
            for (int i = 0; i < 6; i++) {
                int gIdx = i + off; if (gIdx >= totalItens) break;
                int yP = listY + (i * 120);

                uint32_t corFundo = 0xAA222222;
                uint32_t corTexto = 0xFFFFFFFF;

                if (menuAtual == MENU_EXPLORAR && marcados[gIdx]) corFundo = 0xAAFFFF99;
                if (gIdx == sel) { corFundo = 0xFF00AAFF; corTexto = 0xFF000000; }

                for (int by = 0; by < listH; by++) for (int bx = 0; bx < listW; bx++) {
                    int px = listX + bx; int py = yP + by; if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = corFundo;
                }
                desenharTexto(p, nomes[gIdx], 35, listX + 20, yP + 20, corTexto);
            }
        }

        if (menuAtual == MENU_NOTEPAD) {
            for (int by = 0; by < 700; by++) {
                for (int bx = 0; bx < 1400; bx++) {
                    int px = 260 + bx; int py = 150 + by;
                    if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = 0xFFEEEEEE;
                }
            }
            for (int by = 0; by < 60; by++) {
                for (int bx = 0; bx < 1400; bx++) {
                    int px = 260 + bx; int py = 150 + by;
                    if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = 0xFFD05050;
                }
            }
            desenharTexto(p, "BLOCO DE NOTAS", 40, 280, 160, 0xFFFFFFFF);
            desenharTexto(p, "[X] Escrever   [O] Voltar", 30, 1200, 160, 0xFFFFFFFF);
            desenharTexto(p, bufferTecladoC, 40, 280, 260, 0xFF000000);
        }

        if (menuAtual == JOGAR_XML || editMode) {
            int idx = sel % 6;
            if (capasAssets[idx]) desenharRedimensionado(p, capasAssets[idx], wC[idx], hC[idx], capaW, capaH, capaX, capaY);
            if (discosAssets[idx]) desenharDiscoRedondo(p, discosAssets[idx], wD[idx], hD[idx], discoW, discoH, discoX, discoY);
        }
        else if (menuAtual == SCRAPER_LIST && imgPreview) desenharRedimensionado(p, imgPreview, wP, hP, capaW, capaH, capaX, capaY);
        else if (menuAtual == MENU_EXPLORAR) {
            char bread[300]; sprintf(bread, "Caminho: %s", pathExplorar);
            desenharTexto(p, bread, 30, listX, 1020, 0xFFFFFFFF);
        }

        if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) {
            for (int my = 0; my < 500; my++) for (int mx = 0; mx < 350; mx++) {
                int px = discoX + mx; int py = discoY - 100 + my; if (px < 1920 && py < 1080 && py >= 0) p[py * 1920 + px] = 0xEE111111;
            }
            for (int i = 0; i < 10; i++) {
                uint32_t corOp = (i == selOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
                desenharTexto(p, listaOpcoes[i], 30, discoX + 20, discoY - 80 + (i * 45), corOp);
            }
        }

        if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
            for (int my = 0; my < 550; my++) {
                for (int mx = 0; mx < 350; mx++) {
                    int px = listX + 600 + mx; int py = listY + my;
                    if (px < 1920 && py < 1080 && py >= 0) p[py * 1920 + px] = 0xEE111111;
                }
            }
            for (int i = 0; i < 11; i++) {
                uint32_t corOp = (i == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
                desenharTexto(p, listaOpcoesAudio[i], 30, listX + 620, listY + 50 + (i * 45), corOp);
            }
        }

        if (msgTimer > 0) { desenharTexto(p, msgStatus, 40, 100, 950, 0xFFFFFFFF); msgTimer--; }

        sceVideoOutSubmitFlip(video, bA, 1, 0); bA = (bA + 1) % 2; sceKernelUsleep(16000);
    }
}