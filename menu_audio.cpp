#include "menu_audio.h"
#include "menu.h"
#include "graphics.h"

// Puxamos as variáveis externas necessárias para desenhar o menu de áudio
extern bool showOpcoes;
extern int selAudioOpcao;
extern const char* listaOpcoesAudio[11];
extern int listX, listY;

void desenharMenuAudio(uint32_t* p) {
    if (menuAtual == MENU_AUDIO_OPCOES && showOpcoes) {
        // Fundo semi-transparente do menu suspenso
        for (int my = 0; my < 550; my++) {
            for (int mx = 0; mx < 350; mx++) {
                int pxX = listX + 600 + mx;
                int pyY = listY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = 0xEE111111;
                }
            }
        }

        // Desenha as opções da lista de Áudio
        for (int i = 0; i < 11; i++) {
            uint32_t corOp = (i == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF; // Amarelo se selecionado
            desenharTexto(p, listaOpcoesAudio[i], 30, listX + 620, listY + 50 + (i * 45), corOp);
        }
    }
}