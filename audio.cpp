#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h> 

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list va_list
#endif
#endif

#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#include <orbis/Http.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include "audio.h"
#include "explorar.h" 
#include "elementos_sonoros.h" 
#include "instrumentos.h" 
#include "video.h"        

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

static int audioPort = -1;
static volatile bool audioRodando = false;
static pthread_t audioThreadId;
static bool sistemaAudioIniciado = false;

static volatile bool comandoTrocar = false;
volatile bool comandoPausar = false;

volatile int modoReproducao = 0;

volatile int comandoBuscarSegundos = 0;
int volumeGeral = 100;
char musicaAtual[256] = "PARADO";
char ultimaMusicaTocada[256] = "";

char caminhosMusicasMenu[3000][256];
char caminhoNavegacaoMusicas[512] = "/data/HyperNeiva/Musicas";

volatile float audioTempoAtual = 0.0f;

#define EMU_AUDIO_BUFFER_SIZE 4096
static int16_t emuAudioBuffer[EMU_AUDIO_BUFFER_SIZE * 2];
static volatile int emuAudioReadIdx = 0;
static volatile int emuAudioWriteIdx = 0;

void enviarAmostraAudio(int16_t L, int16_t R) {
    int nextWrite = (emuAudioWriteIdx + 1) % EMU_AUDIO_BUFFER_SIZE;
    if (nextWrite != emuAudioReadIdx) {
        emuAudioBuffer[emuAudioWriteIdx * 2] = L;
        emuAudioBuffer[emuAudioWriteIdx * 2 + 1] = R;
        emuAudioWriteIdx = nextWrite;
    }
}

static void misturarAudioEmulador(int16_t* pSamples, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        if (emuAudioReadIdx != emuAudioWriteIdx) {
            int16_t eL = emuAudioBuffer[emuAudioReadIdx * 2];
            int16_t eR = emuAudioBuffer[emuAudioReadIdx * 2 + 1];

            int32_t mixL = pSamples[i * 2] + eL;
            int32_t mixR = pSamples[i * 2 + 1] + eR;

            if (mixL > 32767) mixL = 32767; else if (mixL < -32768) mixL = -32768;
            if (mixR > 32767) mixR = 32767; else if (mixR < -32768) mixR = -32768;

            pSamples[i * 2] = (int16_t)mixL;
            pSamples[i * 2 + 1] = (int16_t)mixR;

            emuAudioReadIdx = (emuAudioReadIdx + 1) % EMU_AUDIO_BUFFER_SIZE;
        }
    }
}

// =========================================================================
// O TUBO VIRTUAL DE REDE: LEITURA BLOQUEANTE (PAUSA FORÇADA DO FFMPEG)
// =========================================================================
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

// Telemetria Dupla
static volatile int debugBytesBaixadosThisSec = 0;
static volatile int debugBytesConsumidosThisSec = 0;
static volatile int debugNetSpeedKBps = 0;
static volatile int debugFomeKBps = 0;
static char debugFfmpegStatus[64] = "Aguardando arranque...";

// THREAD 1: O MOTO-BOY DA INTERNET (Enche a RAM)
void* radioNetThreadFunc(void* arg) {
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

            // O FFmpeg arranca assim que tiver 256KB
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

// THREAD 2: O CLIENTE EXIGENTE (O FFmpeg pede áudio aqui)
static int read_http_stream(void* opaque, uint8_t* buf, int buf_size) {
    if (!radioNetRodando && radioRingCount == 0) return AVERROR_EOF;

    int bytesRead = 0;
    int retries = 0;

    // O SEGREDO MÁXIMO: O loop agora é BLOQUEANTE. Não deixamos o FFmpeg sair daqui sem áudio!
    while (bytesRead == 0) {
        pthread_mutex_lock(&radioRingMutex);

        // Só entregamos áudio ao FFmpeg se a RAM tiver dados suficientes para ele não se engasgar
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
            return bytesRead; // Sucesso! O FFmpeg recebe a comida.
        }

        if (!radioNetRodando) {
            strcpy(debugFfmpegStatus, "FIM DO ARQUIVO");
            return AVERROR_EOF; // A internet caiu de vez e a RAM secou.
        }

        // SE A RAM ESTIVER VAZIA: Nós prendemos o FFmpeg aqui a dormir! (Não devolvemos EAGAIN)
        strcpy(debugFfmpegStatus, "AGUARDANDO REDE...");
        sceKernelUsleep(20000); // Adormece 20 milissegundos
        retries++;

        if (retries > 500) { // Ficou 10 SEGUNDOS à espera da internet? Desiste.
            strcpy(debugFfmpegStatus, "TIMEOUT DA INTERNET");
            return AVERROR_EOF;
        }
    }

    return bytesRead;
}
// =========================================================================

static AVFormatContext* pRadioFormatCtx = NULL;
static AVCodecContext* pRadioCodecCtx = NULL;
static SwrContext* pRadioSwrCtx = NULL;
static int radioStreamIdx = -1;
static uint8_t* radioResampleBuf = NULL;
static AVFrame* pRadioFrame = NULL;
static AVPacket radioPacket;

static AudioType currentAudioType = AUDIO_NONE;

void adiantarAudio() { comandoBuscarSegundos = 10; }
void retrocederAudio() { comandoBuscarSegundos = -10; }

void aumentarVolume() {
    volumeGeral += 10;
    if (volumeGeral > 100) volumeGeral = 100;
    salvarConfiguracaoAudio();
}

void diminuirVolume() {
    volumeGeral -= 10;
    if (volumeGeral < 0) volumeGeral = 0;
    salvarConfiguracaoAudio();
}

void salvarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "wb");
    if (f) {
        fwrite(musicaAtual, 1, sizeof(musicaAtual), f);
        fwrite(&volumeGeral, 1, sizeof(int), f);
        int modoSalvar = (int)modoReproducao;
        fwrite(&modoSalvar, 1, sizeof(int), f);
        fwrite(ultimaMusicaTocada, 1, sizeof(ultimaMusicaTocada), f);

        int pausado = comandoPausar ? 1 : 0;
        fwrite(&pausado, 1, sizeof(int), f);

        fclose(f);
    }
}

void carregarConfiguracaoAudio() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "rb");
    if (f) {
        if (fread(ultimaMusicaTocada, 1, sizeof(ultimaMusicaTocada), f) <= 0) {
            strcpy(ultimaMusicaTocada, "");
            if (strcmp(musicaAtual, "PARADO") != 0) strcpy(ultimaMusicaTocada, musicaAtual);
        }

        int pausado = 0;
        if (fread(&pausado, 1, sizeof(int), f) > 0) {
            comandoPausar = (pausado == 1);
        }
        else {
            comandoPausar = false;
        }

        fclose(f);
    }
    else {
        volumeGeral = 100;
        modoReproducao = 0;
        strcpy(ultimaMusicaTocada, "");
        comandoPausar = false;
    }
}

#define MAX_PLAYLIST 2000

void scanPlaylistRecursivo(const char* basePath, char (*lista)[256], int* total) {
    DIR* d = opendir(basePath);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && *total < MAX_PLAYLIST) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, dir->d_name);
        struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
            scanPlaylistRecursivo(fullPath, lista, total);
        }
        else {
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") || strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                strncpy(lista[*total], fullPath, 255); lista[*total][255] = '\0'; (*total)++;
            }
        }
    }
    closedir(d);
}

void scanPastaSimples(const char* basePath, char (*lista)[256], int* total) {
    DIR* d = opendir(basePath);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && *total < MAX_PLAYLIST) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, dir->d_name);
        struct stat st;
        if (dir->d_type != DT_DIR && (dir->d_type != DT_UNKNOWN || (stat(fullPath, &st) == 0 && !S_ISDIR(st.st_mode)))) {
            if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") || strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                strncpy(lista[*total], fullPath, 255); lista[*total][255] = '\0'; (*total)++;
            }
        }
    }
    closedir(d);
}

static bool obterProximaMusica(char* proximaMusicaPath) {
    char (*listaAudios)[256] = (char (*)[256])malloc(MAX_PLAYLIST * 256);
    if (!listaAudios) return false;
    int totalAudios = 0;
    if (modoReproducao == 2 || modoReproducao == 3) {
        char pastaAtual[512]; strcpy(pastaAtual, musicaAtual);
        char* lastSlash = strrchr(pastaAtual, '/');
        if (lastSlash) { *lastSlash = '\0'; scanPastaSimples(pastaAtual, listaAudios, &totalAudios); }
    }
    if (totalAudios == 0) scanPlaylistRecursivo("/data/HyperNeiva/Musicas", listaAudios, &totalAudios);
    if (totalAudios == 0) { free(listaAudios); return false; }
    if (modoReproducao == 3 || modoReproducao == 4) {
        int r = rand() % totalAudios;
        if (totalAudios > 1 && strcmp(listaAudios[r], musicaAtual) == 0) r = (r + 1) % totalAudios;
        strcpy(proximaMusicaPath, listaAudios[r]);
    }
    else {
        for (int i = 0; i < totalAudios - 1; i++) {
            for (int j = i + 1; j < totalAudios; j++) {
                if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                    char temp[256]; strcpy(temp, listaAudios[i]); strcpy(listaAudios[i], listaAudios[j]); strcpy(listaAudios[j], temp);
                }
            }
        }
        int idx = -1;
        for (int i = 0; i < totalAudios; i++) { if (strcmp(listaAudios[i], musicaAtual) == 0) { idx = i; break; } }
        if (idx != -1 && idx + 1 < totalAudios) strcpy(proximaMusicaPath, listaAudios[idx + 1]);
        else strcpy(proximaMusicaPath, listaAudios[0]);
    }
    free(listaAudios); return true;
}

static bool obterMusicaAnterior(char* musicaAnteriorPath) {
    char (*listaAudios)[256] = (char (*)[256])malloc(MAX_PLAYLIST * 256);
    if (!listaAudios) return false;
    int totalAudios = 0;
    if (modoReproducao == 2 || modoReproducao == 3) {
        char pastaAtual[512]; strcpy(pastaAtual, musicaAtual);
        char* lastSlash = strrchr(pastaAtual, '/');
        if (lastSlash) { *lastSlash = '\0'; scanPastaSimples(pastaAtual, listaAudios, &totalAudios); }
    }
    if (totalAudios == 0) scanPlaylistRecursivo("/data/HyperNeiva/Musicas", listaAudios, &totalAudios);
    if (totalAudios == 0) { free(listaAudios); return false; }
    if (modoReproducao == 3 || modoReproducao == 4) {
        int r = rand() % totalAudios;
        if (totalAudios > 1 && strcmp(listaAudios[r], musicaAtual) == 0) { r = (r + 1) % totalAudios; }
        strcpy(musicaAnteriorPath, listaAudios[r]);
    }
    else {
        for (int i = 0; i < totalAudios - 1; i++) {
            for (int j = i + 1; j < totalAudios; j++) {
                if (strcasecmp(listaAudios[i], listaAudios[j]) > 0) {
                    char temp[256]; strcpy(temp, listaAudios[i]); strcpy(listaAudios[i], listaAudios[j]); strcpy(listaAudios[j], temp);
                }
            }
        }
        int idx = -1;
        for (int i = 0; i < totalAudios; i++) { if (strcmp(listaAudios[i], musicaAtual) == 0) { idx = i; break; } }
        if (idx != -1 && idx - 1 >= 0) strcpy(musicaAnteriorPath, listaAudios[idx - 1]);
        else strcpy(musicaAnteriorPath, listaAudios[totalAudios - 1]);
    }
    free(listaAudios); return true;
}

static bool prepararArquivoAudio(char* caminhoFinal) {
    if (strcmp(musicaAtual, "PARADO") == 0) return false;

    if (strncmp(musicaAtual, "http", 4) == 0) {
        strcpy(caminhoFinal, musicaAtual);
        return true;
    }

    if (strlen(musicaAtual) > 0) {
        FILE* fCustom = fopen(musicaAtual, "rb");
        if (fCustom) { fclose(fCustom); strcpy(caminhoFinal, musicaAtual); return true; }
    }
    const char* pathHD = "/data/HyperNeiva/configuracao/bgm.wav";
    FILE* fHD = fopen(pathHD, "rb");
    if (fHD) { fclose(fHD); strcpy(caminhoFinal, pathHD); return true; }

    const char* pathInterno = "/app0/assets/audio/bgm.wav";
    FILE* fInt = fopen(pathInterno, "rb");
    if (!fInt) return false;

    FILE* fOut = fopen(pathHD, "wb");
    if (!fOut) { fclose(fInt); strcpy(caminhoFinal, pathInterno); return true; }

    char buffer[8192];
    size_t bytesLidos;
    while ((bytesLidos = fread(buffer, 1, sizeof(buffer), fInt)) > 0) fwrite(buffer, 1, bytesLidos, fOut);
    fclose(fInt); fclose(fOut);

    strcpy(caminhoFinal, pathHD);
    return true;
}

static void limparStreamingRadio() {
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

static bool iniciarRadioCustomAVIO(const char* url) {
    extern int httpCtxId;

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
        limparStreamingRadio();
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

        extern char msgStatus[128];
        extern int msgTimer;
        extern unsigned int msgStatusColor;
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

    limparStreamingRadio();
    return false;
}

static void* audioThreadFunc(void* argp) {
    if (!sistemaAudioIniciado) { sceAudioOutInit(); sistemaAudioIniciado = true; }
    int32_t userId;
    if (sceUserServiceGetInitialUser(&userId) < 0) userId = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
    audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (audioPort < 0) { audioRodando = false; return NULL; }

    drwav wav; drmp3 mp3;
    int16_t pSampleData[256 * 2];
    char caminhoAudio[256];
    uint64_t currentFrame = 0;

    if (!comandoPausar && prepararArquivoAudio(caminhoAudio)) {
        if (strncmp(caminhoAudio, "http", 4) == 0) {
            if (iniciarRadioCustomAVIO(caminhoAudio)) {
                currentAudioType = AUDIO_STREAM;
            }
            else {
                extern char msgStatus[128]; extern int msgTimer; extern unsigned int msgStatusColor;
                snprintf(msgStatus, sizeof(msgStatus), "Erro ao conectar no servidor da Radio.");
                msgTimer = 400; msgStatusColor = 0xFF0000FF;
            }
        }
        else if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
            if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
        }
        else {
            if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
        }
    }

    while (audioRodando) {
        if (audioPort < 0) {
            audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        }

        // TELEMETRIA: Observa a Net a baixar e o FFmpeg a "comer"
        if (currentAudioType == AUDIO_STREAM && radioNetRodando) {
            extern char msgStatus[128];
            extern int msgTimer;
            extern unsigned int msgStatusColor;

            snprintf(msgStatus, sizeof(msgStatus), "Net:%dKB/s | Fome:%dKB/s | RAM:%dKB | %s", debugNetSpeedKBps, debugFomeKBps, radioRingCount / 1024, debugFfmpegStatus);
            msgTimer = 100;
            msgStatusColor = 0xFF00FF00;
        }

        if (comandoBuscarSegundos != 0) {
            if (currentAudioType != AUDIO_NONE) {
                int sampleRate = (currentAudioType == AUDIO_WAV) ? wav.sampleRate : mp3.sampleRate;
                int64_t frameOffset = (int64_t)comandoBuscarSegundos * sampleRate;
                int64_t targetFrame = (int64_t)currentFrame + frameOffset;

                if (targetFrame < 0) targetFrame = 0;

                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, (uint64_t)targetFrame);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, (uint64_t)targetFrame);
                currentFrame = (uint64_t)targetFrame;

                if (currentAudioType == AUDIO_WAV) audioTempoAtual = (float)currentFrame / wav.sampleRate;
                else if (currentAudioType == AUDIO_MP3) audioTempoAtual = (float)currentFrame / mp3.sampleRate;
            }
            comandoBuscarSegundos = 0;
        }

        if (comandoTrocar) {
            comandoTrocar = false;
            if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
            else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
            else if (currentAudioType == AUDIO_STREAM) limparStreamingRadio();
            currentAudioType = AUDIO_NONE;

            if (prepararArquivoAudio(caminhoAudio)) {
                if (strncmp(caminhoAudio, "http", 4) == 0) {
                    if (iniciarRadioCustomAVIO(caminhoAudio)) {
                        currentAudioType = AUDIO_STREAM;
                    }
                    else {
                        strcpy(musicaAtual, "PARADO");
                        comandoPausar = true;
                        limparStreamingRadio();
                    }
                }
                else if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
                    if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
                }
                else {
                    if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
                }
            }
            currentFrame = 0;
            audioTempoAtual = 0.0f;
        }

        if (comandoPausar || currentAudioType == AUDIO_NONE) {
            for (int i = 0; i < 256 * 2; i++) pSampleData[i] = 0;
            misturarEfeitosSonoros(pSampleData, 256);
            misturarAudioPiano(pSampleData, 256);
            misturarAudioVideo(pSampleData, 256);
            misturarAudioEmulador(pSampleData, 256);
            if (audioPort >= 0) sceAudioOutOutput(audioPort, pSampleData);
            continue;
        }

        size_t framesLidos = 0;
        uint32_t currentChannels = 2;

        if (currentAudioType == AUDIO_WAV) {
            framesLidos = drwav_read_pcm_frames_s16(&wav, 256, pSampleData);
            currentChannels = wav.channels;
        }
        else if (currentAudioType == AUDIO_MP3) {
            framesLidos = drmp3_read_pcm_frames_s16(&mp3, 256, pSampleData);
            currentChannels = mp3.channels;
        }
        else if (currentAudioType == AUDIO_STREAM) {
            int ret_read = av_read_frame(pRadioFormatCtx, &radioPacket);
            if (ret_read >= 0) {
                if (radioPacket.stream_index == radioStreamIdx) {
                    if (avcodec_send_packet(pRadioCodecCtx, &radioPacket) == 0) {
                        if (avcodec_receive_frame(pRadioCodecCtx, pRadioFrame) == 0) {
                            int samples = swr_convert(pRadioSwrCtx, &radioResampleBuf, 256, (const uint8_t**)pRadioFrame->data, pRadioFrame->nb_samples);
                            if (samples > 0) {
                                memcpy(pSampleData, radioResampleBuf, samples * 4);
                                framesLidos = samples;
                            }
                        }
                    }
                }
                av_packet_unref(&radioPacket);
            }
            else {
                // Erro fatal real (EOF)
                strcpy(musicaAtual, "PARADO");
                comandoPausar = true;
            }
            currentChannels = 2;
        }

        if (framesLidos > 0 && currentChannels == 1) {
            for (int i = framesLidos - 1; i >= 0; i--) {
                pSampleData[i * 2] = pSampleData[i];
                pSampleData[i * 2 + 1] = pSampleData[i];
            }
        }

        currentFrame += framesLidos;

        if (currentAudioType == AUDIO_WAV) {
            audioTempoAtual = (float)currentFrame / wav.sampleRate;
        }
        else if (currentAudioType == AUDIO_MP3) {
            audioTempoAtual = (float)currentFrame / mp3.sampleRate;
        }

        if (framesLidos == 0) {
            if (modoReproducao == 1) {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
                audioTempoAtual = 0.0f;
            }
            else if (strstr(musicaAtual, "/data/HyperNeiva/Musicas/") != NULL) {
                char proxima[256];
                if (obterProximaMusica(proxima)) {
                    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
                    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
                    currentAudioType = AUDIO_NONE;

                    strcpy(musicaAtual, proxima);
                    salvarConfiguracaoAudio();

                    bool sucesso = false;
                    if (strstr(musicaAtual, ".mp3") || strstr(musicaAtual, ".MP3")) {
                        if (drmp3_init_file(&mp3, musicaAtual, NULL)) { currentAudioType = AUDIO_MP3; sucesso = true; }
                    }
                    else {
                        if (drwav_init_file(&wav, musicaAtual, NULL)) { currentAudioType = AUDIO_WAV; sucesso = true; }
                    }
                    if (sucesso) {
                        currentFrame = 0;
                        audioTempoAtual = 0.0f;
                        continue;
                    }
                }
            }
            else {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
                audioTempoAtual = 0.0f;
            }
            continue;
        }

        if (framesLidos < 256) {
            for (size_t i = framesLidos * 2; i < 256 * 2; i++) pSampleData[i] = 0;
        }

        if (volumeGeral < 100) {
            float fatorVolume = volumeGeral / 100.0f;
            for (int i = 0; i < 256 * 2; i++) {
                pSampleData[i] = (int16_t)(pSampleData[i] * fatorVolume);
            }
        }

        misturarEfeitosSonoros(pSampleData, 256);
        misturarAudioPiano(pSampleData, 256);
        misturarAudioVideo(pSampleData, 256);
        misturarAudioEmulador(pSampleData, 256);
        if (audioPort >= 0) sceAudioOutOutput(audioPort, pSampleData);
    }

    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
    else if (currentAudioType == AUDIO_STREAM) limparStreamingRadio();
    sceAudioOutClose(audioPort);
    return NULL;
}

void inicializarAudio() {
    if (audioRodando) return;
    srand(time(NULL));
    avformat_network_init();

    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
    carregarConfiguracaoAudio();
    audioRodando = true;

    if (strcmp(musicaAtual, "PARADO") == 0) comandoPausar = true;

    comandoTrocar = false;
    if (pthread_create(&audioThreadId, NULL, audioThreadFunc, NULL) != 0) audioRodando = false;
}

void pararAudio() {
    if (!audioRodando) return;
    audioRodando = false;
    pthread_join(audioThreadId, NULL);
}

void tocarMusicaNova(const char* path) {
    if (strcmp(path, "PARADO") == 0) {
        strcpy(musicaAtual, "PARADO");
        salvarConfiguracaoAudio();
        comandoPausar = true;
        comandoTrocar = false;
        return;
    }
    if (strncmp(path, "http", 4) == 0) {
        extern char msgStatus[128];
        extern int msgTimer;
        extern unsigned int msgStatusColor;
        strcpy(msgStatus, "Conectando ao Servidor...");
        msgTimer = 200;
        msgStatusColor = 0xFF00FF00;
    }
    strcpy(musicaAtual, path);
    strcpy(ultimaMusicaTocada, path);
    salvarConfiguracaoAudio();
    comandoPausar = false;
    comandoTrocar = true;
}

void tocarProximaMusica() {
    bool estavaParado = false;
    if (strcmp(musicaAtual, "PARADO") == 0) {
        if (strlen(ultimaMusicaTocada) == 0) return;
        strcpy(musicaAtual, ultimaMusicaTocada);
        estavaParado = true;
    }

    char proxima[256];
    if (obterProximaMusica(proxima)) {
        tocarMusicaNova(proxima);
    }
    else if (estavaParado) {
        strcpy(musicaAtual, "PARADO");
    }
}

void tocarMusicaAnterior() {
    bool estavaParado = false;
    if (strcmp(musicaAtual, "PARADO") == 0) {
        if (strlen(ultimaMusicaTocada) == 0) return;
        strcpy(musicaAtual, ultimaMusicaTocada);
        estavaParado = true;
    }

    char anterior[256];
    if (obterMusicaAnterior(anterior)) {
        tocarMusicaNova(anterior);
    }
    else if (estavaParado) {
        strcpy(musicaAtual, "PARADO");
    }
}

struct ItemAudioTemp {
    char nome[64];
    char path[256];
    bool ehPasta;
};

void preencherMenuMusicas() {
    memset(nomes, 0, sizeof(nomes));
    memset(caminhosMusicasMenu, 0, sizeof(caminhosMusicasMenu));
    totalItens = 0;

    if (strcmp(caminhoNavegacaoMusicas, "/data/HyperNeiva/Musicas") == 0) {
        strcpy(nomes[totalItens], "PARAR MUSICA");
        strcpy(caminhosMusicasMenu[totalItens], "PARADO");
        totalItens++;
    }

    struct ItemAudioTemp temp[3000];
    int count = 0;

    DIR* d = opendir(caminhoNavegacaoMusicas);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && count < 2900) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", caminhoNavegacaoMusicas, dir->d_name);

            struct stat st;

            if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
                strncpy(temp[count].nome, dir->d_name, 63);
                temp[count].nome[63] = '\0';
                temp[count].ehPasta = true;
                snprintf(temp[count].path, 255, "%s", fullPath);
                count++;
            }
            else {
                if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") ||
                    strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3")) {
                    strncpy(temp[count].nome, dir->d_name, 63);
                    temp[count].nome[63] = '\0';
                    temp[count].ehPasta = false;
                    snprintf(temp[count].path, 255, "%s", fullPath);
                    count++;
                }
            }
        }
        closedir(d);
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            bool trocar = false;
            if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
            else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
            if (trocar) { ItemAudioTemp aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
        }
    }

    for (int i = 0; i < count; i++) {
        if (temp[i].ehPasta) snprintf(nomes[totalItens], 64, "[%s]", temp[i].nome);
        else strcpy(nomes[totalItens], temp[i].nome);
        strcpy(caminhosMusicasMenu[totalItens], temp[i].path);
        totalItens++;
    }

    menuAtual = MENU_MUSICAS;
}