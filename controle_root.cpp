// --- INÍCIO DO ARQUIVO controle_root.cpp ---
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

#include "controle_root.h"
#include "menu.h"
#include "stb_image.h"

extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern char nomes[3000][64];
extern char ultimoJogoCarregado[64];
extern unsigned char* imgPreview;
extern char bufferTecladoC[128];

// CORRIGIDO AQUI: A variável correta é caminhoXMLAtual
extern char caminhoXMLAtual[256];

extern void carregarXML(const char* path);
extern void preencherMenuBaixar();
extern void preencherMenuEditar();
extern void preencherExplorerHome();
extern void preencherMenuMusicas();
extern void preencherRoot();

void acaoCross_Root() {
    if (menuAtual == ROOT) {
        if (sel == 0) carregarXML("/app0/assets/lista.xml");
        else if (sel == 1) { preencherMenuBaixar(); sel = 0; off = 0; }
        else if (sel == 2) { preencherMenuEditar(); sel = 0; off = 0; }
        else if (sel == 3) { preencherExplorerHome(); sel = 0; off = 0; }
        else if (sel == 4) {
            if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
            strcpy(ultimoJogoCarregado, ""); preencherMenuMusicas(); sel = 0; off = 0;
        }
        else if (sel == 5) {
            menuAtual = MENU_NOTEPAD;
            memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
        }
    }
    else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) {
        carregarXML("/app0/assets/sp.xml");
    }
}

void acaoCircle_Root() {
    if (menuAtual == JOGAR_XML) {
        if (strstr(caminhoXMLAtual, "sp.xml")) carregarXML("/app0/assets/lista.xml");
        else preencherRoot();
    }
}
// --- FIM DO ARQUIVO controle_root.cpp ---