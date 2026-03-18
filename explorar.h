#ifndef EXPLORAR_H
#define EXPLORAR_H

#include <stdbool.h>

// Enumeração compartilhada do Nível do Menu
enum MenuLevel {
    ROOT, MENU_BAIXAR, MENU_CAPAS, MENU_RETROARCH, MENU_CONSOLES,
    MENU_EDITAR, MENU_EDIT_TARGET, SCRAPER_LIST, JOGAR_XML,
    MENU_EXPLORAR_HOME, MENU_EXPLORAR
};

// Estruturas
struct ItemLista { char nome[64]; bool ehPasta; };

// Variáveis Globais do Explorador 
extern char pathExplorar[256];
extern char baseRaiz[256];
extern bool marcados[3000];
extern char clipboardPaths[100][256];
extern int clipboardCount;
extern bool clipboardIsCut;
extern bool showOpcoes;
extern int selOpcao;
extern const char* listaOpcoes[10];

// Variáveis Globais Compartilhadas do Sistema (Definidas no main.cpp)
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern char msgStatus[128];
extern int msgTimer;
extern MenuLevel menuAtual;

// Funções Públicas do Explorador
void copiarArquivoReal(const char* origem, const char* destino);
void listarDiretorio(const char* path);
void acaoArquivo(int op);

#endif