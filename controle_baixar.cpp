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
#include "baixar_lojas.h" 

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);

extern bool emSubmenuDropbox;
extern void preencherMenuDropbox();
extern void atualizarHBStore();

void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (menuAtual == MENU_BAIXAR) {
        if (!emSubmenuLojas && !emSubmenuDropbox) {
            if (sel == 0) preencherMenuRepositorios();
            else if (sel == 1) { menuAtual = MENU_BAIXAR_LINK_DIRETO; acaoCross_Notepad(uId, imeSetting, imeTitle, ""); }
            else if (sel == 2) { preencherMenuDropbox(); }
            else if (sel == 3) { preencherMenuLojas(); }
        }
        else if (emSubmenuDropbox) {
            if (sel == 0) { acessarDropbox(""); }
            else if (sel == 1) { preencherMenuBackup(); }
        }
        else if (emSubmenuLojas) {
            if (sel == 0) { emApolloSaves = false; acessarHBStore(); }
            else if (sel == 1) { emApolloSaves = true; acessarApolloSaves("https://bucanero.github.io/apollo-saves/"); }
            else if (sel == 2) {
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
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_BACKUP) {
        // LÓGICA DO EXPLORER APLICADA AO UPLOAD!
        if (sel == 0) listarArquivosUpload("/data/HyperNeiva");
        else if (sel == 1) listarArquivosUpload("/");
        else if (sel == 2) listarArquivosUpload("/mnt/usb0");
        else if (sel == 3) listarArquivosUpload("/mnt/usb1");
        else if (sel == 4) executarBackupTodos();
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        char urlSel[1024]; strcpy(urlSel, linksAtuais[sel]); int tam = strlen(urlSel);

        if (strcmp(urlSel, "UPDATE_HB_STORE") == 0) {
            atualizarHBStore();
            return;
        }

        if (tam > 0 && urlSel[tam - 1] == '/') {
            if (emApolloSaves) {
                acessarApolloSaves(urlSel);
            }
            else {
                urlSel[tam - 1] = '\0';
                acessarDropbox(urlSel);
            }
        }
        else {
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
    else if (menuAtual == MENU_CONSOLES) {
        consoleAtual = sel;
        acaoRede(NULL, true, false);
    }
    else if (menuAtual == SCRAPER_LIST) {
        acaoRede(nomes[sel], false, true);
    }
}

void acaoCircle_Baixar() {
    if (menuAtual == MENU_BAIXAR) {
        if (emSubmenuLojas || emSubmenuDropbox) { preencherMenuBaixar(); }
        else { preencherRoot(); }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_BACKUP) { preencherMenuDropbox(); }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        if (emApolloSaves) {
            if (strcmp(currentApolloUrl, "https://bucanero.github.io/apollo-saves/") == 0 || strlen(currentApolloUrl) < 41) {
                preencherMenuLojas();
            }
            else {
                int len = strlen(currentApolloUrl);
                if (currentApolloUrl[len - 1] == '/') currentApolloUrl[len - 1] = '\0';
                char* ultimaBarra = strrchr(currentApolloUrl, '/');
                if (ultimaBarra != NULL) {
                    *(ultimaBarra + 1) = '\0';
                    acessarApolloSaves(currentApolloUrl);
                }
                else {
                    preencherMenuLojas();
                }
            }
        }
        else {
            if (!emSubmenuLojas && !emApolloSaves && !emSubmenuDropbox && strlen(currentDropboxPath) == 0) {
                preencherMenuLojas();
            }
            else if (strlen(currentDropboxPath) == 0 || strcmp(currentDropboxPath, "/") == 0) {
                if (emSubmenuLojas) preencherMenuLojas();
                else if (emSubmenuDropbox) preencherMenuDropbox();
                else preencherMenuBaixar();
            }
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
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
        // Lógica ultra-inteligente para o Círculo no Explorador de Upload
        if (strcmp(currentUploadPath, "/") == 0 || strcmp(currentUploadPath, "/data/HyperNeiva") == 0 || strcmp(currentUploadPath, "/mnt/usb0") == 0 || strcmp(currentUploadPath, "/mnt/usb1") == 0 || strlen(currentUploadPath) == 0) {
            preencherMenuBackup(); // Se estiver numa das 4 raízes, volta para o Menu Base
        }
        else {
            char* ultimaBarra = strrchr(currentUploadPath, '/');
            if (ultimaBarra != NULL) {
                if (ultimaBarra == currentUploadPath) {
                    strcpy(currentUploadPath, "/");
                    listarArquivosUpload(currentUploadPath);
                }
                else {
                    *ultimaBarra = '\0';
                    listarArquivosUpload(currentUploadPath);
                }
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
    else if (menuAtual == MENU_CONSOLES) {
        preencherMenuLojas();
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
}