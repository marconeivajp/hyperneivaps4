#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "controle_explorar.h"
#include "menu.h"
#include "explorar.h"

extern MenuLevel menuAtual;
extern int sel;
extern char nomes[3000][64];
extern char baseRaiz[256];
extern char pathExplorar[256];
extern bool showOpcoes;
extern int selOpcao;
extern int cd;
extern bool marcados[3000];

extern bool esperandoNomePasta; // Variável externa do explorador.cpp
extern bool esperandoRenomear;  // Variável externa da renomeação

extern void listarDiretorio(const char* path);
extern void preencherExplorerHome();
extern void acaoArquivo(int acao);
extern void preencherRoot();

void acaoCross_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return; // Bloqueia se teclado estiver aberto

    if (menuAtual == MENU_EXPLORAR && showOpcoes) {
        acaoArquivo(selOpcao);
    }
    else if (menuAtual == MENU_EXPLORAR_HOME) {
        if (sel == 0) { strcpy(baseRaiz, "/data/HyperNeiva"); listarDiretorio(baseRaiz); }
        else if (sel == 1) { strcpy(baseRaiz, "/"); listarDiretorio(baseRaiz); }
        else if (sel == 2) { strcpy(baseRaiz, "/mnt/usb0"); listarDiretorio(baseRaiz); }
        else if (sel == 3) { strcpy(baseRaiz, "/mnt/usb1"); listarDiretorio(baseRaiz); }
    }
    else if (menuAtual == MENU_EXPLORAR) {
        if (nomes[sel][0] == '[') {
            char pL[128]; strncpy(pL, &nomes[sel][1], strlen(nomes[sel]) - 2); pL[strlen(nomes[sel]) - 2] = '\0';
            char nP[256]; sprintf(nP, "%s/%s", pathExplorar, pL); listarDiretorio(nP);
        }
    }
}

void acaoCircle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;

    if (menuAtual == MENU_EXPLORAR_HOME) {
        preencherRoot();
    }
    else if (menuAtual == MENU_EXPLORAR) {
        if (strcmp(pathExplorar, baseRaiz) == 0) preencherExplorerHome();
        else {
            char* last = strrchr(pathExplorar, '/');
            if (last) {
                if (last == pathExplorar) strcpy(pathExplorar, "/");
                else *last = '\0';
                listarDiretorio(pathExplorar);
            }
        }
    }
}

void acaoTriangle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;

    if (menuAtual == MENU_EXPLORAR) {
        showOpcoes = !showOpcoes;
        selOpcao = 0;
    }
}

void acaoR1_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;

    if (menuAtual == MENU_EXPLORAR) {
        if (cd <= 0) { marcados[sel] = !marcados[sel]; cd = 12; }
    }
}