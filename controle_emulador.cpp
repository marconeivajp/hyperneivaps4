#include "controle_emulador.h"
#include "graphics.h"
#include "libretro_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <math.h>

#define MIN_ABS(x) (((x) < 0) ? -(x) : (x))

int selMapeamento = 0;
bool esperandoBotao = false;

extern int menuAtual;
extern char msgStatus[128];
extern int msgTimer;
extern uint32_t* backupEmuFrame;
extern int gba_to_ps4_map[16][2]; // Mapeamento global

const char* getNomeBotaoPS4(int btn) {
    if (btn == 0) return "---";
    if (btn == PS4_ANALOG_L_UP)    return "Analogo D-Pad Cima";
    if (btn == PS4_ANALOG_L_DOWN)  return "Analogo D-Pad Baixo";
    if (btn == PS4_ANALOG_L_LEFT)  return "Analogo D-Pad Esq";
    if (btn == PS4_ANALOG_L_RIGHT) return "Analogo D-Pad Dir";
    if (btn == PS4_ANALOG_R_UP)    return "Direcional R Cima";
    if (btn == PS4_ANALOG_R_DOWN)  return "Direcional R Baixo";
    if (btn == PS4_ANALOG_R_LEFT)  return "Direcional R Esq";
    if (btn == PS4_ANALOG_R_RIGHT) return "Direcional R Dir";
    
    if (btn & ORBIS_PAD_BUTTON_CROSS) return "X (CROSS)";
    if (btn & ORBIS_PAD_BUTTON_CIRCLE) return "O (CIRCLE)";
    if (btn & ORBIS_PAD_BUTTON_SQUARE) return "[] (SQUARE)";
    if (btn & ORBIS_PAD_BUTTON_TRIANGLE) return "/\\ (TRIANGLE)";
    if (btn & ORBIS_PAD_BUTTON_L1) return "L1";
    if (btn & ORBIS_PAD_BUTTON_R1) return "R1";
    if (btn & ORBIS_PAD_BUTTON_L2) return "L2";
    if (btn & ORBIS_PAD_BUTTON_R2) return "R2";
    if (btn & ORBIS_PAD_BUTTON_L3) return "L3 Click";
    if (btn & ORBIS_PAD_BUTTON_R3) return "R3 Click";
    if (btn & ORBIS_PAD_BUTTON_UP) return "Seta CIMA";
    if (btn & ORBIS_PAD_BUTTON_DOWN) return "Seta BAIXO";
    if (btn & ORBIS_PAD_BUTTON_LEFT) return "Seta ESQUERDA";
    if (btn & ORBIS_PAD_BUTTON_RIGHT) return "Seta DIREITA";
    if (btn & ORBIS_PAD_BUTTON_OPTIONS) return "OPTIONS";
    if (btn & ORBIS_PAD_BUTTON_TOUCH_PAD) return "TOUCHPAD";
    return "DESCONHECIDO";
}

const char* gba_button_names_modular[] = {
    "A (BOTAO)", "B (BOTAO)", "SELECT", "START", "CIMA", "BAIXO", "ESQUERDA", "DIREITA",
    "L (OMBRO)", "R (OMBRO)", "L2", "R2", "L3", "R3", "NAO USA", "NAO USA"
};

void atualizarMapeamentoControles(const OrbisPadData* pData, uint32_t ultimos) {
    if (menuAtual != 50) return;

    // Tempo atual em microsegundos
    uint64_t now = sceKernelGetProcessTime();

    // TRAVA DE SEGURANÇA (Flush de 500ms) ao entrar/trocar
    static uint64_t entradaControleTime = 0;
    if (entradaControleTime == 0) entradaControleTime = now;
    if (now - entradaControleTime < 500000) return;

    if (esperandoBotao) {
        static bool aguardandoSoltarMapping = true;
        // Espera soltar tudo antes de aceitar nova entrada
        if (pData->buttons == 0 && MIN_ABS(pData->leftStick.x - 128) < 30 && MIN_ABS(pData->leftStick.y - 128) < 30) {
            aguardandoSoltarMapping = false;
        }
        
        if (!aguardandoSoltarMapping) {
            int capturado = 0;
            if (pData->buttons != 0) capturado = pData->buttons;
            else if (pData->leftStick.y < 50)  capturado = PS4_ANALOG_L_UP;
            else if (pData->leftStick.y > 200) capturado = PS4_ANALOG_L_DOWN;
            else if (pData->leftStick.x < 50)  capturado = PS4_ANALOG_L_LEFT;
            else if (pData->leftStick.x > 200) capturado = PS4_ANALOG_L_RIGHT;
            else if (pData->rightStick.y < 50) capturado = PS4_ANALOG_R_UP;
            else if (pData->rightStick.y > 200)capturado = PS4_ANALOG_R_DOWN;
            else if (pData->rightStick.x < 50) capturado = PS4_ANALOG_R_LEFT;
            else if (pData->rightStick.x > 200)capturado = PS4_ANALOG_R_RIGHT;

            if (capturado != 0) {
                if (gba_to_ps4_map[selMapeamento][0] == 0) {
                    gba_to_ps4_map[selMapeamento][0] = capturado;
                } else if (gba_to_ps4_map[selMapeamento][1] == 0 && gba_to_ps4_map[selMapeamento][0] != capturado) {
                    gba_to_ps4_map[selMapeamento][1] = capturado;
                } else {
                    gba_to_ps4_map[selMapeamento][0] = capturado;
                    gba_to_ps4_map[selMapeamento][1] = 0;
                }
                
                snprintf(msgStatus, 128, "BOTAO CONFIGURADO!");
                msgTimer = 60;
                esperandoBotao = false;
                aguardandoSoltarMapping = true;
                entradaControleTime = now; // Reinicia trava após mapear
            }
        }
    } else {
        // NAVEGAÇÃO ESTREITA (SÓ MOVE SE SOLTAR E APERTAR DE NOVO)
        if ((pData->buttons & ORBIS_PAD_BUTTON_UP) && !(ultimos & ORBIS_PAD_BUTTON_UP)) {
            selMapeamento--; if (selMapeamento < 0) selMapeamento = 15;
        }
        else if ((pData->buttons & ORBIS_PAD_BUTTON_DOWN) && !(ultimos & ORBIS_PAD_BUTTON_DOWN)) {
            selMapeamento++; if (selMapeamento > 15) selMapeamento = 0;
        }
        else if ((pData->buttons & ORBIS_PAD_BUTTON_CROSS) && !(ultimos & ORBIS_PAD_BUTTON_CROSS)) {
            esperandoBotao = true;
        }
        else if ((pData->buttons & ORBIS_PAD_BUTTON_CIRCLE) && !(ultimos & ORBIS_PAD_BUTTON_CIRCLE)) {
            extern bool menuEmuladorAtivo;
            menuAtual = 51; // Volta para o Menu Pausa 
            menuEmuladorAtivo = true; // Força ligar o menu pausa de volta
            entradaControleTime = 0;
        }
    }
}

void desenharAjudaControles(uint32_t* pixels) {
    if (menuAtual != 50) return;

    // Escurece o fundo
    if (backupEmuFrame) {
        for (int i = 0; i < 1920 * 1080; i++) {
            uint32_t bg = backupEmuFrame[i];
            uint8_t r = (((bg >> 16) & 0xFF) * 1) / 10; 
            uint8_t g = (((bg >> 8) & 0xFF) * 1) / 10;
            uint8_t b = ((bg & 0xFF) * 1) / 10;
            pixels[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    int boxW = 800;
    int boxH = 850;
    int boxX = (1920 - boxW) / 2;
    int boxY = (1080 - boxH) / 2;

    for (int y = boxY; y < boxY + boxH; y++) {
        for (int x = boxX; x < boxX + boxW; x++) {
            pixels[y * 1920 + x] = 0xF0151515;
        }
    }

    desenharTexto(pixels, "CONFIGURACAO DE CONTROLES (PS4 -> EMULADOR)", 35, boxX + 60, boxY + 50, 0xFF00FF00);
    desenharTexto(pixels, "Pressione X no item para mudar o botao", 25, boxX + 60, boxY + 90, 0xFFBBBBBB);

    for (int i = 0; i < 16; i++) {
        uint32_t cor = (selMapeamento == i) ? 0xFF00FF00 : 0xFFFFFFFF;
        if (esperandoBotao && selMapeamento == i) cor = 0xFFFFFF00;

        char buffer[256];
        const char* p1 = getNomeBotaoPS4(gba_to_ps4_map[i][0]);
        const char* p2 = getNomeBotaoPS4(gba_to_ps4_map[i][1]);
        
        if (gba_to_ps4_map[i][1] != 0) {
            snprintf(buffer, 256, "%-20s -> %s / %s", gba_button_names_modular[i], p1, p2);
        } else {
            snprintf(buffer, 256, "%-20s -> %s", gba_button_names_modular[i], p1);
        }
        
        desenharTexto(pixels, buffer, 30, boxX + 60, boxY + 150 + (i * 38), cor);
        if (selMapeamento == i) desenharTexto(pixels, ">", 30, boxX + 30, boxY + 150 + (i * 38), 0xFF00FF00);
    }

    desenharTexto(pixels, "(X) EDITAR | (CIRCULO) VOLTAR", 25, boxX + 220, boxY + boxH - 40, 0xFFAAAAAA);
}