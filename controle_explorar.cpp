#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <orbis/libkernel.h> 

#include "controle_explorar.h"
#include "menu.h"
#include "explorar.h"
#include "stb_image.h" 
#include "audio.h"   
#include "bloco_de_notas.h" // INCLUIDO PARA USAR O BLOCO DE NOTAS

extern int cd;
extern void preencherExplorerHome();
extern void preencherRoot();

// Variáveis da Interface
extern void atualizarBarra(float progresso);
extern char msgStatus[128];
extern int msgTimer;

// --- VARIÁVEIS DA IMAGEM ---
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;

// --- VARIÁVEIS PARA O ÁUDIO ---
extern char caminhoNavegacaoMusicas[512];
static char caminhoMusicaTocando[512] = "";

// === FUNÇÃO DEFINITIVA (MÉTODO HB-STORE) ===
void instalarPkgLocal(const char* caminhoAbsoluto) {

    // Extrai o nome do ficheiro (ex: jogo.pkg)
    char nomeArquivo[128] = "arquivo.pkg";
    char* ref = strrchr(caminhoAbsoluto, '/');
    if (ref) strncpy(nomeArquivo, ref + 1, 127);

    // Garante que a pasta oficial do GoldHEN existe no HD
    sceKernelMkdir("/data/pkg", 0777);

    char destino[512];
    sprintf(destino, "/data/pkg/%s", nomeArquivo);

    // Se o utilizador já clicou no PKG dentro da própria pasta do GoldHEN
    if (strcmp(caminhoAbsoluto, destino) == 0) {
        sprintf(msgStatus, "PRONTO! Va em GoldHEN -> Package Installer");
        atualizarBarra(1.0f);
    }
    else {
        sprintf(msgStatus, "PREPARANDO INSTALACAO (AGUARDE)...");
        atualizarBarra(0.5f);

        // A função 'rename' move o ficheiro no HD instantaneamente sem copiar (leva 1 milissegundo)
        int resMove = rename(caminhoAbsoluto, destino);

        if (resMove == 0) {
            sprintf(msgStatus, "PREPARADO! Va em GoldHEN -> Package Installer");
            atualizarBarra(1.0f);
        }
        else {
            sprintf(msgStatus, "ERRO AO MOVER PKG (Erro %d)", resMove);
            atualizarBarra(0.0f);
        }
    }

    msgTimer = 600; // Tempo longo na ecrã para o utilizador ler com calma
}

void acaoL2_Explorar() {
    painelDuplo = !painelDuplo;
    if (painelDuplo) {
        painelAtivo = 0;
        menuAtualEsq = MENU_EXPLORAR_HOME;
        selEsq = 0;
    }
    else {
        painelAtivo = 1;
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
        if (nItems[sAtual][0] == '[') {
            char pL[128]; strncpy(pL, &nItems[sAtual][1], strlen(nItems[sAtual]) - 2); pL[strlen(nItems[sAtual]) - 2] = '\0';
            char nP[256]; sprintf(nP, "%s/%s", pExplorar, pL);
            if (ehEsq) listarDiretorioEsq(nP); else listarDiretorio(nP);
        }
        else {
            char caminhoArquivo[512];
            sprintf(caminhoArquivo, "%s/%s", pExplorar, nItems[sAtual]);
            char* ext = strrchr(nItems[sAtual], '.');

            if (ext) {
                // Preparação de PKG ao apertar X no Explorador
                if (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0) {
                    instalarPkgLocal(caminhoArquivo);
                }
                else if (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0) {
                    if (strcmp(caminhoMusicaTocando, caminhoArquivo) == 0) {
                        tocarMusicaNova("PARADO");
                        strcpy(caminhoMusicaTocando, "");
                        sprintf(msgStatus, "Música Parada");
                        msgTimer = 90;
                    }
                    else {
                        tocarMusicaNova(caminhoArquivo);
                        strcpy(caminhoMusicaTocando, caminhoArquivo);
                        sprintf(msgStatus, "Reproduzindo Áudio");
                        msgTimer = 90;
                    }
                }
                else if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".bmp") == 0) {
                    if (imgMidia) {
                        stbi_image_free(imgMidia);
                        imgMidia = NULL;
                    }
                    int canais;
                    imgMidia = stbi_load(caminhoArquivo, &wM, &hM, &canais, 4);
                    if (imgMidia) {
                        visualizandoMidiaImagem = true;
                        zoomMidia = 1.0f;
                        fullscreenMidia = false;
                    }
                    else {
                        sprintf(msgStatus, "ERRO AO CARREGAR IMAGEM");
                        msgTimer = 90;
                    }
                }
                // INTEGRAÇÃO: Abrir textos no Bloco de Notas para edição
                else if (strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".xml") == 0 ||
                    strcasecmp(ext, ".json") == 0 || strcasecmp(ext, ".ini") == 0 ||
                    strcasecmp(ext, ".cfg") == 0 || strcasecmp(ext, ".log") == 0) {

                    editarArquivoExistente(pExplorar, nItems[sAtual]);

                    if (ehEsq) {
                        menuAtualEsq = MENU_NOTEPAD;
                    }
                    else {
                        menuAtual = MENU_NOTEPAD;
                    }
                }
            }
        }
    }
}

void acaoCircle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;

    if (visualizandoMidiaImagem) {
        visualizandoMidiaImagem = false;
        if (imgMidia) {
            stbi_image_free(imgMidia);
            imgMidia = NULL;
        }
        return;
    }

    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

    if (mAtual == MENU_EXPLORAR_HOME) {
        if (painelDuplo) {
            painelDuplo = false;
            painelAtivo = 1;
        }
        if (!ehEsq) {
            preencherRoot();
        }
    }
    else if (mAtual == MENU_EXPLORAR) {
        if (strcmp(pExplorar, baseRaiz) == 0 || strcmp(pExplorar, "/") == 0) {
            if (ehEsq) menuAtualEsq = MENU_EXPLORAR_HOME;
            else preencherExplorerHome();
        }
        else {
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