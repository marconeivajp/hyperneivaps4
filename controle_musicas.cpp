#include "controle_musicas.h"
#include "menu.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

extern int sel;
extern int off;
extern char nomes[3000][256];
extern char msgStatus[128];
extern int msgTimer;
extern MenuLevel menuAtual;
extern bool showOpcoes;
extern int selAudioOpcao;

extern void tocarMusicaNova(const char* path);
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);
extern void preencherMenuMidia(); // Declaração para voltar ao menu MIDIA
extern char caminhoBaseMusicas[512];

void acaoCross_Musicas() {
    if (showOpcoes && menuAtual == MENU_AUDIO_OPCOES) {
        tratarSelecaoAudio(selAudioOpcao);
    }
    else if (!showOpcoes && menuAtual == MENU_MUSICAS) {
        if (strcmp(nomes[sel], "PARAR MUSICA") == 0) {
            tocarMusicaNova("PARADO");
            sprintf(msgStatus, "MUSICA PARADA");
            msgTimer = 120;
        }
        else if (nomes[sel][0] == '[') {
            // SE CLICOU NUMA PASTA, ENTRA NELA E LISTA OS ARQUIVOS!
            strcpy(caminhoNavegacaoMusicas, caminhosMusicasMenu[sel]);
            preencherMenuMusicas();
            sel = 0;
            off = 0;
        }
        else {
            // SE CLICOU NUMA MÚSICA, TOCA! O player abre só pelo Triângulo.
            if (strcmp(caminhosMusicasMenu[sel], musicaAtual) != 0) tocarMusicaNova(caminhosMusicasMenu[sel]);
            else if (comandoPausar) { comandoPausar = false; salvarConfiguracaoAudio(); }
            sprintf(msgStatus, "A TOCAR MUSICA...  (TRIANGULO ABRE O PLAYER)");
            msgTimer = 120;
        }
    }
}

// Sobe um nível de pasta. Devolve false na raiz, para o controle.cpp sair das músicas.
bool acaoCircle_Musicas() {
    if (strcmp(caminhoNavegacaoMusicas, caminhoBaseMusicas) != 0) {
        char temp[512];
        strcpy(temp, caminhoNavegacaoMusicas);
        char* ultimaBarra = strrchr(temp, '/');
        if (ultimaBarra != NULL) {
            if (ultimaBarra == temp) ultimaBarra[1] = '\0';
            else *ultimaBarra = '\0';
            strcpy(caminhoNavegacaoMusicas, temp);
            preencherMenuMusicas();
            sel = 0; off = 0;
            return true;
        }
    }
    return false;
}

// O Triângulo abre o player novo, em tela pequena (ele substitui o antigo menu de opções).
void acaoTriangle_Musicas() {
    extern void musicaPlayerAbrir(bool cheio);
    musicaPlayerAbrir(false);
}