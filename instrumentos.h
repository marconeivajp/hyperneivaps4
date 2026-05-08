#pragma once
#include <stdint.h>
#include <stddef.h> // Necessário para o size_t

void renderizarInstrumentos(uint32_t* p);

// Função que vai gerar e injetar o som do piano no buffer
void misturarAudioPiano(int16_t* bufferAudio, size_t frames);