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

enum MenuLevel {
    ROOT,
    MENU_TIPO_JOGO, // <--- NOVO MENU DE ESCOLHA
    MENU_JOGAR_PS4, // <--- NOVO MENU COM A LISTA DE PS4
    MENU_MIDIA,
    MENU_BAIXAR,
    MENU_LOJAS,
    MENU_BAIXAR_REPOS,
    MENU_BAIXAR_GAMES_XMLS,
    MENU_BAIXAR_GAMES_LIST,
    MENU_BAIXAR_LINKS,
    MENU_BAIXAR_LINK_DIRETO,
    MENU_BAIXAR_DROPBOX_LISTA,
    MENU_BAIXAR_DROPBOX_UPLOAD,
    MENU_BAIXAR_DROPBOX_BACKUP,
    MENU_BAIXAR_FTP_SERVIDORES,
    MENU_BAIXAR_FTP_EDITAR_SERVIDOR,
    MENU_BAIXAR_FTP_LISTA,
    MENU_BAIXAR_FTP_UPLOAD_RAIZES,
    MENU_BAIXAR_FTP_UPLOAD,
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
extern int selAudioOpcao;

extern char msgStatus[128];
extern int msgTimer;

extern char caminhoMidiaAtual[512];

void preencherRoot();
void preencherExplorerHome();
void preencherMenuMidia();
void abrirPastaMidia(const char* caminho);

#endif