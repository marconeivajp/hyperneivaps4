#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <emmintrin.h>
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
#include <libavutil/error.h>
}

#include "video.h"
#include "graphics.h"
#include "audio.h"

extern int32_t global_uId;
extern char msgStatus[128];
extern int msgTimer;
extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
extern void desenharMenuPlayerVideo(uint32_t* p);
extern bool menu_video_aberto, pip_selecionado;

// ==========================================
// VARIÁVEIS GLOBAIS DO VÍDEO (Declaradas cedo para a IA enxergar)
// ==========================================
bool videoRodando = false, video_pausado = false, video_minimizado = false;
static bool video_pausado_por_tela = false;
bool bloqueio_audio_nativo = false;
char ultimo_video_tocado[512] = "";
char path_pasta_video[512] = "";
double video_time_current = 0;
double audio_time_current = 0;
double video_time_total = 0;
double video_fps_multiplier = 1.0;

// ==========================================
// SISTEMA DE LOG DE PERFORMANCE (V2)
// ==========================================
#define LOG_VERSION "v2"
#define LOG_PATH "/data/HyperNeiva/video_debug_log.txt"
#define LOG_PATH_ANTIGO "/data/HyperNeiva/video_debug_log.old.txt"
#define LOG_TAMANHO_MAXIMO (4 * 1024 * 1024)

void logVideoInfo(const char* formato, ...) {
    FILE* f = fopen(LOG_PATH, "a");
    if (!f) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", t);

    fprintf(f, "[%s] %s ", LOG_VERSION, time_str);

    va_list args;
    va_start(args, formato);
    vfprintf(f, formato, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}

static void rotacionarLogVideo() {
    struct stat info;
    if (stat(LOG_PATH, &info) == 0 && info.st_size > LOG_TAMANHO_MAXIMO) {
        remove(LOG_PATH_ANTIGO);
        rename(LOG_PATH, LOG_PATH_ANTIGO);
    }
}

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
bool mostrar_hud_perf = false;
int frame_skip = 0, video_volume = 100, comando_trocar_video = 0;
int pip_x = 110, pip_y = 90, pip_w = 768, pip_h = 432;
int total_audios = 0, total_legendas = 0;
double fps_video = 24.0, tempo_pulo_alvo = 0;

bool mostrar_timeline = false;
uint32_t thumb_cache[10][192 * 108];
bool thumb_ready[10];

bool bloqueio_fechar_video = false;

struct TrackInfo { char nome[64]; };
TrackInfo audios_encontrados[10], legendas_encontradas[10];

struct VideoMarker { char nome[128]; double inicio; double fim; };
VideoMarker markers[50];
int total_markers = 0;

void pularParaTempo(double tempo);

// ==========================================
// MOTOR DE VÍDEO: VISÃO GERAL
// ------------------------------------------
// Cada etapa roda na sua própria thread, para uma etapa lenta nunca travar as outras:
//
//   demux --> filaVideo --> decodificação (N threads do FFmpeg) --> filaFrames --> apresentador
//         --> filaAudio --> áudio --> anel --> misturarAudioVideo (thread de som do app)
//
// O áudio é o relógio mestre. Se o vídeo atrasar ele descarta quadros em vez de andar em
// câmera lenta. O apresentador é sincronizado com o vsync e desenha sem depender do loop
// principal, então um menu pesado no PiP não atrasa o vídeo. Tela cheia nem desenha o menu.
// ==========================================
#define AUDIO_TAXA 48000
#define VIDEO_RING_BUFFER_SIZE (AUDIO_TAXA * 2 * 4)
#define AUDIO_RING_ALVO (AUDIO_TAXA * 2)            // ~1 s de áudio à frente, no máximo
#define AUDIO_BUFFER_FRAMES AUDIO_TAXA

#define FILA_VIDEO_CAP 1024
#define FILA_AUDIO_CAP 2048
#define FILA_VIDEO_MAX_BYTES (32 * 1024 * 1024)
#define FILA_FRAMES_CAP 8

#define PILHA_THREAD (2 * 1024 * 1024)
#define PRIORIDADE_APRESENTADOR 400                  // 256 = máxima, 767 = mínima, 700 = padrão
#define THREADS_CONVERSAO 3

#define ESPERA_REDUCAO_INICIAL_US 4000000ULL
#define ESPERA_REDUCAO_MAXIMA_US 64000000ULL

// Variáveis escritas pelo loop principal e lidas aqui nas outras threads: a leitura precisa ir
// mesmo à memória, senão o otimizador (LTO) pode mantê-las fixas dentro do laço da thread.
template <typename T>
static inline T lerCompartilhado(const T& valor) { return *(const volatile T*)&valor; }

static AVFormatContext* pFormatCtx = NULL;
static AVCodecContext* pCodecCtx = NULL, * aCodecCtx = NULL;
static SwrContext* swr_ctx = NULL;
static uint8_t* audio_out_buffer = NULL;
static int videoStream = -1, audioStream = -1;
static bool temAudio = false;
static AVRational tbVideo = { 1, 1 }, tbAudio = { 1, 1 };
static double tempoBaseArquivo = 0.0;
static double duracaoFrame = 1.0 / 24.0;

// ---- Threads ----
static pthread_t thDemux, thVideo, thAudio, thApresentador;
static bool thDemuxViva = false, thVideoViva = false, thAudioViva = false, thApresentadorViva = false;
static volatile bool pararThreads = false;

// ---- Busca: cada seek/repetição muda o serial, e tudo com serial antigo é jogado fora ----
static volatile int serialAtual = 1;
static volatile bool seekPedido = false;
static volatile double seekAlvo = 0.0;

// ---- Filas de pacotes (um produtor, um consumidor, sem trava) ----
enum { ENTRADA_PACOTE = 0, ENTRADA_FLUSH = 1, ENTRADA_FIM = 2 };
struct EntradaPacote { AVPacket* pkt; int serial; int tipo; double alvo; };
struct FilaPacotes {
    EntradaPacote* itens;
    int capacidade;
    int cabeca;
    int cauda;
    int64_t bytes;
};
static FilaPacotes filaVideo = { NULL, 0, 0, 0, 0 }, filaAudio = { NULL, 0, 0, 0, 0 };

static bool filaPacotesPush(FilaPacotes* f, const EntradaPacote& e) {
    int cauda = __atomic_load_n(&f->cauda, __ATOMIC_RELAXED);
    int proximo = (cauda + 1) % f->capacidade;
    if (proximo == __atomic_load_n(&f->cabeca, __ATOMIC_ACQUIRE)) return false;
    f->itens[cauda] = e;
    if (e.pkt) __atomic_add_fetch(&f->bytes, (int64_t)e.pkt->size, __ATOMIC_RELAXED);
    __atomic_store_n(&f->cauda, proximo, __ATOMIC_RELEASE);
    return true;
}

static bool filaPacotesPop(FilaPacotes* f, EntradaPacote* e) {
    int cabeca = __atomic_load_n(&f->cabeca, __ATOMIC_RELAXED);
    if (cabeca == __atomic_load_n(&f->cauda, __ATOMIC_ACQUIRE)) return false;
    *e = f->itens[cabeca];
    if (e->pkt) __atomic_sub_fetch(&f->bytes, (int64_t)e->pkt->size, __ATOMIC_RELAXED);
    __atomic_store_n(&f->cabeca, (cabeca + 1) % f->capacidade, __ATOMIC_RELEASE);
    return true;
}

static int filaPacotesTamanho(FilaPacotes* f) {
    int tamanho = __atomic_load_n(&f->cauda, __ATOMIC_ACQUIRE) - __atomic_load_n(&f->cabeca, __ATOMIC_ACQUIRE);
    return tamanho < 0 ? tamanho + f->capacidade : tamanho;
}

static bool filaPacotesCheia(FilaPacotes* f) { return filaPacotesTamanho(f) >= f->capacidade - 2; }

static void filaPacotesEsvaziar(FilaPacotes* f) {
    if (!f->itens) return;
    EntradaPacote e;
    while (filaPacotesPop(f, &e)) { if (e.pkt) av_packet_free(&e.pkt); }
}

// ---- Fila de quadros decodificados (ainda em YUV: a conversão só acontece se forem exibidos) ----
struct EntradaFrame { AVFrame* frame; double pts; int serial; uint32_t numero; };
static EntradaFrame filaFrames[FILA_FRAMES_CAP];
static int filaFramesCabeca = 0, filaFramesCauda = 0;
static uint32_t proximoNumeroFrame = 1;

static int filaFramesTamanho() {
    int tamanho = __atomic_load_n(&filaFramesCauda, __ATOMIC_ACQUIRE) - __atomic_load_n(&filaFramesCabeca, __ATOMIC_ACQUIRE);
    return tamanho < 0 ? tamanho + FILA_FRAMES_CAP : tamanho;
}

// ---- Anel de áudio ----
static int16_t video_audio_ring[VIDEO_RING_BUFFER_SIZE];
static volatile int video_ring_write_pos = 0;
static volatile int video_ring_read_pos = 0;
static volatile int64_t audioEscritas = 0;           // amostras por canal gravadas no anel
static volatile int64_t audioTocadas = 0;            // amostras por canal entregues à saída de som
static volatile double audioAncora = 0.0;            // tempo do áudio = audioAncora + audioTocadas / taxa
static volatile bool audioAncoraValida = false;
static volatile bool audioLiberado = false;          // só toca depois do primeiro quadro aparecer
static volatile bool audioFlushPedido = false;

// ---- Relógio ----
static volatile double relogioVideo = 0.0;
static volatile int serialRelogio = 0;               // relógio em uso quando == serialAtual
static uint64_t relogioUltimoUs = 0;
static int64_t relogioTocadasAnterior = -1;
static uint64_t relogioTocadasUs = 0;
static double multiplicadorAnterior = 1.0;
static bool relogioAlinharAudio = false;             // primeiro ajuste depois de abrir ou buscar

// ---- Decodificação ----
static int videoSerial = 1;
static double videoDescartarAte = 0.0;
static double videoUltimoPts = 0.0;
static uint64_t videoUltimaEntregaUs = 0;
static volatile bool videoDrenado = false;
static volatile int nivelQualidade = 0;
static double atrasoSuave = 0.0;
static uint64_t ultimaMudancaNivelUs = 0, ultimaReducaoNivelUs = 0;
static uint64_t esperaReducaoNivelUs = ESPERA_REDUCAO_INICIAL_US;
static volatile bool reiniciarQualidade = false;

// ---- Apresentador ----
static volatile bool apresentadorAtivo = false;
static volatile bool apresentadorSair = false;
static int apresentadorEmPasso = 0;
static AVFrame* frameAtual = NULL;
static double ptsAtual = 0.0;
static uint32_t numeroFrameAtual = 0, numeroFrameAdiantado = 0;
static uint32_t* camadaAtual = NULL;
static uint64_t versaoCamada = 0;
static uint64_t carimboFundo[2] = { 0, 0 };
static uint32_t apresentadorTick = 0;
static int ultimoDw = 0, ultimoDh = 0, ultimoDx = 0, ultimoDy = 0;
static bool ultimoPip = false, ultimoOverlay = false;

struct ConversorVideo {
    struct SwsContext* ctxEscala;   // etapa 1: redimensiona em YUV (só quando precisa)
    struct SwsContext* ctxCor;      // etapa 2: YUV 4:2:0 -> BGRA pelo caminho SIMD do swscale
    AVFrame* intermediario;
    AVFrame* imagens[2];            // uma em exibição e outra já com o próximo quadro convertido
    int imagemAtual;
    int srcW, srcH, srcFmt, srcRange, srcEspaco;
    int dstW, dstH;
    bool valido;
};
static ConversorVideo conversor;
static bool conversorSemThreads = false;

// ---- Estatísticas (cada uma é escrita por uma só thread) ----
static volatile int estatExibidos = 0, estatDecodificados = 0, estatDescartados = 0, estatUnderruns = 0;
static volatile uint64_t estatConverterUs = 0, estatComporUs = 0;
static int logExibidos = 0, logDecodificados = 0, logDescartados = 0, logUnderruns = 0;
static uint64_t logConverterUs = 0, logComporUs = 0, logUltimoUs = 0;

// ==========================================
// FUNÇÕES DE ÁUDIO (SOM DE VOLTA!)
// ==========================================
void misturarAudioVideo(int16_t* out_buffer, int amostras) {
    if (audioFlushPedido) {
        video_ring_read_pos = video_ring_write_pos;
        audioTocadas = audioEscritas;
        audioFlushPedido = false;
    }
    if (!lerCompartilhado(videoRodando) || lerCompartilhado(video_pausado) || !audioLiberado) return;
    int read_pos = video_ring_read_pos;
    int esc_pos = video_ring_write_pos;
    int samples_to_read = amostras * 2;
    int available = esc_pos - read_pos;
    if (available < 0) available += VIDEO_RING_BUFFER_SIZE;
    if (available < samples_to_read) { estatUnderruns = estatUnderruns + 1; return; }
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    int volume = lerCompartilhado(video_volume);
    for (int i = 0; i < samples_to_read; i++) {
        int val = out_buffer[i] + (video_audio_ring[read_pos] * volume) / 100;
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        out_buffer[i] = val;
        read_pos++;
        if (read_pos >= VIDEO_RING_BUFFER_SIZE) read_pos = 0;
    }
    video_ring_read_pos = read_pos;
    audioTocadas = audioTocadas + amostras;
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
// DEMUX: lê o arquivo e separa os pacotes
// ==========================================
static void registrarEstatisticas() {
    uint64_t agora = sceKernelGetProcessTime();
    if (logUltimoUs == 0) { logUltimoUs = agora; return; }
    uint64_t passou = agora - logUltimoUs;
    if (passou < 5000000) return;

    int exibidos = estatExibidos, decodificados = estatDecodificados;
    int descartados = estatDescartados, underruns = estatUnderruns;
    uint64_t converterUs = estatConverterUs, comporUs = estatComporUs;
    int exibidosPeriodo = exibidos - logExibidos;
    double segundos = (double)passou / 1000000.0;
    double fps = (double)exibidosPeriodo / segundos;
    double divisor = exibidosPeriodo > 0 ? (double)exibidosPeriodo * 1000.0 : 1000.0;
    double vTime = lerCompartilhado(video_time_current), aTime = lerCompartilhado(audio_time_current);
    double diferenca = temAudio ? vTime - aTime : 0.0;

    logVideoInfo("PLAYBACK: FPS:%.1f | Dec:%.1f | Drop:%d | V_Time:%.2fs | A_Time:%.2fs | Diff:%.3fs | Filas V:%d A:%d Q:%d | Nivel:%d | Conv:%.1fms | Compor:%.1fms | Underrun:%d | Mult:%.2fx",
        fps, (double)(decodificados - logDecodificados) / segundos, descartados - logDescartados, vTime, aTime, diferenca,
        filaPacotesTamanho(&filaVideo), filaPacotesTamanho(&filaAudio), filaFramesTamanho(), nivelQualidade,
        (double)(converterUs - logConverterUs) / divisor, (double)(comporUs - logComporUs) / divisor,
        underruns - logUnderruns, lerCompartilhado(video_fps_multiplier));

    if (!lerCompartilhado(video_pausado)) fps_video = fps;
    logExibidos = exibidos; logDecodificados = decodificados; logDescartados = descartados; logUnderruns = underruns;
    logConverterUs = converterUs; logComporUs = comporUs; logUltimoUs = agora;
}

static bool empurrarFila(FilaPacotes* f, const EntradaPacote& e) {
    while (!filaPacotesPush(f, e)) {
        if (pararThreads) return false;
        sceKernelUsleep(1000);
    }
    return true;
}

static void executarSeek(double alvo) {
    int serial = __atomic_add_fetch(&serialAtual, 1, __ATOMIC_SEQ_CST);
    audioLiberado = false;

    int64_t destino = (int64_t)((alvo + tempoBaseArquivo) * AV_TIME_BASE);
    if (destino < 0) destino = 0;
    int resultado = av_seek_frame(pFormatCtx, -1, destino, AVSEEK_FLAG_BACKWARD);
    if (resultado < 0) logVideoInfo("SEEK FALHOU: alvo %.2fs erro %d", alvo, resultado);

    videoDrenado = false;
    EntradaPacote marcador = { NULL, serial, ENTRADA_FLUSH, alvo };
    empurrarFila(&filaVideo, marcador);
    if (temAudio) empurrarFila(&filaAudio, marcador);
}

// Fim do arquivo: deixa tudo acabar de tocar e recomeça do início (o player sempre repetiu).
static void tratarFimDoArquivo() {
    EntradaPacote fim = { NULL, serialAtual, ENTRADA_FIM, 0.0 };
    videoDrenado = false;
    if (!empurrarFila(&filaVideo, fim)) return;

    // A leitura anda muito à frente da imagem: aqui ainda podem faltar dezenas de segundos de
    // vídeo. Só desiste de esperar se a reprodução parar de avançar.
    uint64_t ultimoAvanco = sceKernelGetProcessTime();
    double ultimoTempo = -1.0;
    while (!pararThreads && !seekPedido) {
        registrarEstatisticas();
        uint64_t agora = sceKernelGetProcessTime();
        double tempo = lerCompartilhado(video_time_current);
        if (tempo != ultimoTempo || lerCompartilhado(video_pausado)) { ultimoTempo = tempo; ultimoAvanco = agora; }

        int anel = video_ring_write_pos - video_ring_read_pos;
        if (anel < 0) anel += VIDEO_RING_BUFFER_SIZE;
        bool videoAcabou = videoDrenado && filaFramesTamanho() == 0;
        bool audioAcabou = !temAudio || (filaPacotesTamanho(&filaAudio) == 0 && anel < 2048);
        if (videoAcabou && audioAcabou) break;
        if (agora - ultimoAvanco > 6000000) break;
        sceKernelUsleep(5000);
    }
    if (pararThreads || seekPedido) return;

    logVideoInfo("--- FIM DO ARQUIVO / REPETINDO ---");
    executarSeek(0.0);
}

static void* threadDemux(void*) {
    AVPacket* pkt = av_packet_alloc();
    int errosSeguidos = 0;

    while (pkt && !pararThreads) {
        registrarEstatisticas();

        if (seekPedido) {
            double alvo = seekAlvo;
            seekPedido = false;
            executarSeek(alvo);
            continue;
        }

        bool videoCheio = filaPacotesCheia(&filaVideo) ||
            __atomic_load_n(&filaVideo.bytes, __ATOMIC_RELAXED) > FILA_VIDEO_MAX_BYTES;
        bool audioCheio = temAudio && filaPacotesCheia(&filaAudio);
        // Sem o primeiro quadro o áudio ainda não anda: esperar por ele aqui seria um impasse.
        bool audioEmEspera = serialRelogio != serialAtual;
        if (videoCheio || (audioCheio && !audioEmEspera)) { sceKernelUsleep(2000); continue; }

        int resultado = av_read_frame(pFormatCtx, pkt);
        if (resultado == AVERROR_EOF || (resultado < 0 && ++errosSeguidos > 100)) {
            if (resultado != AVERROR_EOF) logVideoInfo("ERRO DE LEITURA: %d", resultado);
            errosSeguidos = 0;
            tratarFimDoArquivo();
            continue;
        }
        if (resultado < 0) { sceKernelUsleep(5000); continue; }
        errosSeguidos = 0;

        FilaPacotes* destino = NULL;
        if (pkt->stream_index == videoStream) destino = &filaVideo;
        else if (temAudio && pkt->stream_index == audioStream) destino = &filaAudio;
        if (!destino) { av_packet_unref(pkt); continue; }

        AVPacket* copia = av_packet_alloc();
        if (!copia) { av_packet_unref(pkt); continue; }
        av_packet_move_ref(copia, pkt);
        EntradaPacote entrada = { copia, serialAtual, ENTRADA_PACOTE, 0.0 };
        bool entregue = (destino == &filaAudio) ? filaPacotesPush(destino, entrada) : empurrarFila(destino, entrada);
        if (!entregue) av_packet_free(&copia);
    }

    if (pkt) av_packet_free(&pkt);
    return NULL;
}

// ==========================================
// DECODIFICAÇÃO DE VÍDEO
// ==========================================
// 0 = qualidade total. Cada nível economiza mais CPU e só entra se o vídeo estiver atrasando.
static void aplicarNivelQualidade(int nivel) {
    AVDiscard filtro = AVDISCARD_DEFAULT, quadros = AVDISCARD_DEFAULT;
    if (nivel == 1) filtro = AVDISCARD_NONREF;
    if (nivel >= 2) filtro = AVDISCARD_ALL;
    if (nivel == 3) quadros = AVDISCARD_NONREF;
    if (nivel >= 4) quadros = AVDISCARD_NONKEY;
    pCodecCtx->skip_loop_filter = filtro;
    pCodecCtx->skip_frame = quadros;
}

static void ajustarQualidade(double atraso, uint64_t agora) {
    int nivel = nivelQualidade, novo = nivel;
    uint64_t desdeMudanca = agora - ultimaMudancaNivelUs;

    if (atraso > 2.0) novo = 4;
    else if (nivel == 4) { if (atraso < 0.3) novo = 3; }
    else if (atrasoSuave > 0.08 && nivel < 3 && desdeMudanca > 1000000) {
        novo = nivel + 1;
        // Voltou a atrasar logo depois de melhorar a imagem: espera mais antes de tentar de novo.
        if (agora - ultimaReducaoNivelUs < 20000000) {
            esperaReducaoNivelUs *= 2;
            if (esperaReducaoNivelUs > ESPERA_REDUCAO_MAXIMA_US) esperaReducaoNivelUs = ESPERA_REDUCAO_MAXIMA_US;
        }
    }
    else if (atrasoSuave < -0.05 && nivel > 0 && desdeMudanca > esperaReducaoNivelUs) {
        novo = nivel - 1;
        ultimaReducaoNivelUs = agora;
    }

    if (novo != nivel) {
        nivelQualidade = novo;
        ultimaMudancaNivelUs = agora;
        aplicarNivelQualidade(novo);
    }
}

static void entregarFrame(AVFrame* frame) {
    int64_t carimbo = frame->best_effort_timestamp;
    if (carimbo == AV_NOPTS_VALUE) carimbo = frame->pts;
    double pts = (carimbo != AV_NOPTS_VALUE) ? carimbo * av_q2d(tbVideo) - tempoBaseArquivo : videoUltimoPts + duracaoFrame;
    if (pts < 0.0) pts = 0.0;
    videoUltimoPts = pts;
    estatDecodificados = estatDecodificados + 1;

    // Depois de uma busca, decodifica até o ponto pedido sem mostrar nada no caminho.
    if (videoDescartarAte > 0.0) {
        if (pts + duracaoFrame * 0.5 < videoDescartarAte) { av_frame_unref(frame); return; }
        videoDescartarAte = 0.0;
        aplicarNivelQualidade(nivelQualidade);
    }

    uint64_t agora = sceKernelGetProcessTime();
    if (serialRelogio == videoSerial && !lerCompartilhado(video_pausado)) {
        double atraso = relogioVideo - pts;
        atrasoSuave = atrasoSuave * 0.9 + atraso * 0.1;
        ajustarQualidade(atraso, agora);

        // Atrasado demais: nem ocupa a fila, mas garante alguns quadros por segundo na tela.
        if (atraso > 0.10 && agora - videoUltimaEntregaUs < 200000) {
            estatDescartados = estatDescartados + 1;
            av_frame_unref(frame);
            return;
        }
    }

    int serial = videoSerial;
    int cauda = __atomic_load_n(&filaFramesCauda, __ATOMIC_RELAXED);
    int proximo = (cauda + 1) % FILA_FRAMES_CAP;
    while (proximo == __atomic_load_n(&filaFramesCabeca, __ATOMIC_ACQUIRE)) {
        if (pararThreads || serial != serialAtual) { av_frame_unref(frame); return; }
        sceKernelUsleep(1500);
    }

    AVFrame* copia = av_frame_alloc();
    if (!copia) { av_frame_unref(frame); return; }
    av_frame_move_ref(copia, frame);
    filaFrames[cauda].frame = copia;
    filaFrames[cauda].pts = pts;
    filaFrames[cauda].serial = serial;
    filaFrames[cauda].numero = proximoNumeroFrame++;
    __atomic_store_n(&filaFramesCauda, proximo, __ATOMIC_RELEASE);
    videoUltimaEntregaUs = agora;
}

static void receberFrames(AVFrame* frame) {
    while (!pararThreads && avcodec_receive_frame(pCodecCtx, frame) == 0) entregarFrame(frame);
}

static void* threadVideo(void*) {
    AVFrame* frame = av_frame_alloc();

    while (frame && !pararThreads) {
        EntradaPacote e;
        if (!filaPacotesPop(&filaVideo, &e)) { sceKernelUsleep(1000); continue; }

        if (e.tipo == ENTRADA_FLUSH) {
            if (e.serial == serialAtual) {
                avcodec_flush_buffers(pCodecCtx);
                videoSerial = e.serial;
                videoDescartarAte = e.alvo;
                videoUltimoPts = e.alvo;
                atrasoSuave = 0.0;
                if (reiniciarQualidade) {
                    // A velocidade voltou a 1x: o atraso de antes não diz nada sobre o arquivo.
                    reiniciarQualidade = false;
                    nivelQualidade = 0;
                    esperaReducaoNivelUs = ESPERA_REDUCAO_INICIAL_US;
                }
                aplicarNivelQualidade(nivelQualidade);
                if (e.alvo > 0.0) pCodecCtx->skip_frame = AVDISCARD_NONREF;
            }
            continue;
        }

        if (e.tipo == ENTRADA_FIM) {
            if (e.serial == serialAtual) {
                avcodec_send_packet(pCodecCtx, NULL);
                receberFrames(frame);
                avcodec_flush_buffers(pCodecCtx);
            }
            videoDrenado = true;
            continue;
        }

        if (e.serial == serialAtual && avcodec_send_packet(pCodecCtx, e.pkt) == 0) receberFrames(frame);
        av_packet_free(&e.pkt);
    }

    if (frame) av_frame_free(&frame);
    return NULL;
}

// ==========================================
// DECODIFICAÇÃO DE ÁUDIO
// ==========================================
static void limparAnelAudio() {
    audioAncoraValida = false;
    audioFlushPedido = true;
    for (int i = 0; i < 30 && audioFlushPedido && !pararThreads; i++) sceKernelUsleep(1000);
    if (audioFlushPedido) {
        video_ring_read_pos = video_ring_write_pos;
        audioTocadas = audioEscritas;
        audioFlushPedido = false;
    }
}

static void escreverAnelAudio(const int16_t* amostras, int quantidade, int serial, double ptsFinal) {
    int total = quantidade * 2;
    for (;;) {
        if (pararThreads || serial != serialAtual) return;
        int ocupado = video_ring_write_pos - video_ring_read_pos;
        if (ocupado < 0) ocupado += VIDEO_RING_BUFFER_SIZE;
        if (ocupado + total <= AUDIO_RING_ALVO) break;
        // Velocidade alterada: o som não acompanha, então o excesso é descartado.
        if (lerCompartilhado(video_fps_multiplier) != 1.0) return;
        sceKernelUsleep(3000);
    }

    int posicao = video_ring_write_pos;
    int primeiro = VIDEO_RING_BUFFER_SIZE - posicao;
    if (primeiro > total) primeiro = total;
    memcpy(video_audio_ring + posicao, amostras, primeiro * sizeof(int16_t));
    if (total > primeiro) memcpy(video_audio_ring, amostras + primeiro, (total - primeiro) * sizeof(int16_t));
    posicao = (posicao + total) % VIDEO_RING_BUFFER_SIZE;

    __atomic_thread_fence(__ATOMIC_RELEASE);
    video_ring_write_pos = posicao;
    audioEscritas = audioEscritas + quantidade;
    audioAncora = ptsFinal - (double)audioEscritas / AUDIO_TAXA;
    audioAncoraValida = true;
}

static void* threadAudio(void*) {
    AVFrame* frame = av_frame_alloc();
    int serial = serialAtual;
    double descartarAte = 0.0, ptsProximo = 0.0;

    while (frame && !pararThreads) {
        EntradaPacote e;
        if (!filaPacotesPop(&filaAudio, &e)) { sceKernelUsleep(2000); continue; }

        if (e.tipo == ENTRADA_FLUSH) {
            if (e.serial == serialAtual) {
                avcodec_flush_buffers(aCodecCtx);
                swr_init(swr_ctx);
                serial = e.serial;
                descartarAte = e.alvo;
                ptsProximo = e.alvo;
                limparAnelAudio();
            }
            continue;
        }

        if (e.serial != serialAtual || avcodec_send_packet(aCodecCtx, e.pkt) != 0) { av_packet_free(&e.pkt); continue; }
        av_packet_free(&e.pkt);

        while (!pararThreads && avcodec_receive_frame(aCodecCtx, frame) == 0) {
            int64_t carimbo = frame->best_effort_timestamp;
            double pts = (carimbo != AV_NOPTS_VALUE) ? carimbo * av_q2d(tbAudio) - tempoBaseArquivo : ptsProximo;
            double duracao = (double)frame->nb_samples / (frame->sample_rate > 0 ? frame->sample_rate : AUDIO_TAXA);
            ptsProximo = pts + duracao;

            bool manter = true;
            if (descartarAte > 0.0) {
                if (pts + duracao < descartarAte) manter = false;
                else descartarAte = 0.0;
            }
            if (manter) {
                uint8_t* saida[1] = { audio_out_buffer };
                int convertidas = swr_convert(swr_ctx, saida, AUDIO_BUFFER_FRAMES, (const uint8_t**)frame->extended_data, frame->nb_samples);
                if (convertidas > 0) escreverAnelAudio((const int16_t*)audio_out_buffer, convertidas, serial, ptsProximo);
            }
            av_frame_unref(frame);
        }
    }

    if (frame) av_frame_free(&frame);
    return NULL;
}

// ==========================================
// CONVERSÃO DE COR E ESCALA
// ==========================================
static struct SwsContext* criarContextoSws(int srcW, int srcH, int srcFmt, int dstW, int dstH, int dstFmt, int threads) {
    struct SwsContext* contexto = sws_alloc_context();
    if (!contexto) return NULL;
    av_opt_set_int(contexto, "srcw", srcW, 0);
    av_opt_set_int(contexto, "srch", srcH, 0);
    av_opt_set_int(contexto, "src_format", srcFmt, 0);
    av_opt_set_int(contexto, "dstw", dstW, 0);
    av_opt_set_int(contexto, "dsth", dstH, 0);
    av_opt_set_int(contexto, "dst_format", dstFmt, 0);
    av_opt_set_int(contexto, "sws_flags", SWS_BILINEAR, 0);
    av_opt_set_int(contexto, "threads", conversorSemThreads ? 1 : threads, 0);
    if (sws_init_context(contexto, NULL, NULL) < 0) { sws_freeContext(contexto); return NULL; }
    return contexto;
}

// Em YUV 4:2:0 -> RGB, uma fatia que começa em linha ímpar pegaria o croma da linha errada.
static int threadsComFatiaPar(int altura, int maximo) {
    for (int n = maximo; n > 1; n--) {
        if ((((altura + n - 1) / n) & 1) == 0) return n;
    }
    return 1;
}

static void liberarConversor() {
    if (conversor.ctxEscala) sws_freeContext(conversor.ctxEscala);
    if (conversor.ctxCor) sws_freeContext(conversor.ctxCor);
    if (conversor.intermediario) av_frame_free(&conversor.intermediario);
    if (conversor.imagens[0]) av_frame_free(&conversor.imagens[0]);
    if (conversor.imagens[1]) av_frame_free(&conversor.imagens[1]);
    memset(&conversor, 0, sizeof(conversor));
    numeroFrameAdiantado = 0;
}

static AVFrame* alocarImagem(int largura, int altura, AVPixelFormat formato) {
    AVFrame* imagem = av_frame_alloc();
    if (!imagem) return NULL;
    imagem->width = largura;
    imagem->height = altura;
    imagem->format = formato;
    if (av_frame_get_buffer(imagem, 64) < 0) { av_frame_free(&imagem); return NULL; }
    return imagem;
}

static bool formatoFaixaCompleta(int formato) {
    return formato == AV_PIX_FMT_YUVJ420P || formato == AV_PIX_FMT_YUVJ422P || formato == AV_PIX_FMT_YUVJ444P ||
        formato == AV_PIX_FMT_YUVJ440P || formato == AV_PIX_FMT_YUVJ411P;
}

static int espacoDeCor(const AVFrame* frame) {
    if (frame->colorspace == AVCOL_SPC_BT709) return SWS_CS_ITU709;
    if (frame->colorspace == AVCOL_SPC_BT2020_NCL || frame->colorspace == AVCOL_SPC_BT2020_CL) return SWS_CS_BT2020;
    if (frame->colorspace == AVCOL_SPC_SMPTE170M || frame->colorspace == AVCOL_SPC_BT470BG) return SWS_CS_ITU601;
    return frame->height >= 600 ? SWS_CS_ITU709 : SWS_CS_ITU601;
}

static bool conversorServePara(const AVFrame* src, int dw, int dh) {
    const ConversorVideo* c = &conversor;
    return c->valido && c->srcW == src->width && c->srcH == src->height && c->srcFmt == src->format &&
        c->srcRange == src->color_range && c->srcEspaco == espacoDeCor(src) && c->dstW == dw && c->dstH == dh;
}

static bool prepararConversor(const AVFrame* src, int dw, int dh) {
    ConversorVideo* c = &conversor;
    if (conversorServePara(src, dw, dh)) return true;

    int espaco = espacoDeCor(src);
    liberarConversor();

    bool yuv420 = src->format == AV_PIX_FMT_YUV420P || src->format == AV_PIX_FMT_YUVJ420P;
    bool escalar = src->width != dw || src->height != dh || !yuv420;
    bool faixaCompleta = src->color_range == AVCOL_RANGE_JPEG;
    int formatoCor = src->format;

    if (escalar) {
        c->ctxEscala = criarContextoSws(src->width, src->height, src->format, dw, dh, AV_PIX_FMT_YUV420P, THREADS_CONVERSAO);
        c->intermediario = alocarImagem(dw, dh, AV_PIX_FMT_YUV420P);
        if (!c->ctxEscala || !c->intermediario) { liberarConversor(); return false; }
        formatoCor = AV_PIX_FMT_YUV420P;
        if (formatoFaixaCompleta(src->format)) faixaCompleta = false;   // o swscale já converteu a faixa
    }
    else if (formatoFaixaCompleta(src->format)) faixaCompleta = true;

    c->ctxCor = criarContextoSws(dw, dh, formatoCor, dw, dh, AV_PIX_FMT_BGRA, threadsComFatiaPar(dh, THREADS_CONVERSAO));
    c->imagens[0] = alocarImagem(dw, dh, AV_PIX_FMT_BGRA);
    c->imagens[1] = alocarImagem(dw, dh, AV_PIX_FMT_BGRA);
    if (!c->ctxCor || !c->imagens[0] || !c->imagens[1]) { liberarConversor(); return false; }

    const int* coeficientes = sws_getCoefficients(espaco);
    sws_setColorspaceDetails(c->ctxCor, coeficientes, faixaCompleta ? 1 : 0, coeficientes, 1, 0, 1 << 16, 1 << 16);

    c->srcW = src->width; c->srcH = src->height; c->srcFmt = src->format;
    c->srcRange = src->color_range; c->srcEspaco = espaco;
    c->dstW = dw; c->dstH = dh;
    c->valido = true;
    logVideoInfo("CONVERSOR: %dx%d fmt:%d -> %dx%d | escala:%d | threads:%d | matriz:%d | faixa:%d",
        src->width, src->height, src->format, dw, dh, escalar ? 1 : 0, conversorSemThreads ? 1 : THREADS_CONVERSAO, espaco, faixaCompleta ? 1 : 0);
    return true;
}

static bool converterFrame(AVFrame* src, AVFrame* destino) {
    uint64_t inicio = sceKernelGetProcessTime();
    AVFrame* entrada = src;
    int resultado = 0;
    if (conversor.ctxEscala) {
        resultado = sws_scale_frame(conversor.ctxEscala, conversor.intermediario, src);
        entrada = conversor.intermediario;
    }
    if (resultado >= 0) resultado = sws_scale_frame(conversor.ctxCor, destino, entrada);
    estatConverterUs = estatConverterUs + (sceKernelGetProcessTime() - inicio);
    if (resultado >= 0) return true;

    logVideoInfo("CONVERSOR FALHOU: erro %d, repetindo sem threads", resultado);
    conversorSemThreads = true;
    liberarConversor();
    return false;
}

// Deixa a imagem de frameAtual pronta: normalmente ela já foi convertida antes da hora.
static bool imagemDoFrameAtual(int dw, int dh, bool frameNovo) {
    if (numeroFrameAtual != 0 && numeroFrameAtual == numeroFrameAdiantado && conversorServePara(frameAtual, dw, dh)) {
        conversor.imagemAtual ^= 1;
        numeroFrameAdiantado = 0;
        return true;
    }
    if (!frameNovo && conversorServePara(frameAtual, dw, dh)) return true;
    if (!prepararConversor(frameAtual, dw, dh)) return false;
    return converterFrame(frameAtual, conversor.imagens[conversor.imagemAtual]);
}

// Roda logo depois do flip: enquanto o vsync não chega, o próximo quadro da fila já é convertido.
// Assim, na hora de exibir só resta copiar a imagem para a tela, e o vsync não é perdido.
static void adiantarConversao() {
    if (!frameAtual || !conversor.valido) return;

    int cabeca = __atomic_load_n(&filaFramesCabeca, __ATOMIC_RELAXED);
    if (cabeca == __atomic_load_n(&filaFramesCauda, __ATOMIC_ACQUIRE)) return;
    EntradaFrame* e = &filaFrames[cabeca];
    if (e->serial != serialAtual || e->numero == numeroFrameAdiantado) return;
    if (!conversorServePara(e->frame, conversor.dstW, conversor.dstH)) return;

    if (converterFrame(e->frame, conversor.imagens[conversor.imagemAtual ^ 1])) numeroFrameAdiantado = e->numero;
}

// ==========================================
// APRESENTADOR: mostra cada quadro na hora certa, sincronizado com o vsync
// ==========================================
static uint32_t* faixaControles = NULL;

// Os buffers de saída são write-combining: a escrita em bloco que não passa pelo cache é a rápida.
static inline void copiarPixels(uint32_t* destino, const uint32_t* origem, int pixels) {
    while (pixels > 0 && ((uintptr_t)destino & 15)) { *destino++ = *origem++; pixels--; }
    for (; pixels >= 8; pixels -= 8, destino += 8, origem += 8) {
        __m128i a = _mm_loadu_si128((const __m128i*)origem);
        __m128i b = _mm_loadu_si128((const __m128i*)(origem + 4));
        _mm_stream_si128((__m128i*)destino, a);
        _mm_stream_si128((__m128i*)(destino + 4), b);
    }
    while (pixels-- > 0) *destino++ = *origem++;
}

static inline void preencherPreto(uint32_t* destino, int pixels) {
    const __m128i preto = _mm_set1_epi32((int)0xFF000000);
    for (; pixels >= 4; pixels -= 4, destino += 4) _mm_stream_si128((__m128i*)destino, preto);
    while (pixels-- > 0) *destino++ = 0xFF000000;
}

static void desenharImagemVideo(uint32_t* tela, int dw, int dh, int dx, int dy) {
    int x0 = dx < 0 ? -dx : 0, y0 = dy < 0 ? -dy : 0;
    int x1 = dx + dw > 1920 ? 1920 - dx : dw;
    int y1 = dy + dh > 1080 ? 1080 - dy : dh;
    if (x1 <= x0) return;

    const AVFrame* imagem = conversor.imagens[conversor.imagemAtual];
    const uint8_t* origem = imagem->data[0];
    int passo = imagem->linesize[0];
    for (int y = y0; y < y1; y++) {
        copiarPixels(tela + (dy + y) * 1920 + dx + x0, (const uint32_t*)(origem + (size_t)y * passo) + x0, x1 - x0);
    }
}

// A barra do player fica sobre o vídeo escurecido. Tudo é montado em RAM e copiado de uma vez:
// ler da tela (write-combining) seria muito lento.
static void desenharFaixaControles(uint32_t* tela, bool temImagem, int dw, int dh, int dx, int dy) {
    const int altura = 1080 - PLAYER_FAIXA_Y;
    if (!faixaControles) faixaControles = (uint32_t*)malloc((size_t)1920 * altura * 4);
    if (!faixaControles) return;

    const AVFrame* imagem = temImagem ? conversor.imagens[conversor.imagemAtual] : NULL;
    for (int y = 0; y < altura; y++) {
        uint32_t* linha = faixaControles + y * 1920;
        memset(linha, 0, 1920 * 4);
        int origemY = PLAYER_FAIXA_Y + y - dy;
        if (imagem && origemY >= 0 && origemY < dh) {
            int x0 = dx < 0 ? 0 : dx, x1 = dx + dw > 1920 ? 1920 : dx + dw;
            if (x1 > x0) memcpy(linha + x0, (const uint32_t*)(imagem->data[0] + (size_t)origemY * imagem->linesize[0]) + (x0 - dx), (size_t)(x1 - x0) * 4);
        }
        // Começa sem escurecer para a faixa não ter borda visível sobre o vídeo.
        uint32_t brilho = y < 70 ? 256 - (uint32_t)(y * 110 / 70) : 146 - (uint32_t)((y - 70) * 106 / (altura - 70));
        for (int x = 0; x < 1920; x++) {
            uint32_t px = linha[x];
            linha[x] = 0xFF000000 | ((((px & 0xFF00FF) * brilho) >> 8) & 0xFF00FF) | ((((px & 0x00FF00) * brilho) >> 8) & 0x00FF00);
        }
    }

    desenharControlesPlayer(faixaControles - (size_t)PLAYER_FAIXA_Y * 1920);
    for (int y = 0; y < altura; y++) copiarPixels(tela + (size_t)(PLAYER_FAIXA_Y + y) * 1920, faixaControles + y * 1920, 1920);
}

static void calcularGeometria(int srcW, int srcH, bool pip, int* dw, int* dh, int* dx, int* dy) {
    int caixaW = pip ? lerCompartilhado(pip_w) : 1920;
    int caixaH = pip ? lerCompartilhado(pip_h) : 1080;
    int caixaX = pip ? lerCompartilhado(pip_x) : 0;
    int caixaY = pip ? lerCompartilhado(pip_y) : 0;
    if (caixaW < 16) caixaW = 16;
    if (caixaH < 16) caixaH = 16;
    if (srcW < 1) srcW = 1;
    if (srcH < 1) srcH = 1;

    int w, h;
    if ((int64_t)srcW * caixaH >= (int64_t)srcH * caixaW) {
        w = caixaW;
        h = (int)(((int64_t)srcH * caixaW + srcW / 2) / srcW);
    }
    else {
        h = caixaH;
        w = (int)(((int64_t)srcW * caixaH + srcH / 2) / srcH);
    }
    w &= ~1; h &= ~1;
    if (w < 2) w = 2;
    if (h < 2) h = 2;
    *dw = w; *dh = h;
    *dx = caixaX + (caixaW - w) / 2;
    *dy = caixaY + (caixaH - h) / 2;
}

static void pedirSeek(double alvo) {
    seekAlvo = alvo;
    seekPedido = true;
}

static void atualizarRelogio(uint64_t agora) {
    double passou = (double)(agora - relogioUltimoUs) / 1000000.0;
    relogioUltimoUs = agora;

    double multiplicador = lerCompartilhado(video_fps_multiplier);
    if (multiplicador != multiplicadorAnterior) {
        // Fora de 1x o som fica para trás: ao voltar, realinha tudo na posição do vídeo.
        if (multiplicador == 1.0) {
            reiniciarQualidade = true;
            pedirSeek(lerCompartilhado(video_time_current));
        }
        multiplicadorAnterior = multiplicador;
    }

    if (serialRelogio != serialAtual || lerCompartilhado(video_pausado)) return;
    if (passou > 0.25) passou = 0.25;
    double relogio = relogioVideo + passou * multiplicador;

    if (temAudio && multiplicador == 1.0 && audioAncoraValida) {
        int64_t tocadas = audioTocadas;
        if (tocadas != relogioTocadasAnterior) { relogioTocadasAnterior = tocadas; relogioTocadasUs = agora; }
        // Só confia no som enquanto ele está andando. Se parar, o relógio interno segue sozinho.
        if (agora - relogioTocadasUs < 200000) {
            double tempoAudio = audioAncora + (double)tocadas / AUDIO_TAXA;
            double diferenca = tempoAudio - relogio;
            if (relogioAlinharAudio || diferenca > 0.35 || diferenca < -0.35) relogio = tempoAudio;
            else relogio += diferenca * 0.03;
            relogioAlinharAudio = false;
            audio_time_current = tempoAudio;
        }
    }
    relogioVideo = relogio;
}

static bool selecionarFrame(uint64_t agora) {
    bool novo = false;

    for (;;) {
        int cabeca = __atomic_load_n(&filaFramesCabeca, __ATOMIC_RELAXED);
        if (cabeca == __atomic_load_n(&filaFramesCauda, __ATOMIC_ACQUIRE)) break;

        EntradaFrame* e = &filaFrames[cabeca];
        int proximo = (cabeca + 1) % FILA_FRAMES_CAP;
        if (e->serial != serialAtual) {
            av_frame_free(&e->frame);
            __atomic_store_n(&filaFramesCabeca, proximo, __ATOMIC_RELEASE);
            continue;
        }

        if (serialRelogio != e->serial) {
            // Primeiro quadro depois de abrir ou buscar: o relógio e o som começam daqui.
            relogioVideo = e->pts;
            relogioUltimoUs = agora;
            relogioTocadasAnterior = -1;
            relogioAlinharAudio = temAudio;
            serialRelogio = e->serial;
            audioLiberado = temAudio;
        }
        else {
            double relogio = relogioVideo;
            if (e->pts > relogio + 10.0) relogioVideo = e->pts;   // salto de tempo no próprio arquivo
            else if (e->pts > relogio + 0.012) break;             // ainda não é a hora dele
        }

        if (frameAtual) av_frame_free(&frameAtual);
        frameAtual = e->frame;
        ptsAtual = e->pts;
        numeroFrameAtual = e->numero;
        __atomic_store_n(&filaFramesCabeca, proximo, __ATOMIC_RELEASE);
        novo = true;
    }

    if (novo) video_time_current = ptsAtual;
    return novo;
}

static void passoApresentador() {
    // O fim do flip anterior marca o vsync: é o que dá o ritmo certo da imagem.
    for (int i = 0; i < 40 && graficosFlipPendente(); i++) {
        if (!apresentadorAtivo || apresentadorSair) return;
        sceKernelUsleep(1000);
    }

    uint64_t agora = sceKernelGetProcessTime();
    atualizarRelogio(agora);
    bool frameNovo = selecionarFrame(agora);

    bool pip = lerCompartilhado(video_minimizado);
    bool camadaNova = false;
    if (pip) {
        camadaAtual = graficosPegarCamada(&camadaNova);
        if (camadaNova) versaoCamada++;
    }

    // Enquanto não houver o que mostrar, a tela anterior continua lá (sem piscar em preto).
    if (pip ? !camadaAtual : !frameAtual) {
        if (!graficosRepetirFlip()) sceKernelUsleep(2000);
        else sceKernelUsleep(1000);
        return;
    }

    int dw = 0, dh = 0, dx = 0, dy = 0;
    if (frameAtual) calcularGeometria(frameAtual->width, frameAtual->height, pip, &dw, &dh, &dx, &dy);

    bool controles = !pip && playerControlesVisiveis();
    bool overlay = controles || lerCompartilhado(mostrar_timeline) || lerCompartilhado(menu_video_aberto) ||
        lerCompartilhado(exibir_botao_pular) || (pip && lerCompartilhado(pip_selecionado));
    bool geometriaMudou = dw != ultimoDw || dh != ultimoDh || dx != ultimoDx || dy != ultimoDy;
    apresentadorTick++;

    bool redesenhar = frameNovo || camadaNova || geometriaMudou || pip != ultimoPip ||
        overlay != ultimoOverlay || (overlay && (apresentadorTick & 1) == 0);
    if (!redesenhar) {
        bool repetiu = graficosRepetirFlip();
        adiantarConversao();
        sceKernelUsleep(repetiu ? 1000 : 2000);
        return;
    }

    bool temImagem = frameAtual && imagemDoFrameAtual(dw, dh, frameNovo);

    uint64_t inicioComposicao = sceKernelGetProcessTime();
    int indice = graficosIndiceBack();
    uint32_t* tela = graficosBufferBack();

    // Cada um dos dois buffers lembra qual fundo já tem, para não recopiar a tela inteira à toa.
    uint64_t retangulo = (uint64_t)(uint16_t)dw | ((uint64_t)(uint16_t)dh << 16) |
        ((uint64_t)(uint16_t)dx << 32) | ((uint64_t)(uint16_t)dy << 48);
    uint64_t carimbo = (pip ? (versaoCamada + 1) * 0x9E3779B97F4A7C15ULL : 0x51ED270B9A1F3C55ULL) ^ retangulo;
    bool cobreTudo = !pip && temImagem && dx <= 0 && dy <= 0 && dx + dw >= 1920 && dy + dh >= 1080;
    if (!cobreTudo && (overlay || carimboFundo[indice] != carimbo)) {
        if (pip) copiarPixels(tela, camadaAtual, 1920 * 1080);
        else preencherPreto(tela, 1920 * 1080);
    }
    carimboFundo[indice] = (overlay || cobreTudo) ? 0 : carimbo;

    if (temImagem) desenharImagemVideo(tela, dw, dh, dx, dy);

    if (controles) desenharFaixaControles(tela, temImagem, dw, dh, dx, dy);
    else if (overlay) {
        desenharLinhaDoTempo(tela);
        desenharMenuPlayerVideo(tela);
    }

    _mm_sfence();
    graficosFlip();
    estatComporUs = estatComporUs + (sceKernelGetProcessTime() - inicioComposicao);
    if (frameNovo) estatExibidos = estatExibidos + 1;

    ultimoDw = dw; ultimoDh = dh; ultimoDx = dx; ultimoDy = dy;
    ultimoPip = pip; ultimoOverlay = overlay;

    adiantarConversao();
}

static void* threadApresentador(void*) {
    scePthreadSetprio(scePthreadSelf(), PRIORIDADE_APRESENTADOR);

    while (!apresentadorSair) {
        __atomic_store_n(&apresentadorEmPasso, 1, __ATOMIC_SEQ_CST);
        bool ativo = apresentadorAtivo;
        if (ativo) passoApresentador();
        __atomic_store_n(&apresentadorEmPasso, 0, __ATOMIC_SEQ_CST);
        if (!ativo) sceKernelUsleep(2000);
    }
    return NULL;
}

// Chamada pelo loop principal: diz se o vídeo aparece agora. Quando aparece, o loop principal
// passa a desenhar a interface numa camada em RAM e quem cuida da tela é o apresentador.
void definirApresentacaoVideo(bool naTela) {
    if (!thApresentadorViva || naTela == apresentadorAtivo) return;

    if (naTela) {
        graficosEntrarApresentacao();
        camadaAtual = NULL;
        carimboFundo[0] = 0; carimboFundo[1] = 0;
        ultimoDw = 0; ultimoDh = 0;
        relogioUltimoUs = sceKernelGetProcessTime();
        apresentadorAtivo = true;
        return;
    }

    apresentadorAtivo = false;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    while (__atomic_load_n(&apresentadorEmPasso, __ATOMIC_SEQ_CST)) sceKernelUsleep(500);
    graficosSairApresentacao();
}

// ==========================================
// CONTROLE DO MOTOR DE VÍDEO
// ==========================================
static bool criarThread(pthread_t* thread, void* (*funcao)(void*)) {
    pthread_attr_t atributos;
    pthread_attr_init(&atributos);
    pthread_attr_setstacksize(&atributos, PILHA_THREAD);
    int resultado = pthread_create(thread, &atributos, funcao, NULL);
    pthread_attr_destroy(&atributos);
    if (resultado != 0) resultado = pthread_create(thread, NULL, funcao, NULL);
    return resultado == 0;
}

static void encerrarThreads() {
    definirApresentacaoVideo(false);
    audioLiberado = false;

    apresentadorSair = true;
    if (thApresentadorViva) { pthread_join(thApresentador, NULL); thApresentadorViva = false; }

    pararThreads = true;
    if (thDemuxViva) { pthread_join(thDemux, NULL); thDemuxViva = false; }
    if (thVideoViva) { pthread_join(thVideo, NULL); thVideoViva = false; }
    if (thAudioViva) { pthread_join(thAudio, NULL); thAudioViva = false; }
}

static void liberarRecursosVideo() {
    filaPacotesEsvaziar(&filaVideo);
    filaPacotesEsvaziar(&filaAudio);
    free(filaVideo.itens); filaVideo.itens = NULL;
    free(filaAudio.itens); filaAudio.itens = NULL;

    while (filaFramesCabeca != filaFramesCauda) {
        av_frame_free(&filaFrames[filaFramesCabeca].frame);
        filaFramesCabeca = (filaFramesCabeca + 1) % FILA_FRAMES_CAP;
    }
    if (frameAtual) av_frame_free(&frameAtual);
    liberarConversor();
    free(faixaControles); faixaControles = NULL;

    if (swr_ctx) swr_free(&swr_ctx);
    if (aCodecCtx) avcodec_free_context(&aCodecCtx);
    if (audio_out_buffer) { av_free(audio_out_buffer); audio_out_buffer = NULL; }
    if (pCodecCtx) avcodec_free_context(&pCodecCtx);
    if (pFormatCtx) avformat_close_input(&pFormatCtx);

    temAudio = false;
}

void pararVideo() {
    if (!videoRodando || bloqueio_fechar_video) return;

    reiniciarControlesPlayer();
    encerrarThreads();
    liberarRecursosVideo();

    videoRodando = false; video_minimizado = false;
    bloqueio_audio_nativo = false;
    logVideoInfo("--- FIM VIDEO | exibidos:%d decodificados:%d descartados:%d underruns:%d ---",
        estatExibidos, estatDecodificados, estatDescartados, estatUnderruns);
}

void pausarVideoParaTela() {
    if (!videoRodando) return;
    if (!video_pausado) {
        video_pausado = true;
        video_pausado_por_tela = true;
    }
}

void retomarVideoAposTela() {
    if (!video_pausado_por_tela) return;
    if (videoRodando) video_pausado = false;
    video_pausado_por_tela = false;
}

static bool abrirDecodificadorVideo() {
    AVStream* stream = pFormatCtx->streams[videoStream];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) return false;

    int largura = stream->codecpar->width, altura = stream->codecpar->height;
    int threads = (int64_t)largura * altura <= 1280 * 720 ? 4 : 5;

    // O decodificador roda fora do loop principal, então a latência das threads por quadro é
    // só alguns quadros a mais de espera na abertura, e nunca segura a tela.
    for (int tentativa = 0; tentativa < 2; tentativa++) {
        pCodecCtx = avcodec_alloc_context3(codec);
        if (!pCodecCtx) return false;
        avcodec_parameters_to_context(pCodecCtx, stream->codecpar);
        pCodecCtx->pkt_timebase = stream->time_base;
        pCodecCtx->thread_count = tentativa == 0 ? threads : 1;
        pCodecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        pCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
        if (largura > 1920 || altura > 1920) pCodecCtx->lowres = 1;
        if (avcodec_open2(pCodecCtx, codec, NULL) >= 0) return true;
        avcodec_free_context(&pCodecCtx);
    }
    return false;
}

static bool abrirDecodificadorAudio() {
    AVStream* stream = pFormatCtx->streams[audioStream];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) return false;

    aCodecCtx = avcodec_alloc_context3(codec);
    if (!aCodecCtx) return false;
    avcodec_parameters_to_context(aCodecCtx, stream->codecpar);
    aCodecCtx->pkt_timebase = stream->time_base;
    aCodecCtx->thread_count = 1;
    if (avcodec_open2(aCodecCtx, codec, NULL) < 0) return false;

#if LIBAVUTIL_VERSION_MAJOR >= 58
    AVChannelLayout saida;
    av_channel_layout_default(&saida, 2);
    swr_alloc_set_opts2(&swr_ctx, &saida, AV_SAMPLE_FMT_S16, AUDIO_TAXA,
        &aCodecCtx->ch_layout, aCodecCtx->sample_fmt, aCodecCtx->sample_rate, 0, NULL);
#else
    uint64_t layoutEntrada = aCodecCtx->channel_layout;
    if (layoutEntrada == 0) layoutEntrada = av_get_default_channel_layout(aCodecCtx->channels);
    swr_ctx = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, AUDIO_TAXA,
        layoutEntrada, aCodecCtx->sample_fmt, aCodecCtx->sample_rate, 0, NULL);
#endif
    if (!swr_ctx || swr_init(swr_ctx) < 0) return false;

    audio_out_buffer = (uint8_t*)av_malloc(AUDIO_BUFFER_FRAMES * 4);
    return audio_out_buffer != NULL;
}

static void reiniciarEstadoMotor() {
    pararThreads = false; apresentadorSair = false; apresentadorAtivo = false;
    serialAtual = 1; serialRelogio = 0; seekPedido = false;
    filaFramesCabeca = 0; filaFramesCauda = 0;
    video_ring_read_pos = 0; video_ring_write_pos = 0;
    audioEscritas = 0; audioTocadas = 0; audioAncora = 0.0;
    audioAncoraValida = false; audioLiberado = false; audioFlushPedido = false;
    relogioVideo = 0.0; relogioTocadasAnterior = -1;
    multiplicadorAnterior = lerCompartilhado(video_fps_multiplier);
    videoSerial = 1; videoDescartarAte = 0.0; videoUltimoPts = 0.0; videoUltimaEntregaUs = 0; videoDrenado = false;
    nivelQualidade = 0; atrasoSuave = 0.0; reiniciarQualidade = false;
    ultimaMudancaNivelUs = sceKernelGetProcessTime(); ultimaReducaoNivelUs = 0;
    esperaReducaoNivelUs = ESPERA_REDUCAO_INICIAL_US;
    conversorSemThreads = false;
    ptsAtual = 0.0; versaoCamada = 0; apresentadorTick = 0; camadaAtual = NULL;
    proximoNumeroFrame = 1; numeroFrameAtual = 0; numeroFrameAdiantado = 0;
    ultimoDw = 0; ultimoDh = 0; ultimoDx = 0; ultimoDy = 0; ultimoPip = false; ultimoOverlay = false;
    estatExibidos = 0; estatDecodificados = 0; estatDescartados = 0; estatUnderruns = 0;
    estatConverterUs = 0; estatComporUs = 0;
    logExibidos = 0; logDecodificados = 0; logDescartados = 0; logUnderruns = 0;
    logConverterUs = 0; logComporUs = 0; logUltimoUs = 0;
}

void iniciarVideoMP4(const char* caminho) {
    bloqueio_fechar_video = false;
    bloqueio_audio_nativo = true;

    tocarMusicaNova("PARADO");
    sceKernelUsleep(150000);

    if (videoRodando) pararVideo();

    video_pausado = false;
    video_pausado_por_tela = false;
    video_minimizado = false;
    memset(thumb_ready, 0, sizeof(thumb_ready));
    video_time_current = 0.0; audio_time_current = 0.0;

    snprintf(msgStatus, sizeof(msgStatus), "FFMPEG: Descritor Turbo Ativo!");
    msgTimer = 180;

    if (caminho && strlen(caminho) > 0 && caminho != ultimo_video_tocado) {
        snprintf(ultimo_video_tocado, sizeof(ultimo_video_tocado), "%s", caminho);
    }

    const char* nomeArquivo = strrchr(ultimo_video_tocado, '/');
    snprintf(info_nome, sizeof(info_nome), "%s", nomeArquivo ? nomeArquivo + 1 : ultimo_video_tocado);

    rotacionarLogVideo();
    reiniciarControlesPlayer();
    reiniciarEstadoMotor();

    if (avformat_open_input(&pFormatCtx, caminho, NULL, NULL) != 0) {
        logVideoInfo("ERRO: nao abriu %s", ultimo_video_tocado);
        bloqueio_audio_nativo = false;
        return;
    }
    avformat_find_stream_info(pFormatCtx, NULL);

    videoStream = -1; audioStream = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
        AVCodecParameters* p = pFormatCtx->streams[i]->codecpar;
        if (p->codec_type == AVMEDIA_TYPE_VIDEO && videoStream == -1) videoStream = i;
        else if (p->codec_type == AVMEDIA_TYPE_AUDIO && audioStream == -1) audioStream = i;
    }

    if (videoStream == -1 || !abrirDecodificadorVideo()) {
        logVideoInfo("ERRO: sem video decodificavel em %s", ultimo_video_tocado);
        liberarRecursosVideo();
        bloqueio_audio_nativo = false;
        return;
    }

    if (audioStream != -1) {
        temAudio = abrirDecodificadorAudio();
        if (!temAudio) {
            logVideoInfo("AVISO: audio nao abriu, tocando sem som");
            if (swr_ctx) swr_free(&swr_ctx);
            if (aCodecCtx) avcodec_free_context(&aCodecCtx);
        }
    }

    AVStream* streamVideo = pFormatCtx->streams[videoStream];
    tbVideo = streamVideo->time_base;
    if (temAudio) tbAudio = pFormatCtx->streams[audioStream]->time_base;
    tempoBaseArquivo = pFormatCtx->start_time != AV_NOPTS_VALUE ? (double)pFormatCtx->start_time / AV_TIME_BASE : 0.0;
    video_time_total = pFormatCtx->duration > 0 ? (double)pFormatCtx->duration / AV_TIME_BASE : 0.0;

    AVRational taxa = streamVideo->avg_frame_rate;
    if (taxa.num <= 0 || taxa.den <= 0) taxa = streamVideo->r_frame_rate;
    double fps = (taxa.num > 0 && taxa.den > 0) ? av_q2d(taxa) : 24.0;
    if (fps < 1.0 || fps > 240.0) fps = 24.0;
    duracaoFrame = 1.0 / fps;
    fps_video = fps;

    filaVideo.capacidade = FILA_VIDEO_CAP; filaVideo.cabeca = 0; filaVideo.cauda = 0; filaVideo.bytes = 0;
    filaAudio.capacidade = FILA_AUDIO_CAP; filaAudio.cabeca = 0; filaAudio.cauda = 0; filaAudio.bytes = 0;
    filaVideo.itens = (EntradaPacote*)calloc(FILA_VIDEO_CAP, sizeof(EntradaPacote));
    filaAudio.itens = (EntradaPacote*)calloc(FILA_AUDIO_CAP, sizeof(EntradaPacote));

    if (!filaVideo.itens || !filaAudio.itens || !graficosPrepararApresentacao()) {
        logVideoInfo("ERRO: sem memoria para o player");
        liberarRecursosVideo();
        bloqueio_audio_nativo = false;
        return;
    }

    logVideoInfo("--- INICIO VIDEO ---");
    logVideoInfo("Arquivo: %s", ultimo_video_tocado);
    logVideoInfo("Resolucao Original: %dx%d (LowRes:%d)", streamVideo->codecpar->width, streamVideo->codecpar->height, pCodecCtx->lowres);
    logVideoInfo("Duracao Total: %.2fs", video_time_total);
    logVideoInfo("FPS Original: %.2f", fps);
    logVideoInfo("Decoder: %s | Threads:%d | ThreadType:%d | Audio:%s", pCodecCtx->codec->name, pCodecCtx->thread_count,
        pCodecCtx->active_thread_type, temAudio ? aCodecCtx->codec->name : "nenhum");

    videoRodando = true;
    thVideoViva = criarThread(&thVideo, threadVideo);
    if (temAudio) thAudioViva = criarThread(&thAudio, threadAudio);
    thDemuxViva = criarThread(&thDemux, threadDemux);
    thApresentadorViva = criarThread(&thApresentador, threadApresentador);

    if (!thVideoViva || !thDemuxViva || !thApresentadorViva || (temAudio && !thAudioViva)) {
        logVideoInfo("ERRO: nao criou as threads do player");
        pararVideo();
    }
}

void pularParaTempo(double tempo) {
    if (!videoRodando) return;
    if (video_time_total > 1.0 && tempo > video_time_total - 1.0) tempo = video_time_total - 1.0;
    if (tempo < 0) tempo = 0;
    video_time_current = tempo;
    pedirSeek(tempo);
}
