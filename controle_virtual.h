#pragma once
#ifndef CONTROLE_VIRTUAL_H
#define CONTROLE_VIRTUAL_H

#include <stdint.h>
#include <stdarg.h>

// Proteção para o Visual Studio não surtar com código do PS4
#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <orbis/ImeDialog.h>

// Modificado para receber o texto inicial e limpar buffer
void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);
void acaoCircle_Notepad();

#endif