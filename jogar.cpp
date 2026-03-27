#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include "jogar.h"
#include "explorar.h" // Acesso às variáveis globais da interface
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// INCLUDES OBRIGATÓRIOS PARA ACESSAR A MEMÓRIA DO PS4
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h> 

// Puxando as variáveis de notificação lá da sua interface!
extern char msgStatus[128];
extern int msgTimer;

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

// =========================================================================
// O MOTOR DE LANÇAMENTO (COM SISTEMA DE DEBUG POR NOTIFICAÇÕES)
// =========================================================================
void chamarJogo(const char* titleId, const char* romPath) {
    sprintf(msgStatus, "CARREGANDO MODULO...");
    msgTimer = 120;

    // 1. Pede para o PS4 carregar o módulo do sistema
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);

    // 2. Obtém o "Handle" da biblioteca na memória do PS4
    int libSystemService = sceKernelLoadStartModule("libSceSystemService.sprx", 0, NULL, 0, NULL, NULL);

    if (libSystemService < 0) {
        // Se bater aqui, o PS4 recusou o carregamento da biblioteca!
        sprintf(msgStatus, "ERRO 1: MODULO RECUSADO (0x%08X)", libSystemService);
        msgTimer = 300;
        return;
    }

    // 3. Define a assinatura exata da função escondida
    typedef int (*LaunchAppFunc)(const char*, char**, void*);
    LaunchAppFunc launchAppReal = NULL;

    // 4. MÁGICA: Extrai a função direto da RAM do PS4
    int resSym = sceKernelDlsym(libSystemService, "sceSystemServiceLaunchApp", (void**)&launchAppReal);

    if (resSym < 0 || launchAppReal == NULL) {
        // Se bater aqui, a biblioteca carregou, mas a função tem outro nome ou tá bloqueada
        sprintf(msgStatus, "ERRO 2: FUNCAO NAO ENCONTRADA (0x%08X)", resSym);
        msgTimer = 300;
        return;
    }

    // 5. Executa a função e pega o resultado final do Kernel
    int launchRes = 0;
    if (romPath != NULL && strlen(romPath) > 0) {
        char* args[2];
        args[0] = (char*)romPath; // Caminho da ROM
        args[1] = NULL;
        launchRes = launchAppReal(titleId, args, NULL);
    }
    else {
        launchRes = launchAppReal(titleId, NULL, NULL); // Jogo de PS4 Puro
    }

    // 6. Analisa se o PS4 aceitou iniciar o jogo ou deu Crash de Permissão
    if (launchRes < 0) {
        sprintf(msgStatus, "ERRO 3: FALHA AO ABRIR JOGO (0x%08X)", launchRes);
        msgTimer = 400;
    }
    else {
        sprintf(msgStatus, "JOGO %s INICIADO!", titleId);
        msgTimer = 180;
    }
}