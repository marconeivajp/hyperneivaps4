#include "controle_editar.h"
#include "menu.h"
#include "editar.h"
#include "elementos_animados_sprite_sheet.h"
#include <string.h>
#include <stdio.h>

#include <orbis/ImeDialog.h>
#include <stddef.h>

extern MenuLevel menuAtual; extern int sel; extern int off; extern int editType; extern int editTarget; extern bool editMode;
extern int mapAcoes[50];

extern int listXV, listYV, listSpcV, listXH, listYH, listSpcH, listW, listH, capaX, capaY, capaW, capaH, discoX, discoY, discoW, discoH, backX, backY, backW, backH, barX, barY, barW, barH, audioX, audioY, audioW, audioH, upX, upY, upW, upH, fontTam, msgX, msgY, msgTam, listOri, listBg, barBg, barFill, listMark, listHoverMark, fontAlign, fontScroll;
extern int elem1X, elem1Y, elem1W, elem1H, elem1On, ctrl1X, ctrl1Y, ctrl1W, ctrl1H, ctrl1On, pont1X, pont1Y, pont1W, pont1H, pont1On, pont1Modo, pont1Lado;
extern int sfxLigado, sfxVolume;
extern int upBg, upTextNorm, upTextSel;

extern void salvarConfiguracao(); extern void preencherMenuEditTarget(); extern void preencherMenuEditar(); extern void preencherRoot();

bool tecladoImeAberto = false;
wchar_t bufferTextoIme[256];

void abrirTecladoIme() {
    if (tecladoImeAberto) return;

    for (int i = 0; i < 256; i++) bufferTextoIme[i] = 0;

    const char* def = "nova_animacao";
    uint16_t* bufWrite = (uint16_t*)bufferTextoIme;
    for (int i = 0; i < 13; i++) bufWrite[i] = (uint16_t)def[i];

    OrbisImeDialogSetting imeConfig;
    memset(&imeConfig, 0, sizeof(OrbisImeDialogSetting));

    imeConfig.userId = 0;
    imeConfig.maxTextLength = 60;
    imeConfig.inputTextBuffer = bufferTextoIme;
    imeConfig.title = (wchar_t*)L"Digite o Nome da Animacao";

    sceImeDialogInit(&imeConfig, NULL);
    tecladoImeAberto = true;
}

void verificarTecladoVirtual() {
    if (!tecladoImeAberto) return;

    OrbisDialogStatus status = sceImeDialogGetStatus();

    if (status == 2) {
        OrbisDialogResult res;
        memset(&res, 0, sizeof(OrbisDialogResult));
        sceImeDialogGetResult(&res);

        char nomeFinalASCII[256];
        memset(nomeFinalASCII, 0, sizeof(nomeFinalASCII));

        uint16_t* bufRead = (uint16_t*)bufferTextoIme;
        int len = 0;

        for (int i = 0; i < 255; i++) {
            if (bufRead[i] == 0x0000) break;
            nomeFinalASCII[len] = (char)bufRead[i];
            len++;
        }
        nomeFinalASCII[len] = '\0';

        if (len > 0) {
            salvarAnimacaoComNome(nomeFinalASCII);
            salvarAnimacaoUnity(nomeFinalASCII);

            sprintf(msgStatus, "SALVO EM: configuracao/%s.bin", nomeFinalASCII);
            msgTimer = 180;
        }

        sceImeDialogTerm();
        tecladoImeAberto = false;
    }
}

void acaoCross_Editar() {
    if (menuAtual == MENU_EDITAR) {
        if (sel == 15) {
            listXV = 1054; listYV = 204; listSpcV = 120; listXH = 50; listYH = 800; listSpcH = 300; listW = 550; listH = 80; capaX = 150; capaY = 640; capaW = 300; capaH = 400; discoX = 555; discoY = 650; discoW = 300; discoH = 300; backX = 0; backY = 0; backW = 1920; backH = 1080; barX = 95; barY = 911; barW = 345; barH = 15; audioX = 545; audioY = 632; audioW = 321; audioH = 378; upX = 545; upY = 632; upW = 321; upH = 378; fontTam = 35; msgX = 100; msgY = 970; msgTam = 40; listOri = 0; listBg = 0; barBg = 6; barFill = 7; listMark = 8; listHoverMark = 9; fontAlign = 0; fontScroll = 0;
            elem1X = 100; elem1Y = 358; elem1W = 200; elem1H = 200; elem1On = 0; ctrl1X = 724; ctrl1Y = 361; ctrl1W = 200; ctrl1H = 200; ctrl1On = 0; pont1X = 0; pont1Y = 0; pont1W = 50; pont1H = 50; pont1On = 0; pont1Modo = 0; pont1Lado = 0;
            sfxLigado = 1; sfxVolume = 100;
            upBg = 0; upTextNorm = 12; upTextSel = 8;

            anim_ativo = true; anim_usarColorKey = true; anim_posX = 445; anim_posY = -130;
            anim_colunas = 24; anim_linhas = 19; anim_velocidade = 100; anim_escala = 4.5f;
            anim_keyR = 55; anim_keyG = 39; anim_keyB = 130;
            anim_offsetX = 65; anim_offsetY = 0; anim_frameInicial = 0; anim_frameFinal = 7;
            anim_modoTeste = false;

            anim_usarColorKey2 = true; anim_keyR2 = 0; anim_keyG2 = 0; anim_keyB2 = 255;
            anim_autoCenter = false;
            anim_tolerancia = 100;

            listStyle = 0;
            fontAnim = 0;
            listCurvature = 15;
            listZoomCentro = 15;

            for (int i = 0; i < 100; i++) { anim_frameOffsetX[i] = 0; anim_frameOffsetY[i] = 0; }

            salvarConfiguracao();
        }
        else { editTarget = sel; preencherMenuEditTarget(); sel = 0; off = 0; }
    }
    else if (menuAtual == MENU_EDIT_TARGET) {
        int acaoReal = mapAcoes[sel];
        if (acaoReal == 9) {
            if (editTarget == 0) { listXV = 1054; listYV = 204; listSpcV = 120; listXH = 50; listYH = 800; listSpcH = 300; listW = 550; listH = 80; listOri = 0; listBg = 0; listStyle = 0; listCurvature = 15; listZoomCentro = 15; }
            else if (editTarget == 1) { capaX = 150; capaY = 640; capaW = 300; capaH = 400; }
            else if (editTarget == 2) { discoX = 555; discoY = 650; discoW = 300; discoH = 300; }
            else if (editTarget == 3) { backX = 0; backY = 0; backW = 1920; backH = 1080; }
            else if (editTarget == 4) { barX = 95; barY = 911; barW = 345; barH = 15; barBg = 6; barFill = 7; }
            else if (editTarget == 5) { audioX = 545; audioY = 632; audioW = 321; audioH = 378; }
            else if (editTarget == 6) { upX = 545; upY = 632; upW = 321; upH = 378; }
            else if (editTarget == 7) { fontTam = 35; fontAlign = 0; fontScroll = 0; fontAnim = 0; }
            else if (editTarget == 8) { msgX = 100; msgY = 970; msgTam = 40; }
            else if (editTarget == 9) { listMark = 8; listHoverMark = 9; upX = 545; upY = 632; upW = 321; upH = 378; upBg = 0; upTextNorm = 12; upTextSel = 8; }
            else if (editTarget == 10) { elem1X = 100; elem1Y = 358; elem1W = 200; elem1H = 200; elem1On = 0; }
            else if (editTarget == 11) { ctrl1X = 724; ctrl1Y = 361; ctrl1W = 200; ctrl1H = 200; ctrl1On = 0; }
            else if (editTarget == 12) { pont1X = 0; pont1Y = 0; pont1W = 50; pont1H = 50; pont1On = 0; pont1Modo = 0; pont1Lado = 0; }
            else if (editTarget == 13) { sfxLigado = 1; sfxVolume = 100; }
            else if (editTarget == 14) {
                anim_ativo = true; anim_usarColorKey = true; anim_posX = 445; anim_posY = -130;
                anim_colunas = 24; anim_linhas = 19; anim_velocidade = 100; anim_escala = 4.5f;
                anim_keyR = 55; anim_keyG = 39; anim_keyB = 130;
                anim_offsetX = 65; anim_offsetY = 0; anim_frameInicial = 0; anim_frameFinal = 7;
                anim_modoTeste = false;
                anim_usarColorKey2 = true; anim_keyR2 = 0; anim_keyG2 = 0; anim_keyB2 = 255;
                anim_autoCenter = false;
                anim_tolerancia = 100;
                for (int i = 0; i < 100; i++) { anim_frameOffsetX[i] = 0; anim_frameOffsetY[i] = 0; }
            }
            salvarConfiguracao();
        }
        else if (acaoReal == 41) {
            abrirTecladoIme();
        }
        else if (acaoReal == 29 || acaoReal == 44) {
            editType = acaoReal; editMode = true;
            int frameW = wSprite / (anim_colunas > 0 ? anim_colunas : 1);
            int frameH = hSprite / (anim_linhas > 0 ? anim_linhas : 1);
            anim_cursorX = anim_posX + (int)((frameW * anim_escala) / 2.0f);
            anim_cursorY = anim_posY + (int)((frameH * anim_escala) / 2.0f);
            anim_editandoPivoVisual = false;
        }
        else { editType = acaoReal; editMode = true; }
    }
}

void acaoCircle_Editar() {
    if (menuAtual == MENU_EDITAR) {
        preencherRoot(); sel = 0; off = 0;
    }
    else if (menuAtual == MENU_EDIT_TARGET) {
        preencherMenuEditar(); sel = 0; off = 0;
        anim_modoTeste = false;
    }
}