#include "globals.h"
#include <orbis/libkernel.h>

// --- 1. GERENCIAMENTO DE PASTAS E CONFIGURAÇÕES ---

void inicializarPastas() {
    // Cria a estrutura de pastas no HD do PS4 (/data)
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
}

void salvarConfiguracao() {
    LayoutConfig cfg = {
        listX, listY, listW, listH,
        capaX, capaY, capaW, capaH,
        discoX, discoY, discoW, discoH,
        backX, backY, backW, backH
    };

    FILE* f = fopen("/data/HyperNeiva/configuracao/settings.bin", "wb");
    if (f) {
        fwrite(&cfg, sizeof(LayoutConfig), 1, f);
        fclose(f);
        strcpy(msgStatus, "CONFIGURACOES SALVAS!");
    }
    else {
        strcpy(msgStatus, "ERRO AO SALVAR CONFIG!");
    }
    msgTimer = 90;
}

void carregarConfiguracao() {
    LayoutConfig cfg;
    FILE* f = fopen("/data/HyperNeiva/configuracao/settings.bin", "rb");
    if (f) {
        if (fread(&cfg, sizeof(LayoutConfig), 1, f) == 1) {
            listX = cfg.lX; listY = cfg.lY; listW = cfg.lW; listH = cfg.lH;
            capaX = cfg.cX; capaY = cfg.cY; capaW = cfg.cW; capaH = cfg.cH;
            discoX = cfg.dX; discoY = cfg.dY; discoW = cfg.dW; discoH = cfg.dH;
            backX = cfg.bX; backY = cfg.bY; backW = cfg.bW; backH = cfg.bH;
        }
        fclose(f);
    }
}

// --- 2. PARSER DE XML (LISTA DE JOGOS) ---

void carregarXML(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        sprintf(msgStatus, "ERRO: XML %s NAO ENCONTRADO", path);
        msgTimer = 90;
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc(sz + 1);
    if (!buffer) { fclose(fp); return; }

    fread(buffer, 1, sz, fp);
    buffer[sz] = '\0';
    fclose(fp);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    char* p = buffer;

    while (totalItens < 2000) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        p += 12;
        char* f = strchr(p, '\"');
        if (f) {
            int len = (int)(f - p);
            if (len > 63) len = 63;
            strncpy(nomes[totalItens], p, len);
            nomes[totalItens][len] = '\0';
            totalItens++;
            p = f;
        }
        else break;
    }

    free(buffer);
    menuAtual = JOGAR_XML;
    strcpy(xmlCaminhoAtual, path);
    sel = 0; off = 0;
}

// --- 3. MENUS DE INTERFACE DE SISTEMA ---

void preencherMenuEditar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "POSICAO");
    strcpy(nomes[1], "TAMANHO");
    strcpy(nomes[2], "ESTICAR");
    strcpy(nomes[3], "RESETAR TUDO");
    totalItens = 4;
    menuAtual = MENU_EDITAR;
    sel = 0; off = 0;
}

void preencherMenuEditTarget() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "LISTA");
    strcpy(nomes[1], "CAPA");
    strcpy(nomes[2], "DISCO");
    strcpy(nomes[3], "FUNDO");
    strcpy(nomes[4], "VOLTAR");
    totalItens = 5;
    menuAtual = MENU_EDIT_TARGET;
    sel = 0; off = 0;
}