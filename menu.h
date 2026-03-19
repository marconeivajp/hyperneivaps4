#pragma once
#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

// <-- AQUI ESTÃO TODOS OS ESTADOS JUNTOS E CORRIGIDOS -->
enum MenuLevel {
    ROOT,
    MENU_MIDIA,
    MENU_BAIXAR,
    MENU_BAIXAR_REPOS,
    MENU_BAIXAR_GAMES_XMLS,
    MENU_BAIXAR_GAMES_LIST,
    MENU_BAIXAR_LINKS,
    MENU_BAIXAR_LINK_DIRETO,
    MENU_BAIXAR_NAVEGADOR_OPCOES,
    MENU_BAIXAR_NAVEGADOR_FAVORITOS,
    MENU_BAIXAR_NAVEGADOR,
    MENU_BAIXAR_NAVEGADOR_GOOGLE,
    MENU_BAIXAR_NAVEGADOR_URL,
    MENU_BAIXAR_NAVEGADOR_LISTA,
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

extern MenuLevel menuAtual;
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern int off;

extern char msgStatus[128];
extern int msgTimer;

extern char caminhoMidiaAtual[512];

void preencherRoot();
void preencherExplorerHome();
void preencherMenuMidia();
void abrirPastaMidia(const char* caminho);

#endif