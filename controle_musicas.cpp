// --- INÍCIO DO ARQUIVO controle_musicas.cpp ---
#include "controle_musicas.h"
#include "menu.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

// Puxamos as variáveis globais que precisamos ler/alterar
extern int sel;
extern int off;
extern char nomes[3000][64];
extern char msgStatus[128];
extern int msgTimer;
extern MenuLevel menuAtual;
extern bool showOpcoes;
extern int selAudioOpcao;

// Puxamos as funções de outros arquivos
extern void tocarMusicaNova(const char* path);
extern void abrirMenuAudioOpcoes();
extern void tratarSelecaoAudio(int op);
extern void preencherRoot();

void acaoCross_Musicas() {
    if (showOpcoes && menuAtual == MENU_AUDIO_OPCOES) {
        tratarSelecaoAudio(selAudioOpcao);
    }
    else if (!showOpcoes && menuAtual == MENU_MUSICAS) {
        if (sel == 0) {
            tocarMusicaNova("PARADO");
            sprintf(msgStatus, "MUSICA PARADA");
            msgTimer = 120;
        }
        else {
            // Usa o caminho absoluto mapeado na busca recursiva
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
        preencherRoot();
        sel = 0;
        off = 0;
    }
}

void acaoTriangle_Musicas() {
    abrirMenuAudioOpcoes();
    menuAtual = MENU_AUDIO_OPCOES;
    showOpcoes = true;
    selAudioOpcao = 0;
}
// --- FIM DO ARQUIVO controle_musicas.cpp ---