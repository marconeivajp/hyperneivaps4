#ifndef CONTROLE_EMULADOR_H
#define CONTROLE_EMULADOR_H

#include <stdint.h>
#include <orbis/Pad.h>

extern int selMapeamento;
extern bool esperandoBotao;

void atualizarMapeamentoControles(const OrbisPadData* pData, uint32_t ultimos);
void desenharAjudaControles(uint32_t* pixels);

#endif
