#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "controle_explorar.h"
#include "menu.h"
#include "explorar.h"
#include "stb_image.h" // NECESSÁRIO PARA LER AS IMAGENS
#include "audio.h"     // NECESSÁRIO PARA TOCAR A MÚSICA

extern int cd;
extern void preencherExplorerHome();
extern void preencherRoot();

// --- VARIÁVEIS DA IMAGEM (Vindas do menu_grafico) ---
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;

// --- VARIÁVEL PARA O ÁUDIO ---
static char caminhoMusicaTocando[512] = "";


void acaoL2_Explorar() {
    painelDuplo = !painelDuplo;
    if (painelDuplo) {
        painelAtivo = 0; // Foca no painel esquerdo ao abrir
        menuAtualEsq = MENU_EXPLORAR_HOME; // Reseta pra Home (Hyper Neiva, Raiz, USB0, USB1)
        selEsq = 0;
    }
    else {
        painelAtivo = 1; // Foca no direito ao fechar
    }
}

void alternarPainelAtivo() {
    if (painelDuplo && !showOpcoes) {
        painelAtivo = (painelAtivo == 0) ? 1 : 0;
    }
}

void acaoCross_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;

    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
    int sAtual = ehEsq ? selEsq : sel;
    char (*nItems)[64] = ehEsq ? nomesEsq : nomes;
    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

    if (mAtual == MENU_EXPLORAR && showOpcoes) {
        acaoArquivo(selOpcao);
        return;
    }

    if (mAtual == MENU_EXPLORAR_HOME) {
        char tempBase[256];
        if (sAtual == 0) strcpy(tempBase, "/data/HyperNeiva");
        else if (sAtual == 1) strcpy(tempBase, "/");
        else if (sAtual == 2) strcpy(tempBase, "/mnt/usb0");
        else if (sAtual == 3) strcpy(tempBase, "/mnt/usb1");

        if (ehEsq) { listarDiretorioEsq(tempBase); }
        else { strcpy(baseRaiz, tempBase); listarDiretorio(baseRaiz); }
    }
    else if (mAtual == MENU_EXPLORAR) {
        // SE FOR UMA PASTA
        if (nItems[sAtual][0] == '[') {
            char pL[128]; strncpy(pL, &nItems[sAtual][1], strlen(nItems[sAtual]) - 2); pL[strlen(nItems[sAtual]) - 2] = '\0';
            char nP[256]; sprintf(nP, "%s/%s", pExplorar, pL);
            if (ehEsq) listarDiretorioEsq(nP); else listarDiretorio(nP);
        }
        // SE FOR UM ARQUIVO (AÇÃO AO CLICAR)
        else {
            char caminhoArquivo[512];
            sprintf(caminhoArquivo, "%s/%s", pExplorar, nItems[sAtual]);

            // Pega a extensão do arquivo (ex: .mp3, .png)
            char* ext = strrchr(nItems[sAtual], '.');

            if (ext) {
                // 1. LÓGICA DE ÁUDIO (MP3 / WAV) COM A FUNÇÃO CORRETA "tocarMusicaNova"
                if (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0) {

                    if (strcmp(caminhoMusicaTocando, caminhoArquivo) == 0) {
                        // Clicou na MESMA música que já está a tocar -> PARA A MÚSICA
                        // A sua lógica exige enviar "PARADO" para pausar
                        tocarMusicaNova("PARADO");

                        strcpy(caminhoMusicaTocando, ""); // Limpa o registo
                        sprintf(msgStatus, "Música Parada");
                        msgTimer = 90;
                    }
                    else {
                        // Clicou numa música NOVA ou nenhuma estava a tocar -> TOCA A MÚSICA
                        tocarMusicaNova(caminhoArquivo);

                        strcpy(caminhoMusicaTocando, caminhoArquivo); // Regista qual está a tocar
                        sprintf(msgStatus, "Reproduzindo Áudio");
                        msgTimer = 90;
                    }
                }
                // 2. LÓGICA DE IMAGEM (PNG / JPG / JPEG / BMP)
                else if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".bmp") == 0) {

                    if (imgMidia) {
                        stbi_image_free(imgMidia); // Limpa a memória da imagem anterior
                        imgMidia = NULL;
                    }

                    int canais;
                    imgMidia = stbi_load(caminhoArquivo, &wM, &hM, &canais, 4); // Carrega a nova

                    if (imgMidia) {
                        visualizandoMidiaImagem = true; // Ativa a tela de visualização no menu_grafico
                        zoomMidia = 1.0f;
                        fullscreenMidia = false;
                    }
                    else {
                        sprintf(msgStatus, "ERRO AO CARREGAR IMAGEM");
                        msgTimer = 90;
                    }
                }
            }
        }
    }
}

void acaoCircle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;

    // SE ESTIVER A VER UMA IMAGEM, A BOLINHA APENAS FECHA A IMAGEM
    if (visualizandoMidiaImagem) {
        visualizandoMidiaImagem = false;
        if (imgMidia) {
            stbi_image_free(imgMidia);
            imgMidia = NULL;
        }
        return; // Sai da função para não fechar o explorador!
    }

    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

    if (mAtual == MENU_EXPLORAR_HOME) {
        // 1. SE APERTAR BOLINHA NA TELA INICIAL (HOME), DESLIGA O PAINEL DUPLO
        if (painelDuplo) {
            painelDuplo = false;
            painelAtivo = 1; // Devolve o foco ao lado direito de forma segura
        }

        // 2. SE ESTAVA A USAR O LADO DIREITO (PRINCIPAL), VOLTA PARA O MENU ROOT
        if (!ehEsq) {
            preencherRoot();
        }
    }
    else if (mAtual == MENU_EXPLORAR) {
        // Se estiver na base ou na raiz do disco, volta para a tela inicial (Home) do explorador
        if (strcmp(pExplorar, baseRaiz) == 0 || strcmp(pExplorar, "/") == 0) {
            if (ehEsq) menuAtualEsq = MENU_EXPLORAR_HOME;
            else preencherExplorerHome();
        }
        else {
            // Volta uma pasta para trás
            char temp[256]; strcpy(temp, pExplorar);
            char* last = strrchr(temp, '/');
            if (last) {
                if (last == temp) strcpy(temp, "/");
                else *last = '\0';
                if (ehEsq) listarDiretorioEsq(temp); else listarDiretorio(temp);
            }
        }
    }
}

void acaoTriangle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;

    if (mAtual == MENU_EXPLORAR) {
        showOpcoes = !showOpcoes;
        selOpcao = 0;
    }
}

void acaoR1_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
    int sAtual = ehEsq ? selEsq : sel;
    bool* mItems = ehEsq ? marcadosEsq : marcados;

    if (mAtual == MENU_EXPLORAR) {
        if (cd <= 0) { mItems[sAtual] = !mItems[sAtual]; cd = 12; }
    }
}