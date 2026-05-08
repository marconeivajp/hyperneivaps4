#pragma once
#ifndef BLOCO_DE_NOTAS_H
#define BLOCO_DE_NOTAS_H

#include <stdint.h>

#define MAX_LINHAS 100
#define MAX_CHARS_LINHA 256

extern int estadoNotepad;

extern char linhasNotepad[MAX_LINHAS][MAX_CHARS_LINHA];
extern int linhaSelecionada;
extern int totalLinhasNotepad;

// Variáveis para o menu de pastas
extern const char* pastasDisponiveis[4];
extern int totalPastas;
extern int pastaSelecionada;

extern char nomeArquivo[256];

void inicializarNotepad();
void renderizarNotepad(uint32_t* pixels);

void aplicarTextoNotepad(const char* textoDigitado);
void salvarArquivoNotepad();

#endif