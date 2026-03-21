#include "menu_upload.h"
#include "menu.h"
#include "graphics.h"
#include "baixar_dropbox_download.h"
#include <stdio.h>
#include <string.h>

bool showUploadOpcoes = false;
int selUploadOpcao = 0;

extern MenuLevel menuAtual;
extern int discoX, discoY;
extern char msgStatus[128];
extern int msgTimer;

extern char linksAtuais[3000][1024];
extern char nomes[3000][64];
extern int sel;
extern char currentUploadPath[512];
extern bool marcados[3000];
extern int totalItens;

// Conta quantos arquivos estão marcados
int contarMarcados() {
    int c = 0;
    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) c++;
    }
    return c;
}

void desenharMenuUpload(uint32_t* p) {
    if ((menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) && showUploadOpcoes) {
        int startX = discoX;
        int startY = discoY - 100;

        // Fundo escuro ajustado para 3 opções
        for (int my = 0; my < 190; my++) {
            for (int mx = 0; mx < 480; mx++) {
                int pxX = startX + mx;
                int pyY = startY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = 0xEE111111;
                }
            }
        }

        int qtd = contarMarcados();
        bool isAll = (qtd == totalItens && totalItens > 0);
        bool isCurrentMarked = marcados[sel];

        char opt0[64];
        char opt1[64];
        char opt2[64];

        // Se já está marcado, muda o texto para "Desmarcar"
        strcpy(opt0, isCurrentMarked ? "Desmarcar" : "Selecionar");
        strcpy(opt1, isAll ? "Desmarcar Tudo" : "Selecionar Tudo");

        // LÓGICA INTELIGENTE: Ou mostra Selecionados, ou mostra a Pasta!
        if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
            if (qtd > 0) sprintf(opt2, "Upload Selecionados (%d)", qtd);
            else strcpy(opt2, "Fazer Upload Desta Pasta");
        }
        else {
            if (qtd > 0) sprintf(opt2, "Download Selecionados (%d)", qtd);
            else strcpy(opt2, "Fazer Download Desta Pasta");
        }

        const char* opcoes[3] = { opt0, opt1, opt2 };

        // Desenha as 3 opções
        for (int i = 0; i < 3; i++) {
            uint32_t corOp = (i == selUploadOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
            desenharTexto(p, opcoes[i], 30, startX + 20, startY + 50 + (i * 45), corOp);
        }
    }
}

void acaoCross_MenuUpload() {
    int qtd = contarMarcados();
    char* alvo = linksAtuais[sel];
    int len = strlen(alvo);

    if (selUploadOpcao == 0) {
        // OPÇÃO 0: Selecionar / Desmarcar o item atual
        marcados[sel] = !marcados[sel];
        showUploadOpcoes = false;
    }
    else if (selUploadOpcao == 1) {
        // OPÇÃO 1: Selecionar Tudo / Desmarcar Tudo
        bool isAll = (qtd == totalItens && totalItens > 0);
        bool newState = !isAll;
        for (int i = 0; i < totalItens; i++) marcados[i] = newState;
        showUploadOpcoes = false;
    }
    else if (selUploadOpcao == 2) {
        // OPÇÃO 2: Executa a ação dependendo se tem ou não arquivos marcados
        if (qtd > 0) {
            // AÇÃO DOS SELECIONADOS
            if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) fazerUploadSelecionados();
            else fazerDownloadSelecionados();
        }
        else {
            // AÇÃO DA PASTA INTEIRA
            if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
                if (len > 0 && alvo[len - 1] == '/') {
                    char caminhoPastaLimpo[512]; strcpy(caminhoPastaLimpo, alvo); caminhoPastaLimpo[len - 1] = '\0';
                    fazerUploadPastaRecursivo(caminhoPastaLimpo);
                }
                else {
                    sprintf(msgStatus, "ERRO: O ITEM SELECIONADO NAO E UMA PASTA!"); msgTimer = 180;
                }
            }
            else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
                if (len > 0 && alvo[len - 1] == '/') {
                    char caminhoPastaLimpo[512]; strcpy(caminhoPastaLimpo, alvo); caminhoPastaLimpo[len - 1] = '\0';
                    fazerDownloadPastaRecursivo(caminhoPastaLimpo, nomes[sel]);
                }
                else {
                    iniciarDownload(alvo);
                }
            }
        }
        showUploadOpcoes = false;
    }
}

void acaoTriangle_MenuUpload() {
    if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        showUploadOpcoes = !showUploadOpcoes;
        selUploadOpcao = 0; // Volta o cursor para o topo
    }
}

void acaoCircle_MenuUpload() {
    showUploadOpcoes = false;
}