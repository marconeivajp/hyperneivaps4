#ifndef BAIXAR_H
#define BAIXAR_H

#include <stdbool.h>

// Estrutura de Consoles suportados
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

#endif