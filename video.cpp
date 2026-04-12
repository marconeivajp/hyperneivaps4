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

// ==========================================
// VARIÁVEIS GLOBAIS DO VÍDEO
// ==========================================
bool videoRodando = false, video_pausado = false, video_minimizado = false;
bool bloqueio_audio_nativo = false;
bool mostrar_hud_perf = false; // Tela de depuração
int frame_skip = 0, video_volume = 100, comando_trocar_video = 0;
int pip_x = 110, pip_y = 90, pip_w = 768, pip_h = 432;
int total_audios = 0, total_legendas = 0;
double fps_video = 24.0, video_time_current = 0, video_time_total = 0, tempo_pulo_alvo = 0, video_fps_multiplier = 1.0;

char info_nome_arquivo[256], info_resolucao[64], info_datas[128];
char info_nome[128], info_ext[16], info_res[32], info_aspect[32], info_fps[16], info_data_cria[32], info_data_mod[32];
char path_pasta_video[512] = "";

// Memória para o atalho Global (L2 + Quadrado)
char ultimo_video_tocado[512] = "";

static AVFormatContext* pFormatCtx = NULL;
static AVCodecContext* pCodecCtx = NULL, * aCodecCtx = NULL;
static AVFrame* pFrame = NULL, * pFrameRGB = NULL;
static AVPacket* packet = NULL;
static struct SwsContext* sws_ctx = NULL;
static SwrContext* swr_ctx = NULL;
static uint8_t* buffer = NULL, * audio_out_buffer = NULL;
static int videoStream = -1, audioStream = -1;

#define AUDIO_OUT_SAMPLES 1024 
static int16_t audio_accum_buf[AUDIO_OUT_SAMPLES * 2 * 20];
static int audio_accum_samples = 0;
static double audio_time_current = 0;

// RING BUFFER PARA PASSAR O AUDIO PARA O AUDIO.CPP
#define VIDEO_RING_BUFFER_SIZE (48000 * 2 * 4) // 4 segundos de áudio
static int16_t video_audio_ring[VIDEO_RING_BUFFER_SIZE];
static volatile int video_ring_write_pos = 0;
static volatile int video_ring_read_pos = 0;

void misturarAudioVideo(int16_t* out_buffer, int amostras) {
    if (!videoRodando) return;
    int read_pos = video_ring_read_pos;
    int esc_pos = video_ring_write_pos;
    int samples_to_read = amostras * 2;
    int available = esc_pos - read_pos;
    if (available < 0) available += VIDEO_RING_BUFFER_SIZE;
    if (available < samples_to_read) return; // Underflow
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

// Flag controlada pelo main.cpp para impedir o explorador de fechar o vídeo no PiP
bool bloqueio_fechar_video = false;

struct TrackInfo { char nome[64]; };
TrackInfo audios_encontrados[10], legendas_encontradas[10];

struct VideoMarker { char nome[128]; double inicio; double fim; };
VideoMarker markers[50];
int total_markers = 0;
bool exibir_botao_pular = false;

// Função declarada aqui para ser usada no restaurarUltimoVideo
void pularParaTempo(double tempo);
void iniciarVideoMP4(const char* caminho);

// ==========================================
// GERENCIADOR DE MARKERS (PULAR ABERTURA)
// ==========================================
void carregarMarkers() {
    total_markers = 0;
    char pM[512]; sprintf(pM, "%s/neiva_markers.txt", path_pasta_video);
    FILE* f = fopen(pM, "r");
    if (f) {
        while (total_markers < 50 && fscanf(f, "%[^|]|%lf|%lf\n", markers[total_markers].nome, &markers[total_markers].inicio, &markers[total_markers].fim) == 3) {
            total_markers++;
        }
        fclose(f);
    }
}

void salvarTodosMarkers() {
    char pM[512]; sprintf(pM, "%s/neiva_markers.txt", path_pasta_video);
    FILE* f = fopen(pM, "w");
    if (f) {
        for (int i = 0; i < total_markers; i++) fprintf(f, "%s|%.2f|%.2f\n", markers[i].nome, markers[i].inicio, markers[i].fim);
        fclose(f);
    }
}

void salvarMarker(double inicio, double fim, bool global) {
    if (total_markers < 50) {
        if (global) strcpy(markers[total_markers].nome, "GLOBAL");
        else strcpy(markers[total_markers].nome, info_nome);
        markers[total_markers].inicio = inicio;
        markers[total_markers].fim = fim;
        total_markers++;
        salvarTodosMarkers();
    }
}

void removerMarker(int index) {
    if (index < 0 || index >= total_markers) return;
    for (int i = index; i < total_markers - 1; i++) markers[i] = markers[i + 1];
    total_markers--;
    salvarTodosMarkers();
}

// ==========================================
// GERENCIADOR DE CONFIGURAÇÕES (TXT)
// ==========================================
#define ARQUIVO_SETTINGS "/data/HyperNeiva/configuracao/video_settings.txt"

void salvarConfiguracaoVideo() {
    FILE* f = fopen(ARQUIVO_SETTINGS, "w");
    if (f) {
        // Salva o caminho do vídeo na primeira linha
        fprintf(f, "%s\n", ultimo_video_tocado);
        // Salva as outras configs nas linhas seguintes
        fprintf(f, "%lf\n", video_time_current);
        fprintf(f, "%d\n", video_volume);
        fprintf(f, "%d\n", frame_skip);
        fclose(f);
    }
}

void carregarApenasCaminhoUltimoVideo() {
    FILE* f = fopen(ARQUIVO_SETTINGS, "r");
    if (f) {
        char linha[512] = { 0 };
        if (fgets(linha, sizeof(linha), f)) {
            linha[strcspn(linha, "\r\n")] = 0; // Remove quebra de linha
            strcpy(ultimo_video_tocado, linha);
        }
        fclose(f);
    }
}

void restaurarUltimoVideo() {
    char caminho[512] = "";
    double tempo = 0;
    int vol = 100, f_skip = 0;

    FILE* f = fopen(ARQUIVO_SETTINGS, "r");
    if (f) {
        char linha[512] = { 0 };
        // Lê a primeira linha (caminho do vídeo)
        if (fgets(linha, sizeof(linha), f)) {
            linha[strcspn(linha, "\r\n")] = 0; // Remove quebra de linha
            strcpy(caminho, linha);
            // Lê o resto dos dados
            fscanf(f, "%lf\n%d\n%d", &tempo, &vol, &f_skip);
        }
        fclose(f);
    }

    // Se achou um caminho salvo no TXT, abre ele e aplica as configs
    if (strlen(caminho) > 0) {
        iniciarVideoMP4(caminho);
        
        // Sincroniza a mente do Explorador para que o L1 / R1 ache os vídeos vizinhos!
        extern void listarDiretorio(const char* path);
        listarDiretorio(path_pasta_video);
        
        // Acopla a mira cega do Menu exatamente em cima do nosso nome
        extern char nomes[3000][64];
        extern int totalItens;
        extern int sel;
        for (int i = 0; i < totalItens; i++) {
            if (strstr(caminho, nomes[i]) != NULL) {
                sel = i;
                break;
            }
        }

        if (videoRodando) {
            pularParaTempo(tempo);
            video_volume = vol;
            frame_skip = f_skip;
        }
    }
}

// ==========================================
// MOTOR DE VÍDEO (FFMPEG)
// ==========================================
void pararVideo() {
    if (!videoRodando) return;

    // SE O EXPLORADOR TENTAR FECHAR INDEVIDAMENTE (PiP solto), ELE PARA AQUI!
    if (bloqueio_fechar_video) return;

    // SALVA O ESTADO DO VÍDEO NO TXT ANTES DE LIMPAR A MEMÓRIA!
    salvarConfiguracaoVideo();

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
    videoRodando = false; video_minimizado = false;
    bloqueio_audio_nativo = false;
}

void iniciarVideoMP4(const char* caminho) {
    bloqueio_fechar_video = false;
    bloqueio_audio_nativo = true;

    video_ring_read_pos = 0;
    video_ring_write_pos = 0;

    // Pausa a música de fundo!
    tocarMusicaNova("PARADO");
    sceKernelUsleep(150000);

    // Grava o vídeo atual na memória para o atalho L2 + Quadrado
    if (caminho && strlen(caminho) > 0) {
        strcpy(ultimo_video_tocado, caminho);
        salvarConfiguracaoVideo(); // CORREÇÃO: Salva no TXT mesmo se o app for morto violentamente depois!
    }

    // SALVA O ESTADO ATUAL! Se estava em PiP, vai continuar em PiP na troca (L1/R1).
    bool estado_anterior_pip = video_minimizado;

    if (videoRodando) pararVideo();
    if (avformat_open_input(&pFormatCtx, caminho, NULL, NULL) != 0) return;
    avformat_find_stream_info(pFormatCtx, NULL);

    videoStream = -1; audioStream = -1; total_audios = 0; total_legendas = 0;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
        AVCodecParameters* p = pFormatCtx->streams[i]->codecpar;
        if (p->codec_type == AVMEDIA_TYPE_VIDEO) videoStream = i;
        else if (p->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (audioStream == -1) audioStream = i;
            AVDictionaryEntry* tag = av_dict_get(pFormatCtx->streams[i]->metadata, "language", NULL, 0);
            sprintf(audios_encontrados[total_audios].nome, "Audio %d [%s]", total_audios + 1, tag ? tag->value : "und");
            total_audios++;
        }
        else if (p->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            AVDictionaryEntry* tag = av_dict_get(pFormatCtx->streams[i]->metadata, "language", NULL, 0);
            sprintf(legendas_encontradas[total_legendas].nome, "Legenda %d [%s]", total_legendas + 1, tag ? tag->value : "und");
            total_legendas++;
        }
    }
    if (videoStream == -1) return;

    AVCodecParameters* pV = pFormatCtx->streams[videoStream]->codecpar;
    const AVCodec* cV = avcodec_find_decoder(pV->codec_id);
    pCodecCtx = avcodec_alloc_context3(cV);
    avcodec_parameters_to_context(pCodecCtx, pV);

    // MODO TURBO DE EQUILIBRIO: Usa 4 núcleos pro Vídeo (50% da CPU Real do PS4)
    pCodecCtx->thread_count = 4;
    pCodecCtx->thread_type = FF_THREAD_FRAME;
    pCodecCtx->skip_loop_filter = AVDISCARD_ALL;
    avcodec_open2(pCodecCtx, cV, NULL);

    if (audioStream != -1) {
        AVCodecParameters* pA = pFormatCtx->streams[audioStream]->codecpar;
        const AVCodec* cA = avcodec_find_decoder(pA->codec_id);
        aCodecCtx = avcodec_alloc_context3(cA);
        avcodec_parameters_to_context(aCodecCtx, pA);
        aCodecCtx->thread_count = 2;
        avcodec_open2(aCodecCtx, cA, NULL);

        int canais = aCodecCtx->ch_layout.nb_channels;
        int64_t layout_mask = (canais >= 6) ? 0x3F : ((canais == 1) ? 4 : 3);

        AVChannelLayout out_layout;
        av_channel_layout_default(&out_layout, 2);

        swr_alloc_set_opts2(&swr_ctx, 
            &out_layout, AV_SAMPLE_FMT_S16, 48000,
            &aCodecCtx->ch_layout, aCodecCtx->sample_fmt, pA->sample_rate,
            0, NULL);
            
        int swr_err = 0;
        if (swr_ctx) swr_err = swr_init(swr_ctx);

        sprintf(msgStatus, "Video. Resampler: %s", swr_err == 0 ? "OK" : "ERRO");
        msgTimer = 600;

        audio_out_buffer = (uint8_t*)av_malloc(192000);
        audio_accum_samples = 0;
        audio_time_current = 0;
    }

    const char* barra = strrchr(caminho, '/'); strcpy(info_nome_arquivo, barra ? barra + 1 : caminho); strcpy(info_nome, info_nome_arquivo);
    const char* ponto = strrchr(info_nome, '.'); strcpy(info_ext, ponto ? ponto : "N/A");
    int vw = pCodecCtx->width, vh = pCodecCtx->height;
    sprintf(info_res, "%dx%d", vw, vh); sprintf(info_resolucao, "%dx%d", vw, vh);
    if (vh > 0) sprintf(info_aspect, "%.2f:1", (float)vw / vh);
    fps_video = av_q2d(pFormatCtx->streams[videoStream]->avg_frame_rate);
    if (fps_video < 10) fps_video = 30.0; sprintf(info_fps, "%.2f", fps_video);

    struct stat st; if (stat(caminho, &st) == 0) { strftime(info_data_mod, 32, "%d/%m/%Y %H:%M", localtime(&st.st_mtime)); strcpy(info_datas, info_data_mod); }
    strcpy(path_pasta_video, caminho); char* last = strrchr(path_pasta_video, '/'); if (last) *last = '\0';
    carregarMarkers();

    pFrame = av_frame_alloc(); pFrameRGB = av_frame_alloc(); packet = av_packet_alloc();

    // MAGIA DA COR E VELOCIDADE EXTREMA: O FFmpeg vai cuspir AV_PIX_FMT_BGRA
    // Formato BGRA se alinha milimetricamente em Little Endian como o "uint32_t" de tela nativo do PS4!
    buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGRA, vw, vh, 1));
    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_BGRA, vw, vh, 1);
    sws_ctx = sws_getContext(vw, vh, pCodecCtx->pix_fmt, vw, vh, AV_PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);

    // RESTAURA O ESTADO DO PIP
    video_minimizado = estado_anterior_pip;
    videoRodando = true;
    video_time_total = (double)pFormatCtx->duration / AV_TIME_BASE;
}

// ESCALONADOR NATIVO EM PONTO FIXO DE ALTA VELOCIDADE (Bypass de Gargalo da Tela Cheia)
static void desenharVideoRedimensionadoFast(uint32_t* tela, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY) {
    uint32_t stepX = (imgW << 16) / dW;
    uint32_t stepY = (imgH << 16) / dH;
    uint32_t currentY = 0;
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; 
        if (pY < 0 || pY >= 1080) { currentY += stepY; continue; }
        int oY = currentY >> 16;
        uint32_t* rowSrc32 = (uint32_t*)(img + (oY * imgW * 4));
        uint32_t* rowDst = tela + (pY * 1920) + posX;
        uint32_t currentX = 0;
        int xStart = (posX < 0) ? -posX : 0;
        int xEnd = (posX + dW > 1920) ? (1920 - posX) : dW;
        currentX += stepX * xStart;
        rowDst += xStart;
        for (int x = xStart; x < xEnd; x++) {
            // A maior magia de velocidade do projeto: copia o pixel de 32 bits nativo num pulo só de Assembly!
            *rowDst++ = rowSrc32[currentX >> 16];
            currentX += stepX;
        }
        currentY += stepY;
    }
}

void atualizarVideoFFmpeg(uint32_t* tela) {
    if (!videoRodando || !packet) return;

    // Tenta esvaziar áudio pendente da rodada anterior (Não bloqueante)
    if (audio_accum_samples > 0) {
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
    int dw_box = video_minimizado ? pip_w : 1920;
    int dh_box = video_minimizado ? pip_h : 1080;
    int dx_box = video_minimizado ? pip_x : 0;
    int dy_box = video_minimizado ? pip_y : 0;

    int dw = dw_box, dh = dh_box, dx = dx_box, dy = dy_box;

    // Preserva Aspect Ratio Autêntico em Ambos os Modos (Fullscreen e PiP/Miniatura)
    if (pCodecCtx && pCodecCtx->height > 0) {
        float fW = (float)pCodecCtx->width;
        float fH = (float)pCodecCtx->height;
        float aspFile = fW / fH;
        float aspDisp = (float)dw_box / (float)dh_box;
        if (aspFile > aspDisp) { // Cinema Widescreen
            dh = (int)(dw_box / aspFile);
            dy = dy_box + (dh_box - dh) / 2;
        } else if (aspFile < aspDisp) { // Celular Vertical / Portrait Format
            dw = (int)(dh_box * aspFile);
            dx = dx_box + (dw_box - dw) / 2;
        }
    }

    // BOTÃO DE SALTAR DESAPARECE NA HORA CERTA (< markers[i].fim)
    exibir_botao_pular = false;
    for (int i = 0; i < total_markers; i++) {
        if ((strcmp(markers[i].nome, "GLOBAL") == 0 || strcmp(markers[i].nome, info_nome) == 0) &&
            video_time_current >= markers[i].inicio && video_time_current < markers[i].fim) {
            exibir_botao_pular = true; tempo_pulo_alvo = markers[i].fim; break;
        }
    }

    if (video_pausado) {
        if (pFrameRGB->data[0]) desenharVideoRedimensionadoFast(tela, pFrameRGB->data[0], pCodecCtx->width, pCodecCtx->height, dw, dh, dx, dy);
        return;
    }

    bool frame_desenhado = false;
    static int f_count = 0;
    int frames_pulados_por_lag = 0;

    // O NOVO LOOP DE ÁUDIO/VÍDEO (Fim do Engasgo)
    while (!frame_desenhado && av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoStream && avcodec_send_packet(pCodecCtx, packet) == 0) {
            while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                f_count++;
                video_time_current = pFrame->best_effort_timestamp * av_q2d(pFormatCtx->streams[videoStream]->time_base);

                // Se estamos pulando frames porque o usuário mandou no menu
                if (frame_skip > 0 && (f_count % (frame_skip + 1) != 0)) {
                    continue; // Ignora o vídeo, mas CONTINUA lendo o arquivo para não matar o áudio!
                }

                // Sincronia de A/V nativa: Se o vídeo ficar lento, pula o frame para emparelhar com o som
                if (audioStream != -1 && audio_time_current > 0.5) {
                    if (video_time_current < audio_time_current - 0.05) {
                        if (frames_pulados_por_lag < 3) {
                            frames_pulados_por_lag++;
                            continue;
                        }
                    } else if (video_time_current > audio_time_current + 0.02) {
                         // FREIO ABS DE BURST: Delay sutil para manter o 60fps estável
                         int usDelay = (int)((video_time_current - audio_time_current) * 1000000.0f);
                         if (usDelay > 8000) usDelay = 8000; // Cap de 8ms para o Main Loop não engasgar
                         if (usDelay > 500) sceKernelUsleep(usDelay);
                    }
                }

                sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
                desenharVideoRedimensionadoFast(tela, pFrameRGB->data[0], pCodecCtx->width, pCodecCtx->height, dw, dh, dx, dy);
                frame_desenhado = true; // Achou um quadro válido, desenhou, sai do loop!
            }
        }
        else if (packet->stream_index == audioStream && avcodec_send_packet(aCodecCtx, packet) == 0) {
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

                    // Isso manda o áudio pro PS4 pelo Ring Buffer compartilhado (Tenta mandar o máximo possível)
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
            }
        }
        av_packet_unref(packet);
    }

    // Finaliza ou aplica delay extra (se o vídeo não tiver áudio nenhum pra segurar o ritmo)
    if (!frame_desenhado) av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
    else if (audioStream == -1 && video_fps_multiplier <= 1.0f) {
        int d = (int)((1000000.0 / fps_video) / video_fps_multiplier);
        if (d > 0) sceKernelUsleep(d);
    }

    // TELA DE DEPURAÇÃO (HUD PERFORMANCE)
    if (mostrar_hud_perf) {
        static uint64_t last_t = 0;
        uint64_t curr_t = sceKernelGetProcessTime();
        double frame_time = (double)(curr_t - last_t) / 1000.0;
        last_t = curr_t;

        char hud_buf[128];
        sprintf(hud_buf, "FPS: %.2f | V: %.2fs | A: %.2fs | Sync: %.3fs", 
                1000.0 / frame_time, video_time_current, audio_time_current, video_time_current - audio_time_current);
        
        // Desenha uma tarja preta no fundo para legibilidade no canto inferior direito
        for (int hy = 1030; hy < 1070; hy++) {
            for (int hx = 1350; hx < 1910; hx++) {
                tela[hy * 1920 + hx] = 0xAA000000;
            }
        }
        desenharTexto(tela, hud_buf, 20, 1370, 1040, 0xFF00FF00); // Texto verde neon para depuração
    }
}

void pularParaTempo(double tempo) {
    if (!videoRodando || !pFormatCtx) return;
    // Pulo temporal Universal Seguro em MicroSegundos (Base inteira do arquivo em vez de Stream Desconectada)
    int64_t target_pts = (int64_t)(tempo * AV_TIME_BASE);
    av_seek_frame(pFormatCtx, -1, target_pts, AVSEEK_FLAG_BACKWARD);
    
    // Sangria obrigatória: O FFmpeg precisa cuspir os quadros antigos que estavam entalados na CPU após um pulo!
    if (pCodecCtx) avcodec_flush_buffers(pCodecCtx);
    if (audioStream != -1 && aCodecCtx) avcodec_flush_buffers(aCodecCtx);
    
    audio_accum_samples = 0;
}