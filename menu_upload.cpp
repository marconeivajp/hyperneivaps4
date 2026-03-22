#include "menu_upload.h"
#include "menu.h"
#include "graphics.h"
#include "baixar_dropbox_download.h"
#include <stdio.h>
#include <string.h>

bool showUploadOpcoes = false;
int selUploadOpcao = 0;
int offUploadOpcao = 0;

extern MenuLevel menuAtual;
extern char msgStatus[128];
extern int msgTimer;
extern char linksAtuais[3000][1024];
extern char nomes[3000][64];
extern int sel;
extern char currentUploadPath[512];
extern bool marcados[3000];
extern int totalItens;

extern int upX, upY, upW, upH;
extern int fontTam, listBg;

uint32_t getSysColor(int index);
// Importa o Motor Mágico de Texto!
extern void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor);

int contarMarcados() {
    int c = 0;
    for (int i = 0; i < totalItens; i++) if (marcados[i]) c++;
    return c;
}

void desenharMenuUpload(uint32_t* p) {
    if ((menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) && showUploadOpcoes) {
        int startX = upX;
        int startY = upY;

        for (int my = 0; my < upH; my++) {
            for (int mx = 0; mx < upW; mx++) {
                int pxX = startX + mx;
                int pyY = startY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = getSysColor(listBg);
                }
            }
        }

        int qtd = contarMarcados();
        bool isAll = (qtd == totalItens && totalItens > 0);
        bool isCurrentMarked = marcados[sel];

        char opt0[64]; char opt1[64]; char opt2[64];

        strcpy(opt0, isCurrentMarked ? "Desmarcar" : "Selecionar");
        strcpy(opt1, isAll ? "Desmarcar Tudo" : "Selecionar Tudo");

        if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
            if (qtd > 0) sprintf(opt2, "Upload Selecionados (%d)", qtd);
            else strcpy(opt2, "Fazer Upload Desta Pasta");
        }
        else {
            if (qtd > 0) sprintf(opt2, "Download Selecionados (%d)", qtd);
            else strcpy(opt2, "Fazer Download Desta Pasta");
        }

        const char* opcoes[3] = { opt0, opt1, opt2 };

        int maxVisible = (upH - 50) / 45;
        if (maxVisible < 1) maxVisible = 1;

        for (int i = 0; i < maxVisible; i++) {
            int gIdx = i + offUploadOpcao;
            if (gIdx >= 3) break;

            uint32_t corOp = (gIdx == selUploadOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;
            desenharTextoAlinhado(p, opcoes[gIdx], fontTam, startX, startY + 50 + (i * 45), upW, corOp);
        }
    }
}

void acaoCross_MenuUpload() {
    int qtd = contarMarcados(); char* alvo = linksAtuais[sel]; int len = strlen(alvo);

    if (selUploadOpcao == 0) { marcados[sel] = !marcados[sel]; showUploadOpcoes = false; }
    else if (selUploadOpcao == 1) { bool isAll = (qtd == totalItens && totalItens > 0); bool newState = !isAll; for (int i = 0; i < totalItens; i++) marcados[i] = newState; showUploadOpcoes = false; }
    else if (selUploadOpcao == 2) {
        if (qtd > 0) { if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) fazerUploadSelecionados(); else fazerDownloadSelecionados(); }
        else {
            if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
                if (len > 0 && alvo[len - 1] == '/') { char caminhoPastaLimpo[512]; strcpy(caminhoPastaLimpo, alvo); caminhoPastaLimpo[len - 1] = '\0'; fazerUploadPastaRecursivo(caminhoPastaLimpo); }
                else { sprintf(msgStatus, "ERRO: O ITEM SELECIONADO NAO E UMA PASTA!"); msgTimer = 180; }
            }
            else if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
                if (len > 0 && alvo[len - 1] == '/') { char caminhoPastaLimpo[512]; strcpy(caminhoPastaLimpo, alvo); caminhoPastaLimpo[len - 1] = '\0'; fazerDownloadPastaRecursivo(caminhoPastaLimpo, nomes[sel]); }
                else { iniciarDownload(alvo); }
            }
        }
        showUploadOpcoes = false;
    }
}

void acaoTriangle_MenuUpload() {
    if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) {
        showUploadOpcoes = !showUploadOpcoes;
        selUploadOpcao = 0;
        offUploadOpcao = 0;
    }
}

void acaoCircle_MenuUpload() { showUploadOpcoes = false; }