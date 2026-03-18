#include <stdio.h>
#include <string.h>

// Resolve o erro de va_list caso algum header Sony seja puxado
#include <stdarg.h>
#ifndef __builtin_va_list
typedef va_list __builtin_va_list;
#endif

#include "globals.h"

// ATENÇÃO: A linha "int selAudioOpcao = 0;" FOI REMOVIDA daqui 
// para evitar o erro de "Duplicate Symbol", pois ela já existe no main.cpp.

// =========================================================
// 1. FUNÇÕES DO MENU
// =========================================================

void abrirMenuAudioOpcoes() {
    menuAtual = MENU_AUDIO_OPCOES;
    selAudioOpcao = 0; // Reseta a seleção ao abrir
    showOpcoes = true;
}

void tratarSelecaoAudio(int op) {
    // Proteção: se não houver itens na lista, ignora comandos de faixa
    if (totalItens <= 0 && menuAtual == MENU_MUSICAS_LISTA) return;

    switch (op) {
    case 0: // PLAY / PAUSE
        estaPausado = !estaPausado;
        sprintf(msgStatus, estaPausado ? "AUDIO PAUSADO" : "REPRODUZINDO...");
        msgTimer = 90;
        break;

    case 1: // PARAR
        pararAudio();
        sprintf(msgStatus, "MUSICA PARADA");
        msgTimer = 90;
        break;

    case 2: // PRÓXIMA FAIXA
        if (totalItens > 0) {
            sel = (sel + 1 >= totalItens) ? 0 : sel + 1;
            off = (sel / 6) * 6; // Ajusta scroll visual
            char caminho[512];
            sprintf(caminho, "/data/HyperNeiva/Musicas/%s", nomes[sel]);
            tocarMusicaNova(caminho);
            sprintf(msgStatus, "PROXIMA: %s", nomes[sel]);
        }
        msgTimer = 90;
        break;

    case 3: // FAIXA ANTERIOR
        if (totalItens > 0) {
            sel = (sel - 1 < 0) ? totalItens - 1 : sel - 1;
            off = (sel / 6) * 6;
            char caminho[512];
            sprintf(caminho, "/data/HyperNeiva/Musicas/%s", nomes[sel]);
            tocarMusicaNova(caminho);
            sprintf(msgStatus, "ANTERIOR: %s", nomes[sel]);
        }
        msgTimer = 90;
        break;

    case 4: // VOLUME +
        volumeMusica += 0.1f;
        if (volumeMusica > 2.0f) volumeMusica = 2.0f;
        sprintf(msgStatus, "VOLUME: %.0f%%", volumeMusica * 100);
        msgTimer = 60;
        break;

    case 5: // VOLUME -
        volumeMusica -= 0.1f;
        if (volumeMusica < 0.0f) volumeMusica = 0.0f;
        sprintf(msgStatus, "VOLUME: %.0f%%", volumeMusica * 100);
        msgTimer = 60;
        break;

    case 6: // ADIANTAR 10s
        adiantarMusica(10);
        sprintf(msgStatus, "AVANCAR +10s");
        msgTimer = 60;
        break;

    case 7: // RETROCEDER 10s
        adiantarMusica(-10);
        sprintf(msgStatus, "RETROCEDER -10s");
        msgTimer = 60;
        break;

    case 8: // REPETIR (Toggle)
        repetirMusica = !repetirMusica;
        sprintf(msgStatus, "REPETIR: %s", repetirMusica ? "LIGADO" : "DESLIGADO");
        msgTimer = 90;
        break;

    case 10: // VOLTAR (Sair do HUD)
        showOpcoes = false;
        break;

    default:
        break;
    }
}