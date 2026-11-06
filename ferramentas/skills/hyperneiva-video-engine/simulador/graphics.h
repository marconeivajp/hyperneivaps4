#pragma once
#include <stdint.h>
#include <stddef.h>
uint32_t* obterBufferVideo();
void submeterTela();
bool graficosPrepararApresentacao();
void graficosEntrarApresentacao();
void graficosSairApresentacao();
uint32_t* graficosPegarCamada(bool* nova);
uint32_t* graficosBufferBack();
int graficosIndiceBack();
void graficosFlip();
bool graficosRepetirFlip();
bool graficosFlipPendente();
