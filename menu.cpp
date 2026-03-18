#include "menu.h"
#include <string.h>
#include <stdio.h>

// --- DEFINIÇÃO FÍSICA DAS VARIÁVEIS ---
MenuLevel menuAtual = ROOT;
char nomes[3000][64];
int totalItens = 0;
int sel = 0;
int off = 0;

// Variáveis de feedback visual para o utilizador
char msgStatus[128] = "SISTEMA PRONTO";
int msgTimer = 0;

void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "JOGAR");
    strcpy(nomes[1], "BAIXAR");
    strcpy(nomes[2], "EDITAR");
    strcpy(nomes[3], "EXPLORAR");
    strcpy(nomes[4], "MUSICAS");
    strcpy(nomes[5], "CRIAR");
    totalItens = 6;
    menuAtual = ROOT;
}

void preencherExplorerHome() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva");
    strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB0");
    strcpy(nomes[3], "USB1");
    totalItens = 4;
    menuAtual = MENU_EXPLORAR_HOME;
}