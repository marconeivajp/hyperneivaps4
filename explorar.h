#ifndef EXPLORAR_H
#define EXPLORAR_H

#include <stdbool.h>
#include <stdint.h>
#include "menu.h"

// Estruturas
struct ItemLista { char nome[64]; bool ehPasta; };

// Variáveis Globais do Explorador (Painel Direito/Padrão)
extern char pathExplorar[256];
extern char baseRaiz[256];
extern bool marcados[3000];
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern MenuLevel menuAtual;

// --- VARIÁVEIS DO PAINEL DUPLO (ESQUERDO) ---
extern bool painelDuplo; // Define se a tela está dividida
extern int painelAtivo;  // 0 = Esquerdo, 1 = Direito
extern char pathExplorarEsq[256];
extern char nomesEsq[3000][64];
extern bool marcadosEsq[3000];
extern int totalItensEsq;
extern int selEsq;
extern MenuLevel menuAtualEsq;

// Variáveis da Área de Transferência (Clipboard)
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
extern char msgStatus[128];
extern int msgTimer;

// Funções Públicas
void copiarArquivoReal(const char* origem, const char* destino);
void listarDiretorio(const char* path);
void listarDiretorioEsq(const char* path); // Nova função pro painel esquerdo
void acaoArquivo(int op);
void atualizarImePasta();

#endif