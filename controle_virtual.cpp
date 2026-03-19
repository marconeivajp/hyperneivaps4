// --- INÍCIO DO ARQUIVO controle_virtual.cpp ---
#include "controle_virtual.h"
#include "menu.h"
#include <string.h>

// Puxamos as variáveis do sistema
extern MenuLevel menuAtual;
extern bool tecladoAtivo;
extern uint16_t* bufferTecladoW;
extern char bufferTecladoC[128];

// Função externa para voltar ao menu principal
extern void preencherRoot();

void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    memset(imeSetting, 0, sizeof(OrbisImeDialogSetting));
    imeSetting->userId = uId;
    imeSetting->type = (OrbisImeType)0;
    imeSetting->maxTextLength = 127;

    memset(bufferTecladoW, 0, 1024);
    for (int i = 0; i < 127; i++) {
        bufferTecladoW[i] = (uint16_t)bufferTecladoC[i];
        if (bufferTecladoC[i] == '\0') break;
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
// --- FIM DO ARQUIVO controle_virtual.cpp ---