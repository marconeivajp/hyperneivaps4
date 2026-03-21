#include "menu_audio.h"
#include "menu.h"
#include "graphics.h"
#include "audio.h"      
#include "explorar.h"   
#include <string.h>
#include <stdio.h>

extern bool showOpcoes;
extern int selAudioOpcao;
extern int listX, listY;
extern int totalItens;

const char* listaOpcoesAudio[11] = {
    "PLAY / PAUSE",
    "PARAR",
    "PROXIMA FAIXA",
    "FAIXA ANTERIOR",
    "VOLUME +", // O texto será alterado dinamicamente no desenho
    "VOLUME -", // O texto será alterado dinamicamente no desenho
    "ADIANTAR 10s",
    "RETROCEDER 10s",
    "REPETIR",
    "---",
    "VOLTAR"
};

void desenharMenuAudio(uint32_t* p) {
    if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
        for (int my = 0; my < 550; my++) {
            for (int mx = 0; mx < 350; mx++) {
                int pxX = listX + 600 + mx;
                int pyY = listY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = 0xEE111111;
                }
            }
        }

        for (int i = 0; i < 11; i++) {
            uint32_t corOp = (i == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;

            // Desenho dinâmico para as opções que mudam de texto
            if (i == 4) {
                char txtVol[64]; sprintf(txtVol, "VOLUME + (%d%%)", volumeGeral);
                desenharTexto(p, txtVol, 30, listX + 620, listY + 50 + (i * 45), corOp);
            }
            else if (i == 5) {
                char txtVol[64]; sprintf(txtVol, "VOLUME - (%d%%)", volumeGeral);
                desenharTexto(p, txtVol, 30, listX + 620, listY + 50 + (i * 45), corOp);
            }
            else if (i == 8) {
                desenharTexto(p, modoRepetir ? "MODO: REPETIR FAIXA" : "MODO: LINEAR (TODAS)", 30, listX + 620, listY + 50 + (i * 45), corOp);
            }
            else {
                desenharTexto(p, listaOpcoesAudio[i], 30, listX + 620, listY + 50 + (i * 45), corOp);
            }
        }
    }
}

void abrirMenuAudioOpcoes() {
    menuAtual = MENU_AUDIO_OPCOES;
    selAudioOpcao = 0;
    showOpcoes = true;
}

void tratarSelecaoAudio(int op) {
    if (totalItens <= 0 && menuAtual == MENU_MUSICAS) return;

    switch (op) {
    case 0: // PLAY / PAUSE
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) {
            comandoPausar = !comandoPausar;
            sprintf(msgStatus, comandoPausar ? "MUSICA PAUSADA" : "REPRODUZINDO");
            msgTimer = 90;
        }
        else {
            sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA");
            msgTimer = 90;
        }
        break;

    case 1: // PARAR
        tocarMusicaNova("PARADO");
        sprintf(msgStatus, "MUSICA PARADA");
        msgTimer = 90;
        break;

    case 2: // PROXIMA FAIXA
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) {
            tocarProximaMusica();
            sprintf(msgStatus, "TOCANDO PROXIMA");
            msgTimer = 90;
        }
        else {
            sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA");
            msgTimer = 90;
        }
        break;

    case 3: // FAIXA ANTERIOR
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) {
            tocarMusicaAnterior();
            sprintf(msgStatus, "TOCANDO ANTERIOR");
            msgTimer = 90;
        }
        else {
            sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA");
            msgTimer = 90;
        }
        break;

    case 4: // VOLUME +
        aumentarVolume();
        sprintf(msgStatus, "VOLUME: %d%%", volumeGeral);
        msgTimer = 60;
        break;

    case 5: // VOLUME -
        diminuirVolume();
        sprintf(msgStatus, "VOLUME: %d%%", volumeGeral);
        msgTimer = 60;
        break;

    case 8: // REPETIR / LINEAR
        modoRepetir = !modoRepetir;
        sprintf(msgStatus, modoRepetir ? "REPETICAO ATIVADA" : "MODO LINEAR ATIVADO");
        msgTimer = 90;
        break;

    case 10: // VOLTAR
        showOpcoes = false;
        break;

    default:
        sprintf(msgStatus, "FUNCAO EM DESENVOLVIMENTO");
        msgTimer = 60;
        break;
    }
}