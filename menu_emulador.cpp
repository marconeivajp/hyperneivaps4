#include "menu_emulador.h"
#include "graphics.h"
#include "libretro_bridge.h"
#include "jogar.h"
#include <stdio.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>

int menuEmuSelecao = 0;
bool menuEmuladorAtivo = false;

extern void ligarInterfaceGrafica();
extern void* backImg;
extern int menuAtual;
extern bool emuladorAtivo;
extern char msgStatus[128];
extern int msgTimer;
extern uint32_t* backupEmuFrame;

void atualizarMenuEmulador(uint32_t buttons, uint32_t ultimos) {
    if (!menuEmuladorAtivo) return;

    // Tempo atual em microsegundos
    uint64_t now = sceKernelGetProcessTime();

    // TRAVA DE SEGURANÇA (Flush de 150ms) ao entrar/trocar
    static uint64_t entradaMenuTime = 0;
    if (entradaMenuTime == 0) entradaMenuTime = now;
    if (now - entradaMenuTime < 150000) return;

    bridge_set_pausado(true);
    
    // NAVEGAÇÃO ESTREITA (SÓ MOVE SE SOLTAR E APERTAR DE NOVO)
    if ((buttons & ORBIS_PAD_BUTTON_UP) && !(ultimos & ORBIS_PAD_BUTTON_UP)) {
        menuEmuSelecao--;
        if (menuEmuSelecao < 0) menuEmuSelecao = 4;
    }
    else if ((buttons & ORBIS_PAD_BUTTON_DOWN) && !(ultimos & ORBIS_PAD_BUTTON_DOWN)) {
        menuEmuSelecao++;
        if (menuEmuSelecao > 4) menuEmuSelecao = 0;
    }

    if ((buttons & ORBIS_PAD_BUTTON_CROSS) && !(ultimos & ORBIS_PAD_BUTTON_CROSS)) {
        if (menuEmuSelecao == 0) { // Continuar
            menuEmuladorAtivo = false;
            bridge_set_pausado(false);
            entradaMenuTime = 0;
        }
        else if (menuEmuSelecao == 1) { // Salvar
            bridge_salvar_state("/data/retroarch/savestates/gba_save.state");
            snprintf(msgStatus, 128, "STATE SALVO!");
            msgTimer = 60;
        }
        else if (menuEmuSelecao == 2) { // Carregar
            if (bridge_carregar_state("/data/retroarch/savestates/gba_save.state")) {
                snprintf(msgStatus, 128, "STATE CARREGADO!");
                menuEmuladorAtivo = false;
                bridge_set_pausado(false);
                entradaMenuTime = 0;
            } else {
                snprintf(msgStatus, 128, "ERRO AO CARREGAR!");
            }
            msgTimer = 60;
        }
        else if (menuEmuSelecao == 3) { // Controles
            menuAtual = 50; 
            menuEmuladorAtivo = false; // Desliga menu pausa para o de controles assumir
            entradaMenuTime = 0;
        }
        else if (menuEmuSelecao == 4) { // Sair
            fecharEmulador();
            menuAtual = 1; 
            menuEmuladorAtivo = false;
            emuladorAtivo = false; 
            bridge_set_pausado(false);
            
            // FORÇA RESTAURAÇÃO DO BACKGROUND
            backImg = NULL; // Limpa ponteiro para forçar reload
            ligarInterfaceGrafica();
            
            entradaMenuTime = 0;
        }
    }

    if ((buttons & ORBIS_PAD_BUTTON_OPTIONS) && !(ultimos & ORBIS_PAD_BUTTON_OPTIONS)) {
        menuEmuladorAtivo = false;
        bridge_set_pausado(false);
        entradaMenuTime = 0;
    }
}

void desenharMenuEmulador(uint32_t* pixels) {
    if (!menuEmuladorAtivo) return;

    // (O resto do seu código de desenho continua igual, não precisa alterar)
    if (backupEmuFrame) {
        for (int i = 0; i < 1920 * 1080; i++) {
            uint32_t bg = backupEmuFrame[i];
            uint8_t r = (((bg >> 16) & 0xFF) * 2) / 10; 
            uint8_t g = (((bg >> 8) & 0xFF) * 2) / 10;
            uint8_t b = ((bg & 0xFF) * 2) / 10;
            pixels[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    int boxW = 400;
    int boxH = 450;
    int boxX = (1920 - boxW) / 2;
    int boxY = (1080 - boxH) / 2;

    for (int y = boxY; y < boxY + boxH; y++) {
        for (int x = boxX; x < boxX + boxW; x++) {
            pixels[y * 1920 + x] = (pixels[y * 1920 + x] & 0x00FFFFFF) | 0xDD000000;
        }
    }

    desenharTexto(pixels, "MENU PAUSA", 40, boxX + 80, boxY + 50, 0xFF00FF00);

    const char* itens[] = { "CONTINUAR", "SALVAR STATE", "CARREGAR STATE", "CONTROLES", "SAIR DO JOGO" };
    for (int i = 0; i < 5; i++) {
        uint32_t cor = (menuEmuSelecao == i) ? 0xFF00FF00 : 0xFFFFFFFF;
        desenharTexto(pixels, itens[i], 30, boxX + 60, boxY + 150 + (i * 50), cor);
        if (menuEmuSelecao == i) desenharTexto(pixels, ">", 30, boxX + 30, boxY + 150 + (i * 50), 0xFF00FF00);
    }
}