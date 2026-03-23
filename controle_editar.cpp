#include "controle_editar.h"
#include "menu.h"
#include "editar.h"

extern MenuLevel menuAtual; extern int sel; extern int off; extern int editType; extern int editTarget; extern bool editMode;
extern int mapAcoes[30];

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH, barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH, fontTam, msgX, msgY, msgTam, listOri, listBg, barBg, barFill, listMark, listHoverMark, fontAlign, fontScroll;
extern int elem1X, elem1Y, elem1W, elem1H, elem1On, ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On, pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;
extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;

extern const int dLXV, dLYV, dListSpcV, dLXH, dLYH, dListSpcH, dLW, dLH, dCX, dCY, dCW, dCH, dDX, dDY, dDW, dDH, dBarX, dBarY, dBarW, dBarH, dAudioX, dAudioY, dAudioW, dAudioH, dUpX, dUpY, dUpW, dUpH, dFontTam, dMsgX, dMsgY, dMsgTam, dListOri, dListBg, dBarBg, dBarFill, dListMark, dListHoverMark, dFontAlign, dFontScroll;
extern const int dElem1X, dElem1Y, dElem1W, dElem1H, dElem1On, dCtrl1X, dCtrl1Y, dCtrl1W, dCtrl1H, dCtrl1On, dPont1X, dPont1Y, dPont1W, dPont1H, dPont1On, dPont1Modo, dPont1Lado;
extern const int dSfxLigado, dSfxVolume;
extern const int dUpBg, dUpTextNorm, dUpTextSel;

extern void salvarConfiguracao(); extern void preencherMenuEditTarget(); extern void preencherMenuEditar(); extern void preencherRoot();

void acaoCross_Editar() {
    if (menuAtual == MENU_EDITAR) {
        if (sel == 14) {
            listXV = dLXV; listYV = dLYV; listSpcV = dListSpcV; listXH = dLXH; listYH = dLYH; listSpcH = dListSpcH; listW = dLW; listH = dLH; capaX = dCX; capaY = dCY; capaW = dCW; capaH = dCH; discoX = dDX; discoY = dDY; discoW = dDW; discoH = dDH; backX = 0; backY = 0; backW = 1920; backH = 1080; barX = dBarX; barY = dBarY; barW = dBarW; barH = dBarH; audioX = dAudioX; audioY = dAudioY; audioW = dAudioW; audioH = dAudioH; upX = dUpX; upY = dUpY; upW = dUpW; upH = dUpH; fontTam = dFontTam; msgX = dMsgX; msgY = dMsgY; msgTam = dMsgTam; listOri = dListOri; listBg = dListBg; barBg = dBarBg; barFill = dBarFill; listMark = dListMark; listHoverMark = dListHoverMark; fontAlign = dFontAlign; fontScroll = dFontScroll;
            elem1X = dElem1X; elem1Y = dElem1Y; elem1W = dElem1W; elem1H = dElem1H; elem1On = dElem1On; ctrl1X = dCtrl1X; ctrl1Y = dCtrl1Y; ctrl1W = dCtrl1W; ctrl1H = dCtrl1H; ctrl1On = dCtrl1On; pont1X = dPont1X; pont1Y = dPont1Y; pont1W = dPont1W; pont1H = dPont1H; pont1On = dPont1On; pont1Modo = dPont1Modo; pont1Lado = dPont1Lado;
            sfxLigado = dSfxLigado; sfxVolume = dSfxVolume;
            upBg = dUpBg; upTextNorm = dUpTextNorm; upTextSel = dUpTextSel;
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
            else if (editTarget == 7) { fontTam = dFontTam; fontAlign = dFontAlign; fontScroll = dFontScroll; }
            else if (editTarget == 8) { msgX = dMsgX; msgY = dMsgY; msgTam = dMsgTam; }
            else if (editTarget == 9) { listMark = dListMark; listHoverMark = dListHoverMark; upX = dUpX; upY = dUpY; upW = dUpW; upH = dUpH; upBg = dUpBg; upTextNorm = dUpTextNorm; upTextSel = dUpTextSel; }
            else if (editTarget == 10) { elem1X = dElem1X; elem1Y = dElem1Y; elem1W = dElem1W; elem1H = dElem1H; elem1On = dElem1On; }
            else if (editTarget == 11) { ctrl1X = dCtrl1X; ctrl1Y = dCtrl1Y; ctrl1W = dCtrl1W; ctrl1H = dCtrl1H; ctrl1On = dCtrl1On; }
            else if (editTarget == 12) { pont1X = dPont1X; pont1Y = dPont1Y; pont1W = dPont1W; pont1H = dPont1H; pont1On = dPont1On; pont1Modo = dPont1Modo; pont1Lado = dPont1Lado; }
            else if (editTarget == 13) { sfxLigado = dSfxLigado; sfxVolume = dSfxVolume; }
            salvarConfiguracao();
        }
        else { editType = acaoReal; editMode = true; }
    }
}
void acaoCircle_Editar() { if (menuAtual == MENU_EDITAR) { preencherRoot(); sel = 0; off = 0; } else if (menuAtual == MENU_EDIT_TARGET) { preencherMenuEditar(); sel = 0; off = 0; } }