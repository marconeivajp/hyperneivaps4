#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <orbis/libkernel.h>
#include <orbis/Http.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include "audio_radio.h"

// --- Estruturas e Variáveis Internas ---

struct CustomHttpStream {
    int tmpl;
    bool ownTmpl;
    int conn;
    int req;
};

static CustomHttpStream* radioHttpStream = NULL;
static AVIOContext* radioAvioCtx = NULL;

#define RADIO_RING_SIZE (1024 * 1024 * 2)
static uint8_t* radioRingBuf = NULL;
static volatile int radioRingWrite = 0;
static volatile int radioRingRead = 0;
static volatile int radioRingCount = 0;
static pthread_mutex_t radioRingMutex;
static pthread_t radioNetThreadId;
static volatile bool radioNetRodando = false;
static volatile bool radioPreBuffering = false;

// FFmpeg Contexts
static AVFormatContext* pRadioFormatCtx = NULL;
static AVCodecContext* pRadioCodecCtx = NULL;
static SwrContext* pRadioSwrCtx = NULL;
static int radioStreamIdx = -1;
static uint8_t* radioResampleBuf = NULL;
static AVFrame* pRadioFrame = NULL;
static AVPacket radioPacket;

// Telemetria
static volatile int debugBytesBaixadosThisSec = 0;
static volatile int debugBytesConsumidosThisSec = 0;
static volatile int debugNetSpeedKBps = 0;
static volatile int debugFomeKBps = 0;
static char debugFfmpegStatus[64] = "Aguardando arranque...";

// Globais externas para mensagens (usadas no PS4 UI)
extern char msgStatus[128];
extern int msgTimer;
extern unsigned int msgStatusColor;
extern int httpCtxId;

// --- Implementação ---

static void* radioNetThreadFunc(void* arg) {
    CustomHttpStream* stream = (CustomHttpStream*)arg;
    int errorCount = 0;
    time_t lastTime = time(NULL);

    while (radioNetRodando) {
        time_t currentTime = time(NULL);
        if (currentTime != lastTime) {
            debugNetSpeedKBps = debugBytesBaixadosThisSec / 1024;
            debugFomeKBps = debugBytesConsumidosThisSec / 1024;
            debugBytesBaixadosThisSec = 0;
            debugBytesConsumidosThisSec = 0;
            lastTime = currentTime;
        }

        bool temEspaco = false;
        pthread_mutex_lock(&radioRingMutex);
        if (RADIO_RING_SIZE - radioRingCount > 32768) temEspaco = true;
        pthread_mutex_unlock(&radioRingMutex);

        if (!temEspaco) {
            sceKernelUsleep(10000);
            continue;
        }

        uint8_t temp[32768];
        int n = sceHttpReadData(stream->req, temp, sizeof(temp));

        if (n > 0) {
            errorCount = 0;
            debugBytesBaixadosThisSec += n;

            pthread_mutex_lock(&radioRingMutex);
            if (radioRingWrite + n <= RADIO_RING_SIZE) {
                memcpy(radioRingBuf + radioRingWrite, temp, n);
            }
            else {
                int p1 = RADIO_RING_SIZE - radioRingWrite;
                int p2 = n - p1;
                memcpy(radioRingBuf + radioRingWrite, temp, p1);
                memcpy(radioRingBuf, temp + p1, p2);
            }
            radioRingWrite = (radioRingWrite + n) % RADIO_RING_SIZE;
            radioRingCount += n;

            if (radioPreBuffering && radioRingCount > 262144) {
                radioPreBuffering = false;
            }
            pthread_mutex_unlock(&radioRingMutex);
        }
        else if (n < 0) {
            errorCount++;
            if (errorCount > 50) {
                strcpy(debugFfmpegStatus, "ERRO REDE (Caiu)");
                break;
            }
            sceKernelUsleep(100000);
        }
        else {
            strcpy(debugFfmpegStatus, "SERVIDOR CORTOU");
            break;
        }
    }

    radioNetRodando = false;
    radioPreBuffering = false;
    return NULL;
}

static int read_http_stream(void* opaque, uint8_t* buf, int buf_size) {
    if (!radioNetRodando && radioRingCount == 0) return AVERROR_EOF;

    int bytesRead = 0;
    int retries = 0;

    while (bytesRead == 0) {
        pthread_mutex_lock(&radioRingMutex);

        if (radioRingCount >= buf_size || (!radioNetRodando && radioRingCount > 0)) {
            int toRead = (buf_size < radioRingCount) ? buf_size : radioRingCount;
            if (radioRingRead + toRead <= RADIO_RING_SIZE) {
                memcpy(buf, radioRingBuf + radioRingRead, toRead);
            }
            else {
                int p1 = RADIO_RING_SIZE - radioRingRead;
                int p2 = toRead - p1;
                memcpy(buf, radioRingBuf + radioRingRead, p1);
                memcpy(buf + p1, radioRingBuf, p2);
            }
            radioRingRead = (radioRingRead + toRead) % RADIO_RING_SIZE;
            radioRingCount -= toRead;
            bytesRead = toRead;
        }
        pthread_mutex_unlock(&radioRingMutex);

        if (bytesRead > 0) {
            debugBytesConsumidosThisSec += bytesRead;
            strcpy(debugFfmpegStatus, "TOCANDO LISO");
            return bytesRead;
        }

        if (!radioNetRodando) {
            strcpy(debugFfmpegStatus, "FIM DO ARQUIVO");
            return AVERROR_EOF;
        }

        strcpy(debugFfmpegStatus, "AGUARDANDO REDE...");
        sceKernelUsleep(20000);
        retries++;

        if (retries > 500) {
            strcpy(debugFfmpegStatus, "TIMEOUT DA INTERNET");
            return AVERROR_EOF;
        }
    }

    return bytesRead;
}

void pararRadioStreaming() {
    if (radioNetRodando) {
        radioNetRodando = false;
        pthread_join(radioNetThreadId, NULL);
    }

    if (pRadioSwrCtx) { swr_free(&pRadioSwrCtx); pRadioSwrCtx = NULL; }
    if (pRadioCodecCtx) { avcodec_free_context(&pRadioCodecCtx); pRadioCodecCtx = NULL; }
    if (pRadioFormatCtx) { avformat_close_input(&pRadioFormatCtx); pRadioFormatCtx = NULL; }
    if (pRadioFrame) { av_frame_free(&pRadioFrame); pRadioFrame = NULL; }
    if (radioResampleBuf) { av_free(radioResampleBuf); radioResampleBuf = NULL; }

    if (radioAvioCtx) {
        if (radioAvioCtx->buffer) av_free(radioAvioCtx->buffer);
        av_free(radioAvioCtx);
        radioAvioCtx = NULL;
    }

    if (radioHttpStream) {
        if (radioHttpStream->req >= 0) sceHttpDeleteRequest(radioHttpStream->req);
        if (radioHttpStream->conn >= 0) sceHttpDeleteConnection(radioHttpStream->conn);
        if (radioHttpStream->ownTmpl && radioHttpStream->tmpl >= 0) sceHttpDeleteTemplate(radioHttpStream->tmpl);
        free(radioHttpStream);
        radioHttpStream = NULL;
    }

    radioStreamIdx = -1;
}

bool iniciarRadio(const char* url) {
    pararRadioStreaming();

    radioHttpStream = (CustomHttpStream*)malloc(sizeof(CustomHttpStream));
    radioHttpStream->ownTmpl = true;
    radioHttpStream->tmpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", 1, 1);

    if (radioHttpStream->tmpl < 0) {
        radioHttpStream->tmpl = httpCtxId;
        radioHttpStream->ownTmpl = false;
    }

    radioHttpStream->conn = sceHttpCreateConnectionWithURL(radioHttpStream->tmpl, url, 0);
    radioHttpStream->req = sceHttpCreateRequestWithURL(radioHttpStream->conn, ORBIS_METHOD_GET, url, 0);

    sceHttpAddRequestHeader(radioHttpStream->req, "User-Agent", "HyperNeiva/1.0 (PS4)", 0);
    sceHttpAddRequestHeader(radioHttpStream->req, "Connection", "keep-alive", 0);

    int sendRet = sceHttpSendRequest(radioHttpStream->req, NULL, 0);
    if (sendRet < 0) {
        pararRadioStreaming();
        return false;
    }

    int statusCode = 0;
    sceHttpGetStatusCode(radioHttpStream->req, &statusCode);

    if (statusCode == 200 || statusCode == 206) {
        if (!radioRingBuf) {
            radioRingBuf = (uint8_t*)malloc(RADIO_RING_SIZE);
            pthread_mutex_init(&radioRingMutex, NULL);
        }
        radioRingWrite = 0;
        radioRingRead = 0;
        radioRingCount = 0;
        radioNetRodando = true;
        radioPreBuffering = true;
        debugNetSpeedKBps = 0;
        debugFomeKBps = 0;
        strcpy(debugFfmpegStatus, "Enchendo Pre-Buffer...");

        pthread_create(&radioNetThreadId, NULL, radioNetThreadFunc, radioHttpStream);

        strcpy(msgStatus, "Conectado! Aguarde 2 segundos...");
        msgTimer = 200;
        msgStatusColor = 0xFF00FF00;

        int timeout = 0;
        while (radioPreBuffering && radioNetRodando && timeout < 1000) {
            sceKernelUsleep(10000);
            timeout++;
        }

        uint8_t* avio_buf = (uint8_t*)av_malloc(65536);
        radioAvioCtx = avio_alloc_context(avio_buf, 65536, 0, radioHttpStream, &read_http_stream, NULL, NULL);

        pRadioFormatCtx = avformat_alloc_context();
        pRadioFormatCtx->pb = radioAvioCtx;

        int ret = avformat_open_input(&pRadioFormatCtx, NULL, NULL, NULL);
        if (ret == 0) {
            if (avformat_find_stream_info(pRadioFormatCtx, NULL) >= 0) {
                for (int i = 0; i < pRadioFormatCtx->nb_streams; i++) {
                    if (pRadioFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                        radioStreamIdx = i; break;
                    }
                }
                if (radioStreamIdx != -1) {
                    AVCodecParameters* pParams = pRadioFormatCtx->streams[radioStreamIdx]->codecpar;
                    const AVCodec* pCodec = avcodec_find_decoder(pParams->codec_id);
                    if (pCodec) {
                        pRadioCodecCtx = avcodec_alloc_context3(pCodec);
                        avcodec_parameters_to_context(pRadioCodecCtx, pParams);
                        if (avcodec_open2(pRadioCodecCtx, pCodec, NULL) >= 0) {
                            AVChannelLayout out_layout;
                            av_channel_layout_default(&out_layout, 2);
                            swr_alloc_set_opts2(&pRadioSwrCtx, &out_layout, AV_SAMPLE_FMT_S16, 48000,
                                &pRadioCodecCtx->ch_layout, pRadioCodecCtx->sample_fmt, pRadioCodecCtx->sample_rate, 0, NULL);
                            swr_init(pRadioSwrCtx);
                            pRadioFrame = av_frame_alloc();
                            radioResampleBuf = (uint8_t*)av_malloc(192000);
                            return true;
                        }
                    }
                }
            }
        }
    }

    pararRadioStreaming();
    return false;
}

size_t lerFrameRadio(int16_t* outSamples, size_t frameCount) {
    if (!pRadioFormatCtx || radioStreamIdx == -1) return 0;

    int ret_read = av_read_frame(pRadioFormatCtx, &radioPacket);
    if (ret_read >= 0) {
        if (radioPacket.stream_index == radioStreamIdx) {
            if (avcodec_send_packet(pRadioCodecCtx, &radioPacket) == 0) {
                if (avcodec_receive_frame(pRadioCodecCtx, pRadioFrame) == 0) {
                    int samples = swr_convert(pRadioSwrCtx, &radioResampleBuf, frameCount, (const uint8_t**)pRadioFrame->data, pRadioFrame->nb_samples);
                    if (samples > 0) {
                        memcpy(outSamples, radioResampleBuf, samples * 4);
                        av_packet_unref(&radioPacket);
                        return samples;
                    }
                }
            }
        }
        av_packet_unref(&radioPacket);
    }
    return 0;
}

void obterTelemetriaRadio(char* outMsg, size_t size) {
    if (radioNetRodando) {
        snprintf(outMsg, size, "Net:%dKB/s | Fome:%dKB/s | RAM:%dKB | %s", debugNetSpeedKBps, debugFomeKBps, radioRingCount / 1024, debugFfmpegStatus);
    } else {
        snprintf(outMsg, size, "Radio Desconectada");
    }
}

bool isRadioRodando() {
    return radioNetRodando;
}
