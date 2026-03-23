#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

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

extern int audioX, audioY, audioW, audioH;
extern int fontTam, listBg;

uint32_t getSysColor(int index);
extern void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor);

int offAudioOpcao = 0;

const char* listaOpcoesAudio[11] = {
    "PLAY / PAUSE", "PARAR", "PROXIMA FAIXA", "FAIXA ANTERIOR",
    "VOLUME +", "VOLUME -", "ADIANTAR 10s", "RETROCEDER 10s",
    "MODO REPRODUCAO", "---", "VOLTAR"
};

void desenharMenuAudio(uint32_t* p) {
    if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
        for (int my = 0; my < audioH; my++) {
            for (int mx = 0; mx < audioW; mx++) {
                int pxX = audioX + mx;
                int pyY = audioY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = getSysColor(listBg);
                }
            }
        }

        int maxVisible = (audioH - 50) / 45;
        if (maxVisible < 1) maxVisible = 1;

        for (int i = 0; i < maxVisible; i++) {
            int gIdx = i + offAudioOpcao;
            if (gIdx >= 11) break;

            uint32_t corOp = (gIdx == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;

            if (gIdx == 4) {
                char txtVol[64]; sprintf(txtVol, "VOLUME + (%d%%)", volumeGeral);
                desenharTextoAlinhado(p, txtVol, fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
            else if (gIdx == 5) {
                char txtVol[64]; sprintf(txtVol, "VOLUME - (%d%%)", volumeGeral);
                desenharTextoAlinhado(p, txtVol, fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
            else if (gIdx == 8) {
                // DESENHO INTELIGENTE DOS 5 MODOS
                const char* textosModo[] = {
                    "MODO: LINEAR",
                    "MODO: REPETIR FAIXA",
                    "MODO: REPETIR PASTA",
                    "MODO: ALEATORIO PASTA",
                    "MODO: ALEATORIO TODAS"
                };
                desenharTextoAlinhado(p, textosModo[modoReproducao], fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
            else {
                desenharTextoAlinhado(p, listaOpcoesAudio[gIdx], fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
        }
    }
}

void abrirMenuAudioOpcoes() {
    menuAtual = MENU_AUDIO_OPCOES;
    selAudioOpcao = 0;
    offAudioOpcao = 0;
    showOpcoes = true;
}

void tratarSelecaoAudio(int op) {
    if (totalItens <= 0 && menuAtual == MENU_MUSICAS) return;

    switch (op) {
    case 0:
        if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { comandoPausar = !comandoPausar; sprintf(msgStatus, comandoPausar ? "MUSICA PAUSADA" : "REPRODUZINDO"); msgTimer = 90; }
        else { sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA"); msgTimer = 90; } break;
    case 1: tocarMusicaNova("PARADO"); sprintf(msgStatus, "MUSICA PARADA"); msgTimer = 90; break;
    case 2: if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { tocarProximaMusica(); sprintf(msgStatus, "TOCANDO PROXIMA"); msgTimer = 90; }
          else { sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA"); msgTimer = 90; } break;
    case 3: if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { tocarMusicaAnterior(); sprintf(msgStatus, "TOCANDO ANTERIOR"); msgTimer = 90; }
          else { sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA"); msgTimer = 90; } break;
    case 4: aumentarVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; break;
    case 5: diminuirVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; break;
    case 6: if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { adiantarAudio(); sprintf(msgStatus, "AVANCANDO 10s"); msgTimer = 60; } break;
    case 7: if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { retrocederAudio(); sprintf(msgStatus, "RETROCEDENDO 10s"); msgTimer = 60; } break;
    case 8: { // <--- AS CHAVES MÁGICAS PARA O C++ NÃO RECLAMAR ESTÃO AQUI!
        modoReproducao++;
        if (modoReproducao > 4) modoReproducao = 0;

        const char* msgModos[] = { "LINEAR", "REPETIR FAIXA", "REPETIR PASTA", "ALEATORIO PASTA", "ALEATORIO TODAS" };
        sprintf(msgStatus, "MODO: %s", msgModos[modoReproducao]);
        msgTimer = 90;
        salvarConfiguracaoAudio(); // Salva no HD na mesma hora!
        break;
    }
    case 10: showOpcoes = false; break;
    default: sprintf(msgStatus, "FUNCAO EM DESENVOLVIMENTO"); msgTimer = 60; break;
    }
}