// --- INÍCIO DO ARQUIVO controle_editar.cpp ---
#include "controle_editar.h"
#include "menu.h"
#include "editar.h"

extern MenuLevel menuAtual;
extern int sel;
extern int editType;
extern int editTarget;
extern bool editMode;
extern int listX, listY, listW, listH;
extern int capaX, capaY, capaW, capaH;
extern int discoX, discoY, discoW, discoH;

// CORREÇÃO: Adicionado o "const" que o compilador exigiu
extern const int dLX, dLY, dLW, dLH;
extern const int dCX, dCY, dCW, dCH;
extern const int dDX, dDY, dDW, dDH;

extern void salvarConfiguracao();
extern void preencherMenuEditTarget();
extern void preencherMenuEditar();
extern void preencherRoot();

void acaoCross_Editar() {
    if (menuAtual == MENU_EDITAR) {
        if (sel == 3) {
            listX = dLX; listY = dLY; listW = dLW; listH = dLH;
            capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH;
            discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH;
            salvarConfiguracao();
        }
        else { editType = sel; preencherMenuEditTarget(); }
    }
    else if (menuAtual == MENU_EDIT_TARGET) {
        if (sel == 4) {
            if (editType == 0) { listX = dLX; listY = dLY; }
            else if (editType == 1) { listW = dLW; listH = dLH; }
            salvarConfiguracao();
            preencherMenuEditar();
        }
        else { editTarget = sel; editMode = true; }
    }
}

void acaoCircle_Editar() {
    if (menuAtual == MENU_EDITAR) preencherRoot();
    else if (menuAtual == MENU_EDIT_TARGET) preencherMenuEditar();
}
// --- FIM DO ARQUIVO controle_editar.cpp ---