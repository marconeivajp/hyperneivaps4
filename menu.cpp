#include "menu.h"
#include <string.h>
#include <stdio.h>

// --- DEFINIÇÃO FÍSICA DAS VARIÁVEIS ---
// Estas variáveis são as mesmas declaradas como 'extern' no menu.h
// Aqui é onde o compilador reserva o espaço real na memória.
MenuLevel menuAtual = ROOT;
char nomes[3000][64];
int totalItens = 0;
int sel = 0;
int off = 0;

// Variáveis de feedback visual para o utilizador
char msgStatus[128] = "SISTEMA PRONTO";
int msgTimer = 0;

/**
 * Preenche a lista principal do sistema (Menu Root).
 * Limpa o array de nomes e define as opções iniciais do Hyper Neiva.
 */
void preencherRoot() {
    // Limpa a memória do array de nomes para evitar lixo visual
    memset(nomes, 0, sizeof(nomes));

    // Define as opções do menu principal
    strcpy(nomes[0], "JOGAR");
    strcpy(nomes[1], "BAIXAR");
    strcpy(nomes[2], "EDITAR");
    strcpy(nomes[3], "EXPLORAR");
    strcpy(nomes[4], "MUSICAS");
    strcpy(nomes[5], "CRIAR");

    totalItens = 6;
    menuAtual = ROOT;
}

/**
 * Preenche a lista inicial do Explorador de Ficheiros.
 * Mostra os pontos de montagem disponíveis no sistema.
 */
void preencherExplorerHome() {
    // Limpa a memória do array de nomes
    memset(nomes, 0, sizeof(nomes));

    // Define os destinos do explorador
    strcpy(nomes[0], "Hyper Neiva"); // Pasta interna /data
    strcpy(nomes[1], "Raiz");        // Raiz do sistema /
    strcpy(nomes[2], "USB0");        // Dispositivo USB na porta 0
    strcpy(nomes[3], "USB1");        // Dispositivo USB na porta 1

    totalItens = 4;
    menuAtual = MENU_EXPLORAR_HOME;
}