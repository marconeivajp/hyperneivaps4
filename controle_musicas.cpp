#include "controle_musicas.h"
#include "menu.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

extern int sel;
extern int off;
extern char nomes[3000][64];
extern char msgStatus[128];
extern int msgTimer;
extern MenuLevel menuAtual;
extern bool showOpcoes;
extern int selAudioOpcao;

extern void tocarMusicaNova(const char* path);
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);
extern void preencherMenuMidia(); // Declaração para voltar ao menu MIDIA

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
            // SE CLICOU NUMA MÚSICA, TOCA!
            tocarMusicaNova(caminhosMusicasMenu[sel]);
            sprintf(msgStatus, "A TOCAR MUSICA...");
            msgTimer = 120;
        }
    }
}

void acaoCircle_Musicas() {
    if (showOpcoes || menuAtual == MENU_AUDIO_OPCOES) {
        showOpcoes = false;
        menuAtual = MENU_MUSICAS;
    }
    else {
        // Se estiver na pasta raiz, voltar ao menu MIDIA
        if (strcmp(caminhoNavegacaoMusicas, "/data/HyperNeiva/Musicas") == 0) {
            preencherMenuMidia();
            sel = 0;
            off = 0;
        }
        else {
            // Volta uma pasta para trás
            char temp[512];
            strcpy(temp, caminhoNavegacaoMusicas);
            char* ultimaBarra = strrchr(temp, '/');
            if (ultimaBarra != NULL) {
                *ultimaBarra = '\0';
                strcpy(caminhoNavegacaoMusicas, temp);
                preencherMenuMusicas();
                sel = 0;
                off = 0;
            }
        }
    }
}

void acaoTriangle_Musicas() {
    abrirMenuAudioOpcoes();
    menuAtual = MENU_AUDIO_OPCOES;
    showOpcoes = true;
    selAudioOpcao = 0;
}