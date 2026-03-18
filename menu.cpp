#include "menu.h"
#include <string.h>
#include <stdio.h>

// --- DEFINIÇÃO FÍSICA DAS VARIÁVEIS ---
// É aqui que a memória é efetivamente alocada
MenuLevel menuAtual = ROOT;
char nomes[3000][64];
int totalItens = 0;
int sel = 0;
int off = 0;

char msgStatus[128] = "SISTEMA PRONTO";
int msgTimer = 0;

// Preenche a lista principal do sistema (Menu Root)
void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));

    strcpy(nomes[0], "JOGAR");
    strcpy(nomes[1], "BAIXAR");
    strcpy(nomes[2], "EDITAR");
    strcpy(nomes[3], "EXPLORAR");
    strcpy(nomes[4], "MUSICAS");
    strcpy(nomes[5], "CRIAR"); // Opção para o Bloco de Notas (Notepad)

    totalItens = 6;
    menuAtual = ROOT;
}

// Preenche a lista inicial do Explorador de Ficheiros
void preencherExplorerHome() {
    memset(nomes, 0, sizeof(nomes));

    strcpy(nomes[0], "Hyper Neiva"); // Pasta de dados interna
    strcpy(nomes[1], "Raiz");        // Raiz do sistema PS4 (/)
    strcpy(nomes[2], "USB0");        // Dispositivo USB externo 0
    strcpy(nomes[3], "USB1");        // Dispositivo USB externo 1

    totalItens = 4;
    menuAtual = MENU_EXPLORAR_HOME;
}