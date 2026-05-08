#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <orbis/libkernel.h> 
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "video.h"
#include "graphics.h"
#include "audio.h"

extern int32_t global_uId;
extern char msgStatus[128];
extern int msgTimer;
extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);

// ==========================================
// VARIÁVEIS DE INTERFACE (Para o main.cpp não dar erro)
// ==========================================
bool exibir_botao_pular = false;
char info_nome_arquivo[256] = "", info_resolucao[64] = "", info_datas[128] = "";
char info_nome[128] = "", info_ext[16] = "", info_res[32] = "", info_aspect[32] = "";
char info_fps[16] = "", info_data_cria[32] = "", info_data_mod[32] = "";
float smoothSelEsq = 0.0f;
float smoothSelDir = 0.0f;

// ==========================================
// VARIÁVEIS GLOBAIS DO VÍDEO
// ==========================================
bool videoRodando = false, video_pausado = false, video_minimizado = false;
bool bloqueio_audio_nativo = false;
bool mostrar_hud_perf = false; 
int frame_skip = 0, video_volume = 100, comando_trocar_video = 0;
int pip_x = 110, pip_y = 90, pip_w = 768, pip_h = 432;
int total_audios = 0, total_legendas = 0;
double fps_video = 24.0, video_time_current = 0, video_time_total = 0, tempo_pulo_alvo = 0, video_fps_multiplier = 1.0;

static uint64_t last_sys_time = 0;
static double playback_time = 0.0;
bool mostrar_timeline = false;
uint32_t thumb_cache[10][192 * 108]; 
bool thumb_ready[10];

char path_pasta_video[512] = "";
char ultimo_video_tocado[512] = "";

static AVFormatContext* pFormatCtx = NULL;
static AVCodecContext* pCodecCtx = NULL, * aCodecCtx = NULL;
static AVFrame* pFrame = NULL, * pFrameRGB = NULL;
static AVPacket* packet = NULL;
static struct SwsContext* sws_ctx = NULL;
static SwrContext* swr_ctx = NULL;
static uint8_t* buffer = NULL, * audio_out_buffer = NULL;
static int videoStream = -1, audioStream = -1;

static int current_sws_dw = 0, current_sws_dh = 0;
static int current_src_w = 0; // Ajuda a recalcular os videos com o Hack LowRes

#define AUDIO_OUT_SAMPLES 1024 
static int16_t audio_accum_buf[AUDIO_OUT_SAMPLES * 2 * 20];
static int audio_accum_samples = 0;
static double audio_time_current = 0;

#define VIDEO_RING_BUFFER_SIZE (48000 * 2 * 4) 
static int16_t video_audio_ring[VIDEO_RING_BUFFER_SIZE];
static volatile int video_ring_write_pos = 0;
static volatile int video_ring_read_pos = 0;
bool bloqueio_fechar_video = false;

struct TrackInfo { char nome[64]; };
TrackInfo audios_encontrados[10], legendas_encontradas[10];

struct VideoMarker { char nome[128]; double inicio; double fim; };
VideoMarker markers[50];
int total_markers = 0;

void pularParaTempo(double tempo);

// ==========================================
// FUNÇÕES DE ÁUDIO (SOM DE VOLTA!)
// ==========================================
void misturarAudioVideo(int16_t* out_buffer, int amostras) {
    if (!videoRodando || video_pausado) return;
    int read_pos = video_ring_read_pos;
    int esc_pos = video_ring_write_pos;
    int samples_to_read = amostras * 2;
    int available = esc_pos - read_pos;
    if (available < 0) available += VIDEO_RING_BUFFER_SIZE;
    if (available < samples_to_read) return; 
    for (int i = 0; i < samples_to_read; i++) {
        int val = out_buffer[i] + video_audio_ring[read_pos];
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        out_buffer[i] = val;
        read_pos++;
        if (read_pos >= VIDEO_RING_BUFFER_SIZE) read_pos = 0;
    }
    video_ring_read_pos = read_pos;
}

// ==========================================
// MENUS
// ==========================================
void preencherMenuOpcoesVideo() {
    extern const char* listaOpcoes[150];
    extern int totalOpcoes;
    extern bool showOpcoes;
    extern int mapOpcoes[150];
    extern int selOpcao;
    
    listaOpcoes[0] = video_pausado ? "Continuar Video" : "Pausar Video"; mapOpcoes[0] = 800;
    listaOpcoes[1] = mostrar_timeline ? "Ocultar Linha do Tempo" : "Mostrar Linha do Tempo"; mapOpcoes[1] = 801;
    listaOpcoes[2] = "Aumentar Volume (+10%)"; mapOpcoes[2] = 802;
    listaOpcoes[3] = "Diminuir Volume (-10%)"; mapOpcoes[3] = 803;
    listaOpcoes[4] = "Avançar 10 Segundos"; mapOpcoes[4] = 804;
    listaOpcoes[5] = "Retroceder 10 Segundos"; mapOpcoes[5] = 805;

    totalOpcoes = 6; showOpcoes = true; selOpcao = 0;
}

void executarAcaoMenuVideo(int id) {
    if (id == 800) video_pausado = !video_pausado;
    if (id == 801) mostrar_timeline = !mostrar_timeline;
    if (id == 802) { video_volume += 10; if(video_volume>100) video_volume=100; }
    if (id == 803) { video_volume -= 10; if(video_volume<0) video_volume=0; }
    if (id == 804) pularParaTempo(video_time_current + 10.0);
    if (id == 805) pularParaTempo(video_time_current - 10.0);
}

void desenharLinhaDoTempo(uint32_t* tela) {
    if (!mostrar_timeline || video_time_total <= 0) return;
    int start_y = 880;
    for (int y = start_y; y < 1080; y++) {
        for (int x = 0; x < 1920; x++) tela[y * 1920 + x] = 0xEE1E1E1E; 
    }
    for (int x = 0; x < 1920; x++) tela[start_y * 1920 + x] = 0xFF555555;
    for (int x = 0; x < 1920; x++) tela[(start_y + 80) * 1920 + x] = 0xFF555555;
    for (int i = 0; i < 10; i++) {
        int thumb_x = i * 192; 
        if (thumb_ready[i]) {
            for (int y = 0; y < 80; y++) {
                int src_y = y * 108 / 80;
                for (int x = 0; x < 192; x++) tela[(start_y + y) * 1920 + (thumb_x + x)] = thumb_cache[i][src_y * 192 + x];
            }
        }
    }
    int playhead_x = (int)((video_time_current / video_time_total) * 1920.0);
    if(playhead_x < 0) playhead_x = 0; if(playhead_x > 1918) playhead_x = 1918;
    for (int y = start_y - 20; y < 1080; y++) {
        tela[y * 1920 + playhead_x] = 0xFF0000FF; tela[y * 1920 + playhead_x + 1] = 0xFF0000FF;
    }
    char t_buf[64]; sprintf(t_buf, "Tempo: %.2fs / %.2fs", video_time_current, video_time_total);
    desenharTexto(tela, t_buf, 20, 20, start_y - 25, 0xFFFFFFFF);
}

// Dummies para Settings e Markers
void salvarConfiguracaoVideo() {}
void carregarApenasCaminhoUltimoVideo() {}
void restaurarUltimoVideo() {}
void carregarMarkers() {}
void salvarTodosMarkers() {}
void salvarMarker(double inicio, double fim, bool global) {}
void removerMarker(int index) {}

// ==========================================
// MOTOR DE VÍDEO (FFMPEG TURBO COM LOWRES)
// ==========================================
void pararVideo() {
    if (!videoRodando || bloqueio_fechar_video) return;
    if (swr_ctx) { swr_free(&swr_ctx); swr_ctx = NULL; }
    if (aCodecCtx) { avcodec_free_context(&aCodecCtx); aCodecCtx = NULL; }
    if (audio_out_buffer) { av_free(audio_out_buffer); audio_out_buffer = NULL; }
    if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = NULL; }
    if (pFrame) { av_frame_free(&pFrame); pFrame = NULL; }
    if (pFrameRGB) { av_frame_free(&pFrameRGB); pFrameRGB = NULL; }
    if (packet) { av_packet_free(&packet); packet = NULL; }
    if (buffer) { av_free(buffer); buffer = NULL; }
    if (pCodecCtx) { avcodec_free_context(&pCodecCtx); pCodecCtx = NULL; }
    if (pFormatCtx) { avformat_close_input(&pFormatCtx); pFormatCtx = NULL; }
    
    current_sws_dw = 0; current_sws_dh = 0; current_src_w = 0;
    videoRodando = false; video_minimizado = false;
    bloqueio_audio_nativo = false;
}

void iniciarVideoMP4(const char* caminho) {
    bloqueio_fechar_video = false;
    bloqueio_audio_nativo = true;
    video_ring_read_pos = 0;
    video_ring_write_pos = 0;
    memset(thumb_ready, 0, sizeof(thumb_ready)); 

    tocarMusicaNova("PARADO");
    sceKernelUsleep(150000);

    if (videoRodando) pararVideo();

    snprintf(msgStatus, sizeof(msgStatus), "FFMPEG: A iniciar descodificador Turbo...");
    msgTimer = 180;
    
    if (caminho && strlen(caminho) > 0) strcpy(ultimo_video_tocado, caminho);

    if (avformat_open_input(&pFormatCtx, caminho, NULL, NULL) != 0) return;
    avformat_find_stream_info(pFormatCtx, NULL);

    videoStream = -1; audioStream = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
        AVCodecParameters* p = pFormatCtx->streams[i]->codecpar;
        if (p->codec_type == AVMEDIA_TYPE_VIDEO && videoStream == -1) videoStream = i;
        else if (p->codec_type == AVMEDIA_TYPE_AUDIO && audioStream == -1) audioStream = i;
    }
    
    if (videoStream == -1) return;

    AVCodecParameters* pV = pFormatCtx->streams[videoStream]->codecpar;
    const AVCodec* cV = avcodec_find_decoder(pV->codec_id);
    pCodecCtx = avcodec_alloc_context3(cV);
    avcodec_parameters_to_context(pCodecCtx, pV);

    pCodecCtx->thread_count = 6;
    pCodecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    pCodecCtx->skip_loop_filter = AVDISCARD_ALL;
    pCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
    
    if (pCodecCtx->width >= 1280) {
        pCodecCtx->lowres = 1; 
    }
    
    avcodec_open2(pCodecCtx, cV, NULL);

    if (audioStream != -1) {
        AVCodecParameters* pA = pFormatCtx->streams[audioStream]->codecpar;
        const AVCodec* cA = avcodec_find_decoder(pA->codec_id);
        aCodecCtx = avcodec_alloc_context3(cA);
        avcodec_parameters_to_context(aCodecCtx, pA);
        aCodecCtx->thread_count = 2;
        avcodec_open2(aCodecCtx, cA, NULL);

        uint64_t in_channel_layout = aCodecCtx->channel_layout;
        if (in_channel_layout == 0) in_channel_layout = av_get_default_channel_layout(aCodecCtx->channels);

        swr_ctx = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 48000,
            in_channel_layout, aCodecCtx->sample_fmt, pA->sample_rate, 0, NULL);
        if (swr_ctx) swr_init(swr_ctx);

        audio_out_buffer = (uint8_t*)av_malloc(192000);
        audio_accum_samples = 0; audio_time_current = 0;
    }

    video_time_total = (double)pFormatCtx->duration / AV_TIME_BASE;
    pFrame = av_frame_alloc(); pFrameRGB = av_frame_alloc(); packet = av_packet_alloc();
    
    videoRodando = true;
    last_sys_time = sceKernelGetProcessTime(); 
    playback_time = 0.0; video_time_current = 0.0;
}

static void desenharVideoDireto(uint32_t* tela, unsigned char* img, int dW, int dH, int posX, int posY, int linesize) {
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; 
        if (pY < 0 || pY >= 1080) continue; 
        uint32_t* rowSrc = (uint32_t*)(img + (y * linesize));
        uint32_t* rowDst = tela + (pY * 1920) + posX;
        int xStart = (posX < 0) ? -posX : 0;
        int xEnd = (posX + dW > 1920) ? (1920 - posX) : dW;
        int copyWidth = xEnd - xStart;
        if (copyWidth > 0) memcpy(rowDst + xStart, rowSrc + xStart, copyWidth * sizeof(uint32_t));
    }
}

void atualizarVideoFFmpeg(uint32_t* tela) {
    if (!videoRodando || !packet) return;

    if (audio_accum_samples > 0 && !video_pausado) {
        int read_pos = video_ring_read_pos, esc_pos = video_ring_write_pos;
        int can_write = read_pos - esc_pos - 1; if (can_write < 0) can_write += VIDEO_RING_BUFFER_SIZE;
        int to_write = (audio_accum_samples * 2 < can_write) ? audio_accum_samples * 2 : can_write;
        if (to_write > 0) {
            for (int i = 0; i < to_write; i++) {
                video_audio_ring[esc_pos] = audio_accum_buf[i];
                esc_pos++; if (esc_pos >= VIDEO_RING_BUFFER_SIZE) esc_pos = 0;
            }
            video_ring_write_pos = esc_pos;
            int samples_written = to_write / 2;
            if (samples_written < audio_accum_samples) {
                memmove(audio_accum_buf, audio_accum_buf + (samples_written * 2), (audio_accum_samples - samples_written) * 4);
                audio_accum_samples -= samples_written;
            } else audio_accum_samples = 0;
        }
    }

    int boxW = video_minimizado ? pip_w : 1920;
    int boxH = video_minimizado ? pip_h : 1080;
    int bX = video_minimizado ? pip_x : 0;
    int bY = video_minimizado ? pip_y : 0;
    int dw = boxW, dh = boxH, dx = bX, dy = bY;

    if (video_pausado) {
        if (pFrameRGB->data[0]) desenharVideoDireto(tela, pFrameRGB->data[0], current_sws_dw, current_sws_dh, dx, dy, pFrameRGB->linesize[0]);
        desenharLinhaDoTempo(tela); return; 
    }

    uint64_t curr_time = sceKernelGetProcessTime();
    double delta_sec = (double)(curr_time - last_sys_time) / 1000000.0;
    last_sys_time = curr_time;
    if (delta_sec > 0.1) delta_sec = 0.1;
    playback_time += delta_sec * video_fps_multiplier;

    double sync_target = playback_time;
    if (audioStream != -1 && audio_time_current > 0.1) sync_target = audio_time_current;

    double atraso = sync_target - video_time_current;
    if (atraso > 0.15) pCodecCtx->skip_frame = AVDISCARD_NONREF;
    else pCodecCtx->skip_frame = AVDISCARD_DEFAULT;

    if (video_time_current > sync_target + 0.05) { 
        if (pFrameRGB->data[0]) desenharVideoDireto(tela, pFrameRGB->data[0], current_sws_dw, current_sws_dh, dx, dy, pFrameRGB->linesize[0]);
        desenharLinhaDoTempo(tela); return; 
    }

    bool frame_desenhado = false;
    uint64_t loop_start = sceKernelGetProcessTime();
    int read_ret = 0;

    while (!frame_desenhado && (read_ret = av_read_frame(pFormatCtx, packet)) >= 0) {
        if (packet->stream_index == videoStream) {
            if (avcodec_send_packet(pCodecCtx, packet) == 0) {
                while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                    video_time_current = pFrame->best_effort_timestamp * av_q2d(pFormatCtx->streams[videoStream]->time_base);
                    
                    float aspFile = (float)pFrame->width / (float)(pFrame->height > 0 ? pFrame->height : 1);
                    float aspDisp = (float)boxW / (float)boxH;
                    if (aspFile > aspDisp) {
                        dw = boxW; dh = (int)(boxW / aspFile);
                        dx = bX; dy = bY + (boxH - dh) / 2;
                    } else {
                        dh = boxH; dw = (int)(boxH * aspFile);
                        dy = bY; dx = bX + (boxW - dw) / 2;
                    }

                    if (current_sws_dw != dw || current_sws_dh != dh || current_src_w != pFrame->width) {
                        if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = NULL; }
                        if (buffer) { av_free(buffer); buffer = NULL; }
                        
                        sws_ctx = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, 
                                                 dw, dh, AV_PIX_FMT_BGRA, SWS_FAST_BILINEAR, NULL, NULL, NULL);
                        buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGRA, dw, dh, 1));
                        av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_BGRA, dw, dh, 1);
                        
                        current_sws_dw = dw; current_sws_dh = dh; current_src_w = pFrame->width;
                    }

                    sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pFrame->height, pFrameRGB->data, pFrameRGB->linesize);
                    frame_desenhado = true;
                }
            }
        }
        else if (packet->stream_index == audioStream) {
            if (avcodec_send_packet(aCodecCtx, packet) == 0) {
                while (avcodec_receive_frame(aCodecCtx, pFrame) == 0) {
                    audio_time_current = pFrame->best_effort_timestamp * av_q2d(pFormatCtx->streams[audioStream]->time_base);
                    int out_samples = swr_get_out_samples(swr_ctx, pFrame->nb_samples);
                    uint8_t* out_ptrs[1] = { audio_out_buffer };
                    int converted = swr_convert(swr_ctx, out_ptrs, out_samples, (const uint8_t**)pFrame->data, pFrame->nb_samples);
                    if (converted > 0 && (audio_accum_samples + converted) < (AUDIO_OUT_SAMPLES * 20)) {
                        if (video_volume < 100) {
                            int16_t* s = (int16_t*)audio_out_buffer; float m = video_volume / 100.0f;
                            for (int i = 0; i < converted * 2; i++) s[i] = (int16_t)(s[i] * m);
                        }
                        memcpy(audio_accum_buf + (audio_accum_samples * 2), audio_out_buffer, converted * 4);
                        audio_accum_samples += converted;
                    }
                }
            }
        }
        av_packet_unref(packet);
        if (sceKernelGetProcessTime() - loop_start > 30000) break;
    }

    if (frame_desenhado || pFrameRGB->data[0]) {
        desenharVideoDireto(tela, pFrameRGB->data[0], current_sws_dw, current_sws_dh, dx, dy, pFrameRGB->linesize[0]);
    }
    
    desenharLinhaDoTempo(tela);

    if (read_ret < 0) {
        av_seek_frame(pFormatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
        if (pCodecCtx) avcodec_flush_buffers(pCodecCtx);
        if (aCodecCtx) avcodec_flush_buffers(aCodecCtx);
        playback_time = 0.0; video_time_current = 0.0; audio_time_current = 0.0;
    }
}

void pularParaTempo(double tempo) {
    if (!videoRodando || !pFormatCtx) return;
    if (tempo < 0) tempo = 0; if (tempo > video_time_total) tempo = video_time_total;
    int64_t target_pts = (int64_t)(tempo * AV_TIME_BASE);
    av_seek_frame(pFormatCtx, -1, target_pts, AVSEEK_FLAG_BACKWARD);
    if (pCodecCtx) avcodec_flush_buffers(pCodecCtx);
    if (audioStream != -1 && aCodecCtx) avcodec_flush_buffers(aCodecCtx);
    audio_accum_samples = 0; playback_time = tempo; 
}