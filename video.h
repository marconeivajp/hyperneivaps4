#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

extern bool videoRodando;
extern bool bloqueio_audio_nativo;
extern bool mostrar_timeline; // A nossa variável mágica adicionada aqui!

void iniciarVideoMP4(const char* caminho);
void pararVideo();
void carregarApenasCaminhoUltimoVideo();
void misturarAudioVideo(int16_t* out_buffer, int amostras);

#endif