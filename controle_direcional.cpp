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

// VARIÁVEIS DO PAINEL DUPLO
extern bool painelDuplo;
extern int painelAtivo;
extern int selEsq;
extern int totalItensEsq;
int offEsq = 0; // Nova: Rolagem de tela independente pro painel esquerdo

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
                    textoMidiaScroll -= 2;
                    if (textoMidiaScroll < 0) textoMidiaScroll = 0;
                }
                else if (botoes & ORBIS_PAD_BUTTON_DOWN) {
                    textoMidiaScroll += 2;
                    if (textoMidiaScroll > totalLinhasTexto - 15) textoMidiaScroll = totalLinhasTexto - 15;
                    if (textoMidiaScroll < 0) textoMidiaScroll = 0;
                }
                cd = 4;
            }
        }
        else {
            cd = 0;
        }
        if (cd > 0) cd--;
        return;
    }

    // NOVO: ALTERNAR ENTRE OS PAINÉIS (ESQUERDA / DIREITA)
    if (painelDuplo && !showOpcoes) {
        if (botoes & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) {
            if (cd <= 0) {
                painelAtivo = (painelAtivo == 0) ? 1 : 0;
                cd = 10;
            }
        }
    }

    // 3. NAVEGAÇÃO CLÁSSICA DO MENU
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
                // Identifica se estamos operando a lista da Esquerda ou da Direita
                bool ehEsq = (painelDuplo && painelAtivo == 0);
                int* sAtual = ehEsq ? &selEsq : &sel;
                int* oAtual = ehEsq ? &offEsq : &off;
                int tItens = ehEsq ? totalItensEsq : totalItens;

                if (botoes & ORBIS_PAD_BUTTON_DOWN && *sAtual < (tItens - 1)) {
                    (*sAtual)++;
                    if (*sAtual >= (*oAtual + 6)) (*oAtual)++;
                }
                else if (botoes & ORBIS_PAD_BUTTON_UP && *sAtual > 0) {
                    (*sAtual)--;
                    if (*sAtual < *oAtual) (*oAtual)--;
                }
            }
            cd = 10;
        }
    }
    // Zera o tempo de espera apenas se não apertar Cima, Baixo, Esquerda ou Direita
    else if (!(botoes & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) {
        cd = 0;
    }

    if (cd > 0) cd--;
}
// --- FIM DO ARQUIVO controle_direcional.cpp ---