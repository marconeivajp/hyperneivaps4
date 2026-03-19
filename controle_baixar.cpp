// --- INÍCIO DO ARQUIVO controle_baixar.cpp ---
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

#include "controle_baixar.h"
#include "menu.h"
#include "baixar.h"
#include "network.h"

extern MenuLevel menuAtual;
extern int sel;
extern int totalItens;
extern int consoleAtual;
extern char nomes[3000][64];
extern char caminhoXMLAtual[256];
extern char linksAtuais[10][512];

extern void preencherMenuRepositorios();
extern void listarXMLsRepositorio();
extern void abrirXMLRepositorio(const char* nomeXML);
extern void mostrarLinksJogo(int idJogo);
extern void iniciarDownload(const char* url);
extern void acaoRede(const char* nome, bool ehConsole, bool ehScraper);
extern void preencherMenuBaixar();
extern void preencherRoot();

void acaoCross_Baixar() {
    if (menuAtual == MENU_BAIXAR) {
        if (sel == 0) preencherMenuRepositorios();
        else if (sel == 1) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
    }
    else if (menuAtual == MENU_BAIXAR_REPOS) { if (sel == 0) listarXMLsRepositorio(); }
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) { if (strstr(nomes[sel], ".xml")) abrirXMLRepositorio(nomes[sel]); }
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) { if (strcmp(nomes[0], "XML Vazio ou Invalido") != 0) mostrarLinksJogo(sel); }
    else if (menuAtual == MENU_BAIXAR_LINKS) { if (strcmp(nomes[0], "Nenhum link disponivel") != 0) iniciarDownload(linksAtuais[sel]); }
    else if (menuAtual == MENU_CAPAS) {
        memset(nomes, 0, sizeof(nomes));
        for (int i = 0; i < 5; i++) strcpy(nomes[i], listaConsoles[i].nome);
        totalItens = 5; menuAtual = MENU_CONSOLES;
    }
    else if (menuAtual == MENU_CONSOLES) { consoleAtual = sel; acaoRede(NULL, true, false); }
    else if (menuAtual == SCRAPER_LIST) { acaoRede(nomes[sel], false, true); }
}

void acaoCircle_Baixar() {
    if (menuAtual == MENU_BAIXAR) preencherRoot();
    else if (menuAtual == MENU_BAIXAR_REPOS) preencherMenuBaixar();
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) preencherMenuRepositorios();
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) listarXMLsRepositorio();
    else if (menuAtual == MENU_BAIXAR_LINKS) {
        char nomeXML[256]; char* ultimaBarra = strrchr(caminhoXMLAtual, '/');
        if (ultimaBarra) strcpy(nomeXML, ultimaBarra + 1);
        abrirXMLRepositorio(nomeXML);
    }
    else if (menuAtual == MENU_CAPAS) preencherMenuBaixar();
    else if (menuAtual == MENU_CONSOLES) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
    else if (menuAtual == SCRAPER_LIST) {
        memset(nomes, 0, sizeof(nomes));
        for (int i = 0; i < 5; i++) strcpy(nomes[i], listaConsoles[i].nome);
        totalItens = 5; menuAtual = MENU_CONSOLES;
    }
}
// --- FIM DO ARQUIVO controle_baixar.cpp ---