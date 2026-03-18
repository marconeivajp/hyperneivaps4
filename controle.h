#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>
#include <orbis/Pad.h>
#include <orbis/ImeDialog.h>

// Variáveis globais de controle para que outros arquivos achem
extern int cd;
extern bool pCross;
extern bool pCircle;
extern bool pTri;

// Declarações das funções chamadas por outros arquivos e pelo main
void executarAcaoX();
void executarAcaoBolinha();
void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);

#endif