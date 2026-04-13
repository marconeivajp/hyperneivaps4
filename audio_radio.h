#ifndef AUDIO_RADIO_H
#define AUDIO_RADIO_H

#include <stdint.h>
#include <stddef.h>

// Inicializa o streaming de rádio
bool iniciarRadio(const char* url);

// Para o streaming e limpa recursos
void pararRadioStreaming();

// Lê frames decodificados (PCM 16-bit Stereo 48kHz)
size_t lerFrameRadio(int16_t* outSamples, size_t frameCount);

// Retorna uma string com informações de telemetria da rede
void obterTelemetriaRadio(char* outMsg, size_t size);

// Verifica se o rádio está em execução
bool isRadioRodando();

#endif
