#ifndef BAIXAR_H
#define BAIXAR_H

#include <stdbool.h>

// ==========================================================
// ESTRUTURAS E VARIÁVEIS DE SCRAPING (EXISTENTES)
// ==========================================================
struct Console {
    const char* nome;
    const char* pathServidor;
};

// Variáveis Globais de Scraping
extern Console listaConsoles[5];
extern int consoleAtual;

// Variáveis Globais de Preview (Imagem Temporária)
extern unsigned char* imgPreview;
extern int wP, hP, cP;
extern char ultimoJogoCarregado[64];

// Funções de Scraping
void acaoRede(const char* jogo, bool buscarLista, bool salvarNoHD);

// ==========================================================
// SISTEMA DE REPOSITÓRIO DE JOGOS XML (NOVOS)
// ==========================================================
void preencherMenuBaixar();
void preencherMenuRepositorios();
void listarXMLsRepositorio();
void abrirXMLRepositorio(const char* xmlFile);
void mostrarLinksJogo(int gameIndex);

// Variáveis para o controle.cpp saber qual link/XML processar
extern char linksAtuais[10][512];
extern char caminhoXMLAtual[256];

// Funções de Download
void iniciarDownload(const char* url);

#endif