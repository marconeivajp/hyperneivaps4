// Teclado do PS4 com texto vindo do celular (controle remoto em network.cpp).
// Incluir DEPOIS dos cabeçalhos do ImeDialog: as chamadas passam por network.cpp, que guarda
// o buffer do campo aberto e entrega nele o texto digitado no celular.
#pragma once
#include <stdint.h>
#include <orbis/_types/ime_dialog.h>

#ifdef __cplusplus
extern "C" {
#endif
int32_t hnImeInit(const OrbisImeDialogSetting* param, OrbisImeSettingsExtended* extendido);
OrbisDialogStatus hnImeStatus();
int32_t hnImeResult(OrbisDialogResult* resultado);
#ifdef __cplusplus
}
#endif

#define sceImeDialogInit hnImeInit
#define sceImeDialogGetStatus hnImeStatus
#define sceImeDialogGetResult hnImeResult
