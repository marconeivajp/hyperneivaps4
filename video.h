#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

extern bool videoRodando;
extern bool bloqueio_audio_nativo;
extern bool mostrar_timeline; // A nossa variável mágica adicionada aqui!

void iniciarVideoMP4(const char* caminho);
void pararVideo();
void pausarVideoParaTela();
void retomarVideoAposTela();
void carregarApenasCaminhoUltimoVideo();
void misturarAudioVideo(int16_t* out_buffer, int amostras);

// MOTOR PPLAY (MPV)
void iniciarVideoMPV(const char* caminho);
void atualizarVideoMPV(uint32_t* tela);
void pararVideoMPV();
void executarAcaoMPV(int id);

#endif