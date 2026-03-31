#include "controle_editar.h"
#include "menu.h"
#include "editar.h"
#include "elementos_animados_sprite_sheet.h"
#include "stb_image.h"
#include <string.h>
#include <stdio.h>
#include <orbis/Pad.h> 
#include <orbis/ImeDialog.h>
#include <stddef.h>

extern MenuLevel menuAtual; extern int sel; extern int off; extern int editType; extern int editTarget; extern bool editMode;
extern int mapAcoes[50];
extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH, barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH, fontTam, msgX, msgY, msgTam, listOri, listBg, barBg, barFill, listMark, listHoverMark, fontAlign, fontScroll;
extern int elem1X, elem1Y, elem1W, elem1H, elem1On, ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On, pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;
extern int sfxLigado, sfxVolume; extern int upBg, upTextNorm, upTextSel;
extern int picX, picY, picW, picH;
extern int listStyle, fontAnim, listCurvature, listZoomCentro;

extern void salvarConfiguracao(); extern void preencherMenuEditTarget(); extern void preencherMenuEditar(); extern void preencherRoot();
extern void preencherExplorerHome(); extern void salvarAnimacaoComNome(const char*); extern void salvarAnimacaoUnity(const char*);

bool tecladoImeAberto = false; wchar_t bufferTextoIme[256];

bool crossReleaseRequired = true;
bool circleReleaseRequired = true;
extern bool selecionandoMidiaElemento;

#ifndef CUSTOM_UI_DEF
#define CUSTOM_UI_DEF
struct CustomElementDef {
    bool ativo;
    char caminho[256];
    int pX, pY, pW, pH;
    bool animInAtiva;
    int inX, inY;
    int velIn;
    bool animOutAtiva;
    int outX, outY;
    int velOut;
};
#endif

extern CustomElementDef customUI[6][10];
extern int interfaceTelaAlvo;
extern int interfaceElementoAlvo;
extern int uiW[10], uiH[10];

void abrirTecladoIme() {
    if (tecladoImeAberto) return;
    for (int i = 0; i < 256; i++) bufferTextoIme[i] = 0;
    const char* def = "nova_animacao"; uint16_t* bufWrite = (uint16_t*)bufferTextoIme;
    for (int i = 0; i < 13; i++) bufWrite[i] = (uint16_t)def[i];
    OrbisImeDialogSetting imeConfig; memset(&imeConfig, 0, sizeof(OrbisImeDialogSetting));
    imeConfig.userId = 0; imeConfig.maxTextLength = 60; imeConfig.inputTextBuffer = bufferTextoIme; imeConfig.title = (wchar_t*)L"Digite o Nome da Animacao";
    sceImeDialogInit(&imeConfig, NULL); tecladoImeAberto = true;
}

void verificarTecladoVirtual() {
    if (!tecladoImeAberto) return;
    OrbisDialogStatus status = sceImeDialogGetStatus();
    if (status == 2) {
        OrbisDialogResult res; memset(&res, 0, sizeof(OrbisDialogResult)); sceImeDialogGetResult(&res);
        char nomeFinalASCII[256]; memset(nomeFinalASCII, 0, sizeof(nomeFinalASCII));
        uint16_t* bufRead = (uint16_t*)bufferTextoIme; int len = 0;
        for (int i = 0; i < 255; i++) { if (bufRead[i] == 0x0000) break; nomeFinalASCII[len] = (char)bufRead[i]; len++; }
        nomeFinalASCII[len] = '\0';
        if (len > 0) {
            salvarAnimacaoComNome(nomeFinalASCII);
            salvarAnimacaoUnity(nomeFinalASCII); // UNITY EXPORT RESTAURADO
            sprintf(msgStatus, "SALVO EM: configuracao/%s.bin", nomeFinalASCII);
            msgTimer = 180;
        }
        sceImeDialogTerm(); tecladoImeAberto = false;
    }
}

void acaoCross_Editar() {
    if (menuAtual == MENU_EDITAR) {
        if (sel == 17) {
            listXV = 1054; listYV = 204; listSpcV = 120; listXH = 50; listYH = 800; listSpcH = 300; listW = 550; listH = 80; capaX = 150; capaY = 640; capaW = 300; capaH = 400; discoX = 555; discoY = 650; discoW = 300; discoH = 300; backX = 0; backY = 0; backW = 1920; backH = 1080; barX = 95; barY = 911; barW = 345; barH = 15; audioX = 545; audioY = 632; audioW = 321; audioH = 378; upX = 545; upY = 632; upW = 321; upH = 378; fontTam = 35; msgX = 100; msgY = 970; msgTam = 40; listOri = 0; listBg = 0; barBg = 6; barFill = 7; listMark = 8; listHoverMark = 9; fontAlign = 0; fontScroll = 0; elem1X = 100; elem1Y = 358; elem1W = 200; elem1H = 200; elem1On = 0; ctrl1X = 724; ctrl1Y = 361; ctrl1W = 200; ctrl1H = 200; ctrl1On = 0; pont1X = 0; pont1Y = 0; pont1W = 50; pont1H = 50; pont1On = 0; pont1Modo = 0; pont1Lado = 0; sfxLigado = 1; sfxVolume = 100; upBg = 0; upTextNorm = 12; upTextSel = 8; picX = 150; picY = 100; picW = 730; picH = 410; anim_ativo = false; anim_usarColorKey = true; anim_posX = 445; anim_posY = -130; anim_colunas = 24; anim_linhas = 19; anim_velocidade = 100; anim_escala = 4.5f; anim_keyR = 55; anim_keyG = 39; anim_keyB = 130; anim_offsetX = 65; anim_offsetY = 0; anim_frameInicial = 0; anim_frameFinal = 7; anim_modoTeste = false; anim_usarColorKey2 = true; anim_keyR2 = 0; anim_keyG2 = 0; anim_keyB2 = 255; anim_autoCenter = false; anim_tolerancia = 100; listStyle = 0; fontAnim = 0; listCurvature = 15; listZoomCentro = 15;
            for (int i = 0; i < 100; i++) { anim_frameOffsetX[i] = 0; anim_frameOffsetY[i] = 0; }
            salvarConfiguracao();
        }
        else { editTarget = sel; preencherMenuEditTarget(); sel = 0; off = 0; }
    }
    else if (menuAtual == MENU_EDIT_TARGET) {
        int acaoReal = mapAcoes[sel];
        if (acaoReal >= 100 && acaoReal <= 105) { interfaceTelaAlvo = acaoReal - 100; editTarget = 17; preencherMenuEditTarget(); sel = 0; off = 0; }
        else if (acaoReal == 51) {
            if (editTarget == 17) {
                int slot = -1; for (int i = 0; i < 10; i++) { if (!customUI[interfaceTelaAlvo][i].ativo) { slot = i; break; } }
                if (slot != -1) {
                    interfaceElementoAlvo = slot; selecionandoMidiaElemento = true; editMode = false; preencherExplorerHome(); menuAtual = MENU_EXPLORAR_HOME;
                }
                else { strcpy(msgStatus, "LIMITE MAXIMO (10) ATINGIDO! DELETE UM."); msgTimer = 180; }
            }
            else if (editTarget == 18) {
                selecionandoMidiaElemento = true; editMode = false; preencherExplorerHome(); menuAtual = MENU_EXPLORAR_HOME;
            }
        }
        else if (acaoReal >= 52 && acaoReal <= 61) { interfaceElementoAlvo = acaoReal - 52; editTarget = 18; preencherMenuEditTarget(); sel = 0; off = 0; }
        else if (acaoReal == 66) { customUI[interfaceTelaAlvo][interfaceElementoAlvo].ativo = false; extern void salvarCustomUI(); salvarCustomUI(); editTarget = 17; preencherMenuEditTarget(); sel = 0; off = 0; strcpy(msgStatus, "ELEMENTO DELETADO!"); msgTimer = 90; }
        else if (acaoReal >= 62 && acaoReal <= 71) { editType = acaoReal; editMode = true; crossReleaseRequired = true; circleReleaseRequired = true; }
        else if (acaoReal == 9) { salvarConfiguracao(); }
        else if (acaoReal == 41) { abrirTecladoIme(); }
        else if (acaoReal == 29 || acaoReal == 44) { extern int wSprite, hSprite; editType = acaoReal; editMode = true; crossReleaseRequired = true; circleReleaseRequired = true; int frameW = wSprite / (anim_colunas > 0 ? anim_colunas : 1); int frameH = hSprite / (anim_linhas > 0 ? anim_linhas : 1); anim_cursorX = anim_posX + (int)((frameW * anim_escala) / 2.0f); anim_cursorY = anim_posY + (int)((frameH * anim_escala) / 2.0f); anim_editandoPivoVisual = false; }
        else { editType = acaoReal; editMode = true; crossReleaseRequired = true; circleReleaseRequired = true; }
    }
}

void acaoCircle_Editar() {
    if (menuAtual == MENU_EDITAR) { preencherRoot(); sel = 0; off = 0; }
    else if (menuAtual == MENU_EDIT_TARGET) {
        if (editTarget == 17) { editTarget = 16; preencherMenuEditTarget(); sel = 0; off = 0; }
        else if (editTarget == 18) { editTarget = 17; preencherMenuEditTarget(); sel = 0; off = 0; }
        else { preencherMenuEditar(); sel = 0; off = 0; anim_modoTeste = false; }
    }
}

void processarControlesEdicao(unsigned int buttons) {
    verificarTecladoVirtual();
    static bool pCrossEdit = false; static bool pCircleEdit = false; static bool pTriEdit = false;
    static int pDelay = 0; if (pDelay > 0) pDelay--;

    if (!(buttons & ORBIS_PAD_BUTTON_CROSS)) crossReleaseRequired = false;
    if (!(buttons & ORBIS_PAD_BUTTON_CIRCLE)) circleReleaseRequired = false;

    int* tX = &listXV, * tY = &listYV, * tW = &listW, * tH = &listH;
    if (editTarget == 0) { tX = (listOri == 0) ? &listXV : &listXH; tY = (listOri == 0) ? &listYV : &listYH; tW = &listW; tH = &listH; }
    else if (editTarget == 1) { tX = &capaX; tY = &capaY; tW = &capaW; tH = &capaH; }
    else if (editTarget == 2) { tX = &discoX; tY = &discoY; tW = &discoW; tH = &discoH; }
    else if (editTarget == 3) { tX = &picX; tY = &picY; tW = &picW; tH = &picH; }
    else if (editTarget == 4) { tX = &backX; tY = &backY; tW = &backW; tH = &backH; }
    else if (editTarget == 5) { tX = &barX; tY = &barY; tW = &barW; tH = &barH; }
    else if (editTarget == 6) { tX = &audioX; tY = &audioY; tW = &audioW; tH = &audioH; }
    else if (editTarget == 7 || editTarget == 10) { tX = &upX; tY = &upY; tW = &upW; tH = &upH; }
    else if (editTarget == 8) { tX = &fontTam; tY = &fontTam; tW = &fontTam; tH = &fontTam; }
    else if (editTarget == 9) { tX = &msgX; tY = &msgY; tW = &msgTam; tH = &msgTam; }
    else if (editTarget == 11) { tX = &elem1X; tY = &elem1Y; tW = &elem1W; tH = &elem1H; }
    else if (editTarget == 12) { tX = &ctrl1X; tY = &ctrl1Y; tW = &ctrl1W; tH = &ctrl1H; }
    else if (editTarget == 13) { tX = &pont1X; tY = &pont1Y; tW = &pont1W; tH = &pont1H; }

    // MENUS REGULARES
    if (editType == 3) { if (editTarget == 5) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { barBg--; if (barBg < 0) barBg = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { barBg++; if (barBg > 14) barBg = 0; } } else { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listBg--; if (listBg < 0) listBg = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listBg++; if (listBg > 14) listBg = 0; } } }
    else if (editType == 4) { int* tSpc = (listOri == 0) ? &listSpcV : &listSpcH; if (buttons & ORBIS_PAD_BUTTON_UP) (*tSpc) -= 5; if (buttons & ORBIS_PAD_BUTTON_DOWN) (*tSpc) += 5; if (buttons & ORBIS_PAD_BUTTON_LEFT) (*tSpc) -= 1; if (buttons & ORBIS_PAD_BUTTON_RIGHT) (*tSpc) += 1; }
    else if (editType == 5) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) listOri = (listOri == 0) ? 1 : 0; }
    else if (editType == 6) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listMark--; if (listMark < 0) listMark = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listMark++; if (listMark > 14) listMark = 0; } }
    else if (editType == 7) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listHoverMark--; if (listHoverMark < 0) listHoverMark = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listHoverMark++; if (listHoverMark > 14) listHoverMark = 0; } }
    else if (editType == 8) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { barFill--; if (barFill < 0) barFill = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { barFill++; if (barFill > 14) barFill = 0; } }
    else if (editType == 10) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { fontAlign--; if (fontAlign < 0) fontAlign = 2; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { fontAlign++; if (fontAlign > 2) fontAlign = 0; } }
    else if (editType == 11) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) fontScroll = (fontScroll == 0) ? 1 : 0; }
    else if (editType == 12) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) { if (editTarget == 11) elem1On = (elem1On == 0) ? 1 : 0; else if (editTarget == 12) ctrl1On = (ctrl1On == 0) ? 1 : 0; else if (editTarget == 13) pont1On = (pont1On == 0) ? 1 : 0; } }
    else if (editType == 13) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) pont1Modo = (pont1Modo == 0) ? 1 : 0; }
    else if (editType == 14) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { pont1Lado--; if (pont1Lado < 0) pont1Lado = 3; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { pont1Lado++; if (pont1Lado > 3) pont1Lado = 0; } }
    else if (editType == 15) { if (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) sfxLigado = (sfxLigado == 0) ? 1 : 0; }
    else if (editType == 16) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { sfxVolume -= 5; if (sfxVolume < 0) sfxVolume = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { sfxVolume += 5; if (sfxVolume > 100) sfxVolume = 100; } }
    else if (editType == 17) { if (buttons & ORBIS_PAD_BUTTON_UP) upY -= 2; if (buttons & ORBIS_PAD_BUTTON_DOWN) upY += 2; if (buttons & ORBIS_PAD_BUTTON_LEFT) upX -= 2; if (buttons & ORBIS_PAD_BUTTON_RIGHT) upX += 2; }
    else if (editType == 18) { if (buttons & ORBIS_PAD_BUTTON_UP) { upH -= 2; upW -= 2; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { upH += 2; upW += 2; } }
    else if (editType == 19) { if (buttons & ORBIS_PAD_BUTTON_UP) upH -= 2; if (buttons & ORBIS_PAD_BUTTON_DOWN) upH += 2; if (buttons & ORBIS_PAD_BUTTON_LEFT) upW -= 2; if (buttons & ORBIS_PAD_BUTTON_RIGHT) upW += 2; }
    else if (editType == 20) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { upBg--; if (upBg < 0) upBg = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { upBg++; if (upBg > 14) upBg = 0; } }
    else if (editType == 21) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { upTextNorm--; if (upTextNorm < 0) upTextNorm = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { upTextNorm++; if (upTextNorm > 14) upTextNorm = 0; } }
    else if (editType == 22) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { upTextSel--; if (upTextSel < 0) upTextSel = 14; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { upTextSel++; if (upTextSel > 14) upTextSel = 0; } }

    // ANIMAÇÃO E CHROMA KEYS (100% RESTAURADOS)
    else if (editType == 23) { if (buttons & ORBIS_PAD_BUTTON_UP) anim_posY -= 5; if (buttons & ORBIS_PAD_BUTTON_DOWN) anim_posY += 5; if (buttons & ORBIS_PAD_BUTTON_LEFT) anim_posX -= 5; if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_posX += 5; }
    else if (editType == 24) { if (buttons & ORBIS_PAD_BUTTON_UP) anim_escala += 0.1f; if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_escala -= 0.1f; if (anim_escala < 0.1f) anim_escala = 0.1f; } }
    else if (editType == 25) { if (buttons & ORBIS_PAD_BUTTON_LEFT) anim_velocidade -= 5; if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_velocidade += 5; if (buttons & ORBIS_PAD_BUTTON_UP) anim_velocidade += 20; if (buttons & ORBIS_PAD_BUTTON_DOWN) anim_velocidade -= 20; if (anim_velocidade < 1) anim_velocidade = 1; }
    else if (editType == 26) { if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_colunas -= 1; pDelay = 4; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_colunas += 1; pDelay = 4; } if (buttons & ORBIS_PAD_BUTTON_UP) { anim_linhas -= 1; pDelay = 4; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_linhas += 1; pDelay = 4; } } if (anim_colunas < 1) anim_colunas = 1; if (anim_linhas < 1) anim_linhas = 1; }
    else if (editType == 27) { if (buttons & ORBIS_PAD_BUTTON_LEFT) anim_offsetX -= 1; if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_offsetX += 1; if (buttons & ORBIS_PAD_BUTTON_UP) anim_offsetY -= 1; if (buttons & ORBIS_PAD_BUTTON_DOWN) anim_offsetY += 1; }
    else if (editType == 28) { if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_frameInicial -= 1; pDelay = 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_frameInicial += 1; pDelay = 5; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_frameFinal -= 1; pDelay = 5; } if (buttons & ORBIS_PAD_BUTTON_UP) { anim_frameFinal += 1; pDelay = 5; } } if (anim_frameInicial < 0) anim_frameInicial = 0; if (anim_frameFinal < 0) anim_frameFinal = 0; }
    else if (editType == 32) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyR -= 5; if (anim_keyR < 0) anim_keyR = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyR += 5; if (anim_keyR > 255) anim_keyR = 255; } }
    else if (editType == 33) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyG -= 5; if (anim_keyG < 0) anim_keyG = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyG += 5; if (anim_keyG > 255) anim_keyG = 255; } }
    else if (editType == 34) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyB -= 5; if (anim_keyB < 0) anim_keyB = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyB += 5; if (anim_keyB > 255) anim_keyB = 255; } }
    else if (editType == 36) { if (pDelay == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) { anim_usarColorKey2 = !anim_usarColorKey2; pDelay = 10; } }
    else if (editType == 37) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyR2 -= 5; if (anim_keyR2 < 0) anim_keyR2 = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyR2 += 5; if (anim_keyR2 > 255) anim_keyR2 = 255; } }
    else if (editType == 38) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyG2 -= 5; if (anim_keyG2 < 0) anim_keyG2 = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyG2 += 5; if (anim_keyG2 > 255) anim_keyG2 = 255; } }
    else if (editType == 39) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_keyB2 -= 5; if (anim_keyB2 < 0) anim_keyB2 = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_keyB2 += 5; if (anim_keyB2 > 255) anim_keyB2 = 255; } }
    else if (editType == 40) { if (pDelay == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) { anim_autoCenter = !anim_autoCenter; pDelay = 10; } }
    else if (editType == 42) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_tolerancia -= 50; if (anim_tolerancia < 0) anim_tolerancia = 0; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) anim_tolerancia += 50; if (buttons & ORBIS_PAD_BUTTON_UP) anim_tolerancia += 500; if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_tolerancia -= 500; if (anim_tolerancia < 0) anim_tolerancia = 0; } }
    else if (editType == 45) { if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listStyle -= 1; if (listStyle < 0) listStyle = 3; pDelay = 8; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listStyle += 1; if (listStyle > 3) listStyle = 0; pDelay = 8; } } }
    else if (editType == 46) { if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { fontAnim -= 1; if (fontAnim < 0) fontAnim = 3; pDelay = 8; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { fontAnim += 1; if (fontAnim > 3) fontAnim = 0; pDelay = 8; } } }
    else if (editType == 47) { if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listCurvature -= 1; if (listCurvature < 0) listCurvature = 0; pDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listCurvature += 1; if (listCurvature > 100) listCurvature = 100; pDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_UP) { listCurvature += 5; pDelay = 4; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { listCurvature -= 5; if (listCurvature < 0) listCurvature = 0; pDelay = 4; } } }
    else if (editType == 48) { if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { listZoomCentro -= 1; if (listZoomCentro < 0) listZoomCentro = 0; pDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { listZoomCentro += 1; if (listZoomCentro > 100) listZoomCentro = 100; pDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_UP) { listZoomCentro += 5; pDelay = 4; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { listZoomCentro -= 5; if (listZoomCentro < 0) listZoomCentro = 0; pDelay = 4; } } }

    // MODO TESTE (PIVOT + UNITY EXPORT)
    else if (editType == 29) {
        extern int wSprite, hSprite; anim_modoTeste = true; static int fDelay = 0; if (fDelay > 0) fDelay--; static bool pSqEdit = false; static bool pCrossEdit29 = false;
        int frameDrawW = (wSprite / (anim_colunas > 0 ? anim_colunas : 1)) * anim_escala; int frameDrawH = (hSprite / (anim_linhas > 0 ? anim_linhas : 1)) * anim_escala; int centroBoxX = anim_posX + frameDrawW / 2; int centroBoxY = anim_posY + frameDrawH / 2;
        if (buttons & ORBIS_PAD_BUTTON_L2) {
            if (fDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_colunas -= 1; fDelay = 5; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_colunas += 1; fDelay = 5; } if (buttons & ORBIS_PAD_BUTTON_UP) { anim_linhas -= 1; fDelay = 5; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_linhas += 1; fDelay = 5; } }
            if (anim_colunas < 1) anim_colunas = 1; if (anim_linhas < 1) anim_linhas = 1;
            if (buttons & ORBIS_PAD_BUTTON_SQUARE) { if (!pSqEdit) { anim_mostrarCruz = !anim_mostrarCruz; pSqEdit = true; } }
            else { pSqEdit = false; }
        }
        else {
            if (buttons & ORBIS_PAD_BUTTON_SQUARE) { if (!pSqEdit) { anim_editandoPivoVisual = !anim_editandoPivoVisual; if (anim_editandoPivoVisual) { anim_pivoCursorX = centroBoxX; anim_pivoCursorY = centroBoxY; } pSqEdit = true; } }
            else { pSqEdit = false; }
            if (anim_editandoPivoVisual) {
                if (fDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_pivoCursorX -= 4; fDelay = 1; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_pivoCursorX += 4; fDelay = 1; } if (buttons & ORBIS_PAD_BUTTON_UP) { anim_pivoCursorY -= 4; fDelay = 1; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_pivoCursorY += 4; fDelay = 1; } }
                if (buttons & ORBIS_PAD_BUTTON_CROSS) { if (!pCrossEdit29) { float diffX = anim_pivoCursorX - centroBoxX; float diffY = anim_pivoCursorY - centroBoxY; anim_frameOffsetX[anim_frameAtual] += (int)(diffX / anim_escala); anim_frameOffsetY[anim_frameAtual] += (int)(diffY / anim_escala); anim_editandoPivoVisual = false; salvarConfiguracao(); strcpy(msgStatus, "NOVO PIVO SALVO COM SUCESSO!"); msgTimer = 90; pCrossEdit29 = true; } }
                else { pCrossEdit29 = false; }
            }
            else {
                if (fDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_frameOffsetX[anim_frameAtual] -= 1; fDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_frameOffsetX[anim_frameAtual] += 1; fDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_UP) { anim_frameOffsetY[anim_frameAtual] -= 1; fDelay = 2; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_frameOffsetY[anim_frameAtual] += 1; fDelay = 2; } }
            }
            if (fDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_R1) { anim_frameAtual += 1; if (anim_frameAtual > anim_frameFinal) anim_frameAtual = anim_frameInicial; fDelay = 3; } if (buttons & ORBIS_PAD_BUTTON_L1) { anim_frameAtual -= 1; if (anim_frameAtual < anim_frameInicial) anim_frameAtual = anim_frameFinal; fDelay = 3; } }
            if (buttons & ORBIS_PAD_BUTTON_TRIANGLE) { if (!pTriEdit) { autoCentralizarFrameAtual(); pTriEdit = true; } }
            else { pTriEdit = false; }
        }
    }
    else if (editType == 44) {
        extern int wSprite, hSprite; anim_modoTeste = true; static int fDelay = 0; if (fDelay > 0) fDelay--; static bool pSqEdit = false;
        int frameW = (wSprite / (anim_colunas > 0 ? anim_colunas : 1)) * anim_escala; int frameH = (hSprite / (anim_linhas > 0 ? anim_linhas : 1)) * anim_escala;
        if (buttons & ORBIS_PAD_BUTTON_SQUARE && (buttons & ORBIS_PAD_BUTTON_L2)) { if (!pSqEdit) { anim_mostrarCruz = !anim_mostrarCruz; pSqEdit = true; } }
        else if (!(buttons & ORBIS_PAD_BUTTON_SQUARE)) { pSqEdit = false; }
        if (fDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { anim_cursorX -= 4; fDelay = 1; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { anim_cursorX += 4; fDelay = 1; } if (buttons & ORBIS_PAD_BUTTON_UP) { anim_cursorY -= 4; fDelay = 1; } if (buttons & ORBIS_PAD_BUTTON_DOWN) { anim_cursorY += 4; fDelay = 1; } }
        if (anim_cursorX < anim_posX) anim_cursorX = anim_posX; if (anim_cursorX > anim_posX + frameW) anim_cursorX = anim_posX + frameW; if (anim_cursorY < anim_posY) anim_cursorY = anim_posY; if (anim_cursorY > anim_posY + frameH) anim_cursorY = anim_posY + frameH;
        if (fDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_R1) { anim_frameAtual += 1; if (anim_frameAtual > anim_frameFinal) anim_frameAtual = anim_frameInicial; fDelay = 3; } if (buttons & ORBIS_PAD_BUTTON_L1) { anim_frameAtual -= 1; if (anim_frameAtual < anim_frameInicial) anim_frameAtual = anim_frameFinal; fDelay = 3; } }
        if (buttons & ORBIS_PAD_BUTTON_SQUARE && !(buttons & ORBIS_PAD_BUTTON_L2)) { if (!pSqEdit) { pegarCorNoCursor(false); salvarConfiguracao(); strcpy(msgStatus, "COR PRIMARIA EXCLUIDA E SALVA!"); msgTimer = 90; pSqEdit = true; } }
        else if (buttons & ORBIS_PAD_BUTTON_CROSS) { if (!pCrossEdit) { if (buttons & ORBIS_PAD_BUTTON_L2) { pegarCorNoCursor(true); salvarConfiguracao(); strcpy(msgStatus, "COR SECUNDARIA EXCLUIDA E SALVA!"); msgTimer = 90; } pCrossEdit = true; } }
    }

    // --- CONTROLES CUSTOM UI ---
    else if (editType == 62) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; int vel = (buttons & ORBIS_PAD_BUTTON_R1) ? 20 : 4; if (buttons & ORBIS_PAD_BUTTON_UP) el->pY -= vel; if (buttons & ORBIS_PAD_BUTTON_DOWN) el->pY += vel; if (buttons & ORBIS_PAD_BUTTON_LEFT) el->pX -= vel; if (buttons & ORBIS_PAD_BUTTON_RIGHT) el->pX += vel; }
    else if (editType == 63) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; int vel = (buttons & ORBIS_PAD_BUTTON_R1) ? 20 : 4; float aspect = 1.0f; if (uiH[interfaceElementoAlvo] > 0) aspect = (float)uiW[interfaceElementoAlvo] / (float)uiH[interfaceElementoAlvo]; else if (el->pH > 0) aspect = (float)el->pW / (float)el->pH; if (aspect <= 0.1f) aspect = 1.0f; if (buttons & ORBIS_PAD_BUTTON_UP) { el->pH -= vel; el->pW = (int)(el->pH * aspect); } if (buttons & ORBIS_PAD_BUTTON_DOWN) { el->pH += vel; el->pW = (int)(el->pH * aspect); } if (buttons & ORBIS_PAD_BUTTON_LEFT) { el->pW -= vel; el->pH = (int)(el->pW / aspect); } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { el->pW += vel; el->pH = (int)(el->pW / aspect); } if (el->pW < 10) el->pW = 10; if (el->pH < 10) el->pH = 10; }
    else if (editType == 64) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; int vel = (buttons & ORBIS_PAD_BUTTON_R1) ? 20 : 4; if (buttons & ORBIS_PAD_BUTTON_UP) el->pH -= vel; if (buttons & ORBIS_PAD_BUTTON_DOWN) el->pH += vel; if (buttons & ORBIS_PAD_BUTTON_LEFT) el->pW -= vel; if (buttons & ORBIS_PAD_BUTTON_RIGHT) el->pW += vel; if (el->pW < 10) el->pW = 10; if (el->pH < 10) el->pH = 10; }
    else if (editType == 67) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; if (pDelay == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) { el->animInAtiva = !el->animInAtiva; pDelay = 10; } }
    else if (editType == 65) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; int vel = (buttons & ORBIS_PAD_BUTTON_R1) ? 20 : 4; if (buttons & ORBIS_PAD_BUTTON_UP) el->inY -= vel; if (buttons & ORBIS_PAD_BUTTON_DOWN) el->inY += vel; if (buttons & ORBIS_PAD_BUTTON_LEFT) el->inX -= vel; if (buttons & ORBIS_PAD_BUTTON_RIGHT) el->inX += vel; }
    else if (editType == 68) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { el->velIn--; if (el->velIn < 1) el->velIn = 1; pDelay = 6; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { el->velIn++; if (el->velIn > 10) el->velIn = 10; pDelay = 6; } } }
    else if (editType == 69) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; if (pDelay == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) { el->animOutAtiva = !el->animOutAtiva; pDelay = 10; } }
    else if (editType == 70) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; int vel = (buttons & ORBIS_PAD_BUTTON_R1) ? 20 : 4; if (buttons & ORBIS_PAD_BUTTON_UP) el->outY -= vel; if (buttons & ORBIS_PAD_BUTTON_DOWN) el->outY += vel; if (buttons & ORBIS_PAD_BUTTON_LEFT) el->outX -= vel; if (buttons & ORBIS_PAD_BUTTON_RIGHT) el->outX += vel; }
    else if (editType == 71) { CustomElementDef* el = &customUI[interfaceTelaAlvo][interfaceElementoAlvo]; if (pDelay == 0) { if (buttons & ORBIS_PAD_BUTTON_LEFT) { el->velOut--; if (el->velOut < 1) el->velOut = 1; pDelay = 6; } if (buttons & ORBIS_PAD_BUTTON_RIGHT) { el->velOut++; if (el->velOut > 10) el->velOut = 10; pDelay = 6; } } }
    else if (editType == 31) { if (pDelay == 0 && (buttons & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) { anim_ativo = !anim_ativo; pDelay = 10; } }

    else {
        if (buttons & ORBIS_PAD_BUTTON_UP) { if (editType == 1) { (*tH)--; if (tW != tH) (*tW)--; } else if (editType == 2) (*tH)--; else (*tY)--; }
        if (buttons & ORBIS_PAD_BUTTON_DOWN) { if (editType == 1) { (*tH)++; if (tW != tH) (*tW)++; } else if (editType == 2) (*tH)++; else (*tY)++; }
        if (buttons & ORBIS_PAD_BUTTON_LEFT) { if (editType == 2) (*tW)--; else if (editType == 0) (*tX)--; }
        if (buttons & ORBIS_PAD_BUTTON_RIGHT) { if (editType == 2) (*tW)++; else if (editType == 0) (*tX)++; }
    }

    if (buttons & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCrossEdit && !crossReleaseRequired) {
            if (editType >= 62 && editType <= 71) { extern void salvarCustomUI(); salvarCustomUI(); editMode = false; preencherMenuEditTarget(); strcpy(msgStatus, "POSICOES SALVAS!"); msgTimer = 90; }
            else if (editType != 29 && editType != 44) { salvarConfiguracao(); editMode = false; preencherMenuEditTarget(); }
            pCrossEdit = true;
        }
    }
    else pCrossEdit = false;

    if (buttons & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircleEdit && !circleReleaseRequired) {
            if (editType >= 62 && editType <= 71) { extern void carregarCustomUI(); carregarCustomUI(); editMode = false; preencherMenuEditTarget(); strcpy(msgStatus, "ALTERACOES CANCELADAS!"); msgTimer = 90; }
            else if (editType == 29 || editType == 44) { editMode = false; preencherMenuEditTarget(); }
            else { carregarConfiguracao(); strcpy(msgStatus, "ALTERACOES CANCELADAS!"); msgTimer = 90; editMode = false; preencherMenuEditTarget(); }
            pCircleEdit = true;
        }
    }
    else pCircleEdit = false;
}