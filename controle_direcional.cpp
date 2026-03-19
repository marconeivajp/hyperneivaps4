// --- INÍCIO DO ARQUIVO controle_direcional.cpp ---
#include <stdint.h>
#include <stdbool.h>
#include <orbis/Pad.h>

#include "controle_direcional.h"
#include "menu.h"

int cd = 0;
extern bool showOpcoes;
extern MenuLevel menuAtual;
extern int selAudioOpcao;
extern int selOpcao;
extern int sel;
extern int off;
extern int totalItens;

// IMPORTA VARIÁVEIS DA IMAGEM
extern bool visualizandoMidiaImagem;
extern float zoomMidia;
extern bool fullscreenMidia;

// IMPORTA VARIÁVEIS DO TEXTO
extern bool visualizandoMidiaTexto;
extern int textoMidiaScroll;
extern int totalLinhasTexto;

void processarNavegacaoDPad(uint32_t botoes) {

    // 1. SE ESTIVER VENDO IMAGEM, SETAS CONTROLAM O ZOOM
    if (visualizandoMidiaImagem) {
        if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
            if (cd <= 0) {
                if (botoes & ORBIS_PAD_BUTTON_UP) {
                    fullscreenMidia = false;
                    zoomMidia += 0.5f;
                }
                else if (botoes & ORBIS_PAD_BUTTON_DOWN) {
                    fullscreenMidia = false;
                    zoomMidia -= 0.5f;
                    if (zoomMidia < 0.1f) zoomMidia = 0.1f;
                }
                cd = 2;
            }
        }
        else {
            cd = 0;
        }
        if (cd > 0) cd--;
        return;
    }

    // 2. SE ESTIVER LENDO TEXTO, SETAS ROLAM A PÁGINA
    if (visualizandoMidiaTexto) {
        if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
            if (cd <= 0) {
                if (botoes & ORBIS_PAD_BUTTON_UP) {
                    textoMidiaScroll -= 3; // Rola 3 linhas para cima
                    if (textoMidiaScroll < 0) textoMidiaScroll = 0;
                }
                else if (botoes & ORBIS_PAD_BUTTON_DOWN) {
                    textoMidiaScroll += 3; // Rola 3 linhas para baixo
                    if (textoMidiaScroll > totalLinhasTexto - 20) textoMidiaScroll = totalLinhasTexto - 20;
                    if (textoMidiaScroll < 0) textoMidiaScroll = 0;
                }
                cd = 2; // Mantém ágil para leitura fluida
            }
        }
        else {
            cd = 0;
        }
        if (cd > 0) cd--;
        return;
    }

    // 3. NAVEGAÇÃO CLÁSSICA DO SEU MENU
    if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
        if (cd <= 0) {
            if (showOpcoes) {
                if (menuAtual == MENU_AUDIO_OPCOES) {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && selAudioOpcao < 10) selAudioOpcao++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && selAudioOpcao > 0) selAudioOpcao--;
                }
                else {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && selOpcao < 9) selOpcao++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && selOpcao > 0) selOpcao--;
                }
            }
            else {
                if (botoes & ORBIS_PAD_BUTTON_DOWN && sel < (totalItens - 1)) { sel++; if (sel >= (off + 6)) off++; }
                else if (botoes & ORBIS_PAD_BUTTON_UP && sel > 0) { sel--; if (sel < off) off--; }
            }
            cd = 10;
        }
    }
    else cd = 0;
    if (cd > 0) cd--;
}
// --- FIM DO ARQUIVO controle_direcional.cpp ---