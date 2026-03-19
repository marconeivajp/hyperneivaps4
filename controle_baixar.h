#pragma once
#ifndef CONTROLE_BAIXAR_H
#define CONTROLE_BAIXAR_H

#include <stdint.h>
#include <orbis/ImeDialog.h>

// DECLARAÇÕES PARA O COMPILADOR RECONHECER OS BOTÕES DO MENU BAIXAR
void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
void acaoCircle_Baixar();
void acaoTriangle_Baixar();

#endif