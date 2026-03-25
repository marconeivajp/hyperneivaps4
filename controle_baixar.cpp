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
#include "ftp.h" 
#include "explorar.h"

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);

extern bool emSubmenuDropbox;
extern bool emSubmenuFTP;
extern void preencherMenuDropbox();
extern void preencherMenuFTP();
extern void atualizarHBStore();
extern bool showOpcoes;
extern int selOpcao;

extern char currentFtpPath[1024];
extern bool painelDuplo;
extern int painelAtivo;

extern int ftpL2State;
extern char currentFtpPathEsq[1024];
extern char linksAtuaisEsq[3000][1024];
extern char nomesEsq[3000][64];
extern int selEsq;
extern int offEsq;
extern int totalItensEsq;
extern char pathExplorarEsq[256];
extern MenuLevel menuAtualEsq;

void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (menuAtual == MENU_BAIXAR) {
        if (!emSubmenuLojas && !emSubmenuDropbox && !emSubmenuFTP) {
            if (sel == 0) preencherMenuRepositorios();
            else if (sel == 1) { menuAtual = MENU_BAIXAR_LINK_DIRETO; acaoCross_Notepad(uId, imeSetting, imeTitle, ""); }
            else if (sel == 2) { preencherMenuDropbox(); }
            else if (sel == 3) { preencherMenuLojas(); }
            else if (sel == 4) { preencherMenuFTP(); }
        }
        else if (emSubmenuFTP) {
            if (sel == 0) { preencherMenuFTPServidores(false); }
            else if (sel == 1) { preencherMenuFTPServidores(true); }
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
    // ==========================================
    // LOGICAS DO FTP E EXPLORADOR
    // ==========================================
    else if (menuAtual == MENU_BAIXAR_FTP_SERVIDORES) {
        if (sel == 0) {
            if (totalServidoresFTP < 100) {
                servidorAtualFTPIndex = totalServidoresFTP;
                strcpy(listaServidoresFTP[servidorAtualFTPIndex].name, "Novo Servidor");
                strcpy(listaServidoresFTP[servidorAtualFTPIndex].ip, "192.168.0.5");
                listaServidoresFTP[servidorAtualFTPIndex].port = 21;
                strcpy(listaServidoresFTP[servidorAtualFTPIndex].user, "anonymous");
                strcpy(listaServidoresFTP[servidorAtualFTPIndex].pass, "");
                totalServidoresFTP++;
                preencherMenuEditarServidor(servidorAtualFTPIndex);
            }
        }
        else {
            servidorAtualFTPIndex = sel - 1;
            acessarFTP(servidorAtualFTPIndex, "/");
        }
    }
    else if (menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR) {
        if (sel >= 0 && sel <= 4) abrirTecladoEdicaoFTP(uId, sel);
        else if (sel == 5) { salvarServidoresFTP(); preencherMenuFTPServidores(ftpSelecionandoUpload); }
        else if (sel == 6) {
            for (int i = servidorAtualFTPIndex; i < totalServidoresFTP - 1; i++) listaServidoresFTP[i] = listaServidoresFTP[i + 1];
            totalServidoresFTP--; salvarServidoresFTP(); preencherMenuFTPServidores(ftpSelecionandoUpload);
        }
    }
    else if (menuAtual == MENU_BAIXAR_FTP_LISTA) {
        if (showOpcoes) {
            acaoOpcaoFTP(selOpcao, uId);
        }
        else {
            bool isEsq = (painelDuplo && painelAtivo == 0);
            bool isLocal = (isEsq && ftpL2State == 2);

            char (*lItems)[1024] = isEsq ? linksAtuaisEsq : linksAtuais;
            char (*nItems)[64] = isEsq ? nomesEsq : nomes;
            int sAt = isEsq ? selEsq : sel;

            if (isLocal) { // CLIQUE DO LADO LOCAL PS4
                if (menuAtualEsq == MENU_EXPLORAR_HOME) {
                    if (sAt == 0) listarDiretorioEsq("/data/HyperNeiva");
                    else if (sAt == 1) listarDiretorioEsq("/");
                    else if (sAt == 2) listarDiretorioEsq("/mnt/usb0");
                    else if (sAt == 3) listarDiretorioEsq("/mnt/usb1");
                }
                else {
                    if (nItems[sAt][0] == '[') { // Entrar na Pasta
                        char tempP[512]; sprintf(tempP, "%s%s%s", pathExplorarEsq, strcmp(pathExplorarEsq, "/") == 0 ? "" : "/", &nItems[sAt][1]); tempP[strlen(tempP) - 1] = '\0';
                        listarDiretorioEsq(tempP);
                    }
                    else { // ARQUIVO - AGORA ABRE MENU (IGUAL EXPLORAR)
                        preencherOpcoesFTP();
                    }
                }
            }
            else { // CLIQUE DO LADO FTP PC
                char urlSel[1024]; strcpy(urlSel, lItems[sAt]); int tam = strlen(urlSel);
                if (tam > 0 && urlSel[tam - 1] == '/') { // Pasta FTP
                    urlSel[tam - 1] = '\0';
                    if (isEsq) acessarFTPEsq(servidorAtualFTPIndex, urlSel); else acessarFTP(servidorAtualFTPIndex, urlSel);
                }
                else { // Arquivo FTP
                    char* ext = strrchr(urlSel, '.');
                    if (ext && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".ini") == 0 || strcasecmp(ext, ".xml") == 0)) {
                        prepararPreviewFTP(urlSel); // Abre midia na hora!
                    }
                    else {
                        preencherOpcoesFTP(); // ARQUIVO - AGORA ABRE MENU (IGUAL EXPLORAR)
                    }
                }
            }
        }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_BACKUP) { if (sel == 0) listarArquivosUpload("/data/HyperNeiva"); else if (sel == 1) listarArquivosUpload("/"); else if (sel == 2) listarArquivosUpload("/mnt/usb0"); else if (sel == 3) listarArquivosUpload("/mnt/usb1"); else if (sel == 4) executarBackupTodos(); }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) { char urlSel[1024]; strcpy(urlSel, linksAtuais[sel]); int tam = strlen(urlSel); if (strcmp(urlSel, "UPDATE_HB_STORE") == 0) { atualizarHBStore(); return; } if (tam > 0 && urlSel[tam - 1] == '/') { if (emApolloSaves) acessarApolloSaves(urlSel); else { urlSel[tam - 1] = '\0'; acessarDropbox(urlSel); } } else { iniciarDownload(urlSel); } }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) { char urlSel[1024]; strcpy(urlSel, linksAtuais[sel]); int tam = strlen(urlSel); if (tam > 0 && urlSel[tam - 1] == '/') { urlSel[tam - 1] = '\0'; listarArquivosUpload(urlSel); } else { fazerUploadDropbox(urlSel); } }
    else if (menuAtual == MENU_BAIXAR_REPOS) { if (sel == 0) listarXMLsRepositorio(); }
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) { if (strstr(nomes[sel], ".xml")) abrirXMLRepositorio(nomes[sel]); }
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) mostrarLinksJogo(sel);
    else if (menuAtual == MENU_BAIXAR_LINKS) iniciarDownload(linksAtuais[sel]);
    else if (menuAtual == MENU_CONSOLES) { consoleAtual = sel; acaoRede(NULL, true, false); }
    else if (menuAtual == SCRAPER_LIST) { acaoRede(nomes[sel], false, true); }
}

void acaoCircle_Baixar() {
    if (menuAtual == MENU_BAIXAR) { if (emSubmenuLojas || emSubmenuDropbox || emSubmenuFTP) preencherMenuBaixar(); else preencherRoot(); }
    else if (menuAtual == MENU_BAIXAR_FTP_SERVIDORES) preencherMenuFTP();
    else if (menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR) { carregarServidoresFTP(); preencherMenuFTPServidores(ftpSelecionandoUpload); }
    else if (menuAtual == MENU_BAIXAR_FTP_LISTA) {
        bool isEsq = (painelDuplo && painelAtivo == 0);
        bool isLocal = (isEsq && ftpL2State == 2);

        if (isLocal) {
            if (menuAtualEsq == MENU_EXPLORAR_HOME) {
                // Não faz nada
            }
            else if (strcmp(pathExplorarEsq, "/") == 0 || strcmp(pathExplorarEsq, "/data/HyperNeiva") == 0 || strcmp(pathExplorarEsq, "/mnt/usb0") == 0 || strcmp(pathExplorarEsq, "/mnt/usb1") == 0) {
                memset(nomesEsq, 0, sizeof(nomesEsq));
                strcpy(nomesEsq[0], "Hyper Neiva"); strcpy(nomesEsq[1], "Raiz"); strcpy(nomesEsq[2], "USB 0"); strcpy(nomesEsq[3], "USB 1");
                totalItensEsq = 4; selEsq = 0; offEsq = 0; menuAtualEsq = MENU_EXPLORAR_HOME;
            }
            else {
                char* ultima = strrchr(pathExplorarEsq, '/');
                if (ultima && ultima != pathExplorarEsq) *ultima = '\0'; else strcpy(pathExplorarEsq, "/");
                listarDiretorioEsq(pathExplorarEsq);
            }
        }
        else {
            char pPath[1024]; strcpy(pPath, isEsq ? currentFtpPathEsq : currentFtpPath);
            if (strlen(pPath) == 0 || strcmp(pPath, "/") == 0) {
                if (!painelDuplo) preencherMenuFTPServidores(false);
            }
            else {
                char* ultima = strrchr(pPath, '/');
                if (ultima && ultima != pPath) *ultima = '\0'; else strcpy(pPath, "/");
                if (isEsq) acessarFTPEsq(servidorAtualFTPIndex, pPath); else acessarFTP(servidorAtualFTPIndex, pPath);
            }
        }
    }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_BACKUP) { preencherMenuDropbox(); }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) { if (emApolloSaves) { if (strcmp(currentApolloUrl, "https://bucanero.github.io/apollo-saves/") == 0 || strlen(currentApolloUrl) < 41) { preencherMenuLojas(); } else { int len = strlen(currentApolloUrl); if (currentApolloUrl[len - 1] == '/') currentApolloUrl[len - 1] = '\0'; char* ultimaBarra = strrchr(currentApolloUrl, '/'); if (ultimaBarra != NULL) { *(ultimaBarra + 1) = '\0'; acessarApolloSaves(currentApolloUrl); } else { preencherMenuLojas(); } } } else { if (!emSubmenuLojas && !emApolloSaves && !emSubmenuDropbox && strlen(currentDropboxPath) == 0) { preencherMenuLojas(); } else if (strlen(currentDropboxPath) == 0 || strcmp(currentDropboxPath, "/") == 0) { if (emSubmenuLojas) preencherMenuLojas(); else if (emSubmenuDropbox) preencherMenuDropbox(); else preencherMenuBaixar(); } else { char* ultimaBarra = strrchr(currentDropboxPath, '/'); if (ultimaBarra != NULL) { if (ultimaBarra == currentDropboxPath) strcpy(currentDropboxPath, ""); else *ultimaBarra = '\0'; acessarDropbox(currentDropboxPath); } else { strcpy(currentDropboxPath, ""); acessarDropbox(currentDropboxPath); } } } }
    else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) { if (strcmp(currentUploadPath, "/") == 0 || strcmp(currentUploadPath, "/data/HyperNeiva") == 0 || strcmp(currentUploadPath, "/mnt/usb0") == 0 || strcmp(currentUploadPath, "/mnt/usb1") == 0 || strlen(currentUploadPath) == 0) { preencherMenuBackup(); } else { char* ultimaBarra = strrchr(currentUploadPath, '/'); if (ultimaBarra != NULL) { if (ultimaBarra == currentUploadPath) { strcpy(currentUploadPath, "/"); listarArquivosUpload(currentUploadPath); } else { *ultimaBarra = '\0'; listarArquivosUpload(currentUploadPath); } } else { preencherMenuBackup(); } } }
    else if (menuAtual == MENU_BAIXAR_REPOS) preencherMenuBaixar();
    else if (menuAtual == MENU_BAIXAR_GAMES_XMLS) preencherMenuRepositorios();
    else if (menuAtual == MENU_BAIXAR_GAMES_LIST) listarXMLsRepositorio();
    else if (menuAtual == MENU_BAIXAR_LINKS) { char nomeXML[256]; char* ultimaBarra = strrchr(caminhoXMLAtual, '/'); if (ultimaBarra) strcpy(nomeXML, ultimaBarra + 1); abrirXMLRepositorio(nomeXML); }
    else if (menuAtual == MENU_CONSOLES) { preencherMenuLojas(); }
    else if (menuAtual == SCRAPER_LIST) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "Sony - PlayStation"); strcpy(nomes[1], "Sony - PlayStation Portable"); strcpy(nomes[2], "Nintendo - Super Nintendo Entertainment System"); strcpy(nomes[3], "Sega - Mega Drive - Genesis"); strcpy(nomes[4], "Nintendo - Nintendo Entertainment System"); totalItens = 5; menuAtual = MENU_CONSOLES; sel = 0; off = 0; }
}

void acaoTriangle_Baixar() {
    if (menuAtual == MENU_BAIXAR_FTP_SERVIDORES) {
        if (sel > 0) { servidorAtualFTPIndex = sel - 1; preencherMenuEditarServidor(servidorAtualFTPIndex); }
    }
    else if (menuAtual == MENU_BAIXAR_FTP_LISTA) {
        preencherOpcoesFTP();
    }
}