#ifndef VIDEO_H
#define VIDEO_H

extern bool videoRodando;
extern bool bloqueio_audio_nativo;

void iniciarVideoMP4(const char* caminho);
void pararVideo();
void carregarApenasCaminhoUltimoVideo();
void misturarAudioVideo(int16_t* out_buffer, int amostras);

#endif