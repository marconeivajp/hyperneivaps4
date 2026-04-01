#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#include "jogar.h"
#include "explorar.h" 
#include "audio.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h> 

extern char msgStatus[128];
extern int msgTimer;

extern int globalUserId;

struct app_launch_ctx {
    uint32_t structsize;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
} __attribute__((packed));

char xmlCaminhoAtual[256] = "";

void carregarXML(const char* path) {
    FILE* fp = fopen(path, "rb"); if (!fp) return;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    char* p = b;

    while (totalItens < 2000) {
        p = strstr(p, "<game name=\""); if (!p) break;
        p += 12; char* f = strchr(p, '\"');
        if (f) {
            int l = (int)(f - p);
            strncpy(nomes[totalItens], p, l);
            nomes[totalItens][l] = '\0';
            totalItens++;
            p = f;
        }
        else break;
    }
    free(b);
    menuAtual = JOGAR_XML;
    strcpy(xmlCaminhoAtual, path);
}

void chamarJogo(const char* titleId, const char* romPath) {
    sprintf(msgStatus, "LANCANDO %s...", titleId);
    msgTimer = 120;

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);

    int libSystemService = sceKernelLoadStartModule("libSceSystemService.sprx", 0, NULL, 0, NULL, NULL);
    if (libSystemService < 0) {
        sprintf(msgStatus, "ERRO 1: MODULO RECUSADO (0x%08X)", libSystemService);
        msgTimer = 300;
        return;
    }

    typedef int (*LaunchAppFunc)(const char*, char**, void*);
    LaunchAppFunc launchAppReal = NULL;
    sceKernelDlsym(libSystemService, "sceSystemServiceLaunchApp", (void**)&launchAppReal);

    if (launchAppReal == NULL) return;

    // Desligamos o motor de áudio do Hyper Neiva para liberar a porta do PS4.
    pararAudio();

    struct app_launch_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.structsize = 24;
    ctx.user_id = globalUserId;
    ctx.app_opt = 0;
    ctx.crash_report = 0;
    ctx.check_flag = 0;

    int launchRes = 0;

    if (strcmp(titleId, "SSNE10000") == 0) {
        // TRUQUE DE CONFIGURAÇÃO DO RETROARCH
        FILE* cfg = fopen("/data/retroarch/retroarch.cfg", "a");
        if (cfg) {
            fprintf(cfg, "\nlibretro_path = \"/data/self/retroarch/cores/genesis_plus_gx_libretro_ps4.self\"\n");
            fprintf(cfg, "rgui_browser_directory = \"/data/HyperNeiva/baixado/\"\n");
            fclose(cfg);
        }

        char* argsNulo[] = { (char*)titleId, NULL };
        sprintf(msgStatus, "INICIANDO RETROARCH...");
        launchRes = launchAppReal(titleId, argsNulo, &ctx);
    }
    else if (romPath != NULL && strlen(romPath) > 0) {
        char* args[] = { (char*)titleId, (char*)romPath, NULL };
        launchRes = launchAppReal(titleId, args, &ctx);
    }
    else {
        char* argsNulo[] = { (char*)titleId, NULL };
        launchRes = launchAppReal(titleId, argsNulo, &ctx);
    }

    if (launchRes < 0) {
        sprintf(msgStatus, "ERRO: %s (0x%08X)", titleId, launchRes);
        msgTimer = 400;

        // Se falhar, liga o som do menu de volta
        inicializarAudio();
    }
    else {
        sprintf(msgStatus, "FECHANDO FRONTEND...");

        // Dá meio segundo para o PS4 assumir o controle da tela
        sceKernelUsleep(500000);

        // =====================================================================
        // BUSCA DINÂMICA DA FUNÇÃO DE SAÍDA (Resolve o erro do ld.lld)
        // =====================================================================
        typedef void (*ExitProcessFunc)(int);
        ExitProcessFunc exitProcessReal = NULL;

        int libKernel = sceKernelLoadStartModule("libkernel.sprx", 0, NULL, 0, NULL, NULL);
        if (libKernel >= 0) {
            sceKernelDlsym(libKernel, "sceKernelExitProcess", (void**)&exitProcessReal);
        }

        if (exitProcessReal != NULL) {
            exitProcessReal(0); // Morte Limpa pelo Kernel
        }
        else {
            exit(0); // Plano B (Padrão C++)
        }
    }
}