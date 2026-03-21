#include "menu_upload.h"
#include "menu.h"
#include "graphics.h"
#include "baixar_dropbox_download.h"
#include <stdio.h>
#include <string.h>

bool showUploadOpcoes = false;
int selUploadOpcao = 0;
const char* listaOpcoesUpload[1] = { "Fazer Upload Desta Pasta" };

extern MenuLevel menuAtual;
extern char currentUploadPath[512];

// Puxando as variáveis de posição do CD (Disco)
extern int discoX, discoY;

void desenharMenuUpload(uint32_t* p) {
    if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD && showUploadOpcoes) {

        // Colocando exatamente onde o menu de opções de arquivos (Explorar) fica
        int startX = discoX;
        int startY = discoY - 100;

        // Fundo escuro do mini-menu
        for (int my = 0; my < 120; my++) {
            for (int mx = 0; mx < 450; mx++) {
                int pxX = startX + mx;
                int pyY = startY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = 0xEE111111;
                }
            }
        }

        // Desenha as opções do menu
        for (int i = 0; i < 1; i++) {
            uint32_t corOp = (i == selUploadOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF; // Fica amarelo ao selecionar
            desenharTexto(p, listaOpcoesUpload[i], 30, startX + 20, startY + 50 + (i * 45), corOp);
        }
    }
}

void acaoCross_MenuUpload() {
    if (selUploadOpcao == 0) {
        showUploadOpcoes = false;
        // Inicia a função de varrer a pasta e fazer upload de tudo!
        fazerUploadPastaDropbox(currentUploadPath);
    }
}

void acaoTriangle_MenuUpload() {
    if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) {
        showUploadOpcoes = !showUploadOpcoes;
        selUploadOpcao = 0;
    }
}

void acaoCircle_MenuUpload() {
    showUploadOpcoes = false;
}