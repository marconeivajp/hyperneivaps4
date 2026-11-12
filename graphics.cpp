#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Headers do SDK do PS4 (OpenOrbis) necessários para o vídeo
#ifndef _WIN32
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#else
#include <SDL2/SDL.h>
#endif

// INCLUSÃO DO LEITOR DE PDF E MANGÁ
#include "pdf.h"
#include "libretro_bridge.h"

extern bool menuEmuladorAtivo;

// Instanciação das variáveis da fonte
stbtt_fontinfo font;
int temF = 0;

// --- VARIÁVEIS INTERNAS DE VÍDEO ---
int video = 0;
int bA = 0; 
void* buffers[2];

// V39: Backup para o "Frozen Frame" (Global para V58)
uint32_t* backupEmuFrame = NULL;

#ifdef _WIN32
SDL_Window* sdlWindow = NULL;
SDL_Renderer* sdlRenderer = NULL;
SDL_Texture* sdlTexture = NULL;
uint32_t* screenBuffer = NULL;
#endif

// =========================================================================
// SISTEMA DE VÍDEO DO PS4
// =========================================================================

void inicializarVideo() {
#ifdef _WIN32
    // SDL_Init movido para o main()
    sdlWindow = SDL_CreateWindow("Hyper Winds Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_SHOWN);
    if (!sdlWindow) {
        printf("Erro ao criar janela SDL: %s\n", SDL_GetError());
        exit(1);
    }
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer) {
        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
    }
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 1920, 1080);
    screenBuffer = (uint32_t*)malloc(1920 * 1080 * 4);
    memset(screenBuffer, 0, 1920 * 1080 * 4);

    backupEmuFrame = (uint32_t*)malloc(1920 * 1080 * 4);
    if (backupEmuFrame) memset(backupEmuFrame, 0, 1920 * 1080 * 4);
#else
    video = sceVideoOutOpen(255, 0, 0, NULL);

    size_t bSz = ((1920 * 1080 * 4) + 0x1FFFFF) & ~0x1FFFFF;
    off_t ph;

    sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz * 2, 2097152, 2, &ph);
    void* vM = NULL;
    sceKernelMapDirectMemory(&vM, bSz * 2, 0x33, 0, ph, 2097152);

    buffers[0] = vM;
    buffers[1] = (void*)((uint8_t*)vM + bSz);

    backupEmuFrame = (uint32_t*)malloc(1920 * 1080 * 4);
    if (backupEmuFrame) memset(backupEmuFrame, 0, 1920 * 1080 * 4);

    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, 1920, 1080, 1920);
    sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr);
#endif
}

#ifndef _WIN32
// =========================================================================
// APRESENTAÇÃO DE VÍDEO
// A interface vai para uma de três camadas em RAM (uma sendo desenhada, uma pronta e uma em
// exibição), para o menu e o vídeo nunca esperarem um pelo outro. A saída usa dois buffers
// próprios write-combining, o tipo de memória feito para a CPU só escrever.
// =========================================================================
#define BYTES_TELA (1920 * 1080 * 4)
#define CAMADA_NOVA 4

static uint32_t* camadasUI[3] = { NULL, NULL, NULL };
static volatile bool desenhandoNaCamada = false;
static int camadaEscrita = 0, camadaExibida = 1, camadaPronta = 2;
static bool camadaComConteudo = false;

static void* buffersSaida[2] = { NULL, NULL };
static int indiceBaseSaida = -1;
static bool saidaDedicada = false;
static int saidaBack = 0, saidaExibida = -1;

static void prepararBuffersSaida() {
    if (indiceBaseSaida >= 0) return;

    size_t bSz = (BYTES_TELA + 0x1FFFFF) & ~0x1FFFFF;
    off_t ph;
    void* vM = NULL;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz * 2, 2097152, ORBIS_KERNEL_WC_GARLIC, &ph) == 0) {
        if (sceKernelMapDirectMemory(&vM, bSz * 2, 0x33, 0, ph, 2097152) == 0 && vM) {
            memset(vM, 0, bSz * 2);
            buffersSaida[0] = vM;
            buffersSaida[1] = (void*)((uint8_t*)vM + bSz);

            OrbisVideoOutBufferAttribute attr;
            memset(&attr, 0, sizeof(attr));
            sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, 1920, 1080, 1920);
            if (sceVideoOutRegisterBuffers(video, 2, buffersSaida, 2, &attr) >= 0) {
                indiceBaseSaida = 2;
                saidaDedicada = true;
                return;
            }
        }
        else sceKernelReleaseDirectMemory(ph, bSz * 2);
    }

    // Sem os buffers próprios, o vídeo divide os buffers do menu: funciona, só é mais lento.
    buffersSaida[0] = buffers[0];
    buffersSaida[1] = buffers[1];
    indiceBaseSaida = 0;
    saidaDedicada = false;
}

// As camadas nunca são liberadas: threads de download também desenham nelas (atualizarBarra)
// e podem estar escrevendo no instante em que o vídeo fecha.
bool graficosPrepararApresentacao() {
    prepararBuffersSaida();
    for (int i = 0; i < 3; i++) {
        if (!camadasUI[i]) camadasUI[i] = (uint32_t*)malloc(BYTES_TELA);
        if (!camadasUI[i]) return false;
    }
    return true;
}

// ---- Cópia da tela para o navegador (network.cpp) ----
// A memória de vídeo é lenta de ler. Lida direto do buffer exibido, a leitura durava mais
// que um quadro, e o laço já desenhava o quadro seguinte no mesmo buffer: o navegador via
// quadros pela metade e a imagem piscava. Agora quem desenha copia uma faixa do quadro
// pronto logo antes do flip; o quadro inteiro (640x360 ou 960x540) fecha em poucos quadros.
static uint8_t* capturaRgb = NULL;
static volatile bool capturaLigada = false;
static volatile unsigned capturaVoltas = 0;
// Escala: 2 = 960x540, 3 = 640x360. A pedida só vale no começo de um quadro, para não misturar.
static volatile int capturaEscalaPedida = 3, capturaEscalaPronta = 3;

void graficosCapturaLigar(bool ligar, int escala) {
    if (ligar && !capturaRgb) capturaRgb = (uint8_t*)malloc(960 * 540 * 3);
    capturaEscalaPedida = escala == 2 ? 2 : 3;
    capturaLigada = ligar && capturaRgb;
}
int graficosCapturaEscala() { return capturaEscalaPronta; }
unsigned graficosCapturaVoltas() { return capturaVoltas; }
const uint8_t* graficosCapturaRgb() { return capturaRgb; }

static uint64_t capturaAgoraUs() {
#ifdef _WIN32
    return (uint64_t)SDL_GetTicks() * 1000;
#else
    return sceKernelGetProcessTime();
#endif
}


// Leitura com movntdqa (SSE4.1): na memória de vídeo (write-combining) busca 64 bytes de uma vez,
// em vez de uma leitura lenta por instrução como o memcpy.
#include <smmintrin.h>
static void capturaCopiarLinha(uint32_t* destino, const uint32_t* origem) {
    __m128i* o = (__m128i*)origem;
    __m128i* d = (__m128i*)destino;
    for (int i = 0; i < 1920 * 4 / 16; i += 4) {
        __m128i a = _mm_stream_load_si128(o + i), b = _mm_stream_load_si128(o + i + 1);
        __m128i c = _mm_stream_load_si128(o + i + 2), e = _mm_stream_load_si128(o + i + 3);
        _mm_store_si128(d + i, a); _mm_store_si128(d + i + 1, b); _mm_store_si128(d + i + 2, c); _mm_store_si128(d + i + 3, e);
    }
}

// Leitura em paralelo: medido no console, uma thread só lia uns 40 MB/s da memória de vídeo
// (0,6 quadro por segundo). Três threads leem o quadro que acabou de ir para a tela, cada uma
// uma linha a cada três, enquanto o laço desenha no outro buffer.
#include <pthread.h>
#define CAP_TRAB 3
static pthread_mutex_t capTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t capSinal = PTHREAD_COND_INITIALIZER;
static const uint32_t* volatile capAlvo = NULL;
static volatile int capPendentes = 0;
static volatile bool capInvalida = false;
static unsigned capGeracao = 0;
static int capE = 3;
static uint64_t capInicio = 0, capUltimo = 0;
static volatile int capUltimaMs = 0;
static bool capThreadsCriadas = false;

int graficosCapturaMs() { return capUltimaMs; }

static void* capTrabalhador(void* arg) {
    int k = (int)(intptr_t)arg;
    unsigned vista = 0;
    static uint32_t linhas[CAP_TRAB][1920] __attribute__((aligned(64)));
    uint32_t* linha = linhas[k];
    for (;;) {
        pthread_mutex_lock(&capTrava);
        while (capGeracao == vista) pthread_cond_wait(&capSinal, &capTrava);
        vista = capGeracao;
        const uint32_t* tela = capAlvo;
        int e = capE;
        pthread_mutex_unlock(&capTrava);
        const int W = 1920 / e, H = 1080 / e;
        for (int y = k; y < H && tela && capturaRgb; y += CAP_TRAB) {
            const uint32_t* origem = tela + (size_t)y * e * 1920;
            if (((uintptr_t)origem & 15) == 0) capturaCopiarLinha(linha, origem);
            else memcpy(linha, origem, 1920 * 4);
            uint8_t* d = capturaRgb + (size_t)y * W * 3;
            for (int x = 0; x < W; x++) {
                uint32_t px = linha[x * e];
                d[x * 3] = (uint8_t)(px >> 16); d[x * 3 + 1] = (uint8_t)(px >> 8); d[x * 3 + 2] = (uint8_t)px;
            }
        }
        pthread_mutex_lock(&capTrava);
        if (--capPendentes == 0) {
            capUltimaMs = (int)((capturaAgoraUs() - capInicio) / 1000);
            if (!capInvalida) { capturaEscalaPronta = e; capturaVoltas++; }
        }
        pthread_mutex_unlock(&capTrava);
    }
    return NULL;
}

// Chamada com o quadro pronto, logo antes de ele ir para a tela.
static void capturarFaixa(const uint32_t* tela) {
    // Uma leitura ainda em curso no outro buffer: o laço vai desenhar nele depois deste flip.
    // Espera um pouco; se passar do limite, o quadro lido sai torto e é descartado.
    if (capPendentes > 0 && capAlvo != tela) {
        uint64_t limite = capturaAgoraUs() + 60000;
        while (capPendentes > 0 && capturaAgoraUs() < limite) {
#ifndef _WIN32
            sceKernelUsleep(300);
#endif
        }
        if (capPendentes > 0) capInvalida = true;
    }
    if (!capturaLigada || !tela || !capturaRgb || capPendentes > 0) return;
    uint64_t agora = capturaAgoraUs();
    if (agora - capUltimo < 45000) return;   // até uns 20 quadros por segundo
    if (!capThreadsCriadas) {
        capThreadsCriadas = true;
        for (int i = 0; i < CAP_TRAB; i++) {
            pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256 * 1024);
            pthread_t th;
            if (pthread_create(&th, &at, capTrabalhador, (void*)(intptr_t)i) == 0) pthread_detach(th);
            pthread_attr_destroy(&at);
        }
    }
    pthread_mutex_lock(&capTrava);
    capAlvo = tela;
    capE = capturaEscalaPedida;
    capInvalida = false;
    capPendentes = CAP_TRAB;
    capInicio = capUltimo = agora;
    capGeracao++;
    pthread_cond_broadcast(&capSinal);
    pthread_mutex_unlock(&capTrava);
}

void graficosEntrarApresentacao() {
    camadaEscrita = 0; camadaExibida = 1; camadaPronta = 2;
    camadaComConteudo = false;
    saidaExibida = -1;
    saidaBack = saidaDedicada ? 0 : bA;
    desenhandoNaCamada = true;
}

void graficosSairApresentacao() {
    desenhandoNaCamada = false;
    if (!saidaDedicada) bA = saidaBack;
}

uint32_t* graficosPegarCamada(bool* nova) {
    *nova = false;
    if (__atomic_load_n(&camadaPronta, __ATOMIC_ACQUIRE) & CAMADA_NOVA) {
        camadaExibida = __atomic_exchange_n(&camadaPronta, camadaExibida, __ATOMIC_ACQ_REL) & 3;
        camadaComConteudo = true;
        *nova = true;
    }
    return camadaComConteudo ? camadasUI[camadaExibida] : NULL;
}

uint32_t* graficosBufferBack() { return (uint32_t*)buffersSaida[saidaBack]; }
int graficosIndiceBack() { return saidaBack; }

void graficosFlip() {
    capturarFaixa((const uint32_t*)buffersSaida[saidaBack]);
    saidaExibida = indiceBaseSaida + saidaBack;
    sceVideoOutSubmitFlip(video, saidaExibida, 1, 0);
    saidaBack ^= 1;
}

// Repete o buffer que já está na tela: mantém o ritmo do vsync sem redesenhar nada.
bool graficosRepetirFlip() {
    if (saidaExibida < 0) return false;
    int exibido = saidaExibida - indiceBaseSaida;
    if (exibido >= 0 && exibido < 2) capturarFaixa((const uint32_t*)buffersSaida[exibido]);   // nada muda: lê o que está na tela
    return sceVideoOutSubmitFlip(video, saidaExibida, 1, 0) >= 0;
}

bool graficosFlipPendente() { return sceVideoOutIsFlipPending(video) > 0; }
#endif

uint32_t* obterBufferVideo() {
#ifdef _WIN32
    return screenBuffer;
#else
    if (desenhandoNaCamada) return camadasUI[camadaEscrita];
    return (uint32_t*)buffers[bA];
#endif
}

// Buffer que está na tela agora, para a imagem da tela que vai para a IA (network.cpp).
// Lido sem trava: um quadro com "rasgo" de vez em quando não importa para quem só assiste.
uint32_t* graficosTelaExibida() {
#ifdef _WIN32
    return screenBuffer;
#else
    if (desenhandoNaCamada) {
        int s = saidaExibida - indiceBaseSaida;
        if (saidaExibida >= 0 && s >= 0 && s < 2) return (uint32_t*)buffersSaida[s];
        return camadasUI[camadaExibida];
    }
    return (uint32_t*)buffers[(bA + 1) % 2];
#endif
}

void submeterTela() {
#ifdef _WIN32
    SDL_UpdateTexture(sdlTexture, NULL, screenBuffer, 1920 * 4);
    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            exit(0);
        }
    }
#else
    if (desenhandoNaCamada) {
        camadaEscrita = __atomic_exchange_n(&camadaPronta, camadaEscrita | CAMADA_NOVA, __ATOMIC_ACQ_REL) & 3;
        sceKernelUsleep(16000);
        return;
    }
    capturarFaixa((const uint32_t*)buffers[bA]);
    sceVideoOutSubmitFlip(video, bA, 1, 0);
    bA = (bA + 1) % 2;
    sceKernelUsleep(16000);
#endif
}

void submeterTelaSemPausa() {
#ifdef _WIN32
    submeterTela();
#else
    capturarFaixa((const uint32_t*)buffers[bA]);
    sceVideoOutSubmitFlip(video, bA, 1, 0);
    bA = (bA + 1) % 2;
    // V52.2: Sincronização inteligente: só atrasa no menu/pausa para evitar flicker
    if (bridge_esta_pausado() || menuEmuladorAtivo) sceKernelUsleep(16000); 
#endif
}

// =========================================================================
// FUNÇÕES DE DESENHO ORIGINAIS
// =========================================================================

void desenharRedimensionado(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY) {
    if (!img || !pixels || dW <= 0 || dH <= 0) return;
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; if (pY < 0 || pY >= 1080) continue;
        for (int x = 0; x < dW; x++) {
            int pX = posX + x; if (pX < 0 || pX >= 1920) continue;
            int oX = (x * imgW) / dW; int oY = (y * imgH) / dH;
            int idx = (oY * imgW + oX) * 4; uint8_t a = img[idx + 3];
            if (a > 0) pixels[pY * 1920 + pX] = (a << 24) | (img[idx + 0] << 16) | (img[idx + 1] << 8) | img[idx + 2];
        }
    }
}

void desenharRedimensionadoEspelhado(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY, bool espelharH, bool espelharV) {
    if (!img || !pixels || dW <= 0 || dH <= 0) return;
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; if (pY < 0 || pY >= 1080) continue;
        for (int x = 0; x < dW; x++) {
            int pX = posX + x; if (pX < 0 || pX >= 1920) continue;
            int oX = espelharH ? ((dW - 1 - x) * imgW) / dW : (x * imgW) / dW;
            int oY = espelharV ? ((dH - 1 - y) * imgH) / dH : (y * imgH) / dH;
            int idx = (oY * imgW + oX) * 4; uint8_t a = img[idx + 3];
            if (a > 0) pixels[pY * 1920 + pX] = (a << 24) | (img[idx + 0] << 16) | (img[idx + 1] << 8) | img[idx + 2];
        }
    }
}

void desenharDiscoRedondo(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY) {
    if (!img || !pixels || dW <= 0 || dH <= 0) return;
    float r = dW / 2.0f;
    for (int y = 0; y < dH; y++) {
        int pY = posY + y; if (pY < 0 || pY >= 1080) continue;
        for (int x = 0; x < dW; x++) {
            int pX = posX + x; if (pX < 0 || pX >= 1920) continue;
            float dx = x - r; float dy = y - r;
            if ((dx * dx + dy * dy) <= (r * r)) {
                int idx = ((y * imgH / dH) * imgW + (x * imgW / dW)) * 4; uint8_t a = img[idx + 3];
                if (a > 0) pixels[pY * 1920 + pX] = (a << 24) | (img[idx + 0] << 16) | (img[idx + 1] << 8) | img[idx + 2];
            }
        }
    }
}

// Lê um caractere UTF-8. Byte que não forma sequência válida vale por si só.
int hnProximoCaractere(const char* texto, int* i) {
    const unsigned char* t = (const unsigned char*)texto + *i;
    int extras = t[0] >= 0xF0 ? 3 : t[0] >= 0xE0 ? 2 : t[0] >= 0xC0 ? 1 : 0;
    int valor = extras == 3 ? t[0] & 7 : extras == 2 ? t[0] & 15 : extras == 1 ? t[0] & 31 : t[0];
    for (int k = 1; k <= extras; k++) {
        if ((t[k] & 0xC0) != 0x80) { (*i)++; return t[0]; }
        valor = (valor << 6) | (t[k] & 63);
    }
    *i += extras + 1;
    return valor;
}

void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor) {
    if (!temF || !texto || !pixels) return;
    float s = stbtt_ScaleForPixelHeight(&font, (float)tam);
    int asc; stbtt_GetFontVMetrics(&font, &asc, 0, 0); asc = (int)(asc * s);
    int curX = x;
    for (int i = 0; texto[i];) {
        int caractere = hnProximoCaractere(texto, &i);
        int adv, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&font, caractere, &adv, &lsb);
        stbtt_GetCodepointBitmapBox(&font, caractere, s, s, &x0, &y0, &x1, &y1);
        int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            unsigned char* b = (unsigned char*)malloc(w * h);
            stbtt_MakeCodepointBitmap(&font, b, w, h, w, s, s, caractere);
            for (int cy = 0; cy < h; ++cy) for (int cx = 0; cx < w; ++cx) {
                int pX = curX + x0 + cx; int pY = y + asc + y0 + cy;
                if (pX >= 0 && pX < 1920 && pY >= 0 && pY < 1080) {
                    uint8_t alpha = b[cy * w + cx];
                    if (alpha > 30) pixels[pY * 1920 + pX] = cor;
                }
            }
            free(b);
        }
        curX += (int)(adv * s);
    }
}

void desenharPDFnaTela(uint32_t* pixels) {
    if (visualizandoPDF && imgPaginaAtual != NULL) {
        for (int y = 0; y < 1080; y++) for (int x = 0; x < 1920; x++) pixels[y * 1920 + x] = 0xFF000000;
        int dW = (int)(pdfImgW * pdfZoom); int dH = (int)(pdfImgH * pdfZoom);
        int posX = (1920 - dW) / 2 + pdfOffsetX; int posY = (1080 - dH) / 2 + pdfOffsetY;
        desenharRedimensionado(pixels, imgPaginaAtual, pdfImgW, pdfImgH, dW, dH, posX, posY);
        char txtPagina[100]; sprintf(txtPagina, "PAGINA: %d / %d   |   ZOOM: %d%%", pdfPaginaAtual, pdfTotalPaginas, (int)(pdfZoom * 100));
        desenharTexto(pixels, txtPagina, 30, 52, 1022, 0xFF000000);
        desenharTexto(pixels, "[BOLINHA] Fechar | [L1] Voltar | [R1] Avancar | [L2]/[R2] Zoom | [SETAS] Mover", 24, 52, 1052, 0xFF000000);
        desenharTexto(pixels, txtPagina, 30, 50, 1020, 0xFFFFFFFF);
        desenharTexto(pixels, "[BOLINHA] Fechar | [L1] Voltar | [R1] Avancar | [L2]/[R2] Zoom | [SETAS] Mover", 24, 50, 1050, 0xFFAAAAAA);
    }
}

// =========================================================================
// NOVO: SUPORTE PARA O EMULADOR (LIBRETRO) - OTIMIZADO V32/V39/V40
// =========================================================================
extern int gEmuPixelFormat;

void desenharBufferEmulador(const void* data, unsigned width, unsigned height, size_t pitch) {
    uint32_t* pixels = obterBufferVideo();
    if (!pixels || !data) return;

    const int dW = 1920; const int dH = 1080;
    uint32_t stepX = (width << 16) / dW;
    uint32_t stepY = (height << 16) / dH;
    uint32_t currY = 0;

    if (gEmuPixelFormat == 1) { // 32-BIT
        uint32_t* src = (uint32_t*)data;
        for (int y = 0; y < dH; y++) {
            uint32_t srcY = currY >> 16;
            uint32_t* srcRow = (uint32_t*)((uint8_t*)src + (srcY * pitch));
            uint32_t* dstRow = pixels + (y * dW);
            uint32_t currX = 0;
            for (int x = 0; x < dW; x++) {
                dstRow[x] = 0xFF000000 | srcRow[currX >> 16];
                currX += stepX;
            }
            currY += stepY;
        }
    } else { // 16-BIT
        uint16_t* src = (uint16_t*)data;
        for (int y = 0; y < dH; y++) {
            uint32_t srcY = currY >> 16;
            uint16_t* srcRow = (uint16_t*)((uint8_t*)src + (srcY * pitch));
            uint32_t* dstRow = pixels + (y * dW);
            uint32_t currX = 0;
            for (int x = 0; x < dW; x++) {
                uint16_t c565 = srcRow[currX >> 16];
                uint32_t r, g, b;
                if (gEmuPixelFormat == 0) { // 0RGB1555, padrão do libretro
                    r = (c565 & 0x7C00) << 9; g = (c565 & 0x03E0) << 6; b = (c565 & 0x001F) << 3;
                    r |= (r >> 5) & 0xFF0000; g |= (g >> 5) & 0x00FF00; b |= (b >> 5);
                } else {
                    r = (c565 & 0xF800) << 8;
                    g = (c565 & 0x07E0) << 5;
                    b = (c565 & 0x001F) << 3;
                    r |= (r >> 5) & 0xFF0000; g |= (g >> 6) & 0x00FF00; b |= (b >> 5);
                }
                dstRow[x] = 0xFF000000 | r | g | b;
                currX += stepX;
            }
            currY += stepY;
        }
    }
}

// V40: Captura o frame apenas uma vez no momento da pausa
void capturarUltimoFrame() {
    uint32_t* pixels = obterBufferVideo();
    if (pixels && backupEmuFrame) memcpy(backupEmuFrame, pixels, 1920 * 1080 * 4);
}

// =========================================================================
// NOVO: MENU DE PAUSA DO EMULADOR (V33/V39/V40/V41)
// =========================================================================
void desenharMenuEmulador(int selecao) {
    uint32_t* pixels = obterBufferVideo();
    if (!pixels) return;

    // 1. Restaura o frame congelado do backup
    if (backupEmuFrame) memcpy(pixels, backupEmuFrame, 1920 * 1080 * 4);

    // 2. Caixa do Menu (Escurecimento para destaque)
    int boxW = 500; int boxH = 450;
    int boxX = (1920 - boxW) / 2; int boxY = (1080 - boxH) / 2;
    for (int y = boxY; y < boxY + boxH; y++) {
        uint32_t* row = pixels + (y * 1920);
        for (int x = boxX; x < boxX + boxW; x++) {
            uint32_t bg = row[x];
            uint8_t r = (((bg >> 16) & 0xFF) * 5) / 10; // 50% brilho
            uint8_t g = (((bg >> 8) & 0xFF) * 5) / 10;
            uint8_t b = ((bg & 0xFF) * 5) / 10;
            row[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    uint32_t* p = obterBufferVideo();
    if (p && backupEmuFrame) memcpy(backupEmuFrame, p, 1920 * 1080 * 4);
}