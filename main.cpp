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
#include <sys/types.h>

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>
#include <orbis/AudioOut.h>

// ======================================================================
// FORÇANDO O COMPILADOR A LIGAR O JAILBREAK SEM ERROS DE HEADER
// ======================================================================
extern "C" {
    struct jbc_cred {
        uid_t uid;
        uid_t ruid;
        uid_t svuid;
        gid_t rgid;
        gid_t svgid;
        uintptr_t prison;
        uintptr_t cdir;
        uintptr_t rdir;
        uintptr_t jdir;
        uint64_t sceProcType;
        uint64_t sonyCred;
        uint64_t sceProcCap;
    };

    int jbc_get_cred(struct jbc_cred*);
    int jbc_jailbreak_cred(struct jbc_cred*);
    int jbc_set_cred(const struct jbc_cred*);
}

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
#include "controle_elementos.h" 
#include "elementos_sonoros.h" 

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int selAudioOpcao = 0;
bool tecladoAtivo = false;
uint16_t* bufferTecladoW = NULL;
char bufferTecladoC[128] = "";

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
    // ======================================================================
    // 1º PASSO: LIGAR REDE E ÁUDIO PRIMEIRO
    // ======================================================================
    initNetwork();
    inicializarAudio();

    // ======================================================================
    // 2º PASSO: Módulos de Sistema e Dialogs
    // ======================================================================
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    sceCommonDialogInitialize();

    // ======================================================================
    // 3º PASSO: Ligar o Núcleo do PS4 (Usuário e Controle)
    // ======================================================================
    sceUserServiceInitialize(NULL);
    int32_t uId;
    sceUserServiceGetInitialUser(&uId);

    scePadInit();
    int pad = scePadOpen(uId, 0, 0, NULL);

    // ======================================================================
    // 4º PASSO: Ligar o Vídeo
    // ======================================================================
    inicializarVideo();

    // ======================================================================
    // 5º PASSO: Preparar Memória e Teclado (IME)
    // ======================================================================
    off_t imePh;
    void* imeVm = NULL;
    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 2097152, 2097152, 2, &imePh);
    sceKernelMapDirectMemory(&imeVm, 2097152, 0x33, 0, imePh, 2097152);

    OrbisImeDialogSetting* imeSetting = (OrbisImeDialogSetting*)imeVm;
    bufferTecladoW = (uint16_t*)((uint8_t*)imeVm + 1024);
    uint16_t* imeTitle = (uint16_t*)((uint8_t*)bufferTecladoW + 1024);
    uint16_t t[] = { 'D','i','g','i','t','e',' ','o',' ','L','i','n','k','\0' };
    memcpy(imeTitle, t, sizeof(t));

    // ======================================================================
    // 6º PASSO: Preparar Pastas, Configuração e Elementos Sonoros
    // ======================================================================
    inicializarPastas();
    carregarConfiguracao();
    inicializarElementosSonoros();

    // ======================================================================
    // 7º PASSO: Carregar Fontes e Imagens da pasta /app0/
    // (A Sandbox ainda está ativa aqui, então tudo carrega perfeito!)
    // ======================================================================
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

    backImg = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Background.png", &wB, &hB, &cB, 4);
    if (!backImg) backImg = stbi_load("/app0/assets/images/0_Defalt_Background.png", &wB, &hB, &cB, 4);

    defaultArtwork1 = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Artwork1.png", &wDef1, &hDef1, &cDef1, 4);
    if (!defaultArtwork1) defaultArtwork1 = stbi_load("/app0/assets/images/0_Defalt_Artwork1.png", &wDef1, &hDef1, &cDef1, 4);

    defaultArtwork2 = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Artwork2.png", &wDef2, &hDef2, &cDef2, 4);
    if (!defaultArtwork2) defaultArtwork2 = stbi_load("/app0/assets/images/0_Defalt_Artwork2.png", &wDef2, &hDef2, &cDef2, 4);


    // ======================================================================
    // 8º PASSO: ESCAPAR DA SANDBOX (JAILBREAK)
    // Agora que os gráficos estão na memória, nós quebramos o sistema
    // para podermos ler o pendrive em /mnt/usb0
    // ======================================================================
    struct jbc_cred cred;
    jbc_get_cred(&cred);
    jbc_jailbreak_cred(&cred);
    jbc_set_cred(&cred);

    sceKernelChmod("/mnt/usb0", 0777);
    sceKernelChmod("/mnt/usb1", 0777);

    preencherRoot();

    // ======================================================================
    // LOOP PRINCIPAL
    // ======================================================================
    for (;;) {
        OrbisPadData pData;

        if (scePadReadState(pad, &pData) == 0) {

            if (ctrl1On) {
                int rx = pData.rightStick.x;
                int ry = pData.rightStick.y;
                int lx = pData.leftStick.x;
                int ly = pData.leftStick.y;
                int vel = 15;

                if (rx > 180 || lx > 180) ctrl1X += vel;
                if ((rx < 80 && rx > 0) || (lx < 80 && lx > 0)) ctrl1X -= vel;
                if (ry > 180 || ly > 180) ctrl1Y += vel;
                if ((ry < 80 && ry > 0) || (ly < 80 && ly > 0)) ctrl1Y -= vel;

                if (ctrl1X < -500) ctrl1X = -500;
                if (ctrl1X > 2400) ctrl1X = 2400;
                if (ctrl1Y < -500) ctrl1Y = -500;
                if (ctrl1Y > 1500) ctrl1Y = 1500;
            }
        }

        uint32_t* p = obterBufferVideo();
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;

        if (backImg) desenharRedimensionado(p, backImg, wB, hB, 1920, 1080, 0, 0);

        atualizarImePasta();

        if (tecladoAtivo) {
            int stat = (int)sceImeDialogGetStatus();
            if (stat != 1) {
                OrbisDialogResult res; memset(&res, 0, sizeof(res)); sceImeDialogGetResult(&res);
                if (res.endstatus == 0) {
                    memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
                    for (int i = 0; i < 127; i++) { if (bufferTecladoW[i] == 0) break; bufferTecladoC[i] = (char)(bufferTecladoW[i] & 0xFF); }
                    if (menuAtual == MENU_BAIXAR_LINK_DIRETO) { iniciarDownload(bufferTecladoC); menuAtual = MENU_BAIXAR; }
                    else if (menuAtual == MENU_NOTEPAD) { aplicarTextoNotepad(bufferTecladoC); }
                }
                else { if (menuAtual == MENU_BAIXAR_LINK_DIRETO) menuAtual = MENU_BAIXAR; }
                msgTimer = 120; sceImeDialogTerm(); tecladoAtivo = false;
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