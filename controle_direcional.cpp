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

void processarNavegacaoDPad(uint32_t botoes) {
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