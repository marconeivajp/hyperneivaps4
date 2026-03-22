#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Bibliotecas de Rede (Sockets) Padrão
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "controle_explorar.h"
#include "menu.h"
#include "explorar.h"
#include "stb_image.h" 
#include "audio.h"     

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

// === FUNÇÃO DE INSTALAÇÃO VIA GOLDHEN FTP (COM HANDSHAKE COMPLETO) ===
void instalarPkgLocal(const char* caminhoAbsoluto) {
    sprintf(msgStatus, "CONECTANDO AO GOLDHEN FTP...");
    msgTimer = 150;
    atualizarBarra(0.2f);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = 2; // AF_INET
    addr.sin_port = 0x4908; // Porta 2121 convertida para bytes de rede (0x4908)
    addr.sin_addr.s_addr = 0x0100007F; // IP 127.0.0.1 (Localhost)

    int sock = socket(2, 1, 0); // 2 = AF_INET, 1 = SOCK_STREAM
    if (sock >= 0) {

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            char buf[512];
            memset(buf, 0, sizeof(buf));

            // 1. Recebe mensagem de boas-vindas do FTP (220)
            recv(sock, buf, sizeof(buf) - 1, 0);

            // 2. Envia Login Anónimo
            send(sock, "USER anonymous\r\n", 16, 0);
            memset(buf, 0, sizeof(buf));
            recv(sock, buf, sizeof(buf) - 1, 0); // (331)

            // 3. Envia Password Vazia
            send(sock, "PASS \r\n", 7, 0);
            memset(buf, 0, sizeof(buf));
            recv(sock, buf, sizeof(buf) - 1, 0); // (230 Logged in)

            sprintf(msgStatus, "ENVIANDO COMANDO DE INSTALACAO...");
            atualizarBarra(0.6f);

            // 4. Envia o comando secreto de instalação
            char cmd[1024];
            sprintf(cmd, "SITE INSTALL %s\r\n", caminhoAbsoluto);
            send(sock, cmd, strlen(cmd), 0);

            // 5. Recebe a resposta real do GoldHEN!
            memset(buf, 0, sizeof(buf));
            recv(sock, buf, sizeof(buf) - 1, 0);

            close(sock);

            // Verifica o código de resposta (200 é sucesso no FTP)
            if (strncmp(buf, "200", 3) == 0 || strncmp(buf, "226", 3) == 0) {
                sprintf(msgStatus, "SUCESSO! Jogo adicionado aos Downloads do PS4!");
                atualizarBarra(1.0f);
            }
            else {
                // Limpa quebras de linha para mostrar o erro na ecrã
                for (int i = 0; i < strlen(buf); i++) {
                    if (buf[i] == '\r' || buf[i] == '\n') buf[i] = '\0';
                }
                // Mostra os primeiros 45 caracteres do erro real
                buf[45] = '\0';
                sprintf(msgStatus, "ERRO DO FTP: %s", buf);
                atualizarBarra(0.0f);
            }

        }
        else {
            close(sock);
            sprintf(msgStatus, "ERRO: Ative o 'FTP Server' nas config do GoldHEN!");
            atualizarBarra(0.0f);
        }
    }
    else {
        sprintf(msgStatus, "ERRO AO CRIAR CONEXAO DE REDE");
        atualizarBarra(0.0f);
    }

    msgTimer = 600; // Tempo na ecrã
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
                // Instalação de PKG ao apertar X no Explorador
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