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
extern char linksAtuais[3000][1024]; // <-- IMPORTANTE: Sincronizado para 1024
extern char msgStatus[128];
extern int msgTimer;

extern void preencherMenuRepositorios();
extern void listarXMLsRepositorio();
extern void abrirXMLRepositorio(const char* nomeXML);
extern void mostrarLinksJogo(int idJogo);
extern void iniciarDownload(const char* url);
extern void acaoRede(const char* nome, bool ehConsole, bool ehScraper);
extern void preencherMenuBaixar();
extern void preencherRoot();
extern void acessarSiteNavegador(const char* url);

extern void preencherMenuNavegadorOpcoes();
extern void preencherMenuNavegadorFavoritos();
extern char linksFavoritos[10][512];

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);

void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (menuAtual == MENU_BAIXAR) {
        if (sel == 0) preencherMenuRepositorios();
        else if (sel == 1) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
        else if (sel == 2) {
            menuAtual = MENU_BAIXAR_LINK_DIRETO;
            acaoCross_Notepad(uId, imeSetting, imeTitle);
        }
        else if (sel == 3) {
            preencherMenuNavegadorOpcoes();
        }
    }
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_OPCOES) {
        if (sel == 0) {
            menuAtual = MENU_BAIXAR_NAVEGADOR_GOOGLE;
            acaoCross_Notepad(uId, imeSetting, imeTitle);
        }
        else if (sel == 1) {
            menuAtual = MENU_BAIXAR_NAVEGADOR_URL;
            acaoCross_Notepad(uId, imeSetting, imeTitle);
        }
        else if (sel == 2) {
            preencherMenuNavegadorFavoritos();
        }
    }
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_FAVORITOS) {
        acessarSiteNavegador(linksFavoritos[sel]);
    }
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_LISTA) {
        if (strcmp(nomes[0], "Nenhum arquivo encontrado") != 0 && strcmp(nomes[0], "Erro de Conexao") != 0) {

            char* urlSelecionada = linksAtuais[sel];
            int tamanho = strlen(urlSelecionada);

            if (tamanho > 0 && urlSelecionada[tamanho - 1] == '/') {
                acessarSiteNavegador(urlSelecionada);
            }
            else {
                strcpy(msgStatus, "APERTE [TRIANGULO] PARA BAIXAR");
                msgTimer = 120;
            }
        }
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
    else if (menuAtual == MENU_BAIXAR_LINK_DIRETO) preencherMenuBaixar();
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_OPCOES) preencherMenuBaixar();
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_FAVORITOS) preencherMenuNavegadorOpcoes();
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_URL || menuAtual == MENU_BAIXAR_NAVEGADOR_GOOGLE) preencherMenuNavegadorOpcoes();
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_LISTA) preencherMenuNavegadorOpcoes();
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

void acaoTriangle_Baixar() {
    if (menuAtual == MENU_BAIXAR_NAVEGADOR_LISTA) {
        if (strcmp(nomes[0], "Nenhum arquivo encontrado") != 0 && strcmp(nomes[0], "Erro de Conexao") != 0) {

            char* urlSelecionada = linksAtuais[sel];
            int tamanho = strlen(urlSelecionada);

            if (tamanho > 0 && urlSelecionada[tamanho - 1] != '/') {
                iniciarDownload(urlSelecionada);
            }
            else {
                strcpy(msgStatus, "ISSO E UMA PASTA! APERTE [X] PARA ENTRAR.");
                msgTimer = 120;
            }
        }
    }
}
// --- FIM DO ARQUIVO controle_baixar.cpp ---