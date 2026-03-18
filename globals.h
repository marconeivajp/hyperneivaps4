#ifndef GLOBALS_H
#define GLOBALS_H

// --- 1. CABEÇALHOS BÁSICOS DE SISTEMA ---
#include <stdarg.h> 
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// FIX: Força a compatibilidade do va_list para o Clang/OpenOrbis.
// Isto resolve o erro de "__builtin_va_list não definido" nas bibliotecas stb.
#if !defined(__builtin_va_list) && !defined(__GNUC__)
#define __builtin_va_list va_list
#endif

// Nota: O Clang normalmente já fornece o __builtin_va_list. 
// Se o erro persistir, a biblioteca stb_truetype.h deve ser incluída após o stdarg.h.
#include "stb_truetype.h"

// --- 2. DEFINIÇÃO DE ESTRUTURAS ---
struct LayoutConfig {
    int lX, lY, lW, lH;
    int cX, cY, cW, cH;
    int dX, dY, dW, dH;
    int bX, bY, bW, bH;
};

struct ItemLista {
    char nome[256];
    bool ehPasta;
};

struct Console {
    const char* nome;
    const char* pathServidor;
};

// --- 3. VARIÁVEIS GLOBAIS (EXTERNAS) ---
enum MenuLevel {
    ROOT,
    MENU_BAIXAR,
    MENU_CAPAS,
    MENU_RETROARCH,
    MENU_CONSOLES,
    MENU_EDITAR,
    MENU_EDIT_TARGET,
    SCRAPER_LIST,
    JOGAR_XML,
    MENU_EXPLORAR_HOME,
    MENU_EXPLORAR
};

extern MenuLevel menuAtual;
extern char nomes[3000][64], msgStatus[128], ultimoJogoCarregado[64], pathExplorar[256], baseRaiz[256], xmlCaminhoAtual[256];
extern bool marcados[3000], editMode, showOpcoes;
extern int totalItens, sel, off, bA, video, msgTimer, consoleAtual, editTarget, editType, selOpcao;
extern void* buffers[2];
extern int cd;
extern bool pCross, pCircle, pTri;
extern char clipboardPaths[100][256];
extern int clipboardCount;
extern bool clipboardIsCut;
extern const char* listaOpcoes[10];

// Declaração da lista de consoles (Definida fisicamente no pastas.cpp)
extern Console listaConsolesLocal[];

// Variáveis de Layout e Assets
extern int listX, listY, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH;
extern stbtt_fontinfo font;
extern int temF;
extern unsigned char* backImg, * imgPreview;
extern int wB, hB, cB, wP, hP, cP;

// Variáveis de Hardware/Rede
extern int netPoolId, httpCtxId, sslCtxId;

// --- 4. PROTÓTIPOS DAS FUNÇÕES ---

// Ações de Input e Ficheiros
void executarAcaoX();
void executarAcaoBolinha();
void acaoArquivo(int op);

// Gestão de Pastas e Configurações
void inicializarPastas();
void carregarConfiguracao();
void salvarConfiguracao();

// Navegação de Menus
void preencherRoot();
void entrarMenuBaixar();
void entrarMenuRetroarch();
void listarConsolesBaixar();
void preencherExplorerHome();
void listarDiretorio(const char* path);

// Lógica de Rede e XML
void acaoRede(const char* jogo, bool buscarLista, bool salvarNoHD);
void carregarXML(const char* path);
void abrirListaPrincipal();
void abrirSubListaSP();
void gerenciarVoltaJogar();

// Interface e Desenho
void preencherMenuEditar();
void preencherMenuEditTarget();
void processarControles(int pad);
void desenharTextoCor(uint32_t* p, const char* t, int tam, int x, int y, uint32_t cor);
void desenharRedimensionado(uint32_t* p, unsigned char* i, int iw, int ih, int dw, int dh, int x, int y);
void desenharDiscoRedondo(uint32_t* p, unsigned char* i, int iw, int ih, int dw, int dh, int x, int y);

#endif