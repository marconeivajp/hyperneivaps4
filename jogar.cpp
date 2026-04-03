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

extern char caminhoXMLAtual[256];

// =========================================================================
// O NOVO MOTOR AGORA SALVA TUDO DENTRO DA PASTA /jogar/
// =========================================================================
void carregarXML(const char* nomeArquivoXML) {
    char pathConfig[512];

    // Aponta para a nova pasta 'jogar' no HDD
    sprintf(pathConfig, "/data/HyperNeiva/configuracao/jogar/%s", nomeArquivoXML);

    FILE* fp = fopen(pathConfig, "rb");

    // Se o ficheiro não estiver salvo no PS4, ele extrai do PKG original!
    if (!fp) {
        char pathApp0[512];
        sprintf(pathApp0, "/app0/assets/%s", nomeArquivoXML);
        FILE* fApp0 = fopen(pathApp0, "rb");
        if (fApp0) {
            fseek(fApp0, 0, SEEK_END); long sz = ftell(fApp0); fseek(fApp0, 0, SEEK_SET);
            char* bTemp = (char*)malloc(sz);
            fread(bTemp, 1, sz, fApp0);
            fclose(fApp0);

            // Cria as pastas em cascata por segurança
            sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
            sceKernelMkdir("/data/HyperNeiva/configuracao/jogar", 0777);

            // Salva na pasta jogar!
            FILE* fSave = fopen(pathConfig, "wb");
            if (fSave) {
                fwrite(bTemp, 1, sz, fSave);
                fclose(fSave);
            }
            free(bTemp);

            // Agora abre o ficheiro definitivo no HDD
            fp = fopen(pathConfig, "rb");
        }
    }

    if (!fp) {
        sprintf(msgStatus, "ERRO: %s NAO ENCONTRADO!", nomeArquivoXML);
        msgTimer = 180;
        return;
    }

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
    strcpy(caminhoXMLAtual, pathConfig);
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
        inicializarAudio();
    }
    else {
        sprintf(msgStatus, "FECHANDO FRONTEND...");
        sceKernelUsleep(500000);

        typedef void (*ExitProcessFunc)(int);
        ExitProcessFunc exitProcessReal = NULL;

        int libKernel = sceKernelLoadStartModule("libkernel.sprx", 0, NULL, 0, NULL, NULL);
        if (libKernel >= 0) {
            sceKernelDlsym(libKernel, "sceKernelExitProcess", (void**)&exitProcessReal);
        }

        if (exitProcessReal != NULL) {
            exitProcessReal(0);
        }
        else {
            exit(0);
        }
    }
}