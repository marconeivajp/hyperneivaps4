// --- INÍCIO DO ARQUIVO controle_virtual.h ---
#pragma once
#include <stdint.h>
#include <stdarg.h>

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

#include <orbis/ImeDialog.h>

void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
void acaoCircle_Notepad();
// --- FIM DO ARQUIVO controle_virtual.h ---