#include "menu_audio.h"
#include "menu.h"
#include "graphics.h"
#include "audio.h"      
#include "explorar.h"   
#include <string.h>
#include <stdio.h>

extern bool showOpcoes;
extern int selAudioOpcao;
extern int totalItens;

// IMPORTANDO AS VARIÁVEIS DO MENU AUDIO
extern int audioX, audioY, audioW, audioH;

const char* listaOpcoesAudio[11] = {
    "PLAY / PAUSE",
    "PARAR",
    "PROXIMA FAIXA",
    "FAIXA ANTERIOR",
    "VOLUME +",
    "VOLUME -",
    "ADIANTAR 10s",
    "RETROCEDER 10s",
    "REPETIR",
    "---",
    "VOLTAR"
};

void desenharMenuAudio(uint32_t* p) {
    if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
        for (int my = 0; my < audioH; my++) {
            for (int mx = 0; mx < audioW; mx++) {
                int pxX = audioX + mx;
                int pyY = audioY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = 0xEE111111;
                }
            }
        }

        for (int i = 0; i < 11; i++) {
            uint32_t corOp = (i == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;

            if (i == 4) {
                char txtVol[64]; sprintf(txtVol, "VOLUME + (%d%%)", volumeGeral);
                desenharTexto(p, txtVol, 30, audioX + 20, audioY + 50 + (i * 45), corOp);
            }
            else if (i == 5) {
                char txtVol[64]; sprintf(txtVol, "VOLUME - (%d%%)", volumeGeral);
                desenharTexto(p, txtVol, 30, audioX + 20, audioY + 50 + (i * 45), corOp);
            }
            else if (i == 8) {
                desenharTexto(p, modoRepetir ? "MODO: REPETIR FAIXA" : "MODO: LINEAR (TODAS)", 30, audioX + 20, audioY + 50 + (i * 45), corOp);
            }
            else {
                desenharTexto(p, listaOpcoesAudio[i], 30, audioX + 20, audioY + 50 + (i * 45), corOp);
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
    case 0:
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) {
            comandoPausar = !comandoPausar;
            sprintf(msgStatus, comandoPausar ? "MUSICA PAUSADA" : "REPRODUZINDO");
            msgTimer = 90;
        }
        else { sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA"); msgTimer = 90; }
        break;

    case 1:
        tocarMusicaNova("PARADO");
        sprintf(msgStatus, "MUSICA PARADA");
        msgTimer = 90;
        break;

    case 2:
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) {
            tocarProximaMusica();
            sprintf(msgStatus, "TOCANDO PROXIMA");
            msgTimer = 90;
        }
        else { sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA"); msgTimer = 90; }
        break;

    case 3:
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) {
            tocarMusicaAnterior();
            sprintf(msgStatus, "TOCANDO ANTERIOR");
            msgTimer = 90;
        }
        else { sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA"); msgTimer = 90; }
        break;

    case 4: aumentarVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; break;
    case 5: diminuirVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; break;
    case 6:
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { adiantarAudio(); sprintf(msgStatus, "AVANCANDO 10s"); msgTimer = 60; } break;
    case 7:
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { retrocederAudio(); sprintf(msgStatus, "RETROCEDENDO 10s"); msgTimer = 60; } break;

    case 8: modoRepetir = !modoRepetir; sprintf(msgStatus, modoRepetir ? "REPETICAO ATIVADA" : "MODO LINEAR ATIVADO"); msgTimer = 90; break;
    case 10: showOpcoes = false; break;
    default: sprintf(msgStatus, "FUNCAO EM DESENVOLVIMENTO"); msgTimer = 60; break;
    }
}