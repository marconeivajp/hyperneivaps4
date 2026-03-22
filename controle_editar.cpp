#include "controle_editar.h"
#include "menu.h"
#include "editar.h"

extern MenuLevel menuAtual; extern int sel; extern int off; extern int editType; extern int editTarget; extern bool editMode; extern int mapAcoes[15];

// Declarando TODAS as variáveis
extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH;
extern int listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH;
extern int backX, backY, backW, backH, barX, barY, barW, barH;
extern int audioX, audioY, audioW, audioH, upX, upY, upW, upH;
extern int fontTam, msgX, msgY, msgTam, listOri, listBg;
extern int barBg, barFill, listMark, listHoverMark;

extern const int dLXV, dLYV, dListSpcV, dLXH, dLYH, dListSpcH;
extern const int dLW, dLH, dCX, dCY, dCW, dCH, dDX, dDY, dDW, dDH;
extern const int dBarX, dBarY, dBarW, dBarH, dAudioX, dAudioY, dAudioW, dAudioH, dUpX, dUpY, dUpW, dUpH;
extern const int dFontTam, dMsgX, dMsgY, dMsgTam, dListOri, dListBg, dBarBg, dBarFill, dListMark, dListHoverMark;

extern void salvarConfiguracao(); extern void preencherMenuEditTarget(); extern void preencherMenuEditar(); extern void preencherRoot();

void acaoCross_Editar() {
    if (menuAtual == MENU_EDITAR) {
        if (sel == 10) {
            listXV = dLXV; listYV = dLYV; listSpcV = dListSpcV;
            listXH = dLXH; listYH = dLYH; listSpcH = dListSpcH;
            listW = dLW; listH = dLH; capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH;
            discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH; backX = 0; backY = 0; backW = 1920; backH = 1080;
            barX = dBarX; barY = dBarY; barW = dBarW; barH = dBarH; audioX = dAudioX; audioY = dAudioY; audioW = dAudioW; audioH = dAudioH;
            upX = dUpX; upY = dUpY; upW = dUpW; upH = dUpH; fontTam = dFontTam; msgX = dMsgX; msgY = dMsgY; msgTam = dMsgTam;
            listOri = dListOri; listBg = dListBg; barBg = dBarBg; barFill = dBarFill; listMark = dListMark; listHoverMark = dListHoverMark;
            salvarConfiguracao();
        }
        else { editTarget = sel; preencherMenuEditTarget(); sel = 0; off = 0; }
    }
    else if (menuAtual == MENU_EDIT_TARGET) {
        int acaoReal = mapAcoes[sel];
        if (acaoReal == 9) {
            if (editTarget == 0) { listXV = dLXV; listYV = dLYV; listSpcV = dListSpcV; listXH = dLXH; listYH = dLYH; listSpcH = dListSpcH; listW = dLW; listH = dLH; listOri = dListOri; listBg = dListBg; }
            else if (editTarget == 1) { capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH; }
            else if (editTarget == 2) { discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH; }
            else if (editTarget == 3) { backX = 0; backY = 0; backW = 1920; backH = 1080; }
            else if (editTarget == 4) { barX = dBarX; barY = dBarY; barW = dBarW; barH = dBarH; barBg = dBarBg; barFill = dBarFill; }
            else if (editTarget == 5) { audioX = dAudioX; audioY = dAudioY; audioW = dAudioW; audioH = dAudioH; }
            else if (editTarget == 6) { upX = dUpX; upY = dUpY; upW = dUpW; upH = dUpH; }
            else if (editTarget == 7) { fontTam = dFontTam; }
            else if (editTarget == 8) { msgX = dMsgX; msgY = dMsgY; msgTam = dMsgTam; }
            else if (editTarget == 9) { listMark = dListMark; listHoverMark = dListHoverMark; }
            salvarConfiguracao();
        }
        else { editType = acaoReal; editMode = true; }
    }
}
void acaoCircle_Editar() { if (menuAtual == MENU_EDITAR) { preencherRoot(); sel = 0; off = 0; } else if (menuAtual == MENU_EDIT_TARGET) { preencherMenuEditar(); sel = 0; off = 0; } }