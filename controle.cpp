#include "globals.h"
#include <orbis/Pad.h>

void processarControles(int pad) {
    OrbisPadData pData;
    scePadReadState(pad, &pData);

    if (!(pData.buttons & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_R1))) {
        cd = 0;
    }
    if (cd > 0) cd--;

    if (editMode) {
        // ... (Lógica de edição permanece igual) ...
        int* tX, * tY, * tW, * tH;
        if (editTarget == 0) { tX = &listX;  tY = &listY;  tW = &listW;  tH = &listH; }
        else if (editTarget == 1) { tX = &capaX;  tY = &capaY;  tW = &capaW;  tH = &capaH; }
        else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
        else { tX = &backX;  tY = &backY;  tW = &backW;  tH = &backH; }

        if (pData.buttons & ORBIS_PAD_BUTTON_UP) { if (editType == 1) { (*tH)--; (*tW)--; } else if (editType == 2) (*tH)--; else (*tY)--; }
        if (pData.buttons & ORBIS_PAD_BUTTON_DOWN) { if (editType == 1) { (*tH)++; (*tW)++; } else if (editType == 2) (*tH)++; else (*tY)++; }
        if (pData.buttons & ORBIS_PAD_BUTTON_LEFT) { if (editType == 2) (*tW)--; else if (editType == 0) (*tX)--; }
        if (pData.buttons & ORBIS_PAD_BUTTON_RIGHT) { if (editType == 2) (*tW)++; else if (editType == 0) (*tX)++; }

        if ((pData.buttons & (ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_CIRCLE)) && !pCross) {
            editMode = false; salvarConfiguracao(); preencherMenuEditar(); pCross = true;
        }
        else if (!(pData.buttons & ORBIS_PAD_BUTTON_CROSS)) pCross = false;
    }
    else {
        // --- NAVEGAÇÃO COM LOOP (CIRCULAR) ---
        if (pData.buttons & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
            if (cd <= 0) {
                if (showOpcoes) {
                    if (pData.buttons & ORBIS_PAD_BUTTON_DOWN) {
                        selOpcao++; if (selOpcao > 9) selOpcao = 0; // Loop para o início
                    }
                    else if (pData.buttons & ORBIS_PAD_BUTTON_UP) {
                        selOpcao--; if (selOpcao < 0) selOpcao = 9; // Loop para o fim
                    }
                }
                else {
                    if (pData.buttons & ORBIS_PAD_BUTTON_DOWN) {
                        if (sel < (totalItens - 1)) {
                            sel++; if (sel >= (off + 6)) off++;
                        }
                        else {
                            sel = 0; off = 0; // Loop para o topo
                        }
                    }
                    else if (pData.buttons & ORBIS_PAD_BUTTON_UP) {
                        if (sel > 0) {
                            sel--; if (sel < off) off--;
                        }
                        else {
                            sel = totalItens - 1;
                            off = totalItens - 6; if (off < 0) off = 0; // Loop para o fundo
                        }
                    }
                }
                cd = 8;
            }
        }

        if (menuAtual == MENU_EXPLORAR) {
            if ((pData.buttons & ORBIS_PAD_BUTTON_R1) && cd <= 0) { marcados[sel] = !marcados[sel]; cd = 12; }
            if ((pData.buttons & ORBIS_PAD_BUTTON_TRIANGLE) && !pTri) { showOpcoes = !showOpcoes; selOpcao = 0; pTri = true; }
            else if (!(pData.buttons & ORBIS_PAD_BUTTON_TRIANGLE)) pTri = false;
        }

        if ((pData.buttons & ORBIS_PAD_BUTTON_CROSS) && !pCross) {
            if (showOpcoes) acaoArquivo(selOpcao); else executarAcaoX();
            pCross = true;
        }
        else if (!(pData.buttons & ORBIS_PAD_BUTTON_CROSS)) pCross = false;

        if ((pData.buttons & ORBIS_PAD_BUTTON_CIRCLE) && !pCircle) {
            if (showOpcoes) showOpcoes = false; else executarAcaoBolinha();
            pCircle = true;
        }
        else if (!(pData.buttons & ORBIS_PAD_BUTTON_CIRCLE)) pCircle = false;
    }
}