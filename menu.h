#pragma once
#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

// 1. Correção para o IntelliSense do Visual Studio
#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

// 2. Definição ÚNICA do Tipo de Menu
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
    MENU_EXPLORAR,
    MENU_MUSICAS,
    MENU_AUDIO_OPCOES,
    MENU_NOTEPAD
};

// 3. Declaração das Variáveis Globais (EXTERN)
extern MenuLevel menuAtual;
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern int off;

extern char msgStatus[128];
extern int msgTimer;

// 4. Protótipos das Funções do Menu
void preencherRoot();
void preencherExplorerHome();

#endif