#ifndef EXPLORAR_H
#define EXPLORAR_H

#include <stdbool.h>
#include <stdint.h>
#include "menu.h"

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

// --- VARIÁVEIS DO TECLADO E RENOMEAÇÃO ---
extern bool esperandoNomePasta;
extern bool esperandoRenomear;
extern wchar_t textoTeclado[64];
extern char oldPathParaRenomear[512];
extern char oldExtParaRenomear[64];
extern bool ehPastaParaRenomear;

// Variáveis Globais Compartilhadas do Sistema
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern char msgStatus[128];
extern int msgTimer;
extern MenuLevel menuAtual;

// Funções Públicas
void copiarArquivoReal(const char* origem, const char* destino);
void listarDiretorio(const char* path);
void acaoArquivo(int op);
void atualizarImePasta();

#endif