#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include "jogar.h"
#include "explorar.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h> 

extern char msgStatus[128];
extern int msgTimer;

// A VARIÁVEL QUE GUARDA O ID DO SEU USUÁRIO LOGADO (Vem do controle!)
extern int globalUserId;

// A ESTRUTURA OCULTA DO ITEMZFLOW PARA INICIAR JOGOS
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

    int resSym = sceKernelDlsym(libSystemService, "sceSystemServiceLaunchApp", (void**)&launchAppReal);
    if (resSym < 0 || launchAppReal == NULL) {
        sprintf(msgStatus, "ERRO 2: FUNCAO NAO ENCONTRADA (0x%08X)", resSym);
        msgTimer = 300;
        return;
    }

    // A MÁGICA: Passando o usuário que tem a permissão para o Kernel!
    struct app_launch_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.structsize = 24; // Tamanho exato da estrutura do PS4
    ctx.user_id = globalUserId;
    ctx.app_opt = 0;
    ctx.crash_report = 0;
    ctx.check_flag = 0;

    int launchRes = 0;
    if (romPath != NULL && strlen(romPath) > 0) {
        char* args[] = { (char*)romPath, NULL };
        launchRes = launchAppReal(titleId, args, &ctx); // Envia o ctx preenchido!
    }
    else {
        // Envia o titleId como arg0 para garantir padrão UNIX
        char* argsNulo[] = { (char*)titleId, NULL };
        launchRes = launchAppReal(titleId, argsNulo, &ctx); // Envia o ctx preenchido!
    }

    if (launchRes < 0) {
        sprintf(msgStatus, "ERRO 3: %s FALHOU (0x%08X)", titleId, launchRes);
        msgTimer = 400;
    }
    else {
        sprintf(msgStatus, "JOGO %s INICIADO COM SUCESSO!", titleId);
        msgTimer = 180;
    }
}