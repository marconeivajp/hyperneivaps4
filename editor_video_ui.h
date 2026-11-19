// =========================================================================
// EDITOR DE VÍDEO (tela e controles), no estilo CapCut: prévia em cima, linha do tempo embaixo com
// o cursor fixo no centro, faixas de vídeo/imagem/texto e de áudio, ferramentas no Triângulo.
// Usa o motor do editor_video.h e as mesmas funções de plataforma do editor de imagem
// (edPlatTexto, edPlatLargura, edPlatAgoraMs, edPlatTecladoAbrir/Resultado, edPlatAnalogico, edPlatEmoji).
// Incluído no network.cpp depois do editor_imagem_ui.h.
// =========================================================================
#pragma once
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef EV_PASTA
#define EV_PASTA "/data/HyperNeiva/editor_video"
#endif
#ifndef EV_FONTE_TTF
#define EV_FONTE_TTF "/app0/assets/fonts/font.ttf"
#endif

// ------------------------------------------------------------------ texto e imagens para o motor
static stbtt_fontinfo evFonteTtf; static unsigned char* evFonteDados = NULL;
static void evPlatLog(const char* formato, ...);
static uint8_t* evPlatTexto(const char* texto, int px, uint32_t cor, int* w, int* h) {
    if (!evFonteDados) {
        FILE* f = fopen(EV_FONTE_TTF, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        evFonteDados = (unsigned char*)malloc((size_t)n);
        if (!evFonteDados || fread(evFonteDados, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(evFonteDados); evFonteDados = NULL; return NULL; }
        fclose(f);
        stbtt_InitFont(&evFonteTtf, evFonteDados, 0);
    }
    if (px < 8) px = 8;
    float esc = stbtt_ScaleForPixelHeight(&evFonteTtf, (float)px);
    int asc, desc, gap; stbtt_GetFontVMetrics(&evFonteTtf, &asc, &desc, &gap);
    // Decodifica UTF-8 (acentos) e mede.
    unsigned cps[256]; int n = 0;
    for (const unsigned char* s = (const unsigned char*)texto; *s && n < 256; ) {
        unsigned cp = *s;
        if (cp < 0x80) s++;
        else if ((cp & 0xE0) == 0xC0 && s[1]) { cp = ((cp & 0x1F) << 6) | (s[1] & 0x3F); s += 2; }
        else if ((cp & 0xF0) == 0xE0 && s[1] && s[2]) { cp = ((cp & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); s += 3; }
        else { s++; continue; }
        cps[n++] = cp;
    }
    int largura = 0; for (int i = 0; i < n; i++) { int av, lsb; stbtt_GetCodepointHMetrics(&evFonteTtf, (int)cps[i], &av, &lsb); largura += (int)(av * esc); }
    int borda = px / 14 + 2;   // contorno preto para o texto aparecer em qualquer fundo
    *w = largura + borda * 2 + 4; *h = (int)((asc - desc) * esc) + borda * 2 + 4;
    uint8_t* alfa = (uint8_t*)calloc((size_t)*w * *h, 1);
    uint8_t* img = (uint8_t*)calloc((size_t)*w * *h, 4);
    if (!alfa || !img) { free(alfa); free(img); return NULL; }
    int x = borda + 2, base = (int)(asc * esc) + borda + 2;
    for (int i = 0; i < n; i++) {
        int gw, gh, xo, yo; unsigned char* g = stbtt_GetCodepointBitmap(&evFonteTtf, 0, esc, (int)cps[i], &gw, &gh, &xo, &yo);
        for (int j = 0; j < gh; j++) for (int k = 0; k < gw; k++) {
            int px2 = x + xo + k, py = base + yo + j;
            if (px2 >= 0 && py >= 0 && px2 < *w && py < *h && g[j * gw + k] > alfa[py * *w + px2]) alfa[py * *w + px2] = g[j * gw + k];
        }
        if (g) stbtt_FreeBitmap(g, NULL);
        int av, lsb; stbtt_GetCodepointHMetrics(&evFonteTtf, (int)cps[i], &av, &lsb); x += (int)(av * esc);
    }
    for (int y = 0; y < *h; y++) for (int xx = 0; xx < *w; xx++) {
        int a = alfa[y * *w + xx];
        int contorno = 0;
        for (int dy = -borda; dy <= borda && contorno < 255; dy += borda) for (int dx = -borda; dx <= borda; dx += borda) {
            int yy = y + dy, x2 = xx + dx;
            if (yy >= 0 && x2 >= 0 && yy < *h && x2 < *w && alfa[yy * *w + x2] > contorno) contorno = alfa[yy * *w + x2];
        }
        uint8_t* d = img + ((size_t)y * *w + xx) * 4;
        // cor por cima do contorno preto
        int r = (cor >> 16) & 255, gg = (cor >> 8) & 255, b = cor & 255;
        d[0] = (uint8_t)(r * a / 255); d[1] = (uint8_t)(gg * a / 255); d[2] = (uint8_t)(b * a / 255);
        d[3] = (uint8_t)(a > contorno ? a : contorno);
    }
    free(alfa);
    return img;
}
#include "editor_video.h"   // o motor usa o evPlatTexto acima

// Memória flexível livre (MB) e limpeza dos caches do app (Telegram, gráficos), como o player faz.
static int evMemoriaMB() {
    size_t livre = 0;
    ((int32_t (*)(size_t*))(void*)sceKernelAvailableFlexibleMemorySize)(&livre);
    return (int)(livre / (1024 * 1024));
}
static void evLiberarMemoriaApp() {
    extern void telegramLiberarMemoriaParaVideo(), limparCacheGrafico(), liberarSpriteSheetParaEmu();
    telegramLiberarMemoriaParaVideo();
    limparCacheGrafico();
    liberarSpriteSheetParaEmu();
}

// Arquivo de imagem (stb_image) ou emoji ("emoji:1f600", do pacote do Telegram, 72x72).
static uint8_t* evLerImagemArquivo(const char* caminho, int* w, int* h) {
    if (!strncmp(caminho, "emoji:", 6)) {
        uint32_t* px = (uint32_t*)malloc(72 * 72 * 4);
        if (!px || !edPlatEmoji(caminho + 6, px)) { free(px); return NULL; }
        uint8_t* o = (uint8_t*)px;   // ARGB -> RGBA no lugar
        for (int i = 0; i < 72 * 72; i++) { uint32_t c = px[i]; o[i * 4] = (c >> 16) & 255; o[i * 4 + 1] = (c >> 8) & 255; o[i * 4 + 2] = c & 255; o[i * 4 + 3] = c >> 24; }
        *w = 72; *h = 72;
        return o;
    }
    int c; return stbi_load(caminho, w, h, &c, 4);
}
static void evPlatLog(const char* formato, ...) {
    FILE* f = fopen(EV_PASTA "/editor_log.txt", "a");
    if (!f) return;
    va_list a; va_start(a, formato); vfprintf(f, formato, a); va_end(a);
    fputc('\n', f); fclose(f);
}

// ------------------------------------------------------------------ estado da tela
bool editorVideoAberto = false;
static EvProjeto evP;
static EvRender* evRPrev = NULL;            // pré-visualização (thread da tela)
static double evCursor = 0;                 // instante no cursor (s)
static int evFaixaSel = 0, evClipeSel = -1;
static float evPxPorSeg = 80;               // zoom da linha do tempo
static bool evMudou = false;
enum { EV_M_LINHA = 0, EV_M_MENU, EV_M_ARQUIVOS, EV_M_ANIMAR, EV_M_EXPORTANDO, EV_M_GRAVANDO, EV_M_ESCOLHA, EV_M_TRANSFORMAR };
// Conteúdo escolhido esperando o usuário dizer a faixa e a posição.
struct EvPendente { bool ativo; int tipoClipe; char arquivo[512]; char texto[128]; double dur; int faixa; };
static EvPendente evPend;
static int evOpcFaixas[EV_MAX_FAIXAS + 1];   // faixa de cada item da lista "Em qual faixa" (-1 = nova)
static int evModo = EV_M_LINHA;
static char evAviso[200] = ""; static uint64_t evAvisoAte = 0;
static void evAvisar(const char* t) { snprintf(evAviso, sizeof(evAviso), "%s", t); evAvisoAte = edPlatAgoraMs() + 3500; }
// Prévia
static uint8_t* evPrev = NULL; static int evPrevW = 0, evPrevH = 0; static double evPrevT = -1; static bool evPrevSujo = true;
static bool evTocando = false; static uint64_t evTocarMs = 0; static double evTocarT = 0;
// Som da prévia (thread própria, EvRender próprio)
static volatile bool evSomParar = true; static volatile double evSomT = 0; static pthread_t evSomThread; static bool evSomRodando = false;
// Menus
static int evMenuSel = 0, evMenuTotal = 0, evMenuTopo = 0;
static const char* evMenuItens[40]; static int evMenuAcoes[40]; static char evMenuTitulo[80] = "";
static int evEscolhaTipo = 0;              // o que a lista de escolha decide
// Teclado
static int evTecladoPara = 0;              // 1 texto novo, 2 editar texto, 3 nome do projeto
// Arquivos
struct EvArq { char nome[200]; bool pasta; };
static EvArq* evArqs = NULL; static int evTotalArqs = 0, evArqSel = 0, evArqTopo = 0; static char evArqPasta[512] = ""; static int evArqTipo = 0;   // 0 vídeo, 1 imagem, 2 áudio, 3 projeto
// Exportação
static EvExportacao evExp; static pthread_t evExpThread; static char evExpSaida[300] = ""; static EvProjeto* evExpProjeto = NULL;
// Gravação de narração
static volatile bool evGravando = false, evGravarParar = false; static pthread_t evGravThread; static char evGravArquivo[300] = ""; static double evGravInicio = 0; static volatile double evGravSegundos = 0;

static int evLim(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static void evRet(uint32_t* p, int x, int y, int w, int h, uint32_t c) {
    int x0 = evLim(x, 0, 1920), x1 = evLim(x + w, 0, 1920), y0 = evLim(y, 0, 1080), y1 = evLim(y + h, 0, 1080);
    for (int yy = y0; yy < y1; yy++) { uint32_t* l = p + yy * 1920; for (int xx = x0; xx < x1; xx++) l[xx] = c; }
}
static void evBorda(uint32_t* p, int x, int y, int w, int h, uint32_t c, int e) { evRet(p, x, y, w, e, c); evRet(p, x, y + h - e, w, e, c); evRet(p, x, y, e, h, c); evRet(p, x + w - e, y, e, h, c); }
static void evTexto(uint32_t* p, const char* t, int tam, int x, int y, uint32_t c) { edPlatTexto(p, t, tam, x, y, c); }
static void evTempo(double s, char* o, int n) { if (s < 0) s = 0; int t = (int)(s * 100 + 0.5); snprintf(o, n, "%02d:%02d.%02d", t / 6000, (t / 100) % 60, t % 100); }
static EvClipe* evSelecionado() {
    if (evFaixaSel < 0 || evFaixaSel >= evP.totalFaixas || evClipeSel < 0 || evClipeSel >= evP.faixas[evFaixaSel].total) return NULL;
    return &evP.faixas[evFaixaSel].clipes[evClipeSel];
}
static void evMudar() { evMudou = true; evPrevSujo = true; }
static bool evJuncao = false;                // seleção é a junção antes do pedaço evClipeSel
static bool evArrastando = false;            // cursor sendo arrastado: prévia aproximada (rápida)
static bool evTemJuncao(const EvFaixa* f, int k);

// ------------------------------------------------------------------ prévia em thread própria
// Montar um quadro (decodificar, buscar no vídeo) leva até 1,5 s no PS4. Isso roda nesta thread,
// fora da thread da tela: os controles nunca esperam o vídeo. A tela mostra o último quadro pronto.
static pthread_mutex_t evPvTrava; static pthread_cond_t evPvSinal; static bool evPvIniciada = false;
static pthread_t evPvThread; static bool evPvViva = false;
static EvProjeto* evPvProj = NULL; static bool evPvProjNovo = false;
static bool evPvPedido = false, evPvSair = false, evPvLiberar = false, evPvRapido = false;
static volatile bool evPvOcupada = false;
static double evPvT = 0; static int evPvW = 0, evPvH = 0, evPvSelF = -1, evPvSelC = -1;
static uint8_t* evPvPronto = NULL; static size_t evPvProntoCap = 0; static int evPvProntoW = 0, evPvProntoH = 0;
static int evPvCaixaW = 0, evPvCaixaH = 0;   // tamanho natural da imagem/texto selecionado (para o contorno)
static void* evPvLaco(void*) {
    EvProjeto* proj = (EvProjeto*)calloc(1, sizeof(EvProjeto));
    uint8_t* trab = NULL; size_t trabCap = 0;
    int logProxima = 1;   // registra as próximas montagens (depois de abrir ou de exportar)
    if (!proj) return NULL;
    for (;;) {
        pthread_mutex_lock(&evPvTrava);
        while (!evPvPedido && !evPvSair && !evPvLiberar) pthread_cond_wait(&evPvSinal, &evPvTrava);
        if (evPvSair) { pthread_mutex_unlock(&evPvTrava); break; }
        if (evPvLiberar) {   // exportação ou projeto novo: fecha os arquivos e devolve a memória
            evPvLiberar = false; pthread_mutex_unlock(&evPvTrava);
            if (evRPrev) evRenderLiberar(evRPrev);
            free(trab); trab = NULL; trabCap = 0;
            evPlatLog("PREVIA liberou (memoria %d MB)", evMemoriaMB());
            logProxima = 3;
            continue;
        }
        if (evPvProjNovo && evPvProj) { memcpy(proj, evPvProj, sizeof(EvProjeto)); evPvProjNovo = false; }
        double t = evPvT; int w = evPvW, h = evPvH, sf = evPvSelF, sc = evPvSelC; bool rapido = evPvRapido;
        evPvPedido = false; evPvOcupada = true;
        pthread_mutex_unlock(&evPvTrava);
        size_t precisa = (size_t)w * h * 4;
        if (precisa && trabCap < precisa) { free(trab); trab = (uint8_t*)malloc(precisa); trabCap = trab ? precisa : 0; }
        int cw = 0, ch = 0;
        bool feito = false;
        if (trab && precisa && evRPrev) {
            evRPrev->rapido = rapido;
            // Pouca memória: fecha os arquivos da prévia antes de abrir de novo (sem cair).
            { int mem = evMemoriaMB(); if (mem < 70) { evRenderLiberar(evRPrev); evPlatLog("PREVIA pouca memoria (%d MB): fechou os arquivos", mem); logProxima = 3; } }
            if (logProxima > 0) evPlatLog("PREVIA monta t=%.2f %dx%d (memoria %d MB)", t, w, h, evMemoriaMB());
            evMontarQuadro(proj, evRPrev, t, trab, w, h);
            if (logProxima > 0) { evPlatLog("PREVIA montou (memoria %d MB)", evMemoriaMB()); logProxima--; }
            if (sf >= 0 && sf < proj->totalFaixas && sc >= 0 && sc < proj->faixas[sf].total) {
                const EvClipe* c = &proj->faixas[sf].clipes[sc];
                if (c->tipo == EV_CLIPE_IMAGEM || c->tipo == EV_CLIPE_TEXTO) evRenderImagem(evRPrev, c, h, &cw, &ch);
            }
            feito = true;
        }
        pthread_mutex_lock(&evPvTrava);
        if (feito) {
            uint8_t* x = evPvPronto; size_t xc = evPvProntoCap;
            evPvPronto = trab; evPvProntoCap = trabCap; trab = x; trabCap = xc;
            evPvProntoW = w; evPvProntoH = h; evPvCaixaW = cw; evPvCaixaH = ch;
        }
        evPvOcupada = false;
        pthread_mutex_unlock(&evPvTrava);
    }
    if (evRPrev) evRenderLiberar(evRPrev);
    free(trab); free(proj);
    return NULL;
}
static void evPvIniciar() {
    if (!evPvIniciada) { pthread_mutex_init(&evPvTrava, NULL); pthread_cond_init(&evPvSinal, NULL); evPvIniciada = true; }
    if (evPvViva) return;
    if (!evPvProj) evPvProj = (EvProjeto*)malloc(sizeof(EvProjeto));
    if (!evPvProj) return;
    evPvSair = false; evPvPedido = false; evPvLiberar = false;
    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 1024 * 1024);
    evPvViva = pthread_create(&evPvThread, &at, evPvLaco, NULL) == 0;
    pthread_attr_destroy(&at);
}
static void evPvParar() {
    if (!evPvViva) return;
    pthread_mutex_lock(&evPvTrava); evPvSair = true; pthread_cond_signal(&evPvSinal); pthread_mutex_unlock(&evPvTrava);
    pthread_join(evPvThread, NULL);
    evPvViva = false;
    free(evPvPronto); evPvPronto = NULL; evPvProntoCap = 0; evPvProntoW = evPvProntoH = 0;
}
static void evPvLiberarMemoria() {
    if (!evPvViva) { if (evRPrev) evRenderLiberar(evRPrev); return; }
    pthread_mutex_lock(&evPvTrava); evPvLiberar = true; evPvPedido = false; pthread_cond_signal(&evPvSinal); pthread_mutex_unlock(&evPvTrava);
}
static void evPvPedir(double t, int w, int h, bool rapido, bool projeto, int selF, int selC) {
    if (!evPvViva) return;
    pthread_mutex_lock(&evPvTrava);
    if (projeto && evPvProj) { memcpy(evPvProj, &evP, sizeof(EvProjeto)); evPvProjNovo = true; }
    evPvT = t; evPvW = w; evPvH = h; evPvRapido = rapido; evPvSelF = selF; evPvSelC = selC;
    evPvPedido = true;
    pthread_cond_signal(&evPvSinal);
    pthread_mutex_unlock(&evPvTrava);
}

// ------------------------------------------------------------------ pastas e projeto
static void evCriarPastas() {
    mkdir(EV_PASTA, 0777); mkdir(EV_PASTA "/projetos", 0777); mkdir(EV_PASTA "/exportados", 0777); mkdir(EV_PASTA "/gravacoes", 0777);
}
static void evCaminhoProjeto(char* o, int n) { snprintf(o, n, EV_PASTA "/projetos/%s.hnv", evP.nome); }
static void evSalvar() {
    char c[300]; evCaminhoProjeto(c, sizeof(c));
    if (evProjetoSalvar(&evP, c)) { evMudou = false; char t[200]; snprintf(t, sizeof(t), "Projeto salvo: %.150s", c); evAvisar(t); }
    else evAvisar("Nao consegui salvar o projeto");
}

// ------------------------------------------------------------------ som da prévia
static void* evSomLaco(void*) {
    EvRender* r = (EvRender*)calloc(1, sizeof(EvRender));
    static float mix[1024 * 2]; static int16_t s16[1024 * 2];
    double t = evSomT;
    extern bool appSomEmpurrar(const int16_t* estereo, int quadros, volatile bool* parar);
    while (!evSomParar && r) {
        evMixar(&evP, r, t, 1024, mix);
        for (int i = 0; i < 2048; i++) s16[i] = (int16_t)(mix[i] * 32000);
        if (!appSomEmpurrar(s16, 1024, &evSomParar)) break;
        t += 1024.0 / EV_TAXA_AUDIO;
    }
    if (r) { evRenderLiberar(r); free(r); }
    evSomRodando = false;
    return NULL;
}
static void evSomComecar(double t) {
    if (evSomRodando) return;
    evSomParar = false; evSomT = t; evSomRodando = true;
    if (pthread_create(&evSomThread, NULL, evSomLaco, NULL) != 0) evSomRodando = false;
    else pthread_detach(evSomThread);
}
static void evSomParar_() {
    evSomParar = true;
    extern void appSomLimpar();
    for (int i = 0; i < 100 && evSomRodando; i++) sceKernelUsleep(10000);
    appSomLimpar();
}
static void evTocar(bool tocar) {
    if (tocar == evTocando) return;
    evTocando = tocar;
    if (tocar) { evTocarMs = edPlatAgoraMs(); evTocarT = evCursor; evSomComecar(evCursor); }
    else { evSomParar_(); evPrevSujo = true; }
}

// ------------------------------------------------------------------ narração (microfone, 16 kHz, WAV)
static void evWavCabecalho(FILE* f, uint32_t amostras) {
    uint32_t dados = amostras * 2, taxa = 16000, bps = 32000; uint16_t um = 1, bits = 16, bloco = 2;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); uint32_t tam = 36 + dados; fwrite(&tam, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    uint32_t dezesseis = 16; fwrite(&dezesseis, 4, 1, f); fwrite(&um, 2, 1, f); fwrite(&um, 2, 1, f);
    fwrite(&taxa, 4, 1, f); fwrite(&bps, 4, 1, f); fwrite(&bloco, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dados, 4, 1, f);
}
static void* evGravarLaco(void*) {
    typedef int (*AbrirMic)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef int (*LerMic)(int32_t, void*);
    typedef int (*FecharMic)(int32_t);
    extern int32_t global_uId;
    int mic = ((AbrirMic)sceAudioInOpen)(global_uId, 0, 0, 256, 16000, 0);
    if (mic < 0) mic = ((AbrirMic)sceAudioInOpen)(255, 0, 0, 256, 16000, 0);
    FILE* f = mic >= 0 ? fopen(evGravArquivo, "wb") : NULL;
    if (!f) {
        if (mic >= 0) ((FecharMic)sceAudioInClose)(mic);
        snprintf(evAviso, sizeof(evAviso), mic < 0 ? "Microfone nao encontrado (%x): ligue um fone com microfone no controle" : "Nao consegui criar o arquivo da gravacao", (unsigned)mic);
        evAvisoAte = edPlatAgoraMs() + 5000;
        evGravando = false;
        return NULL;
    }
    evWavCabecalho(f, 0);
    static int16_t lido[256];
    uint32_t total = 0;
    while (!evGravarParar) {
        int n = ((LerMic)sceAudioInInput)(mic, lido);
        if (n < 0) break;
        if (n == 0 || n > 256) n = 256;
        fwrite(lido, 2, (size_t)n, f);
        total += (uint32_t)n;
        evGravSegundos = total / 16000.0;
    }
    ((FecharMic)sceAudioInClose)(mic);
    evWavCabecalho(f, total);
    fclose(f);
    evGravando = false;
    return NULL;
}
static void evGravarComecar() {
    if (evGravando) return;
    char nome[80]; snprintf(nome, sizeof(nome), "narracao_%llu.wav", (unsigned long long)edPlatAgoraMs());
    snprintf(evGravArquivo, sizeof(evGravArquivo), EV_PASTA "/gravacoes/%s", nome);
    evGravInicio = evCursor; evGravSegundos = 0; evGravarParar = false; evGravando = true;
    if (pthread_create(&evGravThread, NULL, evGravarLaco, NULL) != 0) { evGravando = false; evAvisar("Nao consegui iniciar a gravacao"); return; }
    evModo = EV_M_GRAVANDO;
    evTocar(true);   // o vídeo roda enquanto você fala
}
static void evGravarTerminar() {
    evGravarParar = true;
    pthread_join(evGravThread, NULL);
    evTocar(false);
    evModo = EV_M_LINHA;
    if (evGravSegundos < 0.3) { remove(evGravArquivo); evAvisar("Gravacao muito curta"); return; }
    int faixa = -1;
    for (int i = 0; i < evP.totalFaixas && faixa < 0; i++) {
        if (evP.faixas[i].tipo != EV_FAIXA_AUDIO) continue;
        bool livre = true;
        for (int k = 0; k < evP.faixas[i].total; k++) { const EvClipe* o = &evP.faixas[i].clipes[k]; if (o->inicio < evGravInicio + evGravSegundos && evClipeFim(o) > evGravInicio) livre = false; }
        if (livre) faixa = i;
    }
    if (faixa < 0) faixa = evFaixaNova(&evP, EV_FAIXA_AUDIO);
    if (faixa < 0 || !evClipeNovo(&evP, faixa, EV_CLIPE_AUDIO, evGravArquivo, evGravInicio, evGravSegundos)) { evAvisar("Sem espaco para a narracao"); return; }
    evMudar();
    char t[120]; snprintf(t, sizeof(t), "Narracao de %.1f s adicionada", evGravSegundos); evAvisar(t);
}

// ------------------------------------------------------------------ exportação (thread)
static void* evExportarLaco(void*) {
    evExportar(evExpProjeto, evExpSaida, &evExp);
    evExp.terminou = true;
    return NULL;
}
static bool evExportando() { return evModo == EV_M_EXPORTANDO; }
static void evExportarComecar() {
    if (evProjetoDuracao(&evP) < 0.1) { evAvisar("Adicione algum video, imagem ou texto antes de exportar"); return; }
    evTocar(false);
    evSalvar();
    char nome[80]; snprintf(nome, sizeof(nome), "%s_%llu.mp4", evP.nome, (unsigned long long)(edPlatAgoraMs() / 1000));
    for (char* c = nome; *c; c++) if (strchr(" /\\:*?\"<>|", *c)) *c = '_';
    snprintf(evExpSaida, sizeof(evExpSaida), EV_PASTA "/exportados/%s", nome);
    if (!evExpProjeto) evExpProjeto = (EvProjeto*)malloc(sizeof(EvProjeto));
    if (!evExpProjeto) { evAvisar("Sem memoria para exportar"); return; }
    *evExpProjeto = evP;   // cópia: a tela pode continuar mexendo
    memset(&evExp, 0, sizeof(evExp));
    // A prévia fecha os arquivos abertos e o app solta os caches: a exportação precisa da memória.
    evPvLiberarMemoria();
    evLiberarMemoriaApp();
    for (int i = 0; i < 30 && evPvViva && (evPvOcupada || evPvLiberar); i++) sceKernelUsleep(50000);
    int mem = evMemoriaMB();
    evExp.threadsDecodificar = mem >= 170 ? 3 : mem >= 120 ? 2 : 1;   // mais threads só com folga de memória
    evPlatLog("EXPORTAR memoria livre %d MB, %d threads", mem, evExp.threadsDecodificar);
    if (mem < 90) {
        char t[160]; snprintf(t, sizeof(t), "Pouca memoria para exportar (%d MB livres). Feche outras coisas abertas e tente de novo.", mem);
        evAvisar(t); evAvisoAte = edPlatAgoraMs() + 6000; evPrevSujo = true;
        return;
    }
    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 1024 * 1024);
    if (pthread_create(&evExpThread, &at, evExportarLaco, NULL) != 0) { pthread_attr_destroy(&at); evAvisar("Nao consegui iniciar a exportacao"); return; }
    pthread_attr_destroy(&at);
    evModo = EV_M_EXPORTANDO;
    evPlatLog("EXPORTAR %s (%.1f s, %dx%d)", evExpSaida, evProjetoDuracao(&evP), evP.largura, evP.altura);
}

// ------------------------------------------------------------------ arquivos
static bool evExtensao(const char* n, const char* const* lista) {
    const char* x = strrchr(n, '.'); if (!x) return false;
    for (int i = 0; lista[i]; i++) if (!strcasecmp(x, lista[i])) return true;
    return false;
}
static const char* const evExtVideo[] = { ".mp4", ".mkv", ".avi", ".mov", ".webm", ".m4v", ".ts", ".flv", ".wmv", ".mpg", ".3gp", NULL };
static const char* const evExtImagem[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tga", NULL };
static const char* const evExtAudio[] = { ".mp3", ".wav", ".ogg", ".m4a", ".aac", ".flac", ".opus", ".wma", ".mp4", ".mkv", NULL };
static const char* const evExtProjeto[] = { ".hnv", NULL };
// Mesmo começo do Explorar.
static const char* const evRaizes[4][2] = { { "/data/HyperNeiva", "Hyper Neiva" }, { "/", "Raiz" }, { "/mnt/usb0", "USB 0" }, { "/mnt/usb1", "USB 1" } };
static int evCmpArq(const void* a, const void* b) { const EvArq* x = (const EvArq*)a; const EvArq* y = (const EvArq*)b; if (x->pasta != y->pasta) return x->pasta ? -1 : 1; return strcasecmp(x->nome, y->nome); }
static void evListar(const char* pasta) {
    if (!evArqs) evArqs = (EvArq*)malloc(sizeof(EvArq) * 2000);
    if (!evArqs) return;
    snprintf(evArqPasta, sizeof(evArqPasta), "%s", pasta);
    evTotalArqs = 0; evArqSel = 0; evArqTopo = 0;
    if (!pasta[0]) {
        for (int i = 0; i < 4; i++) { snprintf(evArqs[evTotalArqs].nome, 200, "%s", evRaizes[i][0]); evArqs[evTotalArqs++].pasta = true; }
        return;
    }
    const char* const* ext = evArqTipo == 0 ? evExtVideo : evArqTipo == 1 ? evExtImagem : evArqTipo == 2 ? evExtAudio : evExtProjeto;
    DIR* d = opendir(pasta);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && evTotalArqs < 2000) {
        if (e->d_name[0] == '.') continue;
        char c[800]; snprintf(c, sizeof(c), "%s%s%s", pasta, pasta[strlen(pasta) - 1] == '/' ? "" : "/", e->d_name);
        struct stat st; if (stat(c, &st) != 0) continue;
        bool dir = S_ISDIR(st.st_mode);
        if (!dir && !evExtensao(e->d_name, ext)) continue;
        snprintf(evArqs[evTotalArqs].nome, 200, "%s", e->d_name); evArqs[evTotalArqs++].pasta = dir;
    }
    closedir(d);
    qsort(evArqs, (size_t)evTotalArqs, sizeof(EvArq), evCmpArq);
}
static void evAbrirArquivos(int tipo) {
    evArqTipo = tipo;
    evListar(tipo == 3 ? EV_PASTA "/projetos" : "");
    if (!evTotalArqs) evListar("");
    evModo = EV_M_ARQUIVOS;
}
// Duração de um arquivo de mídia (s); 5 s para imagem.
static double evDuracaoArquivo(const char* caminho) {
    AVFormatContext* f = NULL;
    if (avformat_open_input(&f, caminho, NULL, NULL) < 0) return -1;
    avformat_find_stream_info(f, NULL);
    double d = f->duration > 0 ? f->duration / (double)AV_TIME_BASE : -1;
    avformat_close_input(&f);
    return d;
}
// Faixa visual livre no intervalo (cria uma nova se precisar). Para vídeo, prefere a faixa selecionada.
static int evFaixaLivre(int tipo, double ini, double fim, int preferida) {
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < evP.totalFaixas; i++) {
            if (pass == 0 && i != preferida) continue;
            if (evP.faixas[i].tipo != tipo) continue;
            bool livre = true;
            for (int k = 0; k < evP.faixas[i].total; k++) { const EvClipe* o = &evP.faixas[i].clipes[k]; if (o->inicio < fim - 0.001 && evClipeFim(o) > ini + 0.001) livre = false; }
            if (livre) return i;
        }
    return evFaixaNova(&evP, tipo);
}
static void evPerguntarOnde(int tipoClipe, const char* arquivo, const char* texto, double dur);
static void evInserirArquivo(const char* caminho) {
    if (evArqTipo == 3) {
        // O projeto tem ~1 MB: nunca na pilha.
        EvProjeto* novo = (EvProjeto*)malloc(sizeof(EvProjeto));
        if (!novo) { evAvisar("Sem memoria para abrir o projeto"); return; }
        if (!evProjetoAbrir(novo, caminho)) { free(novo); evAvisar("Projeto invalido"); return; }
        evP = *novo; free(novo); evCursor = 0; evFaixaSel = 0; evClipeSel = -1; evJuncao = false; evMudou = false; evPrevSujo = true;
        evPvLiberarMemoria();
        evModo = EV_M_LINHA; evAvisar("Projeto aberto");
        return;
    }
    double dur = evArqTipo == 1 ? 5.0 : evDuracaoArquivo(caminho);
    if (dur <= 0.05) { evAvisar("Nao consegui ler esse arquivo"); return; }
    int tipoClipe = evArqTipo == 0 ? EV_CLIPE_VIDEO : evArqTipo == 1 ? EV_CLIPE_IMAGEM : EV_CLIPE_AUDIO;
    evPerguntarOnde(tipoClipe, caminho, NULL, dur);
}

// ------------------------------------------------------------------ menus
enum {
    EV_A_VIDEO = 1, EV_A_IMAGEM, EV_A_AUDIO, EV_A_TEXTO, EV_A_EMOJI, EV_A_GRAVAR, EV_A_FAIXA_VIDEO, EV_A_FAIXA_AUDIO,
    EV_A_FORMATO, EV_A_SALVAR, EV_A_ABRIR, EV_A_NOVO, EV_A_EXPORTAR, EV_A_SAIR,
    EV_A_DIVIDIR, EV_A_APAGAR, EV_A_DUPLICAR, EV_A_MOVER_CURSOR, EV_A_APARAR_INICIO, EV_A_APARAR_FIM, EV_A_SEPARAR_AUDIO,
    EV_A_VOLUME_MAIS, EV_A_VOLUME_MENOS, EV_A_MUDO, EV_A_TRANSICAO, EV_A_ANIMAR, EV_A_EDITAR_TEXTO, EV_A_COR_TEXTO, EV_A_TAMANHO_MAIS, EV_A_TAMANHO_MENOS,
    EV_A_OCULTAR_FAIXA, EV_A_MUDAR_FAIXA_MUDA, EV_A_TRANSFORMAR, EV_A_TRANS_DUR, EV_A_RESETAR,
};
static void evMenuAdicionar(const char* t, int a) { if (evMenuTotal < 40) { evMenuItens[evMenuTotal] = t; evMenuAcoes[evMenuTotal++] = a; } }
static void evAbrirMenu() {
    evMenuTotal = 0; evMenuSel = 0; evMenuTopo = 0;
    EvClipe* c = evSelecionado();
    if (c) {
        snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Pedaco selecionado");
        if (c->tipo != EV_CLIPE_AUDIO) {
            evMenuAdicionar("Tamanho, posicao e giro", EV_A_TRANSFORMAR);
            evMenuAdicionar("Transicao com o pedaco anterior", EV_A_TRANSICAO);
            if (c->transicao) evMenuAdicionar("Duracao da transicao", EV_A_TRANS_DUR);
        }
        evMenuAdicionar("Dividir no cursor", EV_A_DIVIDIR);
        evMenuAdicionar("Excluir", EV_A_APAGAR);
        evMenuAdicionar("Duplicar (logo depois)", EV_A_DUPLICAR);
        evMenuAdicionar("Mover o inicio para o cursor", EV_A_MOVER_CURSOR);
        evMenuAdicionar("Aparar o inicio ate o cursor", EV_A_APARAR_INICIO);
        evMenuAdicionar("Aparar o fim ate o cursor", EV_A_APARAR_FIM);
        if (c->tipo == EV_CLIPE_VIDEO) evMenuAdicionar(c->semAudio ? "Audio ja separado" : "Separar o audio", EV_A_SEPARAR_AUDIO);
        if (c->tipo == EV_CLIPE_VIDEO || c->tipo == EV_CLIPE_AUDIO) { evMenuAdicionar("Volume +", EV_A_VOLUME_MAIS); evMenuAdicionar("Volume -", EV_A_VOLUME_MENOS); evMenuAdicionar("Silenciar / ativar som", EV_A_MUDO); }
        if (c->tipo != EV_CLIPE_AUDIO) { evMenuAdicionar("Animar (movimento com pontos)", EV_A_ANIMAR); evMenuAdicionar("Voltar tamanho e posicao ao normal", EV_A_RESETAR); }
        if (c->tipo == EV_CLIPE_TEXTO) { evMenuAdicionar("Editar o texto", EV_A_EDITAR_TEXTO); evMenuAdicionar("Cor do texto", EV_A_COR_TEXTO); evMenuAdicionar("Texto maior", EV_A_TAMANHO_MAIS); evMenuAdicionar("Texto menor", EV_A_TAMANHO_MENOS); }
    }
    else {
        snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Projeto");
        if (evFaixaSel >= 0 && evFaixaSel < evP.totalFaixas && evP.faixas[evFaixaSel].total) evMenuAdicionar("(Dica: X num pedaco abre as opcoes dele)", 0);
        evMenuAdicionar("Adicionar video", EV_A_VIDEO);
        evMenuAdicionar("Adicionar imagem", EV_A_IMAGEM);
        evMenuAdicionar("Adicionar musica ou audio", EV_A_AUDIO);
        evMenuAdicionar("Adicionar texto", EV_A_TEXTO);
        evMenuAdicionar("Adicionar emoji", EV_A_EMOJI);
        evMenuAdicionar("Gravar narracao (microfone)", EV_A_GRAVAR);
        evMenuAdicionar("Nova faixa de video", EV_A_FAIXA_VIDEO);
        evMenuAdicionar("Nova faixa de audio", EV_A_FAIXA_AUDIO);
        evMenuAdicionar("Ocultar / mostrar a faixa selecionada", EV_A_OCULTAR_FAIXA);
        evMenuAdicionar("Silenciar / ativar a faixa selecionada", EV_A_MUDAR_FAIXA_MUDA);
        evMenuAdicionar("Formato do video", EV_A_FORMATO);
        evMenuAdicionar("Exportar MP4", EV_A_EXPORTAR);
        evMenuAdicionar("Salvar projeto", EV_A_SALVAR);
        evMenuAdicionar("Abrir projeto", EV_A_ABRIR);
        evMenuAdicionar("Novo projeto", EV_A_NOVO);
        evMenuAdicionar("Sair do editor", EV_A_SAIR);
    }
    evModo = EV_M_MENU;
}
// Listas de escolha (transição, formato, cor, emoji).
enum { EV_E_TRANSICAO = 1, EV_E_FORMATO, EV_E_COR, EV_E_EMOJI, EV_E_FAIXA, EV_E_POSICAO, EV_E_TRANS_DUR };
static const double evTransDur[] = { 0.3, 0.6, 1.0, 1.5, 2.0 };
static const char* const evFormatos[] = { "16:9  1280x720 (HD)", "16:9  1920x1080 (Full HD)", "9:16  720x1280 (celular, Reels/TikTok)", "1:1  1080x1080 (quadrado)", "4:3  960x720" };
static const int evFormatosWH[][2] = { { 1280, 720 }, { 1920, 1080 }, { 720, 1280 }, { 1080, 1080 }, { 960, 720 } };
static const char* const evCores[] = { "Branco", "Amarelo", "Vermelho", "Verde", "Azul", "Rosa", "Laranja", "Preto" };
static const uint32_t evCoresV[] = { 0xFFFFFFFF, 0xFFFFEB3B, 0xFFF44336, 0xFF4CAF50, 0xFF2196F3, 0xFFE91E63, 0xFFFF9800, 0xFF000000 };
static const char* const evEmojis[][2] = {
    { "1f600", "Sorriso" }, { "1f602", "Rindo" }, { "1f60d", "Apaixonado" }, { "1f60e", "Oculos" }, { "1f62e", "Surpreso" }, { "1f622", "Chorando" },
    { "1f621", "Bravo" }, { "1f44d", "Joinha" }, { "1f44f", "Palmas" }, { "1f64f", "Obrigado" }, { "2764", "Coracao" }, { "1f525", "Fogo" },
    { "2b50", "Estrela" }, { "1f389", "Festa" }, { "1f3ae", "Videogame" }, { "26bd", "Bola" }, { "1f3b5", "Musica" }, { "1f4f7", "Camera" },
    { "1f680", "Foguete" }, { "1f4af", "100" }, { "2705", "Certo" }, { "274c", "Errado" }, { "1f449", "Apontando" }, { "1f914", "Pensando" } };
static void evAbrirEscolha(int tipo) {
    evEscolhaTipo = tipo; evMenuTotal = 0; evMenuSel = 0; evMenuTopo = 0;
    if (tipo == EV_E_TRANS_DUR) { snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Duracao da transicao"); static const char* const n[] = { "0,3 s (rapida)", "0,6 s", "1 s", "1,5 s", "2 s (lenta)" }; for (int i = 0; i < 5; i++) evMenuAdicionar(n[i], i); }
    else if (tipo == EV_E_TRANSICAO) { snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Transicao com o pedaco anterior"); for (int i = 0; i < EV_TOTAL_TRANS; i++) evMenuAdicionar(evNomesTransicao[i], i); }
    else if (tipo == EV_E_FORMATO) { snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Formato do video"); for (int i = 0; i < 5; i++) evMenuAdicionar(evFormatos[i], i); }
    else if (tipo == EV_E_COR) { snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Cor do texto"); for (int i = 0; i < 8; i++) evMenuAdicionar(evCores[i], i); }
    else if (tipo == EV_E_EMOJI) { snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Emoji"); for (int i = 0; i < 24; i++) evMenuAdicionar(evEmojis[i][1], i); }
    evModo = EV_M_ESCOLHA;
}
static void evAdicionarElemento(int tipo, const char* arquivo, const char* texto) { evPerguntarOnde(tipo, arquivo, texto, 4); }

// ------------------------------------------------------------------ onde adicionar
// Nome da faixa como aparece na linha do tempo ("Video 2", "Audio 1").
static void evNomeFaixa(int fi, char* o, int n) {
    int numero = 1; for (int k = 0; k < fi; k++) if (evP.faixas[k].tipo == evP.faixas[fi].tipo) numero++;
    snprintf(o, n, "%s %d", evP.faixas[fi].tipo == EV_FAIXA_VISUAL ? "Video" : "Audio", numero);
}
static char evRotulos[EV_MAX_FAIXAS + 8][90];
// Pedaço de referência para "antes/depois": o selecionado, se for dessa faixa; senão o que está no cursor.
static int evClipeReferencia(int faixa) {
    if (faixa == evFaixaSel && evSelecionado()) return evClipeSel;
    const EvFaixa* f = &evP.faixas[faixa];
    for (int k = 0; k < f->total; k++) if (evCursor >= f->clipes[k].inicio && evCursor < evClipeFim(&f->clipes[k])) return k;
    return -1;
}
enum { EV_POS_CURSOR = 0, EV_POS_ANTES, EV_POS_DEPOIS, EV_POS_COMECO, EV_POS_FIM };
static void evPerguntarOnde(int tipoClipe, const char* arquivo, const char* texto, double dur) {
    memset(&evPend, 0, sizeof(evPend));
    evPend.ativo = true; evPend.tipoClipe = tipoClipe; evPend.dur = dur;
    snprintf(evPend.arquivo, sizeof(evPend.arquivo), "%s", arquivo ? arquivo : "");
    snprintf(evPend.texto, sizeof(evPend.texto), "%s", texto ? texto : "");
    int tipoFaixa = tipoClipe == EV_CLIPE_AUDIO ? EV_FAIXA_AUDIO : EV_FAIXA_VISUAL;
    evEscolhaTipo = EV_E_FAIXA; evMenuTotal = 0; evMenuSel = 0; evMenuTopo = 0;
    snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Em qual faixa?");
    // Mesma ordem da tela: visuais de cima para baixo.
    int ordem[EV_MAX_FAIXAS], n = 0;
    for (int i = evP.totalFaixas - 1; i >= 0; i--) if (evP.faixas[i].tipo == tipoFaixa) ordem[n++] = i;
    if (tipoFaixa == EV_FAIXA_AUDIO) { n = 0; for (int i = 0; i < evP.totalFaixas; i++) if (evP.faixas[i].tipo == tipoFaixa) ordem[n++] = i; }
    int r = 0;
    if (evP.totalFaixas < EV_MAX_FAIXAS) { snprintf(evRotulos[r], 90, "Nova faixa (por cima de tudo)"); if (tipoFaixa == EV_FAIXA_AUDIO) snprintf(evRotulos[r], 90, "Nova faixa de audio"); evOpcFaixas[r] = -1; evMenuAdicionar(evRotulos[r], r); r++; }
    for (int j = 0; j < n; j++) {
        int fi = ordem[j]; char nome[30]; evNomeFaixa(fi, nome, sizeof(nome));
        snprintf(evRotulos[r], 90, "%s  (%d pedacos)%s", nome, evP.faixas[fi].total, fi == evFaixaSel ? "  <- selecionada" : "");
        evOpcFaixas[r] = fi; evMenuAdicionar(evRotulos[r], r);
        if (fi == evFaixaSel) evMenuSel = r;
        r++;
    }
    // Texto e emoji costumam ir numa faixa por cima: sem faixa selecionada desse tipo, sugere a nova.
    evModo = EV_M_ESCOLHA;
}
static void evPerguntarPosicao(int faixa) {
    evPend.faixa = faixa;
    evEscolhaTipo = EV_E_POSICAO; evMenuTotal = 0; evMenuSel = 0; evMenuTopo = 0;
    char nome[30] = "faixa nova"; if (faixa >= 0) evNomeFaixa(faixa, nome, sizeof(nome));
    snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Onde na %s?", nome);
    int ref = faixa >= 0 ? evClipeReferencia(faixa) : -1;
    int r = EV_MAX_FAIXAS + 2;
    char tc[24]; evTempo(evCursor, tc, sizeof(tc));
    snprintf(evRotulos[r], 90, "No cursor (%s)", tc); evMenuAdicionar(evRotulos[r], EV_POS_CURSOR);
    if (ref >= 0) { evMenuAdicionar("Antes do pedaco selecionado", EV_POS_ANTES); evMenuAdicionar("Depois do pedaco selecionado", EV_POS_DEPOIS); }
    evMenuAdicionar("No comeco do video", EV_POS_COMECO);
    if (faixa >= 0) evMenuAdicionar("No fim da faixa", EV_POS_FIM);
    // Vídeo na faixa principal costuma ir no fim; o resto, no cursor.
    if (faixa >= 0 && evPend.tipoClipe == EV_CLIPE_VIDEO && evP.faixas[faixa].total) evMenuSel = evMenuTotal - 1;
    evModo = EV_M_ESCOLHA;
}
// Põe o conteúdo pendente na faixa e posição escolhidas. Os pedaços a partir do ponto se afastam
// para a direita (no meio de um pedaço, ele é cortado antes).
static void evInserirPendente(int pos) {
    int faixa = evPend.faixa;
    evPend.ativo = false;
    if (faixa < 0) {
        faixa = evFaixaNova(&evP, evPend.tipoClipe == EV_CLIPE_AUDIO ? EV_FAIXA_AUDIO : EV_FAIXA_VISUAL);
        if (faixa < 0) { evAvisar("Limite de faixas"); return; }
    }
    EvFaixa* f = &evP.faixas[faixa];
    if (f->total >= EV_MAX_CLIPES - 1) { evAvisar("Limite de pedacos nesta faixa"); return; }
    int ref = evPend.faixa >= 0 ? evClipeReferencia(faixa) : -1;
    double T = evCursor;
    if (pos == EV_POS_ANTES && ref >= 0) T = f->clipes[ref].inicio;
    else if (pos == EV_POS_DEPOIS && ref >= 0) T = evClipeFim(&f->clipes[ref]);
    else if (pos == EV_POS_COMECO) T = 0;
    else if (pos == EV_POS_FIM) { T = 0; for (int k = 0; k < f->total; k++) if (evClipeFim(&f->clipes[k]) > T) T = evClipeFim(&f->clipes[k]); }
    // No meio de um pedaço: corta nele para abrir espaço.
    for (int k = 0; k < f->total; k++) if (T > f->clipes[k].inicio + 0.05 && T < evClipeFim(&f->clipes[k]) - 0.05) { evClipeDividir(&evP, faixa, k, T); break; }
    // Afasta só se o novo pedaço encostar em algum que vem depois.
    bool bate = false;
    for (int k = 0; k < f->total; k++) if (f->clipes[k].inicio >= T - 0.001 && f->clipes[k].inicio < T + evPend.dur - 0.001) bate = true;
    if (bate) for (int k = 0; k < f->total; k++) if (f->clipes[k].inicio >= T - 0.001) f->clipes[k].inicio += evPend.dur;
    EvClipe* c = evClipeNovo(&evP, faixa, evPend.tipoClipe, evPend.arquivo, T, evPend.dur);
    if (!c) { evAvisar("Limite de pedacos"); return; }
    if (evPend.texto[0]) snprintf(c->texto, sizeof(c->texto), "%s", evPend.texto);
    if (c->tipo == EV_CLIPE_IMAGEM && !strncmp(c->arquivo, "emoji:", 6)) c->chaves[0].escala = 0.25f;
    if (c->tipo == EV_CLIPE_TEXTO) c->chaves[0].y = 0.8f;
    // Imagem numa faixa por cima de outra com conteúdo: entra como sobreposição (40%).
    bool temBaixo = false;
    for (int i = 0; i < faixa; i++) if (evP.faixas[i].tipo == EV_FAIXA_VISUAL && evP.faixas[i].total) temBaixo = true;
    if (c->tipo == EV_CLIPE_IMAGEM && strncmp(c->arquivo, "emoji:", 6) && temBaixo) c->chaves[0].escala = 0.4f;
    evFaixaSel = faixa; evClipeSel = (int)(c - f->clipes);
    evCursor = T;
    evMudar(); evModo = EV_M_LINHA;
    char t[200];
    snprintf(t, sizeof(t), "Adicionado (%.1f s)%s", evPend.dur, c->tipo == EV_CLIPE_AUDIO ? "" : "   X de novo: tamanho, posicao e giro");
    evAvisar(t);
}
static void evExecutar(int acao, int32_t usuario) {
    EvClipe* c = evSelecionado();
    evModo = EV_M_LINHA;
    switch (acao) {
    case EV_A_VIDEO: evAbrirArquivos(0); break;
    case EV_A_IMAGEM: evAbrirArquivos(1); break;
    case EV_A_AUDIO: evAbrirArquivos(2); break;
    case EV_A_ABRIR: evAbrirArquivos(3); break;
    case EV_A_TEXTO: if (edPlatTecladoAbrir(usuario, "Texto do video", 120, "")) evTecladoPara = 1; break;
    case EV_A_EMOJI: evAbrirEscolha(EV_E_EMOJI); break;
    case EV_A_GRAVAR: evGravarComecar(); break;
    case EV_A_FAIXA_VIDEO: if (evFaixaNova(&evP, EV_FAIXA_VISUAL) < 0) evAvisar("Limite de faixas"); else evMudar(); break;
    case EV_A_FAIXA_AUDIO: if (evFaixaNova(&evP, EV_FAIXA_AUDIO) < 0) evAvisar("Limite de faixas"); else evMudar(); break;
    case EV_A_OCULTAR_FAIXA: evP.faixas[evFaixaSel].oculta = !evP.faixas[evFaixaSel].oculta; evMudar(); break;
    case EV_A_MUDAR_FAIXA_MUDA: evP.faixas[evFaixaSel].muda = !evP.faixas[evFaixaSel].muda; evMudar(); break;
    case EV_A_FORMATO: evAbrirEscolha(EV_E_FORMATO); break;
    case EV_A_SALVAR: evSalvar(); break;
    case EV_A_NOVO: if (evMudou) evSalvar(); { char nome[40]; snprintf(nome, sizeof(nome), "Projeto_%llu", (unsigned long long)(edPlatAgoraMs() / 1000)); evProjetoNovo(&evP, nome, 1280, 720, 30); } evCursor = 0; evFaixaSel = 0; evClipeSel = -1; evPrevSujo = true; break;
    case EV_A_EXPORTAR: evExportarComecar(); break;
    case EV_A_SAIR: if (evMudou) evSalvar(); evTocar(false); evPvParar(); if (evRPrev) { evRenderLiberar(evRPrev); free(evRPrev); evRPrev = NULL; } evPrevW = 0; editorVideoAberto = false; break;
    case EV_A_DIVIDIR: if (c && evClipeDividir(&evP, evFaixaSel, evClipeSel, evCursor)) { evMudar(); evAvisar("Dividido"); } else evAvisar("Ponha o cursor no meio do clipe"); break;
    case EV_A_APAGAR: if (c) { evClipeApagar(&evP, evFaixaSel, evClipeSel); evClipeSel = -1; evMudar(); } break;
    case EV_A_DUPLICAR: if (c) { EvClipe copia = *c; EvFaixa* f = &evP.faixas[evFaixaSel]; copia.inicio = evClipeFim(c); if (f->total < EV_MAX_CLIPES) { f->clipes[f->total++] = copia; evFaixaOrdenar(f); evMudar(); } } break;
    case EV_A_MOVER_CURSOR: if (c) { c->inicio = evCursor; evFaixaOrdenar(&evP.faixas[evFaixaSel]); evClipeSel = -1; evMudar(); } break;
    case EV_A_APARAR_INICIO: if (c && evCursor > c->inicio && evCursor < evClipeFim(c) - 0.05) { double d = evCursor - c->inicio; c->fonteInicio += d; c->duracao -= d; c->inicio = evCursor; evMudar(); } else evAvisar("Ponha o cursor dentro do clipe"); break;
    case EV_A_APARAR_FIM: if (c && evCursor > c->inicio + 0.05 && evCursor < evClipeFim(c)) { c->duracao = evCursor - c->inicio; evMudar(); } else evAvisar("Ponha o cursor dentro do clipe"); break;
    case EV_A_SEPARAR_AUDIO: if (c && evClipeSepararAudio(&evP, evFaixaSel, evClipeSel)) { evMudar(); evAvisar("Audio separado numa faixa de audio"); } break;
    case EV_A_VOLUME_MAIS: if (c) { c->volume = fminf(2.0f, c->volume + 0.1f); evMudar(); } break;
    case EV_A_VOLUME_MENOS: if (c) { c->volume = fmaxf(0.0f, c->volume - 0.1f); evMudar(); } break;
    case EV_A_MUDO: if (c) { c->volume = c->volume > 0 ? 0 : 1; evMudar(); } break;
    case EV_A_TRANSICAO: if (c) evAbrirEscolha(EV_E_TRANSICAO); break;
    case EV_A_TRANS_DUR: if (c) evAbrirEscolha(EV_E_TRANS_DUR); break;
    case EV_A_TRANSFORMAR: if (c) { evModo = EV_M_TRANSFORMAR; evTocar(false); if (evCursor < c->inicio || evCursor >= evClipeFim(c)) evCursor = c->inicio + fmin(0.1, c->duracao / 2); evPrevSujo = true; } break;
    case EV_A_RESETAR: if (c) { c->totalChaves = 1; c->chaves[0].t = 0; c->chaves[0].x = c->chaves[0].y = 0.5f; c->chaves[0].escala = c->tipo == EV_CLIPE_IMAGEM && !strncmp(c->arquivo, "emoji:", 6) ? 0.25f : 1.0f; c->chaves[0].alfa = 1; c->chaves[0].rot = 0; evMudar(); } break;
    case EV_A_ANIMAR: if (c) { evModo = EV_M_ANIMAR; evTocar(false); evAvisar("Animar: analogico move, L1/R1 tamanho, L2/R2 transparencia, X marca ponto, Quadrado apaga ponto"); } break;
    case EV_A_EDITAR_TEXTO: if (c && edPlatTecladoAbrir(usuario, "Texto do video", 120, c->texto)) evTecladoPara = 2; break;
    case EV_A_COR_TEXTO: if (c) evAbrirEscolha(EV_E_COR); break;
    case EV_A_TAMANHO_MAIS: if (c) { c->tamanhoTexto = evLim(c->tamanhoTexto + 12, 24, 400); evMudar(); } break;
    case EV_A_TAMANHO_MENOS: if (c) { c->tamanhoTexto = evLim(c->tamanhoTexto - 12, 24, 400); evMudar(); } break;
    }
}
static void evEscolher(int i) {
    EvClipe* c = evSelecionado();
    evModo = EV_M_LINHA;
    if (evEscolhaTipo == EV_E_FAIXA) { if (evPend.ativo) evPerguntarPosicao(evOpcFaixas[i]); return; }
    if (evEscolhaTipo == EV_E_POSICAO) { if (evPend.ativo) evInserirPendente(i); return; }
    if (evEscolhaTipo == EV_E_TRANS_DUR && c) { c->transicaoDur = fmin(evTransDur[i], c->duracao * 0.9); evMudar(); return; }
    if (evEscolhaTipo == EV_E_TRANSICAO && c) {
        c->transicao = i; c->transicaoDur = i ? fmin(c->transicaoDur > 0 ? c->transicaoDur : 0.6, c->duracao * 0.9) : 0; evMudar();
        if (i) {
            int fi = evFaixaSel; const EvClipe* a = evClipeAnterior(&evP.faixas[fi], c);
            evAvisar(a ? "Transicao aplicada entre este pedaco e o anterior" : "Sem pedaco encostado antes: a transicao vira entrada sobre o fundo");
            evJuncao = evTemJuncao(&evP.faixas[fi], evClipeSel);
            evCursor = fmax(0, c->inicio - 0.5); evTocar(true);   // mostra a transição
        }
    }
    else if (evEscolhaTipo == EV_E_FORMATO) { evP.largura = evFormatosWH[i][0]; evP.altura = evFormatosWH[i][1]; evMudar(); }
    else if (evEscolhaTipo == EV_E_COR && c) { c->cor = evCoresV[i]; evMudar(); }
    else if (evEscolhaTipo == EV_E_EMOJI) { char a[40]; snprintf(a, sizeof(a), "emoji:%s", evEmojis[i][0]); evAdicionarElemento(EV_CLIPE_IMAGEM, a, NULL); }
}

// ------------------------------------------------------------------ abrir
bool editorVideoAbrir() {
    evCriarPastas();
    if (!evRPrev) evRPrev = (EvRender*)calloc(1, sizeof(EvRender));
    if (!evRPrev) return false;
    { int w, h; uint8_t* t = evPlatTexto("a", 20, 0xFFFFFFFF, &w, &h); free(t); }   // carrega a fonte antes das threads
    { extern void hnInstalarRegistradorCrashApp(); hnInstalarRegistradorCrashApp(); }
    evPvIniciar();
    evPlatLog("ABRIR editor (memoria %d MB)", evMemoriaMB());
    if (evP.largura <= 0) { char nome[40]; snprintf(nome, sizeof(nome), "Projeto_%llu", (unsigned long long)(edPlatAgoraMs() / 1000)); evProjetoNovo(&evP, nome, 1280, 720, 30); }
    evModo = EV_M_LINHA; evPrevSujo = true;
    editorVideoAberto = true;
    evAvisar("Triangulo: ferramentas   X: seleciona o clipe   Quadrado: play");
    return true;
}

// ------------------------------------------------------------------ desenho
#define EV_TL_Y 760
#define EV_TL_CAB 150
#define EV_FAIXA_H 44
static uint32_t evCorClipe(int tipo) { return tipo == EV_CLIPE_VIDEO ? 0xFF2F80ED : tipo == EV_CLIPE_IMAGEM ? 0xFF8E44AD : tipo == EV_CLIPE_TEXTO ? 0xFFE67E22 : 0xFF27AE60; }
static void evDesenharPrevia(uint32_t* p) {
    // Área da prévia: 1200x675 no centro do topo; o vídeo cabe nela mantendo o formato.
    int areaX = 360, areaY = 70, areaW = 1200, areaH = 675;
    double esc = fmin((double)areaW / evP.largura, (double)areaH / evP.altura);
    int dw = (int)(evP.largura * esc) & ~1, dh = (int)(evP.altura * esc) & ~1;
    // Parado: meia resolução. Tocando: um terço. Montada na thread da prévia.
    int fator = evTocando ? 3 : 2;
    int pw = dw / fator, ph = dh / fator;
    bool rapido = evArrastando && !evTocando;
    static bool ultimoRapido = false;
    bool mudouT = fabs(evPrevT - evCursor) > 0.0005;
    if (evModo != EV_M_EXPORTANDO && (evPrevSujo || mudouT || pw != evPrevW || ph != evPrevH || (ultimoRapido && !rapido))) {
        evPvPedir(evCursor, pw, ph, rapido, evPrevSujo, evSelecionado() ? evFaixaSel : -1, evClipeSel);
        evPrevT = evCursor; evPrevW = pw; evPrevH = ph; evPrevSujo = false; ultimoRapido = rapido;
    }
    int ox = areaX + (areaW - dw) / 2, oy = areaY + (areaH - dh) / 2;
    evRet(p, areaX, areaY, areaW, areaH, 0xFF0B0C0F);
    int caixaW = 0, caixaH = 0, prontoH = 0;
    if (evPvIniciada) {
        pthread_mutex_lock(&evPvTrava);
        int qw = evPvProntoW, qh = evPvProntoH;
        if (evPvPronto && qw > 0 && qh > 0) {
            for (int y = 0; y < dh; y++) {
                const uint8_t* l = evPvPronto + (size_t)(y * qh / dh) * qw * 4; uint32_t* d = p + (oy + y) * 1920 + ox;
                for (int x = 0; x < dw; x++) { const uint8_t* s = l + (x * qw / dw) * 4; d[x] = 0xFF000000u | ((uint32_t)s[0] << 16) | ((uint32_t)s[1] << 8) | s[2]; }
            }
        }
        else evRet(p, ox, oy, dw, dh, 0xFF000000);
        caixaW = evPvCaixaW; caixaH = evPvCaixaH; prontoH = qh;
        pthread_mutex_unlock(&evPvTrava);
    }
    // Pedaço visual selecionado: contorno girado com cantos (como no CapCut).
    EvClipe* c = evSelecionado();
    if (c && !evJuncao && c->tipo != EV_CLIPE_AUDIO && evCursor >= c->inicio && evCursor < evClipeFim(c) && (c->tipo == EV_CLIPE_VIDEO || caixaW > 0)) {
        EvChave k = evClipeAnimacao(c, evCursor - c->inicio);
        int sw = evP.largura, sh = evP.altura;
        if (c->tipo != EV_CLIPE_VIDEO) { sw = caixaW; sh = caixaH; }
        double base = c->tipo == EV_CLIPE_TEXTO ? (double)dh / fmax(1, prontoH) : fmin((double)dw / fmax(1, sw), (double)dh / fmax(1, sh));
        double bw = sw * base * k.escala, bh = sh * base * k.escala;
        double cx = ox + k.x * dw, cy = oy + k.y * dh, rad = k.rot * 3.14159265358979 / 180.0, cs = cos(rad), sn = sin(rad);
        double px[4], py[4]; const int sx[4] = { -1, 1, 1, -1 }, sy[4] = { -1, -1, 1, 1 };
        for (int i = 0; i < 4; i++) { double ux = sx[i] * bw / 2, uy = sy[i] * bh / 2; px[i] = cx + ux * cs - uy * sn; py[i] = cy + ux * sn + uy * cs; }
        uint32_t cor = evModo == EV_M_TRANSFORMAR || evModo == EV_M_ANIMAR ? 0xFFFFEB3B : 0xFFFFFFFF;
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4; int passos = (int)fmax(fabs(px[j] - px[i]), fabs(py[j] - py[i])) + 1;
            if (passos > 6000) passos = 6000;
            for (int q = 0; q <= passos; q++) {
                int x = (int)(px[i] + (px[j] - px[i]) * q / passos), y = (int)(py[i] + (py[j] - py[i]) * q / passos);
                if (x >= areaX && x < areaX + areaW - 1 && y >= areaY && y < areaY + areaH - 1) { p[y * 1920 + x] = cor; p[(y + 1) * 1920 + x] = cor; p[y * 1920 + x + 1] = cor; }
            }
            int qx = (int)px[i], qy = (int)py[i];
            if (qx >= areaX + 6 && qx < areaX + areaW - 6 && qy >= areaY + 6 && qy < areaY + areaH - 6) evRet(p, qx - 6, qy - 6, 12, 12, cor);
        }
        if (evModo == EV_M_TRANSFORMAR || evModo == EV_M_ANIMAR) {
            int ix = (int)cx, iy = (int)cy;
            if (ix > areaX + 20 && ix < areaX + areaW - 20 && iy > areaY + 20 && iy < areaY + areaH - 20) { evRet(p, ix - 20, iy - 1, 40, 3, cor); evRet(p, ix - 1, iy - 20, 3, 40, cor); }
        }
    }
}
static void evDesenharLinhaDoTempo(uint32_t* p) {
    evRet(p, 0, EV_TL_Y, 1920, 1080 - EV_TL_Y, 0xFF16181D);
    int x0 = EV_TL_CAB, largura = 1920 - EV_TL_CAB, centro = x0 + largura / 2;
    // régua
    evRet(p, x0, EV_TL_Y, largura, 28, 0xFF1E2128);
    double passo = evPxPorSeg >= 120 ? 1 : evPxPorSeg >= 40 ? 2 : evPxPorSeg >= 15 ? 5 : 15;
    double tIni = evCursor - (centro - x0) / evPxPorSeg, tFim = evCursor + (1920 - centro) / evPxPorSeg;
    for (double t = floor(tIni / passo) * passo; t <= tFim; t += passo) {
        if (t < 0) continue;
        int x = centro + (int)((t - evCursor) * evPxPorSeg);
        evRet(p, x, EV_TL_Y + 18, 1, 10, 0xFF6B7280);
        char s[16]; int ts = (int)t; snprintf(s, sizeof(s), "%d:%02d", ts / 60, ts % 60);
        evTexto(p, s, 16, x + 4, EV_TL_Y + 2, 0xFF9AA3AF);
    }
    // faixas: visuais de cima para baixo (a de cima aparece por cima no vídeo), áudio embaixo
    int ordem[EV_MAX_FAIXAS], n = 0;
    for (int i = evP.totalFaixas - 1; i >= 0; i--) if (evP.faixas[i].tipo == EV_FAIXA_VISUAL) ordem[n++] = i;
    for (int i = 0; i < evP.totalFaixas; i++) if (evP.faixas[i].tipo == EV_FAIXA_AUDIO) ordem[n++] = i;
    int visiveis = (1080 - EV_TL_Y - 34) / EV_FAIXA_H;
    int posSel = 0; for (int i = 0; i < n; i++) if (ordem[i] == evFaixaSel) posSel = i;
    int topo = posSel >= visiveis ? posSel - visiveis + 1 : 0;
    for (int j = 0; j < visiveis && topo + j < n; j++) {
        int fi = ordem[topo + j]; const EvFaixa* f = &evP.faixas[fi];
        int y = EV_TL_Y + 32 + j * EV_FAIXA_H;
        bool sel = fi == evFaixaSel;
        evRet(p, 0, y, EV_TL_CAB - 4, EV_FAIXA_H - 4, sel ? 0xFF2A3B55 : 0xFF1E2128);
        char nome[40]; int numero = 1; for (int k = 0; k < fi; k++) if (evP.faixas[k].tipo == f->tipo) numero++;
        snprintf(nome, sizeof(nome), "%s %d%s%s", f->tipo == EV_FAIXA_VISUAL ? "Video" : "Audio", numero, f->oculta ? " (oculta)" : "", f->muda ? " (muda)" : "");
        evTexto(p, nome, 17, 8, y + 11, sel ? 0xFFFFFFFF : 0xFFAAB2BD);
        evRet(p, x0, y, largura, EV_FAIXA_H - 4, sel ? 0xFF20242C : 0xFF1A1D23);
        for (int k = 0; k < f->total; k++) {
            const EvClipe* c = &f->clipes[k];
            int cx = centro + (int)((c->inicio - evCursor) * evPxPorSeg), cw = (int)(c->duracao * evPxPorSeg);
            if (cx + cw < x0 || cx > 1920) continue;
            int a = cx < x0 ? x0 : cx, b = cx + cw > 1920 ? 1920 : cx + cw;
            uint32_t cor = evCorClipe(c->tipo);
            if (c->tipo == EV_CLIPE_VIDEO && c->semAudio) cor = 0xFF1C5DB8;
            evRet(p, a + 1, y + 2, b - a - 2, EV_FAIXA_H - 8, cor);
            if (sel && k == evClipeSel && !evJuncao) evBorda(p, a, y, b - a, EV_FAIXA_H - 4, 0xFFFFEB3B, 3);
            char rot[120];
            const char* base = c->tipo == EV_CLIPE_TEXTO ? c->texto : strrchr(c->arquivo, '/') ? strrchr(c->arquivo, '/') + 1 : c->arquivo;
            snprintf(rot, sizeof(rot), "%s", base);
            if (b - a > 40) { char corte[60]; snprintf(corte, sizeof(corte), "%.*s", evLim((b - a - 16) / 9, 1, 58), rot); evTexto(p, corte, 16, a + 8, y + 12, 0xFFFFFFFF); }
            // Junção com o pedaço anterior: botão de transição (preenchido se houver uma).
            if (c->tipo != EV_CLIPE_AUDIO && cx >= x0 + 10 && cx < 1910) {
                bool junta = k > 0 && fabs(evClipeFim(&f->clipes[k - 1]) - c->inicio) < 0.05;
                if (junta || c->transicao) {
                    uint32_t cj = c->transicao ? 0xFFFFEB3B : 0xFFFFFFFF;
                    bool juncaoSel = sel && k == evClipeSel && evJuncao;
                    if (juncaoSel) { evRet(p, cx - 16, y - 2, 32, EV_FAIXA_H, 0xFFFFEB3B); evRet(p, cx - 12, y + 2, 24, EV_FAIXA_H - 8, 0xFF101216); }
                    else evRet(p, cx - 9, y + 8, 18, EV_FAIXA_H - 20, 0xFF101216);
                    if (c->transicao) evRet(p, cx - 6, y + 11, 12, EV_FAIXA_H - 26, cj); else evBorda(p, cx - 6, y + 11, 12, EV_FAIXA_H - 26, cj, 2);
                    // A duração da transição aparece como uma faixa sobre o começo do pedaço.
                    if (c->transicao && c->transicaoDur > 0) { int tw = (int)(c->transicaoDur * evPxPorSeg); if (tw > 2 && cx + 8 < 1920) evRet(p, cx + 8, y + EV_FAIXA_H - 10, evLim(tw - 8, 0, 1920 - cx - 8), 4, 0xFFFFEB3B); }
                }
            }
            // pontos de animação
            if (c->totalChaves > 1) for (int q = 0; q < c->totalChaves; q++) {
                int kx = cx + (int)(c->chaves[q].t * evPxPorSeg);
                if (kx >= x0 && kx < 1920) evRet(p, kx - 4, y + EV_FAIXA_H - 14, 8, 8, 0xFFFFEB3B);
            }
        }
    }
    // cursor
    evRet(p, centro - 1, EV_TL_Y, 3, 1080 - EV_TL_Y, 0xFFFFFFFF);
}
static void evDesenharLista(uint32_t* p, const char* titulo, const char* const* itens, int total, int sel, const char* dica) {
    int w = 760, linhas = total < 14 ? total : 14, h = 90 + linhas * 40, x = (1920 - w) / 2, y = (1080 - h) / 2;
    if (sel < evMenuTopo) evMenuTopo = sel; if (sel >= evMenuTopo + linhas) evMenuTopo = sel - linhas + 1;
    evRet(p, x, y, w, h, 0xFF1B1E24); evBorda(p, x, y, w, h, 0xFF29B6F6, 3);
    evTexto(p, titulo, 26, x + 24, y + 16, 0xFFFFFFFF);
    for (int i = 0; i < linhas && evMenuTopo + i < total; i++) {
        int yy = y + 58 + i * 40, k = evMenuTopo + i;
        if (k == sel) evRet(p, x + 10, yy - 4, w - 20, 38, 0xFF29B6F6);
        evTexto(p, itens[k], 21, x + 28, yy + 4, k == sel ? 0xFF0B1A26 : 0xFFD0D4DA);
    }
    evTexto(p, dica, 17, x + 24, y + h - 26, 0xFF9AA3AF);
}
void editorVideoRenderizar(uint32_t* p) {
    // Avança o cursor tocando.
    if (evTocando) {
        evCursor = evTocarT + (edPlatAgoraMs() - evTocarMs) / 1000.0;
        double fim = evProjetoDuracao(&evP);
        if (evCursor >= fim) { evCursor = fim; if (evModo != EV_M_GRAVANDO) evTocar(false); }
    }
    evRet(p, 0, 0, 1920, EV_TL_Y, 0xFF101216);
    char t[300], a[24], b[24];
    evTempo(evCursor, a, sizeof(a)); evTempo(evProjetoDuracao(&evP), b, sizeof(b));
    snprintf(t, sizeof(t), "Editor de video  -  %.60s%s   %dx%d %dfps   %s / %s", evP.nome, evMudou ? " *" : "", evP.largura, evP.altura, evP.fps, a, b);
    evTexto(p, t, 24, 20, 22, 0xFFFFFFFF);
    evDesenharPrevia(p);
    // Painel esquerdo: clipe selecionado
    EvClipe* c = evSelecionado();
    evRet(p, 0, 70, 350, 675, 0xFF16181D);
    if (c && evJuncao) {
        char l[200]; int y = 84;
        evTexto(p, "Juncao (ponto de transicao)", 22, 16, y, 0xFFFFEB3B); y += 40;
        snprintf(l, sizeof(l), "Entre o pedaco %d e o %d", evClipeSel, evClipeSel + 1); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
        snprintf(l, sizeof(l), "Transicao: %s", evNomesTransicao[c->transicao % EV_TOTAL_TRANS]); evTexto(p, l, 19, 16, y, 0xFFFFFFFF); y += 30;
        if (c->transicao) { snprintf(l, sizeof(l), "Duracao: %.1f s", c->transicaoDur); evTexto(p, l, 19, 16, y, 0xFFFFFFFF); y += 40; }
        else y += 10;
        evTexto(p, "X: escolher a transicao", 18, 16, y, 0xFFAAB2BD); y += 28;
        evTexto(p, "L1/R1: mais curta / mais longa", 18, 16, y, 0xFFAAB2BD); y += 28;
        evTexto(p, "Quadrado: ver a transicao", 18, 16, y, 0xFFAAB2BD); y += 28;
        evTexto(p, "Analogico: proximo trecho", 18, 16, y, 0xFFAAB2BD);
    }
    else if (c) {
        static const char* const tipos[] = { "Video", "Imagem", "Audio", "Texto" };
        char l[200]; int y = 84;
        snprintf(l, sizeof(l), "%s", tipos[c->tipo]); evTexto(p, l, 24, 16, y, 0xFFFFEB3B); y += 40;
        evTempo(c->inicio, a, sizeof(a)); snprintf(l, sizeof(l), "Inicio: %s", a); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
        evTempo(c->duracao, a, sizeof(a)); snprintf(l, sizeof(l), "Duracao: %s", a); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
        if (c->tipo == EV_CLIPE_VIDEO || c->tipo == EV_CLIPE_AUDIO) { snprintf(l, sizeof(l), "Volume: %d%%%s", (int)(c->volume * 100 + 0.5f), c->semAudio ? " (audio separado)" : ""); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30; }
        if (c->tipo != EV_CLIPE_AUDIO) {
            if (c->transicao) snprintf(l, sizeof(l), "Transicao: %s (%.1f s)", evNomesTransicao[c->transicao % EV_TOTAL_TRANS], c->transicaoDur);
            else snprintf(l, sizeof(l), "Transicao: Nenhuma");
            evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
            snprintf(l, sizeof(l), "Pontos de animacao: %d", c->totalChaves); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
            EvChave k = evClipeAnimacao(c, evCursor - c->inicio);
            snprintf(l, sizeof(l), "Posicao %.0f%% x %.0f%%", k.x * 100, k.y * 100); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
            snprintf(l, sizeof(l), "Tamanho %.0f%%  Opacidade %.0f%%", k.escala * 100, k.alfa * 100); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
            snprintf(l, sizeof(l), "Giro %.0f graus", k.rot); evTexto(p, l, 19, 16, y, 0xFFD0D4DA); y += 30;
        }
        if (c->tipo == EV_CLIPE_TEXTO) { snprintf(l, sizeof(l), "\"%.40s\"", c->texto); evTexto(p, l, 19, 16, y, 0xFFFFFFFF); }
    }
    else evTexto(p, "Nenhum clipe selecionado", 19, 16, 84, 0xFF8A93A0);
    // Painel direito: botões
    evRet(p, 1570, 70, 350, 675, 0xFF16181D);
    static const char* const dicasLinha[] = { "Analogico esq.: trechos e juncoes", "  (cima/baixo: troca de faixa)", "Direcional: move o cursor", "L2/R2: pedaco anterior/proximo", "X: seleciona o pedaco", "X de novo: tamanho e posicao", "Quadrado: play / pausa", "Triangulo: ferramentas", "L1/R1: zoom (na juncao: duracao)", "Options: exportar", "O: tira a selecao / sai" };
    static const char* const dicasAnimar[] = { "Analogico esq.: move", "Analogico dir.: gira", "L1/R1: tamanho", "L2/R2: opacidade", "Esq/Dir: move o cursor", "X: marca um ponto", "Quadrado: apaga o ponto", "O: termina de animar" };
    static const char* const dicasTransf[] = { "Analogico esq.: move", "Direcional: move fino", "L1/R1: tamanho", "Analogico dir.: gira", "R3: giro volta a 0", "L2/R2: opacidade", "Quadrado: centraliza", "X ou O: pronto" };
    const char* const* d = evModo == EV_M_ANIMAR ? dicasAnimar : evModo == EV_M_TRANSFORMAR ? dicasTransf : dicasLinha;
    int nd = evModo == EV_M_ANIMAR ? 8 : evModo == EV_M_TRANSFORMAR ? 8 : 11;
    bool destaque = evModo == EV_M_ANIMAR || evModo == EV_M_TRANSFORMAR;
    evTexto(p, evModo == EV_M_ANIMAR ? "ANIMAR" : evModo == EV_M_TRANSFORMAR ? "TAMANHO E POSICAO" : "Controles", 22, 1586, 84, destaque ? 0xFFFFEB3B : 0xFFFFFFFF);
    for (int i = 0; i < nd; i++) evTexto(p, d[i], 18, 1586, 124 + i * 30, 0xFFAAB2BD);
    evDesenharLinhaDoTempo(p);
    // Sobreposições
    if (evModo == EV_M_MENU) evDesenharLista(p, evMenuTitulo, evMenuItens, evMenuTotal, evMenuSel, "X escolhe   O fecha");
    else if (evModo == EV_M_ESCOLHA) evDesenharLista(p, evMenuTitulo, evMenuItens, evMenuTotal, evMenuSel, "X escolhe   O fecha");
    else if (evModo == EV_M_ARQUIVOS) {
        static const char* nomes[400]; static char rotulos[400][210];
        int n = evTotalArqs < 400 ? evTotalArqs : 400;
        for (int i = 0; i < n; i++) {
            const char* rot = evArqs[i].nome;
            if (!evArqPasta[0]) for (int r = 0; r < 4; r++) if (!strcmp(rot, evRaizes[r][0])) rot = evRaizes[r][1];
            snprintf(rotulos[i], sizeof(rotulos[i]), "%s%s", evArqs[i].pasta && evArqPasta[0] ? "[pasta] " : "", rot); nomes[i] = rotulos[i];
        }
        char tit[200]; static const char* const tipos[] = { "video", "imagem", "audio", "projeto" };
        snprintf(tit, sizeof(tit), "Escolha o %s: %.120s", tipos[evArqTipo], evArqPasta[0] ? evArqPasta : "onde procurar?");
        if (n == 0) { static const char* vazio[] = { "(pasta vazia: O volta)" }; evDesenharLista(p, tit, vazio, 1, 0, "O volta uma pasta"); }
        else evDesenharLista(p, tit, nomes, n, evArqSel, "X abre / escolhe   O volta uma pasta   Triangulo fecha");
    }
    else if (evModo == EV_M_EXPORTANDO) {
        int w = 760, h = 160, x = (1920 - w) / 2, y = 300;
        evRet(p, x, y, w, h, 0xFF1B1E24); evBorda(p, x, y, w, h, 0xFF29B6F6, 3);
        snprintf(t, sizeof(t), "Exportando... %d.%d%%", evExp.progresso / 10, evExp.progresso % 10); evTexto(p, t, 28, x + 30, y + 24, 0xFFFFFFFF);
        double qps = evExp.quadrosPorSeg;
        if (qps > 0.1) {
            double faltam = (1000 - evExp.progresso) / 1000.0 * evProjetoDuracao(&evP) * evP.fps / qps;
            int fm = (int)(faltam / 60), fs = (int)faltam % 60;
            snprintf(t, sizeof(t), "%.1f quadros/s (%.2fx)   faltam ~%d:%02d", qps, qps / evP.fps, fm, fs);
            evTexto(p, t, 20, x + 30, y + 118, 0xFFAAB2BD);
        }
        evRet(p, x + 30, y + 80, w - 60, 20, 0xFF2A2E36); evRet(p, x + 30, y + 80, (w - 60) * evExp.progresso / 1000, 20, 0xFF29B6F6);
        evTexto(p, evExp.cancelar ? "Cancelando..." : "O cancela", 18, x + w - 170, y + 120, evExp.cancelar ? 0xFFFFEB3B : 0xFF9AA3AF);
    }
    else if (evModo == EV_M_GRAVANDO) {
        evRet(p, 760, 90, 400, 60, 0xFFB71C1C);
        snprintf(t, sizeof(t), "GRAVANDO  %.1f s   (X para)", (double)evGravSegundos); evTexto(p, t, 24, 780, 106, 0xFFFFFFFF);
    }
    if (evAvisoAte > edPlatAgoraMs()) {
        int w = edPlatLargura(evAviso, 22) + 40, x = (1920 - w) / 2;
        evRet(p, x, 680, w, 44, 0xFF101215); evBorda(p, x, 680, w, 44, 0xFF29B6F6, 2);
        evTexto(p, evAviso, 22, x + 20, 690, 0xFFFFFFFF);
    }
}

// ------------------------------------------------------------------ controles
static int evSegurado = 0;
static void evIrParaCorte(int dir) {
    double melhor = dir > 0 ? 1e18 : -1;
    for (int i = 0; i < evP.totalFaixas; i++) for (int k = 0; k < evP.faixas[i].total; k++) {
        const EvClipe* c = &evP.faixas[i].clipes[k];
        double pontos[2] = { c->inicio, evClipeFim(c) };
        for (int j = 0; j < 2; j++) {
            if (dir > 0 && pontos[j] > evCursor + 0.01 && pontos[j] < melhor) melhor = pontos[j];
            if (dir < 0 && pontos[j] < evCursor - 0.01 && pontos[j] > melhor) melhor = pontos[j];
        }
    }
    if (dir > 0 && melhor < 1e17) evCursor = melhor;
    if (dir < 0 && melhor >= 0) evCursor = melhor;
}
static int evClipeNoCursor() {
    if (evFaixaSel < 0 || evFaixaSel >= evP.totalFaixas) return -1;
    const EvFaixa* f = &evP.faixas[evFaixaSel];
    for (int k = 0; k < f->total; k++) if (evCursor >= f->clipes[k].inicio && evCursor < evClipeFim(&f->clipes[k])) return k;
    return -1;
}
// X: seleciona o pedaço no cursor; no pedaço já selecionado, abre tamanho/posição/giro (áudio: o menu).
static void evSelecionarNoCursor() {
    evJuncao = false;
    int k = evClipeNoCursor();
    if (k < 0) { evClipeSel = -1; evAvisar("Nenhum pedaco no cursor nesta faixa (Cima/Baixo troca de faixa)"); return; }
    if (k == evClipeSel) {
        if (evP.faixas[evFaixaSel].clipes[k].tipo == EV_CLIPE_AUDIO) evAbrirMenu();
        else { evModo = EV_M_TRANSFORMAR; evTocar(false); evPrevSujo = true; }
        return;
    }
    evClipeSel = k;
}
// Junção antes do pedaço k: encostado no anterior (ou já com transição).
static bool evTemJuncao(const EvFaixa* f, int k) {
    if (k <= 0 || k >= f->total || f->clipes[k].tipo == EV_CLIPE_AUDIO) return false;
    return fabs(evClipeFim(&f->clipes[k - 1]) - f->clipes[k].inicio) < 0.05 || f->clipes[k].transicao;
}
static void evMostrarItem() {
    const EvFaixa* f = &evP.faixas[evFaixaSel]; const EvClipe* c = &f->clipes[evClipeSel];
    char t[160];
    if (evJuncao) {
        evCursor = c->inicio + (c->transicao ? c->transicaoDur / 2 : 0);
        if (c->transicao) snprintf(t, sizeof(t), "Juncao %d/%d: %s %.1f s   X troca   L1/R1 duracao", evClipeSel, evClipeSel + 1, evNomesTransicao[c->transicao % EV_TOTAL_TRANS], c->transicaoDur);
        else snprintf(t, sizeof(t), "Juncao %d/%d sem transicao   X escolhe uma", evClipeSel, evClipeSel + 1);
    }
    else { evCursor = c->inicio; char a[24]; evTempo(c->duracao, a, sizeof(a)); snprintf(t, sizeof(t), "Pedaco %d de %d  (%s)   X de novo: tamanho e posicao", evClipeSel + 1, f->total, a); }
    evAvisar(t); evAvisoAte = edPlatAgoraMs() + 2000;
}
// Analógico: anda pelos itens da faixa na ordem pedaço, junção, pedaço, junção...
// (item 2k = junção antes do pedaço k, 2k+1 = pedaço k).
static void evPassoItem(int dir) {
    const EvFaixa* f = &evP.faixas[evFaixaSel];
    if (f->total == 0) { evAvisar("Faixa vazia (cima/baixo troca de faixa)"); return; }
    int item;
    if (evSelecionado()) item = evJuncao ? 2 * evClipeSel : 2 * evClipeSel + 1;
    else {
        // Nada selecionado: começa no pedaço do cursor (ou o mais perto na direção).
        int k = evClipeNoCursor();
        if (k < 0) {
            if (dir > 0) { k = f->total - 1; for (int i = 0; i < f->total; i++) if (f->clipes[i].inicio > evCursor) { k = i; break; } }
            else { k = 0; for (int i = f->total - 1; i >= 0; i--) if (f->clipes[i].inicio < evCursor) { k = i; break; } }
        }
        evClipeSel = k; evJuncao = false; evMostrarItem(); return;
    }
    int novoItem = item;
    for (int i = item + dir; i >= 1 && i <= 2 * f->total - 1; i += dir) {
        if (i % 2 == 1 || evTemJuncao(f, i / 2)) { novoItem = i; break; }
    }
    if (novoItem == item) return;
    evClipeSel = novoItem / 2; evJuncao = novoItem % 2 == 0;
    evMostrarItem();
}
static void evDuracaoTransicao(double d) {
    EvClipe* c = evSelecionado(); if (!c) return;
    if (!c->transicao) { evAvisar("Sem transicao aqui: X escolhe uma"); return; }
    c->transicaoDur = fmax(0.1, fmin(c->transicaoDur + d, fmin(5.0, c->duracao * 0.9)));
    evMudar(); evMostrarItem();
}
// L2/R2: pula para o pedaço anterior/seguinte da faixa e já o seleciona (cursor no começo dele).
static void evIrParaPedaco(int dir) {
    const EvFaixa* f = &evP.faixas[evFaixaSel];
    if (f->total == 0) { evIrParaCorte(dir); return; }
    int alvo = -1;
    if (evSelecionado()) alvo = evLim(evClipeSel + dir, 0, f->total - 1);
    else if (dir > 0) { for (int k = 0; k < f->total && alvo < 0; k++) if (f->clipes[k].inicio > evCursor + 0.001) alvo = k; if (alvo < 0) alvo = f->total - 1; }
    else { for (int k = f->total - 1; k >= 0 && alvo < 0; k--) if (f->clipes[k].inicio < evCursor - 0.001) alvo = k; if (alvo < 0) alvo = 0; }
    evClipeSel = alvo; evJuncao = false;
    evCursor = f->clipes[alvo].inicio;
    // Mantém o pedaço visível na linha do tempo, mesmo com zoom alto.
    char t[160], a[24]; evTempo(f->clipes[alvo].duracao, a, sizeof(a));
    snprintf(t, sizeof(t), "Pedaco %d de %d  (%s)", alvo + 1, f->total, a); evAvisar(t); evAvisoAte = edPlatAgoraMs() + 1200;
}
static void evTrocarFaixa(int dir) {
    // Mesma ordem da tela: visuais (de cima) e depois áudio.
    int ordem[EV_MAX_FAIXAS], n = 0;
    for (int i = evP.totalFaixas - 1; i >= 0; i--) if (evP.faixas[i].tipo == EV_FAIXA_VISUAL) ordem[n++] = i;
    for (int i = 0; i < evP.totalFaixas; i++) if (evP.faixas[i].tipo == EV_FAIXA_AUDIO) ordem[n++] = i;
    int pos = 0; for (int i = 0; i < n; i++) if (ordem[i] == evFaixaSel) pos = i;
    pos = evLim(pos + dir, 0, n - 1);
    evFaixaSel = ordem[pos]; evClipeSel = evClipeNoCursor(); evJuncao = false;   // já seleciona o pedaço no cursor
}
bool editorVideoControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    uint32_t novo = botoes & ~antigos;
    // Teclado do sistema aberto.
    if (evTecladoPara) {
        char t[300]; int r = edPlatTecladoResultado(t, sizeof(t));
        if (r < 0) return true;
        int para = evTecladoPara; evTecladoPara = 0;
        if (r == 1 && t[0]) {
            if (para == 1) evAdicionarElemento(EV_CLIPE_TEXTO, "", t);
            else if (para == 2 && evSelecionado()) { snprintf(evSelecionado()->texto, sizeof(evSelecionado()->texto), "%s", t); evMudar(); }
        }
        return true;
    }
    // Exportação em andamento.
    if (evModo == EV_M_EXPORTANDO) {
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) evExp.cancelar = true;
        // A tela nunca espera: só junta a thread depois que ela terminou de verdade.
        if (evExp.terminou) {
            pthread_join(evExpThread, NULL);
            evModo = EV_M_LINHA;
            char t[200];
            if (evExp.erro[0]) snprintf(t, sizeof(t), "Exportacao: %s", evExp.erro);
            else { const char* b = strrchr(evExpSaida, '/'); snprintf(t, sizeof(t), "Pronto: exportados/%.140s", b ? b + 1 : evExpSaida); }
            evAvisar(t); evAvisoAte = edPlatAgoraMs() + 8000;
            evPlatLog("EXPORTAR fim: %s (memoria %d MB)", evExp.erro[0] ? evExp.erro : "ok", evMemoriaMB());
            evPrevSujo = true;
        }
        return true;
    }
    if (evModo == EV_M_GRAVANDO) { if (novo & (ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_CIRCLE)) evGravarTerminar(); return true; }
    // Listas (menu, escolha, arquivos)
    if (evModo == EV_M_MENU || evModo == EV_M_ESCOLHA || evModo == EV_M_ARQUIVOS) {
        int total = evModo == EV_M_ARQUIVOS ? (evTotalArqs < 400 ? evTotalArqs : 400) : evMenuTotal;
        int* sel = evModo == EV_M_ARQUIVOS ? &evArqSel : &evMenuSel;
        bool cima = botoes & ORBIS_PAD_BUTTON_UP, baixo = botoes & ORBIS_PAD_BUTTON_DOWN;
        if (cima || baixo) { evSegurado++; if ((novo & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN)) || (evSegurado > 18 && evSegurado % 3 == 0)) *sel = total ? (*sel + (baixo ? 1 : -1) + total) % total : 0; }
        else evSegurado = 0;
        if (evModo == EV_M_ARQUIVOS) {
            if (novo & ORBIS_PAD_BUTTON_TRIANGLE) { evModo = EV_M_LINHA; return true; }
            if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
                // Volta uma pasta; numa das raízes (Hyper Neiva, Raiz, USB), volta à lista inicial.
                bool raiz = !evArqPasta[0];
                for (int i = 0; i < 4; i++) if (!strcmp(evArqPasta, evRaizes[i][0])) raiz = true;
                if (!evArqPasta[0]) evModo = EV_M_LINHA;
                else if (raiz) evListar("");
                else {
                    char pasta[512]; snprintf(pasta, sizeof(pasta), "%s", evArqPasta);
                    char* b = strrchr(pasta, '/');
                    if (b == pasta) pasta[1] = 0; else if (b) *b = 0;
                    evListar(pasta);
                }
                return true;
            }
            if ((novo & ORBIS_PAD_BUTTON_CROSS) && total > 0) {
                size_t lp = strlen(evArqPasta);
                char c[800]; snprintf(c, sizeof(c), "%s%s%s", evArqPasta, lp && evArqPasta[lp - 1] != '/' ? "/" : "", evArqs[evArqSel].nome);
                if (evArqs[evArqSel].pasta) evListar(c); else evInserirArquivo(c);
            }
            return true;
        }
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) { evModo = EV_M_LINHA; return true; }
        if ((novo & ORBIS_PAD_BUTTON_CROSS) && total > 0) { if (evModo == EV_M_MENU) evExecutar(evMenuAcoes[evMenuSel], usuario); else evEscolher(evMenuAcoes[evMenuSel]); }
        return true;
    }
    // Cursor: direcional ou analógico; acelera segurando.
    int lx = 128, ly = 128, rx = 128, ry = 128; edPlatAnalogico(&lx, &ly, &rx, &ry);
    int dir = 0;
    if (evModo == EV_M_TRANSFORMAR) dir = 0;
    else if (botoes & ORBIS_PAD_BUTTON_LEFT) dir = -1;
    else if (botoes & ORBIS_PAD_BUTTON_RIGHT) dir = 1;
    evArrastando = dir != 0;
    static uint64_t ultimoMs = 0; uint64_t agora = edPlatAgoraMs(); double dt = ultimoMs ? (agora - ultimoMs) / 1000.0 : 0; ultimoMs = agora; if (dt > 0.2) dt = 0.2;
    static double seguradoS = 0;
    if (dir) {
        if (evTocando) evTocar(false);
        evJuncao = false;
        if (novo & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) { evCursor += dir / (double)evP.fps; seguradoS = 0; }
        else { seguradoS += dt; double v = (seguradoS < 0.25 ? 0 : 0.5 + seguradoS * seguradoS * 2.0) * (120.0 / evPxPorSeg); if (v > 60) v = 60; evCursor += dir * v * dt; }
        if (evCursor < 0) evCursor = 0;
        double fim = evProjetoDuracao(&evP) + 10; if (evCursor > fim) evCursor = fim;
    }
    else seguradoS = 0;
    if (evModo == EV_M_TRANSFORMAR) {
        EvClipe* c = evSelecionado();
        if (!c || (novo & (ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_TRIANGLE))) { evModo = EV_M_LINHA; return true; }
        float ax = (lx - 128) / 128.0f, ay = (ly - 128) / 128.0f, gx = (rx - 128) / 128.0f;
        float dx = 0, dy = 0, fe = 1, dr = 0, da = 0; float fdt = (float)dt;
        if (fabsf(ax) > 0.15f) dx += ax * fabsf(ax) * 0.6f * fdt;
        if (fabsf(ay) > 0.15f) dy += ay * fabsf(ay) * 0.6f * fdt;
        if (botoes & ORBIS_PAD_BUTTON_LEFT) dx -= 0.08f * fdt; if (botoes & ORBIS_PAD_BUTTON_RIGHT) dx += 0.08f * fdt;
        if (botoes & ORBIS_PAD_BUTTON_UP) dy -= 0.08f * fdt; if (botoes & ORBIS_PAD_BUTTON_DOWN) dy += 0.08f * fdt;
        if (botoes & ORBIS_PAD_BUTTON_R1) fe *= 1.0f + 0.9f * fdt;
        if (botoes & ORBIS_PAD_BUTTON_L1) fe /= 1.0f + 0.9f * fdt;
        if (fabsf(gx) > 0.2f) dr += gx * fabsf(gx) * 120.0f * fdt;
        if (botoes & ORBIS_PAD_BUTTON_R2) da += 0.6f * fdt;
        if (botoes & ORBIS_PAD_BUTTON_L2) da -= 0.6f * fdt;
        if (dx || dy || fe != 1 || dr || da) { evClipeTransformar(c, dx, dy, fe, dr, da); evMudar(); }
        if (novo & ORBIS_PAD_BUTTON_R3) { float r0 = evClipeAnimacao(c, evCursor - c->inicio).rot; evClipeTransformar(c, 0, 0, 1, -r0, 0); evMudar(); }
        if (novo & ORBIS_PAD_BUTTON_SQUARE) { EvChave k = evClipeAnimacao(c, evCursor - c->inicio); evClipeTransformar(c, 0.5f - k.x, 0.5f - k.y, 1, 0, 0); evMudar(); }
        return true;
    }
    if (evModo == EV_M_ANIMAR) {
        EvClipe* c = evSelecionado();
        if (!c || (novo & ORBIS_PAD_BUTTON_CIRCLE)) { evModo = EV_M_LINHA; return true; }
        double tl = evCursor - c->inicio;
        if (tl < 0 || tl > c->duracao) { if (novo & ORBIS_PAD_BUTTON_CROSS) evAvisar("Ponha o cursor dentro do clipe para marcar pontos"); return true; }
        EvChave k = evClipeAnimacao(c, tl);
        bool mexeu = false;
        float ax = (lx - 128) / 128.0f, ay = (ly - 128) / 128.0f;
        if (fabsf(ax) > 0.2f || fabsf(ay) > 0.2f) { k.x += ax * 0.5f * (float)dt; k.y += ay * 0.5f * (float)dt; mexeu = true; }
        if (botoes & ORBIS_PAD_BUTTON_R1) { k.escala *= 1.0f + 0.8f * (float)dt; mexeu = true; }
        if (botoes & ORBIS_PAD_BUTTON_L1) { k.escala /= 1.0f + 0.8f * (float)dt; mexeu = true; }
        if (botoes & ORBIS_PAD_BUTTON_R2) { k.alfa += 0.6f * (float)dt; mexeu = true; }
        if (botoes & ORBIS_PAD_BUTTON_L2) { k.alfa -= 0.6f * (float)dt; mexeu = true; }
        float gx = (rx - 128) / 128.0f;
        if (fabsf(gx) > 0.2f) { k.rot += gx * fabsf(gx) * 120.0f * (float)dt; mexeu = true; }
        if (mexeu || (novo & ORBIS_PAD_BUTTON_CROSS)) {
            if (k.x < -0.5f) k.x = -0.5f; if (k.x > 1.5f) k.x = 1.5f; if (k.y < -0.5f) k.y = -0.5f; if (k.y > 1.5f) k.y = 1.5f;
            if (k.escala < 0.02f) k.escala = 0.02f; if (k.escala > 8) k.escala = 8; if (k.alfa < 0) k.alfa = 0; if (k.alfa > 1) k.alfa = 1;
            // Um clipe com um ponto só e o cursor longe dele: mexer cria o ponto aqui (o de antes fica).
            evClipeDefinirChave(c, c->totalChaves == 1 && fabs(c->chaves[0].t - tl) < 0.02 ? 0 : tl, k.x, k.y, k.escala, k.alfa, k.rot);
            evMudar();
        }
        if (novo & ORBIS_PAD_BUTTON_SQUARE) { evClipeApagarChave(c, tl); evMudar(); }
        return true;
    }
    if (novo & ORBIS_PAD_BUTTON_UP) evTrocarFaixa(-1);
    if (novo & ORBIS_PAD_BUTTON_DOWN) evTrocarFaixa(1);
    // Analógico esquerdo: um toque = próximo item; segurando, repete.
    {
        static int ultimoH = 0, ultimoV = 0; static uint64_t proximoH = 0, proximoV = 0;
        int h = lx < 50 ? -1 : lx > 206 ? 1 : 0, v = ly < 50 ? -1 : ly > 206 ? 1 : 0;
        if (h && abs(ly - 128) > abs(lx - 128)) h = 0;   // diagonal: vale o eixo mais forte
        if (v && abs(lx - 128) > abs(ly - 128)) v = 0;
        if (h != ultimoH) proximoH = 0;
        if (v != ultimoV) proximoV = 0;
        if (h && agora >= proximoH) { if (evTocando) evTocar(false); evPassoItem(h); proximoH = agora + (ultimoH == h ? 220 : 450); }
        if (v && agora >= proximoV) { evTrocarFaixa(v); proximoV = agora + (ultimoV == v ? 300 : 450); }
        ultimoH = h; ultimoV = v;
    }
    if (evJuncao && evSelecionado()) {
        if (novo & ORBIS_PAD_BUTTON_R1) evDuracaoTransicao(0.1);
        if (novo & ORBIS_PAD_BUTTON_L1) evDuracaoTransicao(-0.1);
        if (novo & ORBIS_PAD_BUTTON_CROSS) { evAbrirEscolha(EV_E_TRANSICAO); return true; }
        if (novo & ORBIS_PAD_BUTTON_SQUARE) {   // toca a transição desde um pouco antes
            if (evTocando) evTocar(false); else { evCursor = fmax(0, evSelecionado()->inicio - 1.0); evTocar(true); }
            return true;
        }
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) { evJuncao = false; evClipeSel = -1; return true; }
        novo &= ~(ORBIS_PAD_BUTTON_R1 | ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_CROSS);
    }
    if (novo & ORBIS_PAD_BUTTON_R1) evPxPorSeg = fminf(600, evPxPorSeg * 1.4f);
    if (novo & ORBIS_PAD_BUTTON_L1) evPxPorSeg = fmaxf(4, evPxPorSeg / 1.4f);
    if (novo & ORBIS_PAD_BUTTON_R2) { evTocar(false); evIrParaPedaco(1); }
    if (novo & ORBIS_PAD_BUTTON_L2) { evTocar(false); evIrParaPedaco(-1); }
    if (novo & ORBIS_PAD_BUTTON_SQUARE) evTocar(!evTocando);
    if (novo & ORBIS_PAD_BUTTON_CROSS) evSelecionarNoCursor();
    if (novo & ORBIS_PAD_BUTTON_TRIANGLE) { evTocar(false); evAbrirMenu(); }
    if (novo & ORBIS_PAD_BUTTON_OPTIONS) evExportarComecar();
    if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
        if (evClipeSel >= 0) { evClipeSel = -1; evJuncao = false; }
        else { evTocar(false); evMenuTotal = 0; evMenuSel = 0; snprintf(evMenuTitulo, sizeof(evMenuTitulo), "Sair do editor?"); evMenuAdicionar("Salvar e sair", EV_A_SAIR); evMenuAdicionar("Continuar editando", 0); evModo = EV_M_MENU; }
    }
    return true;
}
