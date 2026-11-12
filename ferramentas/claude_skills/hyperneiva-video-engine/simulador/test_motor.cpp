// Simulador do PS4 para o motor de video: vsync, saida de som e loop principal falsos.
#include <windows.h>
#include <psapi.h>
#include <timeapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <pthread.h>
#include "stubs/orbis/libkernel.h"
#include "stubs/orbis/Pad.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "graphics.h"
#include "audio.h"
#include "video.h"

extern bool video_pausado, video_minimizado, bloqueio_fechar_video, mostrar_timeline;
extern double video_time_current, video_time_total, video_fps_multiplier, audio_time_current, fps_video;
extern int pip_x, pip_y, pip_w, pip_h, video_volume;
extern void pularParaTempo(double t);
extern bool menu_video_aberto, pip_selecionado;
extern int comando_trocar_video, menu_video_sel;
enum VideoMenuState { V_MAIN, V_PIP, V_SKIP_VIDEOS, V_SKIP_LIST, V_SKIP_ADD, V_SKIP_EDIT, V_INFO, V_TRACKS };
extern VideoMenuState vState;
extern void processarControlesMenuVideo(unsigned int btn, OrbisPadData* pData, bool& cross_consumido, bool& circle_consumido);
bool pCross = false, menuL2_Aberto = false, visualizandoPDF = false, visualizandoMidiaTexto = false, visualizandoMidiaImagem = false;
bool visualizandoPic1 = false, bloco_notas_aberto = false, instrumento_aberto = false, teste_controle_aberto = false;
int32_t global_uId = 0; char msgStatus[128]; int msgTimer = 0;
const char* listaOpcoes[150]; int totalOpcoes = 0, selOpcao = 0, mapOpcoes[150]; bool showOpcoes = false;

static LARGE_INTEGER freq, base;
extern "C" uint64_t sceKernelGetProcessTime(void) { LARGE_INTEGER n; QueryPerformanceCounter(&n); return (uint64_t)((n.QuadPart - base.QuadPart) * 1000000.0 / freq.QuadPart); }
extern "C" int32_t sceKernelUsleep(uint32_t us) {
    Sleep(us < 1500 ? 1 : us / 1000);
    return 0;
}
extern "C" OrbisPthread scePthreadSelf(void) { return pthread_self(); }
extern "C" int32_t scePthreadSetprio(OrbisPthread, int) { return 0; }
void tocarMusicaNova(const char*) {}
static stbtt_fontinfo fonte; static bool temFonte = false; static std::vector<unsigned char> dadosFonte;
static void carregarFonte() {
    FILE* f = fopen("C:/Users/Marco/source/repos/teste3/teste3/assets/fonts/font.ttf", "rb"); if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); dadosFonte.resize(n); fread(dadosFonte.data(), 1, n, f); fclose(f);
    temFonte = stbtt_InitFont(&fonte, dadosFonte.data(), 0) != 0;
}
void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor) {
    if (!temFonte) return;
    float s = stbtt_ScaleForPixelHeight(&fonte, (float)tam);
    int asc; stbtt_GetFontVMetrics(&fonte, &asc, 0, 0); asc = (int)(asc * s);
    int curX = x;
    for (int i = 0; texto[i]; ++i) {
        int adv, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&fonte, texto[i], &adv, &lsb);
        stbtt_GetCodepointBitmapBox(&fonte, texto[i], s, s, &x0, &y0, &x1, &y1);
        int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            unsigned char* b = (unsigned char*)malloc(w * h);
            stbtt_MakeCodepointBitmap(&fonte, b, w, h, w, s, s, texto[i]);
            for (int cy = 0; cy < h; ++cy) for (int cx = 0; cx < w; ++cx) {
                int pX = curX + x0 + cx, pY = y + asc + y0 + cy;
                if (pX >= 0 && pX < 1920 && pY >= 0 && pY < 1080 && b[cy * w + cx] > 30) pixels[pY * 1920 + pX] = cor;
            }
            free(b);
        }
        curX += (int)(adv * s);
    }
}

// ---------- graficos (mesma logica de graphics.cpp) ----------
#define BYTES_TELA (1920 * 1080 * 4)
#define CAMADA_NOVA 4
static uint32_t* fb[4];
static uint32_t* camadasUI[3];
static volatile bool desenhandoNaCamada = false;
static int camadaEscrita = 0, camadaExibida = 1, camadaPronta = 2;
static bool camadaComConteudo = false;
static int saidaBack = 0, saidaExibida = -1, bA = 0;
static std::atomic<int> pendentes(0);
static std::atomic<int> visivel(-1);
static volatile bool simViva = true;

bool graficosPrepararApresentacao() { for (int i = 0; i < 3; i++) { if (!camadasUI[i]) camadasUI[i] = (uint32_t*)malloc(BYTES_TELA); } return true; }
void graficosEntrarApresentacao() { camadaEscrita = 0; camadaExibida = 1; camadaPronta = 2; camadaComConteudo = false; saidaExibida = -1; saidaBack = 0; desenhandoNaCamada = true; }
void graficosSairApresentacao() { desenhandoNaCamada = false; }
uint32_t* graficosPegarCamada(bool* nova) {
    *nova = false;
    if (__atomic_load_n(&camadaPronta, __ATOMIC_ACQUIRE) & CAMADA_NOVA) {
        camadaExibida = __atomic_exchange_n(&camadaPronta, camadaExibida, __ATOMIC_ACQ_REL) & 3;
        camadaComConteudo = true; *nova = true;
    }
    return camadaComConteudo ? camadasUI[camadaExibida] : NULL;
}
uint32_t* graficosBufferBack() { return fb[2 + saidaBack]; }
int graficosIndiceBack() { return saidaBack; }

// medicoes
struct Marca { uint64_t t; double pts; };
static std::vector<Marca> flipsNovos; static double ultimoPts = -1;
static std::vector<uint64_t> flashes, bipes; static bool emFlash = false, emBipe = false;
static int amostraX = 960, amostraY = 540;
static uint64_t proximoVsync = 0; static int totalFlips = 0, totalRepeticoes = 0;
static const uint64_t VSYNC_US = 16683;

static int rasgados = 0, flipsVerificados = 0;
static std::atomic<bool> capturaPedida(false), capturaPronta(false);
static uint32_t* captura = NULL;
void graficosFlip() {
    uint32_t* tela = fb[2 + saidaBack];
    if (!menu_video_aberto) {
        int x = video_minimizado ? pip_x + pip_w / 2 + 30 : 1030;
        int ya = video_minimizado ? pip_y + pip_h * 18 / 100 : 200;
        int yb = video_minimizado ? pip_y + pip_h * 56 / 100 : 610;
        int yc = video_minimizado ? pip_y + pip_h * 83 / 100 : 800;
        bool a = ((tela[ya * 1920 + x] >> 16) & 255) > 150, b = ((tela[yb * 1920 + x] >> 16) & 255) > 150, c = ((tela[yc * 1920 + x] >> 16) & 255) > 150;
        flipsVerificados++;
        if (a != b || b != c) rasgados++;
    }
    if (capturaPedida.load()) { memcpy(captura, tela, BYTES_TELA); capturaPedida = false; capturaPronta = true; }
    // Amostra a imagem do video (no PiP o centro da tela cai dentro da janela do video? usa o centro da caixa)
    int ax = video_minimizado ? pip_x + pip_w / 2 + 30 : amostraX + 70;
    int ay = video_minimizado ? pip_y + pip_h / 2 + 30 : amostraY + 70;
    uint32_t px = fb[2 + saidaBack][ay * 1920 + ax];
    int r = (px >> 16) & 255;
    uint64_t exibeEm = proximoVsync;
    if (r > 180 && !emFlash) { emFlash = true; flashes.push_back(exibeEm); }
    if (r < 90) emFlash = false;
    double pts = video_time_current;
    if (pts != ultimoPts) { flipsNovos.push_back({ exibeEm, pts }); ultimoPts = pts; }
    saidaExibida = 2 + saidaBack; pendentes++; totalFlips++;
    visivel = saidaExibida; saidaBack ^= 1;
}
bool graficosRepetirFlip() { if (saidaExibida < 0) return false; pendentes++; totalRepeticoes++; return true; }
bool graficosFlipPendente() { return pendentes.load() > 0; }
uint32_t* obterBufferVideo() { if (desenhandoNaCamada) return camadasUI[camadaEscrita]; return fb[bA]; }
void submeterTela() {
    if (desenhandoNaCamada) { camadaEscrita = __atomic_exchange_n(&camadaPronta, camadaEscrita | CAMADA_NOVA, __ATOMIC_ACQ_REL) & 3; sceKernelUsleep(16000); return; }
    visivel = bA; pendentes++; bA = (bA + 1) % 2; sceKernelUsleep(16000);
}
static void* vsyncThread(void*) {
    proximoVsync = sceKernelGetProcessTime() + VSYNC_US;
    while (simViva) {
        while (sceKernelGetProcessTime() + 1500 < proximoVsync) Sleep(1);
        while (sceKernelGetProcessTime() < proximoVsync) {}
        pendentes = 0;
        proximoVsync += VSYNC_US;
    }
    return NULL;
}
static void* audioThread(void*) {
    int16_t buf[512]; uint64_t prox = sceKernelGetProcessTime();
    while (simViva) {
        memset(buf, 0, sizeof(buf));
        misturarAudioVideo(buf, 256);
        int pico = 0; for (int i = 0; i < 512; i++) { int a = abs(buf[i]); if (a > pico) pico = a; }
        if (pico > 2000 && !emBipe) { emBipe = true; bipes.push_back(sceKernelGetProcessTime()); }
        if (pico < 200) emBipe = false;
        prox += 5333;
        for (;;) {
            uint64_t n = sceKernelGetProcessTime();
            if (n >= prox) break;
            if (prox - n > 1500) Sleep(1);
        }
    }
    return NULL;
}

// ---------- loop principal simulado (mesma ordem do main.cpp) ----------
static bool telaSemVideo = false;
static void rodarPor(double segundos, int custoUiMs) {
    uint64_t fim = sceKernelGetProcessTime() + (uint64_t)(segundos * 1e6);
    while (sceKernelGetProcessTime() < fim) {
        if (telaSemVideo) pausarVideoParaTela(); else retomarVideoAposTela();
        bool naTela = videoRodando && !telaSemVideo;
        definirApresentacaoVideo(naTela);
        if (naTela && !video_minimizado) { sceKernelUsleep(33000); continue; }
        uint32_t* p = obterBufferVideo();
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF204060;
        if (custoUiMs) sceKernelUsleep(custoUiMs * 1000);
        submeterTela();
    }
}
static void limparMedidas() { flipsNovos.clear(); flashes.clear(); bipes.clear(); totalFlips = totalRepeticoes = 0; }
static void salvarTela(const char* nome) {
    if (!captura) captura = (uint32_t*)malloc(BYTES_TELA);
    capturaPronta = false; capturaPedida = true;
    for (int i = 0; i < 300 && !capturaPronta.load(); i++) Sleep(2);
    capturaPedida = false;
    if (!capturaPronta.load()) { printf("    (captura %s: nenhum flip novo)\n", nome); return; }
    uint32_t* src = captura;
    char c[256]; snprintf(c, sizeof(c), "out/%s.ppm", nome);
    FILE* f = fopen(c, "wb"); if (!f) return;
    fprintf(f, "P6\n1920 1080\n255\n");
    std::vector<unsigned char> linha(1920 * 3);
    for (int y = 0; y < 1080; y++) { for (int x = 0; x < 1920; x++) { uint32_t v = src[y * 1920 + x]; linha[x * 3] = (v >> 16) & 255; linha[x * 3 + 1] = (v >> 8) & 255; linha[x * 3 + 2] = v & 255; } fwrite(linha.data(), 1, linha.size(), f); }
    fclose(f);
}
static int falhas = 0;
static void relatorio(const char* nome, double fpsEsperado, double segundos, bool checarSync) {
    int n = (int)flipsNovos.size(); double fps = n / segundos;
    std::vector<double> gaps; for (int i = 1; i < n; i++) gaps.push_back((flipsNovos[i].t - flipsNovos[i - 1].t) / 1000.0);
    double maior = 0; for (double g : gaps) if (g > maior) maior = g;
    int hist[6] = { 0 }; for (double g : gaps) { int v = (int)(g / 16.683 + 0.5); if (v > 5) v = 5; hist[v]++; }
    double avanco = n > 1 ? flipsNovos[n - 1].pts - flipsNovos[0].pts : 0;
    double tReal = n > 1 ? (flipsNovos[n - 1].t - flipsNovos[0].t) / 1e6 : 1;
    double velocidade = avanco / tReal;
    double soma = 0, pior = 0; int pares = 0;
    for (uint64_t f : flashes) { double melhor = 1e9; for (uint64_t b : bipes) { double d = ((double)f - (double)b) / 1000.0; if (fabs(d) < fabs(melhor)) melhor = d; } if (fabs(melhor) < 500) { soma += melhor; pares++; if (fabs(melhor) > fabs(pior)) pior = melhor; } }
    bool ok = true;
    if (fpsEsperado > 0 && fps < fpsEsperado * 0.93) ok = false;
    if (fpsEsperado > 0 && (velocidade < 0.97 || velocidade > 1.03)) ok = false;
    if (checarSync && (pares < 2 || fabs(pior) > 60)) ok = false;
    if (!ok) falhas++;
    printf("[%s] %-34s fps=%.1f vel=%.3fx maiorGap=%.0fms vsyncs{1:%d 2:%d 3:%d 4:%d 5+:%d}", ok ? " OK " : "FALHA", nome, fps, velocidade, maior, hist[1], hist[2], hist[3], hist[4], hist[5]);
    if (pares) printf(" A/V media=%.0fms pior=%.0fms(n=%d)", soma / pares, pior, pares);
    printf("\n"); fflush(stdout);
}
static void checar(const char* nome, bool ok) { if (!ok) falhas++; printf("[%s] %s\n", ok ? " OK " : "FALHA", nome); fflush(stdout); }
static size_t memoriaMB() { PROCESS_MEMORY_COUNTERS c; GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof(c)); return c.WorkingSetSize / (1024 * 1024); }
static void fechar() { bloqueio_fechar_video = false; pararVideo(); definirApresentacaoVideo(false); }

static void testeCompleto(const char* arq) {
    printf("\n=== Sequencia completa: %s\n", arq);
    iniciarVideoMP4(arq);
    checar("abriu", videoRodando);
    rodarPor(1.0, 0); limparMedidas();
    rodarPor(6.0, 0); relatorio("tela cheia", 22.0, 6.0, true); salvarTela("tela_cheia");

    video_minimizado = true; rodarPor(0.5, 60); limparMedidas();
    rodarPor(6.0, 60); relatorio("PiP com menu lento (60ms)", 22.0, 6.0, true); salvarTela("pip");

    pip_selecionado = true; menu_video_aberto = true; rodarPor(1.0, 60); salvarTela("pip_menu"); rodarPor(0.2, 60); menu_video_aberto = false; pip_selecionado = false;
    video_minimizado = false; rodarPor(0.5, 0);

    double v0 = video_time_current; video_pausado = true; rodarPor(1.5, 0);
    double v1 = video_time_current; checar("pausa congela o tempo", fabs(v1 - v0) < 0.2);
    video_pausado = false; rodarPor(0.3, 0); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois da pausa", 22.0, 3.0, true);

    double a = video_time_current; pularParaTempo(a + 10.0); rodarPor(0.8, 0);
    checar("avancar 10s", fabs(video_time_current - (a + 10.8)) < 0.5); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois de avancar", 22.0, 3.0, true);

    a = video_time_current; pularParaTempo(a - 7.0); rodarPor(0.8, 0);
    checar("voltar 7s", fabs(video_time_current - (a - 6.2)) < 0.5); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois de voltar", 22.0, 3.0, true);

    a = video_time_current; telaSemVideo = true; rodarPor(1.0, 5); telaSemVideo = false; rodarPor(0.4, 0);
    checar("tela sem video pausa e retoma do ponto", fabs(video_time_current - (a + 0.4)) < 0.5); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois da tela sem video", 22.0, 3.0, true);

    video_fps_multiplier = 2.0; a = video_time_current; rodarPor(3.0, 0);
    checar("velocidade 2x", fabs((video_time_current - a) / 3.0 - 2.0) < 0.3);
    video_fps_multiplier = 1.0; rodarPor(1.0, 0); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois de voltar a 1x", 22.0, 3.0, true);

    pularParaTempo(10.0); rodarPor(0.3, 0);
    for (int i = 0; i < 6; i++) { pularParaTempo(video_time_current + 3.0); rodarPor(0.05, 0); }
    rodarPor(1.0, 0); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois de 6 buscas rapidas", 22.0, 3.0, true);

    double total = video_time_total; pularParaTempo(total - 2.0); rodarPor(3.5, 0);
    checar("repete ao chegar no fim", video_time_current < 3.0); limparMedidas();
    rodarPor(3.0, 0); relatorio("depois de repetir", 22.0, 3.0, true);
    fechar();
    checar("parou", !videoRodando);

    // O arquivo inteiro cabe na leitura adiantada: tem que tocar ate o fim antes de repetir.
    iniciarVideoMP4("media/sd43.mp4");
    double maior = 0; bool repetiu = false;
    for (int i = 0; i < 95; i++) { rodarPor(0.1, 0); if (video_time_current > maior) maior = video_time_current; if (maior > 7.0 && video_time_current < 2.0) repetiu = true; }
    printf("    tempo maximo antes de repetir: %.2fs de %.2fs\n", maior, video_time_total);
    checar("toca ate o fim antes de repetir", maior > video_time_total - 0.6);
    checar("depois do fim recomeca", repetiu);
    fechar();
}
static void testeArquivo(const char* arq, double fps, double seg, bool sync, const char* tela) {
    iniciarVideoMP4(arq);
    if (!videoRodando) { printf("[FALHA] nao abriu %s\n", arq); falhas++; return; }
    int rasgadosAntes = rasgados;
    rodarPor(1.0, 0); limparMedidas(); rodarPor(seg, 0); relatorio(arq, fps, seg, sync);
    if (tela) salvarTela(tela);
    video_minimizado = true; rodarPor(1.0, 30);
    printf("    rasgados neste arquivo: %d\n", rasgados - rasgadosAntes);
    if (strstr(arq, "semaudio")) rasgados = rasgadosAntes;  // padrao colorido: os 3 pontos diferem de verdade
    char n[128]; if (tela) { snprintf(n, sizeof(n), "%s_pip", tela); salvarTela(n); }
    fechar();
}
// ---------- controle simulado (mesma logica do main.cpp) ----------
static unsigned int ultimosB = 0, consumidos = 0; static int ultimoComando = 0;
static void tick(unsigned int botoes, int lx = 128, int ly = 128) {
    OrbisPadData d; d.buttons = botoes; d.leftStick.x = (uint8_t)lx; d.leftStick.y = (uint8_t)ly; d.rightStick.x = 128; d.rightStick.y = 128;
    unsigned int fis = botoes; consumidos &= fis;
    unsigned int btn = fis & ~ultimosB; d.buttons &= ~consumidos;
    bool cross = false, circle = false;
    if ((btn & ORBIS_PAD_BUTTON_CIRCLE) && videoRodando && !menu_video_aberto && !video_minimizado) {
        if (!fecharControlesPlayer()) { bloqueio_fechar_video = false; pararVideo(); }
        circle = true;
    }
    if (videoRodando) processarControlesMenuVideo(btn, &d, cross, circle);
    if (cross) d.buttons &= ~ORBIS_PAD_BUTTON_CROSS;
    if (circle) d.buttons &= ~ORBIS_PAD_BUTTON_CIRCLE;
    consumidos |= fis & ~d.buttons; ultimosB = fis;
    if (comando_trocar_video) { ultimoComando = comando_trocar_video; comando_trocar_video = 0; }
    definirApresentacaoVideo(videoRodando);
    Sleep(33);
}
static void segurar(unsigned int b, int ms, int lx = 128, int ly = 128) { for (int t = 0; t < ms; t += 34) tick(b, lx, ly); }
static void soltar(int ms) { segurar(0, ms); }
static void toque(unsigned int b) { segurar(b, 80); soltar(80); }
static void testePlayer() {
    const unsigned int R1 = ORBIS_PAD_BUTTON_R1, L1 = ORBIS_PAD_BUTTON_L1, R2 = ORBIS_PAD_BUTTON_R2, L2 = ORBIS_PAD_BUTTON_L2;
    const unsigned int X = ORBIS_PAD_BUTTON_CROSS, O = ORBIS_PAD_BUTTON_CIRCLE, TRI = ORBIS_PAD_BUTTON_TRIANGLE;
    const unsigned int ESQ = ORBIS_PAD_BUTTON_LEFT, DIR = ORBIS_PAD_BUTTON_RIGHT, BAIXO = ORBIS_PAD_BUTTON_DOWN, CIMA = ORBIS_PAD_BUTTON_UP;
    printf("\n=== Controles do player\n");
    iniciarVideoMP4("media/sync720.mp4"); soltar(1000);

    ultimoComando = 0; toque(R1); soltar(600); checar("R1 uma vez nao troca", ultimoComando == 0);
    toque(R1); toque(R1); checar("R1 duas vezes: proximo", ultimoComando == 1); soltar(600);
    ultimoComando = 0; toque(L1); toque(L1); checar("L1 duas vezes: anterior", ultimoComando == -1); soltar(600);

    double v = video_time_current; segurar(R2, 1200);
    checar("segurar R2: 2x", video_fps_multiplier == 2.0); soltar(100);
    checar("soltar R2: volta a 1x", video_fps_multiplier == 1.0); soltar(800);
    printf("    avanco com R2 em 1,2 s reais: %.1fs\n", video_time_current - v);

    v = video_time_current; toque(R2); toque(R2); soltar(30);
    checar("R2 duas vezes: +10s", fabs(video_time_current - (v + 10.0)) < 1.0);
    v = video_time_current; toque(R2); toque(R2); toque(R2); soltar(30);
    checar("R2 seguido continua pulando", video_time_current - v > 18.0); soltar(900);

    v = video_time_current; toque(L2); toque(L2); soltar(30);
    checar("L2 duas vezes: -10s", fabs(video_time_current - (v - 10.0)) < 1.0); soltar(900);
    v = video_time_current; segurar(L2, 1700); soltar(100); soltar(700);
    checar("segurar L2: volta no tempo", video_time_current < v - 2.0);

    toque(X); checar("X pausa", video_pausado); toque(X); checar("X continua", !video_pausado);
    soltar(4500); checar("barra some sozinha", !playerControlesVisiveis());

    v = video_time_current; segurar(0, 100, 30, 128); soltar(100); checar("analogico mostra a barra", playerControlesVisiveis());
    checar("primeiro toque so mostra", fabs(video_time_current - v) < 1.0);
    segurar(DIR, 900); salvarTela("player_barra"); soltar(700);
    checar("mover na barra nao salta sozinho", video_time_current < v + 4.0);
    toque(O); soltar(600); checar("Bolinha cancela e mantem a barra", videoRodando && playerControlesVisiveis() && video_time_current < v + 5.0);
    segurar(DIR, 900); soltar(300); toque(X); soltar(300);
    checar("X confirma a nova posicao", video_time_current > v + 8.0); printf("    cursor levou de %.1f para %.1f\n", v, video_time_current);
    pularParaTempo(20.0); soltar(900); v = video_time_current; segurar(ESQ, 400); soltar(100); toque(BAIXO); soltar(100); toque(CIMA); soltar(500);
    checar("descer aos icones cancela o cursor", fabs(video_time_current - (v + 1.5)) < 1.0);

    toque(BAIXO); toque(DIR); toque(DIR); toque(DIR); salvarTela("player_icones");
    toque(X); int vol = video_volume; segurar(ESQ, 400); checar("volume ajusta", video_volume < vol); toque(X);
    toque(DIR); int antes = video_volume; toque(X); checar("mudo", video_volume == 0); salvarTela("player_mudo"); toque(X); checar("sai do mudo", video_volume == antes);
    toque(DIR); segurar(X, 500); soltar(100); checar("icone tela pequena", video_minimizado);
    video_minimizado = false; soltar(100);

    toque(CIMA); toque(ESQ); checar("ainda sem menu", !menu_video_aberto);
    toque(BAIXO); for (int i = 0; i < 8; i++) toque(DIR);
    segurar(X, 300); soltar(100);
    checar("tres pontos abre o menu principal", menu_video_aberto && vState == V_MAIN);
    toque(O); checar("Bolinha fecha o menu e mantem o video", !menu_video_aberto && videoRodando);

    int trocas = 0; bool ant = menu_video_aberto;
    for (int i = 0; i < 30; i++) { tick(TRI); if (menu_video_aberto != ant) { trocas++; ant = menu_video_aberto; } }
    checar("segurar Triangulo abre uma vez so", trocas == 1); soltar(100); toque(TRI);

    segurar(0, 100, 128, 30); soltar(100); checar("mostra de novo", playerControlesVisiveis());
    toque(O); checar("Bolinha so fecha a barra", videoRodando && !playerControlesVisiveis());
    toque(O); checar("segunda Bolinha fecha o video", !videoRodando);
}
int main(int argc, char** argv) {
    QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&base); timeBeginPeriod(1);
    for (int i = 0; i < 4; i++) { fb[i] = (uint32_t*)malloc(BYTES_TELA); memset(fb[i], 0, BYTES_TELA); }
    pthread_t tv, ta; pthread_create(&tv, NULL, vsyncThread, NULL); pthread_create(&ta, NULL, audioThread, NULL);
    carregarFonte();
    const char* modo = argc > 1 ? argv[1] : "tudo";
    if (!strcmp(modo, "player")) testePlayer();
    if (!strcmp(modo, "tudo") || !strcmp(modo, "completo")) testeCompleto("media/sync720.mp4");
    if (!strcmp(modo, "tudo") || !strcmp(modo, "formatos")) {
        printf("\n=== Formatos\n");
        testeArquivo("media/sync1080p60.mp4", 55.0, 4, true, "f1080p60");
        testeArquivo("media/sd43.mp4", 23.0, 4, true, "fsd43");
        testeArquivo("media/impar.mp4", 22.0, 4, true, "fimpar");
        testeArquivo("media/largo.mp4", 22.0, 4, true, "flargo");
        testeArquivo("media/fullrange.mp4", 22.0, 3, false, "ffullrange");
        testeArquivo("media/hevc10.mkv", 22.0, 4, true, "fhevc10");
        testeArquivo("media/mpeg4.avi", 23.0, 3, false, "fmpeg4");
        testeArquivo("media/semaudio.mp4", 22.0, 3, false, "fsemaudio");
        testeArquivo("media/sync4k.mp4", 0, 4, false, "f4k");
    }
    if (!strcmp(modo, "tudo") || !strcmp(modo, "estresse")) {
        printf("\n=== Estresse (abre/fecha, PiP e buscas aleatorias)\n");
        size_t m0 = 0;
        for (int i = 0; i < 40; i++) {
            iniciarVideoMP4(i % 2 ? "media/sync720.mp4" : "media/sync1080p60.mp4");
            for (int k = 0; k < 6; k++) {
                int a = rand() % 5;
                if (a == 0) video_minimizado = !video_minimizado;
                if (a == 1) pularParaTempo((rand() % 1000) / 1000.0 * video_time_total);
                if (a == 2) video_pausado = !video_pausado;
                if (a == 3) telaSemVideo = !telaSemVideo;
                rodarPor(0.02 * (rand() % 8), 5);
            }
            telaSemVideo = false; video_pausado = false;
            fechar();
            if (i == 5) m0 = memoriaMB();
        }
        size_t m1 = memoriaMB();
        printf("memoria apos 6 ciclos: %zu MB, apos 40: %zu MB\n", m0, m1);
        checar("sem vazamento de memoria", m1 < m0 + 40);
        checar("sobreviveu a 40 ciclos sem travar", true);
    }
    if (!strcmp(modo, "largo3")) { for (int i = 0; i < 3; i++) testeArquivo("media/largo.mp4", 22.0, 5, true, NULL); }
    if (!strcmp(modo, "pesado")) {
        printf("\n=== 1080p 8 Mbps com ruido\n");
        iniciarVideoMP4("media/pesado1080.mp4");
        rodarPor(1.5, 0); limparMedidas(); rodarPor(8.0, 0); relatorio("tela cheia 1080p pesado", 22.0, 8.0, false);
        video_minimizado = true; rodarPor(1.0, 60); limparMedidas(); rodarPor(5.0, 60); relatorio("PiP 1080p pesado, menu 60ms", 22.0, 5.0, false);
        fechar();
    }
    if (!strcmp(modo, "lento")) {
        printf("\n=== CPU limitada a 1 nucleo (simula decodificacao que nao da conta)\n");
        SetProcessAffinityMask(GetCurrentProcess(), 1);
        iniciarVideoMP4("media/sync4k.mp4");
        rodarPor(2.0, 0); limparMedidas(); rodarPor(12.0, 0);
        relatorio("4K em 1 nucleo: tempo real?", 0, 12.0, false);
        int n = (int)flipsNovos.size();
        double vel = n > 1 ? (flipsNovos[n - 1].pts - flipsNovos[0].pts) / ((flipsNovos[n - 1].t - flipsNovos[0].t) / 1e6) : 0;
        checar("nao vira camera lenta (descarta quadros)", vel > 0.85 && vel < 1.35 && fabs(video_time_current - audio_time_current) < 1.0);
        fechar();
    }
    printf("\nFlips verificados: %d | quadros rasgados: %d\n", flipsVerificados, rasgados);
    if (rasgados) falhas++;
    printf("FALHAS: %d\n", falhas);
    simViva = false; pthread_join(tv, NULL); pthread_join(ta, NULL);
    return falhas ? 1 : 0;
}
