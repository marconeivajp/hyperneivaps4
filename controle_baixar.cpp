#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include "controle_baixar.h"
#include "menu.h"
#include "baixar.h"
#include "network.h"

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);

void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (menuAtual == MENU_BAIXAR) {
        if (sel == 0) preencherMenuRepositorios();
        else if (sel == 1) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
        else if (sel == 2) { menuAtual = MENU_BAIXAR_LINK_DIRETO; acaoCross_Notepad(uId, imeSetting, imeTitle); }
        else if (sel == 3) {
            acessarDropbox(""); // Acessa a raiz via API
        }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        char urlSel[1024];
        strcpy(urlSel, linksAtuais[sel]);
        int tam = strlen(urlSel);

        if (tam > 0 && (urlSel[tam - 1] == '/' || strchr(nomes[sel], '.') == NULL)) {
            if (urlSel[tam - 1] == '/') urlSel[tam - 1] = '\0';
            acessarDropbox(urlSel);
        }
        else {
            sprintf(msgStatus, "USE [TRIANGULO] PARA BAIXAR %s", nomes[sel]);
            msgTimer = 120;
        }
    }
    else if (menuAtual == MENU_BAIXAR_REPOS) { if (sel == 0) listarXMLsRepositorio(); }
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) { if (strstr(nomes[sel], ".xml")) abrirXMLRepositorio(nomes[sel]); }
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) mostrarLinksJogo(sel);
    else if (menuAtual == MENU_BAIXAR_LINKS) iniciarDownload(linksAtuais[sel]);
    else if (menuAtual == MENU_CONSOLES) { consoleAtual = sel; acaoRede(NULL, true, false); }
}

void acaoCircle_Baixar() {
    if (menuAtual == MENU_BAIXAR) {
        preencherRoot();
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        // Lógica inteligente de Voltar do Dropbox
        if (strlen(currentDropboxPath) == 0 || strcmp(currentDropboxPath, "/") == 0) {
            // Se já estiver na raiz, sai do Dropbox
            preencherMenuBaixar();
        }
        else {
            // Procura a última barra na string (ex: "/Roms/PS1" -> encontra a barra antes de PS1)
            char* ultimaBarra = strrchr(currentDropboxPath, '/');
            if (ultimaBarra != NULL) {
                if (ultimaBarra == currentDropboxPath) {
                    // Se a única barra é no começo (ex: "/Roms"), volta para a raiz ""
                    strcpy(currentDropboxPath, "");
                }
                else {
                    // Corta a string apagando a última pasta (ex: vira apenas "/Roms")
                    *ultimaBarra = '\0';
                }
                // Carrega a pasta anterior!
                acessarDropbox(currentDropboxPath);
            }
            else {
                // Segurança
                strcpy(currentDropboxPath, "");
                acessarDropbox(currentDropboxPath);
            }
        }
    }
    else if (menuAtual == MENU_BAIXAR_REPOS) preencherMenuBaixar();
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) preencherMenuRepositorios();
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) listarXMLsRepositorio();
    else if (menuAtual == MENU_BAIXAR_LINKS) {
        char nomeXML[256]; char* ultimaBarra = strrchr(caminhoXMLAtual, '/');
        if (ultimaBarra) strcpy(nomeXML, ultimaBarra + 1);
        abrirXMLRepositorio(nomeXML);
    }
}

void acaoTriangle_Baixar() {
    if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        iniciarDownload(linksAtuais[sel]);
    }
}