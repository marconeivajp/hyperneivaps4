// --- INÍCIO DO ARQUIVO controle_editar.cpp ---
#include "controle_editar.h"
#include "menu.h"
#include "editar.h"

extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern int editType;
extern int editTarget;
extern bool editMode;

extern int listX, listY, listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;
extern int backX, backY, backW, backH;
extern int barX, barY, barW, barH;

extern const int dLX, dLY, dLW, dLH;
extern const int dCX, dCY, dCW, dCH;
extern const int dDX, dDY, dDW, dDH;
extern const int dBarX, dBarY, dBarW, dBarH;

extern void salvarConfiguracao();
extern void preencherMenuEditTarget();
extern void preencherMenuEditar();
extern void preencherRoot();

void acaoCross_Editar() {
    // 1º MENU: MENU_EDITAR (Escolhendo Lista, Capa, Disco...)
    if (menuAtual == MENU_EDITAR) {
        if (sel == 5) { // Botão RESETAR TUDO
            listX = dLX; listY = dLY; listW = dLW; listH = dLH;
            capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH;
            discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH;
            backX = 0; backY = 0; backW = 1920; backH = 1080;
            barX = dBarX; barY = dBarY; barW = dBarW; barH = dBarH;
            salvarConfiguracao();
        }
        else {
            editTarget = sel; // Salva o Alvo que você clicou
            preencherMenuEditTarget(); // Abre as ações (Tamanho, Pos...)
            sel = 0;
            off = 0;
        }
    }
    // 2º MENU: MENU_EDIT_TARGET (Escolhendo Posição, Tamanho, Esticar...)
    else if (menuAtual == MENU_EDIT_TARGET) {
        if (sel == 3) { // Botão RESETAR ESTE ITEM
            if (editTarget == 0) { listX = dLX; listY = dLY; listW = dLW; listH = dLH; }
            else if (editTarget == 1) { capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH; }
            else if (editTarget == 2) { discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH; }
            else if (editTarget == 3) { backX = 0; backY = 0; backW = 1920; backH = 1080; }
            else if (editTarget == 4) { barX = dBarX; barY = dBarY; barW = dBarW; barH = dBarH; }

            salvarConfiguracao();
            preencherMenuEditar(); // Volta pro menu de escolher os alvos
            sel = 0;
            off = 0;
        }
        else {
            editType = sel; // Salva a Ação que você quer
            editMode = true; // Libera o D-PAD para se mover livremente!
        }
    }
}

void acaoCircle_Editar() {
    if (menuAtual == MENU_EDITAR) { preencherRoot(); sel = 0; off = 0; }
    else if (menuAtual == MENU_EDIT_TARGET) { preencherMenuEditar(); sel = 0; off = 0; }
}
// --- FIM DO ARQUIVO controle_editar.cpp ---