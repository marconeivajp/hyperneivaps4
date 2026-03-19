// --- INÍCIO DO ARQUIVO controle_virtual.h ---
#pragma once
#include <stdint.h>
#include <orbis/ImeDialog.h>

// Funções que o controle.cpp vai chamar
void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
void acaoCircle_Notepad();
// --- FIM DO ARQUIVO controle_virtual.h ---