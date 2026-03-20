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

// ASSINATURA ATUALIZADA (4 Parâmetros)
extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);

void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (menuAtual == MENU_BAIXAR) {
        if (sel == 0) preencherMenuRepositorios();
        else if (sel == 1) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
        // ATUALIZADO: Passando string vazia "" no quarto parâmetro
        else if (sel == 2) { menuAtual = MENU_BAIXAR_LINK_DIRETO; acaoCross_Notepad(uId, imeSetting, imeTitle, ""); }
        else if (sel == 3) { acessarDropbox(""); }
        else if (sel == 4) { preencherMenuBackup(); }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_BACKUP) {
        if (sel == 0) listarArquivosUpload("/");
        else if (sel == 1) executarBackupTodos();
        else if (sel == 2) listarArquivosUpload("/user/home/10000000/savedata");
        else if (sel == 3) listarArquivosUpload("/data/retroarch");
        else if (sel == 4) listarArquivosUpload("/data/HyperNeiva/configuracao");
        else if (sel == 5) fazerUploadDropbox("/system_data/priv/mms/app.db");
        else if (sel == 6) listarArquivosUpload("/user/av_contents/photo");
        else if (sel == 7) listarArquivosUpload("/user/home/10000000/trophy");
        else if (sel == 8) listarArquivosUpload("/data/apollo");
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        char urlSel[1024]; strcpy(urlSel, linksAtuais[sel]); int tam = strlen(urlSel);
        if (tam > 0 && (urlSel[tam - 1] == '/' || strchr(nomes[sel], '.') == NULL)) {
            if (urlSel[tam - 1] == '/') urlSel[tam - 1] = '\0';
            acessarDropbox(urlSel);
        }
        else {
            sprintf(msgStatus, "USE [TRIANGULO] PARA BAIXAR %s", nomes[sel]); msgTimer = 120;
        }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
        char urlSel[1024]; strcpy(urlSel, linksAtuais[sel]); int tam = strlen(urlSel);
        if (tam > 0 && urlSel[tam - 1] == '/') {
            urlSel[tam - 1] = '\0';
            listarArquivosUpload(urlSel);
        }
        else {
            fazerUploadDropbox(urlSel);
        }
    }
    else if (menuAtual == MENU_BAIXAR_REPOS) { if (sel == 0) listarXMLsRepositorio(); }
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) { if (strstr(nomes[sel], ".xml")) abrirXMLRepositorio(nomes[sel]); }
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) mostrarLinksJogo(sel);
    else if (menuAtual == MENU_BAIXAR_LINKS) iniciarDownload(linksAtuais[sel]);
    else if (menuAtual == MENU_CONSOLES) { consoleAtual = sel; acaoRede(NULL, true, false); }
}

void acaoCircle_Baixar() {
    if (menuAtual == MENU_BAIXAR) { preencherRoot(); }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_BACKUP) { preencherMenuBaixar(); }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        if (strlen(currentDropboxPath) == 0 || strcmp(currentDropboxPath, "/") == 0) { preencherMenuBaixar(); }
        else {
            char* ultimaBarra = strrchr(currentDropboxPath, '/');
            if (ultimaBarra != NULL) {
                if (ultimaBarra == currentDropboxPath) strcpy(currentDropboxPath, "");
                else *ultimaBarra = '\0';
                acessarDropbox(currentDropboxPath);
            }
            else { strcpy(currentDropboxPath, ""); acessarDropbox(currentDropboxPath); }
        }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
        if (strcmp(currentUploadPath, "/") == 0) {
            preencherMenuBackup();
        }
        else {
            char* ultimaBarra = strrchr(currentUploadPath, '/');
            if (ultimaBarra != NULL && ultimaBarra != currentUploadPath) {
                *ultimaBarra = '\0';
                listarArquivosUpload(currentUploadPath);
            }
            else {
                preencherMenuBackup();
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