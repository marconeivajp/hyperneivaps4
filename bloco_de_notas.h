#pragma once
#ifndef BLOCO_DE_NOTAS_H
#define BLOCO_DE_NOTAS_H

#include <stdint.h>

#define MAX_LINHAS 2000
#define MAX_CHARS_LINHA 256

extern int estadoNotepad;
extern bool notepadSomenteLeitura;

extern char linhasNotepad[MAX_LINHAS][MAX_CHARS_LINHA];
extern int linhaSelecionada;
extern int totalLinhasNotepad;

extern char pastaDestinoFinal[512];
extern char nomeArquivo[256];

void inicializarNotepad();
void abrirTextoNoNotepad(const char* textoCompleto);
void renderizarNotepad(uint32_t* pixels);

void aplicarTextoNotepad(const char* textoDigitado);
void salvarArquivoNotepad();

// Nova função para abrir arquivos direto do HD no modo edição
void editarArquivoExistente(const char* pasta, const char* arquivo);

#endif