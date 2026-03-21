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
#include "baixar_repositorio.h"
#include "baixar_dropbox_download.h"

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);

void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (menuAtual == MENU_BAIXAR) {
        if (sel == 0) preencherMenuRepositorios();
        else if (sel == 1) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "RETROARCH"); totalItens = 1; menuAtual = MENU_CAPAS; }
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

        // Verifica se é uma pasta (termina com '/')
        if (tam > 0 && urlSel[tam - 1] == '/') {
            urlSel[tam - 1] = '\0';
            acessarDropbox(urlSel); // Entra na pasta
        }
        else {
            // SE FOR ARQUIVO, AGORA ELE BAIXA DIRETO NO BOTÃO X!
            iniciarDownload(urlSel);
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
    else if (menuAtual == MENU_CAPAS) {
        if (sel == 0) {
            memset(nomes, 0, sizeof(nomes));
            strcpy(nomes[0], "Sony - PlayStation");
            strcpy(nomes[1], "Sony - PlayStation Portable");
            strcpy(nomes[2], "Nintendo - Super Nintendo Entertainment System");
            strcpy(nomes[3], "Sega - Mega Drive - Genesis");
            strcpy(nomes[4], "Nintendo - Nintendo Entertainment System");
            totalItens = 5;
            menuAtual = MENU_CONSOLES;
            sel = 0;
            off = 0;
        }
    }
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
    else if (menuAtual == MENU_CAPAS) {
        preencherMenuBaixar();
    }
    else if (menuAtual == MENU_CONSOLES) {
        memset(nomes, 0, sizeof(nomes));
        strcpy(nomes[0], "RETROARCH");
        totalItens = 1;
        menuAtual = MENU_CAPAS;
        sel = 0;
        off = 0;
    }
    else if (menuAtual == SCRAPER_LIST) {
        memset(nomes, 0, sizeof(nomes));
        strcpy(nomes[0], "Sony - PlayStation");
        strcpy(nomes[1], "Sony - PlayStation Portable");
        strcpy(nomes[2], "Nintendo - Super Nintendo Entertainment System");
        strcpy(nomes[3], "Sega - Mega Drive - Genesis");
        strcpy(nomes[4], "Nintendo - Nintendo Entertainment System");
        totalItens = 5;
        menuAtual = MENU_CONSOLES;
        sel = 0;
        off = 0;
    }
}

void acaoTriangle_Baixar() {
    // Agora o Triângulo é tratado diretamente no arquivo controle.cpp (para abrir o menu suspenso),
    // portanto não precisamos de código aqui para evitar conflitos!
}