#pragma once
#ifndef BLOCO_DE_NOTAS_H
#define BLOCO_DE_NOTAS_H

#include <stdint.h>

// Aumentado para suportar leitura de arquivos maiores do Explorer
#define MAX_LINHAS 2000
#define MAX_CHARS_LINHA 256

extern int estadoNotepad;
extern bool notepadSomenteLeitura; // Nova trava de edição

extern char linhasNotepad[MAX_LINHAS][MAX_CHARS_LINHA];
extern int linhaSelecionada;
extern int totalLinhasNotepad;

extern char pastaAtualNotepad[512];
extern char pastasNotepad[100][256];
extern int totalPastasNotepad;
extern int pastaSelecionada;
extern char pastaDestinoFinal[512];

extern char nomeArquivo[256];

void inicializarNotepad();
void abrirTextoNoNotepad(const char* textoCompleto); // Função para o Media Player
void carregarAtalhosNotepad();
void lerDiretorioNotepad(const char* path);
void renderizarNotepad(uint32_t* pixels);

void aplicarTextoNotepad(const char* textoDigitado);
void salvarArquivoNotepad();

#endif