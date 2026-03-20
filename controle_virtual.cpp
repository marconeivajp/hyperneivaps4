#include "controle_virtual.h"
#include "menu.h"
#include <string.h>

extern MenuLevel menuAtual;
extern bool tecladoAtivo;
extern uint16_t* bufferTecladoW;
extern char bufferTecladoC[128];

extern void preencherRoot();

void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial) {
    memset(imeSetting, 0, sizeof(OrbisImeDialogSetting));
    imeSetting->userId = uId;
    imeSetting->type = (OrbisImeType)0; // Default
    imeSetting->maxTextLength = 127;

    // LIMPEZA ABSOLUTA DO TECLADO
    memset(bufferTecladoW, 0, 1024);
    memset(bufferTecladoC, 0, 128);

    // Se a linha já tinha algo escrito, coloca no teclado. Se não, fica em branco.
    if (textoInicial != NULL && strlen(textoInicial) > 0) {
        strncpy(bufferTecladoC, textoInicial, 127);
        for (int i = 0; i < 127; i++) {
            if (bufferTecladoC[i] == '\0') break;
            bufferTecladoW[i] = (uint16_t)bufferTecladoC[i];
        }
    }

    imeSetting->inputTextBuffer = (wchar_t*)bufferTecladoW;
    imeSetting->title = (wchar_t*)imeTitle;

    if (sceImeDialogInit(imeSetting, NULL) >= 0) {
        tecladoAtivo = true;
    }
}

void acaoCircle_Notepad() {
    preencherRoot();
}