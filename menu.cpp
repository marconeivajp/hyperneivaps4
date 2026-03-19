#include "menu.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>

// --- DEFINIÇÃO FÍSICA DAS VARIÁVEIS ---
MenuLevel menuAtual = ROOT;
char nomes[3000][64];
int totalItens = 0;
int sel = 0;
int off = 0;

// Variáveis de feedback visual para o utilizador
char msgStatus[128] = "SISTEMA PRONTO";
int msgTimer = 0;

// NOVA VARIÁVEL GLOBAL (Guarda onde estamos na navegação)
char caminhoMidiaAtual[512] = "/data/HyperNeiva/midia";

void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "JOGAR");
    strcpy(nomes[1], "MIDIA");
    strcpy(nomes[2], "BAIXAR");
    strcpy(nomes[3], "EDITAR");
    strcpy(nomes[4], "EXPLORAR");
    strcpy(nomes[5], "MUSICAS");
    strcpy(nomes[6], "CRIAR");
    totalItens = 7;
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

// NOVA FUNÇÃO: ABRE UMA PASTA ESPECÍFICA E LÊ O QUE TEM DENTRO
void abrirPastaMidia(const char* caminho) {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    strcpy(caminhoMidiaAtual, caminho); // Grava a pasta atual

    DIR* d = opendir(caminhoMidiaAtual);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            // Ignora atalhos de sistema "." e ".."
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                strcpy(nomes[totalItens], dir->d_name);
                totalItens++;
            }
        }
        closedir(d);
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Pasta vazia");
        totalItens = 1;
    }

    menuAtual = MENU_MIDIA;
    sel = 0; // Joga o cursor para o topo ao entrar na pasta
    off = 0; // Reseta a rolagem
}

void preencherMenuMidia() {
    abrirPastaMidia("/data/HyperNeiva/midia");
}