#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#include "jogar.h"
#include "explorar.h" 
#include "audio.h" 
#include "video.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <orbis/Sysmodule.h>
#include <orbis/Http.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include "network.h"
#include "ImeDialog.h"
#include "CommonDialog.h"
#include "menu.h"
#include "radio.h"
#include "graphics.h"
#include "stb_image.h"
#include "menu_grafico_cache_grafico.h"
#include <orbis/libkernel.h> 

#define HN_VERSAO "3.00"

extern char msgStatus[128];
extern int msgTimer;

// Player de video e TV
extern bool video_minimizado, menu_video_aberto, pip_selecionado, bloqueio_fechar_video;
extern int pip_x, pip_y, pip_w, pip_h;
extern void processarControlesMenuVideo(unsigned int btn, OrbisPadData* pData, bool& cross_consumido, bool& circle_consumido);
extern void atenderFimDeVideo();

// Radio (globais preenchidas pela thread de busca em menu.cpp/radio.cpp)
extern char nomes[3000][256];
extern char linksAtuais[3000][1024];
extern int totalItens;

extern int globalUserId;
bool emuladorAtivo = false;
char caminhoCoreSelecionado[512] = "";
char romParaCarregar[512] = ""; 

// LAUNCHER MULTI-MODO V62
int modoLauncher = 0; // 0 = XML, 1 = PASTA EMULADOR
extern void preencherMenuGBA();
extern void carregarXML(const char* nomeArquivoXML);

void alternarModoLauncher() {
    modoLauncher = (modoLauncher == 0) ? 1 : 0;
    
    if (modoLauncher == 1) { // MODO EMULADOR
        memset(nomes, 0, sizeof(nomes));
        strcpy(nomes[0], "NINTENDO GAME BOY ADVANCE");
        totalItens = 1;
        sel = 0; off = 0;
    } else { // MODO XML (Volta pro system)
        carregarXML("system.xml");
    }
}

#include "libretro_bridge.h"

void iniciarEmulador(const char* romPath) {
    bool ehGBA = false;
    const char* ext = strrchr(romPath, '.');
    if (ext && (strcasecmp(ext, ".gba") == 0)) ehGBA = true;

    if (!ehGBA && strlen(caminhoCoreSelecionado) == 0) {
        snprintf(msgStatus, sizeof(msgStatus), "ERRO: SELECIONE O NUCLEO NO EXPLORADOR!");
        msgTimer = 240;
        return;
    }
    snprintf(msgStatus, sizeof(msgStatus), "Iniciando: %s", romPath); msgTimer = 180;
    if (bridge_iniciar(caminhoCoreSelecionado, romPath)) {
        extern void pausarMusicaParaVideo();
        pausarMusicaParaVideo();
        extern void bibliotecaAoIniciarJogo();
        bibliotecaAoIniciarJogo();
        emuladorAtivo = true;
        extern MenuLevel menuAtual;
        menuAtual = MENU_EMULADOR_EXECUCAO; // V21: Foca no emulador e esconde menus
    }
}

void fecharEmulador() {
    extern void retomarMusicaAposVideo();
    bridge_finalizar();
    emuladorAtivo = false;
    retomarMusicaAposVideo();
}

struct app_launch_ctx {
    uint32_t structsize;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
} __attribute__((packed));

extern char caminhoXMLAtual[256];

// =========================================================================
// O NOVO MOTOR AGORA SALVA TUDO DENTRO DA PASTA /jogar/
// =========================================================================
void carregarXML(const char* nomeArquivoXML) {
    char pathConfig[512];

    // Aponta para a nova pasta 'jogar' no HDD
    sprintf(pathConfig, "/data/HyperNeiva/configuracao/jogar/%s", nomeArquivoXML);

    FILE* fp = fopen(pathConfig, "rb");

    // Se o ficheiro não estiver salvo no PS4, ele extrai do PKG original!
    if (!fp) {
        char pathApp0[512];
        sprintf(pathApp0, "/app0/assets/%s", nomeArquivoXML);
        FILE* fApp0 = fopen(pathApp0, "rb");
        if (fApp0) {
            fseek(fApp0, 0, SEEK_END); long sz = ftell(fApp0); fseek(fApp0, 0, SEEK_SET);
            char* bTemp = (char*)malloc(sz);
            fread(bTemp, 1, sz, fApp0);
            fclose(fApp0);

            // Cria as pastas em cascata por segurança
            sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
            sceKernelMkdir("/data/HyperNeiva/configuracao/jogar", 0777);

            // Salva na pasta jogar!
            FILE* fSave = fopen(pathConfig, "wb");
            if (fSave) {
                fwrite(bTemp, 1, sz, fSave);
                fclose(fSave);
            }
            free(bTemp);

            // Agora abre o ficheiro definitivo no HDD
            fp = fopen(pathConfig, "rb");
        }
    }

    if (!fp) {
        sprintf(msgStatus, "ERRO: %s NAO ENCONTRADO!", nomeArquivoXML);
        msgTimer = 180;
        return;
    }

    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    char* p = b;

    while (totalItens < 2000) {
        p = strstr(p, "<game name=\""); if (!p) break;
        p += 12; char* f = strchr(p, '\"');
        if (f) {
            int l = (int)(f - p);
            if (l > 255) l = 255;
            strncpy(nomes[totalItens], p, l);
            nomes[totalItens][l] = '\0';
            totalItens++;
            p = f;
        }
        else break;
    }
    free(b);
    menuAtual = JOGAR_XML;
    strcpy(caminhoXMLAtual, pathConfig);
}

void chamarJogo(const char* titleId, const char* romPath) {
    sprintf(msgStatus, "LANCANDO %s...", titleId);
    msgTimer = 120;

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);

    int libSystemService = sceKernelLoadStartModule("libSceSystemService.sprx", 0, NULL, 0, NULL, NULL);
    if (libSystemService < 0) {
        sprintf(msgStatus, "ERRO 1: MODULO RECUSADO (0x%08X)", libSystemService);
        msgTimer = 300;
        return;
    }

    typedef int (*LaunchAppFunc)(const char*, char**, void*);
    LaunchAppFunc launchAppReal = NULL;
    sceKernelDlsym(libSystemService, "sceSystemServiceLaunchApp", (void**)&launchAppReal);

    if (launchAppReal == NULL) return;

    pararAudio();

    struct app_launch_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.structsize = 24;
    ctx.user_id = globalUserId;
    ctx.app_opt = 0;
    ctx.crash_report = 0;
    ctx.check_flag = 0;

    int launchRes = 0;

    if (strcmp(titleId, "SSNE10000") == 0) {
        FILE* cfg = fopen("/data/retroarch/retroarch.cfg", "a");
        if (cfg) {
            fprintf(cfg, "\nlibretro_path = \"/data/self/retroarch/cores/genesis_plus_gx_libretro_ps4.self\"\n");
            fprintf(cfg, "rgui_browser_directory = \"/data/HyperNeiva/baixado/\"\n");
            fclose(cfg);
        }

        char* argsNulo[] = { (char*)titleId, NULL };
        sprintf(msgStatus, "INICIANDO RETROARCH...");
        launchRes = launchAppReal(titleId, argsNulo, &ctx);
    }
    else if (romPath != NULL && strlen(romPath) > 0) {
        char* args[] = { (char*)titleId, (char*)romPath, NULL };
        launchRes = launchAppReal(titleId, args, &ctx);
    }
    else {
        char* argsNulo[] = { (char*)titleId, NULL };
        launchRes = launchAppReal(titleId, argsNulo, &ctx);
    }

    if (launchRes < 0) {
        sprintf(msgStatus, "ERRO: %s (0x%08X)", titleId, launchRes);
        msgTimer = 400;
        inicializarAudio();
    }
    else {
        sprintf(msgStatus, "FECHANDO FRONTEND...");
        sceKernelUsleep(500000);

        typedef void (*ExitProcessFunc)(int);
        ExitProcessFunc exitProcessReal = NULL;

        int libKernel = sceKernelLoadStartModule("libkernel.sprx", 0, NULL, 0, NULL, NULL);
        if (libKernel >= 0) {
            sceKernelDlsym(libKernel, "sceKernelExitProcess", (void**)&exitProcessReal);
        }

        if (exitProcessReal != NULL) {
            exitProcessReal(0);
        }
        else {
            exit(0);
        }
    }
}

// ===== BIBLIOTECA DE JOGOS =====
// Favoritos, capas e trapaças da lista de Game Boy Advance. O download roda numa thread só, uma tarefa por vez.
#define BIB_PASTA "/data/HyperNeiva/emulador"
#define BIB_CAPAS BIB_PASTA "/capas"
#define BIB_TRAPACAS BIB_PASTA "/trapacas"
#define BIB_FAVORITOS BIB_PASTA "/favoritos.txt"
#define BIB_JOGADOS BIB_PASTA "/jogados.txt"
#define BIB_CAPAS_EMUSUNI "/data/EmuSuNi/covers/gba"
#define BIB_URL_CAPAS "https://thumbnails.libretro.com/Nintendo%20-%20Game%20Boy%20Advance/Named_Boxarts/"
#define BIB_URL_TRAPACAS "https://api.github.com/repos/libretro/libretro-database/contents/cht/Nintendo%20-%20Game%20Boy%20Advance"
#define BIB_RAW_TRAPACAS "https://raw.githubusercontent.com/libretro/libretro-database/master/cht/Nintendo%20-%20Game%20Boy%20Advance/"
#define BIB_MAX_FAV 1000
#define BIB_MAX_OPCOES 140
#define BIB_MAX_ACHADOS 12

extern const char* listaOpcoes[150];
extern int mapOpcoes[150], totalOpcoes, selOpcao;
extern bool showOpcoes;
extern int httpCtxId;
extern char caminhoROMsAtual[512];
extern void preencherMenuGBA();

enum { BIB_OP_INICIAR, BIB_OP_FAVORITO, BIB_OP_TRAPACAS, BIB_OP_CONTROLE, BIB_OP_RENOMEAR, BIB_OP_CAPA, BIB_OP_PESQUISAR, BIB_OP_LIMPAR, BIB_OP_ORDENAR, BIB_OP_FILTRAR, BIB_OP_CANCELAR };
enum { BIB_PAINEL_JOGO, BIB_PAINEL_TRAPACAS, BIB_PAINEL_CAPAS, BIB_PAINEL_ACHADOS_TRAPACA };
enum { BIB_TAREFA_NADA, BIB_TAREFA_TODAS_CAPAS, BIB_TAREFA_ACHAR_CAPA, BIB_TAREFA_BAIXAR_CAPA, BIB_TAREFA_ACHAR_TRAPACA, BIB_TAREFA_BAIXAR_TRAPACA, BIB_TAREFA_PREVIA };
enum { BIB_IME_NADA, BIB_IME_RENOMEAR, BIB_IME_PESQUISAR };

struct BibTrapaca { char desc[120]; char codigo[400]; bool ligada; };

static char (*bibFavoritos)[256] = NULL;
static int bibTotalFavoritos = 0, bibAba = 0, bibOrdem = 0, bibPainel = BIB_PAINEL_JOGO, bibModoIme = BIB_IME_NADA;
static char bibBusca[128] = "", bibJogo[256] = "", bibSubtitulo[200] = "";
static char bibOpcoes[150][200];
static char bibAchados[BIB_MAX_ACHADOS][200];
static int bibTotalAchados = 0;
static BibTrapaca* bibTrapacas = NULL;
static int bibTotalTrapacas = 0;
static wchar_t bibTextoIme[300];
static bool bibCapasPedidas = false;

static volatile int bibTarefa = BIB_TAREFA_NADA, bibResultado = BIB_TAREFA_NADA, bibTarefaEspera = BIB_TAREFA_NADA;
static unsigned char* bibPreviaImg = NULL;
static int bibPreviaW = 0, bibPreviaH = 0, bibPreviaSel = -1, bibPreviaQuadros = 0;
static volatile bool bibThreadViva = false, bibCapaNova = false;
static char bibTarefaNome[256] = "", bibTarefaJogo[256] = "";
static char (*bibFilaCapas)[256] = NULL;
static int bibTotalFila = 0;

static void bibBase(const char* jogo, char* saida, size_t tamanho) {
    snprintf(saida, tamanho, "%s", jogo);
    char* ponto = strrchr(saida, '.');
    if (ponto) *ponto = '\0';
}

static void bibPastas() {
    sceKernelMkdir(BIB_PASTA, 0777);
    sceKernelMkdir(BIB_CAPAS, 0777);
    sceKernelMkdir(BIB_TRAPACAS, 0777);
}

static bool bibExiste(const char* caminho) {
    struct stat info;
    return stat(caminho, &info) == 0 && info.st_size > 0;
}

bool bibliotecaCaminhoCapa(const char* jogo, char* saida, size_t tamanho) {
    char base[256];
    bibBase(jogo, base, sizeof(base));
    snprintf(saida, tamanho, "%s/%s.png", BIB_CAPAS, base);
    if (bibExiste(saida)) return true;
    snprintf(saida, tamanho, "%s/%s.png", BIB_CAPAS_EMUSUNI, base);
    return bibExiste(saida);
}

static void bibCarregarFavoritos() {
    if (!bibFavoritos) bibFavoritos = (char (*)[256])calloc(BIB_MAX_FAV, 256);
    bibTotalFavoritos = 0;
    FILE* f = fopen(BIB_FAVORITOS, "r");
    if (!f || !bibFavoritos) { if (f) fclose(f); return; }
    char linha[300];
    while (bibTotalFavoritos < BIB_MAX_FAV && fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\r\n")] = '\0';
        if (linha[0]) snprintf(bibFavoritos[bibTotalFavoritos++], 256, "%s", linha);
    }
    fclose(f);
}

static void bibGravarFavoritos() {
    bibPastas();
    FILE* f = fopen(BIB_FAVORITOS, "w");
    if (!f) return;
    for (int i = 0; i < bibTotalFavoritos; i++) fprintf(f, "%s\n", bibFavoritos[i]);
    fclose(f);
}

bool bibliotecaFavorito(const char* nome) {
    for (int i = 0; i < bibTotalFavoritos; i++) if (strcmp(bibFavoritos[i], nome) == 0) return true;
    return false;
}

static void bibAlternarFavorito(const char* nome) {
    extern void conquistaGanhar(const char* chave);
    conquistaGanhar("favorito");
    if (!bibFavoritos) bibCarregarFavoritos();
    if (!bibFavoritos) return;
    for (int i = 0; i < bibTotalFavoritos; i++) {
        if (strcmp(bibFavoritos[i], nome) != 0) continue;
        memmove(bibFavoritos[i], bibFavoritos[i + 1], (size_t)(bibTotalFavoritos - i - 1) * 256);
        bibTotalFavoritos--;
        bibGravarFavoritos();
        snprintf(msgStatus, sizeof(msgStatus), "REMOVIDO DOS FAVORITOS");
        msgTimer = 120;
        return;
    }
    if (bibTotalFavoritos >= BIB_MAX_FAV) return;
    snprintf(bibFavoritos[bibTotalFavoritos++], 256, "%s", nome);
    bibGravarFavoritos();
    snprintf(msgStatus, sizeof(msgStatus), "ADICIONADO AOS FAVORITOS");
    msgTimer = 120;
}

static int bibRecente(const char* nome) {
    FILE* f = fopen(BIB_JOGADOS, "r");
    if (!f) return 1 << 30;
    char linha[300];
    int posicao = 0;
    while (fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\r\n")] = '\0';
        if (strcmp(linha, nome) == 0) { fclose(f); return posicao; }
        posicao++;
    }
    fclose(f);
    return 1 << 30;
}

static void bibRegistrarJogado(const char* nome) {
    bibPastas();
    static char linhas[200][256];
    int total = 0;
    FILE* f = fopen(BIB_JOGADOS, "r");
    if (f) {
        char linha[300];
        while (total < 199 && fgets(linha, sizeof(linha), f)) {
            linha[strcspn(linha, "\r\n")] = '\0';
            if (linha[0] && strcmp(linha, nome) != 0) snprintf(linhas[total++], 256, "%s", linha);
        }
        fclose(f);
    }
    f = fopen(BIB_JOGADOS, "w");
    if (!f) return;
    fprintf(f, "%s\n", nome);
    for (int i = 0; i < total; i++) fprintf(f, "%s\n", linhas[i]);
    fclose(f);
}

static int bibCompararDesc(const void* a, const void* b) { return strcasecmp((const char*)b, (const char*)a); }

static int bibCompararRecente(const void* a, const void* b) {
    int x = bibRecente((const char*)a), y = bibRecente((const char*)b);
    return x != y ? (x < y ? -1 : 1) : strcasecmp((const char*)a, (const char*)b);
}

// Quantas letras separam duas palavras curtas: "michey" e "mickey" estão a uma.
static int bibDistancia(const char* a, const char* b) {
    int ta = (int)strlen(a), tb = (int)strlen(b);
    int anterior[40], atual[40];
    if (ta > 32 || tb > 32) return 99;
    for (int y = 0; y <= tb; y++) anterior[y] = y;
    for (int x = 1; x <= ta; x++) {
        atual[0] = x;
        for (int y = 1; y <= tb; y++) {
            int troca = anterior[y - 1] + (a[x - 1] != b[y - 1]);
            int apaga = anterior[y] + 1, poe = atual[y - 1] + 1;
            atual[y] = troca < apaga ? (troca < poe ? troca : poe) : (apaga < poe ? apaga : poe);
        }
        memcpy(anterior, atual, sizeof(int) * (size_t)(tb + 1));
    }
    return anterior[tb];
}

static int bibPalavras(const char* nome, char palavras[][32], int maximo);

// 0 = não combina. Começo do nome vale mais que começo de palavra, que vale mais que erro de digitação.
static int bibPontosBusca(const char* nome) {
    char termos[6][32], palavras[12][32];
    int totalTermos = bibPalavras(bibBusca, termos, 6), totalPalavras = bibPalavras(nome, palavras, 12), soma = 0;
    if (totalTermos == 0) return 1;
    for (int t = 0; t < totalTermos; t++) {
        size_t n = strlen(termos[t]);
        int melhor = 0;
        for (int p = 0; p < totalPalavras; p++) {
            int pontos = 0;
            if (strncmp(palavras[p], termos[t], n) == 0) pontos = p == 0 ? 100 : 70;
            else if (n >= 3 && strstr(palavras[p], termos[t])) pontos = 40;
            else if (n >= 4) {
                int limite = n >= 7 ? 2 : 1;
                char corte[32];
                snprintf(corte, sizeof(corte), "%.*s", (int)n, palavras[p]);
                if (bibDistancia(termos[t], palavras[p]) <= limite) pontos = 30;
                else if (strlen(palavras[p]) > n && bibDistancia(termos[t], corte) <= limite) pontos = 20;
            }
            if (pontos > melhor) melhor = pontos;
        }
        if (melhor == 0) return 0;
        soma += melhor;
    }
    return soma;
}

static int bibCompararBusca(const void* a, const void* b) {
    int x = bibPontosBusca((const char*)a), y = bibPontosBusca((const char*)b);
    return x != y ? (x > y ? -1 : 1) : strcasecmp((const char*)a, (const char*)b);
}

// Recebe a lista de A a Z em nomes[] e aplica aba, busca e ordem.
void bibliotecaAplicar() {
    if (!bibFavoritos) bibCarregarFavoritos();
    int fica = 0;
    for (int i = 0; i < totalItens; i++) {
        if (bibAba == 1 && !bibliotecaFavorito(nomes[i])) continue;
        if (bibBusca[0] && bibPontosBusca(nomes[i]) == 0) continue;
        if (fica != i) memcpy(nomes[fica], nomes[i], 256);
        fica++;
    }
    for (int i = fica; i < totalItens; i++) nomes[i][0] = '\0';
    totalItens = fica;
    if (bibBusca[0]) qsort(nomes, totalItens, sizeof(nomes[0]), bibCompararBusca);
    else if (bibOrdem == 1) qsort(nomes, totalItens, sizeof(nomes[0]), bibCompararDesc);
    else if (bibOrdem == 2) qsort(nomes, totalItens, sizeof(nomes[0]), bibCompararRecente);
    static const char* ordens[] = { "A a Z", "Z a A", "Jogados recentemente" };
    if (bibBusca[0]) snprintf(bibSubtitulo, sizeof(bibSubtitulo), "%d resultados para: %.60s   (O limpa)", totalItens, bibBusca);
    else snprintf(bibSubtitulo, sizeof(bibSubtitulo), "%d jogos  -  %s", totalItens, ordens[bibOrdem]);
}

const char* bibliotecaSubtitulo() { return bibSubtitulo; }
int bibliotecaAba() { return bibAba; }

const char* bibliotecaTituloPainel() {
    if (bibPainel == BIB_PAINEL_TRAPACAS) return "Trapaças";
    if (bibPainel == BIB_PAINEL_CAPAS) return "Escolha a capa";
    if (bibPainel == BIB_PAINEL_ACHADOS_TRAPACA) return "Escolha a lista";
    return "Jogo";
}

static void bibRecarregar(const char* manter) {
    preencherMenuGBA();
    for (int i = 0; manter && i < totalItens; i++) if (strcmp(nomes[i], manter) == 0) { sel = i; break; }
    clearCoverCache();
}

static bool bibSemJogo() { return totalItens <= 0 || strcmp(nomes[sel], "Nenhuma ROM GBA encontrada") == 0; }

static void bibLimparPrevia() {
    if (bibPreviaImg) { stbi_image_free(bibPreviaImg); bibPreviaImg = NULL; }
    bibPreviaSel = -1;
    bibPreviaQuadros = 0;
}

unsigned char* bibliotecaPrevia(int* w, int* h) {
    *w = bibPreviaW;
    *h = bibPreviaH;
    return showOpcoes && bibPainel == BIB_PAINEL_CAPAS ? bibPreviaImg : NULL;
}

static void bibFecharPainel() { showOpcoes = false; bibPainel = BIB_PAINEL_JOGO; bibLimparPrevia(); }

static void bibMostrarPainel(int painel, int total) {
    for (int i = total; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
    bibPainel = painel;
    totalOpcoes = total;
    selOpcao = 0;
    showOpcoes = true;
}

static void bibAbrirPainelSemJogo() {
    int n = 0;
    listaOpcoes[n] = "Pesquisar"; mapOpcoes[n++] = BIB_OP_PESQUISAR;
    if (bibBusca[0]) { listaOpcoes[n] = "Limpar pesquisa"; mapOpcoes[n++] = BIB_OP_LIMPAR; }
    if (bibAba) { listaOpcoes[n] = "Filtrar: todos os jogos"; mapOpcoes[n++] = BIB_OP_FILTRAR; }
    listaOpcoes[n] = "Cancelar"; mapOpcoes[n++] = BIB_OP_CANCELAR;
    bibMostrarPainel(BIB_PAINEL_JOGO, n);
}

static void bibAbrirPainelJogo() {
    int n = 0;
    snprintf(bibJogo, sizeof(bibJogo), "%s", nomes[sel]);
    listaOpcoes[n] = "Iniciar"; mapOpcoes[n++] = BIB_OP_INICIAR;
    listaOpcoes[n] = bibliotecaFavorito(bibJogo) ? "Remover dos favoritos" : "Adicionar aos favoritos"; mapOpcoes[n++] = BIB_OP_FAVORITO;
    listaOpcoes[n] = "Trapaças"; mapOpcoes[n++] = BIB_OP_TRAPACAS;
    listaOpcoes[n] = "Controle"; mapOpcoes[n++] = BIB_OP_CONTROLE;
    listaOpcoes[n] = "Renomear"; mapOpcoes[n++] = BIB_OP_RENOMEAR;
    listaOpcoes[n] = "Buscar capa"; mapOpcoes[n++] = BIB_OP_CAPA;
    listaOpcoes[n] = "Pesquisar"; mapOpcoes[n++] = BIB_OP_PESQUISAR;
    if (bibBusca[0]) { listaOpcoes[n] = "Limpar pesquisa"; mapOpcoes[n++] = BIB_OP_LIMPAR; }
    listaOpcoes[n] = "Ordenar"; mapOpcoes[n++] = BIB_OP_ORDENAR;
    listaOpcoes[n] = bibAba ? "Filtrar: todos os jogos" : "Filtrar: só favoritos"; mapOpcoes[n++] = BIB_OP_FILTRAR;
    listaOpcoes[n] = "Cancelar"; mapOpcoes[n++] = BIB_OP_CANCELAR;
    bibMostrarPainel(BIB_PAINEL_JOGO, n);
}

// ----- Download -----
static void bibUrl(const char* texto, char* saida, size_t tamanho) {
    size_t n = 0;
    for (const unsigned char* p = (const unsigned char*)texto; *p && n + 4 < tamanho; p++) {
        if (isalnum(*p) || strchr("-_.~", *p)) saida[n++] = (char)*p;
        else n += (size_t)snprintf(saida + n, 4, "%%%02X", *p);
    }
    saida[n] = '\0';
}

static bool bibBaixar(const char* url, const char* destino) {
    char temporario[600];
    snprintf(temporario, sizeof(temporario), "%s.tmp", destino);
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
    bool certo = false;
    if (req >= 0 && sceHttpSendRequest(req, NULL, 0) >= 0) {
        int32_t codigo = 0;
        sceHttpGetStatusCode(req, &codigo);
        if (codigo == 200) {
            FILE* f = fopen(temporario, "wb");
            if (f) {
                static unsigned char bloco[32768];
                int n;
                long total = 0;
                while ((n = sceHttpReadData(req, bloco, sizeof(bloco))) > 0) { fwrite(bloco, 1, (size_t)n, f); total += n; }
                fclose(f);
                certo = n == 0 && total > 0;
            }
        }
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    if (certo) rename(temporario, destino);
    else unlink(temporario);
    return certo;
}

static void bibNomeLibretro(const char* base, char* saida, size_t tamanho) {
    snprintf(saida, tamanho, "%s", base);
    for (char* p = saida; *p; p++) if (strchr("&*/:`<>?\\|", *p)) *p = '_';
}

// Palavras do nome antes do primeiro parêntese, em minúsculas e sem pontuação.
static int bibPalavras(const char* nome, char palavras[][32], int maximo) {
    int total = 0, n = 0;
    for (const char* p = nome;; p++) {
        bool fim = !*p || *p == '(' || *p == '[';
        if (!fim && isalnum((unsigned char)*p)) {
            if (n < 31) palavras[total][n++] = (char)tolower((unsigned char)*p);
            continue;
        }
        if (n > 0) { palavras[total][n] = '\0'; n = 0; total++; if (total >= maximo) break; }
        if (fim) break;
    }
    return total;
}

static int bibPontuar(const char* jogo, const char* candidato) {
    char a[12][32], b[12][32];
    int ta = bibPalavras(jogo, a, 12), tb = bibPalavras(candidato, b, 12), iguais = 0;
    for (int i = 0; i < ta; i++) for (int j = 0; j < tb; j++) if (strcmp(a[i], b[j]) == 0) { iguais++; break; }
    if (iguais == 0) return 0;
    return iguais * 20 - abs(ta - tb) * 3;
}

static void bibGuardarAchado(const char* candidato, int* pontos, const char* jogo) {
    int p = bibPontuar(jogo, candidato);
    if (p <= 0) return;
    int i = bibTotalAchados < BIB_MAX_ACHADOS ? bibTotalAchados++ : BIB_MAX_ACHADOS - 1;
    if (i == BIB_MAX_ACHADOS - 1 && pontos[i] >= p && bibAchados[i][0]) return;
    while (i > 0 && pontos[i - 1] < p) {
        pontos[i] = pontos[i - 1];
        memcpy(bibAchados[i], bibAchados[i - 1], sizeof(bibAchados[0]));
        i--;
    }
    pontos[i] = p;
    snprintf(bibAchados[i], sizeof(bibAchados[0]), "%s", candidato);
}

static char* bibLerArquivo(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long tamanho = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* dados = (char*)malloc((size_t)tamanho + 1);
    if (dados) { dados[fread(dados, 1, (size_t)tamanho, f)] = '\0'; }
    fclose(f);
    return dados;
}

static void bibDecodificar(char* texto) {
    char* escrita = texto;
    for (char* p = texto; *p; p++) {
        unsigned int valor;
        if (*p == '%' && sscanf(p + 1, "%2x", &valor) == 1) { *escrita++ = (char)valor; p += 2; }
        else if (strncmp(p, "&amp;", 5) == 0) { *escrita++ = '&'; p += 4; }
        else *escrita++ = *p;
    }
    *escrita = '\0';
}

static void bibAcharCapas(const char* jogo) {
    int pontos[BIB_MAX_ACHADOS] = { 0 };
    const char* lista = BIB_PASTA "/lista_capas.html";
    bibTotalAchados = 0;
    if (!bibExiste(lista) && !bibBaixar(BIB_URL_CAPAS, lista)) return;
    char* dados = bibLerArquivo(lista);
    if (!dados) return;
    for (char* p = dados; (p = strstr(p, "href=\"")) != NULL;) {
        p += 6;
        char* fim = strstr(p, ".png\"");
        char* aspas = strchr(p, '"');
        if (!fim || !aspas || fim > aspas) continue;
        char nome[256];
        int n = (int)(fim - p);
        if (n > 250) n = 250;
        memcpy(nome, p, (size_t)n);
        nome[n] = '\0';
        bibDecodificar(nome);
        bibGuardarAchado(nome, pontos, jogo);
    }
    free(dados);
}

static void bibAcharTrapacas(const char* jogo) {
    int pontos[BIB_MAX_ACHADOS] = { 0 };
    const char* lista = BIB_PASTA "/lista_trapacas.json";
    bibTotalAchados = 0;
    if (!bibExiste(lista) && !bibBaixar(BIB_URL_TRAPACAS, lista)) return;
    char* dados = bibLerArquivo(lista);
    if (!dados) return;
    for (char* p = dados; (p = strstr(p, "\"name\":")) != NULL;) {
        p = strchr(p + 7, '"');
        if (!p) break;
        p++;
        char* fim = strchr(p, '"');
        if (!fim) break;
        char nome[256];
        int n = (int)(fim - p);
        if (n > 250) n = 250;
        memcpy(nome, p, (size_t)n);
        nome[n] = '\0';
        p = fim;
        char* ponto = strrchr(nome, '.');
        if (!ponto || strcmp(ponto, ".cht") != 0) continue;
        *ponto = '\0';
        bibGuardarAchado(nome, pontos, jogo);
    }
    free(dados);
}

static void* bibThread(void*) {
    int tarefa = bibTarefa;
    char url[900], nome[600], destino[600], base[256];
    bibPastas();
    if (tarefa == BIB_TAREFA_TODAS_CAPAS) {
        for (int i = 0; i < bibTotalFila; i++) {
            char existente[600];
            if (bibliotecaCaminhoCapa(bibFilaCapas[i], existente, sizeof(existente))) continue;
            bibBase(bibFilaCapas[i], base, sizeof(base));
            bibNomeLibretro(base, nome, sizeof(nome));
            char codificado[700];
            bibUrl(nome, codificado, sizeof(codificado));
            snprintf(url, sizeof(url), "%s%s.png", BIB_URL_CAPAS, codificado);
            snprintf(destino, sizeof(destino), "%s/%s.png", BIB_CAPAS, base);
            if (bibBaixar(url, destino)) bibCapaNova = true;
            // O pedido do usuário passa na frente; o resto das capas volta depois.
            if (bibTarefaEspera != BIB_TAREFA_NADA) { bibCapasPedidas = false; break; }
        }
    }
    else if (tarefa == BIB_TAREFA_PREVIA) {
        char codificado[700];
        bibUrl(bibTarefaNome, codificado, sizeof(codificado));
        snprintf(url, sizeof(url), "%s%s.png", BIB_URL_CAPAS, codificado);
        bibBaixar(url, BIB_PASTA "/previa.png");
    }
    else if (tarefa == BIB_TAREFA_ACHAR_CAPA) bibAcharCapas(bibTarefaJogo);
    else if (tarefa == BIB_TAREFA_ACHAR_TRAPACA) bibAcharTrapacas(bibTarefaJogo);
    else if (tarefa == BIB_TAREFA_BAIXAR_CAPA || tarefa == BIB_TAREFA_BAIXAR_TRAPACA) {
        bool capa = tarefa == BIB_TAREFA_BAIXAR_CAPA;
        char codificado[700];
        bibBase(bibTarefaJogo, base, sizeof(base));
        bibUrl(bibTarefaNome, codificado, sizeof(codificado));
        snprintf(url, sizeof(url), "%s%s%s", capa ? BIB_URL_CAPAS : BIB_RAW_TRAPACAS, codificado, capa ? ".png" : ".cht");
        snprintf(destino, sizeof(destino), "%s/%s%s", capa ? BIB_CAPAS : BIB_TRAPACAS, base, capa ? ".png" : ".cht");
        bool certo = bibBaixar(url, destino);
        if (capa && certo) bibCapaNova = true;
        if (!certo) { snprintf(msgStatus, sizeof(msgStatus), "NAO FOI POSSIVEL BAIXAR"); msgTimer = 240; }
    }
    bibResultado = tarefa;
    bibTarefa = BIB_TAREFA_NADA;
    bibThreadViva = false;
    return NULL;
}

static bool bibIniciarTarefa(int tarefa) {
    if (bibThreadViva) { bibTarefaEspera = tarefa; return true; }
    bibTarefa = tarefa;
    bibThreadViva = true;
    pthread_t id;
    pthread_attr_t atributos;
    pthread_attr_init(&atributos);
    pthread_attr_setstacksize(&atributos, 512 * 1024);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);
    int erro = pthread_create(&id, &atributos, bibThread, NULL);
    pthread_attr_destroy(&atributos);
    if (erro != 0) { bibThreadViva = false; bibTarefa = BIB_TAREFA_NADA; return false; }
    return true;
}

// ----- Trapaças -----
static void bibCaminhoTrapaca(const char* jogo, char* saida, size_t tamanho) {
    char base[256];
    bibBase(jogo, base, sizeof(base));
    snprintf(saida, tamanho, "%s/%s.cht", BIB_TRAPACAS, base);
}

static void bibValorCht(const char* linha, char* saida, size_t tamanho) {
    const char* inicio = strchr(linha, '=');
    saida[0] = '\0';
    if (!inicio) return;
    inicio++;
    while (*inicio == ' ' || *inicio == '"') inicio++;
    snprintf(saida, tamanho, "%s", inicio);
    size_t n = strcspn(saida, "\"\r\n");
    saida[n] = '\0';
}

static int bibLerTrapacas(const char* jogo) {
    char caminho[600];
    bibCaminhoTrapaca(jogo, caminho, sizeof(caminho));
    if (!bibTrapacas) bibTrapacas = (BibTrapaca*)calloc(BIB_MAX_OPCOES, sizeof(BibTrapaca));
    bibTotalTrapacas = 0;
    FILE* f = fopen(caminho, "r");
    if (!f || !bibTrapacas) { if (f) fclose(f); return 0; }
    memset(bibTrapacas, 0, sizeof(BibTrapaca) * BIB_MAX_OPCOES);
    char linha[640];
    while (fgets(linha, sizeof(linha), f)) {
        int indice = -1, usados = 0;
        if (sscanf(linha, "cheat%d_%n", &indice, &usados) < 1 || indice < 0 || indice >= BIB_MAX_OPCOES) continue;
        BibTrapaca* t = &bibTrapacas[indice];
        const char* campo = linha + usados;
        if (strncmp(campo, "desc", 4) == 0) bibValorCht(linha, t->desc, sizeof(t->desc));
        else if (strncmp(campo, "code", 4) == 0) bibValorCht(linha, t->codigo, sizeof(t->codigo));
        else if (strncmp(campo, "enable", 6) == 0) t->ligada = strstr(campo, "true") != NULL;
        if (indice >= bibTotalTrapacas) bibTotalTrapacas = indice + 1;
    }
    fclose(f);
    return bibTotalTrapacas;
}

static void bibGravarTrapacas(const char* jogo) {
    char caminho[600];
    bibCaminhoTrapaca(jogo, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "w");
    if (!f) return;
    fprintf(f, "cheats = %d\n\n", bibTotalTrapacas);
    for (int i = 0; i < bibTotalTrapacas; i++) {
        fprintf(f, "cheat%d_desc = \"%s\"\ncheat%d_code = \"%s\"\ncheat%d_enable = %s\n\n", i, bibTrapacas[i].desc, i, bibTrapacas[i].codigo, i, bibTrapacas[i].ligada ? "true" : "false");
    }
    fclose(f);
}

static void bibMostrarTrapacas(int selecionada) {
    int n = 0;
    for (int i = 0; i < bibTotalTrapacas && n < BIB_MAX_OPCOES; i++) {
        snprintf(bibOpcoes[n], sizeof(bibOpcoes[0]), "[%c] %.180s", bibTrapacas[i].ligada ? 'X' : ' ', bibTrapacas[i].desc[0] ? bibTrapacas[i].desc : "Sem nome");
        listaOpcoes[n] = bibOpcoes[n];
        mapOpcoes[n] = i;
        n++;
    }
    listaOpcoes[n] = "Baixar outra lista";
    mapOpcoes[n++] = 1000;
    bibMostrarPainel(BIB_PAINEL_TRAPACAS, n);
    selOpcao = selecionada < n ? selecionada : 0;
}

static void bibMostrarAchados(int painel) {
    if (bibTotalAchados == 0) {
        bibFecharPainel();
        snprintf(msgStatus, sizeof(msgStatus), "NADA ENCONTRADO PARA ESTE JOGO");
        msgTimer = 240;
        return;
    }
    for (int i = 0; i < bibTotalAchados; i++) { listaOpcoes[i] = bibAchados[i]; mapOpcoes[i] = i; }
    bibMostrarPainel(painel, bibTotalAchados);
}

void bibliotecaAoIniciarJogo() {
    extern void (*retro_cheat_reset_ptr)(void);
    extern void (*retro_cheat_set_ptr)(unsigned, bool, const char*);
    extern char romParaCarregar[512];
    const char* nome = strrchr(romParaCarregar, '/');
    nome = nome ? nome + 1 : romParaCarregar;
    bibRegistrarJogado(nome);
    if (!retro_cheat_set_ptr) return;
    if (retro_cheat_reset_ptr) retro_cheat_reset_ptr();
    int total = bibLerTrapacas(nome), ligadas = 0;
    for (int i = 0; i < total; i++) {
        if (!bibTrapacas[i].ligada || !bibTrapacas[i].codigo[0]) continue;
        retro_cheat_set_ptr((unsigned)ligadas++, true, bibTrapacas[i].codigo);
    }
    if (ligadas) { snprintf(msgStatus, sizeof(msgStatus), "TRAPACAS ATIVAS: %d", ligadas); msgTimer = 240; }
}

// ----- Teclado -----
static void bibAbrirTeclado(int modo, const wchar_t* titulo, const char* inicial) {
    if (bibModoIme != BIB_IME_NADA) return;
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(bibTextoIme, 0, sizeof(bibTextoIme));
    for (int i = 0; inicial[i] && i < 250; i++) bibTextoIme[i] = (unsigned char)inicial[i];
    int32_t usuario = 0;
    sceUserServiceGetInitialUser(&usuario);
    param.userId = usuario;
    param.maxTextLength = 250;
    param.inputTextBuffer = bibTextoIme;
    param.title = titulo;
    param.type = (OrbisImeType)0;
    if (sceImeDialogInit(&param, NULL) >= 0) bibModoIme = modo;
}

static void bibRenomear(const char* novoSemExtensao) {
    char antigo[800], novo[800], baseAntiga[256], arquivoNovo[300];
    const char* extensao = strrchr(bibJogo, '.');
    if (!extensao || strpbrk(novoSemExtensao, "/\\:*?\"<>|")) { snprintf(msgStatus, sizeof(msgStatus), "NOME INVALIDO"); msgTimer = 180; return; }
    snprintf(arquivoNovo, sizeof(arquivoNovo), "%s%s", novoSemExtensao, extensao);
    snprintf(antigo, sizeof(antigo), "%s/%s", caminhoROMsAtual, bibJogo);
    snprintf(novo, sizeof(novo), "%s/%s", caminhoROMsAtual, arquivoNovo);
    if (bibExiste(novo) || rename(antigo, novo) != 0) { snprintf(msgStatus, sizeof(msgStatus), "NAO FOI POSSIVEL RENOMEAR"); msgTimer = 180; return; }
    bibBase(bibJogo, baseAntiga, sizeof(baseAntiga));
    snprintf(antigo, sizeof(antigo), "%s/%s.png", BIB_CAPAS, baseAntiga);
    snprintf(novo, sizeof(novo), "%s/%s.png", BIB_CAPAS, novoSemExtensao);
    rename(antigo, novo);
    snprintf(antigo, sizeof(antigo), "%s/%s.cht", BIB_TRAPACAS, baseAntiga);
    snprintf(novo, sizeof(novo), "%s/%s.cht", BIB_TRAPACAS, novoSemExtensao);
    rename(antigo, novo);
    for (int i = 0; i < bibTotalFavoritos; i++) {
        if (strcmp(bibFavoritos[i], bibJogo) == 0) { snprintf(bibFavoritos[i], 256, "%s", arquivoNovo); bibGravarFavoritos(); break; }
    }
    bibRecarregar(arquivoNovo);
    snprintf(msgStatus, sizeof(msgStatus), "JOGO RENOMEADO");
    msgTimer = 180;
}

// Chamada a cada quadro dos menus.
void bibliotecaAtualizar() {
    if (bibModoIme != BIB_IME_NADA) {
        int status = (int)sceImeDialogGetStatus();
        if (status != 0 && status != 1) {
            OrbisDialogResult resultado;
            memset(&resultado, 0, sizeof(resultado));
            sceImeDialogGetResult(&resultado);
            int32_t botao = *(int32_t*)&resultado;
            char texto[260];
            int n = 0;
            uint16_t* lido = (uint16_t*)bibTextoIme;
            for (; n < 255 && lido[n]; n++) texto[n] = (char)lido[n];
            texto[n] = '\0';
            sceImeDialogTerm();
            int modo = bibModoIme;
            bibModoIme = BIB_IME_NADA;
            if (botao == 0 && modo == BIB_IME_PESQUISAR) {
                snprintf(bibBusca, sizeof(bibBusca), "%s", texto);
                bibRecarregar(NULL);
            }
            else if (botao == 0 && modo == BIB_IME_RENOMEAR && n > 0) bibRenomear(texto);
        }
    }
    if (menuAtual != MENU_EMULADOR) return;

    if (!bibCapasPedidas && !bibThreadViva && strstr(caminhoROMsAtual, "Game Boy Adva")) {
        bibCapasPedidas = true;
        if (!bibFilaCapas) bibFilaCapas = (char (*)[256])calloc(3000, 256);
        if (bibFilaCapas) {
            bibTotalFila = 0;
            for (int i = 0; i < totalItens && i < 3000; i++) if (strcmp(nomes[i], "Nenhuma ROM GBA encontrada") != 0) memcpy(bibFilaCapas[bibTotalFila++], nomes[i], 256);
            bibIniciarTarefa(BIB_TAREFA_TODAS_CAPAS);
        }
    }
    // O Círculo fecha o painel por fora: sem isto a prévia ficava no lugar da capa de todo jogo.
    if (!showOpcoes && bibPainel != BIB_PAINEL_JOGO) bibPainel = BIB_PAINEL_JOGO;
    if (bibPainel != BIB_PAINEL_CAPAS && (bibPreviaImg || bibPreviaSel != -1)) bibLimparPrevia();
    // Só as posições sem capa tentam de novo: limpar tudo fazia a tela piscar a cada download.
    if (bibCapaNova) {
        bibCapaNova = false;
        for (int i = 0; i < MAX_GRID_CACHE; i++) if (coverCache[i].index != -1 && !coverCache[i].img) coverCache[i].index = -1;
    }
    if (!bibThreadViva && bibTarefaEspera != BIB_TAREFA_NADA) {
        int proxima = bibTarefaEspera;
        bibTarefaEspera = BIB_TAREFA_NADA;
        bibIniciarTarefa(proxima);
    }
    // Espera a seleção parar antes de baixar a prévia da capa.
    if (showOpcoes && bibPainel == BIB_PAINEL_CAPAS && !bibThreadViva && bibTarefaEspera == BIB_TAREFA_NADA) {
        if (bibPreviaSel != selOpcao) {
            if (++bibPreviaQuadros > 20) {
                bibPreviaQuadros = 0;
                bibPreviaSel = selOpcao;
                snprintf(bibTarefaNome, sizeof(bibTarefaNome), "%s", bibAchados[mapOpcoes[selOpcao]]);
                bibIniciarTarefa(BIB_TAREFA_PREVIA);
            }
        }
    }
    int feita = bibResultado;
    if (feita == BIB_TAREFA_NADA) return;
    bibResultado = BIB_TAREFA_NADA;
    if (feita == BIB_TAREFA_BAIXAR_CAPA) {
        // A capa escolhida substitui uma que já estava na tela.
        clearCoverCache();
        snprintf(msgStatus, sizeof(msgStatus), "CAPA ATUALIZADA");
        msgTimer = 150;
    }
    else if (feita == BIB_TAREFA_PREVIA) {
        if (showOpcoes && bibPainel == BIB_PAINEL_CAPAS) {
            int componentes = 0;
            if (bibPreviaImg) stbi_image_free(bibPreviaImg);
            bibPreviaImg = stbi_load(BIB_PASTA "/previa.png", &bibPreviaW, &bibPreviaH, &componentes, 4);
        }
    }
    else if (feita == BIB_TAREFA_ACHAR_CAPA) bibMostrarAchados(BIB_PAINEL_CAPAS);
    else if (feita == BIB_TAREFA_ACHAR_TRAPACA) bibMostrarAchados(BIB_PAINEL_ACHADOS_TRAPACA);
    else if (feita == BIB_TAREFA_BAIXAR_TRAPACA) {
        if (bibLerTrapacas(bibJogo) > 0) bibMostrarTrapacas(0);
    }
}

void bibliotecaTriangulo() {
    if (showOpcoes) { bibFecharPainel(); return; }
    if (bibSemJogo()) bibAbrirPainelSemJogo();
    else bibAbrirPainelJogo();
}

// O Círculo desfaz a pesquisa e a aba de favoritos antes de sair da biblioteca.
bool bibliotecaCirculo() {
    if (bibBusca[0]) { bibBusca[0] = '\0'; bibRecarregar(NULL); return true; }
    if (bibAba) { bibAba = 0; bibRecarregar(NULL); return true; }
    return false;
}

void bibliotecaBotoes(uint32_t novos) {
    if (showOpcoes || bibModoIme != BIB_IME_NADA) return;
    if (novos & (ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1)) {
        bibAba = bibAba ? 0 : 1;
        bibRecarregar(NULL);
    }
    else if ((novos & ORBIS_PAD_BUTTON_L2) && !bibSemJogo()) {
        char nome[256];
        snprintf(nome, sizeof(nome), "%s", nomes[sel]);
        bibAlternarFavorito(nome);
        if (bibAba == 1) bibRecarregar(NULL);
    }
    else if (novos & ORBIS_PAD_BUTTON_R2) bibAbrirTeclado(BIB_IME_PESQUISAR, L"Pesquisar jogo", bibBusca);
    else if (novos & ORBIS_PAD_BUTTON_OPTIONS) {
        char nome[256];
        snprintf(nome, sizeof(nome), "%s", nomes[sel]);
        bibOrdem = (bibOrdem + 1) % 3;
        bibRecarregar(nome);
    }
}

// Devolve true quando o X foi usado pelo painel.
bool bibliotecaCruz() {
    if (!showOpcoes) return false;
    int escolha = mapOpcoes[selOpcao];
    if (bibPainel == BIB_PAINEL_TRAPACAS) {
        if (escolha == 1000) { snprintf(bibTarefaJogo, sizeof(bibTarefaJogo), "%s", bibJogo); bibIniciarTarefa(BIB_TAREFA_ACHAR_TRAPACA); return true; }
        if (escolha >= 0 && escolha < bibTotalTrapacas) {
            bibTrapacas[escolha].ligada = !bibTrapacas[escolha].ligada;
            bibGravarTrapacas(bibJogo);
            bibMostrarTrapacas(selOpcao);
        }
        return true;
    }
    if (bibPainel == BIB_PAINEL_CAPAS || bibPainel == BIB_PAINEL_ACHADOS_TRAPACA) {
        bool capa = bibPainel == BIB_PAINEL_CAPAS;
        snprintf(bibTarefaNome, sizeof(bibTarefaNome), "%s", bibAchados[escolha]);
        snprintf(bibTarefaJogo, sizeof(bibTarefaJogo), "%s", bibJogo);
        bibFecharPainel();
        bibIniciarTarefa(capa ? BIB_TAREFA_BAIXAR_CAPA : BIB_TAREFA_BAIXAR_TRAPACA);
        snprintf(msgStatus, sizeof(msgStatus), "BAIXANDO...");
        msgTimer = 120;
        return true;
    }
    if (escolha == BIB_OP_INICIAR) { bibFecharPainel(); return false; }
    if (escolha == BIB_OP_CANCELAR) { bibFecharPainel(); return true; }
    bibFecharPainel();
    if (escolha == BIB_OP_FAVORITO) { bibAlternarFavorito(bibJogo); if (bibAba == 1) bibRecarregar(NULL); }
    else if (escolha == BIB_OP_TRAPACAS) {
        if (bibLerTrapacas(bibJogo) > 0) bibMostrarTrapacas(0);
        else { snprintf(bibTarefaJogo, sizeof(bibTarefaJogo), "%s", bibJogo); if (bibIniciarTarefa(BIB_TAREFA_ACHAR_TRAPACA)) { snprintf(msgStatus, sizeof(msgStatus), "BUSCANDO TRAPACAS..."); msgTimer = 180; } }
    }
    else if (escolha == BIB_OP_CONTROLE) { snprintf(msgStatus, sizeof(msgStatus), "NO JOGO: L1 + R1 + OPTIONS + TOUCHPAD > CONTROLES"); msgTimer = 300; }
    else if (escolha == BIB_OP_RENOMEAR) {
        char base[256];
        bibBase(bibJogo, base, sizeof(base));
        bibAbrirTeclado(BIB_IME_RENOMEAR, L"Novo nome do jogo", base);
    }
    else if (escolha == BIB_OP_CAPA) {
        char base[256];
        bibBase(bibJogo, base, sizeof(base));
        snprintf(bibTarefaJogo, sizeof(bibTarefaJogo), "%s", bibJogo);
        if (bibIniciarTarefa(BIB_TAREFA_ACHAR_CAPA)) { snprintf(msgStatus, sizeof(msgStatus), "BUSCANDO CAPAS..."); msgTimer = 180; }
    }
    else if (escolha == BIB_OP_PESQUISAR) bibAbrirTeclado(BIB_IME_PESQUISAR, L"Pesquisar jogo", bibBusca);
    else if (escolha == BIB_OP_LIMPAR) { bibBusca[0] = '\0'; bibRecarregar(NULL); }
    else if (escolha == BIB_OP_ORDENAR) { bibOrdem = (bibOrdem + 1) % 3; bibRecarregar(bibJogo); }
    else if (escolha == BIB_OP_FILTRAR) { bibAba = bibAba ? 0 : 1; bibRecarregar(NULL); }
    return true;
}

extern int globalPadHandle;

namespace escritorio3d {

struct Ponto { float x, y, z; };
struct Camera { float x, y, z, yaw, pitch, cy, sy, cp, sp, foco; };
struct Obstaculo { float x0, x1, z0, z1; };
struct DadosAgente { const char* nome; const char* papel; float x, z, dir; uint32_t camisa, pele, cabelo; };

static const int LARGURA = 640;
static const int ALTURA = 360;
static const int TELA_L = 1920;
static const int TELA_A = 1080;
static const int ESCALA_TELA = TELA_L / LARGURA;
static const float PROX = 0.05f;
static const float PI3 = 3.14159265358979323846f;

static uint32_t pixels[LARGURA * ALTURA];
static float profundidade[LARGURA * ALTURA];
static Obstaculo obstaculos[192];
static int totalObstaculos = 0;

struct TriRaster {
    uint32_t cor;
    float invArea;
    float A0, B0, C0;
    float A1, B1, C1;
    float A2, B2, C2;
    float p0, p1, p2;
    float minX, maxX, minY, maxY;
    const uint32_t* tex;
    int texW, texH;
    float qu0, qv0, qu1, qv1, qu2, qv2;
};
static const int MAX_TRIANGULOS = 65536;
static TriRaster listaTriangulos[MAX_TRIANGULOS];
static int totalTriangulos = 0;

static const int LINHAS_POR_FAIXA = 8;
static const int TOTAL_FAIXAS = (ALTURA + LINHAS_POR_FAIXA - 1) / LINHAS_POR_FAIXA;
static const int MAX_THREADS_RENDER = 4;

static pthread_t threadsRender[MAX_THREADS_RENDER];
static int totalThreadsRender = 0;
static volatile int renderSair = 0;
static volatile int renderGeracao = 0;
static volatile int renderConcluidas = 0;
static volatile int renderFallback = 0;
static int threadIndices[MAX_THREADS_RENDER];
static bool cullingAtivo = true;
static int triangulosDescartados = 0;
static uint32_t corFundoCima = 0xFFB9D9EE;
static uint32_t corFundoBaixo = 0xFFE8F2F8;

static const int TV_TEX_W = 960;
static const int TV_TEX_H = 256;
static uint32_t tvTextura[TV_TEX_W * TV_TEX_H];
static bool tvTexturaAtiva = false;
enum { TVMIDIA_NENHUMA = 0, TVMIDIA_IMAGEM, TVMIDIA_AUDIO };
static int tvMidia = TVMIDIA_NENHUMA;

static const DadosAgente agentes[] = {
    { "REDATOR",      "Texto, legenda, roteiro e anuncio",       -26.0f, -16.0f,  1.0f, 0xFFE8B931, 0xFFF1C7A5, 0xFF3B2416 },
    { "DESIGNER",     "Arte, criativo, post e motion",           -20.0f, -16.0f,  1.0f, 0xFFB65CF0, 0xFFA8704B, 0xFF141414 },
    { "VIDEO",        "Edicao de video",                         -14.0f, -16.0f,  1.0f, 0xFFF0577E, 0xFFD9A07A, 0xFF7A3B12 },
    { "TRAFEGO",      "Campanhas pagas, publico e metricas",      14.0f, -16.0f,  1.0f, 0xFF2FBF7A, 0xFF7A4A2E, 0xFF141414 },
    { "SOCIAL",       "Redes sociais e agendamento",              20.0f, -16.0f,  1.0f, 0xFFFF8C2E, 0xFFF5D3B8, 0xFFC9A15A },
    { "ESTRATEGIA",   "Publico, oferta e posicionamento",         26.0f, -16.0f,  1.0f, 0xFF2BB8C4, 0xFFD9A07A, 0xFF4A2A1A },
    { "REUNIAO",      "Diagnostico e proposta",                  -26.0f,  16.0f, -1.0f, 0xFF3D8EF0, 0xFFF1C7A5, 0xFF141414 },
    { "SITES",        "Site, landing page e rastreamento",       -20.0f,  16.0f, -1.0f, 0xFF7BC63A, 0xFFF5D3B8, 0xFF8C2F1C },
    { "ATENDIMENTO",  "Chatbot, agente de IA e funil",           -14.0f,  16.0f, -1.0f, 0xFFE9579F, 0xFFA8704B, 0xFF2B1A10 },
    { "OPERACAO",     "Cliente novo, processos e cobranca",       14.0f,  16.0f, -1.0f, 0xFF7D8FAA, 0xFF7A4A2E, 0xFF141414 },
    { "SISTEMAS",     "Constroi ferramentas internas",            20.0f,  16.0f, -1.0f, 0xFFE07A4F, 0xFFD9A07A, 0xFF3B2416 },
    { "TRANSCRICAO",  "Transcreve videos e reunioes",             26.0f,  16.0f, -1.0f, 0xFF9C86F5, 0xFFF1C7A5, 0xFFC9A15A },
    { "SUPERVISOR",   "Recebe pedidos e leva ao agente certo",     0.0f,  -6.1f,  1.0f, 0xFFF1521B, 0xFFD9A07A, 0xFF1A1A1A }
};
static const int TOTAL_AGENTES = (int)(sizeof(agentes) / sizeof(agentes[0]));

static Ponto ponto(float x, float y, float z) { Ponto p = { x, y, z }; return p; }

static uint32_t escurecer(uint32_t cor, float f) {
    int r = (int)((cor >> 16) & 255) * f;
    int g = (int)((cor >> 8) & 255) * f;
    int b = (int)(cor & 255) * f;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void atualizarCamera(Camera& c) {
    c.cy = cosf(c.yaw);
    c.sy = sinf(c.yaw);
    c.cp = cosf(c.pitch);
    c.sp = sinf(c.pitch);
    c.foco = (LARGURA * 0.5f) / tanf(35.0f * PI3 / 180.0f);
}

static void paraCamera(const Ponto& p, const Camera& c, Ponto& q) {
    float dx = p.x - c.x;
    float dy = p.y - c.y;
    float dz = p.z - c.z;
    float cz = dx * c.sy - dz * c.cy;
    q.x = dx * c.cy + dz * c.sy;
    q.y = dy * c.cp - cz * c.sp;
    q.z = cz * c.cp + dy * c.sp;
}

static void projetar(const Ponto& q, const Camera& c, float& sx, float& sy, float& invZ) {
    invZ = 1.0f / q.z;
    sx = LARGURA * 0.5f + q.x * c.foco * invZ;
    sy = ALTURA * 0.5f - q.y * c.foco * invZ;
}

struct VerticeTela { float x, y, invZ; };

static float borda(const VerticeTela& a, const VerticeTela& b, float px, float py) {
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

static void adicionarTrianguloTela(const VerticeTela& a, const VerticeTela& b, const VerticeTela& c, uint32_t cor, bool cull) {
    VerticeTela va = a, vb = b, vc = c;
    float area = borda(va, vb, vc.x, vc.y);
    if (cull) {
        if (area <= 0.0001f) return;
    }
    else {
        if (fabsf(area) < 0.0001f) return;
        if (area < 0.0f) {
            VerticeTela temp = vb; vb = vc; vc = temp;
            area = -area;
        }
    }
    if (totalTriangulos >= MAX_TRIANGULOS) { triangulosDescartados++; return; }

    TriRaster& t = listaTriangulos[totalTriangulos++];
    t.cor = cor;
    t.tex = NULL;
    t.invArea = 1.0f / area;

    t.A0 = -(vb.y - va.y); t.B0 = vb.x - va.x;
    t.C0 = (vb.y - va.y) * va.x - (vb.x - va.x) * va.y;
    t.A1 = -(vc.y - vb.y); t.B1 = vc.x - vb.x;
    t.C1 = (vc.y - vb.y) * vb.x - (vc.x - vb.x) * vb.y;
    t.A2 = -(va.y - vc.y); t.B2 = va.x - vc.x;
    t.C2 = (va.y - vc.y) * vc.x - (va.x - vc.x) * vc.y;

    t.p0 = vc.invZ * t.invArea;
    t.p1 = va.invZ * t.invArea;
    t.p2 = vb.invZ * t.invArea;

    t.minX = va.x < vb.x ? (va.x < vc.x ? va.x : vc.x) : (vb.x < vc.x ? vb.x : vc.x);
    t.maxX = va.x > vb.x ? (va.x > vc.x ? va.x : vc.x) : (vb.x > vc.x ? vb.x : vc.x);
    t.minY = va.y < vb.y ? (va.y < vc.y ? va.y : vc.y) : (vb.y < vc.y ? vb.y : vc.y);
    t.maxY = va.y > vb.y ? (va.y > vc.y ? va.y : vc.y) : (vb.y > vc.y ? vb.y : vc.y);
}

static void adicionarTrianguloTelaTextura(const VerticeTela& a, const VerticeTela& b, const VerticeTela& c,
                                          float uA, float vA, float uB, float vB, float uC, float vC,
                                          const uint32_t* tex, int texW, int texH) {
    VerticeTela va = a, vb = b, vc = c;
    float us[3] = { uA, uB, uC };
    float vs[3] = { vA, vB, vC };
    float area = borda(va, vb, vc.x, vc.y);
    if (fabsf(area) < 0.0001f) return;
    if (area < 0.0f) {
        VerticeTela temp = vb; vb = vc; vc = temp;
        float tu = us[1]; us[1] = us[2]; us[2] = tu;
        float tv = vs[1]; vs[1] = vs[2]; vs[2] = tv;
        area = -area;
    }
    if (totalTriangulos >= MAX_TRIANGULOS) { triangulosDescartados++; return; }

    TriRaster& t = listaTriangulos[totalTriangulos++];
    t.cor = 0xFFFFFFFF;
    t.tex = tex;
    t.texW = texW;
    t.texH = texH;
    t.invArea = 1.0f / area;

    t.A0 = -(vb.y - va.y); t.B0 = vb.x - va.x;
    t.C0 = (vb.y - va.y) * va.x - (vb.x - va.x) * va.y;
    t.A1 = -(vc.y - vb.y); t.B1 = vc.x - vb.x;
    t.C1 = (vc.y - vb.y) * vb.x - (vc.x - vb.x) * vb.y;
    t.A2 = -(va.y - vc.y); t.B2 = va.x - vc.x;
    t.C2 = (va.y - vc.y) * vc.x - (va.x - vc.x) * vc.y;

    t.p0 = vc.invZ * t.invArea;
    t.p1 = va.invZ * t.invArea;
    t.p2 = vb.invZ * t.invArea;
    t.qu0 = us[2] * vc.invZ * t.invArea;
    t.qv0 = vs[2] * vc.invZ * t.invArea;
    t.qu1 = us[0] * va.invZ * t.invArea;
    t.qv1 = vs[0] * va.invZ * t.invArea;
    t.qu2 = us[1] * vb.invZ * t.invArea;
    t.qv2 = vs[1] * vb.invZ * t.invArea;

    t.minX = va.x < vb.x ? (va.x < vc.x ? va.x : vc.x) : (vb.x < vc.x ? vb.x : vc.x);
    t.maxX = va.x > vb.x ? (va.x > vc.x ? va.x : vc.x) : (vb.x > vc.x ? vb.x : vc.x);
    t.minY = va.y < vb.y ? (va.y < vc.y ? va.y : vc.y) : (vb.y < vc.y ? vb.y : vc.y);
    t.maxY = va.y > vb.y ? (va.y > vc.y ? va.y : vc.y) : (vb.y > vc.y ? vb.y : vc.y);
}

static void desenharTrianguloCamera(const Ponto& a, const Ponto& b, const Ponto& c, uint32_t cor, const Camera& cam, bool cull) {
    const Ponto* entrada[3] = { &a, &b, &c };
    Ponto saida[4];
    int n = 0;

    for (int i = 0; i < 3; i++) {
        const Ponto& p = *entrada[i];
        const Ponto& q = *entrada[(i + 1) % 3];
        bool pHa = p.z >= PROX;
        bool qHa = q.z >= PROX;
        if (pHa) {
            if (n < 4) saida[n++] = p;
        }
        if (pHa != qHa) {
            float t = (PROX - p.z) / (q.z - p.z);
            Ponto r;
            r.x = p.x + (q.x - p.x) * t;
            r.y = p.y + (q.y - p.y) * t;
            r.z = PROX;
            if (n < 4) saida[n++] = r;
        }
    }
    if (n < 3) return;

    VerticeTela tela[4];
    for (int i = 0; i < n; i++) projetar(saida[i], cam, tela[i].x, tela[i].y, tela[i].invZ);
    for (int i = 1; i < n - 1; i++) adicionarTrianguloTela(tela[0], tela[i], tela[i + 1], cor, cull);
}

static void adicionarTri(const Ponto& a, const Ponto& b, const Ponto& c, uint32_t cor, const Camera& cam, bool cull = true) {
    float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float comprimento = sqrtf(nx * nx + ny * ny + nz * nz);
    float luz = 0.78f;
    if (comprimento > 0.0001f) {
        nx /= comprimento; ny /= comprimento; nz /= comprimento;
        float d = fabsf(nx * 0.28f - ny * 0.86f - nz * 0.42f);
        luz = 0.58f + 0.42f * d;
    }

    Ponto ca, cb, cc;
    paraCamera(a, cam, ca);
    paraCamera(b, cam, cb);
    paraCamera(c, cam, cc);
    desenharTrianguloCamera(ca, cb, cc, escurecer(cor, luz), cam, cull && cullingAtivo);
}

static inline void rasterizarTriangulo(const TriRaster& t, int inicio, int fim) {
    int x0 = (int)floorf(t.minX);
    int x1 = (int)ceilf(t.maxX);
    int y0 = (int)floorf(t.minY);
    int y1 = (int)ceilf(t.maxY);
    if (x0 < 0) x0 = 0;
    if (y0 < inicio) y0 = inicio;
    if (x1 >= LARGURA) x1 = LARGURA - 1;
    if (y1 >= fim) y1 = fim - 1;
    if (x0 > x1 || y0 > y1) return;

    float passoZ = t.A0 * t.p0 + t.A1 * t.p1 + t.A2 * t.p2;
    float pxInicial = x0 + 0.5f;
    bool temTex = t.tex != NULL;
    float passoQu = 0.0f, passoQv = 0.0f;
    if (temTex) {
        passoQu = t.A0 * t.qu0 + t.A1 * t.qu1 + t.A2 * t.qu2;
        passoQv = t.A0 * t.qv0 + t.A1 * t.qv1 + t.A2 * t.qv2;
    }

    for (int y = y0; y <= y1; y++) {
        float py = y + 0.5f;
        float w0 = t.A0 * pxInicial + t.B0 * py + t.C0;
        float w1 = t.A1 * pxInicial + t.B1 * py + t.C1;
        float w2 = t.A2 * pxInicial + t.B2 * py + t.C2;
        float z = w0 * t.p0 + w1 * t.p1 + w2 * t.p2;
        float qu = 0.0f, qv = 0.0f;
        if (temTex) {
            qu = w0 * t.qu0 + w1 * t.qu1 + w2 * t.qu2;
            qv = w0 * t.qv0 + w1 * t.qv1 + w2 * t.qv2;
        }
        uint32_t* linha = pixels + y * LARGURA;
        float* linhaZ = profundidade + y * LARGURA;
        for (int x = x0; x <= x1; x++) {
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f && z > linhaZ[x]) {
                if (temTex) {
                    float invZ = 1.0f / z;
                    int u = (int)((qu * invZ) * (float)t.texW);
                    int v = (int)((qv * invZ) * (float)t.texH);
                    if (u < 0) u = 0; if (u >= t.texW) u = t.texW - 1;
                    if (v < 0) v = 0; if (v >= t.texH) v = t.texH - 1;
                    linha[x] = t.tex[v * t.texW + u];
                }
                else linha[x] = t.cor;
                linhaZ[x] = z;
            }
            w0 += t.A0; w1 += t.A1; w2 += t.A2; z += passoZ;
            if (temTex) { qu += passoQu; qv += passoQv; }
        }
    }
}

static void processarFaixasRender(int indice, int total) {
    if (total < 1) total = 1;
    if (indice < 0 || indice >= total) return;
    for (int faixa = indice; faixa < TOTAL_FAIXAS; faixa += total) {
        int inicio = faixa * LINHAS_POR_FAIXA;
        int fim = inicio + LINHAS_POR_FAIXA;
        if (fim > ALTURA) fim = ALTURA;
        for (int y = inicio; y < fim; y++) {
            uint32_t cor = y < ALTURA / 2 ? corFundoCima : corFundoBaixo;
            uint32_t* linha = pixels + y * LARGURA;
            float* linhaZ = profundidade + y * LARGURA;
            for (int x = 0; x < LARGURA; x++) { linha[x] = cor; linhaZ[x] = 0.0f; }
        }
        for (int i = 0; i < totalTriangulos; i++) {
            const TriRaster& t = listaTriangulos[i];
            if (t.maxY < (float)inicio || t.minY > (float)(fim - 1)) continue;
            rasterizarTriangulo(t, inicio, fim);
        }
    }
}

static void* threadRender(void* arg) {
    scePthreadSetprio(scePthreadSelf(), 380);
    int indice = *(int*)arg + 1;
    int ultima = 0;
    for (;;) {
        while (__atomic_load_n(&renderGeracao, __ATOMIC_ACQUIRE) == ultima) {
            if (__atomic_load_n(&renderSair, __ATOMIC_ACQUIRE)) return NULL;
            sceKernelUsleep(200);
        }
        ultima = __atomic_load_n(&renderGeracao, __ATOMIC_ACQUIRE);
        processarFaixasRender(indice, totalThreadsRender + 1);
        __atomic_fetch_add(&renderConcluidas, 1, __ATOMIC_SEQ_CST);
    }
}

static void iniciarThreadsRender() {
    renderSair = 0; renderGeracao = 0; renderConcluidas = 0; renderFallback = 0;
    totalThreadsRender = 0;
    for (int i = 0; i < MAX_THREADS_RENDER; i++) {
        pthread_t id;
        pthread_attr_t atributos;
        pthread_attr_init(&atributos);
        pthread_attr_setstacksize(&atributos, 512 * 1024);
        int slot = totalThreadsRender;
        threadIndices[slot] = slot;
        if (pthread_create(&id, &atributos, threadRender, &threadIndices[slot]) == 0) threadsRender[totalThreadsRender++] = id;
        pthread_attr_destroy(&atributos);
    }
}

static void pararThreadsRender() {
    __atomic_store_n(&renderSair, 1, __ATOMIC_SEQ_CST);
    for (int i = 0; i < totalThreadsRender; i++) pthread_detach(threadsRender[i]);
    totalThreadsRender = 0;
}

static void desenharQuadroRender() {
    __atomic_store_n(&renderConcluidas, 0, __ATOMIC_SEQ_CST);
    if (renderFallback || totalThreadsRender == 0) {
        processarFaixasRender(0, 1);
        return;
    }
    __atomic_fetch_add(&renderGeracao, 1, __ATOMIC_SEQ_CST);
    processarFaixasRender(0, totalThreadsRender + 1);
    int espera = 0;
    while (__atomic_load_n(&renderConcluidas, __ATOMIC_ACQUIRE) < totalThreadsRender && espera < 30) {
        sceKernelUsleep(1000);
        espera++;
    }
    if (__atomic_load_n(&renderConcluidas, __ATOMIC_ACQUIRE) < totalThreadsRender) {
        renderFallback = 1;
        __atomic_store_n(&renderSair, 1, __ATOMIC_SEQ_CST);
        processarFaixasRender(0, 1);
    }
}

static void adicionarQuad(const Ponto& a, const Ponto& b, const Ponto& c, const Ponto& d, uint32_t cor, const Camera& cam, bool cull = true) {
    adicionarTri(a, b, c, cor, cam, cull);
    adicionarTri(a, c, d, cor, cam, cull);
}

static void adicionarCaixa(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t cor, const Camera& cam) {
    Ponto a = ponto(x0, y0, z0), b = ponto(x1, y0, z0), c = ponto(x1, y0, z1), d = ponto(x0, y0, z1);
    Ponto e = ponto(x0, y1, z0), f = ponto(x1, y1, z0), g = ponto(x1, y1, z1), h = ponto(x0, y1, z1);
    adicionarQuad(a, b, f, e, cor, cam);
    adicionarQuad(b, c, g, f, cor, cam);
    adicionarQuad(c, d, h, g, cor, cam);
    adicionarQuad(d, a, e, h, cor, cam);
    adicionarQuad(e, f, g, h, cor, cam);
    adicionarQuad(a, d, c, b, cor, cam);
}

static void adicionarObstaculo(float x0, float x1, float z0, float z1) {
    if (totalObstaculos >= (int)(sizeof(obstaculos) / sizeof(obstaculos[0]))) return;
    Obstaculo o = { x0, x1, z0, z1 };
    obstaculos[totalObstaculos++] = o;
}

static void adicionarTelaTV(const Camera& cam) {
    if (!tvTexturaAtiva) return;
    const float z = -19.74f;
    Ponto a = ponto(1.46f, 1.26f, z);
    Ponto b = ponto(6.14f, 1.26f, z);
    Ponto c = ponto(6.14f, 2.49f, z);
    Ponto d = ponto(1.46f, 2.49f, z);
    Ponto qa, qb, qc, qd;
    paraCamera(a, cam, qa);
    paraCamera(b, cam, qb);
    paraCamera(c, cam, qc);
    paraCamera(d, cam, qd);
    if (qa.z < PROX + 0.02f || qb.z < PROX + 0.02f || qc.z < PROX + 0.02f || qd.z < PROX + 0.02f) return;
    float ax, ay, ai, bx, by, bi, cx, cy, ci, dx, dy, di;
    projetar(qa, cam, ax, ay, ai);
    projetar(qb, cam, bx, by, bi);
    projetar(qc, cam, cx, cy, ci);
    projetar(qd, cam, dx, dy, di);
    VerticeTela va = { ax, ay, ai };
    VerticeTela vb = { bx, by, bi };
    VerticeTela vc = { cx, cy, ci };
    VerticeTela vd = { dx, dy, di };
    adicionarTrianguloTelaTextura(va, vb, vc, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, tvTextura, TV_TEX_W, TV_TEX_H);
    adicionarTrianguloTelaTextura(va, vc, vd, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, tvTextura, TV_TEX_W, TV_TEX_H);
}

static void adicionarPiso(float x0, float x1, float z0, float z1, uint32_t corA, uint32_t corB, float y, const Camera& cam, bool paraBaixo = false) {
    const float passo = 5.0f;
    int ix = 0;
    for (float x = x0; x < x1 - 0.001f; x += passo, ix++) {
        float xf = x + passo;
        if (xf > x1) xf = x1;
        int iz = 0;
        for (float z = z0; z < z1 - 0.001f; z += passo, iz++) {
            float zf = z + passo;
            if (zf > z1) zf = z1;
            uint32_t cor = ((ix + iz) & 1) ? corB : corA;
            if (paraBaixo) {
                adicionarQuad(ponto(x, y, z), ponto(x, y, zf), ponto(xf, y, zf), ponto(xf, y, z), cor, cam);
            }
            else {
                adicionarQuad(ponto(x, y, z), ponto(xf, y, z), ponto(xf, y, zf), ponto(x, y, zf), cor, cam);
            }
        }
    }
}

static void paredeZ(float x0, float x1, float z, uint32_t cor, const Camera& cam) {
    adicionarCaixa(x0, 0.0f, z - 0.09f, x1, 3.0f, z + 0.09f, cor, cam);
    adicionarObstaculo(x0 - 0.08f, x1 + 0.08f, z - 0.12f, z + 0.12f);
}

static void paredeX(float x, float z0, float z1, uint32_t cor, const Camera& cam) {
    adicionarCaixa(x - 0.09f, 0.0f, z0, x + 0.09f, 3.0f, z1, cor, cam);
    adicionarObstaculo(x - 0.12f, x + 0.12f, z0 - 0.08f, z1 + 0.08f);
}

static void esfera(float cx, float cy, float cz, float raio, uint32_t cor, const Camera& cam, int fatias = 8, int aneis = 4) {
    if (fatias < 4) fatias = 4;
    if (aneis < 3) aneis = 3;
    if (fatias > 12) fatias = 12;
    if (aneis > 6) aneis = 6;
    float cosV[7], sinV[7], cosU[13], sinU[13];
    for (int i = 0; i <= aneis; i++) {
        float v = PI3 * i / aneis - PI3 * 0.5f;
        cosV[i] = cosf(v);
        sinV[i] = sinf(v);
    }
    for (int j = 0; j <= fatias; j++) {
        float u = 2.0f * PI3 * j / fatias;
        cosU[j] = cosf(u);
        sinU[j] = sinf(u);
    }
    for (int i = 0; i < aneis; i++) {
        for (int j = 0; j < fatias; j++) {
            Ponto a = ponto(cx + raio * cosV[i] * cosU[j], cy + raio * sinV[i], cz + raio * cosV[i] * sinU[j]);
            Ponto b = ponto(cx + raio * cosV[i + 1] * cosU[j], cy + raio * sinV[i + 1], cz + raio * cosV[i + 1] * sinU[j]);
            Ponto c = ponto(cx + raio * cosV[i + 1] * cosU[j + 1], cy + raio * sinV[i + 1], cz + raio * cosV[i + 1] * sinU[j + 1]);
            Ponto d = ponto(cx + raio * cosV[i] * cosU[j + 1], cy + raio * sinV[i], cz + raio * cosV[i] * sinU[j + 1]);
            adicionarTri(a, b, c, cor, cam, false);
            adicionarTri(a, c, d, cor, cam, false);
        }
    }
}

static void cadeira(float x, float z, float dir, uint32_t cor, const Camera& cam) {
    adicionarCaixa(x - 0.24f, 0.42f, z - 0.24f, x + 0.24f, 0.50f, z + 0.24f, cor, cam);
    float bz = z - 0.24f * dir;
    adicionarCaixa(x - 0.24f, 0.50f, bz - 0.06f, x + 0.24f, 1.10f, bz + 0.06f, cor, cam);
    adicionarCaixa(x - 0.05f, 0.10f, z - 0.05f, x + 0.05f, 0.42f, z + 0.05f, 0xFF3A3F47, cam);
    adicionarCaixa(x - 0.20f, 0.03f, z - 0.20f, x + 0.20f, 0.10f, z - 0.14f, 0xFF3A3F47, cam);
    adicionarCaixa(x - 0.20f, 0.03f, z + 0.14f, x + 0.20f, 0.10f, z + 0.20f, 0xFF3A3F47, cam);
}

static void mesa(float x, float z, float dir, uint32_t cor, const Camera& cam) {
    adicionarCaixa(x - 1.65f, 0.72f, z - 0.72f, x + 1.65f, 0.80f, z + 0.72f, cor, cam);
    adicionarCaixa(x - 1.50f, 0.0f, z - 0.60f, x - 1.38f, 0.72f, z - 0.48f, 0xFF8A8F99, cam);
    adicionarCaixa(x + 1.38f, 0.0f, z - 0.60f, x + 1.50f, 0.72f, z - 0.48f, 0xFF8A8F99, cam);
    adicionarCaixa(x - 1.50f, 0.0f, z + 0.48f, x - 1.38f, 0.72f, z + 0.60f, 0xFF8A8F99, cam);
    adicionarCaixa(x + 1.38f, 0.0f, z + 0.48f, x + 1.50f, 0.72f, z + 0.60f, 0xFF8A8F99, cam);

    float mz = z + 0.20f * dir;
    adicionarCaixa(x - 0.32f, 0.80f, mz - 0.08f, x + 0.32f, 1.18f, mz + 0.08f, 0xFF1B2430, cam);
    adicionarCaixa(x - 0.27f, 0.85f, mz - 0.01f, x + 0.27f, 1.13f, mz + 0.02f, 0xFF2E6DB4, cam);
    adicionarCaixa(x - 0.05f, 0.80f, mz - 0.05f, x + 0.05f, 0.82f, mz + 0.05f, 0xFF2A3340, cam);

    float zk = z - 0.35f * dir;
    adicionarCaixa(x - 0.45f, 0.80f, zk - 0.10f, x + 0.45f, 0.83f, zk + 0.10f, 0xFF2B3242, cam);
    adicionarCaixa(x - 0.42f, 0.83f, zk - 0.07f, x + 0.42f, 0.84f, zk + 0.07f, 0xFF3D4757, cam);
    adicionarCaixa(x + 0.55f, 0.80f, zk - 0.06f, x + 0.67f, 0.84f, zk + 0.06f, 0xFF2B3242, cam);
    adicionarCaixa(x + 0.92f, 0.80f, zk - 0.07f, x + 1.06f, 0.99f, zk + 0.07f, 0xFFF4F1EC, cam);
    adicionarCaixa(x + 0.95f, 0.955f, zk - 0.045f, x + 1.03f, 0.975f, zk + 0.045f, 0xFF6B3A20, cam);
    adicionarCaixa(x - 1.22f, 0.80f, z - 0.16f, x - 0.86f, 0.82f, z + 0.16f, 0xFFF4F1EC, cam);
    adicionarCaixa(x - 1.18f, 0.82f, z - 0.12f, x - 0.90f, 0.83f, z + 0.12f, 0xFFE5E0D6, cam);
    adicionarObstaculo(x - 1.75f, x + 1.75f, z - 0.82f, z + 0.82f);
}

static void planta(float x, float z, float escala, const Camera& cam) {
    adicionarCaixa(x - 0.22f * escala, 0.0f, z - 0.22f * escala, x + 0.22f * escala, 0.42f * escala, z + 0.22f * escala, 0xFFB9844F, cam);
    adicionarCaixa(x - 0.10f * escala, 0.42f * escala, z - 0.10f * escala, x + 0.10f * escala, 1.05f * escala, z + 0.10f * escala, 0xFF2F6B3C, cam);
    adicionarCaixa(x - 0.34f * escala, 1.00f * escala, z - 0.34f * escala, x + 0.34f * escala, 1.32f * escala, z + 0.34f * escala, 0xFF3D8A4C, cam);
}

// ---------------------------------------------------------------------------
// Decoracao do escritorio: plantas, mesas, bebedouro, sofas e tapetes
// ---------------------------------------------------------------------------
enum { DECOR_PLANTA = 0, DECOR_MESA, DECOR_BEBEDOURO, DECOR_SOFA, DECOR_TAPETE, DECOR_TIPOS,
       EDIT_CADEIRA = DECOR_TIPOS, EDIT_TV, EDIT_COMPUTADOR, EDIT_TECLADO, EDIT_PIANO,
       EDIT_VIOLAO, EDIT_VIOLINO, EDIT_BATERIA, EDIT_PAREDE, EDIT_CHAO, EDIT_TETO, EDIT_JANELA,
       OBJ_TIPOS };
static const char* OBJ_NOMES[OBJ_TIPOS] = {
    "PLANTA", "MESA", "BEBEDOURO", "SOFA", "TAPETE",
    "CADEIRA", "TV", "COMPUTADOR", "TECLADO", "PIANO",
    "VIOLAO", "VIOLINO", "BATERIA", "PAREDE", "CHAO", "TETO", "JANELA"
};
static const uint32_t DECOR_PALETA[8] = {
    0xFF8A5A32, 0xFFB23A48, 0xFF3B6EA5, 0xFF2F7D4F,
    0xFFE8DCC0, 0xFF3A3F47, 0xFF7A4FA0, 0xFFE07A2E
};
static const int DECOR_MAX = 80;

// --- pintura do escritorio (0 = cor padrao do projeto) ---
static uint32_t corParedeEdit = 0;
static uint32_t corChaoEdit = 0;
static uint32_t corTetoEdit = 0;
static const uint32_t CORES_PAREDE[8] = {
    0u, 0xFFA9D8EE, 0xFFF4F1EC, 0xFFE8D6B0, 0xFFC8E6C9, 0xFFF8C8DC, 0xFF3A3F47, 0xFFD4AF37
};
static const uint32_t CORES_CHAO[8] = {
    0u, 0xFF8A5A32, 0xFFBFC4CC, 0xFFE8D6A8, 0xFF2E6DB4, 0xFF2F7D4F, 0xFFB23A48, 0xFF3A3F47
};
static const uint32_t CORES_TETO[6] = {
    0u, 0xFFFFFFFF, 0xFFDCE6F3, 0xFFE8D6B0, 0xFFC8E6C9, 0xFF3A3F47
};

static int paletaAchar(const uint32_t* pal, int n, uint32_t cor) {
    for (int i = 0; i < n; i++) if (pal[i] == cor) return i;
    return -1;
}

static uint32_t paletaCiclar(const uint32_t* pal, int n, uint32_t cor, int dir) {
    int i = paletaAchar(pal, n, cor);
    i = (i < 0) ? 0 : (i + dir + n) % n;
    return pal[i];
}

// Cicla apenas cores reais (ignora o indice 0 = "padrao" das paletas estruturais).
static uint32_t paletaCiclarCor(const uint32_t* pal, int n, int inicio, uint32_t cor, int dir) {
    int i = paletaAchar(pal, n, cor);
    if (i < inicio) i = inicio;
    for (int k = 0; k < n; k++) {
        i += dir;
        if (i >= n) i = inicio;
        if (i < inicio) i = n - 1;
        if (pal[i] != 0 || inicio == 0) break;
    }
    return pal[i];
}
static const char DECOR_ARQUIVO[] = "/data/HyperNeiva/configuracao/decoracao.txt";
struct DecorItem { int tipo; float x, z, rot; uint32_t cor; };
static DecorItem decorItens[DECOR_MAX];
static int decorTotal = 0;
static int decorEditando = -1;
static bool decorEditNovo = false;
static DecorItem decorAntes;

static uint32_t decorCorPadrao(int tipo) {
    if (tipo == DECOR_PLANTA) return 0xFFB9844F;
    if (tipo == DECOR_MESA) return DECOR_PALETA[0];
    if (tipo == DECOR_BEBEDOURO) return 0xFFE8E8E8;
    if (tipo == DECOR_SOFA) return DECOR_PALETA[2];
    if (tipo == DECOR_TAPETE) return DECOR_PALETA[1];
    if (tipo == EDIT_CADEIRA) return 0xFF2C3A4E;
    if (tipo == EDIT_TV) return 0xFF14191F;
    if (tipo == EDIT_COMPUTADOR) return 0xFF5A3E2B;
    if (tipo == EDIT_TECLADO) return 0xFF1B2430;
    if (tipo == EDIT_PIANO) return 0xFF2A1D12;
    if (tipo == EDIT_VIOLAO) return 0xFF8A5A32;
    if (tipo == EDIT_VIOLINO) return 0xFF7A3B1E;
    if (tipo == EDIT_BATERIA) return 0xFFB23A48;
    if (tipo == EDIT_PAREDE) return 0xFFF4F1EC;
    if (tipo == EDIT_CHAO) return 0xFF8A5A32;
    if (tipo == EDIT_TETO) return 0xFFDCE6F3;
    if (tipo == EDIT_JANELA) return 0xFF38424F;
    return 0xFFFFFFFF;
}

static Ponto pontoRot(float cx, float cz, float rot, float lx, float ly, float lz) {
    float co = cosf(rot), si = sinf(rot);
    return ponto(cx + lx * co - lz * si, ly, cz + lx * si + lz * co);
}

static void adicionarCaixaRot(float cx, float cz, float rot, float x0, float y0, float z0, float x1, float y1, float z1, uint32_t cor, const Camera& cam) {
    Ponto a = pontoRot(cx, cz, rot, x0, y0, z0), b = pontoRot(cx, cz, rot, x1, y0, z0);
    Ponto c = pontoRot(cx, cz, rot, x1, y0, z1), d = pontoRot(cx, cz, rot, x0, y0, z1);
    Ponto e = pontoRot(cx, cz, rot, x0, y1, z0), f = pontoRot(cx, cz, rot, x1, y1, z0);
    Ponto g = pontoRot(cx, cz, rot, x1, y1, z1), h = pontoRot(cx, cz, rot, x0, y1, z1);
    adicionarQuad(a, b, f, e, cor, cam);
    adicionarQuad(b, c, g, f, cor, cam);
    adicionarQuad(c, d, h, g, cor, cam);
    adicionarQuad(d, a, e, h, cor, cam);
    adicionarQuad(e, f, g, h, cor, cam);
    adicionarQuad(a, d, c, b, cor, cam);
}

static void decorDesenharItem(const DecorItem& it, const Camera& cam) {
    float x = it.x, z = it.z, r = it.rot;
    if (it.tipo == DECOR_PLANTA) {
        adicionarCaixaRot(x, z, r, -0.22f, 0.0f, -0.22f, 0.22f, 0.42f, 0.22f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.09f, 0.42f, -0.09f, 0.09f, 1.05f, 0.09f, 0xFF2F6B3C, cam);
        adicionarCaixaRot(x, z, r, -0.34f, 1.00f, -0.34f, 0.34f, 1.32f, 0.34f, 0xFF3D8A4C, cam);
    } else if (it.tipo == DECOR_MESA) {
        uint32_t pe = escurecer(it.cor, 0.62f);
        adicionarCaixaRot(x, z, r, -0.80f, 0.40f, -0.45f, 0.80f, 0.50f, 0.45f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.72f, 0.0f, -0.37f, -0.64f, 0.40f, -0.29f, pe, cam);
        adicionarCaixaRot(x, z, r, 0.64f, 0.0f, -0.37f, 0.72f, 0.40f, -0.29f, pe, cam);
        adicionarCaixaRot(x, z, r, -0.72f, 0.0f, 0.29f, -0.64f, 0.40f, 0.37f, pe, cam);
        adicionarCaixaRot(x, z, r, 0.64f, 0.0f, 0.29f, 0.72f, 0.40f, 0.37f, pe, cam);
    } else if (it.tipo == DECOR_BEBEDOURO) {
        adicionarCaixaRot(x, z, r, -0.24f, 0.0f, -0.24f, 0.24f, 1.02f, 0.24f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.18f, 0.30f, -0.27f, 0.18f, 0.46f, 0.27f, 0xFF2E6DB4, cam);
        adicionarCaixaRot(x, z, r, -0.15f, 1.02f, -0.15f, 0.15f, 1.48f, 0.15f, 0xFF8ED4FF, cam);
        adicionarCaixaRot(x, z, r, -0.06f, 1.48f, -0.06f, 0.06f, 1.58f, 0.06f, 0xFFE6F6FF, cam);
    } else if (it.tipo == DECOR_SOFA) {
        uint32_t claro = escurecer(it.cor, 1.16f);
        uint32_t escuro = escurecer(it.cor, 0.72f);
        adicionarCaixaRot(x, z, r, -0.95f, 0.14f, -0.42f, 0.95f, 0.45f, 0.42f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.95f, 0.45f, 0.26f, 0.95f, 0.92f, 0.44f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.95f, 0.45f, -0.42f, -0.74f, 0.70f, 0.44f, claro, cam);
        adicionarCaixaRot(x, z, r, 0.74f, 0.45f, -0.42f, 0.95f, 0.70f, 0.44f, claro, cam);
        adicionarCaixaRot(x, z, r, -0.88f, 0.0f, -0.36f, -0.80f, 0.14f, -0.28f, 0xFF2A1D12, cam);
        adicionarCaixaRot(x, z, r, 0.80f, 0.0f, -0.36f, 0.88f, 0.14f, -0.28f, 0xFF2A1D12, cam);
        adicionarCaixaRot(x, z, r, -0.88f, 0.0f, 0.28f, -0.80f, 0.14f, 0.36f, 0xFF2A1D12, cam);
        adicionarCaixaRot(x, z, r, 0.80f, 0.0f, 0.28f, 0.88f, 0.14f, 0.36f, 0xFF2A1D12, cam);
    } else if (it.tipo == DECOR_TAPETE) {
        adicionarCaixaRot(x, z, r, -1.20f, 0.015f, -0.80f, 1.20f, 0.030f, 0.80f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -1.00f, 0.030f, -0.62f, 1.00f, 0.040f, 0.62f, escurecer(it.cor, 1.30f), cam);
    } else if (it.tipo == EDIT_CADEIRA) {
        uint32_t pe = escurecer(it.cor, 0.55f);
        adicionarCaixaRot(x, z, r, -0.24f, 0.42f, -0.24f, 0.24f, 0.50f, 0.24f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.24f, 0.50f, -0.27f, 0.24f, 1.05f, -0.19f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.05f, 0.10f, -0.05f, 0.05f, 0.42f, 0.05f, pe, cam);
        adicionarCaixaRot(x, z, r, -0.20f, 0.03f, -0.20f, 0.20f, 0.10f, -0.14f, pe, cam);
        adicionarCaixaRot(x, z, r, -0.20f, 0.03f, 0.14f, 0.20f, 0.10f, 0.20f, pe, cam);
    } else if (it.tipo == EDIT_TV) {
        uint32_t escuro = escurecer(it.cor, 0.75f);
        adicionarCaixaRot(x, z, r, -0.55f, 0.0f, -0.18f, 0.55f, 0.35f, 0.18f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.06f, 0.35f, -0.04f, 0.06f, 0.57f, 0.04f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.70f, 0.55f, -0.05f, 0.70f, 1.35f, 0.05f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.64f, 0.60f, 0.04f, 0.64f, 1.30f, 0.06f, 0xFF2E6DB4, cam);
    } else if (it.tipo == EDIT_COMPUTADOR) {
        uint32_t pe = escurecer(it.cor, 0.6f);
        adicionarCaixaRot(x, z, r, -0.60f, 0.72f, -0.30f, 0.60f, 0.78f, 0.30f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.56f, 0.0f, -0.26f, -0.50f, 0.72f, -0.20f, pe, cam);
        adicionarCaixaRot(x, z, r, 0.50f, 0.0f, -0.26f, 0.56f, 0.72f, -0.20f, pe, cam);
        adicionarCaixaRot(x, z, r, -0.56f, 0.0f, 0.20f, -0.50f, 0.72f, 0.26f, pe, cam);
        adicionarCaixaRot(x, z, r, 0.50f, 0.0f, 0.20f, 0.56f, 0.72f, 0.26f, pe, cam);
        adicionarCaixaRot(x, z, r, -0.12f, 0.78f, -0.06f, 0.12f, 0.82f, 0.06f, 0xFF20242A, cam);
        adicionarCaixaRot(x, z, r, -0.04f, 0.82f, -0.04f, 0.04f, 0.95f, 0.04f, 0xFF20242A, cam);
        adicionarCaixaRot(x, z, r, -0.28f, 0.95f, -0.04f, 0.28f, 1.35f, 0.04f, 0xFF20242A, cam);
        adicionarCaixaRot(x, z, r, -0.25f, 0.98f, 0.03f, 0.25f, 1.32f, 0.05f, 0xFF2E6DB4, cam);
        adicionarCaixaRot(x, z, r, -0.22f, 0.78f, 0.10f, 0.22f, 0.80f, 0.22f, 0xFF2A3340, cam);
    } else if (it.tipo == EDIT_TECLADO) {
        uint32_t escuro = escurecer(it.cor, 0.6f);
        adicionarCaixaRot(x, z, r, -0.36f, 0.0f, -0.05f, -0.30f, 0.70f, 0.05f, escuro, cam);
        adicionarCaixaRot(x, z, r, 0.30f, 0.0f, -0.05f, 0.36f, 0.70f, 0.05f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.36f, 0.30f, -0.03f, 0.36f, 0.36f, 0.03f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.65f, 0.70f, -0.16f, 0.65f, 0.78f, 0.16f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.60f, 0.78f, -0.12f, 0.60f, 0.80f, 0.10f, 0xFFF4F1EC, cam);
        for (int k = 0; k < 8; k++) {
            float kx = -0.52f + k * 0.14f;
            adicionarCaixaRot(x, z, r, kx, 0.80f, -0.08f, kx + 0.06f, 0.81f, 0.02f, 0xFF171A1F, cam);
        }
    } else if (it.tipo == EDIT_PIANO) {
        uint32_t claro = escurecer(it.cor, 1.5f);
        adicionarCaixaRot(x, z, r, -0.75f, 0.0f, -0.32f, 0.75f, 1.18f, 0.30f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.78f, 1.18f, -0.35f, 0.78f, 1.24f, 0.33f, claro, cam);
        adicionarCaixaRot(x, z, r, -0.70f, 0.70f, 0.30f, 0.70f, 0.76f, 0.50f, claro, cam);
        adicionarCaixaRot(x, z, r, -0.66f, 0.76f, 0.33f, 0.66f, 0.78f, 0.47f, 0xFFF4F1EC, cam);
        for (int k = 0; k < 9; k++) {
            float kx = -0.58f + k * 0.145f;
            adicionarCaixaRot(x, z, r, kx, 0.78f, 0.35f, kx + 0.06f, 0.79f, 0.44f, 0xFF171A1F, cam);
        }
        adicionarCaixaRot(x, z, r, -0.66f, 0.0f, 0.42f, -0.58f, 0.70f, 0.50f, it.cor, cam);
        adicionarCaixaRot(x, z, r, 0.58f, 0.0f, 0.42f, 0.66f, 0.70f, 0.50f, it.cor, cam);
    } else if (it.tipo == EDIT_VIOLAO) {
        uint32_t escuro = escurecer(it.cor, 0.6f);
        adicionarCaixaRot(x, z, r, -0.16f, 0.0f, -0.16f, 0.16f, 0.06f, 0.16f, 0xFF3A3F47, cam);
        adicionarCaixaRot(x, z, r, -0.04f, 0.06f, -0.04f, 0.04f, 0.45f, 0.04f, 0xFF3A3F47, cam);
        adicionarCaixaRot(x, z, r, -0.20f, 0.45f, -0.07f, 0.20f, 1.00f, 0.07f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.09f, 0.62f, 0.07f, 0.09f, 0.80f, 0.08f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.05f, 1.00f, -0.03f, 0.05f, 1.72f, 0.03f, 0xFF3A2A1C, cam);
        adicionarCaixaRot(x, z, r, -0.08f, 1.72f, -0.04f, 0.08f, 1.90f, 0.04f, escuro, cam);
    } else if (it.tipo == EDIT_VIOLINO) {
        uint32_t escuro = escurecer(it.cor, 0.6f);
        adicionarCaixaRot(x, z, r, -0.14f, 0.0f, -0.14f, 0.14f, 0.06f, 0.14f, 0xFF3A3F47, cam);
        adicionarCaixaRot(x, z, r, -0.03f, 0.06f, -0.03f, 0.03f, 0.90f, 0.03f, 0xFF3A3F47, cam);
        adicionarCaixaRot(x, z, r, -0.14f, 0.90f, -0.05f, 0.14f, 1.24f, 0.05f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.06f, 1.00f, 0.05f, 0.06f, 1.14f, 0.06f, escuro, cam);
        adicionarCaixaRot(x, z, r, -0.03f, 1.24f, -0.02f, 0.03f, 1.58f, 0.02f, 0xFF3A2A1C, cam);
        adicionarCaixaRot(x, z, r, -0.05f, 1.58f, -0.03f, 0.05f, 1.70f, 0.03f, escuro, cam);
    } else if (it.tipo == EDIT_BATERIA) {
        uint32_t pele = 0xFFE8DCC0;
        adicionarCaixaRot(x, z, r, -0.40f, 0.10f, -0.22f, 0.40f, 0.70f, 0.22f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.30f, 0.20f, 0.22f, 0.30f, 0.60f, 0.25f, pele, cam);
        adicionarCaixaRot(x, z, r, -0.34f, 0.75f, -0.14f, -0.10f, 0.95f, 0.12f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.30f, 0.95f, -0.11f, -0.14f, 0.97f, 0.09f, pele, cam);
        adicionarCaixaRot(x, z, r, 0.10f, 0.70f, -0.14f, 0.34f, 0.88f, 0.12f, it.cor, cam);
        adicionarCaixaRot(x, z, r, 0.14f, 0.88f, -0.11f, 0.30f, 0.90f, 0.09f, pele, cam);
        adicionarCaixaRot(x, z, r, -0.02f, 0.50f, 0.30f, 0.16f, 0.62f, 0.42f, 0xFFF4F1EC, cam);
        adicionarCaixaRot(x, z, r, -0.68f, 0.0f, -0.03f, -0.62f, 1.10f, 0.03f, 0xFF3A3F47, cam);
        adicionarCaixaRot(x, z, r, -0.82f, 1.08f, -0.16f, -0.48f, 1.11f, 0.16f, 0xFFD4AF37, cam);
        adicionarCaixaRot(x, z, r, 0.62f, 0.0f, -0.03f, 0.68f, 1.02f, 0.03f, 0xFF3A3F47, cam);
        adicionarCaixaRot(x, z, r, 0.48f, 1.00f, -0.16f, 0.82f, 1.03f, 0.16f, 0xFFD4AF37, cam);
    } else if (it.tipo == EDIT_PAREDE) {
        uint32_t borda = escurecer(it.cor, 0.7f);
        adicionarCaixaRot(x, z, r, -1.25f, 0.0f, -0.06f, 1.25f, 3.0f, 0.06f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 0.0f, -0.09f, 1.25f, 0.12f, 0.09f, borda, cam);
    } else if (it.tipo == EDIT_CHAO) {
        uint32_t borda = escurecer(it.cor, 0.55f);
        adicionarCaixaRot(x, z, r, -1.25f, 0.02f, -1.25f, 1.25f, 0.12f, 1.25f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 0.12f, -1.25f, 1.25f, 0.15f, -1.15f, borda, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 0.12f, 1.15f, 1.25f, 0.15f, 1.25f, borda, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 0.12f, -1.15f, -1.15f, 0.15f, 1.15f, borda, cam);
        adicionarCaixaRot(x, z, r, 1.15f, 0.12f, -1.15f, 1.25f, 0.15f, 1.15f, borda, cam);
    } else if (it.tipo == EDIT_TETO) {
        uint32_t borda = escurecer(it.cor, 0.55f);
        adicionarCaixaRot(x, z, r, -1.25f, 2.82f, -1.25f, 1.25f, 2.94f, 1.25f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 2.78f, -1.25f, 1.25f, 2.82f, -1.15f, borda, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 2.78f, 1.15f, 1.25f, 2.82f, 1.25f, borda, cam);
        adicionarCaixaRot(x, z, r, -1.25f, 2.78f, -1.15f, -1.15f, 2.82f, 1.15f, borda, cam);
        adicionarCaixaRot(x, z, r, 1.15f, 2.78f, -1.15f, 1.25f, 2.82f, 1.15f, borda, cam);
    } else if (it.tipo == EDIT_JANELA) {
        uint32_t vidroJan = 0xFFBFE8FF;
        adicionarCaixaRot(x, z, r, -0.90f, 1.00f, -0.02f, 0.90f, 2.00f, 0.02f, vidroJan, cam);
        adicionarCaixaRot(x, z, r, -1.00f, 0.90f, -0.06f, -0.90f, 2.10f, 0.06f, it.cor, cam);
        adicionarCaixaRot(x, z, r, 0.90f, 0.90f, -0.06f, 1.00f, 2.10f, 0.06f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.90f, 2.00f, -0.06f, 0.90f, 2.10f, 0.06f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.90f, 0.90f, -0.06f, 0.90f, 1.00f, 0.06f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.05f, 1.00f, -0.05f, 0.05f, 2.00f, 0.05f, it.cor, cam);
        adicionarCaixaRot(x, z, r, -0.90f, 1.48f, -0.05f, 0.90f, 1.52f, 0.05f, it.cor, cam);
    }
}

static void decorExtensao(const DecorItem& it, float* foraHx, float* foraHz, bool* foraGirado) {
    float hx = 0.0f, hz = 0.0f;
    bool girado = false;
    if (it.tipo == DECOR_PLANTA) { hx = 0.30f; hz = 0.30f; }
    else if (it.tipo == DECOR_MESA) { hx = 0.85f; hz = 0.50f; }
    else if (it.tipo == DECOR_BEBEDOURO) { hx = 0.28f; hz = 0.28f; }
    else if (it.tipo == DECOR_SOFA) { hx = 1.00f; hz = 0.50f; }
    else if (it.tipo == EDIT_CADEIRA) { hx = 0.28f; hz = 0.28f; }
    else if (it.tipo == EDIT_TV) { hx = 0.72f; hz = 0.20f; girado = true; }
    else if (it.tipo == EDIT_COMPUTADOR) { hx = 0.62f; hz = 0.32f; girado = true; }
    else if (it.tipo == EDIT_TECLADO) { hx = 0.70f; hz = 0.20f; girado = true; }
    else if (it.tipo == EDIT_PIANO) { hx = 0.80f; hz = 0.52f; girado = true; }
    else if (it.tipo == EDIT_VIOLAO || it.tipo == EDIT_VIOLINO) { hx = 0.22f; hz = 0.22f; }
    else if (it.tipo == EDIT_BATERIA) { hx = 0.85f; hz = 0.45f; girado = true; }
    else if (it.tipo == EDIT_PAREDE) { hx = 1.30f; hz = 0.12f; girado = true; }
    else if (it.tipo == EDIT_JANELA) { hx = 1.05f; hz = 0.12f; girado = true; }
    *foraHx = hx; *foraHz = hz; *foraGirado = girado;
}

static void desenharDecoracoes(const Camera& cam, float tempo) {
    for (int i = 0; i < decorTotal; i++) {
        const DecorItem& it = decorItens[i];
        float hx = 0.0f, hz = 0.0f;
        bool girado = false;
        decorExtensao(it, &hx, &hz, &girado);
        if (i == decorEditando) {
            float pulso = 1.0f + 0.05f * sinf(tempo * 6.0f);
            float hy = 0.02f;
            if (it.tipo == EDIT_TETO) hy = 2.75f;
            else if (it.tipo == EDIT_JANELA) hy = 0.85f;
            else if (it.tipo == EDIT_CHAO) hy = 0.17f;
            float ex = (hx > 0.0f ? hx : 0.9f) + 0.06f;
            float ez = (hz > 0.0f ? hz : 0.6f) + 0.06f;
            adicionarCaixaRot(it.x, it.z, it.rot, -ex * pulso, hy, -ez * pulso,
                              ex * pulso, hy + 0.012f, ez * pulso, 0xFFFFD166, cam);
        }
        decorDesenharItem(it, cam);
        if (hx > 0.0f) {
            if (girado) {
                float c = fabsf(cosf(it.rot)), s = fabsf(sinf(it.rot));
                float ax = hx * c + hz * s, az = hx * s + hz * c;
                adicionarObstaculo(it.x - ax, it.x + ax, it.z - az, it.z + az);
            } else {
                adicionarObstaculo(it.x - hx, it.x + hx, it.z - hz, it.z + hz);
            }
        }
    }
}

static void decorLog(const char* evento, bool limpar) {
    FILE* f = fopen("/data/HyperNeiva/decor_debug_log.txt", limpar ? "w" : "a");
    if (!f) return;
    fprintf(f, "%s itens=%d parede=%u chao=%u teto=%u\n", evento, decorTotal,
            (unsigned)corParedeEdit, (unsigned)corChaoEdit, (unsigned)corTetoEdit);
    fclose(f);
}

static void salvarDecoracoes() {
    FILE* f = fopen(DECOR_ARQUIVO, "w");
    if (!f) return;
    fprintf(f, "P %u %u %u\n", (unsigned)corParedeEdit, (unsigned)corChaoEdit, (unsigned)corTetoEdit);
    for (int i = 0; i < decorTotal; i++) {
        fprintf(f, "O %d %.3f %.3f %.4f %u\n", decorItens[i].tipo, decorItens[i].x,
                decorItens[i].z, decorItens[i].rot, (unsigned)decorItens[i].cor);
    }
    fclose(f);
    decorLog("SALVO", false);
}

static void carregarDecoracoes() {
    decorTotal = 0;
    corParedeEdit = 0;
    corChaoEdit = 0;
    corTetoEdit = 0;
    FILE* f = fopen(DECOR_ARQUIVO, "r");
    if (!f) { decorLog("CARREGADO SEM ARQUIVO", true); return; }
    char linha[192];
    while (decorTotal < DECOR_MAX && fgets(linha, sizeof(linha), f)) {
        char* p = linha;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 'P' || *p == 'p') {
            unsigned pr = 0, ch = 0, te = 0;
            if (sscanf(p + 1, "%u %u %u", &pr, &ch, &te) == 3) {
                corParedeEdit = pr; corChaoEdit = ch; corTetoEdit = te;
            }
            continue;
        }
        int tipo = 0;
        float x = 0.0f, z = 0.0f, rot = 0.0f;
        unsigned cor = 0;
        int lidos = 0;
        if (*p == 'O' || *p == 'o') lidos = sscanf(p + 1, "%d %f %f %f %u", &tipo, &x, &z, &rot, &cor);
        else lidos = sscanf(p, "%d %f %f %f %u", &tipo, &x, &z, &rot, &cor);
        if (lidos != 5) continue;
        if (tipo < 0 || tipo >= OBJ_TIPOS) continue;
        DecorItem& it = decorItens[decorTotal++];
        it.tipo = tipo; it.x = x; it.z = z; it.rot = rot;
        it.cor = cor ? cor : decorCorPadrao(tipo);
    }
    fclose(f);
    decorLog("CARREGADO", true);
}

static void desenharAgente(const DadosAgente& a, float fase, const Camera& cam) {
    float dx = a.x - cam.x;
    float dz = a.z - cam.z;
    float distancia = sqrtf(dx * dx + dz * dz);
    bool detalhado = distancia < 20.0f;

    cadeira(a.x, a.z, a.dir, 0xFF2C3A4E, cam);

    float sobe = 0.018f * sinf(fase);
    float balanco = 0.010f * sinf(fase * 0.8f);
    uint32_t calca = escurecer(a.camisa, 0.42f);
    uint32_t camisaEscura = escurecer(a.camisa, 0.78f);
    uint32_t sapato = 0xFF171A1F;
    float frente = a.dir;

    if (!detalhado) {
        adicionarCaixa(a.x - 0.25f, 0.52f, a.z - 0.22f, a.x + 0.25f, 1.22f, a.z + 0.22f, a.camisa, cam);
        adicionarCaixa(a.x - 0.30f, 1.15f, a.z - 0.18f, a.x + 0.30f, 1.27f, a.z + 0.18f, camisaEscura, cam);
        esfera(a.x, 1.47f + sobe, a.z, 0.18f, a.pele, cam, 6, 3);
        adicionarCaixa(a.x - 0.17f, 1.56f + sobe, a.z - 0.15f, a.x + 0.17f, 1.66f + sobe, a.z + 0.15f, a.cabelo, cam);
        return;
    }

    float coxaA = a.z - 0.18f;
    float coxaB = a.z + 0.30f * frente;
    if (coxaA > coxaB) { float troca = coxaA; coxaA = coxaB; coxaB = troca; }
    adicionarCaixa(a.x - 0.20f, 0.38f, coxaA, a.x - 0.05f, 0.54f, coxaB, calca, cam);
    adicionarCaixa(a.x + 0.05f, 0.38f, coxaA, a.x + 0.20f, 0.54f, coxaB, calca, cam);
    adicionarCaixa(a.x - 0.20f, 0.02f, a.z + 0.20f * frente - 0.10f, a.x - 0.05f, 0.40f, a.z + 0.38f * frente + 0.10f, calca, cam);
    adicionarCaixa(a.x + 0.05f, 0.02f, a.z + 0.20f * frente - 0.10f, a.x + 0.20f, 0.40f, a.z + 0.38f * frente + 0.10f, calca, cam);
    adicionarCaixa(a.x - 0.21f, 0.0f, a.z + 0.30f * frente - 0.10f, a.x - 0.04f, 0.09f, a.z + 0.50f * frente + 0.12f, sapato, cam);
    adicionarCaixa(a.x + 0.04f, 0.0f, a.z + 0.30f * frente - 0.10f, a.x + 0.21f, 0.09f, a.z + 0.50f * frente + 0.12f, sapato, cam);

    adicionarCaixa(a.x - 0.25f, 0.52f, a.z - 0.20f, a.x + 0.25f, 1.20f, a.z + 0.20f, a.camisa, cam);
    adicionarCaixa(a.x - 0.31f, 1.15f, a.z - 0.17f, a.x + 0.31f, 1.27f, a.z + 0.17f, camisaEscura, cam);

    float afast = 0.31f;
    adicionarCaixa(a.x - afast - 0.07f, 0.95f, a.z - 0.10f, a.x - afast + 0.07f, 1.22f, a.z + 0.10f, camisaEscura, cam);
    adicionarCaixa(a.x + afast - 0.07f, 0.95f, a.z - 0.10f, a.x + afast + 0.07f, 1.22f, a.z + 0.10f, camisaEscura, cam);
    float antebracoZ = a.z + 0.16f * frente;
    adicionarCaixa(a.x - afast - 0.06f, 0.78f + balanco, antebracoZ - 0.19f, a.x - afast + 0.06f, 0.97f, antebracoZ + 0.06f, a.pele, cam);
    adicionarCaixa(a.x + afast - 0.06f, 0.78f + balanco, antebracoZ - 0.19f, a.x + afast + 0.06f, 0.97f, antebracoZ + 0.06f, a.pele, cam);
    float maoZ = antebracoZ + 0.18f * frente;
    adicionarCaixa(a.x - afast - 0.055f, 0.76f + balanco, maoZ - 0.07f, a.x - afast + 0.055f, 0.84f + balanco, maoZ + 0.09f, a.pele, cam);
    adicionarCaixa(a.x + afast - 0.055f, 0.76f + balanco, maoZ - 0.07f, a.x + afast + 0.055f, 0.84f + balanco, maoZ + 0.09f, a.pele, cam);

    adicionarCaixa(a.x - 0.075f, 1.22f, a.z - 0.06f, a.x + 0.075f, 1.33f + sobe, a.z + 0.06f, a.pele, cam);
    esfera(a.x, 1.47f + sobe, a.z, 0.18f, a.pele, cam, 8, 4);

    adicionarCaixa(a.x - 0.18f, 1.57f + sobe, a.z - 0.16f, a.x + 0.18f, 1.66f + sobe, a.z + 0.16f, a.cabelo, cam);
    float nucaZ = a.z - frente * 0.14f;
    adicionarCaixa(a.x - 0.17f, 1.40f + sobe, nucaZ - 0.05f, a.x + 0.17f, 1.60f + sobe, nucaZ + 0.05f, a.cabelo, cam);

    float olhoZ = a.z + frente * 0.155f;
    float olhoZ2 = olhoZ + frente * 0.020f;
    if (olhoZ > olhoZ2) { float troca = olhoZ; olhoZ = olhoZ2; olhoZ2 = troca; }
    adicionarCaixa(a.x - 0.09f, 1.475f + sobe, olhoZ, a.x - 0.03f, 1.515f + sobe, olhoZ2, 0xFF101820, cam);
    adicionarCaixa(a.x + 0.03f, 1.475f + sobe, olhoZ, a.x + 0.09f, 1.515f + sobe, olhoZ2, 0xFF101820, cam);

    float crachaZ = a.z + frente * 0.20f;
    float crachaZ2 = crachaZ + frente * 0.012f;
    if (crachaZ > crachaZ2) { float troca = crachaZ; crachaZ = crachaZ2; crachaZ2 = troca; }
    adicionarCaixa(a.x - 0.07f, 0.94f, crachaZ, a.x + 0.07f, 1.12f, crachaZ2, 0xFFF4F1EC, cam);
    adicionarCaixa(a.x - 0.06f, 1.02f, crachaZ + 0.002f, a.x + 0.06f, 1.05f, crachaZ2 + 0.002f, a.camisa, cam);
}

enum { AV_CAMISA, AV_CALCA, AV_SAPATO, AV_PELE, AV_CABELO, AV_OLHO, AV_TOTAL };
struct AvatarCfg { int v[AV_TOTAL]; };

static const uint32_t PAL_CAMISA[] = {
    0xFFF1521B, 0xFF2E6DB4, 0xFF2F9E56, 0xFF8B5CD6, 0xFFE8B931, 0xFFE9579F, 0xFF2BB8C4, 0xFF22262E
};
static const uint32_t PAL_CALCA[] = {
    0xFF1A1C22, 0xFF243B6B, 0xFF5A6472, 0xFF3A5A8C, 0xFFB9A27A, 0xFF6B4A2F
};
static const uint32_t PAL_SAPATO[] = {
    0xFF171A1F, 0xFFF4F1EC, 0xFF7A4A2E, 0xFFB03A2E, 0xFF2E5FA8
};
static const uint32_t PAL_PELE[] = {
    0xFFF5D3B8, 0xFFE8B98E, 0xFFD9A07A, 0xFFB97A50, 0xFF8A5A3A, 0xFF5C3A26
};
static const uint32_t PAL_CABELO[] = {
    0xFF141414, 0xFF3B2416, 0xFFC9A15A, 0xFF8C2F1C, 0xFF9AA0A6, 0xFFE8E6DF, 0xFF2BB8C4, 0xFFE9579F
};
static const uint32_t PAL_OLHO[] = {
    0xFF101820, 0xFF4A2A1A, 0xFF2E6DB4, 0xFF2F6B3C, 0xFF8B6B2F, 0xFF6E7A88
};
static const uint32_t* const PALETAS[AV_TOTAL] = { PAL_CAMISA, PAL_CALCA, PAL_SAPATO, PAL_PELE, PAL_CABELO, PAL_OLHO };
static const int PALETAS_N[AV_TOTAL] = {
    (int)(sizeof(PAL_CAMISA) / sizeof(PAL_CAMISA[0])),
    (int)(sizeof(PAL_CALCA) / sizeof(PAL_CALCA[0])),
    (int)(sizeof(PAL_SAPATO) / sizeof(PAL_SAPATO[0])),
    (int)(sizeof(PAL_PELE) / sizeof(PAL_PELE[0])),
    (int)(sizeof(PAL_CABELO) / sizeof(PAL_CABELO[0])),
    (int)(sizeof(PAL_OLHO) / sizeof(PAL_OLHO[0]))
};
static const char* const CAT_NOMES[AV_TOTAL] = { "CAMISA", "CALCA", "SAPATO", "PELE", "CABELO", "OLHOS" };

#define AVATAR_CFG "/data/HyperNeiva/configuracao/escritorio3d_avatar.bin"

static void avatarPadrao(AvatarCfg& c) {
    c.v[AV_CAMISA] = 3;
    c.v[AV_CALCA] = 0;
    c.v[AV_SAPATO] = 0;
    c.v[AV_PELE] = 1;
    c.v[AV_CABELO] = 1;
    c.v[AV_OLHO] = 0;
}

static void carregarAvatar(AvatarCfg& c) {
    avatarPadrao(c);
    FILE* f = fopen(AVATAR_CFG, "rb");
    if (!f) return;
    int tmp[AV_TOTAL];
    if (fread(tmp, sizeof(tmp), 1, f) == 1) {
        for (int i = 0; i < AV_TOTAL; i++) {
            if (tmp[i] >= 0 && tmp[i] < PALETAS_N[i]) c.v[i] = tmp[i];
        }
    }
    fclose(f);
}

static void salvarAvatar(const AvatarCfg& c) {
    FILE* f = fopen(AVATAR_CFG, "wb");
    if (!f) return;
    fwrite(c.v, sizeof(c.v), 1, f);
    fclose(f);
}

static void desenharAvatar(float x, float z, float dir, float fase, const AvatarCfg& cfg, const Camera& cam) {
    uint32_t camisa = PALETAS[AV_CAMISA][cfg.v[AV_CAMISA]];
    uint32_t calca = PALETAS[AV_CALCA][cfg.v[AV_CALCA]];
    uint32_t sapato = PALETAS[AV_SAPATO][cfg.v[AV_SAPATO]];
    uint32_t pele = PALETAS[AV_PELE][cfg.v[AV_PELE]];
    uint32_t cabelo = PALETAS[AV_CABELO][cfg.v[AV_CABELO]];
    uint32_t olho = PALETAS[AV_OLHO][cfg.v[AV_OLHO]];

    float sobe = 0.018f * sinf(fase);
    float balanco = 0.010f * sinf(fase * 0.8f);
    uint32_t camisaEscura = escurecer(camisa, 0.78f);
    float frente = dir;

    float coxaA = z - 0.18f;
    float coxaB = z + 0.30f * frente;
    if (coxaA > coxaB) { float troca = coxaA; coxaA = coxaB; coxaB = troca; }
    adicionarCaixa(x - 0.20f, 0.38f, coxaA, x - 0.05f, 0.54f, coxaB, calca, cam);
    adicionarCaixa(x + 0.05f, 0.38f, coxaA, x + 0.20f, 0.54f, coxaB, calca, cam);
    adicionarCaixa(x - 0.20f, 0.02f, z + 0.20f * frente - 0.10f, x - 0.05f, 0.40f, z + 0.38f * frente + 0.10f, calca, cam);
    adicionarCaixa(x + 0.05f, 0.02f, z + 0.20f * frente - 0.10f, x + 0.20f, 0.40f, z + 0.38f * frente + 0.10f, calca, cam);
    adicionarCaixa(x - 0.21f, 0.0f, z + 0.30f * frente - 0.10f, x - 0.04f, 0.09f, z + 0.50f * frente + 0.12f, sapato, cam);
    adicionarCaixa(x + 0.04f, 0.0f, z + 0.30f * frente - 0.10f, x + 0.21f, 0.09f, z + 0.50f * frente + 0.12f, sapato, cam);

    adicionarCaixa(x - 0.25f, 0.52f, z - 0.20f, x + 0.25f, 1.20f, z + 0.20f, camisa, cam);
    adicionarCaixa(x - 0.31f, 1.15f, z - 0.17f, x + 0.31f, 1.27f, z + 0.17f, camisaEscura, cam);

    float afast = 0.31f;
    adicionarCaixa(x - afast - 0.07f, 0.95f, z - 0.10f, x - afast + 0.07f, 1.22f, z + 0.10f, camisaEscura, cam);
    adicionarCaixa(x + afast - 0.07f, 0.95f, z - 0.10f, x + afast + 0.07f, 1.22f, z + 0.10f, camisaEscura, cam);
    float antebracoZ = z + 0.16f * frente;
    adicionarCaixa(x - afast - 0.06f, 0.78f + balanco, antebracoZ - 0.19f, x - afast + 0.06f, 0.97f, antebracoZ + 0.06f, pele, cam);
    adicionarCaixa(x + afast - 0.06f, 0.78f + balanco, antebracoZ - 0.19f, x + afast + 0.06f, 0.97f, antebracoZ + 0.06f, pele, cam);
    float maoZ = antebracoZ + 0.18f * frente;
    adicionarCaixa(x - afast - 0.055f, 0.76f + balanco, maoZ - 0.07f, x - afast + 0.055f, 0.84f + balanco, maoZ + 0.09f, pele, cam);
    adicionarCaixa(x + afast - 0.055f, 0.76f + balanco, maoZ - 0.07f, x + afast + 0.055f, 0.84f + balanco, maoZ + 0.09f, pele, cam);

    adicionarCaixa(x - 0.075f, 1.22f, z - 0.06f, x + 0.075f, 1.33f + sobe, z + 0.06f, pele, cam);
    esfera(x, 1.47f + sobe, z, 0.18f, pele, cam, 8, 4);

    adicionarCaixa(x - 0.18f, 1.57f + sobe, z - 0.16f, x + 0.18f, 1.66f + sobe, z + 0.16f, cabelo, cam);
    float nucaZ = z - frente * 0.14f;
    adicionarCaixa(x - 0.17f, 1.40f + sobe, nucaZ - 0.05f, x + 0.17f, 1.60f + sobe, nucaZ + 0.05f, cabelo, cam);

    float olhoZ = z + frente * 0.155f;
    float olhoZ2 = olhoZ + frente * 0.020f;
    if (olhoZ > olhoZ2) { float troca = olhoZ; olhoZ = olhoZ2; olhoZ2 = troca; }
    adicionarCaixa(x - 0.09f, 1.475f + sobe, olhoZ, x - 0.03f, 1.515f + sobe, olhoZ2, olho, cam);
    adicionarCaixa(x + 0.03f, 1.475f + sobe, olhoZ, x + 0.09f, 1.515f + sobe, olhoZ2, olho, cam);

    float crachaZ = z + frente * 0.20f;
    float crachaZ2 = crachaZ + frente * 0.012f;
    if (crachaZ > crachaZ2) { float troca = crachaZ; crachaZ = crachaZ2; crachaZ2 = troca; }
    adicionarCaixa(x - 0.07f, 0.94f, crachaZ, x + 0.07f, 1.12f, crachaZ2, 0xFFF4F1EC, cam);
    adicionarCaixa(x - 0.06f, 1.02f, crachaZ + 0.002f, x + 0.06f, 1.05f, crachaZ2 + 0.002f, camisa, cam);
}

static void construirEspelho(const AvatarCfg& cfg, const Camera& cam, float tempo) {
    adicionarPiso(-3.0f, 3.0f, -2.4f, 3.0f, 0xFF4A3A30, 0xFF352A22, 0.0f, cam);
    adicionarCaixa(-2.8f, 0.0f, -1.35f, 2.8f, 3.0f, -1.25f, 0xFF2A3142, cam);
    adicionarCaixa(-2.3f, 2.65f, -1.24f, 2.3f, 2.78f, -1.22f, 0xFFFFE6B0, cam);
    adicionarCaixa(-2.3f, 0.10f, -1.24f, 2.3f, 0.18f, -1.22f, 0xFFF1521B, cam);
    adicionarCaixa(-0.9f, 0.0f, 0.55f, 0.9f, 0.10f, 1.55f, 0xFF6B4A2F, cam);
    desenharAvatar(0.0f, 0.0f, 1.0f, tempo * 1.2f, cfg, cam);
}

static const char* nomeSala(float x, float z) {
    if (z > 20.0f) return "AREA EXTERNA";
    if (x < -10.0f && z < -4.0f) return "CRIACAO";
    if (x > 10.0f && z < -4.0f) return "TRAFEGO E SOCIAL";
    if (x < -10.0f && z > 4.0f) return "COMERCIAL";
    if (x > 10.0f && z > 4.0f) return "OPERACAO";
    if (z < -8.0f) return "SALA DE REUNIAO";
    if (z > 8.0f) return "CAFE E LOUNGE";
    return "RECEPCAO";
}

static void construirCena(const Camera& cam, float tempo) {
    totalObstaculos = 0;

    uint32_t chaoA = corChaoEdit ? corChaoEdit : 0xFFD9D2C6;
    uint32_t chaoB = corChaoEdit ? escurecer(corChaoEdit, 0.82f) : 0xFFD9D2C6;
    adicionarPiso(-30.0f, 30.0f, -20.0f, 20.0f, chaoA, chaoB, 0.0f, cam);
    if (corChaoEdit == 0) {
        adicionarPiso(-30.0f, -10.0f, -20.0f, -4.0f, 0xFFF7E6CC, 0xFFEFDCC0, 0.02f, cam);
        adicionarPiso(10.0f, 30.0f, -20.0f, -4.0f, 0xFFDCEBF8, 0xFFCFE0F0, 0.02f, cam);
        adicionarPiso(-30.0f, -10.0f, 4.0f, 20.0f, 0xFFE9E1F6, 0xFFDED3F0, 0.02f, cam);
        adicionarPiso(10.0f, 30.0f, 4.0f, 20.0f, 0xFFE2F1E0, 0xFFD2E8D0, 0.02f, cam);
        adicionarPiso(-10.0f, 10.0f, -8.0f, 8.0f, 0xFFEEE8DE, 0xFFD3CBBE, 0.02f, cam);
        adicionarPiso(-8.0f, 8.0f, -20.0f, -8.0f, 0xFFC9CDD4, 0xFFBFC4CC, 0.02f, cam);
        adicionarPiso(-8.0f, 8.0f, 8.0f, 20.0f, 0xFFD9935A, 0xFFB97A48, 0.02f, cam);

        adicionarPiso(-4.0f, 4.0f, -3.0f, 4.0f, 0xFF2E6DB4, 0xFF2E6DB4, 0.03f, cam);
        adicionarPiso(-4.0f, -3.70f, -3.0f, 4.0f, 0xFFF1521B, 0xFFF1521B, 0.035f, cam);
        adicionarPiso(3.70f, 4.0f, -3.0f, 4.0f, 0xFFF1521B, 0xFFF1521B, 0.035f, cam);
        adicionarPiso(-4.0f, 4.0f, -3.0f, -2.70f, 0xFFF1521B, 0xFFF1521B, 0.035f, cam);
        adicionarPiso(-4.0f, 4.0f, 3.70f, 4.0f, 0xFFF1521B, 0xFFF1521B, 0.035f, cam);
    }

    uint32_t tetoA = corTetoEdit ? corTetoEdit : 0xFFF6F4EF;
    uint32_t tetoB = corTetoEdit ? escurecer(corTetoEdit, 0.88f) : 0xFFDEDAD1;
    adicionarPiso(-30.0f, 30.0f, -20.0f, 20.0f, tetoA, tetoB, 3.0f, cam, true);

    uint32_t luzTeto = 0xFFFBF7EA;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 4; j++) {
            float lx = -24.0f + i * 12.0f;
            float lz = -15.0f + j * 10.0f;
            adicionarCaixa(lx - 2.2f, 2.90f, lz - 0.7f, lx + 2.2f, 2.96f, lz + 0.7f, luzTeto, cam);
        }
    }

    uint32_t vidro = corParedeEdit ? corParedeEdit : 0xFFA9D8EE;
    adicionarCaixa(-30.25f, 0.0f, -20.25f, 30.25f, 3.0f, -19.85f, vidro, cam);
    adicionarCaixa(-30.25f, 0.0f, 19.85f, -3.0f, 3.0f, 20.25f, vidro, cam);
    adicionarCaixa(3.0f, 0.0f, 19.85f, 30.25f, 3.0f, 20.25f, vidro, cam);
    adicionarCaixa(-3.2f, 0.0f, 19.80f, -2.9f, 3.0f, 20.30f, 0xFF38424F, cam);
    adicionarCaixa(2.9f, 0.0f, 19.80f, 3.2f, 3.0f, 20.30f, 0xFF38424F, cam);
    adicionarCaixa(-3.2f, 2.80f, 19.80f, 3.2f, 3.0f, 20.30f, 0xFF38424F, cam);
    adicionarCaixa(-30.25f, 0.0f, -20.25f, -29.85f, 3.0f, 20.25f, vidro, cam);
    adicionarCaixa(29.85f, 0.0f, -20.25f, 30.25f, 3.0f, 20.25f, vidro, cam);

    paredeZ(-30.0f, -22.0f, -4.0f, vidro, cam);
    paredeZ(-18.0f, -10.0f, -4.0f, vidro, cam);
    paredeZ(10.0f, 18.0f, -4.0f, vidro, cam);
    paredeZ(22.0f, 30.0f, -4.0f, vidro, cam);
    paredeZ(-30.0f, -22.0f, 4.0f, vidro, cam);
    paredeZ(-18.0f, -10.0f, 4.0f, vidro, cam);
    paredeZ(10.0f, 18.0f, 4.0f, vidro, cam);
    paredeZ(22.0f, 30.0f, 4.0f, vidro, cam);

    paredeX(-10.0f, -20.0f, -14.0f, vidro, cam);
    paredeX(-10.0f, -10.0f, -4.0f, vidro, cam);
    paredeX(10.0f, -20.0f, -14.0f, vidro, cam);
    paredeX(10.0f, -10.0f, -4.0f, vidro, cam);
    paredeX(-10.0f, 4.0f, 10.0f, vidro, cam);
    paredeX(-10.0f, 14.0f, 20.0f, vidro, cam);
    paredeX(10.0f, 4.0f, 10.0f, vidro, cam);
    paredeX(10.0f, 14.0f, 20.0f, vidro, cam);

    uint32_t esquadria = 0xFF38424F;
    for (float mx = -20.0f; mx <= 20.0f; mx += 10.0f) {
        adicionarCaixa(mx - 0.07f, 0.0f, -19.87f, mx + 0.07f, 3.0f, -19.81f, esquadria, cam);
        if (mx > -1.0f && mx < 1.0f) continue;
        adicionarCaixa(mx - 0.07f, 0.0f, 19.81f, mx + 0.07f, 3.0f, 19.87f, esquadria, cam);
    }
    for (float mz = -10.0f; mz <= 10.0f; mz += 10.0f) {
        adicionarCaixa(-29.87f, 0.0f, mz - 0.07f, -29.81f, 3.0f, mz + 0.07f, esquadria, cam);
        adicionarCaixa(29.81f, 0.0f, mz - 0.07f, 29.87f, 3.0f, mz + 0.07f, esquadria, cam);
    }
    adicionarCaixa(-30.0f, 0.0f, -19.86f, 30.0f, 0.14f, -19.80f, esquadria, cam);
    adicionarCaixa(-30.0f, 0.0f, 19.80f, 30.0f, 0.14f, 19.86f, esquadria, cam);
    adicionarCaixa(-29.86f, 0.0f, -20.0f, -29.80f, 0.14f, 20.0f, esquadria, cam);
    adicionarCaixa(29.80f, 0.0f, -20.0f, 29.86f, 0.14f, 20.0f, esquadria, cam);

    adicionarCaixa(-3.0f, 0.0f, -8.18f, 3.0f, 3.0f, -7.94f, 0xFF062348, cam);
    adicionarCaixa(-2.4f, 1.82f, -7.93f, 2.4f, 2.02f, -7.88f, 0xFFF1521B, cam);
    adicionarObstaculo(-3.05f, 3.05f, -8.22f, -7.90f);

    adicionarCaixa(-1.7f, 0.0f, -5.45f, 1.7f, 1.05f, -4.55f, 0xFFF4F1EC, cam);
    adicionarCaixa(-1.72f, 1.05f, -5.48f, 1.72f, 1.11f, -4.52f, 0xFFE9E3D8, cam);
    adicionarCaixa(-1.71f, 0.30f, -4.56f, 1.71f, 0.84f, -4.54f, 0xFFF1521B, cam);
    adicionarObstaculo(-1.78f, 1.78f, -5.50f, -4.50f);
    planta(-2.6f, -5.2f, 1.1f, cam);
    planta(2.8f, -5.2f, 0.9f, cam);

    adicionarCaixa(-3.8f, 0.72f, -15.2f, 3.8f, 0.80f, -12.8f, 0xFFF1521B, cam);
    adicionarCaixa(-3.6f, 0.0f, -15.0f, -3.35f, 0.72f, -14.75f, 0xFFC33F10, cam);
    adicionarCaixa(3.35f, 0.0f, -15.0f, 3.6f, 0.72f, -14.75f, 0xFFC33F10, cam);
    adicionarObstaculo(-3.9f, 3.9f, -15.3f, -12.7f);
    for (int i = 0; i < 5; i++) {
        float x = -3.0f + i * 1.5f;
        cadeira(x, -12.3f, -1.0f, 0xFF3B4256, cam);
        cadeira(x, -15.7f, 1.0f, 0xFF3B4256, cam);
    }

    adicionarCaixa(18.80f, 0.72f, -12.60f, 21.20f, 0.78f, -11.40f, 0xFF1F6F43, cam);
    adicionarCaixa(18.90f, 0.78f, -12.55f, 21.10f, 0.80f, -11.45f, 0xFF2F9E56, cam);
    adicionarCaixa(19.97f, 0.80f, -12.55f, 20.03f, 0.81f, -11.45f, 0xFFFFFFFF, cam);
    adicionarCaixa(19.94f, 0.80f, -12.66f, 20.06f, 0.98f, -11.34f, 0xFFF2F2F2, cam);
    adicionarCaixa(18.95f, 0.0f, -12.50f, 19.10f, 0.72f, -12.35f, 0xFF2E4A7A, cam);
    adicionarCaixa(20.90f, 0.0f, -12.50f, 21.05f, 0.72f, -12.35f, 0xFF2E4A7A, cam);
    adicionarCaixa(18.95f, 0.0f, -11.65f, 19.10f, 0.72f, -11.50f, 0xFF2E4A7A, cam);
    adicionarCaixa(20.90f, 0.0f, -11.65f, 21.05f, 0.72f, -11.50f, 0xFF2E4A7A, cam);
    adicionarObstaculo(18.70f, 21.30f, -12.70f, -11.30f);

    adicionarCaixa(18.85f, 0.70f, 11.40f, 21.15f, 0.80f, 12.60f, 0xFF5A3E2B, cam);
    adicionarCaixa(18.95f, 0.80f, 11.50f, 21.05f, 0.82f, 12.50f, 0xFF0E6B3A, cam);
    adicionarCaixa(18.80f, 0.80f, 11.35f, 21.20f, 0.90f, 11.52f, 0xFF6B4A33, cam);
    adicionarCaixa(18.80f, 0.80f, 12.48f, 21.20f, 0.90f, 12.65f, 0xFF6B4A33, cam);
    adicionarCaixa(18.80f, 0.80f, 11.35f, 18.95f, 0.90f, 12.65f, 0xFF6B4A33, cam);
    adicionarCaixa(21.05f, 0.80f, 11.35f, 21.20f, 0.90f, 12.65f, 0xFF6B4A33, cam);
    adicionarCaixa(18.95f, 0.0f, 11.55f, 19.10f, 0.70f, 11.70f, 0xFF3A2A1C, cam);
    adicionarCaixa(20.90f, 0.0f, 11.55f, 21.05f, 0.70f, 11.70f, 0xFF3A2A1C, cam);
    adicionarCaixa(18.95f, 0.0f, 12.30f, 19.10f, 0.70f, 12.45f, 0xFF3A2A1C, cam);
    adicionarCaixa(20.90f, 0.0f, 12.30f, 21.05f, 0.70f, 12.45f, 0xFF3A2A1C, cam);
    adicionarObstaculo(18.70f, 21.30f, 11.25f, 12.75f);

    adicionarCaixa(-21.00f, 0.70f, -12.50f, -19.00f, 0.80f, -11.50f, 0xFF22456E, cam);
    adicionarCaixa(-20.90f, 0.80f, -12.40f, -19.10f, 0.82f, -11.60f, 0xFFDCEBF8, cam);
    adicionarCaixa(-20.25f, 0.82f, -12.38f, -19.75f, 0.83f, -11.62f, 0xFF2E6DB4, cam);
    adicionarCaixa(-20.35f, 0.80f, -12.55f, -19.65f, 0.86f, -12.44f, 0xFF101010, cam);
    adicionarCaixa(-20.35f, 0.80f, -11.56f, -19.65f, 0.86f, -11.45f, 0xFF101010, cam);
    adicionarCaixa(-20.95f, 0.0f, -12.45f, -20.80f, 0.70f, -12.30f, 0xFF1A3252, cam);
    adicionarCaixa(-19.20f, 0.0f, -12.45f, -19.05f, 0.70f, -12.30f, 0xFF1A3252, cam);
    adicionarCaixa(-20.95f, 0.0f, -11.70f, -20.80f, 0.70f, -11.55f, 0xFF1A3252, cam);
    adicionarCaixa(-19.20f, 0.0f, -11.70f, -19.05f, 0.70f, -11.55f, 0xFF1A3252, cam);
    adicionarObstaculo(-21.10f, -18.90f, -12.60f, -11.40f);

    adicionarCaixa(-20.80f, 0.80f, 11.55f, -19.20f, 0.90f, 12.45f, 0xFF3A2A1C, cam);
    adicionarCaixa(-20.70f, 0.90f, 11.65f, -19.30f, 0.91f, 12.35f, 0xFF2F6B3A, cam);
    adicionarCaixa(-20.85f, 0.93f, 11.88f, -19.15f, 0.97f, 11.92f, 0xFFBFC4CC, cam);
    adicionarCaixa(-20.85f, 0.93f, 12.08f, -19.15f, 0.97f, 12.12f, 0xFFBFC4CC, cam);
    adicionarCaixa(-20.85f, 0.93f, 12.28f, -19.15f, 0.97f, 12.32f, 0xFFBFC4CC, cam);
    for (int i = 0; i < 3; i++) {
        float rx = -20.55f + i * 0.55f;
        adicionarCaixa(rx, 0.97f, 11.90f, rx + 0.10f, 1.18f, 12.10f, i == 1 ? 0xFFF1521B : 0xFF2E6DB4, cam);
        adicionarCaixa(rx + 0.20f, 0.97f, 12.10f, rx + 0.30f, 1.18f, 12.30f, i == 1 ? 0xFF2E6DB4 : 0xFFF1521B, cam);
    }
    adicionarCaixa(-20.80f, 0.0f, 11.60f, -20.65f, 0.80f, 11.75f, 0xFF2A1D12, cam);
    adicionarCaixa(-19.35f, 0.0f, 11.60f, -19.20f, 0.80f, 11.75f, 0xFF2A1D12, cam);
    adicionarCaixa(-20.80f, 0.0f, 12.25f, -20.65f, 0.80f, 12.40f, 0xFF2A1D12, cam);
    adicionarCaixa(-19.35f, 0.0f, 12.25f, -19.20f, 0.80f, 12.40f, 0xFF2A1D12, cam);
    adicionarObstaculo(-20.90f, -19.10f, 11.45f, 12.55f);

    adicionarCaixa(-6.8f, 1.00f, -19.84f, -1.2f, 2.60f, -19.79f, 0xFFE8E6DF, cam);
    adicionarCaixa(-6.9f, 0.95f, -19.85f, -1.1f, 1.02f, -19.78f, 0xFF5A6472, cam);
    adicionarCaixa(-6.9f, 2.58f, -19.85f, -1.1f, 2.65f, -19.78f, 0xFF5A6472, cam);
    adicionarCaixa(-6.9f, 0.95f, -19.85f, -6.8f, 2.65f, -19.78f, 0xFF5A6472, cam);
    adicionarCaixa(-1.2f, 0.95f, -19.85f, -1.1f, 2.65f, -19.78f, 0xFF5A6472, cam);
    adicionarCaixa(-6.3f, 2.10f, -19.79f, -2.0f, 2.16f, -19.77f, 0xFFF1521B, cam);
    adicionarCaixa(-6.3f, 1.80f, -19.79f, -3.4f, 1.86f, -19.77f, 0xFF2E6DB4, cam);
    adicionarCaixa(-6.3f, 1.50f, -19.79f, -2.8f, 1.56f, -19.77f, 0xFF5A6472, cam);

    adicionarCaixa(1.3f, 1.15f, -19.84f, 6.3f, 2.60f, -19.78f, 0xFF14191F, cam);
    adicionarCaixa(1.45f, 1.25f, -19.78f, 6.15f, 2.50f, -19.75f, 0xFF223E66, cam);
    adicionarTelaTV(cam);

    adicionarCaixa(3.65f, 0.60f, 11.65f, 5.35f, 0.68f, 13.35f, 0xFF7A5230, cam);
    adicionarCaixa(3.75f, 0.68f, 11.75f, 5.25f, 0.76f, 13.25f, 0xFF2F6B3A, cam);
    adicionarCaixa(3.80f, 0.0f, 11.80f, 3.95f, 0.60f, 11.95f, 0xFF5A3E2B, cam);
    adicionarCaixa(5.05f, 0.0f, 11.80f, 5.20f, 0.60f, 11.95f, 0xFF5A3E2B, cam);
    adicionarCaixa(3.80f, 0.0f, 13.05f, 3.95f, 0.60f, 13.20f, 0xFF5A3E2B, cam);
    adicionarCaixa(5.05f, 0.0f, 13.05f, 5.20f, 0.60f, 13.20f, 0xFF5A3E2B, cam);
    adicionarCaixa(4.00f, 0.76f, 12.05f, 4.62f, 0.78f, 12.67f, 0xFFE8D6B0, cam);
    adicionarCaixa(4.75f, 0.76f, 12.15f, 5.05f, 0.78f, 12.55f, 0xFFFFFFFF, cam);
    adicionarCaixa(3.00f, 0.0f, 12.10f, 3.45f, 0.46f, 12.85f, 0xFF3B4256, cam);
    adicionarCaixa(5.55f, 0.0f, 12.10f, 6.00f, 0.46f, 12.85f, 0xFF3B4256, cam);
    adicionarCaixa(4.10f, 0.0f, 10.95f, 4.85f, 0.46f, 11.40f, 0xFF3B4256, cam);
    adicionarCaixa(4.10f, 0.0f, 13.60f, 4.85f, 0.46f, 14.05f, 0xFF3B4256, cam);
    adicionarObstaculo(3.55f, 5.45f, 11.55f, 13.45f);

    adicionarCaixa(2.6f, 0.0f, 19.0f, 7.8f, 1.05f, 19.8f, 0xFFF4F1EC, cam);
    adicionarCaixa(2.55f, 1.05f, 18.95f, 7.85f, 1.11f, 19.85f, 0xFF2C3A4E, cam);
    adicionarObstaculo(2.5f, 7.9f, 18.9f, 19.9f);
    adicionarCaixa(-1.9f, 0.0f, 17.4f, 1.9f, 0.42f, 18.4f, 0xFF2F6FD1, cam);
    adicionarCaixa(-1.9f, 0.42f, 18.2f, 1.9f, 0.95f, 18.4f, 0xFF275DB0, cam);
    adicionarObstaculo(-2.0f, 2.0f, 17.3f, 18.5f);
    adicionarCaixa(-4.9f, 0.0f, 12.4f, -3.9f, 0.42f, 15.6f, 0xFF2F6FD1, cam);
    adicionarCaixa(-4.9f, 0.42f, 12.4f, -4.7f, 0.95f, 15.6f, 0xFF275DB0, cam);
    adicionarObstaculo(-5.0f, -3.8f, 12.3f, 15.7f);
    adicionarCaixa(-0.9f, 0.0f, 13.8f, 0.9f, 0.38f, 14.9f, 0xFFB9844F, cam);
    adicionarObstaculo(-1.0f, 1.0f, 13.7f, 15.0f);
    planta(-7.2f, 9.0f, 1.2f, cam);
    planta(7.2f, 19.0f, 1.1f, cam);
    planta(-7.2f, 19.2f, 1.0f, cam);

    adicionarPiso(-3.5f, 3.5f, 10.5f, 16.5f, 0xFFC56B3E, 0xFFB65C34, 0.045f, cam);

    adicionarCaixa(-1.75f, 0.42f, 17.45f, -0.15f, 0.56f, 18.30f, 0xFF4C8BE8, cam);
    adicionarCaixa(0.15f, 0.42f, 17.45f, 1.75f, 0.56f, 18.30f, 0xFF4C8BE8, cam);
    adicionarCaixa(-1.75f, 0.52f, 18.10f, -0.15f, 0.92f, 18.36f, 0xFF3C74C9, cam);
    adicionarCaixa(0.15f, 0.52f, 18.10f, 1.75f, 0.92f, 18.36f, 0xFF3C74C9, cam);
    adicionarCaixa(-4.55f, 0.42f, 12.45f, -3.95f, 0.56f, 15.55f, 0xFF4C8BE8, cam);
    adicionarCaixa(-4.70f, 0.52f, 12.45f, -4.45f, 0.92f, 15.55f, 0xFF3C74C9, cam);

    adicionarCaixa(5.9f, 0.0f, 11.7f, 6.6f, 1.25f, 12.5f, 0xFFE9EEF4, cam);
    adicionarCaixa(6.0f, 1.25f, 11.85f, 6.5f, 1.85f, 12.35f, 0xFF7CC4E8, cam);
    adicionarObstaculo(5.8f, 6.7f, 11.6f, 12.6f);

    adicionarCaixa(-29.84f, 1.5f, -18.0f, -29.78f, 2.5f, -14.0f, 0xFFF2B84B, cam);
    adicionarCaixa(-29.77f, 1.65f, -17.4f, -29.74f, 2.35f, -14.6f, 0xFFF7E6CC, cam);
    adicionarCaixa(29.78f, 1.5f, -18.0f, 29.84f, 2.5f, -14.0f, 0xFF2E6DB4, cam);
    adicionarCaixa(29.74f, 1.65f, -17.4f, 29.77f, 2.35f, -14.6f, 0xFFDCEBF8, cam);
    adicionarCaixa(-29.84f, 1.5f, 12.0f, -29.78f, 2.5f, 16.0f, 0xFF8B5CD6, cam);
    adicionarCaixa(-29.77f, 1.65f, 12.6f, -29.74f, 2.35f, 15.4f, 0xFFE9E1F6, cam);
    adicionarCaixa(29.78f, 1.5f, 12.0f, 29.84f, 2.5f, 16.0f, 0xFF2F9E56, cam);
    adicionarCaixa(29.74f, 1.65f, 12.6f, 29.77f, 2.35f, 15.4f, 0xFFE2F1E0, cam);

    uint32_t madeira = 0xFF5A3E2B;
    adicionarCaixa(9.15f, 0.0f, -1.35f, 9.55f, 2.25f, 0.95f, madeira, cam);
    adicionarCaixa(9.10f, 0.18f, -1.20f, 9.14f, 2.00f, 0.80f, 0xFFBFE6F5, cam);
    adicionarCaixa(9.05f, 0.05f, -1.30f, 9.16f, 2.15f, -1.15f, 0xFF3A2A1C, cam);
    adicionarCaixa(9.05f, 0.05f, 0.75f, 9.16f, 2.15f, 0.90f, 0xFF3A2A1C, cam);
    adicionarCaixa(9.05f, 2.00f, -1.30f, 9.16f, 2.15f, 0.90f, 0xFF3A2A1C, cam);
    adicionarCaixa(9.05f, 0.05f, -1.30f, 9.16f, 0.18f, 0.90f, 0xFF3A2A1C, cam);
    adicionarCaixa(8.30f, 1.55f, 1.05f, 9.20f, 1.62f, 1.15f, 0xFF8A8F99, cam);
    adicionarCaixa(8.35f, 0.0f, 1.05f, 8.45f, 1.60f, 1.15f, 0xFF8A8F99, cam);
    adicionarCaixa(9.05f, 0.0f, 1.05f, 9.15f, 1.60f, 1.15f, 0xFF8A8F99, cam);
    adicionarCaixa(8.45f, 0.55f, 0.98f, 8.75f, 1.55f, 1.22f, 0xFFE9579F, cam);
    adicionarCaixa(8.75f, 0.35f, 0.98f, 9.05f, 1.55f, 1.22f, 0xFF2BB8C4, cam);
    adicionarObstaculo(8.20f, 9.60f, -1.45f, 1.35f);

    for (int i = 0; i < TOTAL_AGENTES - 1; i++) {
        const DadosAgente& a = agentes[i];
        mesa(a.x, a.z + 0.75f * a.dir, a.dir, 0xFFD3B58A, cam);
        desenharAgente(a, tempo * 1.4f + i * 0.7f, cam);
    }
    desenharAgente(agentes[TOTAL_AGENTES - 1], tempo * 1.1f, cam);

    planta(-9.2f, -7.3f, 1.2f, cam);
    planta(9.2f, -7.3f, 1.2f, cam);
    planta(-9.2f, 7.3f, 1.1f, cam);
    planta(9.2f, 7.3f, 1.1f, cam);
    planta(-29.0f, -19.0f, 1.1f, cam);
    planta(29.0f, -19.0f, 1.1f, cam);
    planta(-29.0f, 19.0f, 1.1f, cam);
    planta(29.0f, 19.0f, 1.1f, cam);

    uint32_t gramaA = 0xFF3E8E4A, gramaB = 0xFF357B40;
    uint32_t pedraA = 0xFFCFC4AE, pedraB = 0xFFBDB09A;
    uint32_t branco = 0xFFFFFFFF;
    adicionarPiso(-30.0f, 30.0f, 20.0f, 60.0f, gramaA, gramaB, 0.0f, cam);
    adicionarPiso(-4.0f, 4.0f, 20.0f, 27.0f, pedraA, pedraB, 0.02f, cam);
    adicionarPiso(-2.0f, 2.0f, 27.0f, 44.0f, pedraA, pedraB, 0.025f, cam);

    uint32_t cerca = 0xFF8A8F99, mourão = 0xFF5A6472;
    adicionarCaixa(-30.20f, 0.0f, 20.0f, -29.90f, 1.60f, 60.0f, cerca, cam);
    adicionarCaixa(29.90f, 0.0f, 20.0f, 30.20f, 1.60f, 60.0f, cerca, cam);
    adicionarCaixa(-30.20f, 0.0f, 59.70f, 30.20f, 1.60f, 60.0f, cerca, cam);
    adicionarCaixa(-30.20f, 1.60f, 20.0f, -29.90f, 1.75f, 60.0f, mourão, cam);
    adicionarCaixa(29.90f, 1.60f, 20.0f, 30.20f, 1.75f, 60.0f, mourão, cam);
    adicionarCaixa(-30.20f, 1.60f, 59.70f, 30.20f, 1.75f, 60.0f, mourão, cam);
    for (float fz = 20.0f; fz <= 60.0f; fz += 5.0f) {
        adicionarCaixa(-30.25f, 0.0f, fz - 0.10f, -29.85f, 1.90f, fz + 0.10f, mourão, cam);
        adicionarCaixa(29.85f, 0.0f, fz - 0.10f, 30.25f, 1.90f, fz + 0.10f, mourão, cam);
    }
    for (float fx = -30.0f; fx <= 30.0f; fx += 10.0f)
        adicionarCaixa(fx - 0.10f, 0.0f, 59.65f, fx + 0.10f, 1.90f, 60.05f, mourão, cam);

    uint32_t areiaA = 0xFFE8D6A8, areiaB = 0xFFDCC894;
    adicionarPiso(-24.0f, -6.0f, 27.0f, 41.0f, areiaA, areiaB, 0.03f, cam);
    adicionarPiso(-23.5f, -6.5f, 27.5f, 27.8f, branco, branco, 0.045f, cam);
    adicionarPiso(-23.5f, -6.5f, 40.2f, 40.5f, branco, branco, 0.045f, cam);
    adicionarPiso(-23.5f, -23.2f, 27.5f, 40.5f, branco, branco, 0.045f, cam);
    adicionarPiso(-6.8f, -6.5f, 27.5f, 40.5f, branco, branco, 0.045f, cam);
    adicionarPiso(-15.15f, -14.85f, 27.5f, 40.5f, branco, branco, 0.045f, cam);
    adicionarCaixa(-15.15f, 0.0f, 27.0f, -14.85f, 2.45f, 27.25f, mourão, cam);
    adicionarCaixa(-15.15f, 0.0f, 40.75f, -14.85f, 2.45f, 41.0f, mourão, cam);
    adicionarCaixa(-15.06f, 2.20f, 27.2f, -14.94f, 2.35f, 40.8f, branco, cam);
    adicionarObstaculo(-15.25f, -14.75f, 27.0f, 41.0f);

    uint32_t tenisA = 0xFF2E7D5B, tenisB = 0xFF266B4D;
    adicionarPiso(-4.0f, 12.0f, 27.0f, 41.0f, tenisA, tenisB, 0.03f, cam);
    adicionarPiso(-3.5f, 11.5f, 27.5f, 27.8f, branco, branco, 0.045f, cam);
    adicionarPiso(-3.5f, 11.5f, 40.2f, 40.5f, branco, branco, 0.045f, cam);
    adicionarPiso(-3.5f, -3.2f, 27.5f, 40.5f, branco, branco, 0.045f, cam);
    adicionarPiso(11.2f, 11.5f, 27.5f, 40.5f, branco, branco, 0.045f, cam);
    adicionarPiso(3.85f, 4.15f, 27.5f, 40.5f, branco, branco, 0.045f, cam);
    adicionarCaixa(3.85f, 0.0f, 27.0f, 4.15f, 1.35f, 27.25f, mourão, cam);
    adicionarCaixa(3.85f, 0.0f, 40.75f, 4.15f, 1.35f, 41.0f, mourão, cam);
    adicionarCaixa(3.94f, 1.10f, 27.2f, 4.06f, 1.25f, 40.8f, branco, cam);
    adicionarObstaculo(3.75f, 4.25f, 27.0f, 41.0f);

    uint32_t campoA = 0xFF2F8E4A, campoB = 0xFF277A3F;
    adicionarPiso(-24.0f, 24.0f, 44.0f, 58.0f, campoA, campoB, 0.03f, cam);
    adicionarPiso(-23.5f, 23.5f, 44.5f, 44.8f, branco, branco, 0.045f, cam);
    adicionarPiso(-23.5f, 23.5f, 57.2f, 57.5f, branco, branco, 0.045f, cam);
    adicionarPiso(-23.5f, -23.2f, 44.5f, 57.5f, branco, branco, 0.045f, cam);
    adicionarPiso(23.2f, 23.5f, 44.5f, 57.5f, branco, branco, 0.045f, cam);
    adicionarPiso(-23.5f, 23.5f, 50.85f, 51.15f, branco, branco, 0.045f, cam);
    adicionarPiso(-0.3f, 0.3f, 50.7f, 51.3f, branco, branco, 0.05f, cam);
    adicionarCaixa(-24.30f, 0.0f, 48.0f, -24.0f, 1.00f, 48.3f, branco, cam);
    adicionarCaixa(-24.30f, 0.0f, 53.7f, -24.0f, 1.00f, 54.0f, branco, cam);
    adicionarCaixa(-24.30f, 0.90f, 48.0f, -24.0f, 1.00f, 54.0f, branco, cam);
    adicionarCaixa(24.0f, 0.0f, 48.0f, 24.30f, 1.00f, 48.3f, branco, cam);
    adicionarCaixa(24.0f, 0.0f, 53.7f, 24.30f, 1.00f, 54.0f, branco, cam);
    adicionarCaixa(24.0f, 0.90f, 48.0f, 24.30f, 1.00f, 54.0f, branco, cam);

    adicionarCaixa(-28.0f, 0.0f, 24.0f, -27.70f, 3.20f, 24.30f, mourão, cam);
    adicionarCaixa(27.70f, 0.0f, 24.0f, 28.0f, 3.20f, 24.30f, mourão, cam);
    esfera(-27.85f, 3.35f, 24.15f, 0.45f, 0xFFFFF2B0, cam, 8, 4);
    esfera(27.85f, 3.35f, 24.15f, 0.45f, 0xFFFFF2B0, cam, 8, 4);
    esfera(0.0f, 32.0f, 120.0f, 7.0f, 0xFFFFF2B0, cam, 10, 5);
}

static bool posicaoLivre(float x, float z) {
    const float raio = 0.34f;
    if (x < -29.35f || x > 29.35f || z < -19.35f || z > 59.35f) return false;
    if (z > 19.35f && z < 20.75f && (x < -2.60f || x > 2.60f)) return false;
    for (int i = 0; i < totalObstaculos; i++) {
        const Obstaculo& o = obstaculos[i];
        if (x + raio > o.x0 && x - raio < o.x1 && z + raio > o.z0 && z - raio < o.z1) return false;
    }
    return true;
}

static void preencherRet(uint32_t* tela, int x, int y, int w, int h, uint32_t cor) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > TELA_L ? TELA_L : x + w;
    int y1 = y + h > TELA_A ? TELA_A : y + h;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* linha = tela + yy * TELA_L;
        for (int xx = x0; xx < x1; xx++) linha[xx] = cor;
    }
}

static void desenharMinimapa(uint32_t* tela, float px, float pz) {
    const int mx = 1574, my = 20, mw = 300, mh = 200;
    preencherRet(tela, mx - 5, my - 5, mw + 10, mh + 10, 0xFF062348);
    preencherRet(tela, mx, my, mw, mh, 0xFF0E2B55);
    struct MapaSala { float x0, z0, x1, z1; uint32_t cor; };
    static const MapaSala salas[] = {
        { -30.0f, -20.0f, -10.0f, -4.0f, 0xFFF7E6CC },
        { 10.0f, -20.0f, 30.0f, -4.0f, 0xFFDCEBF8 },
        { -30.0f, 4.0f, -10.0f, 20.0f, 0xFFE9E1F6 },
        { 10.0f, 4.0f, 30.0f, 20.0f, 0xFFE2F1E0 },
        { -10.0f, -8.0f, 10.0f, 8.0f, 0xFFEEE8DE },
        { -8.0f, -20.0f, 8.0f, -8.0f, 0xFFC9CDD4 },
        { -8.0f, 8.0f, 8.0f, 20.0f, 0xFFD9935A }
    };
    for (int i = 0; i < 7; i++) {
        int x = mx + (int)((salas[i].x0 + 30.0f) / 60.0f * mw);
        int y = my + (int)((salas[i].z0 + 20.0f) / 40.0f * mh);
        int x1 = mx + (int)((salas[i].x1 + 30.0f) / 60.0f * mw);
        int y1 = my + (int)((salas[i].z1 + 20.0f) / 40.0f * mh);
        preencherRet(tela, x, y, x1 - x, y1 - y, salas[i].cor);
    }
    {
        static const float mesasFis[4][3] = {
            { 20.0f, -12.0f, 0.0f }, { 20.0f, 12.0f, 1.0f },
            { -20.0f, -12.0f, 2.0f }, { -20.0f, 12.0f, 3.0f }
        };
        static const uint32_t coresFis[4] = { 0xFF2F9E56, 0xFF0E6B3A, 0xFFDCEBF8, 0xFFF1521B };
        for (int i = 0; i < 4; i++) {
            int fx = mx + (int)((mesasFis[i][0] + 30.0f) / 60.0f * mw);
            int fy = my + (int)((mesasFis[i][1] + 20.0f) / 40.0f * mh);
            preencherRet(tela, fx - 5, fy - 3, 11, 7, coresFis[(int)mesasFis[i][2]]);
        }
    }
    for (int i = 0; i < TOTAL_AGENTES; i++) {
        int x = mx + (int)((agentes[i].x + 30.0f) / 60.0f * mw);
        int y = my + (int)((agentes[i].z + 20.0f) / 40.0f * mh);
        preencherRet(tela, x - 2, y - 2, 5, 5, agentes[i].camisa);
    }
    int x = mx + (int)((px + 30.0f) / 60.0f * mw);
    int y = my + (int)((pz + 20.0f) / 40.0f * mh);
    if (x < mx) x = mx;
    if (x > mx + mw - 1) x = mx + mw - 1;
    if (y < my) y = my;
    if (y > my + mh - 1) y = my + mh - 1;
    preencherRet(tela, x - 4, y - 4, 9, 9, 0xFFF1521B);
    desenharTexto(tela, "MAPA", 18, mx + 10, my + 8, 0xFFFFFFFF);
}

static void desenharHUD(uint32_t* tela, const Camera& cam, float px, float pz, int agentePerto, int dialogoAgente, float dialogoTimer, float fps, float msQuadro, int interacao) {
    preencherRet(tela, 20, 20, 500, 92, 0xE6062348);
    desenharTexto(tela, "ESCRITORIO 3D", 30, 38, 32, 0xFFFFFFFF);
    char linhaSala[128];
    snprintf(linhaSala, sizeof(linhaSala), "SALA: %s", nomeSala(px, pz));
    desenharTexto(tela, linhaSala, 22, 38, 70, 0xFFFFB07A);

    desenharMinimapa(tela, px, pz);

    for (int i = 0; i < TOTAL_AGENTES; i++) {
        float dx = agentes[i].x - cam.x;
        float dz = agentes[i].z - cam.z;
        if (dx * dx + dz * dz > 18.0f * 18.0f) continue;
        Ponto q;
        paraCamera(ponto(agentes[i].x, 1.98f, agentes[i].z), cam, q);
        if (q.z < 0.2f) continue;
        float sx, sy, invZ;
        projetar(q, cam, sx, sy, invZ);
        int fx = (int)(sx * ESCALA_TELA);
        int fy = (int)(sy * ESCALA_TELA);
        if (fx < 0 || fx >= TELA_L || fy < 20 || fy >= TELA_A) continue;
        int largura = (int)strlen(agentes[i].nome) * 10;
        desenharTexto(tela, agentes[i].nome, 18, fx - largura / 2, fy - 20, i == agentePerto ? 0xFFFFB07A : 0xFFFFFFFF);
    }

    int cx = TELA_L / 2;
    int cy = TELA_A / 2;
    for (int i = -14; i <= 14; i++) {
        if (i > -5 && i < 5) continue;
        tela[cy * TELA_L + cx + i] = 0xFFFFFFFF;
        tela[(cy + i) * TELA_L + cx] = 0xFFFFFFFF;
    }

    preencherRet(tela, 330, 1012, 1260, 44, 0xDD062348);
    desenharTexto(tela, "ANALOGICO ESQ MOVER   DIR OLHAR   L2 CORRER   R1 PULAR   X USAR   OPTIONS MENU   BOLINHA SAIR", 18, 348, 1024, 0xFFDCE6F3);

    if (dialogoTimer > 0.0f && dialogoAgente >= 0 && dialogoAgente < TOTAL_AGENTES) {
        const DadosAgente& a = agentes[dialogoAgente];
        preencherRet(tela, 470, 842, 980, 130, 0xE6062348);
        desenharTexto(tela, a.nome, 28, 500, 860, 0xFFFFB07A);
        desenharTexto(tela, a.papel, 21, 500, 900, 0xFFFFFFFF);
        desenharTexto(tela, "X: FALAR NOVAMENTE   BOLINHA: SAIR", 17, 500, 940, 0xFFA9BCD6);
    }
    else if (interacao == 1) {
        preencherRet(tela, 660, 842, 600, 58, 0xE6062348);
        desenharTexto(tela, "X  ABRIR O CAMARIM", 24, 690, 858, 0xFFFFFFFF);
    }
    else if (interacao == 2) {
        preencherRet(tela, 660, 842, 600, 58, 0xE6062348);
        desenharTexto(tela, "X  USAR A TV", 24, 690, 858, 0xFFFFFFFF);
    }
    else if (interacao == 3) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  SENTAR NA MESA DE JOGOS", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 4) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR PING PONG", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 5) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR SINUCA", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 6) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR AIR HOCKEY", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 7) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR PEBOLIM", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 8) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR VOLEI", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 9) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR TENIS", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (interacao == 10) {
        preencherRet(tela, 640, 842, 640, 58, 0xE6062348);
        desenharTexto(tela, "X  JOGAR FUTEBOL", 24, 668, 858, 0xFFFFFFFF);
    }
    else if (agentePerto >= 0 && agentePerto < TOTAL_AGENTES) {
        char aviso[160];
        snprintf(aviso, sizeof(aviso), "X  CONVERSAR COM %s", agentes[agentePerto].nome);
        preencherRet(tela, 660, 842, 600, 58, 0xE6062348);
        desenharTexto(tela, aviso, 24, 690, 858, 0xFFFFFFFF);
    }

    desenharTexto(tela, "HYPER NEIVA", 20, 1640, 1030, 0xFFA9BCD6);

    bool modoCpu = (renderFallback || totalThreadsRender == 0);
    uint32_t corModo = modoCpu ? 0xFFFF7A7A : 0xFF7BE08A;
    preencherRet(tela, 20, 120, 660, 96, 0xDD062348);
    char textoFps[128];
    if (modoCpu) snprintf(textoFps, sizeof(textoFps), "FPS %d  CPU  %.0fms  TRI %d", (int)(fps + 0.5f), msQuadro, totalTriangulos);
    else snprintf(textoFps, sizeof(textoFps), "FPS %d  MT%d  %.0fms  TRI %d", (int)(fps + 0.5f), totalThreadsRender, msQuadro, totalTriangulos);
    desenharTexto(tela, textoFps, 28, 34, 128, corModo);
    char textoDiag[64];
    snprintf(textoDiag, sizeof(textoDiag), "%s  D %d  L1 CULL", cullingAtivo ? "CULL ON" : "CULL OFF", triangulosDescartados);
    desenharTexto(tela, textoDiag, 22, 34, 168, 0xFFFFFFFF);
}

static void desenharUIEspelho(uint32_t* tela, const AvatarCfg& cfg, int cat) {
    preencherRet(tela, 30, 110, 600, 860, 0xDD0A1220);
    desenharTexto(tela, "CAMARIM", 40, 70, 140, 0xFFFFFFFF);
    desenharTexto(tela, "ESPELHO E ROUPAS", 22, 72, 196, 0xFFFFB07A);
    for (int i = 0; i < AV_TOTAL; i++) {
        int y = 260 + i * 105;
        if (i == cat) preencherRet(tela, 55, y - 12, 550, 86, 0xFF1E3A5F);
        desenharTexto(tela, CAT_NOMES[i], 26, 80, y, i == cat ? 0xFFFFD166 : 0xFFFFFFFF);
        uint32_t cor = PALETAS[i][cfg.v[i]];
        preencherRet(tela, 420, y - 2, 90, 54, cor);
        preencherRet(tela, 418, y - 4, 94, 4, 0xFFFFFFFF);
        preencherRet(tela, 418, y + 52, 94, 4, 0xFFFFFFFF);
        preencherRet(tela, 418, y - 2, 4, 54, 0xFFFFFFFF);
        preencherRet(tela, 508, y - 2, 4, 54, 0xFFFFFFFF);
    }

    preencherRet(tela, 1310, 150, 560, 620, 0xDD0A1220);
    desenharTexto(tela, "OPCOES DE COR", 26, 1340, 175, 0xFFFFFFFF);
    int n = PALETAS_N[cat];
    for (int i = 0; i < n; i++) {
        int col = i % 4;
        int lin = i / 4;
        int x = 1350 + col * 125;
        int y = 250 + lin * 125;
        preencherRet(tela, x, y, 100, 90, PALETAS[cat][i]);
        if (i == cfg.v[cat]) {
            preencherRet(tela, x - 5, y - 5, 110, 5, 0xFFFFD166);
            preencherRet(tela, x - 5, y + 90, 110, 5, 0xFFFFD166);
            preencherRet(tela, x - 5, y, 5, 90, 0xFFFFD166);
            preencherRet(tela, x + 100, y, 5, 90, 0xFFFFD166);
        }
    }

    preencherRet(tela, 30, 985, 1860, 48, 0xDD0A1220);
    desenharTexto(tela, "CIMA/BAIXO: CATEGORIA   ESQ/DIR: TROCAR COR   X: PROXIMA   BOLINHA: SALVAR E SAIR", 22, 60, 997, 0xFFDCE6F3);
}

static char tvUltimo[512] = "";

static void desenharUITV(uint32_t* tela, int sel, const char* ultimo) {
    preencherRet(tela, 560, 290, 800, 500, 0xEE0A1220);
    desenharTexto(tela, "TV DA SALA DE REUNIAO", 30, 620, 325, 0xFFFFFFFF);
    const char* opcoes[3] = {
        "MIDIA     (videos, imagens e musicas do app)",
        "EXPLORAR  (Hyper Neiva, Raiz, USB0, USB1)",
        "RADIO     (populares, Sao Paulo e podcasts)"
    };
    for (int i = 0; i < 3; i++) {
        int y = 420 + i * 82;
        if (i == sel) preencherRet(tela, 590, y - 10, 740, 66, 0xFF1E3A5F);
        desenharTexto(tela, opcoes[i], 23, 620, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    if (ultimo && ultimo[0]) {
        char linha[320];
        snprintf(linha, sizeof(linha), "ULTIMO VIDEO: %.180s", ultimo);
        desenharTexto(tela, linha, 18, 620, 690, 0xFFA9BCD6);
    }
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: ENTRAR   QUADRADO: DESLIGAR TV   BOLINHA: SAIR", 19, 620, 735, 0xFFDCE6F3);
}

// ---------------------------------------------------------------------------
static void desenharUIOpcoes(uint32_t* tela, int sel) {
    preencherRet(tela, 560, 290, 800, 500, 0xEE0A1220);
    desenharTexto(tela, "OPCOES", 34, 620, 325, 0xFFFFFFFF);
    const char* op[3] = { "DECORACAO", "MODO EDITAR", "FECHAR" };
    for (int i = 0; i < 3; i++) {
        int y = 430 + i * 86;
        if (i == sel) preencherRet(tela, 590, y - 10, 740, 66, 0xFF1E3A5F);
        desenharTexto(tela, op[i], 24, 620, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: ENTRAR   OPTIONS/BOLINHA: FECHAR", 18, 620, 735, 0xFFDCE6F3);
}

static void desenharUIDecorLista(uint32_t* tela, int sel, int total) {
    preencherRet(tela, 470, 210, 980, 660, 0xEE0A1220);
    desenharTexto(tela, "DECORACAO", 32, 520, 240, 0xFFFFFFFF);
    const char* op[DECOR_TIPOS + 3] = {
        "ADICIONAR PLANTA", "ADICIONAR MESA", "ADICIONAR BEBEDOURO",
        "ADICIONAR SOFA", "ADICIONAR TAPETE",
        "AJUSTAR / APAGAR ITEM", "CONSTRUIR PAREDE / PISO / TETO", "VOLTAR"
    };
    for (int i = 0; i < DECOR_TIPOS + 3; i++) {
        int y = 300 + i * 58;
        if (i == sel) preencherRet(tela, 500, y - 8, 920, 48, 0xFF1E3A5F);
        desenharTexto(tela, op[i], 22, 520, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    char info[128];
    snprintf(info, sizeof(info), "ITENS COLOCADOS: %d de %d", total, DECOR_MAX);
    desenharTexto(tela, info, 18, 520, 770, 0xFFA9BCD6);
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: ESCOLHER   BOLINHA: VOLTAR", 18, 520, 812, 0xFFDCE6F3);
}

static void desenharUIDecorAjuste(uint32_t* tela, int sel, int total) {
    preencherRet(tela, 520, 300, 880, 470, 0xEE0A1220);
    desenharTexto(tela, "AJUSTAR / APAGAR", 30, 580, 335, 0xFFFFFFFF);
    if (total <= 0) {
        desenharTexto(tela, "NENHUM ITEM COLOCADO AINDA", 24, 620, 430, 0xFFFFB07A);
    } else {
        int idx = sel;
        if (idx < 0) idx = 0;
        if (idx >= total) idx = total - 1;
        char linha[160];
        const char* nomeItem = (decorItens[idx].tipo >= 0 && decorItens[idx].tipo < OBJ_TIPOS) ? OBJ_NOMES[decorItens[idx].tipo] : "ITEM";
        snprintf(linha, sizeof(linha), "ITEM %d DE %d: %s", idx + 1, total, nomeItem);
        desenharTexto(tela, linha, 26, 620, 430, 0xFFFFD166);
        snprintf(linha, sizeof(linha), "X %.1f   Z %.1f   GIRO %.2f", decorItens[idx].x, decorItens[idx].z, decorItens[idx].rot);
        desenharTexto(tela, linha, 20, 620, 480, 0xFFDCE6F3);
        desenharTexto(tela, "X: PEGAR E MOVER   QUADRADO: APAGAR", 20, 620, 540, 0xFFFFFFFF);
    }
    desenharTexto(tela, "CIMA/BAIXO: TROCAR ITEM   BOLINHA: VOLTAR", 18, 600, 700, 0xFFDCE6F3);
}

static void desenharUIDecorPlace(uint32_t* tela, const DecorItem& it) {
    preencherRet(tela, 0, 0, TELA_L, 62, 0xDD062348);
    char linha[200];
    const char* nomeItem = (it.tipo >= 0 && it.tipo < OBJ_TIPOS) ? OBJ_NOMES[it.tipo] : "ITEM";
    snprintf(linha, sizeof(linha), "COLOCANDO %s   X %.1f   Z %.1f   GIRO %.2f",
             nomeItem, it.x, it.z, it.rot);
    desenharTexto(tela, linha, 24, 30, 18, 0xFFFFD166);
    preencherRet(tela, 0, 1018, TELA_L, 62, 0xDD062348);
    desenharTexto(tela, "ANALOGICO/DIRECIONAL: MOVER   L1/R1: GIRAR   TRIANGULO: COR   X: CONFIRMAR   BOLINHA: CANCELAR", 20, 30, 1032, 0xFFDCE6F3);
}

static void desenharUIEditLista(uint32_t* tela, int sel, int total) {
    preencherRet(tela, 500, 250, 950, 620, 0xEE0A1220);
    desenharTexto(tela, "MODO EDITAR", 32, 550, 285, 0xFFFFFFFF);
    const char* op[5] = { "PINTAR", "CONSTRUIR", "OBJETOS", "AJUSTAR / APAGAR ITEM", "VOLTAR" };
    for (int i = 0; i < 5; i++) {
        int y = 360 + i * 76;
        if (i == sel) preencherRet(tela, 530, y - 10, 890, 62, 0xFF1E3A5F);
        desenharTexto(tela, op[i], 24, 550, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    char info[128];
    snprintf(info, sizeof(info), "ITENS COLOCADOS: %d de %d", total, DECOR_MAX);
    desenharTexto(tela, info, 18, 550, 770, 0xFFA9BCD6);
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: ENTRAR   BOLINHA: VOLTAR", 18, 550, 812, 0xFFDCE6F3);
}

static void desenharUIEditPintar(uint32_t* tela, int sel) {
    preencherRet(tela, 520, 300, 900, 520, 0xEE0A1220);
    desenharTexto(tela, "PINTAR", 32, 580, 335, 0xFFFFFFFF);
    const char* op[4] = { "PAREDES", "CHAO", "TETO", "VOLTAR" };
    for (int i = 0; i < 4; i++) {
        int y = 410 + i * 78;
        if (i == sel) preencherRet(tela, 550, y - 10, 840, 62, 0xFF1E3A5F);
        char linha[160];
        uint32_t cor = (i == 0) ? corParedeEdit : (i == 1) ? corChaoEdit : corTetoEdit;
        if (i < 3) {
            if (cor == 0) snprintf(linha, sizeof(linha), "%s: PADRAO", op[i]);
            else snprintf(linha, sizeof(linha), "%s: COR %06X", op[i], (unsigned)(cor & 0xFFFFFF));
        } else {
            snprintf(linha, sizeof(linha), "%s", op[i]);
        }
        desenharTexto(tela, linha, 24, 580, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    desenharTexto(tela, "ESQUERDA/DIREITA: TROCAR COR   X: SALVAR   BOLINHA: VOLTAR", 18, 560, 720, 0xFFDCE6F3);
}

static void desenharUIEditConstruir(uint32_t* tela, int sel) {
    preencherRet(tela, 520, 280, 900, 560, 0xEE0A1220);
    desenharTexto(tela, "CONSTRUIR", 32, 580, 315, 0xFFFFFFFF);
    const char* op[5] = { "PAREDE", "PISO", "TETO", "JANELA", "VOLTAR" };
    for (int i = 0; i < 5; i++) {
        int y = 390 + i * 74;
        if (i == sel) preencherRet(tela, 550, y - 10, 840, 60, 0xFF1E3A5F);
        desenharTexto(tela, op[i], 24, 580, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: COLOCAR   BOLINHA: VOLTAR", 18, 560, 790, 0xFFDCE6F3);
}

static void desenharUIEditObjetos(uint32_t* tela, int sel) {
    preencherRet(tela, 460, 190, 980, 700, 0xEE0A1220);
    desenharTexto(tela, "OBJETOS", 32, 520, 225, 0xFFFFFFFF);
    const char* op[9] = {
        "CADEIRA", "TV", "COMPUTADOR", "TECLADO", "PIANO",
        "VIOLAO", "VIOLINO", "BATERIA", "VOLTAR"
    };
    for (int i = 0; i < 9; i++) {
        int y = 285 + i * 58;
        if (i == sel) preencherRet(tela, 490, y - 8, 900, 48, 0xFF1E3A5F);
        desenharTexto(tela, op[i], 22, 520, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: COLOCAR   BOLINHA: VOLTAR", 18, 520, 848, 0xFFDCE6F3);
}

// Explorador de midia da TV: pastas, videos, imagens e musicas
// ---------------------------------------------------------------------------
enum { ITEM_PASTA = 0, ITEM_VIDEO, ITEM_IMAGEM, ITEM_MUSICA, ITEM_RADIO, ITEM_VOLTAR };
static const int MAX_ITENS_PICKER = 500;
struct ItemPicker { char nome[256]; char caminho[512]; int tipo; };
static ItemPicker itensPicker[MAX_ITENS_PICKER];
static int totalItensPicker = 0;
static int selPicker = 0;
static char pickerPath[512] = "";
static int pickerOrigem = 0;        // 0 = MIDIA, 1 = EXPLORAR
static int pickerMostrandoRaizes = 0;

static const char* RAIZES_NOMES[4] = { "HYPER NEIVA", "RAIZ", "USB0", "USB1" };
static const char* RAIZES_PATHS[4] = { "/data/HyperNeiva", "/", "/mnt/usb0", "/mnt/usb1" };
static const char* PASTA_MIDIA = "/data/HyperNeiva/midia";

static bool extEmLista(const char* nome, const char* const* lista, int n) {
    const char* p = strrchr(nome, '.');
    if (!p) return false;
    for (int i = 0; i < n; i++) if (strcasecmp(p, lista[i]) == 0) return true;
    return false;
}

static bool ehVideo(const char* nome) {
    static const char* ext[] = { ".mp4", ".m4v", ".mkv", ".avi", ".mov", ".webm", ".ts", ".flv",
        ".wmv", ".mpg", ".mpeg", ".3gp", ".m2ts", ".mts", ".vob", ".ogv", ".asf", ".f4v", ".divx" };
    return extEmLista(nome, ext, (int)(sizeof(ext) / sizeof(ext[0])));
}

static bool ehImagem(const char* nome) {
    static const char* ext[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tga" };
    return extEmLista(nome, ext, (int)(sizeof(ext) / sizeof(ext[0])));
}

static bool ehAudio(const char* nome) {
    static const char* ext[] = { ".mp3", ".wav", ".flac", ".ogg", ".oga", ".m4a", ".aac", ".wma", ".opus", ".mp2" };
    return extEmLista(nome, ext, (int)(sizeof(ext) / sizeof(ext[0])));
}

static int tipoDoArquivo(const char* nome) {
    if (ehVideo(nome)) return ITEM_VIDEO;
    if (ehImagem(nome)) return ITEM_IMAGEM;
    if (ehAudio(nome)) return ITEM_MUSICA;
    return -1;
}

static int pesoItem(int tipo) {
    if (tipo == ITEM_VOLTAR) return 0;
    if (tipo == ITEM_PASTA) return 1;
    if (tipo == ITEM_RADIO) return 2;
    return 3;
}

static bool itemMenor(const ItemPicker& a, const ItemPicker& b) {
    int pa = pesoItem(a.tipo), pb = pesoItem(b.tipo);
    if (pa != pb) return pa < pb;
    return strcasecmp(a.nome, b.nome) < 0;
}

static void adicionarItem(const char* nome, const char* caminho, int tipo) {
    if (totalItensPicker >= MAX_ITENS_PICKER) return;
    ItemPicker& it = itensPicker[totalItensPicker++];
    snprintf(it.nome, sizeof(it.nome), "%s", nome);
    snprintf(it.caminho, sizeof(it.caminho), "%s", caminho);
    it.tipo = tipo;
}

static void ordenarItensPicker() {
    for (int i = 1; i < totalItensPicker; i++) {
        ItemPicker chave = itensPicker[i];
        int j = i - 1;
        while (j >= 0 && itemMenor(chave, itensPicker[j])) { itensPicker[j + 1] = itensPicker[j]; j--; }
        itensPicker[j + 1] = chave;
    }
}

static void listarPicker(const char* caminho) {
    pickerMostrandoRaizes = 0;
    totalItensPicker = 0;
    selPicker = 0;
    snprintf(pickerPath, sizeof(pickerPath), "%s", caminho);
    if (strcmp(caminho, "/") != 0) adicionarItem("..  (VOLTAR)", caminho, ITEM_VOLTAR);
    if (strcmp(caminho, PASTA_MIDIA) == 0) adicionarItem("RADIO & PODCAST", caminho, ITEM_RADIO);
    DIR* d = opendir(caminho);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char completo[512];
            size_t tam = strlen(caminho);
            snprintf(completo, sizeof(completo), "%s%s%s", caminho, (tam > 0 && caminho[tam - 1] == '/') ? "" : "/", e->d_name);
            struct stat st;
            if (stat(completo, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) adicionarItem(e->d_name, completo, ITEM_PASTA);
            else {
                int t = tipoDoArquivo(e->d_name);
                if (t >= 0) adicionarItem(e->d_name, completo, t);
            }
        }
        closedir(d);
    }
    ordenarItensPicker();
}

static void mostrarRaizesPicker() {
    pickerMostrandoRaizes = 1;
    totalItensPicker = 0;
    selPicker = 0;
    pickerPath[0] = '\0';
    for (int i = 0; i < 4; i++) adicionarItem(RAIZES_NOMES[i], RAIZES_PATHS[i], ITEM_PASTA);
}

static bool ehRaizPadrao(const char* caminho) {
    for (int i = 0; i < 4; i++) if (strcmp(caminho, RAIZES_PATHS[i]) == 0) return true;
    return false;
}

static void caminhoPai(const char* caminho, char* saida, int tam) {
    snprintf(saida, tam, "%s", caminho);
    int n = (int)strlen(saida);
    while (n > 1 && saida[n - 1] == '/') saida[--n] = '\0';
    char* ultima = strrchr(saida, '/');
    if (!ultima || ultima == saida) snprintf(saida, tam, "/");
    else *ultima = '\0';
}

static const char* prefixoTipo(int tipo) {
    if (tipo == ITEM_PASTA) return "[PASTA]  ";
    if (tipo == ITEM_VIDEO) return "[VIDEO]  ";
    if (tipo == ITEM_IMAGEM) return "[IMAGEM] ";
    if (tipo == ITEM_MUSICA) return "[MUSICA] ";
    if (tipo == ITEM_RADIO) return "[RADIO]  ";
    return "";
}

static uint32_t corTipo(int tipo, bool sel) {
    if (sel) return 0xFFFFD166;
    if (tipo == ITEM_PASTA) return 0xFFFFFFFF;
    if (tipo == ITEM_VIDEO) return 0xFF8FD3FF;
    if (tipo == ITEM_IMAGEM) return 0xFF9EF0B5;
    if (tipo == ITEM_MUSICA) return 0xFFFFB07A;
    if (tipo == ITEM_RADIO) return 0xFFC9A7FF;
    return 0xFFA9BCD6;
}

static void desenharUIPicker(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    const char* titulo = pickerMostrandoRaizes ? "EXPLORAR - ONDE PROCURAR" :
        (pickerOrigem == 0 ? "MIDIA - PASTA DO APP" : "EXPLORAR - NAVEGAR NAS PASTAS");
    desenharTexto(tela, titulo, 34, 80, 55, 0xFFFFFFFF);
    if (!pickerMostrandoRaizes && pickerPath[0]) {
        char local[300];
        snprintf(local, sizeof(local), "%.250s", pickerPath);
        desenharTexto(tela, local, 18, 80, 105, 0xFFA9BCD6);
    }
    preencherRet(tela, 80, 140, TELA_L - 160, 4, 0xFF2E6DB4);
    if (totalItensPicker == 0) {
        desenharTexto(tela, "PASTA VAZIA OU SEM MIDIA COMPATIVEL", 28, 120, 300, 0xFFFF7A7A);
        desenharTexto(tela, "Aqui aparecem pastas, videos, imagens e musicas.", 22, 120, 360, 0xFFA9BCD6);
        desenharTexto(tela, "BOLINHA: VOLTAR", 22, 120, 1000, 0xFFDCE6F3);
        return;
    }
    int visiveis = 10;
    int inicio = selPicker - visiveis / 2;
    if (inicio < 0) inicio = 0;
    if (inicio > totalItensPicker - visiveis) inicio = totalItensPicker - visiveis;
    if (inicio < 0) inicio = 0;
    for (int i = 0; i < visiveis && inicio + i < totalItensPicker; i++) {
        int idx = inicio + i;
        int y = 190 + i * 72;
        bool sel = idx == selPicker;
        if (sel) preencherRet(tela, 80, y - 8, TELA_L - 160, 64, 0xFF1E3A5F);
        char linha[400];
        snprintf(linha, sizeof(linha), "%s%.180s", prefixoTipo(itensPicker[idx].tipo), itensPicker[idx].nome);
        desenharTexto(tela, linha, 24, 110, y, corTipo(itensPicker[idx].tipo, sel));
    }
    char rodape[64];
    snprintf(rodape, sizeof(rodape), "%d/%d", selPicker + 1, totalItensPicker);
    desenharTexto(tela, rodape, 19, 110, 950, 0xFFA9BCD6);
    const char* acao = "X: ENTRAR   BOLINHA: VOLTAR";
    int tipoSel = itensPicker[selPicker].tipo;
    if (tipoSel == ITEM_VIDEO || tipoSel == ITEM_IMAGEM) acao = "X: EXIBIR NA TV   BOLINHA: VOLTAR";
    else if (tipoSel == ITEM_MUSICA) acao = "X: TOCAR NA TV   BOLINHA: VOLTAR";
    else if (tipoSel == ITEM_RADIO) acao = "X: ABRIR RADIO   BOLINHA: VOLTAR";
    else if (tipoSel == ITEM_VOLTAR) acao = "X: VOLTAR   BOLINHA: VOLTAR";
    desenharTexto(tela, acao, 22, 110, 1000, 0xFFDCE6F3);
}

// ---------------------------------------------------------------------------
// Radio pela TV
// ---------------------------------------------------------------------------
static int radioSel = 0;
static int radioCategoria = 0;
static const char* RADIO_QUERIES[3] = { "topclick", "saopaulo", "podcast" };
static const char* RADIO_NOMES[3] = {
    "RADIOS POPULARES (TOP 50)",
    "RADIOS DE SAO PAULO",
    "PODCASTS"
};

static void desenharUIRadioMenu(uint32_t* tela, int sel) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    desenharTexto(tela, "RADIO & PODCAST", 36, 80, 60, 0xFFFFFFFF);
    desenharTexto(tela, "Escolha uma categoria", 22, 80, 115, 0xFFA9BCD6);
    for (int i = 0; i < 3; i++) {
        int y = 220 + i * 110;
        if (i == sel) preencherRet(tela, 80, y - 14, TELA_L - 160, 88, 0xFF1E3A5F);
        desenharTexto(tela, RADIO_NOMES[i], 30, 120, y, i == sel ? 0xFFFFD166 : 0xFFFFFFFF);
    }
    desenharTexto(tela, "CIMA/BAIXO: ESCOLHER   X: BUSCAR   BOLINHA: VOLTAR", 22, 80, 1000, 0xFFDCE6F3);
}

static void desenharUIRadioLista(uint32_t* tela, int sel) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    desenharTexto(tela, RADIO_NOMES[radioCategoria], 32, 80, 55, 0xFFFFFFFF);
    char sub[256];
    if (totalItens <= 1) snprintf(sub, sizeof(sub), "Carregando estacoes...");
    else snprintf(sub, sizeof(sub), "%d estacoes encontradas", totalItens);
    desenharTexto(tela, sub, 20, 80, 105, 0xFFA9BCD6);
    int visiveis = 9;
    if (totalItens > 0) {
        int inicio = sel - visiveis / 2;
        if (inicio < 0) inicio = 0;
        if (inicio > totalItens - visiveis) inicio = totalItens - visiveis;
        if (inicio < 0) inicio = 0;
        for (int i = 0; i < visiveis && inicio + i < totalItens; i++) {
            int idx = inicio + i;
            int y = 175 + i * 80;
            bool s = idx == sel;
            if (s) preencherRet(tela, 80, y - 10, TELA_L - 160, 68, 0xFF1E3A5F);
            char linha[300];
            snprintf(linha, sizeof(linha), "%.200s", nomes[idx]);
            desenharTexto(tela, linha, 24, 120, y, s ? 0xFFFFD166 : 0xFFFFFFFF);
        }
    }
    desenharTexto(tela, "X: TOCAR   BOLINHA: VOLTAR", 22, 80, 1000, 0xFFDCE6F3);
}

// ---------------------------------------------------------------------------
// Player de musica / radio em tela cheia
// ---------------------------------------------------------------------------
static char musicaTitulo[256] = "";
static bool musicaEhRadio = false;

static void desenharDisco(uint32_t* tela, int cx, int cy, int raio) {
    int r2 = raio * raio;
    for (int y = -raio; y <= raio; y++) {
        int py = cy + y;
        if (py < 0 || py >= TELA_A) continue;
        for (int x = -raio; x <= raio; x++) {
            int px = cx + x;
            if (px < 0 || px >= TELA_L) continue;
            int d2 = x * x + y * y;
            if (d2 > r2) continue;
            uint32_t cor = (((d2 * 6) / r2) & 1) ? 0xFF131C33 : 0xFF1E2C4F;
            if (d2 < r2 / 9) cor = 0xFFFFD166;
            tela[py * TELA_L + px] = cor;
        }
    }
}

static void desenharUIMusica(uint32_t* tela, const char* titulo, bool radio, bool pausado, int volume) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF070B14);
    desenharDisco(tela, 960, 420, 210);
    desenharTexto(tela, radio ? "AO VIVO NA TV" : "TOCANDO NA TV", 34, 120, 120, 0xFFFFFFFF);
    desenharTexto(tela, pausado ? "PAUSADO" : "REPRODUZINDO", 24, 120, 180, pausado ? 0xFFFF7A7A : 0xFF7BE08A);
    char linha[300];
    snprintf(linha, sizeof(linha), "%.140s", (titulo && titulo[0]) ? titulo : musicaAtual);
    desenharTexto(tela, linha, 30, 120, 730, 0xFFFFD166);
    snprintf(linha, sizeof(linha), "VOLUME %d", volume);
    desenharTexto(tela, linha, 20, 120, 710, 0xFFA9BCD6);
    desenharTexto(tela, radio ? "X: PAUSAR   CIMA/BAIXO: VOLUME   BOLINHA: VOLTAR   QUADRADO: PARAR"
                              : "X: PAUSAR   CIMA/BAIXO: VOLUME   ESQ/DIR: AVANCAR   BOLINHA: VOLTAR   QUADRADO: PARAR",
        20, 90, 1000, 0xFFDCE6F3);
}

// ---------------------------------------------------------------------------
// Visualizador de imagem em tela cheia
// ---------------------------------------------------------------------------
static void desenharUIImagem(uint32_t* tela, unsigned char* img, int w, int h, const char* nome) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF000000);
    if (img && w > 0 && h > 0) {
        float sx = (float)TELA_L / (float)w;
        float sy = (float)TELA_A / (float)h;
        float s = sx < sy ? sx : sy;
        int dw = (int)(w * s);
        int dh = (int)(h * s);
        int dx = (TELA_L - dw) / 2;
        int dy = (TELA_A - dh) / 2;
        desenharRedimensionado(tela, img, w, h, dw, dh, dx, dy);
    }
    preencherRet(tela, 0, 1010, TELA_L, 70, 0xCC0A1220);
    char linha[300];
    snprintf(linha, sizeof(linha), "IMAGEM NA TV  %.160s", nome ? nome : "");
    desenharTexto(tela, linha, 20, 40, 1032, 0xFFFFFFFF);
    desenharTexto(tela, "BOLINHA: VOLTAR   QUADRADO: DESLIGAR TV", 20, 1470, 1032, 0xFFDCE6F3);
}

// ---------------------------------------------------------------------------
// Conteudo estatico na tela da TV (imagem, musica e radio)
// ---------------------------------------------------------------------------
static uint32_t painelTV[1920 * 512];
static uint32_t tvAssinaturaAudio = 0;

static uint32_t misturarCor(uint32_t a, uint32_t b, float f) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    int r = ar + (int)((br - ar) * f);
    int g = ag + (int)((bg - ag) * f);
    int bl = ab + (int)((bb - ab) * f);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (bl < 0) bl = 0; if (bl > 255) bl = 255;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

static void tvGerarTexturaPainel() {
    for (int y = 0; y < TV_TEX_H; y++) {
        const uint32_t* l0 = painelTV + (size_t)(y * 2) * 1920;
        const uint32_t* l1 = l0 + 1920;
        uint32_t* destino = tvTextura + y * TV_TEX_W;
        for (int x = 0; x < TV_TEX_W; x++) {
            uint32_t p0 = l0[x * 2], p1 = l0[x * 2 + 1], p2 = l1[x * 2], p3 = l1[x * 2 + 1];
            int r = ((p0 >> 16 & 255) + (p1 >> 16 & 255) + (p2 >> 16 & 255) + (p3 >> 16 & 255)) / 4;
            int g = ((p0 >> 8 & 255) + (p1 >> 8 & 255) + (p2 >> 8 & 255) + (p3 >> 8 & 255)) / 4;
            int b = ((p0 & 255) + (p1 & 255) + (p2 & 255) + (p3 & 255)) / 4;
            destino[x] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
    tvTexturaAtiva = true;
}

static void tvPainelImagem(unsigned char* img, int w, int h) {
    for (int y = 0; y < 512; y++) {
        uint32_t* linha = painelTV + y * 1920;
        for (int x = 0; x < 1920; x++) linha[x] = 0xFF05080D;
    }
    if (img && w > 0 && h > 0) {
        float sx = (float)1920 / (float)w;
        float sy = (float)512 / (float)h;
        float s = sx < sy ? sx : sy;
        int dw = (int)(w * s);
        int dh = (int)(h * s);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        int dx = (1920 - dw) / 2;
        int dy = (512 - dh) / 2;
        desenharRedimensionado(painelTV, img, w, h, dw, dh, dx, dy);
    }
    tvGerarTexturaPainel();
}

static void tvPainelAudio(const char* titulo, bool radio, bool pausado, int volume) {
    uint32_t topo = 0xFF0E1830;
    uint32_t base = 0xFF2A1640;
    for (int y = 0; y < 512; y++) {
        uint32_t cor = misturarCor(topo, base, (float)y / 511.0f);
        uint32_t* linha = painelTV + y * 1920;
        for (int x = 0; x < 1920; x++) linha[x] = cor;
    }
    for (int x = 0; x < 1920; x++) painelTV[x] = 0xFFF1521B;
    desenharDisco(painelTV, 340, 256, 145);
    desenharTexto(painelTV, radio ? "RADIO AO VIVO NA TV" : "MUSICA NA TV", 30, 580, 105, 0xFFFFB07A);
    char linha[320];
    snprintf(linha, sizeof(linha), "%.62s", (titulo && titulo[0]) ? titulo : "SEM TITULO");
    desenharTexto(painelTV, linha, 56, 580, 160, 0xFFFFFFFF);
    desenharTexto(painelTV, pausado ? "PAUSADO" : "REPRODUZINDO", 30, 580, 260,
        pausado ? 0xFFFF7A7A : 0xFF7BE08A);
    int cheios = volume / 5;
    if (cheios < 0) cheios = 0;
    if (cheios > 20) cheios = 20;
    for (int i = 0; i < 20; i++) {
        uint32_t cor = i < cheios ? 0xFF7BE08A : 0xFF2A3550;
        for (int y = 0; y < 22; y++) {
            uint32_t* linhaP = painelTV + (360 + y) * 1920 + 580 + i * 56;
            for (int x = 0; x < 48; x++) linhaP[x] = cor;
        }
    }
    snprintf(linha, sizeof(linha), "VOLUME %d", volume);
    desenharTexto(painelTV, linha, 24, 580, 395, 0xFFA9BCD6);
    desenharTexto(painelTV, "TRIANGULO: CONTROLES   QUADRADO: PARAR", 24, 1180, 400, 0xFFDCE6F3);
    tvGerarTexturaPainel();
}

static uint32_t tvHashAudio(const char* titulo, bool radio, bool pausado, int volume) {
    uint32_t h = 2166136261u;
    const unsigned char* p = (const unsigned char*)(titulo ? titulo : "");
    while (*p) { h ^= (uint32_t)*p++; h *= 16777619u; }
    h ^= radio ? 0x9E3779B9u : 0u;
    h ^= pausado ? 0x85EBCA6Bu : 0u;
    h ^= (uint32_t)volume * 2654435761u;
    return h;
}

static void tvPararMidia() {
    tvMidia = TVMIDIA_NENHUMA;
    tvTexturaAtiva = false;
    tvAssinaturaAudio = 0;
}

// ---------------------------------------------------------------------------
// Retangulo da tela da TV em pixels da saida (para o video em miniatura)
// ---------------------------------------------------------------------------
static void esconderPipDaTV() {
    pip_x = -4000; pip_y = -4000; pip_w = 16; pip_h = 16;
}

static void atualizarPipDaTV(const Camera& cam) {
    Ponto cantos[4] = {
        { 1.45f, 1.25f, -19.76f },
        { 6.15f, 1.25f, -19.76f },
        { 6.15f, 2.50f, -19.76f },
        { 1.45f, 2.50f, -19.76f }
    };
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (int i = 0; i < 4; i++) {
        Ponto q;
        paraCamera(cantos[i], cam, q);
        if (q.z < 0.20f) { esconderPipDaTV(); return; }
        float sx, sy, invZ;
        projetar(q, cam, sx, sy, invZ);
        sx *= (float)ESCALA_TELA;
        sy *= (float)ESCALA_TELA;
        if (sx < minX) minX = sx;
        if (sx > maxX) maxX = sx;
        if (sy < minY) minY = sy;
        if (sy > maxY) maxY = sy;
    }
    if (maxX < -64.0f || minX > (float)TELA_L + 64.0f || maxY < -64.0f || minY > (float)TELA_A + 64.0f) {
        esconderPipDaTV();
        return;
    }
    int w = (int)(maxX - minX);
    int h = (int)(maxY - minY);
    if (w < 16) w = 16;
    if (h < 16) h = 16;
    pip_x = (int)minX;
    pip_y = (int)minY;
    pip_w = w;
    pip_h = h;
}

// ===========================================================================
// MESA DE JOGOS
// ===========================================================================
enum {
    JOGO_MENU = 0, JOGO_DAMA, JOGO_XADREZ, JOGO_TRUCO, JOGO_POKER, JOGO_DOMINO,
    JOGO_CARTAS_MENU,
    JOGO_21, JOGO_BISCA, JOGO_ESCOPA, JOGO_GUERRA, JOGO_PESCARIA, JOGO_MICO,
    JOGO_MAUMAU, JOGO_SUECA, JOGO_FODINHA,
    JOGO_BURACO, JOGO_CACHETA, JOGO_BIRIBA, JOGO_PIFPAF,
    JOGO_CORACOES, JOGO_ESPADAS,
    JOGO_PACIENCIA, JOGO_FREECELL,
    JOGO_PINGPONG, JOGO_SINUCA, JOGO_AIRHOCKEY, JOGO_PEBOLIM,
    JOGO_VOLEI, JOGO_TENIS, JOGO_FUTEBOL
};
static int jogoAtual = JOGO_MENU;
static int jogoMenuSel = 0;
static int jogoFisico = 0;
static int jogoSairCaminhar = 0;
static char jogoStatus[160] = "";
static unsigned int jogoRng = 12345u;

static int jogoRand() {
    jogoRng = jogoRng * 1103515245u + 12345u;
    return (int)((jogoRng >> 16) & 0x7FFF);
}

static void jogoEmbaralhar(int* v, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = jogoRand() % (i + 1);
        int t = v[i]; v[i] = v[j]; v[j] = t;
    }
}

static void jogoCirculo(uint32_t* tela, int cx, int cy, int raio, uint32_t cor) {
    if (raio < 1) return;
    int r2 = raio * raio;
    int y0 = cy - raio, y1 = cy + raio;
    if (y0 < 0) y0 = 0;
    if (y1 >= TELA_A) y1 = TELA_A - 1;
    for (int y = y0; y <= y1; y++) {
        int dy = y - cy;
        int dx2 = r2 - dy * dy;
        if (dx2 < 0) continue;
        int dx = (int)sqrtf((float)dx2);
        int x0 = cx - dx, x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 >= TELA_L) x1 = TELA_L - 1;
        uint32_t* linha = tela + y * TELA_L;
        for (int x = x0; x <= x1; x++) linha[x] = cor;
    }
}

static void jogoCentro(uint32_t* tela, const char* txt, int tam, int cx, int y, uint32_t cor) {
    int largura = (int)strlen(txt) * tam * 11 / 20;
    desenharTexto(tela, txt, tam, cx - largura / 2, y, cor);
}

static void jogoBorda(uint32_t* tela, int x, int y, int w, int h, uint32_t cor, int esp) {
    preencherRet(tela, x, y, w, esp, cor);
    preencherRet(tela, x, y + h - esp, w, esp, cor);
    preencherRet(tela, x, y, esp, h, cor);
    preencherRet(tela, x + w - esp, y, esp, h, cor);
}

static void jogoBarra(uint32_t* tela, const char* txt) {
    preencherRet(tela, 0, 1028, TELA_L, 52, 0xEE0A1220);
    desenharTexto(tela, txt, 22, 40, 1040, 0xFFDCE6F3);
}

static const char* JOGO_NOMES[4] = { "DAMA", "XADREZ", "JOGOS DE CARTAS", "DOMINO" };
static const char* JOGO_DESC[4] = {
    "Damas 8x8 contra a IA",
    "Xadrez completo contra a IA",
    "Truco, Poker, 21 e outros",
    "Domino de 6 contra a IA"
};
static const int JOGO_MENU_IDS[4] = { JOGO_DAMA, JOGO_XADREZ, JOGO_CARTAS_MENU, JOGO_DOMINO };

#define CARTAS_TOTAL 19
static const char* CARTAS_NOMES[CARTAS_TOTAL] = {
    "TRUCO", "POKER", "21 (BLACKJACK)", "BISCA", "ESCOPA", "GUERRA", "PESCARIA",
    "MICO", "MAU-MAU", "SUECA", "FODINHA", "BURACO", "CACHETA", "BIRIBA",
    "PIF-PAF", "CORACOES", "ESPADAS", "PACIENCIA", "FREECELL"
};
static const char* CARTAS_DESC[CARTAS_TOTAL] = {
    "Truco paulista com manilha", "Texas Hold'em contra a IA", "Compre ate 21 ou pare",
    "Bisca 1x1 com trunfo", "Capture cartas da mesa", "Maior carta leva",
    "Pescaria (Go Fish)", "Mico (Old Maid)", "Bata primeiro e use curingas",
    "Sueca simplificada 1x1", "Aposte quantas vazas faz", "Buraco simplificado 1x1",
    "Cacheta simplificada 1x1", "Biriba simplificada 1x1", "Pif-Paf simplificado 1x1",
    "Coracoes com 4 jogadores", "Espadas em 2 duplas", "Paciencia Klondike",
    "FreeCell"
};
static const int CARTAS_IDS[CARTAS_TOTAL] = {
    JOGO_TRUCO, JOGO_POKER, JOGO_21, JOGO_BISCA, JOGO_ESCOPA, JOGO_GUERRA, JOGO_PESCARIA,
    JOGO_MICO, JOGO_MAUMAU, JOGO_SUECA, JOGO_FODINHA, JOGO_BURACO, JOGO_CACHETA, JOGO_BIRIBA,
    JOGO_PIFPAF, JOGO_CORACOES, JOGO_ESPADAS, JOGO_PACIENCIA, JOGO_FREECELL
};
static int cartasMenuSel = 0;
static int cartasMenuTop = 0;
static char cartasAviso[64] = "";

static void desenharUIMenuJogos(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    jogoCentro(tela, "MESA DE JOGOS", 54, TELA_L / 2, 60, 0xFFFFFFFF);
    jogoCentro(tela, "ESCOLHA O JOGO", 24, TELA_L / 2, 132, 0xFFA9BCD6);
    for (int i = 0; i < 4; i++) {
        int y = 260 + i * 150;
        bool sel = i == jogoMenuSel;
        if (sel) preencherRet(tela, 480, y - 22, 960, 118, 0xFF1E3A5F);
        desenharTexto(tela, JOGO_NOMES[i], 42, 560, y, sel ? 0xFFFFD166 : 0xFFFFFFFF);
        desenharTexto(tela, JOGO_DESC[i], 22, 560, y + 60, sel ? 0xFFDCE6F3 : 0xFFA9BCD6);
    }
    jogoBarra(tela, "CIMA/BAIXO: ESCOLHER   X: JOGAR   BOLINHA: SAIR");
}

static void desenharUICartasMenu(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    jogoCentro(tela, "JOGOS DE CARTAS", 54, TELA_L / 2, 44, 0xFFFFFFFF);
    jogoCentro(tela, "TRUCO, POKER E 21 PRIMEIRO", 22, TELA_L / 2, 112, 0xFFA9BCD6);
    const int visiveis = 9;
    if (cartasMenuSel < cartasMenuTop) cartasMenuTop = cartasMenuSel;
    if (cartasMenuSel >= cartasMenuTop + visiveis) cartasMenuTop = cartasMenuSel - visiveis + 1;
    for (int i = 0; i < visiveis; i++) {
        int idx = cartasMenuTop + i;
        if (idx >= CARTAS_TOTAL) break;
        int y = 176 + i * 92;
        bool sel = idx == cartasMenuSel;
        if (sel) preencherRet(tela, 340, y - 16, 1240, 82, 0xFF1E3A5F);
        desenharTexto(tela, CARTAS_NOMES[idx], 34, 390, y, sel ? 0xFFFFD166 : 0xFFFFFFFF);
        desenharTexto(tela, CARTAS_DESC[idx], 20, 390, y + 42, sel ? 0xFFDCE6F3 : 0xFFA9BCD6);
    }
    if (cartasAviso[0]) jogoCentro(tela, cartasAviso, 24, TELA_L / 2, 1010, 0xFFFF8A80);
    jogoBarra(tela, "CIMA/BAIXO: ESCOLHER   X: JOGAR   BOLINHA: VOLTAR");
}

// ---------------------------------------------------------------------------
// DAMA
// ---------------------------------------------------------------------------
static int dama[64];
static int damaVez = 1;
static int damaSnap[64];
static bool damaSnapValido = false;
static int damaSnapR = 0, damaSnapC = 0;
static int damaCurR = 5, damaCurC = 0;
static int damaSelR = -1, damaSelC = -1;
static int damaCorrR = -1, damaCorrC = -1;
static unsigned char damaDest[64];
static unsigned char damaDestCap[64];
static float damaEspera = 0.0f;
static int damaFim = 0;

static bool damaDentro(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
static int damaDono(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
static bool damaRei(int v) { return v == 2 || v == -2; }

static void damaIniciar() {
    memset(dama, 0, sizeof(dama));
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 8; c++)
            if ((r + c) & 1) dama[r * 8 + c] = -1;
    for (int r = 5; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if ((r + c) & 1) dama[r * 8 + c] = 1;
    damaVez = 1; damaSelR = damaSelC = -1; damaCorrR = damaCorrC = -1;
    damaSnapValido = false; damaSnapR = damaSnapC = 0;
    damaFim = 0; damaEspera = 0.0f;
    damaCurR = 5; damaCurC = 0;
    for (int r = 0; r < 8 && damaCurR != 5; r++) {}
    bool achou = false;
    for (int r = 7; r >= 0 && !achou; r--)
        for (int c = 0; c < 8 && !achou; c++)
            if (damaDono(dama[r * 8 + c]) == 1) { damaCurR = r; damaCurC = c; achou = true; }
    snprintf(jogoStatus, sizeof(jogoStatus), "SUA VEZ");
}

static void damaMovsPeca(int r, int c, bool* cap, bool* sim) {
    for (int i = 0; i < 64; i++) { cap[i] = false; sim[i] = false; }
    int v = dama[r * 8 + c];
    int lado = damaDono(v);
    if (lado == 0) return;
    static const int dr[4] = { -1, -1, 1, 1 };
    static const int dc[4] = { -1, 1, -1, 1 };
    if (damaRei(v)) {
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (damaDentro(nr, nc) && dama[nr * 8 + nc] == 0) sim[nr * 8 + nc] = true;
        }
    } else {
        int nr = r - lado;
        int nc1 = c - 1, nc2 = c + 1;
        if (damaDentro(nr, nc1) && dama[nr * 8 + nc1] == 0) sim[nr * 8 + nc1] = true;
        if (damaDentro(nr, nc2) && dama[nr * 8 + nc2] == 0) sim[nr * 8 + nc2] = true;
    }
    for (int d = 0; d < 4; d++) {
        int er = r + dr[d], ec = c + dc[d];
        int lr = r + dr[d] * 2, lc = c + dc[d] * 2;
        if (!damaDentro(lr, lc) || !damaDentro(er, ec)) continue;
        if (damaDono(dama[er * 8 + ec]) != -lado) continue;
        if (dama[lr * 8 + lc] != 0) continue;
        cap[lr * 8 + lc] = true;
    }
}

static bool damaTemCaptura(int lado) {
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            if (damaDono(dama[r * 8 + c]) != lado) continue;
            bool cap[64], sim[64];
            damaMovsPeca(r, c, cap, sim);
            for (int i = 0; i < 64; i++) if (cap[i]) return true;
        }
    return false;
}

static void damaMontarDestinos() {
    for (int i = 0; i < 64; i++) { damaDest[i] = 0; damaDestCap[i] = 0; }
    if (damaSelR < 0) return;
    bool cap[64], sim[64];
    damaMovsPeca(damaSelR, damaSelC, cap, sim);
    bool obrig = damaTemCaptura(damaVez);
    for (int i = 0; i < 64; i++) {
        if (cap[i] || (!obrig && sim[i])) {
            damaDest[i] = 1;
            damaDestCap[i] = cap[i] ? 1 : 0;
        }
    }
}

static bool damaTemDestino() {
    for (int i = 0; i < 64; i++) if (damaDest[i]) return true;
    return false;
}

static void damaChecarFim();

static void damaExecutar(int r0, int c0, int r1, int c1, bool captura) {
    int v = dama[r0 * 8 + c0];
    int lado = damaDono(v);
    if (captura && lado == 1 && !damaSnapValido) {
        memcpy(damaSnap, dama, sizeof(dama));
        damaSnapValido = true;
        damaSnapR = r0; damaSnapC = c0;
    }
    dama[r1 * 8 + c1] = v;
    dama[r0 * 8 + c0] = 0;
    if (captura) dama[((r0 + r1) / 2) * 8 + (c0 + c1) / 2] = 0;
    if (!damaRei(v) && ((lado == 1 && r1 == 0) || (lado == -1 && r1 == 7))) dama[r1 * 8 + c1] = v * 2;
    bool cap2[64], sim2[64];
    damaMovsPeca(r1, c1, cap2, sim2);
    bool temCap2 = false;
    for (int i = 0; i < 64; i++) if (cap2[i]) temCap2 = true;
    damaSelR = damaSelC = -1;
    if (captura && temCap2) {
        damaCorrR = r1; damaCorrC = c1;
        if (damaVez == 1) {
            damaSelR = r1; damaSelC = c1;
            damaCurR = r1; damaCurC = c1;
            damaMontarDestinos();
            snprintf(jogoStatus, sizeof(jogoStatus), "CONTINUE CAPTURANDO");
        } else {
            damaEspera = 0.30f;
        }
        return;
    }
    damaCorrR = damaCorrC = -1;
    damaSnapValido = false;
    damaVez = -damaVez;
    damaChecarFim();
}

struct DamaJogada { int r0, c0, r1, c1; bool cap; };

static int damaGerarJogadasPeca(int r, int c, DamaJogada* out, int max) {
    bool cap[64], sim[64];
    damaMovsPeca(r, c, cap, sim);
    bool temCap = false;
    for (int i = 0; i < 64; i++) if (cap[i]) temCap = true;
    int n = 0;
    for (int i = 0; i < 64 && n < max; i++) {
        if ((temCap && cap[i]) || (!temCap && sim[i])) {
            out[n].r0 = r; out[n].c0 = c; out[n].r1 = i / 8; out[n].c1 = i % 8; out[n].cap = cap[i];
            n++;
        }
    }
    return n;
}

static int damaGerarJogadas(int lado, DamaJogada* out, int max) {
    bool temCap = damaTemCaptura(lado);
    int n = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            if (damaDono(dama[r * 8 + c]) != lado) continue;
            bool cap[64], sim[64];
            damaMovsPeca(r, c, cap, sim);
            for (int i = 0; i < 64 && n < max; i++) {
                if (cap[i] || (!temCap && sim[i])) {
                    out[n].r0 = r; out[n].c0 = c; out[n].r1 = i / 8; out[n].c1 = i % 8; out[n].cap = cap[i];
                    n++;
                }
            }
        }
    return n;
}

static void damaChecarFim() {
    int lado = damaVez;
    bool temPeca = false;
    for (int i = 0; i < 64; i++) if (damaDono(dama[i]) == lado) { temPeca = true; break; }
    if (!temPeca) {
        damaFim = -lado;
        snprintf(jogoStatus, sizeof(jogoStatus), lado == 1 ? "A IA VENCEU" : "VOCE VENCEU!");
        return;
    }
    bool temCap = false, temSim = false;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            if (damaDono(dama[r * 8 + c]) != lado) continue;
            bool cap[64], sim[64];
            damaMovsPeca(r, c, cap, sim);
            for (int i = 0; i < 64; i++) { if (cap[i]) temCap = true; if (sim[i]) temSim = true; }
        }
    if (!temCap && !temSim) {
        damaFim = -lado;
        snprintf(jogoStatus, sizeof(jogoStatus), lado == 1 ? "SEM JOGADAS - IA VENCEU" : "SEM JOGADAS - VOCE VENCEU!");
        return;
    }
    if (lado == -1) { damaEspera = 0.45f; snprintf(jogoStatus, sizeof(jogoStatus), "VEZ DA IA"); }
    else snprintf(jogoStatus, sizeof(jogoStatus), "SUA VEZ");
}

static void damaIA() {
    DamaJogada jogadas[256];
    int n;
    if (damaCorrR >= 0) n = damaGerarJogadasPeca(damaCorrR, damaCorrC, jogadas, 256);
    else n = damaGerarJogadas(-1, jogadas, 256);
    if (n <= 0) {
        damaFim = 1;
        snprintf(jogoStatus, sizeof(jogoStatus), "VOCE VENCEU!");
        return;
    }
    int melhor = 0, melhorPts = -1000000;
    for (int i = 0; i < n; i++) {
        int pts = jogadas[i].cap ? 500 : 0;
        pts += (jogadas[i].r1 - jogadas[i].r0) * 10;
        pts += 3 - abs(jogadas[i].c1 - 3);
        pts += jogoRand() % 7;
        if (pts > melhorPts) { melhorPts = pts; melhor = i; }
    }
    damaExecutar(jogadas[melhor].r0, jogadas[melhor].c0, jogadas[melhor].r1, jogadas[melhor].c1, jogadas[melhor].cap);
}

static void damaEntrada(uint32_t novos, float dt) {
    if (damaFim) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) damaIniciar();
        else if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_MENU;
        return;
    }
    if (damaVez == -1) {
        if (damaEspera > 0.0f) { damaEspera -= dt; return; }
        damaIA();
        return;
    }
    if (damaCorrR >= 0) {
        damaSelR = damaCorrR; damaSelC = damaCorrC;
        if (novos & ORBIS_PAD_BUTTON_UP) { if (damaCurR > 0) damaCurR--; }
        if (novos & ORBIS_PAD_BUTTON_DOWN) { if (damaCurR < 7) damaCurR++; }
        if (novos & ORBIS_PAD_BUTTON_LEFT) { if (damaCurC > 0) damaCurC--; }
        if (novos & ORBIS_PAD_BUTTON_RIGHT) { if (damaCurC < 7) damaCurC++; }
        damaMontarDestinos();
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            int idx = damaCurR * 8 + damaCurC;
            if (damaDest[idx]) {
                damaExecutar(damaSelR, damaSelC, damaCurR, damaCurC, damaDestCap[idx] != 0);
                return;
            }
        }
        if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
            if (damaSnapValido) memcpy(dama, damaSnap, sizeof(dama));
            damaSnapValido = false;
            damaCorrR = damaCorrC = -1;
            damaSelR = damaSelC = -1;
            damaCurR = damaSnapR; damaCurC = damaSnapC;
            snprintf(jogoStatus, sizeof(jogoStatus), "JOGADA CANCELADA");
        }
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_UP) { if (damaCurR > 0) damaCurR--; }
    if (novos & ORBIS_PAD_BUTTON_DOWN) { if (damaCurR < 7) damaCurR++; }
    if (novos & ORBIS_PAD_BUTTON_LEFT) { if (damaCurC > 0) damaCurC--; }
    if (novos & ORBIS_PAD_BUTTON_RIGHT) { if (damaCurC < 7) damaCurC++; }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        int idx = damaCurR * 8 + damaCurC;
        if (damaSelR >= 0 && damaDest[idx]) {
            damaExecutar(damaSelR, damaSelC, damaCurR, damaCurC, damaDestCap[idx] != 0);
        } else if (damaDono(dama[idx]) == 1) {
            damaSelR = damaCurR; damaSelC = damaCurC;
            damaMontarDestinos();
            if (!damaTemDestino()) {
                damaSelR = damaSelC = -1;
                snprintf(jogoStatus, sizeof(jogoStatus), "ESSA PECA NAO TEM JOGADA");
            }
        }
    }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
        if (damaSelR >= 0) { damaSelR = damaSelC = -1; snprintf(jogoStatus, sizeof(jogoStatus), "SUA VEZ"); }
        else jogoAtual = JOGO_MENU;
    }
}

static void damaDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    desenharTexto(tela, "DAMA", 44, 80, 40, 0xFFFFFFFF);
    desenharTexto(tela, jogoStatus, 26, 80, 100, 0xFFFFB07A);
    const int ox = 180, oy = 130, cel = 92;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            uint32_t cor = ((r + c) & 1) ? 0xFF6B4A2E : 0xFFE9D8B4;
            preencherRet(tela, ox + c * cel, oy + r * cel, cel, cel, cor);
        }
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            int v = dama[r * 8 + c];
            if (v == 0) continue;
            int cx = ox + c * cel + cel / 2;
            int cy = oy + r * cel + cel / 2;
            bool jog = v > 0;
            jogoCirculo(tela, cx, cy, cel / 2 - 8, jog ? 0xFFF7E7C1 : 0xFF2A2F3A);
            jogoCirculo(tela, cx, cy, cel / 2 - 14, jog ? 0xFFF2D7A0 : 0xFF1A1F28);
            if (damaRei(v)) jogoCirculo(tela, cx, cy, 9, 0xFFFFD166);
        }
    if (damaSelR >= 0) jogoBorda(tela, ox + damaSelC * cel, oy + damaSelR * cel, cel, cel, 0xFFFFD166, 5);
    if (damaVez == 1) {
        for (int i = 0; i < 64; i++) {
            if (!damaDest[i]) continue;
            int cx = ox + (i % 8) * cel + cel / 2;
            int cy = oy + (i / 8) * cel + cel / 2;
            jogoCirculo(tela, cx, cy, 13, damaDestCap[i] ? 0xFFFF7A7A : 0xFF7BE08A);
        }
    }
    jogoBorda(tela, ox + damaCurC * cel, oy + damaCurR * cel, cel, cel, 0xFF8FD3FF, 3);
    preencherRet(tela, 1080, 150, 760, 700, 0xEE0A1220);
    desenharTexto(tela, "COMO JOGAR", 28, 1120, 180, 0xFFA9BCD6);
    desenharTexto(tela, "DIRECIONAIS: MOVER O CURSOR", 21, 1120, 240, 0xFFFFFFFF);
    desenharTexto(tela, "X: ESCOLHER PECA / MOVER", 21, 1120, 280, 0xFFFFFFFF);
    desenharTexto(tela, "BOLINHA: CANCELAR / VOLTAR", 21, 1120, 320, 0xFFFFFFFF);
    desenharTexto(tela, "A CAPTURA E OBRIGATORIA", 21, 1120, 380, 0xFFFFB07A);
    desenharTexto(tela, "PEAO ANDA PARA FRENTE", 21, 1120, 420, 0xFFA9BCD6);
    desenharTexto(tela, "DAMA (OURO) ANDA PARA TODOS OS LADOS", 21, 1120, 460, 0xFFA9BCD6);
    if (damaFim) {
        jogoCentro(tela, jogoStatus, 44, 960, 760, 0xFFFFD166);
        jogoCentro(tela, "X: JOGAR DE NOVO     BOLINHA: MENU", 24, 960, 830, 0xFFDCE6F3);
    } else {
        jogoBarra(tela, damaVez == 1 ? "SUA VEZ   X: SELECIONAR/MOVER   BOLINHA: VOLTAR"
                                     : "VEZ DA IA...");
    }
}

// ---------------------------------------------------------------------------
// DOMINO
// ---------------------------------------------------------------------------
static int domJogador[28][2];
static int domTotalJ = 0;
static int domIA[28][2];
static int domTotalIA = 0;
static int domMesa[28][2];
static int domTotalMesa = 0;
static int domMonte[28][2];
static int domTotalMonte = 0;
static int domEsq = -1, domDir = -1;
static int domVez = 1;
static int domSel = 0;
static int domLadoSel = 0;
static int domPasses = 0;
static int domFim = 0;
static float domEspera = 0.0f;

static void domIniciar() {
    int idx[28];
    int n = 0;
    for (int a = 0; a <= 6; a++)
        for (int b = a; b <= 6; b++) idx[n++] = a * 10 + b;
    jogoEmbaralhar(idx, 28);
    domTotalJ = domTotalIA = domTotalMesa = domTotalMonte = 0;
    domEsq = domDir = -1;
    domVez = 1; domSel = 0; domLadoSel = 0; domPasses = 0; domFim = 0; domEspera = 0.0f;
    for (int i = 0; i < 7; i++) { domJogador[domTotalJ][0] = idx[i] / 10; domJogador[domTotalJ][1] = idx[i] % 10; domTotalJ++; }
    for (int i = 7; i < 14; i++) { domIA[domTotalIA][0] = idx[i] / 10; domIA[domTotalIA][1] = idx[i] % 10; domTotalIA++; }
    for (int i = 14; i < 28; i++) { domMonte[domTotalMonte][0] = idx[i] / 10; domMonte[domTotalMonte][1] = idx[i] % 10; domTotalMonte++; }
    snprintf(jogoStatus, sizeof(jogoStatus), "SUA VEZ - ESCOLHA UMA PECA");
}

static bool domEncaixa(int a, int b, int lado) {
    if (domTotalMesa == 0) return true;
    int alvo = (lado == 0) ? domEsq : domDir;
    return a == alvo || b == alvo;
}

static bool domTemJogada() {
    for (int i = 0; i < domTotalJ; i++)
        if (domEncaixa(domJogador[i][0], domJogador[i][1], 0) ||
            domEncaixa(domJogador[i][0], domJogador[i][1], 1)) return true;
    return false;
}

static void domColocar(int a, int b, int lado) {
    if (domTotalMesa == 0) {
        domMesa[0][0] = a; domMesa[0][1] = b;
        domTotalMesa = 1;
        domEsq = a; domDir = b;
        return;
    }
    int alvo = (lado == 0) ? domEsq : domDir;
    int outro = (a == alvo) ? b : a;
    if (lado == 0) {
        for (int i = domTotalMesa; i > 0; i--) { domMesa[i][0] = domMesa[i - 1][0]; domMesa[i][1] = domMesa[i - 1][1]; }
        domMesa[0][0] = outro; domMesa[0][1] = alvo;
        domTotalMesa++;
        domEsq = outro;
    } else {
        domMesa[domTotalMesa][0] = alvo; domMesa[domTotalMesa][1] = outro;
        domTotalMesa++;
        domDir = outro;
    }
}

static void domBloqueado() {
    int pj = 0, pi = 0;
    for (int i = 0; i < domTotalJ; i++) pj += domJogador[i][0] + domJogador[i][1];
    for (int i = 0; i < domTotalIA; i++) pi += domIA[i][0] + domIA[i][1];
    if (pj < pi) { domFim = 1; snprintf(jogoStatus, sizeof(jogoStatus), "JOGO FECHADO - VOCE VENCEU (%d x %d)", pj, pi); }
    else if (pi < pj) { domFim = -1; snprintf(jogoStatus, sizeof(jogoStatus), "JOGO FECHADO - IA VENCEU (%d x %d)", pj, pi); }
    else { domFim = 2; snprintf(jogoStatus, sizeof(jogoStatus), "JOGO FECHADO - EMPATE (%d x %d)", pj, pi); }
}

static void domJogarJogador() {
    if (domTotalJ <= 0) return;
    int a = domJogador[domSel][0], b = domJogador[domSel][1];
    int lado;
    if (domTotalMesa == 0) lado = 1;
    else {
        bool l = domEncaixa(a, b, 0), r = domEncaixa(a, b, 1);
        if (!l && !r) { snprintf(jogoStatus, sizeof(jogoStatus), "ESSA PECA NAO ENCAIXA"); return; }
        lado = (l && r) ? domLadoSel : (l ? 0 : 1);
    }
    domColocar(a, b, lado);
    for (int i = domSel; i < domTotalJ - 1; i++) { domJogador[i][0] = domJogador[i + 1][0]; domJogador[i][1] = domJogador[i + 1][1]; }
    domTotalJ--;
    if (domSel >= domTotalJ) domSel = domTotalJ > 0 ? domTotalJ - 1 : 0;
    domPasses = 0;
    if (domTotalJ == 0) { domFim = 1; snprintf(jogoStatus, sizeof(jogoStatus), "VOCE VENCEU!"); return; }
    domVez = -1; domEspera = 0.45f;
    snprintf(jogoStatus, sizeof(jogoStatus), "VEZ DA IA");
}

static void domJogadaIA() {
    int esc = -1, lado = 1, melhor = -1;
    for (int i = 0; i < domTotalIA; i++) {
        int a = domIA[i][0], b = domIA[i][1];
        bool l = (domTotalMesa == 0) ? true : domEncaixa(a, b, 0);
        bool r = (domTotalMesa == 0) ? true : domEncaixa(a, b, 1);
        if (l) { int pts = a + b; if (pts > melhor) { melhor = pts; esc = i; lado = 0; } }
        if (r) { int pts = a + b; if (pts > melhor) { melhor = pts; esc = i; lado = 1; } }
    }
    if (esc < 0) {
        if (domTotalMonte > 0) {
            domIA[domTotalIA][0] = domMonte[domTotalMonte - 1][0];
            domIA[domTotalIA][1] = domMonte[domTotalMonte - 1][1];
            domTotalIA++; domTotalMonte--;
            domEspera = 0.30f;
            return;
        }
        domPasses++;
        if (domPasses >= 2) { domBloqueado(); return; }
        domVez = 1;
        snprintf(jogoStatus, sizeof(jogoStatus), "IA PASSOU - SUA VEZ");
        return;
    }
    int a = domIA[esc][0], b = domIA[esc][1];
    domColocar(a, b, lado);
    for (int i = esc; i < domTotalIA - 1; i++) { domIA[i][0] = domIA[i + 1][0]; domIA[i][1] = domIA[i + 1][1]; }
    domTotalIA--;
    domPasses = 0;
    if (domTotalIA == 0) { domFim = -1; snprintf(jogoStatus, sizeof(jogoStatus), "A IA VENCEU"); return; }
    domVez = 1;
    snprintf(jogoStatus, sizeof(jogoStatus), "SUA VEZ");
}

static void dominoEntrada(uint32_t novos, float dt) {
    if (domFim) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) domIniciar();
        else if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_MENU;
        return;
    }
    if (domVez == -1) {
        if (domEspera > 0.0f) { domEspera -= dt; return; }
        domJogadaIA();
        return;
    }
    if (domTotalJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) domSel = (domSel + domTotalJ - 1) % domTotalJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) domSel = (domSel + 1) % domTotalJ;
        if (domTotalMesa > 0 && domEncaixa(domJogador[domSel][0], domJogador[domSel][1], 0) &&
            domEncaixa(domJogador[domSel][0], domJogador[domSel][1], 1) &&
            ((novos & ORBIS_PAD_BUTTON_UP) || (novos & ORBIS_PAD_BUTTON_DOWN))) {
            domLadoSel = 1 - domLadoSel;
        }
    }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        if (domTotalJ > 0 && (domTotalMesa == 0 || domEncaixa(domJogador[domSel][0], domJogador[domSel][1], 0) ||
            domEncaixa(domJogador[domSel][0], domJogador[domSel][1], 1))) {
            domJogarJogador();
        } else if (!domTemJogada()) {
            if (domTotalMonte > 0) {
                domJogador[domTotalJ][0] = domMonte[domTotalMonte - 1][0];
                domJogador[domTotalJ][1] = domMonte[domTotalMonte - 1][1];
                domTotalJ++; domTotalMonte--;
                snprintf(jogoStatus, sizeof(jogoStatus), "COMPROU UMA PECA");
            } else {
                domPasses++;
                if (domPasses >= 2) domBloqueado();
                else { domVez = -1; domEspera = 0.40f; snprintf(jogoStatus, sizeof(jogoStatus), "VOCE PASSOU"); }
            }
        } else {
            snprintf(jogoStatus, sizeof(jogoStatus), "PECA NAO ENCAIXA - ESCOLHA OUTRA");
        }
    }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_MENU;
}

static void desenharPecaDomino(uint32_t* tela, int x, int y, int w, int h, int a, int b, bool sel) {
    preencherRet(tela, x, y, w, h, 0xFFF6F1E6);
    jogoBorda(tela, x, y, w, h, sel ? 0xFFFFD166 : 0xFF222833, sel ? 6 : 2);
    int mx = x + w / 2;
    preencherRet(tela, mx - 1, y + 5, 2, h - 10, 0xFF8A8F99);
    char t[8];
    snprintf(t, sizeof(t), "%d", a);
    jogoCentro(tela, t, h / 2, x + w / 4, y + h / 4, 0xFF1A1A1A);
    snprintf(t, sizeof(t), "%d", b);
    jogoCentro(tela, t, h / 2, x + w * 3 / 4, y + h / 4, 0xFF1A1A1A);
}

static void dominoDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    desenharTexto(tela, "DOMINO", 44, 80, 40, 0xFFFFFFFF);
    desenharTexto(tela, jogoStatus, 26, 80, 100, 0xFFFFB07A);
    char info[160];
    snprintf(info, sizeof(info), "MONTE: %d   IA: %d PECAS   PONTAS: ESQ %d  DIR %d",
        domTotalMonte, domTotalIA, domEsq < 0 ? 0 : domEsq, domDir < 0 ? 0 : domDir);
    desenharTexto(tela, info, 21, 1100, 60, 0xFFA9BCD6);
    if (domTotalMesa == 0) {
        jogoCentro(tela, "MESA VAZIA - ESCOLHA A PRIMEIRA PECA", 28, 960, 360, 0xFFDCE6F3);
    } else {
        int dw = 62, dh = 44, passo = 66;
        int largura = domTotalMesa * passo;
        int x0 = (TELA_L - largura) / 2;
        if (x0 < 20) x0 = 20;
        int y = 360;
        for (int i = 0; i < domTotalMesa; i++)
            desenharPecaDomino(tela, x0 + i * passo, y, dw, dh, domMesa[i][0], domMesa[i][1], false);
    }
    for (int i = 0; i < domTotalJ; i++) {
        int col = i % 7, lin = i / 7;
        int x = 512 + col * 128;
        int y = 760 + lin * 66;
        desenharPecaDomino(tela, x, y, 120, 56, domJogador[i][0], domJogador[i][1], i == domSel);
    }
    if (domTotalMesa > 0 && domTotalJ > 0 &&
        domEncaixa(domJogador[domSel][0], domJogador[domSel][1], 0) &&
        domEncaixa(domJogador[domSel][0], domJogador[domSel][1], 1)) {
        desenharTexto(tela, domLadoSel ? "LADO: DIREITO (CIMA/BAIXO TROCA)" : "LADO: ESQUERDO (CIMA/BAIXO TROCA)",
            22, 1100, 500, 0xFFFFD166);
    }
    if (domFim) {
        jogoCentro(tela, "X: JOGAR DE NOVO     BOLINHA: MENU", 26, 960, 680, 0xFFDCE6F3);
    } else {
        jogoBarra(tela, "ESQ/DIR: ESCOLHER PECA   X: JOGAR/COMPRAR   CIMA/BAIXO: LADO   BOLINHA: MENU");
    }
}

// ---------------------------------------------------------------------------
// XADREZ
// ---------------------------------------------------------------------------
enum { XP_PEAO = 1, XP_CAVALO, XP_BISPO, XP_TORRE, XP_DAMA, XP_REI };

struct XadPos {
    int b[64];
    int cast[4];
    int ep;
};

struct XadJogada {
    int r0, c0, r1, c1;
    int promo;
    bool ep;
    bool cast;
};

static XadPos xadPos;
static int xadVez = 1;
static int xadCurR = 6, xadCurC = 4;
static int xadSelR = -1, xadSelC = -1;
static unsigned char xadDest[64];
static float xadEspera = 0.0f;
static int xadFim = 0;

static bool xadDentro(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
static int xadDono(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
static int xadTipo(int v) { return v < 0 ? -v : v; }

static void xadIniciar() {
    memset(&xadPos, 0, sizeof(xadPos));
    int ini[8] = { XP_TORRE, XP_CAVALO, XP_BISPO, XP_DAMA, XP_REI, XP_BISPO, XP_CAVALO, XP_TORRE };
    for (int c = 0; c < 8; c++) {
        xadPos.b[0 * 8 + c] = -ini[c];
        xadPos.b[1 * 8 + c] = -XP_PEAO;
        xadPos.b[6 * 8 + c] = XP_PEAO;
        xadPos.b[7 * 8 + c] = ini[c];
    }
    xadPos.cast[0] = xadPos.cast[1] = xadPos.cast[2] = xadPos.cast[3] = 1;
    xadPos.ep = -1;
    xadVez = 1; xadSelR = xadSelC = -1;
    xadFim = 0; xadEspera = 0.0f;
    xadCurR = 6; xadCurC = 4;
    snprintf(jogoStatus, sizeof(jogoStatus), "SUA VEZ - BRANCAS");
}

static bool xadAtacada(const XadPos& p, int r, int c, int por) {
    int dir = (por == 1) ? 1 : -1;
    for (int dc = -1; dc <= 1; dc += 2) {
        int rr = r + dir, cc = c + dc;
        if (xadDentro(rr, cc) && xadDono(p.b[rr * 8 + cc]) == por && xadTipo(p.b[rr * 8 + cc]) == XP_PEAO) return true;
    }
    static const int nd[8][2] = { { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 }, { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } };
    for (int i = 0; i < 8; i++) {
        int rr = r + nd[i][0], cc = c + nd[i][1];
        if (xadDentro(rr, cc) && xadDono(p.b[rr * 8 + cc]) == por && xadTipo(p.b[rr * 8 + cc]) == XP_CAVALO) return true;
    }
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
            if (!dr && !dc) continue;
            int rr = r + dr, cc = c + dc;
            if (xadDentro(rr, cc) && xadDono(p.b[rr * 8 + cc]) == por && xadTipo(p.b[rr * 8 + cc]) == XP_REI) return true;
        }
    static const int rd[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    for (int i = 0; i < 4; i++) {
        int rr = r + rd[i][0], cc = c + rd[i][1];
        while (xadDentro(rr, cc)) {
            int v = p.b[rr * 8 + cc];
            if (v != 0) {
                if (xadDono(v) == por) { int t = xadTipo(v); if (t == XP_TORRE || t == XP_DAMA) return true; }
                break;
            }
            rr += rd[i][0]; cc += rd[i][1];
        }
    }
    static const int bd[4][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
    for (int i = 0; i < 4; i++) {
        int rr = r + bd[i][0], cc = c + bd[i][1];
        while (xadDentro(rr, cc)) {
            int v = p.b[rr * 8 + cc];
            if (v != 0) {
                if (xadDono(v) == por) { int t = xadTipo(v); if (t == XP_BISPO || t == XP_DAMA) return true; }
                break;
            }
            rr += bd[i][0]; cc += bd[i][1];
        }
    }
    return false;
}

static int xadAcharRei(const XadPos& p, int lado) {
    for (int i = 0; i < 64; i++) if (xadDono(p.b[i]) == lado && xadTipo(p.b[i]) == XP_REI) return i;
    return -1;
}

static bool xadEmXeque(const XadPos& p, int lado) {
    int k = xadAcharRei(p, lado);
    if (k < 0) return true;
    return xadAtacada(p, k / 8, k % 8, -lado);
}

static void xadAplicar(XadPos& p, const XadJogada& m) {
    int v = p.b[m.r0 * 8 + m.c0];
    int lado = xadDono(v);
    int t = xadTipo(v);
    p.ep = -1;
    p.b[m.r1 * 8 + m.c1] = m.promo ? (lado * m.promo) : v;
    p.b[m.r0 * 8 + m.c0] = 0;
    if (m.ep) p.b[m.r0 * 8 + m.c1] = 0;
    if (m.cast) {
        if (m.c1 == 6) { p.b[m.r1 * 8 + 5] = p.b[m.r1 * 8 + 7]; p.b[m.r1 * 8 + 7] = 0; }
        else { p.b[m.r1 * 8 + 3] = p.b[m.r1 * 8 + 0]; p.b[m.r1 * 8 + 0] = 0; }
    }
    if (t == XP_PEAO && abs(m.r1 - m.r0) == 2) p.ep = ((m.r0 + m.r1) / 2) * 8 + m.c1;
    if (t == XP_REI) {
        if (lado == 1) { p.cast[0] = p.cast[1] = 0; }
        else { p.cast[2] = p.cast[3] = 0; }
    }
    if (t == XP_TORRE) {
        if (lado == 1 && m.r0 == 7 && m.c0 == 7) p.cast[0] = 0;
        if (lado == 1 && m.r0 == 7 && m.c0 == 0) p.cast[1] = 0;
        if (lado == -1 && m.r0 == 0 && m.c0 == 7) p.cast[2] = 0;
        if (lado == -1 && m.r0 == 0 && m.c0 == 0) p.cast[3] = 0;
    }
    if (m.r1 == 7 && m.c1 == 7) p.cast[0] = 0;
    if (m.r1 == 7 && m.c1 == 0) p.cast[1] = 0;
    if (m.r1 == 0 && m.c1 == 7) p.cast[2] = 0;
    if (m.r1 == 0 && m.c1 == 0) p.cast[3] = 0;
}

static void xadAddMov(XadJogada* out, int* n, int max, int r0, int c0, int r1, int c1, int promo, bool ep, bool cast) {
    if (*n >= max) return;
    out[*n].r0 = r0; out[*n].c0 = c0; out[*n].r1 = r1; out[*n].c1 = c1;
    out[*n].promo = promo; out[*n].ep = ep; out[*n].cast = cast;
    (*n)++;
}

static int xadGerarPseudo(const XadPos& p, int lado, XadJogada* out, int max) {
    int n = 0;
    static const int nd[8][2] = { { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 }, { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } };
    static const int dirs[8][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            int v = p.b[r * 8 + c];
            if (xadDono(v) != lado) continue;
            int t = xadTipo(v);
            if (t == XP_PEAO) {
                int dir = (lado == 1) ? -1 : 1;
                int startRow = (lado == 1) ? 6 : 1;
                int lastRow = (lado == 1) ? 0 : 7;
                int nr = r + dir;
                if (xadDentro(nr, c) && p.b[nr * 8 + c] == 0) {
                    if (nr == lastRow) xadAddMov(out, &n, max, r, c, nr, c, XP_DAMA, false, false);
                    else {
                        xadAddMov(out, &n, max, r, c, nr, c, 0, false, false);
                        int nr2 = r + dir * 2;
                        if (r == startRow && p.b[nr2 * 8 + c] == 0) xadAddMov(out, &n, max, r, c, nr2, c, 0, false, false);
                    }
                }
                for (int dc = -1; dc <= 1; dc += 2) {
                    int cc = c + dc;
                    if (!xadDentro(nr, cc)) continue;
                    int alvo = p.b[nr * 8 + cc];
                    if (alvo != 0 && xadDono(alvo) == -lado) {
                        if (nr == lastRow) xadAddMov(out, &n, max, r, c, nr, cc, XP_DAMA, false, false);
                        else xadAddMov(out, &n, max, r, c, nr, cc, 0, false, false);
                    } else if (alvo == 0 && p.ep == nr * 8 + cc) {
                        xadAddMov(out, &n, max, r, c, nr, cc, 0, true, false);
                    }
                }
            } else if (t == XP_CAVALO) {
                for (int i = 0; i < 8; i++) {
                    int rr = r + nd[i][0], cc = c + nd[i][1];
                    if (!xadDentro(rr, cc)) continue;
                    if (xadDono(p.b[rr * 8 + cc]) == lado) continue;
                    xadAddMov(out, &n, max, r, c, rr, cc, 0, false, false);
                }
            } else if (t == XP_BISPO || t == XP_TORRE || t == XP_DAMA) {
                int ini = (t == XP_BISPO) ? 4 : 0;
                int fim = (t == XP_TORRE) ? 4 : 8;
                for (int d = ini; d < fim; d++) {
                    int rr = r + dirs[d][0], cc = c + dirs[d][1];
                    while (xadDentro(rr, cc)) {
                        int alvo = p.b[rr * 8 + cc];
                        if (alvo != 0) {
                            if (xadDono(alvo) != lado) xadAddMov(out, &n, max, r, c, rr, cc, 0, false, false);
                            break;
                        }
                        xadAddMov(out, &n, max, r, c, rr, cc, 0, false, false);
                        rr += dirs[d][0]; cc += dirs[d][1];
                    }
                }
            } else if (t == XP_REI) {
                for (int d = 0; d < 8; d++) {
                    int rr = r + dirs[d][0], cc = c + dirs[d][1];
                    if (!xadDentro(rr, cc)) continue;
                    if (xadDono(p.b[rr * 8 + cc]) == lado) continue;
                    xadAddMov(out, &n, max, r, c, rr, cc, 0, false, false);
                }
                int home = (lado == 1) ? 7 : 0;
                int iniCast = (lado == 1) ? 0 : 2;
                if (r == home && c == 4 && !xadAtacada(p, home, 4, -lado)) {
                    if (p.cast[iniCast] && p.b[home * 8 + 7] == lado * XP_TORRE &&
                        p.b[home * 8 + 5] == 0 && p.b[home * 8 + 6] == 0 &&
                        !xadAtacada(p, home, 5, -lado) && !xadAtacada(p, home, 6, -lado)) {
                        xadAddMov(out, &n, max, r, c, home, 6, 0, false, true);
                    }
                    if (p.cast[iniCast + 1] && p.b[home * 8 + 0] == lado * XP_TORRE &&
                        p.b[home * 8 + 1] == 0 && p.b[home * 8 + 2] == 0 && p.b[home * 8 + 3] == 0 &&
                        !xadAtacada(p, home, 3, -lado) && !xadAtacada(p, home, 2, -lado)) {
                        xadAddMov(out, &n, max, r, c, home, 2, 0, false, true);
                    }
                }
            }
        }
    return n;
}

static int xadGerarLegais(const XadPos& p, int lado, XadJogada* out, int max) {
    XadJogada pseudo[320];
    int n = xadGerarPseudo(p, lado, pseudo, 320);
    int m = 0;
    for (int i = 0; i < n && m < max; i++) {
        XadPos q = p;
        xadAplicar(q, pseudo[i]);
        if (!xadEmXeque(q, lado)) out[m++] = pseudo[i];
    }
    return m;
}

static int xadValorPeca(int t) {
    switch (t) {
        case XP_PEAO: return 100;
        case XP_CAVALO: return 320;
        case XP_BISPO: return 330;
        case XP_TORRE: return 500;
        case XP_DAMA: return 900;
        case XP_REI: return 20000;
    }
    return 0;
}

static int xadAvaliar(const XadPos& p, int lado) {
    int s = 0;
    for (int i = 0; i < 64; i++) {
        int v = p.b[i];
        if (!v) continue;
        s += xadDono(v) == lado ? xadValorPeca(xadTipo(v)) : -xadValorPeca(xadTipo(v));
    }
    return s;
}

static int xadNegamax(const XadPos& p, int lado, int prof, int alpha, int beta) {
    XadJogada movs[128];
    int n = xadGerarLegais(p, lado, movs, 128);
    if (n == 0) return xadEmXeque(p, lado) ? (-100000 - prof) : 0;
    if (prof <= 0) return xadAvaliar(p, lado);
    int melhor = -1000000;
    for (int i = 0; i < n; i++) {
        XadPos q = p;
        xadAplicar(q, movs[i]);
        int v = -xadNegamax(q, -lado, prof - 1, -beta, -alpha);
        if (v > melhor) melhor = v;
        if (melhor > alpha) alpha = melhor;
        if (alpha >= beta) break;
    }
    return melhor;
}

static void xadChecarFim() {
    XadJogada movs[128];
    int n = xadGerarLegais(xadPos, xadVez, movs, 128);
    bool xeque = xadEmXeque(xadPos, xadVez);
    if (n == 0) {
        if (xeque) {
            xadFim = -xadVez;
            snprintf(jogoStatus, sizeof(jogoStatus), xadVez == 1 ? "XEQUE-MATE - IA VENCEU" : "XEQUE-MATE - VOCE VENCEU!");
        } else {
            xadFim = 2;
            snprintf(jogoStatus, sizeof(jogoStatus), "EMPATE POR AFOGAMENTO");
        }
        return;
    }
    if (xeque) snprintf(jogoStatus, sizeof(jogoStatus), xadVez == 1 ? "XEQUE! SUA VEZ" : "XEQUE NA IA");
    else snprintf(jogoStatus, sizeof(jogoStatus), xadVez == 1 ? "SUA VEZ" : "VEZ DA IA");
}

static void xadIA() {
    XadJogada movs[128];
    int n = xadGerarLegais(xadPos, -1, movs, 128);
    if (n <= 0) { xadVez = 1; xadChecarFim(); return; }
    int melhor = 0, melhorVal = -1000000;
    for (int i = 0; i < n; i++) {
        XadPos q = xadPos;
        xadAplicar(q, movs[i]);
        int v = -xadNegamax(q, 1, 2, -1000000, 1000000);
        v += jogoRand() % 7;
        if (v > melhorVal) { melhorVal = v; melhor = i; }
    }
    xadAplicar(xadPos, movs[melhor]);
    xadVez = 1;
    xadChecarFim();
}

static void xadEntrada(uint32_t novos, float dt) {
    if (xadFim) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) xadIniciar();
        else if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_MENU;
        return;
    }
    if (xadVez == -1) {
        if (xadEspera > 0.0f) { xadEspera -= dt; return; }
        xadIA();
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_UP) { if (xadCurR > 0) xadCurR--; }
    if (novos & ORBIS_PAD_BUTTON_DOWN) { if (xadCurR < 7) xadCurR++; }
    if (novos & ORBIS_PAD_BUTTON_LEFT) { if (xadCurC > 0) xadCurC--; }
    if (novos & ORBIS_PAD_BUTTON_RIGHT) { if (xadCurC < 7) xadCurC++; }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        int idx = xadCurR * 8 + xadCurC;
        XadJogada movs[128];
        int n = xadGerarLegais(xadPos, 1, movs, 128);
        if (xadSelR >= 0 && xadDest[idx]) {
            int achou = -1;
            for (int i = 0; i < n; i++)
                if (movs[i].r0 == xadSelR && movs[i].c0 == xadSelC && movs[i].r1 == xadCurR && movs[i].c1 == xadCurC) { achou = i; break; }
            if (achou >= 0) {
                xadAplicar(xadPos, movs[achou]);
                xadSelR = xadSelC = -1;
                memset(xadDest, 0, sizeof(xadDest));
                xadVez = -1;
                xadEspera = 0.45f;
                xadChecarFim();
            }
        } else if (xadDono(xadPos.b[idx]) == 1) {
            xadSelR = xadCurR; xadSelC = xadCurC;
            memset(xadDest, 0, sizeof(xadDest));
            bool tem = false;
            for (int i = 0; i < n; i++)
                if (movs[i].r0 == xadCurR && movs[i].c0 == xadCurC) { xadDest[movs[i].r1 * 8 + movs[i].c1] = 1; tem = true; }
            if (!tem) {
                xadSelR = xadSelC = -1;
                snprintf(jogoStatus, sizeof(jogoStatus), "ESSA PECA NAO TEM JOGADA");
            } else {
                snprintf(jogoStatus, sizeof(jogoStatus), "ESCOLHA O DESTINO");
            }
        }
    }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
        if (xadSelR >= 0) { xadSelR = xadSelC = -1; memset(xadDest, 0, sizeof(xadDest)); xadChecarFim(); }
        else jogoAtual = JOGO_MENU;
    }
}

static void xadDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A1220);
    desenharTexto(tela, "XADREZ", 44, 80, 40, 0xFFFFFFFF);
    desenharTexto(tela, jogoStatus, 26, 80, 100, 0xFFFFB07A);
    const int ox = 180, oy = 130, cel = 92;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            uint32_t cor = ((r + c) & 1) ? 0xFF6B4A2E : 0xFFE9D8B4;
            preencherRet(tela, ox + c * cel, oy + r * cel, cel, cel, cor);
        }
    static const char LETRAS[7] = { ' ', 'P', 'C', 'B', 'T', 'D', 'R' };
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            int v = xadPos.b[r * 8 + c];
            if (v == 0) continue;
            int cx = ox + c * cel + cel / 2;
            int cy = oy + r * cel + cel / 2;
            bool branca = v > 0;
            jogoCirculo(tela, cx, cy, cel / 2 - 8, branca ? 0xFFF7E7C1 : 0xFF2A2F3A);
            jogoCirculo(tela, cx, cy, cel / 2 - 14, branca ? 0xFFF2D7A0 : 0xFF1A1F28);
            char t[2] = { LETRAS[xadTipo(v)], 0 };
            jogoCentro(tela, t, 36, cx, cy - 18, branca ? 0xFF1A1F28 : 0xFFF7E7C1);
        }
    if (xadSelR >= 0) jogoBorda(tela, ox + xadSelC * cel, oy + xadSelR * cel, cel, cel, 0xFFFFD166, 5);
    if (xadVez == 1) {
        for (int i = 0; i < 64; i++) {
            if (!xadDest[i]) continue;
            int cx = ox + (i % 8) * cel + cel / 2;
            int cy = oy + (i / 8) * cel + cel / 2;
            bool captura = xadDono(xadPos.b[i]) == -1;
            jogoCirculo(tela, cx, cy, 13, captura ? 0xFFFF7A7A : 0xFF7BE08A);
        }
    }
    jogoBorda(tela, ox + xadCurC * cel, oy + xadCurR * cel, cel, cel, 0xFF8FD3FF, 3);
    preencherRet(tela, 1080, 150, 760, 700, 0xEE0A1220);
    desenharTexto(tela, "COMO JOGAR", 28, 1120, 180, 0xFFA9BCD6);
    desenharTexto(tela, "DIRECIONAIS: MOVER O CURSOR", 21, 1120, 240, 0xFFFFFFFF);
    desenharTexto(tela, "X: ESCOLHER PECA / MOVER", 21, 1120, 280, 0xFFFFFFFF);
    desenharTexto(tela, "BOLINHA: CANCELAR / VOLTAR", 21, 1120, 320, 0xFFFFFFFF);
    desenharTexto(tela, "P PEAO   C CAVALO   B BISPO", 21, 1120, 380, 0xFFA9BCD6);
    desenharTexto(tela, "T TORRE   D DAMA   R REI", 21, 1120, 420, 0xFFA9BCD6);
    desenharTexto(tela, "PECA CLARA: VOCE", 21, 1120, 480, 0xFFF2D7A0);
    desenharTexto(tela, "PECA ESCURA: IA", 21, 1120, 520, 0xFFA9BCD6);
    desenharTexto(tela, "PEAO CHEGA AO FIM = DAMA", 21, 1120, 580, 0xFFFFB07A);
    desenharTexto(tela, "ROQUE E EN PASSANT VALEM", 21, 1120, 620, 0xFFFFB07A);
    if (xadFim) {
        jogoCentro(tela, jogoStatus, 40, 960, 760, 0xFFFFD166);
        jogoCentro(tela, "X: JOGAR DE NOVO     BOLINHA: MENU", 24, 960, 830, 0xFFDCE6F3);
    } else {
        jogoBarra(tela, xadVez == 1 ? "SUA VEZ   X: SELECIONAR/MOVER   BOLINHA: VOLTAR" : "VEZ DA IA...");
    }
}

// ---------------------------------------------------------------------------
// TRUCO PAULISTA
// ---------------------------------------------------------------------------
static const int TR_RANKS[10] = { 1, 2, 3, 4, 5, 6, 7, 10, 11, 12 };

static int trucoRank[3], trucoNaipe[3];
static int trucoIARank[3], trucoIANaipe[3];
static int trucoNJ = 3, trucoNIA = 3;
static int trucoSel = 0;
static int trucoValor = 1;
static int trucoPtsJ = 0, trucoPtsIA = 0;
static int trucoTricksJ = 0, trucoTricksIA = 0;
static int trucoTrick = 0;
static int trucoMao = 1;
static int trucoCartaJ = -1, trucoCartaIA = -1;
static int trucoVezJ = 1;
static float trucoEspera = 0.0f;
static float trucoEsperaRodada = 0.0f;
static bool trucoPediuIA = false;
static int trucoPendente = 0;
static int trucoQuemPediu = 0;
static int trucoAguardaJ = 0;
static bool trucoFechado = false;
static int trucoFim = 0;
static int trucoVira = -1;
static int trucoManilhaRank = 0;

static int trucoRankSeguinte(int r) {
    switch (r) {
        case 4: return 5;
        case 5: return 6;
        case 6: return 7;
        case 7: return 10;
        case 10: return 11;
        case 11: return 12;
        case 12: return 1;
        case 1: return 2;
        case 2: return 3;
        case 3: return 4;
    }
    return 4;
}

static int trucoForca(int rank, int naipe) {
    if (rank == trucoManilhaRank) {
        if (naipe == 0) return 14;
        if (naipe == 2) return 13;
        if (naipe == 3) return 12;
        return 11;
    }
    switch (rank) {
        case 4: return 1;
        case 5: return 2;
        case 6: return 3;
        case 7: return 4;
        case 10: return 5;
        case 11: return 6;
        case 12: return 7;
        case 1: return 8;
        case 2: return 9;
        case 3: return 10;
    }
    return 0;
}

static int trucoMelhorForca(bool jogador) {
    int m = 0;
    if (jogador) { for (int i = 0; i < trucoNJ; i++) { int f = trucoForca(trucoRank[i], trucoNaipe[i]); if (f > m) m = f; } }
    else { for (int i = 0; i < trucoNIA; i++) { int f = trucoForca(trucoIARank[i], trucoIANaipe[i]); if (f > m) m = f; } }
    return m;
}

static int trucoValorSeguinte(int v) {
    if (v == 1) return 3;
    if (v == 3) return 6;
    if (v == 6) return 9;
    return 12;
}

static void trucoNovaRodada() {
    int deck[40];
    int n = 0;
    for (int ri = 0; ri < 10; ri++)
        for (int s = 0; s < 4; s++) deck[n++] = TR_RANKS[ri] * 4 + s;
    jogoEmbaralhar(deck, 40);
    trucoNJ = trucoNIA = 3;
    for (int i = 0; i < 3; i++) {
        trucoRank[i] = deck[i] / 4; trucoNaipe[i] = deck[i] % 4;
        trucoIARank[i] = deck[3 + i] / 4; trucoIANaipe[i] = deck[3 + i] % 4;
    }
    trucoVira = deck[6];
    trucoManilhaRank = trucoRankSeguinte(trucoVira / 4);
    trucoTricksJ = trucoTricksIA = 0;
    trucoTrick = 0;
    trucoCartaJ = trucoCartaIA = -1;
    trucoVezJ = (trucoMao == 1);
    trucoSel = 0;
    trucoValor = 1;
    trucoPediuIA = false;
    trucoPendente = 0;
    trucoQuemPediu = 0;
    trucoAguardaJ = 0;
    trucoFechado = false;
    trucoEspera = 0.0f;
    trucoEsperaRodada = 0.0f;
    snprintf(jogoStatus, sizeof(jogoStatus), trucoMao == 1 ? "SUA VEZ - ESCOLHA UMA CARTA" : "VEZ DA IA");
}

static void trucoIniciar() {
    trucoPtsJ = 0;
    trucoPtsIA = 0;
    trucoFim = 0;
    trucoMao = 1;
    trucoNovaRodada();
}

static void trucoEncerrarRodada(int vencedor, const char* msg) {
    if (vencedor == 1) {
        trucoPtsJ += trucoValor;
        snprintf(jogoStatus, sizeof(jogoStatus), "%s (+%d) - VOCE %d x %d IA", msg, trucoValor, trucoPtsJ, trucoPtsIA);
    } else if (vencedor == -1) {
        trucoPtsIA += trucoValor;
        snprintf(jogoStatus, sizeof(jogoStatus), "%s (+%d) - VOCE %d x %d IA", msg, trucoValor, trucoPtsJ, trucoPtsIA);
    } else {
        snprintf(jogoStatus, sizeof(jogoStatus), "%s - VOCE %d x %d IA", msg, trucoPtsJ, trucoPtsIA);
    }
    trucoMao = -trucoMao;
    if (trucoPtsJ >= 12) {
        trucoFim = 1;
        snprintf(jogoStatus, sizeof(jogoStatus), "VOCE VENCEU O JOGO %d x %d!", trucoPtsJ, trucoPtsIA);
        return;
    }
    if (trucoPtsIA >= 12) {
        trucoFim = -1;
        snprintf(jogoStatus, sizeof(jogoStatus), "IA VENCEU O JOGO %d x %d", trucoPtsJ, trucoPtsIA);
        return;
    }
    trucoEsperaRodada = 1.6f;
}

static void trucoJogarJogador() {
    if (!trucoVezJ || trucoNJ <= 0) return;
    trucoCartaJ = trucoRank[trucoSel] * 4 + trucoNaipe[trucoSel];
    for (int i = trucoSel; i < trucoNJ - 1; i++) { trucoRank[i] = trucoRank[i + 1]; trucoNaipe[i] = trucoNaipe[i + 1]; }
    trucoNJ--;
    if (trucoSel >= trucoNJ) trucoSel = trucoNJ > 0 ? trucoNJ - 1 : 0;
    trucoVezJ = false;
    trucoEspera = 0.45f;
}

static void trucoJogarIA() {
    if (trucoNIA <= 0) return;
    int esc = 0;
    if (trucoCartaJ >= 0) {
        int forcaJ = trucoForca(trucoCartaJ / 4, trucoCartaJ % 4);
        int melhor = -1;
        for (int i = 0; i < trucoNIA; i++) {
            int f = trucoForca(trucoIARank[i], trucoIANaipe[i]);
            if (f > forcaJ && (melhor < 0 || f < trucoForca(trucoIARank[melhor], trucoIANaipe[melhor]))) melhor = i;
        }
        if (melhor < 0) {
            melhor = 0;
            for (int i = 1; i < trucoNIA; i++) if (trucoForca(trucoIARank[i], trucoIANaipe[i]) < trucoForca(trucoIARank[melhor], trucoIANaipe[melhor])) melhor = i;
        }
        esc = melhor;
    } else {
        esc = jogoRand() % trucoNIA;
    }
    trucoCartaIA = trucoIARank[esc] * 4 + trucoIANaipe[esc];
    for (int i = esc; i < trucoNIA - 1; i++) { trucoIARank[i] = trucoIARank[i + 1]; trucoIANaipe[i] = trucoIANaipe[i + 1]; }
    trucoNIA--;
    trucoVezJ = true;
}

static void trucoResolverTrick() {
    int fJ = trucoForca(trucoCartaJ / 4, trucoCartaJ % 4);
    int fIA = trucoForca(trucoCartaIA / 4, trucoCartaIA % 4);
    trucoTrick++;
    if (fJ > fIA) { trucoTricksJ++; trucoMao = 1; }
    else if (fIA > fJ) { trucoTricksIA++; trucoMao = -1; }
    trucoCartaJ = trucoCartaIA = -1;
    trucoVezJ = (trucoMao == 1);
    trucoEspera = 0.5f;
    if (trucoTricksJ >= 2 || trucoTricksIA >= 2 || trucoTrick >= 3) {
        if (trucoTricksJ > trucoTricksIA) trucoEncerrarRodada(1, "VOCE FEZ A RODADA");
        else if (trucoTricksIA > trucoTricksJ) trucoEncerrarRodada(-1, "IA FEZ A RODADA");
        else trucoEncerrarRodada(0, "RODADA EMPATADA");
    } else {
        snprintf(jogoStatus, sizeof(jogoStatus), "TRUCO %d/3 - VOCE %d x %d IA", trucoTrick, trucoTricksJ, trucoTricksIA);
    }
}

static bool trucoIAQuerPedir() {
    int f = trucoMelhorForca(false);
    int r = jogoRand() % 100;
    if (f >= 11) return true;
    if (f >= 8) return r < 70;
    if (f >= 5) return r < 30;
    return r < 12;
}

static int trucoIAResposta(int P) {
    int f = trucoMelhorForca(false);
    int r = jogoRand() % 100;
    bool podeAumentar = P < 12;
    if (f >= 11) {
        if (podeAumentar && r < 60) return 2;
        return 1;
    }
    if (f >= 8) {
        if (podeAumentar && r < 35) return 2;
        if (r < 43) return 0;
        return 1;
    }
    if (f >= 5) {
        if (podeAumentar && r < 10) return 2;
        if (r < 40) return 0;
        return 1;
    }
    if (podeAumentar && r < 5) return 2;
    if (r < 65) return 0;
    return 1;
}

static void trucoResolverPropostaJogador() {
    int P = trucoPendente;
    int resp = trucoIAResposta(P);
    if (resp == 0) {
        trucoValor = P;
        trucoPendente = 0; trucoQuemPediu = 0;
        trucoEncerrarRodada(1, "IA CORREU DO TRUCO");
        return;
    }
    if (resp == 2) {
        int N = trucoValorSeguinte(P);
        trucoPendente = N;
        trucoQuemPediu = -1;
        trucoAguardaJ = 1;
        snprintf(jogoStatus, sizeof(jogoStatus), "IA AUMENTOU PARA %d - X ACEITAR  QUADRADO CORRER  TRIANGULO AUMENTAR", N);
        return;
    }
    trucoValor = P;
    trucoPendente = 0; trucoQuemPediu = 0;
    trucoFechado = true;
    trucoEspera = 0.4f;
    snprintf(jogoStatus, sizeof(jogoStatus), "IA ACEITOU - AGORA VALE %d", trucoValor);
}

static void trucoResponderJogador(uint32_t novos) {
    int P = trucoPendente;
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        trucoValor = P;
        trucoPendente = 0; trucoQuemPediu = 0; trucoAguardaJ = 0;
        trucoFechado = true;
        trucoEspera = 0.4f;
        snprintf(jogoStatus, sizeof(jogoStatus), "VOCE ACEITOU - VALE %d", trucoValor);
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_SQUARE) {
        trucoValor = P;
        trucoPendente = 0; trucoQuemPediu = 0; trucoAguardaJ = 0;
        trucoEncerrarRodada(-1, "VOCE CORREU DO TRUCO");
        return;
    }
    if ((novos & ORBIS_PAD_BUTTON_TRIANGLE) && P < 12) {
        int N = trucoValorSeguinte(P);
        trucoPendente = N;
        trucoQuemPediu = 1;
        snprintf(jogoStatus, sizeof(jogoStatus), "VOCE AUMENTOU PARA %d", N);
        trucoResolverPropostaJogador();
        return;
    }
}

static void trucoEntrada(uint32_t novos, float dt) {
    if (trucoFim) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) trucoIniciar();
        else if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_CARTAS_MENU;
        return;
    }
    if (trucoEsperaRodada > 0.0f) {
        trucoEsperaRodada -= dt;
        if (trucoEsperaRodada <= 0.0f) trucoNovaRodada();
        return;
    }
    if (trucoAguardaJ) {
        trucoResponderJogador(novos);
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (trucoNJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) trucoSel = (trucoSel + trucoNJ - 1) % trucoNJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) trucoSel = (trucoSel + 1) % trucoNJ;
    }
    if (trucoVezJ) {
        if ((novos & ORBIS_PAD_BUTTON_TRIANGLE) && !trucoFechado && trucoValor < 12 && trucoNJ > 0 && trucoNIA > 0) {
            trucoPendente = trucoValorSeguinte(trucoValor);
            trucoQuemPediu = 1;
            trucoResolverPropostaJogador();
            if (trucoAguardaJ || trucoEsperaRodada > 0.0f || trucoFim) return;
        }
        if (novos & ORBIS_PAD_BUTTON_CROSS) trucoJogarJogador();
    }
    if (trucoCartaJ >= 0 && trucoCartaIA >= 0) { trucoResolverTrick(); return; }
    if (!trucoVezJ) {
        if (trucoEspera > 0.0f) { trucoEspera -= dt; return; }
        if (!trucoPediuIA && !trucoFechado && trucoValor < 12 && trucoIAQuerPedir()) {
            trucoPediuIA = true;
            trucoPendente = trucoValorSeguinte(trucoValor);
            trucoQuemPediu = -1;
            trucoAguardaJ = 1;
            snprintf(jogoStatus, sizeof(jogoStatus), "IA PEDIU TRUCO (VALE %d) - X ACEITAR  QUADRADO CORRER  TRIANGULO AUMENTAR", trucoPendente);
            return;
        }
        trucoJogarIA();
    }
    if (trucoCartaJ >= 0 && trucoCartaIA >= 0) trucoResolverTrick();
}

static const char* trucoNomeRank(int r) {
    if (r == 1) return "A";
    if (r == 10) return "Q";
    if (r == 11) return "J";
    if (r == 12) return "K";
    static char buf[4];
    snprintf(buf, sizeof(buf), "%d", r);
    return buf;
}

static const char* trucoNomeNaipe(int n) {
    static const char* nomes[4] = { "P", "O", "C", "E" };
    return nomes[n & 3];
}

static void trucoDesenharCarta(uint32_t* tela, int x, int y, int w, int h, int code, bool sel, bool virada) {
    preencherRet(tela, x, y, w, h, 0xFFF6F1E6);
    jogoBorda(tela, x, y, w, h, sel ? 0xFFFFD166 : 0xFF222833, sel ? 6 : 2);
    if (virada) {
        preencherRet(tela, x + 10, y + 10, w - 20, h - 20, 0xFF2E4A7A);
        jogoCentro(tela, "?", 44, x + w / 2, y + h / 2 - 28, 0xFFDCE6F3);
        return;
    }
    if (code < 0) return;
    char t[8];
    snprintf(t, sizeof(t), "%s%s", trucoNomeRank(code / 4), trucoNomeNaipe(code % 4));
    desenharTexto(tela, t, 26, x + 12, y + 10, 0xFF1A1A1A);
    desenharTexto(tela, t, 26, x + w - 66, y + h - 46, 0xFF1A1A1A);
}

static void trucoDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A2A18);
    desenharTexto(tela, "TRUCO PAULISTA", 44, 80, 40, 0xFFFFFFFF);
    char placar[96];
    if (trucoPendente > 0)
        snprintf(placar, sizeof(placar), "VOCE %d   x   %d IA     PROPOSTA %s: %d", trucoPtsJ, trucoPtsIA, trucoQuemPediu == 1 ? "SUA" : "DA IA", trucoPendente);
    else
        snprintf(placar, sizeof(placar), "VOCE %d   x   %d IA     VALE: %d", trucoPtsJ, trucoPtsIA, trucoValor);
    desenharTexto(tela, placar, 28, 1140, 50, 0xFFFFD166);
    desenharTexto(tela, jogoStatus, 24, 80, 100, 0xFFFFB07A);
    if (trucoVira >= 0) {
        jogoCentro(tela, "VIRA", 18, 210, 392, 0xFFA9BCD6);
        trucoDesenharCarta(tela, 150, 430, 120, 165, trucoVira, false, false);
        char mf[48];
        snprintf(mf, sizeof(mf), "MANILHA: %s", trucoNomeRank(trucoManilhaRank));
        desenharTexto(tela, mf, 20, 280, 430, 0xFFFFD166);
        desenharTexto(tela, "MANILHAS: ZAP > COPAS > ESPADAS > PICAFUMO", 18, 280, 468, 0xFFDCE6F3);
        desenharTexto(tela, "ORDEM: MANILHA > 3 2 A K J Q 7 6 5 4", 18, 280, 502, 0xFFFFFFFF);
    }
    jogoCentro(tela, "IA", 24, 960, 180, 0xFFA9BCD6);
    int wIA = 110, hIA = 150;
    for (int i = 0; i < trucoNIA; i++)
        trucoDesenharCarta(tela, 820 + i * 145, 205, wIA, hIA, -1, false, true);
    jogoCentro(tela, "MESA", 22, 960, 400, 0xFFA9BCD6);
    if (trucoCartaJ >= 0) trucoDesenharCarta(tela, 780, 430, 140, 190, trucoCartaJ, false, false);
    if (trucoCartaIA >= 0) trucoDesenharCarta(tela, 1000, 430, 140, 190, trucoCartaIA, false, false);
    jogoCentro(tela, "SUAS CARTAS", 24, 960, 700, 0xFFA9BCD6);
    int w = 160, h = 220;
    int total = trucoNJ * 180;
    int x0 = 960 - total / 2 + 10;
    for (int i = 0; i < trucoNJ; i++) {
        int x = x0 + i * 180;
        int y = 745 - (i == trucoSel ? 24 : 0);
        trucoDesenharCarta(tela, x, y, w, h, trucoRank[i] * 4 + trucoNaipe[i], i == trucoSel, false);
    }
    preencherRet(tela, 1080, 300, 760, 300, 0xCC061A10);
    desenharTexto(tela, "COMO JOGAR", 26, 1120, 330, 0xFFA9BCD6);
    desenharTexto(tela, "ESQ/DIR: ESCOLHER CARTA", 20, 1120, 390, 0xFFFFFFFF);
    desenharTexto(tela, "X: JOGAR CARTA", 20, 1120, 430, 0xFFFFFFFF);
    desenharTexto(tela, "TRIANGULO: PEDIR TRUCO", 20, 1120, 470, 0xFFFFD166);
    desenharTexto(tela, "BOLINHA: SAIR PARA O MENU", 20, 1120, 510, 0xFFFFFFFF);
    desenharTexto(tela, "ORDEM (FORTE>FRACA): 3 2 A K J Q 7 6 5 4", 20, 1120, 560, 0xFFA9BCD6);
    if (trucoFim) {
        jogoCentro(tela, jogoStatus, 38, 960, 520, 0xFFFFD166);
        jogoCentro(tela, "X: NOVO JOGO     BOLINHA: MENU", 24, 960, 600, 0xFFDCE6F3);
    } else if (trucoAguardaJ) {
        jogoBarra(tela, "X ACEITAR   QUADRADO CORRER   TRIANGULO AUMENTAR");
    } else {
        jogoBarra(tela, trucoVezJ ? "SUA VEZ   ESQ/DIR: CARTA   X: JOGAR   TRIANGULO: TRUCO   BOLINHA: SAIR"
                                  : "VEZ DA IA...");
    }
}

// ---------------------------------------------------------------------------
// POKER TEXAS HOLD'EM (HEADS-UP)
// ---------------------------------------------------------------------------
struct PokerCarta { int v; int n; };

static PokerCarta pokerBaralho[52];
static int pokerTopo = 0;
static PokerCarta pokerJ[2], pokerIA[2];
static PokerCarta pokerMesa[5];
static int pokerNMesa = 0;
static int pokerEtapa = 0;
static int pokerFichasJ = 1000, pokerFichasIA = 1000;
static int pokerPote = 0;
static int pokerApostaJ = 0, pokerApostaIA = 0;
static bool pokerVezJ = true;
static bool pokerAgiuJ = false, pokerAgiuIA = false;
static bool pokerRevelar = false;
static bool pokerMaoEncerrada = false;
static float pokerEspera = 0.0f;
static float pokerEsperaMao = 0.0f;
static int pokerFim = 0;
static int pokerMaoNum = 0;
static char pokerMsg[160] = "";

static void pokerNovoBaralho() {
    int n = 0;
    for (int v = 2; v <= 14; v++)
        for (int s = 0; s < 4; s++) { pokerBaralho[n].v = v; pokerBaralho[n].n = s; n++; }
    for (int i = 51; i > 0; i--) {
        int j = jogoRand() % (i + 1);
        PokerCarta t = pokerBaralho[i]; pokerBaralho[i] = pokerBaralho[j]; pokerBaralho[j] = t;
    }
    pokerTopo = 0;
}

static PokerCarta pokerComprar() { return pokerBaralho[pokerTopo++]; }

static int pokerScore5(const PokerCarta* c, int* catOut) {
    int v[5], s[5];
    for (int i = 0; i < 5; i++) { v[i] = c[i].v; s[i] = c[i].n; }
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 5; j++)
            if (v[j] > v[i]) {
                int t = v[i]; v[i] = v[j]; v[j] = t;
                t = s[i]; s[i] = s[j]; s[j] = t;
            }
    bool flush = (s[0] == s[1] && s[1] == s[2] && s[2] == s[3] && s[3] == s[4]);
    bool straight = false;
    int straightHigh = 0;
    if (v[0] == v[1] + 1 && v[1] == v[2] + 1 && v[2] == v[3] + 1 && v[3] == v[4] + 1) { straight = true; straightHigh = v[0]; }
    if (v[0] == 14 && v[1] == 5 && v[2] == 4 && v[3] == 3 && v[4] == 2) { straight = true; straightHigh = 5; }
    int rank[15] = { 0 };
    for (int i = 0; i < 5; i++) rank[v[i]]++;
    int gr[5], gc[5], ng = 0;
    for (int r = 14; r >= 2; r--) if (rank[r]) { gr[ng] = r; gc[ng] = rank[r]; ng++; }
    for (int i = 0; i < ng; i++)
        for (int j = i + 1; j < ng; j++)
            if (gc[j] > gc[i] || (gc[j] == gc[i] && gr[j] > gr[i])) {
                int t = gr[i]; gr[i] = gr[j]; gr[j] = t;
                t = gc[i]; gc[i] = gc[j]; gc[j] = t;
            }
    int tie[5] = { 0, 0, 0, 0, 0 };
    int cat;
    if (straight && flush) { cat = 8; tie[0] = straightHigh; }
    else if (gc[0] == 4) { cat = 7; tie[0] = gr[0]; tie[1] = gr[1]; }
    else if (gc[0] == 3 && gc[1] == 2) { cat = 6; tie[0] = gr[0]; tie[1] = gr[1]; }
    else if (flush) { cat = 5; for (int i = 0; i < 5; i++) tie[i] = v[i]; }
    else if (straight) { cat = 4; tie[0] = straightHigh; }
    else if (gc[0] == 3) { cat = 3; tie[0] = gr[0]; tie[1] = gr[1]; tie[2] = gr[2]; }
    else if (gc[0] == 2 && gc[1] == 2) { cat = 2; tie[0] = gr[0]; tie[1] = gr[1]; tie[2] = gr[2]; }
    else if (gc[0] == 2) { cat = 1; tie[0] = gr[0]; tie[1] = gr[1]; tie[2] = gr[2]; tie[3] = gr[3]; }
    else { cat = 0; for (int i = 0; i < 5; i++) tie[i] = v[i]; }
    int score = cat;
    for (int i = 0; i < 5; i++) score = score * 15 + tie[i];
    if (catOut) *catOut = cat;
    return score;
}

static int pokerMelhor7(const PokerCarta* sete, int* catOut) {
    int melhor = -1, catM = 0;
    for (int a = 0; a < 3; a++)
        for (int b = a + 1; b < 4; b++)
            for (int c = b + 1; c < 5; c++)
                for (int d = c + 1; d < 6; d++)
                    for (int e = d + 1; e < 7; e++) {
                        PokerCarta m[5] = { sete[a], sete[b], sete[c], sete[d], sete[e] };
                        int cat = 0;
                        int s = pokerScore5(m, &cat);
                        if (s > melhor) { melhor = s; catM = cat; }
                    }
    if (catOut) *catOut = catM;
    return melhor;
}

static const char* pokerNomeCat(int cat) {
    static const char* nomes[9] = { "CARTA ALTA", "PAR", "DOIS PARES", "TRINCA", "SEQUENCIA",
                                    "FLUSH", "FULL HOUSE", "QUADRA", "STRAIGHT FLUSH" };
    if (cat < 0 || cat > 8) return "";
    return nomes[cat];
}

static int pokerPagar(int quem, int alvo) {
    int* f = (quem == 1) ? &pokerFichasJ : &pokerFichasIA;
    int* a = (quem == 1) ? &pokerApostaJ : &pokerApostaIA;
    int diff = alvo - *a;
    if (diff <= 0) return 0;
    if (diff > *f) diff = *f;
    *a += diff;
    *f -= diff;
    pokerPote += diff;
    return diff;
}

static void pokerShowdownFinal() {
    while (pokerNMesa < 5) pokerMesa[pokerNMesa++] = pokerComprar();
    PokerCarta seteJ[7], seteIA[7];
    for (int i = 0; i < 2; i++) { seteJ[i] = pokerJ[i]; seteIA[i] = pokerIA[i]; }
    for (int i = 0; i < 5; i++) { seteJ[2 + i] = pokerMesa[i]; seteIA[2 + i] = pokerMesa[i]; }
    int catJ = 0, catIA = 0;
    int sJ = pokerMelhor7(seteJ, &catJ);
    int sIA = pokerMelhor7(seteIA, &catIA);
    pokerRevelar = true;
    pokerMaoEncerrada = true;
    if (sJ > sIA) {
        pokerFichasJ += pokerPote;
        snprintf(pokerMsg, sizeof(pokerMsg), "VOCE VENCEU (%s) - POTE %d", pokerNomeCat(catJ), pokerPote);
    } else if (sIA > sJ) {
        pokerFichasIA += pokerPote;
        snprintf(pokerMsg, sizeof(pokerMsg), "IA VENCEU (%s) - POTE %d", pokerNomeCat(catIA), pokerPote);
    } else {
        int metade = pokerPote / 2;
        pokerFichasJ += metade;
        pokerFichasIA += pokerPote - metade;
        snprintf(pokerMsg, sizeof(pokerMsg), "EMPATE (%s) - POTE DIVIDIDO", pokerNomeCat(catJ));
    }
    pokerPote = 0;
    pokerEsperaMao = 2.4f;
}

static void pokerProximaEtapa() {
    if (pokerEtapa >= 3) { pokerShowdownFinal(); return; }
    pokerEtapa++;
    pokerApostaJ = pokerApostaIA = 0;
    pokerAgiuJ = pokerAgiuIA = false;
    if (pokerEtapa == 1) {
        pokerMesa[0] = pokerComprar(); pokerMesa[1] = pokerComprar(); pokerMesa[2] = pokerComprar();
        pokerNMesa = 3;
    } else {
        pokerMesa[pokerNMesa++] = pokerComprar();
    }
    pokerVezJ = false;
    pokerEspera = 0.6f;
    snprintf(pokerMsg, sizeof(pokerMsg), pokerEtapa == 1 ? "FLOP - VEZ DA IA" : (pokerEtapa == 2 ? "TURN - VEZ DA IA" : "RIVER - VEZ DA IA"));
}

static void pokerFazerAcao(int quem, int tipo) {
    if (pokerMaoEncerrada || pokerFim) return;
    int alvoOp = (quem == 1) ? pokerApostaIA : pokerApostaJ;
    int* aposta = (quem == 1) ? &pokerApostaJ : &pokerApostaIA;
    if (tipo == 2) {
        int venc = -quem;
        int* f = (venc == 1) ? &pokerFichasJ : &pokerFichasIA;
        *f += pokerPote;
        snprintf(pokerMsg, sizeof(pokerMsg), "%s FOLDEOU - %s LEVA %d", quem == 1 ? "VOCE" : "IA", venc == 1 ? "VOCE" : "IA", pokerPote);
        pokerPote = 0;
        pokerMaoEncerrada = true;
        pokerEsperaMao = 1.8f;
        return;
    }
    if (tipo == 1) {
        pokerPagar(quem, alvoOp + 40);
        if (quem == 1) { pokerAgiuJ = true; pokerAgiuIA = false; }
        else { pokerAgiuIA = true; pokerAgiuJ = false; }
        pokerVezJ = (quem == -1);
        snprintf(pokerMsg, sizeof(pokerMsg), "%s AUMENTOU PARA %d", quem == 1 ? "VOCE" : "IA", (quem == 1 ? pokerApostaJ : pokerApostaIA));
        if (pokerFichasJ <= 0 || pokerFichasIA <= 0) pokerShowdownFinal();
        return;
    }
    bool pagou = (*aposta < alvoOp);
    if (pagou) pokerPagar(quem, alvoOp);
    if (quem == 1) pokerAgiuJ = true;
    else pokerAgiuIA = true;
    if (pokerApostaJ == pokerApostaIA && pokerAgiuJ && pokerAgiuIA) { pokerProximaEtapa(); return; }
    if (pokerFichasJ <= 0 || pokerFichasIA <= 0) { pokerShowdownFinal(); return; }
    pokerVezJ = (quem == -1);
    snprintf(pokerMsg, sizeof(pokerMsg), "%s: %s", quem == 1 ? "VOCE" : "IA", pagou ? "PAGOU" : "PASSOU");
}

static void pokerIAJogar() {
    int alvo = pokerApostaJ;
    int diff = alvo - pokerApostaIA;
    int forca = pokerIA[0].v + pokerIA[1].v;
    if (pokerIA[0].v == pokerIA[1].v) forca += 8;
    if (diff <= 0) {
        if (forca >= 22 && (jogoRand() % 100) < 55) {
            pokerFazerAcao(-1, 1);
        } else if ((jogoRand() % 100) < 18) {
            pokerFazerAcao(-1, 1);
        } else {
            pokerFazerAcao(-1, 0);
        }
    } else {
        if (diff <= 20 || forca >= 24 || (jogoRand() % 100) < 62) {
            pokerFazerAcao(-1, 0);
        } else if (forca >= 26 && (jogoRand() % 100) < 30) {
            pokerFazerAcao(-1, 1);
        } else {
            pokerFazerAcao(-1, 2);
        }
    }
}

static void pokerNovaMao() {
    pokerNovoBaralho();
    pokerJ[0] = pokerComprar(); pokerJ[1] = pokerComprar();
    pokerIA[0] = pokerComprar(); pokerIA[1] = pokerComprar();
    pokerNMesa = 0;
    pokerEtapa = 0;
    pokerPote = 0;
    pokerApostaJ = pokerApostaIA = 0;
    pokerAgiuJ = pokerAgiuIA = false;
    pokerRevelar = false;
    pokerMaoEncerrada = false;
    pokerEspera = 0.0f;
    pokerEsperaMao = 0.0f;
    int apostaJ = pokerFichasJ < 10 ? pokerFichasJ : 10;
    pokerFichasJ -= apostaJ; pokerPote += apostaJ; pokerApostaJ = apostaJ;
    int apostaIA = pokerFichasIA < 20 ? pokerFichasIA : 20;
    pokerFichasIA -= apostaIA; pokerPote += apostaIA; pokerApostaIA = apostaIA;
    pokerVezJ = true;
    pokerMaoNum++;
    snprintf(pokerMsg, sizeof(pokerMsg), "MAO %d - SUA VEZ (SB 10 / BB 20)", pokerMaoNum);
}

static void pokerIniciar() {
    pokerFichasJ = 1000;
    pokerFichasIA = 1000;
    pokerFim = 0;
    pokerMaoNum = 0;
    pokerNovaMao();
}

static void pokerEntrada(uint32_t novos, float dt) {
    if (pokerFim) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) pokerIniciar();
        else if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_CARTAS_MENU;
        return;
    }
    if (pokerEsperaMao > 0.0f) {
        pokerEsperaMao -= dt;
        if (pokerEsperaMao <= 0.0f) {
            if (pokerFichasJ <= 0 || pokerFichasIA <= 0) {
                pokerFim = pokerFichasIA <= 0 ? 1 : -1;
                snprintf(pokerMsg, sizeof(pokerMsg), pokerFim == 1 ? "VOCE ZEROU A IA - VENCEU O JOGO!" : "VOCE FICOU SEM FICHAS - IA VENCEU");
            } else {
                pokerNovaMao();
            }
        }
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (pokerMaoEncerrada) return;
    if (!pokerVezJ) {
        if (pokerEspera > 0.0f) { pokerEspera -= dt; return; }
        pokerIAJogar();
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_CROSS) pokerFazerAcao(1, 0);
    else if (novos & ORBIS_PAD_BUTTON_TRIANGLE) pokerFazerAcao(1, 1);
    else if (novos & ORBIS_PAD_BUTTON_SQUARE) pokerFazerAcao(1, 2);
}

static void pokerDesenharCarta(uint32_t* tela, int x, int y, int w, int h, const PokerCarta* c, bool virada) {
    preencherRet(tela, x, y, w, h, virada ? 0xFF2E4A7A : 0xFFF6F1E6);
    jogoBorda(tela, x, y, w, h, 0xFF222833, 2);
    if (virada) {
        jogoCentro(tela, "?", 40, x + w / 2, y + h / 2 - 24, 0xFFDCE6F3);
        return;
    }
    static const char* RN[15] = { "", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A" };
    const char* sn = (c->n == 0 ? "P" : (c->n == 1 ? "C" : (c->n == 2 ? "O" : "E")));
    char t[8];
    snprintf(t, sizeof(t), "%s%s", RN[c->v], sn);
    uint32_t cor = (c->n == 1 || c->n == 2) ? 0xFFC62828 : 0xFF1A1A1A;
    desenharTexto(tela, t, 22, x + 8, y + 8, cor);
    jogoCentro(tela, t, 30, x + w / 2, y + h / 2 - 20, cor);
}

static void pokerDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A2A18);
    desenharTexto(tela, "POKER - TEXAS HOLD'EM", 40, 80, 36, 0xFFFFFFFF);
    char placar[128];
    snprintf(placar, sizeof(placar), "VOCE: %d fichas     IA: %d fichas", pokerFichasJ, pokerFichasIA);
    desenharTexto(tela, placar, 26, 1120, 44, 0xFFFFD166);
    desenharTexto(tela, pokerMsg, 24, 80, 92, 0xFFFFB07A);
    jogoCentro(tela, "IA", 24, 960, 150, 0xFFA9BCD6);
    for (int i = 0; i < 2; i++)
        pokerDesenharCarta(tela, 860 + i * 130, 185, 110, 150, &pokerIA[i], !pokerRevelar);
    jogoCentro(tela, "MESA", 22, 960, 360, 0xFFA9BCD6);
    for (int i = 0; i < 5; i++) {
        int x = 610 + i * 140;
        if (i < pokerNMesa) pokerDesenharCarta(tela, x, 400, 120, 165, &pokerMesa[i], false);
        else {
            preencherRet(tela, x, 400, 120, 165, 0x33000000);
            jogoBorda(tela, x, 400, 120, 165, 0x55FFFFFF, 2);
        }
    }
    char pote[64];
    snprintf(pote, sizeof(pote), "POTE: %d", pokerPote);
    jogoCentro(tela, pote, 30, 960, 600, 0xFFFFD166);
    jogoCentro(tela, "SUAS CARTAS", 24, 960, 700, 0xFFA9BCD6);
    for (int i = 0; i < 2; i++)
        pokerDesenharCarta(tela, 840 + i * 160, 740, 140, 190, &pokerJ[i], false);
    preencherRet(tela, 80, 340, 360, 240, 0xCC061A10);
    desenharTexto(tela, "COMO JOGAR", 24, 110, 360, 0xFFA9BCD6);
    desenharTexto(tela, "X: PAGAR / PASSAR", 20, 110, 410, 0xFFFFFFFF);
    desenharTexto(tela, "TRIANGULO: AUMENTAR", 20, 110, 450, 0xFFFFD166);
    desenharTexto(tela, "QUADRADO: DESISTIR", 20, 110, 490, 0xFFFF7A7A);
    desenharTexto(tela, "BOLINHA: SAIR PARA O MENU", 20, 110, 530, 0xFFFFFFFF);
    if (pokerFim) {
        jogoCentro(tela, pokerMsg, 36, 960, 660, 0xFFFFD166);
        jogoCentro(tela, "X: NOVO JOGO     BOLINHA: MENU", 24, 960, 720, 0xFFDCE6F3);
    } else if (pokerMaoEncerrada) {
        jogoCentro(tela, pokerMsg, 30, 960, 660, 0xFFFFD166);
    } else {
        jogoBarra(tela, pokerVezJ ? "SUA VEZ   X: PAGAR/PASSAR   TRIANGULO: AUMENTAR   QUADRADO: DESISTIR   BOLINHA: SAIR"
                                  : "VEZ DA IA...");
    }
}

// ---------------------------------------------------------------------------
// CARTAS - base comum e jogos
// ---------------------------------------------------------------------------
static const char* CARTA_R52[14] = { "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
static const char* CARTA_NAIPES[4] = { "P", "O", "C", "E" };

static void cartasBaralho52(int* d) {
    int n = 0;
    for (int r = 1; r <= 13; r++) for (int s = 0; s < 4; s++) d[n++] = r * 4 + s;
}
static void cartasBaralho40(int* d) {
    static const int rr[10] = { 1, 2, 3, 4, 5, 6, 7, 10, 11, 12 };
    int n = 0;
    for (int i = 0; i < 10; i++) for (int s = 0; s < 4; s++) d[n++] = rr[i] * 4 + s;
}
static void cartasCarta(uint32_t* tela, int x, int y, int w, int h, int code, bool virada, bool sel, bool b40) {
    preencherRet(tela, x, y, w, h, virada ? 0xFF2E4A7A : 0xFFF6F1E6);
    jogoBorda(tela, x, y, w, h, sel ? 0xFFFFD166 : 0xFF222833, sel ? 6 : 2);
    if (virada) {
        preencherRet(tela, x + 8, y + 8, w - 16, h - 16, 0xFF33507E);
        jogoCentro(tela, "?", (w * 2) / 5, x + w / 2, y + h / 2 - (w * 2) / 10, 0xFFDCE6F3);
        return;
    }
    if (code < 0) return;
    const char* r = b40 ? trucoNomeRank(code / 4) : CARTA_R52[code / 4];
    const char* n = CARTA_NAIPES[code % 4];
    char t[8];
    snprintf(t, sizeof(t), "%s%s", r, n);
    int tam = w >= 100 ? 30 : (w >= 70 ? 22 : 16);
    uint32_t cor = (code % 4 == 1 || code % 4 == 2) ? 0xFFB00020 : 0xFF1A1A1A;
    desenharTexto(tela, t, tam, x + 8, y + 5, cor);
    int lw = (int)strlen(t) * tam * 11 / 20;
    desenharTexto(tela, t, tam, x + w - 8 - lw, y + h - tam - 10, cor);
}
static void cartasFila(uint32_t* tela, const int* m, int n, int y, int w, int h, int sel, bool virada, bool b40) {
    if (n <= 0) return;
    int esp = w + 6;
    int total = n * esp - 6;
    int x0 = 960 - total / 2;
    for (int i = 0; i < n; i++) cartasCarta(tela, x0 + i * esp, y, w, h, m[i], virada, i == sel, b40);
}

// --- 21 (blackjack) ---
static int v21Deck[52], v21Top;
static int v21J[32], v21NJ, v21D[32], v21ND;
static int v21Fase, v21Vit, v21Der;
static float v21Espera;
static char v21Msg[128];

static int v21Total(const int* m, int n) {
    int s = 0, as = 0;
    for (int i = 0; i < n; i++) {
        int r = m[i] / 4;
        if (r == 1) { s += 11; as++; }
        else if (r >= 10) s += 10;
        else s += r;
    }
    while (s > 21 && as > 0) { s -= 10; as--; }
    return s;
}
static int v21Puxar() { return v21Top > 0 ? v21Deck[--v21Top] : -1; }
static void v21Iniciar() {
    cartasBaralho52(v21Deck); jogoEmbaralhar(v21Deck, 52); v21Top = 52;
    v21NJ = v21ND = 0;
    v21J[v21NJ++] = v21Puxar(); v21D[v21ND++] = v21Puxar();
    v21J[v21NJ++] = v21Puxar(); v21D[v21ND++] = v21Puxar();
    v21Fase = 0; v21Espera = 0.0f;
    if (v21Total(v21J, v21NJ) == 21) { snprintf(v21Msg, sizeof(v21Msg), "BLACKJACK! VOCE FEZ 21"); v21Fase = 1; v21Espera = 0.5f; }
    else snprintf(v21Msg, sizeof(v21Msg), "X COMPRAR   QUADRADO PARAR   BOLINHA SAIR");
}
static void v21Fechar() {
    while (v21Total(v21D, v21ND) < 17 && v21Top > 0) v21D[v21ND++] = v21Puxar();
    int p = v21Total(v21J, v21NJ), d = v21Total(v21D, v21ND);
    if (p > 21) { snprintf(v21Msg, sizeof(v21Msg), "VOCE ESTUROU %d - DEALER %d", p, d); v21Der++; }
    else if (d > 21 || p > d) { snprintf(v21Msg, sizeof(v21Msg), "VOCE VENCEU %d x %d", p, d); v21Vit++; }
    else if (p < d) { snprintf(v21Msg, sizeof(v21Msg), "DEALER VENCEU %d x %d", p, d); v21Der++; }
    else snprintf(v21Msg, sizeof(v21Msg), "EMPATE %d x %d", p, d);
    v21Fase = 2;
}
static void v21Entrada(uint32_t novos, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (v21Fase == 2) { if (novos & ORBIS_PAD_BUTTON_CROSS) v21Iniciar(); return; }
    if (v21Espera > 0.0f) { v21Espera -= dt; if (v21Espera <= 0.0f) v21Fechar(); return; }
    if (v21Fase == 1) { v21Fechar(); return; }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        int c = v21Puxar();
        if (c >= 0) v21J[v21NJ++] = c;
        int p = v21Total(v21J, v21NJ);
        if (p >= 21) { v21Fechar(); return; }
        snprintf(v21Msg, sizeof(v21Msg), "TOTAL %d - X COMPRAR   QUADRADO PARAR", p);
    } else if (novos & ORBIS_PAD_BUTTON_SQUARE) {
        v21Fechar();
    }
}
static void v21Desenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A2A18);
    jogoCentro(tela, "21 (BLACKJACK)", 46, 960, 34, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "VITORIAS %d   DERROTAS %d", v21Vit, v21Der);
    jogoCentro(tela, t, 22, 960, 96, 0xFFA9BCD6);
    jogoCentro(tela, "DEALER", 22, 960, 170, 0xFFA9BCD6);
    for (int i = 0; i < v21ND; i++) {
        bool oculta = (v21Fase == 0 && i == 0);
        cartasCarta(tela, 760 + i * 120, 210, 110, 150, v21D[i], oculta, false, false);
    }
    int vis = v21Fase == 0 ? v21Total(v21D + 1, v21ND - 1) : v21Total(v21D, v21ND);
    snprintf(t, sizeof(t), "TOTAL %d", vis);
    jogoCentro(tela, t, 22, 960, 390, 0xFFDCE6F3);
    jogoCentro(tela, "VOCE", 22, 960, 500, 0xFFA9BCD6);
    for (int i = 0; i < v21NJ; i++) cartasCarta(tela, 800 + i * 90, 540, 80, 112, v21J[i], false, false, false);
    snprintf(t, sizeof(t), "TOTAL %d", v21Total(v21J, v21NJ));
    jogoCentro(tela, t, 22, 960, 675, 0xFFFFD166);
    jogoCentro(tela, v21Msg, 24, 960, 780, 0xFFFFFFFF);
    if (v21Fase == 2) jogoCentro(tela, "X NOVA MAO   BOLINHA VOLTAR", 22, 960, 850, 0xFFA9BCD6);
    else jogoBarra(tela, "X COMPRAR   QUADRADO PARAR   BOLINHA SAIR");
}

// --- guerra ---
static int gDeck[52], gJ[52], gIA[52], gNJ, gNIA;
static int gUltJ = -1, gUltIA = -1, gFim;
static char gMsg[160];

static int guerraValor(int c) { int r = c / 4; return r == 1 ? 14 : r; }
static void guerraIniciar() {
    cartasBaralho52(gDeck); jogoEmbaralhar(gDeck, 52);
    gNJ = gNIA = 26;
    for (int i = 0; i < 26; i++) { gJ[i] = gDeck[i]; gIA[i] = gDeck[26 + i]; }
    gUltJ = gUltIA = -1; gFim = 0;
    snprintf(gMsg, sizeof(gMsg), "X VIRAR A CARTA");
}
static void guerraRodada() {
    if (gNJ <= 0 || gNIA <= 0) { gFim = gNJ <= 0 ? -1 : 1; return; }
    int pile[104], np = 0;
    int a = gJ[--gNJ], b = gIA[--gNIA];
    pile[np++] = a; pile[np++] = b; gUltJ = a; gUltIA = b;
    int va = guerraValor(a), vb = guerraValor(b);
    while (va == vb && gNJ > 0 && gNIA > 0 && np < 100) {
        a = gJ[--gNJ]; b = gIA[--gNIA];
        pile[np++] = a; pile[np++] = b; gUltJ = a; gUltIA = b;
        va = guerraValor(a); vb = guerraValor(b);
    }
    int venc = va > vb ? 1 : (vb > va ? -1 : 0);
    if (venc == 0) venc = gNJ > 0 ? 1 : (gNIA > 0 ? -1 : 0);
    if (venc == 1) for (int i = 0; i < np; i++) gJ[gNJ++] = pile[i];
    else if (venc == -1) for (int i = 0; i < np; i++) gIA[gNIA++] = pile[i];
    if (gNJ <= 0) { gFim = -1; snprintf(gMsg, sizeof(gMsg), "IA VENCEU A GUERRA"); }
    else if (gNIA <= 0) { gFim = 1; snprintf(gMsg, sizeof(gMsg), "VOCE VENCEU A GUERRA!"); }
    else {
        snprintf(gMsg, sizeof(gMsg), venc == 1 ? "VOCE LEVOU A RODADA" : "IA LEVOU A RODADA");
        if (np > 2) snprintf(gMsg + strlen(gMsg), sizeof(gMsg) - strlen(gMsg), " (COM GUERRA)");
    }
}
static void guerraEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (novos & ORBIS_PAD_BUTTON_CROSS) { if (gFim) guerraIniciar(); else guerraRodada(); }
}
static void guerraDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF2A0A14);
    jogoCentro(tela, "GUERRA", 46, 960, 34, 0xFFFFFFFF);
    char t[96];
    snprintf(t, sizeof(t), "SUAS CARTAS %d", gNJ);
    jogoCentro(tela, t, 24, 480, 140, 0xFFDCE6F3);
    snprintf(t, sizeof(t), "CARTAS DA IA %d", gNIA);
    jogoCentro(tela, t, 24, 1440, 140, 0xFFDCE6F3);
    jogoCentro(tela, "VOCE", 22, 960, 300, 0xFFA9BCD6);
    if (gUltJ >= 0) cartasCarta(tela, 860, 340, 120, 160, gUltJ, false, false, false);
    jogoCentro(tela, "IA", 22, 960, 540, 0xFFA9BCD6);
    if (gUltIA >= 0) cartasCarta(tela, 940, 580, 120, 160, gUltIA, false, false, false);
    jogoCentro(tela, gMsg, 26, 960, 810, 0xFFFFD166);
    if (gFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 890, 0xFFA9BCD6);
    else jogoBarra(tela, "X VIRAR   BOLINHA SAIR");
}

// --- pescaria (go fish) ---
static int pescDeck[52], pescTop;
static int pescJ[52], pescNJ, pescIA[52], pescNIA;
static int pescPtsJ, pescPtsIA, pescSel = 1, pescTurno, pescFim;
static float pescEspera;
static char pescMsg[160];

static int pescConta(const int* m, int n, int r) { int c = 0; for (int i = 0; i < n; i++) if (m[i] / 4 == r) c++; return c; }
static int pescRetira(int* m, int n, int r, int* dst, int nd) {
    int w = 0;
    for (int i = 0; i < n; i++) { if (m[i] / 4 == r) dst[nd++] = m[i]; else m[w++] = m[i]; }
    return w;
}
static void pescSets() {
    int tmp[4];
    for (int r = 1; r <= 13; r++) {
        if (pescConta(pescJ, pescNJ, r) == 4) { pescNJ = pescRetira(pescJ, pescNJ, r, tmp, 0); pescPtsJ++; }
        if (pescConta(pescIA, pescNIA, r) == 4) { pescNIA = pescRetira(pescIA, pescNIA, r, tmp, 0); pescPtsIA++; }
    }
}
static void pescIniciar() {
    cartasBaralho52(pescDeck); jogoEmbaralhar(pescDeck, 52); pescTop = 52;
    pescNJ = pescNIA = 0;
    for (int i = 0; i < 7; i++) { pescJ[pescNJ++] = pescDeck[--pescTop]; pescIA[pescNIA++] = pescDeck[--pescTop]; }
    pescPtsJ = pescPtsIA = 0; pescSel = 1; pescTurno = 0; pescFim = 0; pescEspera = 0.0f;
    snprintf(pescMsg, sizeof(pescMsg), "ESCOLHA UM RANK E PECA (X)");
}
static void pescTalvezFim() {
    if (pescTop > 0 || pescFim) return;
    pescSets();
    if (pescPtsJ > pescPtsIA) snprintf(pescMsg, sizeof(pescMsg), "FIM - VOCE VENCEU %d x %d", pescPtsJ, pescPtsIA);
    else if (pescPtsIA > pescPtsJ) snprintf(pescMsg, sizeof(pescMsg), "FIM - IA VENCEU %d x %d", pescPtsJ, pescPtsIA);
    else snprintf(pescMsg, sizeof(pescMsg), "FIM - EMPATE %d x %d", pescPtsJ, pescPtsIA);
    pescFim = 1;
}
static void pescPedir(int quem, int r) {
    const char* rn = CARTA_R52[r];
    if (quem == 0) {
        int c = pescConta(pescIA, pescNIA, r);
        if (c > 0) {
            pescNIA = pescRetira(pescIA, pescNIA, r, pescJ, pescNJ); pescNJ += c;
            snprintf(pescMsg, sizeof(pescMsg), "IA TINHA %d CARTA(S) %s", c, rn);
        } else if (pescTop > 0) {
            pescJ[pescNJ++] = pescDeck[--pescTop];
            snprintf(pescMsg, sizeof(pescMsg), "PESQUEI! IA NAO TINHA %s", rn);
        } else snprintf(pescMsg, sizeof(pescMsg), "IA NAO TINHA %s", rn);
    } else {
        int c = pescConta(pescJ, pescNJ, r);
        if (c > 0) {
            pescNJ = pescRetira(pescJ, pescNJ, r, pescIA, pescNIA); pescNIA += c;
            snprintf(pescMsg, sizeof(pescMsg), "IA PEDIU %s E LEVOU %d", rn, c);
        } else if (pescTop > 0) {
            pescIA[pescNIA++] = pescDeck[--pescTop];
            snprintf(pescMsg, sizeof(pescMsg), "IA PESCARIA %s (NAO TINHA)", rn);
        } else snprintf(pescMsg, sizeof(pescMsg), "IA PEDIU %s", rn);
    }
    pescSets();
}
static void pescEntrada(uint32_t novos, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (pescFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) pescIniciar(); return; }
    if (pescTurno == 1) {
        if (pescEspera > 0.0f) {
            pescEspera -= dt;
            if (pescEspera > 0.0f) return;
        }
        int r = pescNIA > 0 ? pescIA[jogoRand() % pescNIA] / 4 : (jogoRand() % 13) + 1;
        pescPedir(1, r);
        pescTurno = 0;
        pescTalvezFim();
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_LEFT) pescSel = pescSel <= 1 ? 13 : pescSel - 1;
    if (novos & ORBIS_PAD_BUTTON_RIGHT) pescSel = pescSel >= 13 ? 1 : pescSel + 1;
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        pescPedir(0, pescSel);
        pescTurno = 1; pescEspera = 0.9f;
        pescTalvezFim();
    }
}
static void pescDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF102A33);
    jogoCentro(tela, "PESCARIA (GO FISH)", 44, 960, 28, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "LIVROS  VOCE %d   x   %d IA     MONTE %d", pescPtsJ, pescPtsIA, pescTop);
    jogoCentro(tela, t, 22, 960, 88, 0xFFA9BCD6);
    jogoCentro(tela, "IA", 20, 960, 140, 0xFFA9BCD6);
    for (int i = 0; i < pescNIA; i++)
        cartasCarta(tela, 650 + (i % 14) * 48, 168 + (i / 14) * 62, 42, 58, 0, true, false, false);
    jogoCentro(tela, "SUA MAO", 20, 960, 330, 0xFFA9BCD6);
    cartasFila(tela, pescJ, pescNJ, 360, 60, 84, -1, false, false);
    snprintf(t, sizeof(t), "PEDIR: %s    (VOCE TEM %d)", CARTA_R52[pescSel], pescConta(pescJ, pescNJ, pescSel));
    jogoCentro(tela, t, 30, 960, 500, 0xFFFFD166);
    jogoCentro(tela, pescMsg, 24, 960, 650, 0xFFFFFFFF);
    if (pescFim) jogoCentro(tela, "X NOVA PARTIDA   BOLINHA VOLTAR", 22, 960, 780, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR RANK   X PEDIR   BOLINHA SAIR");
}

// --- mico (old maid) ---
static int micoJ[26], micoNJ, micoIA[26], micoNIA;
static int micoSel, micoFim, micoVez;
static float micoEspera;
static char micoMsg[160];

static void micoPares(int* m, int* n) {
    int cont[14] = { 0 };
    for (int i = 0; i < *n; i++) cont[m[i] / 4]++;
    int sobra[14];
    for (int r = 1; r <= 13; r++) sobra[r] = cont[r] % 2;
    int w = 0;
    for (int i = 0; i < *n; i++) {
        int r = m[i] / 4;
        if (sobra[r] > 0) { m[w++] = m[i]; sobra[r]--; }
    }
    *n = w;
}
static void micoIniciar() {
    int deck[52];
    cartasBaralho52(deck);
    int sem = 12 * 4 + 2;
    int n = 0;
    for (int i = 0; i < 52; i++) if (deck[i] != sem) deck[n++] = deck[i];
    jogoEmbaralhar(deck, 51);
    micoNJ = micoNIA = 0;
    for (int i = 0; i < 51; i++) {
        if (i % 2 == 0) micoJ[micoNJ++] = deck[i];
        else micoIA[micoNIA++] = deck[i];
    }
    micoPares(micoJ, &micoNJ);
    micoPares(micoIA, &micoNIA);
    micoSel = 0; micoFim = 0; micoVez = 0; micoEspera = 0.0f;
    snprintf(micoMsg, sizeof(micoMsg), "ESCOLHA UMA CARTA DA IA E PUXE (X)");
}
static void micoEntrada(uint32_t novos, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (micoFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) micoIniciar(); return; }
    if (micoVez == 1) {
        if (micoEspera > 0.0f) {
            micoEspera -= dt;
            if (micoEspera > 0.0f) return;
        }
        if (micoNJ > 0) {
            int idx = jogoRand() % micoNJ;
            micoIA[micoNIA++] = micoJ[idx];
            for (int i = idx; i < micoNJ - 1; i++) micoJ[i] = micoJ[i + 1];
            micoNJ--;
            micoPares(micoIA, &micoNIA);
            snprintf(micoMsg, sizeof(micoMsg), "IA PUXOU UMA CARTA DE VOCE");
            if (micoNIA == 0) { micoFim = 1; snprintf(micoMsg, sizeof(micoMsg), "A IA SE LIVROU - VOCE FICOU COM O MICO"); }
            else if (micoNJ == 0) { micoFim = 1; snprintf(micoMsg, sizeof(micoMsg), "VOCE SE LIVROU DAS CARTAS - VOCE VENCEU!"); }
        }
        micoVez = 0; micoSel = 0;
        return;
    }
    if (micoNIA > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) micoSel = (micoSel + micoNIA - 1) % micoNIA;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) micoSel = (micoSel + 1) % micoNIA;
    }
    if ((novos & ORBIS_PAD_BUTTON_CROSS) && micoNIA > 0) {
        micoJ[micoNJ++] = micoIA[micoSel];
        for (int i = micoSel; i < micoNIA - 1; i++) micoIA[i] = micoIA[i + 1];
        micoNIA--;
        if (micoSel >= micoNIA) micoSel = micoNIA > 0 ? micoNIA - 1 : 0;
        micoPares(micoJ, &micoNJ);
        if (micoNJ == 0) { micoFim = 1; snprintf(micoMsg, sizeof(micoMsg), "VOCE SE LIVROU DAS CARTAS - VOCE VENCEU!"); }
        else if (micoNIA == 0) { micoFim = 1; snprintf(micoMsg, sizeof(micoMsg), "A IA SE LIVROU - VOCE FICOU COM O MICO"); }
        else { snprintf(micoMsg, sizeof(micoMsg), "CARTA PUXADA - VEZ DA IA"); micoVez = 1; micoEspera = 0.7f; }
    }
}
static void micoDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF2A1A0A);
    jogoCentro(tela, "MICO (OLD MAID)", 44, 960, 28, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "SUAS CARTAS %d     CARTAS DA IA %d", micoNJ, micoNIA);
    jogoCentro(tela, t, 22, 960, 88, 0xFFA9BCD6);
    jogoCentro(tela, "MAO DA IA - ESCOLHA E PUXE", 20, 960, 140, 0xFFA9BCD6);
    for (int i = 0; i < micoNIA; i++) {
        int x = 720 + (i % 12) * 74;
        int y = 180 + (i / 12) * 104;
        cartasCarta(tela, x, y, 62, 86, 0, true, i == micoSel, false);
    }
    jogoCentro(tela, "SUA MAO", 20, 960, 430, 0xFFA9BCD6);
    cartasFila(tela, micoJ, micoNJ, 460, 60, 84, -1, false, false);
    jogoCentro(tela, micoMsg, 24, 960, 640, 0xFFFFFFFF);
    if (micoFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 780, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   X PUXAR   BOLINHA SAIR");
}

// --- bisca / sueca (motor de vazas 1x1) ---
static int tr2Deck[40], tr2J[10], tr2IA[10], tr2NJ, tr2NIA;
static int tr2Trunfo, tr2Vez, tr2PtsJ, tr2PtsIA, tr2Fim, tr2Sel, tr2Modo;
static int tr2CJ, tr2CIA, tr2LeadQuem, tr2TemJ, tr2TemIA, tr2Rodada;
static float tr2Espera;
static char tr2Msg[160];

static int cartaPtValor(int r) {
    switch (r) {
        case 1: return 10;
        case 7: return 9;
        case 12: return 8;
        case 11: return 7;
        case 10: return 6;
        default: return r - 1;
    }
}
static int cartaPtPontos(int r) {
    if (r == 1) return 11;
    if (r == 7) return 10;
    if (r == 12) return 4;
    if (r == 11) return 3;
    if (r == 10) return 2;
    return 0;
}
static void tr2Iniciar(int modo) {
    tr2Modo = modo;
    cartasBaralho40(tr2Deck); jogoEmbaralhar(tr2Deck, 40);
    tr2NJ = tr2NIA = 10;
    for (int i = 0; i < 10; i++) { tr2J[i] = tr2Deck[i]; tr2IA[i] = tr2Deck[10 + i]; }
    tr2Trunfo = jogoRand() % 4;
    tr2PtsJ = tr2PtsIA = 0; tr2Fim = 0; tr2Sel = 0; tr2Vez = 0; tr2Rodada = 0;
    tr2CJ = tr2CIA = -1; tr2LeadQuem = 0; tr2TemJ = tr2TemIA = 0; tr2Espera = 0.0f;
    snprintf(tr2Msg, sizeof(tr2Msg), "VOCE COMECA - ESCOLHA UMA CARTA");
}
static void tr2IAJogar(bool seguindo) {
    int idx = 0;
    int bestIdx = -1, bestVal = 999;
    if (seguindo) {
        int leadSuit = tr2CJ % 4;
        for (int i = 0; i < tr2NIA; i++) {
            int c = tr2IA[i];
            if (c % 4 == leadSuit) { int v = cartaPtValor(c / 4); if (v < bestVal) { bestVal = v; bestIdx = i; } }
        }
        if (bestIdx < 0) for (int i = 0; i < tr2NIA; i++) {
            int c = tr2IA[i];
            if (c % 4 == tr2Trunfo) { int v = cartaPtValor(c / 4); if (v < bestVal) { bestVal = v; bestIdx = i; } }
        }
        if (bestIdx < 0) for (int i = 0; i < tr2NIA; i++) {
            int c = tr2IA[i]; int v = cartaPtValor(c / 4); if (v < bestVal) { bestVal = v; bestIdx = i; }
        }
        idx = bestIdx;
    } else {
        for (int i = 0; i < tr2NIA; i++) {
            int c = tr2IA[i];
            if (c % 4 == tr2Trunfo) continue;
            int v = cartaPtValor(c / 4); if (v < bestVal) { bestVal = v; bestIdx = i; }
        }
        idx = bestIdx < 0 ? 0 : bestIdx;
    }
    tr2CIA = tr2IA[idx];
    for (int i = idx; i < tr2NIA - 1; i++) tr2IA[i] = tr2IA[i + 1];
    tr2NIA--; tr2TemIA = 1;
}
static int tr2Vencedor(int leadCard, int respCard) {
    int sl = leadCard % 4, sr = respCard % 4;
    if (sl == sr) return cartaPtValor(leadCard / 4) >= cartaPtValor(respCard / 4) ? 0 : 1;
    if (sr == tr2Trunfo) return 1;
    if (sl == tr2Trunfo) return 0;
    return 0;
}
static void tr2Resolver() {
    int leadCard = tr2LeadQuem == 0 ? tr2CJ : tr2CIA;
    int respCard = tr2LeadQuem == 0 ? tr2CIA : tr2CJ;
    int v = tr2Vencedor(leadCard, respCard);
    int quem = v == 0 ? tr2LeadQuem : 1 - tr2LeadQuem;
    int pts = tr2Modo == 1 ? cartaPtPontos(leadCard / 4) + cartaPtPontos(respCard / 4) : 1;
    if (quem == 0) { tr2PtsJ += pts; snprintf(tr2Msg, sizeof(tr2Msg), "VOCE LEVOU A VAZA (+%d)", pts); }
    else { tr2PtsIA += pts; snprintf(tr2Msg, sizeof(tr2Msg), "IA LEVOU A VAZA (+%d)", pts); }
    tr2CJ = tr2CIA = -1; tr2TemJ = tr2TemIA = 0; tr2Sel = 0; tr2Vez = quem; tr2Rodada++;
    if (tr2NJ == 0 || tr2NIA == 0) {
        tr2Fim = 1;
        if (tr2PtsJ > tr2PtsIA) snprintf(tr2Msg, sizeof(tr2Msg), "VOCE VENCEU %d x %d", tr2PtsJ, tr2PtsIA);
        else if (tr2PtsIA > tr2PtsJ) snprintf(tr2Msg, sizeof(tr2Msg), "IA VENCEU %d x %d", tr2PtsJ, tr2PtsIA);
        else snprintf(tr2Msg, sizeof(tr2Msg), "EMPATE %d x %d", tr2PtsJ, tr2PtsIA);
    }
}
static void tr2Entrada(uint32_t novos, float dt) {
    if (tr2Fim) {
        if (novos & ORBIS_PAD_BUTTON_CIRCLE) jogoAtual = JOGO_CARTAS_MENU;
        else if (novos & ORBIS_PAD_BUTTON_CROSS) tr2Iniciar(tr2Modo);
        return;
    }
    if (tr2Espera > 0.0f) { tr2Espera -= dt; if (tr2Espera <= 0.0f) tr2Resolver(); return; }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (tr2TemJ && tr2TemIA) return;
    if (tr2Vez == 1 && !tr2TemIA) { tr2IAJogar(false); tr2LeadQuem = 1; }
    if (!tr2TemJ && tr2NJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) tr2Sel = (tr2Sel + tr2NJ - 1) % tr2NJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) tr2Sel = (tr2Sel + 1) % tr2NJ;
        if (tr2Sel >= tr2NJ) tr2Sel = 0;
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            tr2CJ = tr2J[tr2Sel];
            for (int i = tr2Sel; i < tr2NJ - 1; i++) tr2J[i] = tr2J[i + 1];
            tr2NJ--; tr2TemJ = 1; tr2Sel = 0;
            if (!tr2TemIA) { tr2LeadQuem = 0; tr2IAJogar(true); }
        }
    }
    if (tr2TemJ && tr2TemIA) tr2Espera = 0.9f;
}
static void tr2Desenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, tr2Modo == 1 ? 0xFF102A33 : 0xFF1A0A2A);
    jogoCentro(tela, tr2Modo == 1 ? "SUECA" : "BISCA", 46, 960, 30, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "TRUNFO: %s    VOCE %d x %d IA    VAZA %d/10", CARTA_NAIPES[tr2Trunfo], tr2PtsJ, tr2PtsIA, tr2Rodada);
    jogoCentro(tela, t, 22, 960, 90, 0xFFFFD166);
    jogoCentro(tela, "IA", 20, 960, 140, 0xFFA9BCD6);
    for (int i = 0; i < tr2NIA; i++)
        cartasCarta(tela, 660 + i * 58, 162, 52, 74, 0, true, false, true);
    if (tr2TemIA) cartasCarta(tela, 780, 380, 100, 140, tr2CIA, false, false, true);
    if (tr2TemJ) cartasCarta(tela, 1020, 380, 100, 140, tr2CJ, false, false, true);
    jogoCentro(tela, "SUA MAO", 20, 960, 470, 0xFFA9BCD6);
    cartasFila(tela, tr2J, tr2NJ, 555, 60, 84, tr2TemJ ? -1 : tr2Sel, false, true);
    jogoCentro(tela, tr2Msg, 24, 960, 650, 0xFFFFFFFF);
    if (tr2Fim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 770, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   X JOGAR   BOLINHA SAIR");
}

// --- escopa ---
static int escDeck[40], escMesa[32], escNMesa, escJ[10], escNJ, escIA[10], escNIA;
static int escSel, escFim, escPtsJ, escPtsIA, escUltJ, escUltIA;
static char escMsg[160];

static void escJogada(int quem, int idx) {
    int* mao = quem == 0 ? escJ : escIA;
    int* nm = quem == 0 ? &escNJ : &escNIA;
    int carta = mao[idx];
    for (int i = idx; i < *nm - 1; i++) mao[i] = mao[i + 1];
    (*nm)--;
    int r = carta / 4, achou = 0;
    for (int i = 0; i < escNMesa; i++) if (escMesa[i] / 4 == r) { achou = 1; break; }
    if (achou) {
        int ganho = 1, w = 0;
        for (int i = 0; i < escNMesa; i++) {
            if (escMesa[i] / 4 == r) ganho++;
            else escMesa[w++] = escMesa[i];
        }
        escNMesa = w;
        if (quem == 0) { escPtsJ += ganho; escUltJ = 1; escUltIA = 0; }
        else { escPtsIA += ganho; escUltIA = 1; escUltJ = 0; }
        snprintf(escMsg, sizeof(escMsg), quem == 0 ? "VOCE CAPTUROU %d CARTA(S)" : "IA CAPTUROU %d CARTA(S)", ganho);
    } else {
        if (escNMesa < 20) escMesa[escNMesa++] = carta;
        snprintf(escMsg, sizeof(escMsg), quem == 0 ? "VOCE JOGOU SEM CAPTURAR" : "IA JOGOU SEM CAPTURAR");
    }
}
static void escIniciar() {
    cartasBaralho40(escDeck); jogoEmbaralhar(escDeck, 40);
    escNJ = escNIA = 9;
    for (int i = 0; i < 9; i++) { escJ[i] = escDeck[i]; escIA[i] = escDeck[9 + i]; }
    escNMesa = 4;
    for (int i = 0; i < 4; i++) escMesa[i] = escDeck[18 + i];
    escSel = 0; escFim = 0; escPtsJ = escPtsIA = 0; escUltJ = escUltIA = 0;
    snprintf(escMsg, sizeof(escMsg), "CAPTURE CARTAS DE MESMO VALOR");
}
static void escTalvezFim() {
    if (escFim || escNJ > 0 || escNIA > 0) return;
    if (escNMesa > 0) {
        if (escUltJ) escPtsJ += escNMesa; else escPtsIA += escNMesa;
        escNMesa = 0;
    }
    escFim = 1;
    if (escPtsJ > escPtsIA) snprintf(escMsg, sizeof(escMsg), "VOCE VENCEU %d x %d", escPtsJ, escPtsIA);
    else if (escPtsIA > escPtsJ) snprintf(escMsg, sizeof(escMsg), "IA VENCEU %d x %d", escPtsJ, escPtsIA);
    else snprintf(escMsg, sizeof(escMsg), "EMPATE %d x %d", escPtsJ, escPtsIA);
}
static void escEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (escFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) escIniciar(); return; }
    if (escNJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) escSel = (escSel + escNJ - 1) % escNJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) escSel = (escSel + 1) % escNJ;
        if (escSel >= escNJ) escSel = 0;
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            escJogada(0, escSel);
            if (escSel >= escNJ) escSel = escNJ > 0 ? escNJ - 1 : 0;
            if (escNIA > 0) {
                int mel = -1;
                for (int i = 0; i < escNIA; i++) {
                    for (int k = 0; k < escNMesa; k++) if (escIA[i] / 4 == escMesa[k] / 4) { mel = i; break; }
                    if (mel >= 0) break;
                }
                escJogada(1, mel >= 0 ? mel : jogoRand() % escNIA);
            }
            escTalvezFim();
        }
    }
}
static void escDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A2A18);
    jogoCentro(tela, "ESCOPA", 46, 960, 30, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "CAPTURADAS  VOCE %d x %d IA", escPtsJ, escPtsIA);
    jogoCentro(tela, t, 22, 960, 90, 0xFFFFD166);
    jogoCentro(tela, "IA", 20, 960, 140, 0xFFA9BCD6);
    for (int i = 0; i < escNIA; i++) cartasCarta(tela, 660 + i * 58, 162, 52, 74, 0, true, false, true);
    jogoCentro(tela, "MESA", 20, 960, 280, 0xFFA9BCD6);
    for (int i = 0; i < escNMesa; i++) cartasCarta(tela, 620 + (i % 10) * 74, 310 + (i / 10) * 96, 64, 90, escMesa[i], false, false, true);
    jogoCentro(tela, "SUA MAO", 20, 960, 470, 0xFFA9BCD6);
    cartasFila(tela, escJ, escNJ, 500, 60, 84, escSel, false, true);
    jogoCentro(tela, escMsg, 24, 960, 650, 0xFFFFFFFF);
    if (escFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 770, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   X JOGAR   BOLINHA SAIR");
}

// --- mau-mau ---
static int mmDeck[52], mmTop, mmJ[32], mmNJ, mmIA[32], mmNIA, mmDesc, mmSel, mmFim, mmComprou;
static char mmMsg[160];

static bool mmCombina(int c) { return c / 4 == mmDesc / 4 || c % 4 == mmDesc % 4; }
static void mmIniciar() {
    cartasBaralho52(mmDeck); jogoEmbaralhar(mmDeck, 52); mmTop = 52;
    mmNJ = mmNIA = 0;
    for (int i = 0; i < 5; i++) { mmJ[mmNJ++] = mmDeck[--mmTop]; mmIA[mmNIA++] = mmDeck[--mmTop]; }
    mmDesc = mmDeck[--mmTop]; mmSel = 0; mmFim = 0; mmComprou = 0;
    snprintf(mmMsg, sizeof(mmMsg), "JOGUE CARTA DE MESMO NAIPE OU VALOR");
}
static void mmIAJogar() {
    int idx = -1;
    for (int i = 0; i < mmNIA; i++) if (mmCombina(mmIA[i])) { idx = i; break; }
    if (idx >= 0) {
        mmDesc = mmIA[idx];
        for (int i = idx; i < mmNIA - 1; i++) mmIA[i] = mmIA[i + 1];
        mmNIA--;
        snprintf(mmMsg, sizeof(mmMsg), "IA JOGOU %s%s", CARTA_R52[mmDesc / 4], CARTA_NAIPES[mmDesc % 4]);
    } else if (mmTop > 0) {
        mmIA[mmNIA++] = mmDeck[--mmTop];
        snprintf(mmMsg, sizeof(mmMsg), "IA COMPROU UMA CARTA");
    } else snprintf(mmMsg, sizeof(mmMsg), "IA PASSOU (MONTE VAZIO)");
}
static void mmEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (mmFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) mmIniciar(); return; }
    if (mmNJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) mmSel = (mmSel + mmNJ - 1) % mmNJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) mmSel = (mmSel + 1) % mmNJ;
        if (mmSel >= mmNJ) mmSel = 0;
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            if (!mmCombina(mmJ[mmSel])) { snprintf(mmMsg, sizeof(mmMsg), "CARTA INVALIDA - TENTE OUTRA OU COMPRE"); return; }
            mmDesc = mmJ[mmSel];
            for (int i = mmSel; i < mmNJ - 1; i++) mmJ[i] = mmJ[i + 1];
            mmNJ--; if (mmSel >= mmNJ) mmSel = mmNJ > 0 ? mmNJ - 1 : 0;
            if (mmNJ == 0) { mmFim = 1; snprintf(mmMsg, sizeof(mmMsg), "VOCE BATEU - VENCEU!"); return; }
            mmComprou = 0;
            mmIAJogar();
            if (mmNIA == 0) { mmFim = 1; snprintf(mmMsg, sizeof(mmMsg), "IA BATEU - VOCE PERDEU"); return; }
            snprintf(mmMsg, sizeof(mmMsg), "SUA VEZ - JOGUE OU COMPRE");
        } else if (novos & ORBIS_PAD_BUTTON_SQUARE) {
            if (!mmComprou && mmTop > 0) {
                mmJ[mmNJ++] = mmDeck[--mmTop]; mmComprou = 1;
                snprintf(mmMsg, sizeof(mmMsg), "VOCE COMPROU - JOGUE OU PASSE (QUADRADO)");
            } else {
                mmComprou = 0;
                mmIAJogar();
                if (mmNIA == 0) { mmFim = 1; snprintf(mmMsg, sizeof(mmMsg), "IA BATEU - VOCE PERDEU"); return; }
                snprintf(mmMsg, sizeof(mmMsg), "VOCE PASSOU - SUA VEZ");
            }
        }
    }
}
static void mmDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF1A2A0A);
    jogoCentro(tela, "MAU-MAU", 46, 960, 30, 0xFFFFFFFF);
    jogoCentro(tela, "COMBINE NAIPE OU VALOR COM A CARTA DA MESA", 20, 960, 92, 0xFFA9BCD6);
    jogoCentro(tela, "IA", 20, 960, 150, 0xFFA9BCD6);
    for (int i = 0; i < mmNIA; i++) cartasCarta(tela, 660 + (i % 18) * 34, 178 + (i / 18) * 46, 30, 42, 0, true, false, false);
    cartasCarta(tela, 880, 320, 120, 168, mmDesc, false, false, false);
    jogoCentro(tela, "MESA", 22, 960, 500, 0xFFA9BCD6);
    jogoCentro(tela, "SUA MAO", 20, 960, 530, 0xFFA9BCD6);
    cartasFila(tela, mmJ, mmNJ, 560, 54, 76, mmSel, false, false);
    jogoCentro(tela, mmMsg, 24, 960, 680, 0xFFFFFFFF);
    if (mmFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 780, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   X JOGAR   QUADRADO COMPRAR   BOLINHA SAIR");
}

// --- fodinha ---
static int fodDeck[40], fodJ[5], fodIA[5], fodNJ, fodNIA;
static int fodAposta, fodFase, fodSel, fodVez, fodFeitasJ, fodFeitasIA, fodTrick;
static int fodCJ, fodCIA, fodLeadQuem, fodTemJ, fodTemIA, fodPontosJ, fodPontosIA;
static char fodMsg[160];

static void fodIniciar() {
    cartasBaralho40(fodDeck); jogoEmbaralhar(fodDeck, 40);
    fodNJ = fodNIA = 5;
    for (int i = 0; i < 5; i++) { fodJ[i] = fodDeck[i]; fodIA[i] = fodDeck[5 + i]; }
    fodAposta = 0; fodFase = 0; fodSel = 0; fodVez = 0;
    fodFeitasJ = fodFeitasIA = 0; fodTrick = 0;
    fodCJ = fodCIA = -1; fodLeadQuem = 0; fodTemJ = fodTemIA = 0;
    snprintf(fodMsg, sizeof(fodMsg), "APOSTE QUANTAS VAZAS VAI FAZER");
}
static void fodIAJogar(bool seguindo) {
    int idx = 0, best = -1, bestVal = 999;
    if (seguindo) {
        int leadSuit = fodCJ % 4;
        for (int i = 0; i < fodNIA; i++) {
            int c = fodIA[i];
            if (c % 4 == leadSuit) { int v = cartaPtValor(c / 4); if (v < bestVal) { bestVal = v; best = i; } }
        }
        if (best < 0) for (int i = 0; i < fodNIA; i++) {
            int c = fodIA[i]; int v = cartaPtValor(c / 4); if (v < bestVal) { bestVal = v; best = i; }
        }
        idx = best < 0 ? 0 : best;
    } else idx = jogoRand() % fodNIA;
    fodCIA = fodIA[idx];
    for (int i = idx; i < fodNIA - 1; i++) fodIA[i] = fodIA[i + 1];
    fodNIA--; fodTemIA = 1;
}
static void fodResolver() {
    int leadCard = fodLeadQuem == 0 ? fodCJ : fodCIA;
    int respCard = fodLeadQuem == 0 ? fodCIA : fodCJ;
    int sl = leadCard % 4, sr = respCard % 4;
    int vencedorLead = (sl == sr) ? (cartaPtValor(leadCard / 4) >= cartaPtValor(respCard / 4)) : 1;
    int quem = vencedorLead ? fodLeadQuem : 1 - fodLeadQuem;
    if (quem == 0) { fodFeitasJ++; snprintf(fodMsg, sizeof(fodMsg), "VOCE LEVOU A VAZA (%d/5)", fodTrick + 1); }
    else { fodFeitasIA++; snprintf(fodMsg, sizeof(fodMsg), "IA LEVOU A VAZA (%d/5)", fodTrick + 1); }
    fodCJ = fodCIA = -1; fodTemJ = fodTemIA = 0; fodSel = 0; fodVez = quem; fodTrick++;
    if (fodTrick >= 5) {
        fodFase = 2;
        if (fodFeitasJ == fodAposta) { fodPontosJ += 10 + fodAposta; snprintf(fodMsg, sizeof(fodMsg), "ACERTOU A APOSTA! +%d PONTOS", 10 + fodAposta); }
        else { snprintf(fodMsg, sizeof(fodMsg), "ERROU: APOSTOU %d, FEZ %d", fodAposta, fodFeitasJ); }
        if (fodFeitasIA == 0) fodPontosIA += 10;
    }
}
static void fodEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (fodFase == 2) { if (novos & ORBIS_PAD_BUTTON_CROSS) fodIniciar(); return; }
    if (fodFase == 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) fodAposta = (fodAposta + 5) % 6;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) fodAposta = (fodAposta + 1) % 6;
        if (novos & ORBIS_PAD_BUTTON_CROSS) { fodFase = 1; snprintf(fodMsg, sizeof(fodMsg), "JOGUE SUAS 5 CARTAS"); }
        return;
    }
    if (fodTemJ && fodTemIA) return;
    if (fodVez == 1 && !fodTemIA) { fodIAJogar(false); fodLeadQuem = 1; }
    if (!fodTemJ && fodNJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) fodSel = (fodSel + fodNJ - 1) % fodNJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) fodSel = (fodSel + 1) % fodNJ;
        if (fodSel >= fodNJ) fodSel = 0;
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            fodCJ = fodJ[fodSel];
            for (int i = fodSel; i < fodNJ - 1; i++) fodJ[i] = fodJ[i + 1];
            fodNJ--; fodTemJ = 1; fodSel = 0;
            if (!fodTemIA) { fodLeadQuem = 0; fodIAJogar(true); }
            if (fodTemJ && fodTemIA) fodResolver();
        }
    }
}
static void fodDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF2A1A0A);
    jogoCentro(tela, "FODINHA", 46, 960, 30, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "PONTOS VOCE %d x %d IA    APOSTA %d", fodPontosJ, fodPontosIA, fodAposta);
    jogoCentro(tela, t, 22, 960, 90, 0xFFFFD166);
    jogoCentro(tela, "IA", 20, 960, 150, 0xFFA9BCD6);
    for (int i = 0; i < fodNIA; i++) cartasCarta(tela, 780 + i * 62, 176, 56, 80, 0, true, false, true);
    if (fodTemIA) cartasCarta(tela, 800, 390, 100, 140, fodCIA, false, false, true);
    if (fodTemJ) cartasCarta(tela, 1020, 390, 100, 140, fodCJ, false, false, true);
    jogoCentro(tela, "SUA MAO", 20, 960, 480, 0xFFA9BCD6);
    cartasFila(tela, fodJ, fodNJ, 560, 62, 86, fodTemJ ? -1 : fodSel, false, true);
    jogoCentro(tela, fodMsg, 24, 960, 660, 0xFFFFFFFF);
    if (fodFase == 0) jogoCentro(tela, "ESQ/DIR APOSTA   X CONFIRMAR", 22, 960, 760, 0xFFA9BCD6);
    else if (fodFase == 2) jogoCentro(tela, "X NOVA MAO   BOLINHA VOLTAR", 22, 960, 760, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   X JOGAR   BOLINHA SAIR");
}

// --- rummy (buraco / cacheta / biriba / pif-paf) ---
static int cartaValor52(int r) { return r == 1 ? 14 : r; }
static int rumDeck[52], rumTop, rumJ[24], rumNJ, rumIA[24], rumNIA, rumDesc, rumSel, rumFim, rumModo, rumVez, rumComprou;
static char rumMsg[160];

static const char* rumNome() {
    if (rumModo == 0) return "BURACO";
    if (rumModo == 1) return "CACHETA";
    if (rumModo == 2) return "BIRIBA";
    return "PIF-PAF";
}
static void rumIniciar(int modo) {
    rumModo = modo;
    cartasBaralho52(rumDeck); jogoEmbaralhar(rumDeck, 52); rumTop = 52;
    rumNJ = rumNIA = 9;
    for (int i = 0; i < 9; i++) { rumJ[i] = rumDeck[--rumTop]; rumIA[i] = rumDeck[--rumTop]; }
    rumDesc = rumDeck[--rumTop];
    rumSel = 0; rumFim = 0; rumVez = 0; rumComprou = 0;
    snprintf(rumMsg, sizeof(rumMsg), "COMPRE (QUADRADO) E DEPOIS DESCARTE (X)");
}
static void rumVezIA() {
    if (rumTop > 0) rumIA[rumNIA++] = rumDeck[--rumTop];
    if (rumNIA > 0) {
        int idx = 0;
        for (int i = 1; i < rumNIA; i++) if (cartaValor52(rumIA[i] / 4) > cartaValor52(rumIA[idx] / 4)) idx = i;
        rumDesc = rumIA[idx];
        for (int i = idx; i < rumNIA - 1; i++) rumIA[i] = rumIA[i + 1];
        rumNIA--;
    }
    if (rumNIA == 0) { rumFim = -1; snprintf(rumMsg, sizeof(rumMsg), "IA BATEU - VOCE PERDEU"); return; }
    rumVez = 0; rumComprou = 0;
    snprintf(rumMsg, sizeof(rumMsg), "SUA VEZ - COMPRE E DESCARTE");
}
static void rumEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (rumFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) rumIniciar(rumModo); return; }
    if (rumNJ > 0) {
        if (novos & ORBIS_PAD_BUTTON_LEFT) rumSel = (rumSel + rumNJ - 1) % rumNJ;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) rumSel = (rumSel + 1) % rumNJ;
        if (rumSel >= rumNJ) rumSel = 0;
        if (novos & ORBIS_PAD_BUTTON_SQUARE) {
            if (rumTop > 0 && !rumComprou) { rumJ[rumNJ++] = rumDeck[--rumTop]; rumComprou = 1; snprintf(rumMsg, sizeof(rumMsg), "COMPROU - AGORA DESCARTE"); }
            else if (rumTop == 0) snprintf(rumMsg, sizeof(rumMsg), "MONTE VAZIO");
            else snprintf(rumMsg, sizeof(rumMsg), "VOCE JA COMPROU - DESCARTE");
        }
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            if (!rumComprou && rumTop > 0) { snprintf(rumMsg, sizeof(rumMsg), "COMPRE PRIMEIRO (QUADRADO)"); return; }
            rumDesc = rumJ[rumSel];
            for (int i = rumSel; i < rumNJ - 1; i++) rumJ[i] = rumJ[i + 1];
            rumNJ--; if (rumSel >= rumNJ) rumSel = rumNJ > 0 ? rumNJ - 1 : 0;
            if (rumNJ == 0) { rumFim = 1; snprintf(rumMsg, sizeof(rumMsg), "VOCE BATEU - VENCEU!"); return; }
            rumVezIA();
        }
    }
}
static void rumDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF1A1A33);
    jogoCentro(tela, rumNome(), 44, 960, 28, 0xFFFFFFFF);
    char t[128];
    snprintf(t, sizeof(t), "MONTE %d    CARTAS DA IA %d    SUAS %d", rumTop, rumNIA, rumNJ);
    jogoCentro(tela, t, 22, 960, 86, 0xFFA9BCD6);
    jogoCentro(tela, "IA", 20, 960, 140, 0xFFA9BCD6);
    for (int i = 0; i < rumNIA; i++) cartasCarta(tela, 660 + (i % 18) * 34, 166 + (i / 18) * 46, 30, 42, 0, true, false, false);
    cartasCarta(tela, 880, 300, 120, 168, rumDesc, false, false, false);
    jogoCentro(tela, "DESCARTE", 20, 960, 484, 0xFFA9BCD6);
    jogoCentro(tela, "SUA MAO", 20, 960, 520, 0xFFA9BCD6);
    cartasFila(tela, rumJ, rumNJ, 550, 58, 80, rumSel, false, false);
    jogoCentro(tela, rumMsg, 24, 960, 680, 0xFFFFFFFF);
    if (rumFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 780, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   QUADRADO COMPRAR   X DESCARTAR   BOLINHA SAIR");
}

// --- coracoes / espadas (4 jogadores) ---
static int cqDeck[52], cqMao[4][13], cqN[4];
static int cqSel, cqFim, cqModo, cqTrick, cqPts[4], cqVencidas[2];
static int cqCarta[4], cqTem[4];
static char cqMsg[160];

static void cqIniciar(int modo) {
    cqModo = modo;
    cartasBaralho52(cqDeck); jogoEmbaralhar(cqDeck, 52);
    int n = 0;
    for (int p = 0; p < 4; p++) { cqN[p] = 13; for (int i = 0; i < 13; i++) cqMao[p][i] = cqDeck[n++]; }
    cqSel = 0; cqFim = 0; cqTrick = 0;
    for (int p = 0; p < 4; p++) { cqPts[p] = 0; cqTem[p] = 0; cqCarta[p] = -1; }
    cqVencidas[0] = cqVencidas[1] = 0;
    snprintf(cqMsg, sizeof(cqMsg), "ESCOLHA UMA CARTA PARA JOGAR");
}
static void cqResolver() {
    int leadSuit = cqCarta[0] % 4;
    int venc = 0;
    for (int p = 1; p < 4; p++) {
        int c = cqCarta[p], s = c % 4, bs = cqCarta[venc] % 4;
        bool tr = (cqModo == 1 && s == 3), btr = (cqModo == 1 && bs == 3);
        if (tr && !btr) venc = p;
        else if (tr == btr) {
            bool seg = s == leadSuit, bseg = bs == leadSuit;
            if (seg && !bseg) venc = p;
            else if (seg == bseg && cartaValor52(c / 4) > cartaValor52(cqCarta[venc] / 4)) venc = p;
        }
    }
    if (cqModo == 0) {
        for (int p = 0; p < 4; p++) {
            int c = cqCarta[p];
            if (c / 4 == 12 && c % 4 == 3) cqPts[p] += 13;
            else if (c % 4 == 2) cqPts[p] += 1;
        }
    } else {
        int eq = (venc == 0 || venc == 2) ? 0 : 1;
        cqVencidas[eq]++;
    }
    cqTrick++;
    for (int p = 0; p < 4; p++) { cqTem[p] = 0; cqCarta[p] = -1; }
    if (cqTrick >= 13) {
        cqFim = 1;
        if (cqModo == 0) {
            int menor = 999;
            for (int p = 0; p < 4; p++) if (cqPts[p] < menor) menor = cqPts[p];
            if (cqPts[0] == menor) snprintf(cqMsg, sizeof(cqMsg), "FIM - VOCE TEM MENOS PONTOS (%d) E VENCEU!", cqPts[0]);
            else snprintf(cqMsg, sizeof(cqMsg), "FIM - VOCE FEZ %d PONTOS", cqPts[0]);
        } else {
            if (cqVencidas[0] > cqVencidas[1]) snprintf(cqMsg, sizeof(cqMsg), "FIM - SUA DUPLA VENCEU %d x %d", cqVencidas[0], cqVencidas[1]);
            else if (cqVencidas[1] > cqVencidas[0]) snprintf(cqMsg, sizeof(cqMsg), "FIM - DUPLA RIVAL VENCEU %d x %d", cqVencidas[0], cqVencidas[1]);
            else snprintf(cqMsg, sizeof(cqMsg), "FIM - EMPATE %d x %d", cqVencidas[0], cqVencidas[1]);
        }
    }
}
static void cqJogar(int p, int idx) {
    cqCarta[p] = cqMao[p][idx];
    for (int i = idx; i < cqN[p] - 1; i++) cqMao[p][i] = cqMao[p][i + 1];
    cqN[p]--; cqTem[p] = 1;
}
static void cqAuto() {
    for (int p = 1; p < 4; p++) {
        if (cqN[p] <= 0) continue;
        int leadSuit = cqCarta[0] % 4;
        int idx = -1;
        for (int i = 0; i < cqN[p]; i++) if (cqMao[p][i] % 4 == leadSuit) { idx = i; break; }
        if (idx < 0) idx = jogoRand() % cqN[p];
        cqJogar(p, idx);
    }
    cqResolver();
}
static void cqEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_CARTAS_MENU; return; }
    if (cqFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) cqIniciar(cqModo); return; }
    if (cqN[0] <= 0) return;
    if (novos & ORBIS_PAD_BUTTON_LEFT) cqSel = (cqSel + cqN[0] - 1) % cqN[0];
    if (novos & ORBIS_PAD_BUTTON_RIGHT) cqSel = (cqSel + 1) % cqN[0];
    if (cqSel >= cqN[0]) cqSel = 0;
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        cqJogar(0, cqSel);
        if (cqSel >= cqN[0]) cqSel = cqN[0] > 0 ? cqN[0] - 1 : 0;
        cqAuto();
    }
}
static void cqDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, cqModo == 0 ? 0xFF2A0A14 : 0xFF0A1A2A);
    jogoCentro(tela, cqModo == 0 ? "CORACOES" : "ESPADAS", 44, 960, 26, 0xFFFFFFFF);
    char t[128];
    if (cqModo == 0) snprintf(t, sizeof(t), "EVITE COPAS E A DAMA DE ESPADAS    VOCE %d pts", cqPts[0]);
    else snprintf(t, sizeof(t), "SUA DUPLA %d x %d RIVAIS    VOCE + PARCEIRO", cqVencidas[0], cqVencidas[1]);
    jogoCentro(tela, t, 20, 960, 86, 0xFFFFD166);
    char tp[64];
    snprintf(tp, sizeof(tp), "IA %d", cqN[1]); jogoCentro(tela, tp, 20, 400, 300, 0xFFA9BCD6);
    snprintf(tp, sizeof(tp), "IA %d", cqN[2]); jogoCentro(tela, tp, 20, 960, 150, 0xFFA9BCD6);
    snprintf(tp, sizeof(tp), "IA %d", cqN[3]); jogoCentro(tela, tp, 20, 1520, 300, 0xFFA9BCD6);
    if (cqTem[1]) cartasCarta(tela, 430, 380, 76, 106, cqCarta[1], false, false, false);
    if (cqTem[2]) cartasCarta(tela, 922, 230, 76, 106, cqCarta[2], false, false, false);
    if (cqTem[3]) cartasCarta(tela, 1414, 380, 76, 106, cqCarta[3], false, false, false);
    if (cqTem[0]) cartasCarta(tela, 922, 430, 76, 106, cqCarta[0], false, false, false);
    jogoCentro(tela, "SUA MAO", 20, 960, 610, 0xFFA9BCD6);
    cartasFila(tela, cqMao[0], cqN[0], 640, 52, 72, cqSel, false, false);
    jogoCentro(tela, cqMsg, 22, 960, 790, 0xFFFFFFFF);
    if (cqFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 860, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR ESCOLHER   X JOGAR   BOLINHA SAIR");
}

// --- freecell ---
static int fcCol[8][52], fcN[8], fcFree[4], fcFound[4];
static int fcCur, fcSel, fcSrcLoc, fcFim;
static char fcMsg[128];

static int fcCor(int c) { return c % 4 == 1 || c % 4 == 2; }
static void fcIniciar() {
    int deck[52]; cartasBaralho52(deck); jogoEmbaralhar(deck, 52);
    int n = 0;
    for (int c = 0; c < 8; c++) {
        fcN[c] = c < 4 ? 7 : 6;
        for (int i = 0; i < fcN[c]; i++) fcCol[c][i] = deck[n++];
    }
    for (int i = 0; i < 4; i++) { fcFree[i] = -1; fcFound[i] = 0; }
    fcCur = 0; fcSel = 0; fcSrcLoc = -1; fcFim = 0;
    snprintf(fcMsg, sizeof(fcMsg), "X ESCOLHE   SETAS MOVEM   TRIANGULO FUNDAMENTO");
}
static int fcTopo(int loc) {
    if (loc < 8) return fcN[loc] > 0 ? fcCol[loc][fcN[loc] - 1] : -1;
    return fcFree[loc - 8];
}
static void fcRemover(int loc) {
    if (loc < 8) fcN[loc]--;
    else fcFree[loc - 8] = -1;
}
static bool fcPodeColuna(int col, int card) {
    if (fcN[col] == 0) return card / 4 == 13;
    int top = fcCol[col][fcN[col] - 1];
    return top / 4 == card / 4 + 1 && fcCor(top) != fcCor(card);
}
static bool fcFundamentar(int card) {
    int s = card % 4, r = card / 4;
    if (fcFound[s] + 1 != r) return false;
    fcFound[s] = r;
    return true;
}
static void fcChecarFim() {
    for (int i = 0; i < 4; i++) if (fcFound[i] != 13) return;
    fcFim = 1;
    snprintf(fcMsg, sizeof(fcMsg), "PARABENS! VOCE COMPLETOU O FREECELL");
}
static void fcEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
        if (fcSel) { fcSel = 0; fcSrcLoc = -1; snprintf(fcMsg, sizeof(fcMsg), "SELECAO CANCELADA"); }
        else jogoAtual = JOGO_CARTAS_MENU;
        return;
    }
    if (fcFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) fcIniciar(); return; }
    if (novos & ORBIS_PAD_BUTTON_LEFT) fcCur = (fcCur + 11) % 12;
    if (novos & ORBIS_PAD_BUTTON_RIGHT) fcCur = (fcCur + 1) % 12;
    if (!fcSel) {
        int card = fcTopo(fcCur);
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            if (card >= 0) { fcSel = 1; fcSrcLoc = fcCur; snprintf(fcMsg, sizeof(fcMsg), "SELECIONADA %s%s", CARTA_R52[card / 4], CARTA_NAIPES[card % 4]); }
        } else if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
            if (card >= 0 && fcFundamentar(card)) {
                fcRemover(fcCur);
                snprintf(fcMsg, sizeof(fcMsg), "FOI PARA O FUNDAMENTO");
                fcChecarFim();
            } else snprintf(fcMsg, sizeof(fcMsg), "NAO PODE IR AO FUNDAMENTO");
        }
        return;
    }
    int card = fcTopo(fcSrcLoc);
    if (card < 0) { fcSel = 0; fcSrcLoc = -1; return; }
    if (novos & ORBIS_PAD_BUTTON_SQUARE) { fcSel = 0; fcSrcLoc = -1; snprintf(fcMsg, sizeof(fcMsg), "SELECAO CANCELADA"); return; }
    if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (fcFundamentar(card)) {
            fcRemover(fcSrcLoc); fcSel = 0; fcSrcLoc = -1;
            snprintf(fcMsg, sizeof(fcMsg), "FOI PARA O FUNDAMENTO");
            fcChecarFim();
        } else snprintf(fcMsg, sizeof(fcMsg), "NAO PODE IR AO FUNDAMENTO");
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        if (fcCur == fcSrcLoc) { fcSel = 0; fcSrcLoc = -1; snprintf(fcMsg, sizeof(fcMsg), "SELECAO CANCELADA"); return; }
        if (fcCur < 8) {
            if (fcPodeColuna(fcCur, card)) {
                fcRemover(fcSrcLoc);
                fcCol[fcCur][fcN[fcCur]++] = card;
                fcSel = 0; fcSrcLoc = -1;
                snprintf(fcMsg, sizeof(fcMsg), "CARTA MOVIDA");
            } else snprintf(fcMsg, sizeof(fcMsg), "MOVIMENTO INVALIDO");
        } else {
            int fi = fcCur - 8;
            if (fcFree[fi] < 0 && fcSrcLoc < 8) {
                fcRemover(fcSrcLoc); fcFree[fi] = card;
                fcSel = 0; fcSrcLoc = -1;
                snprintf(fcMsg, sizeof(fcMsg), "CARTA NA CELULA LIVRE");
            } else snprintf(fcMsg, sizeof(fcMsg), "CELULA OCUPADA");
        }
    }
}
static void fcDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A2A18);
    jogoCentro(tela, "FREECELL", 40, 960, 24, 0xFFFFFFFF);
    for (int i = 0; i < 4; i++) {
        int x = 1300 + i * 130;
        preencherRet(tela, x, 90, 110, 150, 0xFF12351F);
        jogoBorda(tela, x, 90, 110, 150, 0xFF2E6B42, 2);
        if (fcFree[i] >= 0) cartasCarta(tela, x, 90, 110, 150, fcFree[i], false, fcSel && fcSrcLoc == 8 + i, false);
        else jogoCentro(tela, "LIVRE", 18, x + 55, 160, 0xFF5F9E78);
    }
    for (int i = 0; i < 4; i++) {
        int x = 1300 + i * 130;
        preencherRet(tela, x, 270, 110, 150, 0xFF0F2A18);
        jogoBorda(tela, x, 270, 110, 150, 0xFF2E6B42, 2);
        if (fcFound[i] > 0) cartasCarta(tela, x, 270, 110, 150, fcFound[i] * 4 + i, false, false, false);
        else jogoCentro(tela, CARTA_NAIPES[i], 26, x + 55, 330, 0xFF5F9E78);
    }
    for (int c = 0; c < 8; c++) {
        int x = 150 + c * 215;
        if (fcN[c] == 0) {
            preencherRet(tela, x, 460, 140, 190, 0xFF0F2A18);
            jogoBorda(tela, x, 460, 140, 190, fcSel && fcCur == c ? 0xFFFFD166 : 0xFF2E6B42, 2);
        }
        int step = fcN[c] > 0 ? 700 / fcN[c] : 34;
        if (step > 34) step = 34;
        if (step < 12) step = 12;
        for (int i = 0; i < fcN[c]; i++) {
            int y = 460 + i * step;
            bool sel = fcSel && fcSrcLoc == c && i == fcN[c] - 1;
            cartasCarta(tela, x, y, 140, 190, fcCol[c][i], false, sel, false);
        }
        if (fcSel && fcCur == c) jogoBorda(tela, x - 3, 457, 146, step * (fcN[c] > 0 ? fcN[c] : 1) + 193, 0xFFFFD166, 3);
    }
    jogoCentro(tela, fcMsg, 24, 960, 900, 0xFFFFFFFF);
    if (fcFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 960, 0xFFA9BCD6);
    else jogoBarra(tela, "SETAS CURSOR   X SELECIONAR/MOVER   TRIANGULO FUNDAMENTO   QUADRADO CANCELAR");
}

// --- paciencia (klondike) ---
static int pacTab[7][52], pacN[7];
static bool pacUp[7][52];
static int pacStock[52], pacStockN, pacWaste[52], pacWasteN, pacFound[4];
static int pacCur, pacSel, pacSrc, pacFim;
static char pacMsg[128];

static void pacIniciar() {
    int deck[52]; cartasBaralho52(deck); jogoEmbaralhar(deck, 52);
    int n = 0;
    for (int c = 0; c < 7; c++) {
        pacN[c] = c + 1;
        for (int i = 0; i <= c; i++) { pacTab[c][i] = deck[n++]; pacUp[c][i] = (i == c); }
    }
    pacStockN = 0;
    while (n < 52) pacStock[pacStockN++] = deck[n++];
    pacWasteN = 0;
    for (int i = 0; i < 4; i++) pacFound[i] = 0;
    pacCur = 0; pacSel = 0; pacSrc = -1; pacFim = 0;
    snprintf(pacMsg, sizeof(pacMsg), "X NO MONTE COMPRA   ESCOLHA E MOVIMENTE");
}
static bool pacPodeColuna(int col, int card) {
    if (pacN[col] == 0) return card / 4 == 13;
    int top = pacTab[col][pacN[col] - 1];
    return top / 4 == card / 4 + 1 && fcCor(top) != fcCor(card);
}
static bool pacFundamentar(int card) {
    int s = card % 4, r = card / 4;
    if (pacFound[s] + 1 != r) return false;
    pacFound[s] = r;
    return true;
}
static void pacVirarTopo(int col) {
    if (pacN[col] > 0 && !pacUp[col][pacN[col] - 1]) pacUp[col][pacN[col] - 1] = true;
}
static void pacChecarFim() {
    for (int i = 0; i < 4; i++) if (pacFound[i] != 13) return;
    pacFim = 1;
    snprintf(pacMsg, sizeof(pacMsg), "PARABENS! VOCE COMPLETOU A PACIENCIA");
}
static void pacEntrada(uint32_t novos, float dt) {
    (void)dt;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
        if (pacSel) { pacSel = 0; pacSrc = -1; snprintf(pacMsg, sizeof(pacMsg), "SELECAO CANCELADA"); }
        else jogoAtual = JOGO_CARTAS_MENU;
        return;
    }
    if (pacFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) pacIniciar(); return; }
    if (novos & ORBIS_PAD_BUTTON_LEFT) pacCur = (pacCur + 7) % 8;
    if (novos & ORBIS_PAD_BUTTON_RIGHT) pacCur = (pacCur + 1) % 8;
    if (!pacSel) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) {
            if (pacCur == 7) {
                if (pacStockN > 0) { pacWaste[pacWasteN++] = pacStock[--pacStockN]; snprintf(pacMsg, sizeof(pacMsg), "CARTA COMPRADA"); }
                else snprintf(pacMsg, sizeof(pacMsg), "MONTE VAZIO");
            } else if (pacN[pacCur] > 0 && pacUp[pacCur][pacN[pacCur] - 1]) {
                pacSel = 1; pacSrc = pacCur;
                int c = pacTab[pacCur][pacN[pacCur] - 1];
                snprintf(pacMsg, sizeof(pacMsg), "SELECIONADA %s%s", CARTA_R52[c / 4], CARTA_NAIPES[c % 4]);
            } else snprintf(pacMsg, sizeof(pacMsg), "SEM CARTA VIRADA NESSA COLUNA");
        } else if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
            if (pacCur == 7) {
                if (pacWasteN > 0) {
                    pacSel = 1; pacSrc = 7;
                    int c = pacWaste[pacWasteN - 1];
                    snprintf(pacMsg, sizeof(pacMsg), "SELECIONADA %s%s", CARTA_R52[c / 4], CARTA_NAIPES[c % 4]);
                } else snprintf(pacMsg, sizeof(pacMsg), "DESCARTE VAZIO");
            } else if (pacN[pacCur] > 0 && pacUp[pacCur][pacN[pacCur] - 1]) {
                int c = pacTab[pacCur][pacN[pacCur] - 1];
                if (pacFundamentar(c)) { pacN[pacCur]--; pacVirarTopo(pacCur); snprintf(pacMsg, sizeof(pacMsg), "FOI PARA O FUNDAMENTO"); pacChecarFim(); }
                else snprintf(pacMsg, sizeof(pacMsg), "NAO PODE IR AO FUNDAMENTO");
            }
        }
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_SQUARE) { pacSel = 0; pacSrc = -1; snprintf(pacMsg, sizeof(pacMsg), "SELECAO CANCELADA"); return; }
    int card;
    if (pacSrc == 7) card = pacWasteN > 0 ? pacWaste[pacWasteN - 1] : -1;
    else card = pacN[pacSrc] > 0 ? pacTab[pacSrc][pacN[pacSrc] - 1] : -1;
    if (card < 0) { pacSel = 0; pacSrc = -1; return; }
    if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (pacFundamentar(card)) {
            if (pacSrc == 7) pacWasteN--;
            else { pacN[pacSrc]--; pacVirarTopo(pacSrc); }
            pacSel = 0; pacSrc = -1;
            snprintf(pacMsg, sizeof(pacMsg), "FOI PARA O FUNDAMENTO");
            pacChecarFim();
        } else snprintf(pacMsg, sizeof(pacMsg), "NAO PODE IR AO FUNDAMENTO");
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        if (pacCur >= 7) { snprintf(pacMsg, sizeof(pacMsg), "ESCOLHA UMA COLUNA"); return; }
        if (pacCur == pacSrc) { pacSel = 0; pacSrc = -1; snprintf(pacMsg, sizeof(pacMsg), "SELECAO CANCELADA"); return; }
        if (pacPodeColuna(pacCur, card)) {
            if (pacSrc == 7) pacWasteN--;
            else { pacN[pacSrc]--; pacVirarTopo(pacSrc); }
            pacTab[pacCur][pacN[pacCur]++] = card;
            pacUp[pacCur][pacN[pacCur] - 1] = true;
            pacSel = 0; pacSrc = -1;
            snprintf(pacMsg, sizeof(pacMsg), "CARTA MOVIDA");
        } else snprintf(pacMsg, sizeof(pacMsg), "MOVIMENTO INVALIDO");
    }
}
static void pacDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0A2A18);
    jogoCentro(tela, "PACIENCIA (KLONDIKE)", 40, 960, 20, 0xFFFFFFFF);
    preencherRet(tela, 130, 80, 140, 190, 0xFF12351F);
    jogoBorda(tela, 130, 80, 140, 190, pacSel && pacCur == 7 ? 0xFFFFD166 : 0xFF2E6B42, 2);
    if (pacStockN > 0) cartasCarta(tela, 130, 80, 140, 190, 0, true, false, false);
    else jogoCentro(tela, "VAZIO", 20, 200, 175, 0xFF5F9E78);
    preencherRet(tela, 320, 80, 140, 190, 0xFF0F2A18);
    jogoBorda(tela, 320, 80, 140, 190, 0xFF2E6B42, 2);
    if (pacWasteN > 0) cartasCarta(tela, 320, 80, 140, 190, pacWaste[pacWasteN - 1], false, pacSel && pacSrc == 7, false);
    for (int i = 0; i < 4; i++) {
        int x = 1300 + i * 150;
        preencherRet(tela, x, 80, 140, 190, 0xFF0F2A18);
        jogoBorda(tela, x, 80, 140, 190, 0xFF2E6B42, 2);
        if (pacFound[i] > 0) cartasCarta(tela, x, 80, 140, 190, pacFound[i] * 4 + i, false, false, false);
        else jogoCentro(tela, CARTA_NAIPES[i], 26, x + 70, 175, 0xFF5F9E78);
    }
    for (int c = 0; c < 7; c++) {
        int x = 130 + c * 245;
        if (pacN[c] == 0) {
            preencherRet(tela, x, 400, 160, 210, 0xFF0F2A18);
            jogoBorda(tela, x, 400, 160, 210, pacSel && pacCur == c ? 0xFFFFD166 : 0xFF2E6B42, 2);
        }
        int step = pacN[c] > 0 ? 560 / pacN[c] : 40;
        if (step > 40) step = 40;
        if (step < 14) step = 14;
        for (int i = 0; i < pacN[c]; i++) {
            int y = 400 + i * step;
            bool sel = pacSel && pacSrc == c && i == pacN[c] - 1 && pacUp[c][i];
            if (pacUp[c][i]) cartasCarta(tela, x, y, 160, 210, pacTab[c][i], false, sel, false);
            else cartasCarta(tela, x, y, 160, 210, 0, true, false, false);
        }
        if (pacSel && pacCur == c) jogoBorda(tela, x - 3, 397, 166, step * (pacN[c] > 0 ? pacN[c] : 1) + 213, 0xFFFFD166, 3);
    }
    jogoCentro(tela, pacMsg, 24, 960, 900, 0xFFFFFFFF);
    if (pacFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA VOLTAR", 22, 960, 960, 0xFFA9BCD6);
    else jogoBarra(tela, "SETAS CURSOR   X COMPRAR/SELECIONAR   TRIANGULO FUNDAMENTO   QUADRADO CANCELAR");
}

static void fisCirculo(uint32_t* tela, float cx, float cy, float r, uint32_t cor) {
    int ir = (int)(r + 0.5f);
    for (int dy = -ir; dy <= ir; dy++) {
        int dx = (int)sqrtf((float)(ir * ir - dy * dy));
        preencherRet(tela, (int)cx - dx, (int)cy + dy, dx * 2 + 1, 1, cor);
    }
}

static const float PP_FX = 300.0f, PP_FY = 240.0f, PP_FW = 1320.0f, PP_FH = 600.0f;
static const float PP_BR = 12.0f;
static const float PP_PH = 170.0f;
static float ppBallX, ppBallY, ppVX, ppVY;
static float ppPadY, ppIaY;
static int ppPtsJ, ppPtsIA, ppFim;
static float ppEspera, ppFlash;
static char ppMsg[96];

static void ppServir() {
    ppBallX = PP_FX + PP_FW * 0.5f;
    ppBallY = PP_FY + PP_FH * 0.5f;
    ppVX = -430.0f;
    ppVY = (float)((jogoRand() % 200) - 100) * 1.6f;
    ppEspera = 0.9f;
    snprintf(ppMsg, sizeof(ppMsg), "PREPARAR...");
}
static void ppPonto(int quemJ) {
    if (quemJ) ppPtsJ++; else ppPtsIA++;
    if (ppPtsJ >= 7 || ppPtsIA >= 7) {
        ppFim = 1;
        if (ppPtsJ > ppPtsIA) snprintf(ppMsg, sizeof(ppMsg), "VOCE VENCEU %d x %d!", ppPtsJ, ppPtsIA);
        else snprintf(ppMsg, sizeof(ppMsg), "IA VENCEU %d x %d", ppPtsJ, ppPtsIA);
    } else {
        ppServir();
    }
}
static void ppIniciar() {
    ppPtsJ = ppPtsIA = 0; ppFim = 0; ppFlash = 0.0f;
    ppPadY = PP_FY + PP_FH * 0.5f;
    ppIaY = PP_FY + PP_FH * 0.5f;
    ppServir();
}
static void ppRebater() {
    float meia = PP_PH * 0.5f;
    float off = ppBallY - ppPadY;
    float q = fabsf(off) / meia;
    if (q > 1.0f) q = 1.0f;
    float vel = 1100.0f - q * 300.0f;
    float vy = off * 3.6f;
    if (vy > 420.0f) vy = 420.0f;
    if (vy < -420.0f) vy = -420.0f;
    ppVX = vel;
    ppVY = vy;
    float hitX = PP_FX + 64.0f;
    ppBallX = hitX + PP_BR + 4.0f;
    if (q < 0.30f) snprintf(ppMsg, sizeof(ppMsg), "PERFEITO!");
    else if (q < 0.70f) snprintf(ppMsg, sizeof(ppMsg), "BOM!");
    else snprintf(ppMsg, sizeof(ppMsg), "NA PONTA");
    ppFlash = 0.7f;
}
static void ppEntrada(uint32_t novos, uint32_t seg, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoSairCaminhar = 1; return; }
    if (ppFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) ppIniciar(); return; }
    if (ppFlash > 0.0f) ppFlash -= dt;
    if (ppEspera > 0.0f) { ppEspera -= dt; return; }
    if (dt > 0.033f) dt = 0.033f;

    float mvy = 0.0f;
    if (seg & ORBIS_PAD_BUTTON_UP) mvy -= 1.0f;
    if (seg & ORBIS_PAD_BUTTON_DOWN) mvy += 1.0f;
    ppPadY += mvy * 760.0f * dt;
    if (ppPadY < PP_FY + PP_PH * 0.5f) ppPadY = PP_FY + PP_PH * 0.5f;
    if (ppPadY > PP_FY + PP_FH - PP_PH * 0.5f) ppPadY = PP_FY + PP_FH - PP_PH * 0.5f;

    float iaPasso = 430.0f * dt;
    if (ppIaY < ppBallY - 4.0f) ppIaY += iaPasso;
    else if (ppIaY > ppBallY + 4.0f) ppIaY -= iaPasso;
    if (ppIaY < PP_FY + PP_PH * 0.5f) ppIaY = PP_FY + PP_PH * 0.5f;
    if (ppIaY > PP_FY + PP_FH - PP_PH * 0.5f) ppIaY = PP_FY + PP_FH - PP_PH * 0.5f;

    ppBallX += ppVX * dt;
    ppBallY += ppVY * dt;
    if (ppBallY - PP_BR < PP_FY) { ppBallY = PP_FY + PP_BR; ppVY = fabsf(ppVY); }
    if (ppBallY + PP_BR > PP_FY + PP_FH) { ppBallY = PP_FY + PP_FH - PP_BR; ppVY = -fabsf(ppVY); }

    float hitX = PP_FX + 64.0f;
    float meia = PP_PH * 0.5f + PP_BR;
    bool incoming = ppVX < 0.0f;
    bool naFrente = incoming && ppBallX >= hitX - 34.0f && ppBallX <= hitX + 96.0f && fabsf(ppBallY - ppPadY) <= meia;
    if (incoming && (novos & ORBIS_PAD_BUTTON_CROSS)) {
        if (naFrente) ppRebater();
        else if (ppBallX > hitX + 96.0f) { snprintf(ppMsg, sizeof(ppMsg), "CEDO! ESPERA A BOLA"); ppFlash = 0.6f; }
        else { snprintf(ppMsg, sizeof(ppMsg), "FORA DA RAQUETE"); ppFlash = 0.6f; }
    }

    float iax = PP_FX + PP_FW - 64.0f;
    if (ppVX > 0.0f && ppBallX + PP_BR >= iax && fabsf(ppBallY - ppIaY) <= meia) {
        ppBallX = iax - PP_BR;
        ppVX = -fabsf(ppVX) * 1.03f;
        ppVY = (ppBallY - ppIaY) * 3.0f + ppVY * 0.25f;
        if (ppVY > 420.0f) ppVY = 420.0f;
        if (ppVY < -420.0f) ppVY = -420.0f;
    }

    if (ppVX < 0.0f && ppBallX < PP_FX + 24.0f) ppPonto(0);
    else if (ppVX > 0.0f && ppBallX > PP_FX + PP_FW + 40.0f) ppPonto(1);
}
static void ppDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF071A10);
    jogoCentro(tela, "PING PONG", 44, 960, 26, 0xFFFFFFFF);
    preencherRet(tela, (int)PP_FX, (int)PP_FY, (int)PP_FW, (int)PP_FH, 0xFF1C7A55);
    jogoBorda(tela, (int)PP_FX, (int)PP_FY, (int)PP_FW, (int)PP_FH, 0xFFFFFFFF, 4);

    bool incoming = ppVX < 0.0f;
    float hitX = PP_FX + 64.0f;
    float meia = PP_PH * 0.5f + PP_BR;
    bool naZona = incoming && ppBallX >= hitX - 34.0f && ppBallX <= hitX + 96.0f && fabsf(ppBallY - ppPadY) <= meia;

    for (int y = (int)PP_FY; y < (int)(PP_FY + PP_FH); y += 26)
        preencherRet(tela, 956, y, 5, 13, 0xFFFFFFFF);

    preencherRet(tela, (int)(PP_FX + 50.0f), (int)(ppPadY - PP_PH * 0.5f), 14, (int)PP_PH, naZona ? 0xFFFFD166 : 0xFFF1521B);
    preencherRet(tela, (int)(PP_FX + PP_FW - 64.0f), (int)(ppIaY - PP_PH * 0.5f), 14, (int)PP_PH, 0xFF2E6DB4);

    fisCirculo(tela, ppBallX, ppBallY, PP_BR, 0xFFFFFFFF);
    if (naZona) jogoCentro(tela, "AGORA!", 26, (int)(hitX + 20.0f), (int)(ppBallY - 40.0f), 0xFFFFD166);

    jogoCentro(tela, ppMsg, 26, 960, 868, ppFlash > 0.0f ? 0xFFFFD166 : 0xFFDCE6F3);
    char placar[64];
    snprintf(placar, sizeof(placar), "VOCE %d   x   %d IA", ppPtsJ, ppPtsIA);
    jogoCentro(tela, placar, 30, 960, 916, 0xFFFFFFFF);
    if (ppFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA SAIR", 22, 960, 972, 0xFFA9BCD6);
    else jogoBarra(tela, "CIMA/BAIXO MOVER RAQUETE   X BATER NA FRENTE   BOLINHA SAIR");
}

static const float AH_FX = 620.0f, AH_FY = 150.0f, AH_FW = 680.0f, AH_FH = 740.0f;
static const float AH_BR = 14.0f, AH_PR = 36.0f;
static const float AH_GOL = 240.0f;
static float ahBallX, ahBallY, ahVX, ahVY;
static float ahPadX, ahPadY, ahIax, ahIay;
static float ahUltX, ahUltY;
static int ahPtsJ, ahPtsIA, ahFim;
static float ahEspera, ahFlash, ahStuck;
static char ahMsg[96];

static void ahServir() {
    ahBallX = AH_FX + AH_FW * 0.5f;
    ahBallY = AH_FY + AH_FH * 0.5f;
    float ang = (float)((jogoRand() % 101) - 50) / 100.0f * 0.6f;
    ahVX = 520.0f * sinf(ang);
    ahVY = 560.0f;
    ahEspera = 0.9f;
    ahStuck = 0.0f;
    ahUltX = ahBallX; ahUltY = ahBallY;
    snprintf(ahMsg, sizeof(ahMsg), "PREPARAR...");
}
static void ahPonto(int quemJ) {
    if (quemJ) ahPtsJ++; else ahPtsIA++;
    if (ahPtsJ >= 7 || ahPtsIA >= 7) {
        ahFim = 1;
        if (ahPtsJ > ahPtsIA) snprintf(ahMsg, sizeof(ahMsg), "VOCE VENCEU %d x %d!", ahPtsJ, ahPtsIA);
        else snprintf(ahMsg, sizeof(ahMsg), "IA VENCEU %d x %d", ahPtsJ, ahPtsIA);
    } else ahServir();
}
static void ahIniciar() {
    ahPtsJ = ahPtsIA = 0; ahFim = 0; ahFlash = 0.0f; ahStuck = 0.0f;
    ahPadX = AH_FX + AH_FW * 0.5f;
    ahPadY = AH_FY + AH_FH - 105.0f;
    ahIax = AH_FX + AH_FW * 0.5f;
    ahIay = AH_FY + 105.0f;
    ahUltX = AH_FX + AH_FW * 0.5f; ahUltY = AH_FY + AH_FH * 0.5f;
    ahServir();
    ahUltX = ahBallX; ahUltY = ahBallY;
}
static void ahColide(float padX, float padY) {
    float rr = AH_PR + AH_BR;
    float dx = ahBallX - padX, dy = ahBallY - padY;
    float d2 = dx * dx + dy * dy;
    if (d2 < rr * rr && d2 > 0.01f) {
        float d = sqrtf(d2);
        float nx = dx / d, ny = dy / d;
        ahBallX = padX + nx * rr;
        ahBallY = padY + ny * rr;
        float vn = ahVX * nx + ahVY * ny;
        if (vn < 0.0f) {
            ahVX -= 2.0f * vn * nx * 1.02f;
            ahVY -= 2.0f * vn * ny * 1.02f;
            float sp = sqrtf(ahVX * ahVX + ahVY * ahVY);
            if (sp < 210.0f) { ahVX += nx * 210.0f; ahVY += ny * 210.0f; }
        }
    }
}
static void ahEntrada(uint32_t novos, uint32_t seg, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoSairCaminhar = 1; return; }
    if (ahFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) ahIniciar(); return; }
    if (ahFlash > 0.0f) ahFlash -= dt;
    if (ahEspera > 0.0f) { ahEspera -= dt; return; }
    if (dt > 0.033f) dt = 0.033f;

    float mvx = 0.0f, mvy = 0.0f;
    if (seg & ORBIS_PAD_BUTTON_LEFT) mvx -= 1.0f;
    if (seg & ORBIS_PAD_BUTTON_RIGHT) mvx += 1.0f;
    if (seg & ORBIS_PAD_BUTTON_UP) mvy -= 1.0f;
    if (seg & ORBIS_PAD_BUTTON_DOWN) mvy += 1.0f;
    ahPadX += mvx * 980.0f * dt;
    ahPadY += mvy * 840.0f * dt;
    if (ahPadX < AH_FX + AH_PR) ahPadX = AH_FX + AH_PR;
    if (ahPadX > AH_FX + AH_FW - AH_PR) ahPadX = AH_FX + AH_FW - AH_PR;
    if (ahPadY < AH_FY + AH_FH * 0.52f) ahPadY = AH_FY + AH_FH * 0.52f;
    if (ahPadY > AH_FY + AH_FH - AH_PR - 4.0f) ahPadY = AH_FY + AH_FH - AH_PR - 4.0f;

    float alvoX = ahBallX;
    float alvoY = ahBallY;
    if (ahBallX < AH_FX + 110.0f) alvoX = ahBallX + 160.0f;
    else if (ahBallX > AH_FX + AH_FW - 110.0f) alvoX = ahBallX - 160.0f;
    if (ahBallY < AH_FY + 90.0f) alvoY = ahBallY + 150.0f;
    float passo = 440.0f * dt;
    if (ahIax < alvoX - 5.0f) ahIax += passo;
    else if (ahIax > alvoX + 5.0f) ahIax -= passo;
    if (ahIay < alvoY - 5.0f) ahIay += passo * 0.85f;
    else if (ahIay > alvoY + 5.0f) ahIay -= passo * 0.85f;
    if (ahIax < AH_FX + AH_PR) ahIax = AH_FX + AH_PR;
    if (ahIax > AH_FX + AH_FW - AH_PR) ahIax = AH_FX + AH_FW - AH_PR;
    if (ahIay < AH_FY + AH_PR + 4.0f) ahIay = AH_FY + AH_PR + 4.0f;
    if (ahIay > AH_FY + AH_FH * 0.48f) ahIay = AH_FY + AH_FH * 0.48f;

    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        float dx = ahBallX - ahPadX, dy = ahBallY - ahPadY;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < AH_PR + AH_BR + 55.0f) {
            if (d < 1.0f) { dx = 0.0f; dy = -1.0f; d = 1.0f; }
            ahVX = dx / d * 1230.0f;
            ahVY = dy / d * 1230.0f;
            ahFlash = 0.30f;
            snprintf(ahMsg, sizeof(ahMsg), "TACADA!");
        }
    }

    ahBallX += ahVX * dt;
    ahBallY += ahVY * dt;

    if (ahBallX - AH_BR < AH_FX) { ahBallX = AH_FX + AH_BR; ahVX = fabsf(ahVX); }
    if (ahBallX + AH_BR > AH_FX + AH_FW) { ahBallX = AH_FX + AH_FW - AH_BR; ahVX = -fabsf(ahVX); }

    float centro = AH_FX + AH_FW * 0.5f;
    float gol = AH_GOL;
    if (ahBallY - AH_BR < AH_FY) {
        if (fabsf(ahBallX - centro) < gol * 0.5f) {
            if (ahBallY < AH_FY - 30.0f) { ahPonto(1); return; }
        } else { ahBallY = AH_FY + AH_BR; ahVY = fabsf(ahVY); }
    }
    if (ahBallY + AH_BR > AH_FY + AH_FH) {
        if (fabsf(ahBallX - centro) < gol * 0.5f) {
            if (ahBallY > AH_FY + AH_FH + 30.0f) { ahPonto(0); return; }
        } else { ahBallY = AH_FY + AH_FH - AH_BR; ahVY = -fabsf(ahVY); }
    }

    ahColide(ahPadX, ahPadY);
    ahColide(ahIax, ahIay);

    float dpx = ahBallX - ahUltX, dpy = ahBallY - ahUltY;
    if (dpx * dpx + dpy * dpy < 4.0f) ahStuck += dt; else ahStuck = 0.0f;
    ahUltX = ahBallX; ahUltY = ahBallY;
    if (ahStuck > 0.6f) {
        float cx = AH_FX + AH_FW * 0.5f, cy = AH_FY + AH_FH * 0.5f;
        float ddx = cx - ahBallX, ddy = cy - ahBallY;
        float dd = sqrtf(ddx * ddx + ddy * ddy);
        if (dd < 1.0f) { ddx = 0.0f; ddy = 1.0f; dd = 1.0f; }
        if (ahBallY < cy) {
            ahIay += 90.0f;
            if (ahIay > AH_FY + AH_FH * 0.48f) ahIay = AH_FY + AH_FH * 0.48f;
        } else {
            ahPadY -= 90.0f;
            if (ahPadY < AH_FY + AH_FH * 0.52f) ahPadY = AH_FY + AH_FH * 0.52f;
        }
        ahBallX += ddx / dd * 60.0f;
        ahBallY += ddy / dd * 60.0f;
        ahVX = ddx / dd * 540.0f;
        ahVY = ddy / dd * 540.0f;
        ahStuck = 0.0f;
        snprintf(ahMsg, sizeof(ahMsg), "BOLA LIBERADA");
    }
}
static void ahDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF081420);
    jogoCentro(tela, "AIR HOCKEY", 44, 960, 26, 0xFFFFFFFF);
    preencherRet(tela, (int)AH_FX, (int)AH_FY, (int)AH_FW, (int)AH_FH, 0xFFDCEBF8);
    jogoBorda(tela, (int)AH_FX, (int)AH_FY, (int)AH_FW, (int)AH_FH, 0xFFFFFFFF, 4);
    preencherRet(tela, (int)AH_FX, (int)(AH_FY + AH_FH * 0.5f) - 2, (int)AH_FW, 4, 0xFF9DB4CC);
    fisCirculo(tela, AH_FX + AH_FW * 0.5f, AH_FY + AH_FH * 0.5f, 90.0f, 0xFF9DB4CC);
    fisCirculo(tela, AH_FX + AH_FW * 0.5f, AH_FY + AH_FH * 0.5f, 80.0f, 0xFFDCEBF8);
    float centro = AH_FX + AH_FW * 0.5f;
    preencherRet(tela, (int)(centro - AH_GOL * 0.5f), (int)AH_FY - 10, (int)AH_GOL, 14, 0xFF101010);
    preencherRet(tela, (int)(centro - AH_GOL * 0.5f), (int)(AH_FY + AH_FH) - 4, (int)AH_GOL, 14, 0xFF101010);

    fisCirculo(tela, ahPadX, ahPadY, AH_PR + 4.0f, 0xFF7A2E14);
    fisCirculo(tela, ahPadX, ahPadY, AH_PR, 0xFFF1521B);
    fisCirculo(tela, ahIax, ahIay, AH_PR + 4.0f, 0xFF163F6E);
    fisCirculo(tela, ahIax, ahIay, AH_PR, 0xFF2E6DB4);
    fisCirculo(tela, ahBallX, ahBallY, AH_BR, 0xFFFFFFFF);
    if (ahFlash > 0.0f) {
        float dx = ahBallX - ahPadX, dy = ahBallY - ahPadY;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 1.0f) {
            for (int t = 1; t <= 6; t++) {
                float f = t / 6.0f;
                fisCirculo(tela, ahPadX + dx * f, ahPadY + dy * f, 4.0f, 0xFFFFD166);
            }
        }
    }
    jogoCentro(tela, ahMsg, 24, 960, 906, ahFlash > 0.0f ? 0xFFFFD166 : 0xFFDCE6F3);
    char placar[64];
    snprintf(placar, sizeof(placar), "VOCE %d   x   %d IA", ahPtsJ, ahPtsIA);
    jogoCentro(tela, placar, 30, 960, 944, 0xFFFFFFFF);
    if (ahFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA SAIR", 22, 960, 990, 0xFFA9BCD6);
    else jogoBarra(tela, "SETAS MOVER RACLETA   X TACAR PERTO DA BOLA   BOLINHA SAIR");
}

static const float PB_FX = 320.0f, PB_FY = 160.0f, PB_FW = 1280.0f, PB_FH = 710.0f;
static const float PB_BR = 12.0f;
static const int PB_NRODS = 4;
static const int PB_NFIGS[4] = { 1, 2, 5, 3 };
static const float PB_RODY[4] = { 0.90f, 0.74f, 0.56f, 0.36f };
static const float PB_FIG_SP = 76.0f;
static float pbBallX, pbBallY, pbVX, pbVY;
static float pbRodX, pbIaX;
static float pbKickT, pbIaKickT, pbIaCd;
static int pbPtsJ, pbPtsIA, pbFim;
static float pbEspera, pbStuck;
static char pbMsg[96];

static float pbRodY(int r, int ia) {
    float f = PB_RODY[r];
    if (ia) f = 1.0f - f;
    return PB_FY + PB_FH * f;
}
static float pbFigX(float rodX, int figs, int i) {
    return rodX + (i - (figs - 1) * 0.5f) * PB_FIG_SP;
}

static void pbServir() {
    pbBallX = PB_FX + PB_FW * 0.5f;
    pbBallY = PB_FY + PB_FH * 0.5f;
    pbVX = (float)((jogoRand() % 200) - 100) * 1.2f;
    pbVY = 300.0f;
    pbEspera = 0.9f;
    pbStuck = 0.0f;
    snprintf(pbMsg, sizeof(pbMsg), "PREPARAR...");
}
static void pbPonto(int quemJ) {
    if (quemJ) pbPtsJ++; else pbPtsIA++;
    if (pbPtsJ >= 7 || pbPtsIA >= 7) {
        pbFim = 1;
        if (pbPtsJ > pbPtsIA) snprintf(pbMsg, sizeof(pbMsg), "VOCE VENCEU %d x %d!", pbPtsJ, pbPtsIA);
        else snprintf(pbMsg, sizeof(pbMsg), "IA VENCEU %d x %d", pbPtsJ, pbPtsIA);
    } else pbServir();
}
static void pbIniciar() {
    pbPtsJ = pbPtsIA = 0; pbFim = 0; pbStuck = 0.0f;
    pbKickT = pbIaKickT = pbIaCd = 0.0f;
    pbRodX = PB_FX + PB_FW * 0.5f;
    pbIaX = PB_FX + PB_FW * 0.5f;
    pbServir();
}
static void pbFigura(float fx, float fy) {
    float rr = 24.0f + PB_BR;
    float dx = pbBallX - fx, dy = pbBallY - fy;
    float d2 = dx * dx + dy * dy;
    if (d2 < rr * rr && d2 > 0.01f) {
        float d = sqrtf(d2);
        float nx = dx / d, ny = dy / d;
        pbBallX = fx + nx * rr;
        pbBallY = fy + ny * rr;
        float vn = pbVX * nx + pbVY * ny;
        if (vn < 0.0f) {
            pbVX -= 2.0f * vn * nx * 0.90f;
            pbVY -= 2.0f * vn * ny * 0.90f;
        }
    }
}
static int pbChutar(float rodX, float rodY, int figs, int paraCima, uint32_t seg) {
    for (int i = 0; i < figs; i++) {
        float fx = pbFigX(rodX, figs, i);
        float dx = pbBallX - fx;
        float dy = pbBallY - rodY;
        if (fabsf(dx) < 48.0f && fabsf(dy) < 62.0f) {
            float vx = dx * 4.0f;
            if (seg & ORBIS_PAD_BUTTON_LEFT) vx -= 220.0f;
            if (seg & ORBIS_PAD_BUTTON_RIGHT) vx += 220.0f;
            if (vx > 620.0f) vx = 620.0f;
            if (vx < -620.0f) vx = -620.0f;
            pbVX = vx;
            pbVY = paraCima ? -1040.0f : 1040.0f;
            return 1;
        }
    }
    return 0;
}
static void pbEntrada(uint32_t novos, uint32_t seg, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoSairCaminhar = 1; return; }
    if (pbFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) pbIniciar(); return; }
    if (pbEspera > 0.0f) { pbEspera -= dt; return; }
    if (dt > 0.033f) dt = 0.033f;
    if (pbKickT > 0.0f) pbKickT -= dt;
    if (pbIaKickT > 0.0f) pbIaKickT -= dt;
    if (pbIaCd > 0.0f) pbIaCd -= dt;

    float mvx = 0.0f;
    if (seg & ORBIS_PAD_BUTTON_LEFT) mvx -= 1.0f;
    if (seg & ORBIS_PAD_BUTTON_RIGHT) mvx += 1.0f;
    pbRodX += mvx * 820.0f * dt;
    if (pbRodX < PB_FX + 170.0f) pbRodX = PB_FX + 170.0f;
    if (pbRodX > PB_FX + PB_FW - 170.0f) pbRodX = PB_FX + PB_FW - 170.0f;

    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        pbKickT = 0.18f;
        for (int r = 0; r < PB_NRODS; r++)
            pbChutar(pbRodX, pbRodY(r, 0), PB_NFIGS[r], 1, seg);
    }

    float passo = 390.0f * dt;
    if (pbIaX < pbBallX - 6.0f) pbIaX += passo;
    else if (pbIaX > pbBallX + 6.0f) pbIaX -= passo;
    if (pbIaX < PB_FX + 170.0f) pbIaX = PB_FX + 170.0f;
    if (pbIaX > PB_FX + PB_FW - 170.0f) pbIaX = PB_FX + PB_FW - 170.0f;
    if (pbIaCd <= 0.0f) {
        for (int r = 0; r < PB_NRODS; r++) {
            if (pbChutar(pbIaX, pbRodY(r, 1), PB_NFIGS[r], 0, 0)) {
                pbIaKickT = 0.18f;
                pbIaCd = 0.85f;
                break;
            }
        }
    }

    pbBallX += pbVX * dt;
    pbBallY += pbVY * dt;

    if (pbBallX - PB_BR < PB_FX) { pbBallX = PB_FX + PB_BR; pbVX = fabsf(pbVX); }
    if (pbBallX + PB_BR > PB_FX + PB_FW) { pbBallX = PB_FX + PB_FW - PB_BR; pbVX = -fabsf(pbVX); }

    float centro = PB_FX + PB_FW * 0.5f;
    float gol = 300.0f;
    if (pbBallY - PB_BR < PB_FY) {
        if (fabsf(pbBallX - centro) < gol * 0.5f) {
            if (pbBallY < PB_FY - 30.0f) { pbPonto(1); return; }
        } else { pbBallY = PB_FY + PB_BR; pbVY = fabsf(pbVY); }
    }
    if (pbBallY + PB_BR > PB_FY + PB_FH) {
        if (fabsf(pbBallX - centro) < gol * 0.5f) {
            if (pbBallY > PB_FY + PB_FH + 30.0f) { pbPonto(0); return; }
        } else { pbBallY = PB_FY + PB_FH - PB_BR; pbVY = -fabsf(pbVY); }
    }

    for (int r = 0; r < PB_NRODS; r++) {
        float jy = pbRodY(r, 0);
        float iy = pbRodY(r, 1);
        for (int i = 0; i < PB_NFIGS[r]; i++) {
            pbFigura(pbFigX(pbRodX, PB_NFIGS[r], i), jy);
            pbFigura(pbFigX(pbIaX, PB_NFIGS[r], i), iy);
        }
    }

    float sp = sqrtf(pbVX * pbVX + pbVY * pbVY);
    if (sp < 70.0f) pbStuck += dt; else pbStuck = 0.0f;
    if (pbStuck > 1.2f) {
        pbVY = (pbBallY < PB_FY + PB_FH * 0.5f) ? 640.0f : -640.0f;
        pbVX = (float)((jogoRand() % 200) - 100) * 1.6f;
        pbBallY += pbVY * dt * 2.0f;
        pbStuck = 0.0f;
        snprintf(pbMsg, sizeof(pbMsg), "BOLA LIBERADA");
    }
}
static void pbDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0B1A0E);
    jogoCentro(tela, "PEBOLIM", 44, 960, 26, 0xFFFFFFFF);
    preencherRet(tela, (int)PB_FX - 24, (int)PB_FY - 24, (int)PB_FW + 48, (int)PB_FH + 48, 0xFF5A3E2B);
    preencherRet(tela, (int)PB_FX, (int)PB_FY, (int)PB_FW, (int)PB_FH, 0xFF2F6B3A);
    jogoBorda(tela, (int)PB_FX, (int)PB_FY, (int)PB_FW, (int)PB_FH, 0xFFFFFFFF, 4);
    for (int y = (int)PB_FY; y < (int)(PB_FY + PB_FH); y += 26)
        preencherRet(tela, 956, y, 5, 13, 0xFFDCE6F3);

    float centro = PB_FX + PB_FW * 0.5f;
    preencherRet(tela, (int)(centro - 150.0f), (int)PB_FY - 6, 300, 10, 0xFF101010);
    preencherRet(tela, (int)(centro - 150.0f), (int)(PB_FY + PB_FH) - 4, 300, 10, 0xFF101010);

    for (int r = 0; r < PB_NRODS; r++) {
        float jy = pbRodY(r, 0);
        float iy = pbRodY(r, 1);
        preencherRet(tela, (int)PB_FX, (int)jy - 2, (int)PB_FW, 4, 0xFFBFC4CC);
        preencherRet(tela, (int)PB_FX, (int)iy - 2, (int)PB_FW, 4, 0xFFBFC4CC);
        fisCirculo(tela, PB_FX - 14.0f, jy, 7.0f, 0xFFF1521B);
        fisCirculo(tela, PB_FX + PB_FW + 14.0f, jy, 7.0f, 0xFFF1521B);
        fisCirculo(tela, PB_FX - 14.0f, iy, 7.0f, 0xFF2E6DB4);
        fisCirculo(tela, PB_FX + PB_FW + 14.0f, iy, 7.0f, 0xFF2E6DB4);
        float joff = (pbKickT > 0.0f) ? -12.0f : 0.0f;
        float ioff = (pbIaKickT > 0.0f) ? 12.0f : 0.0f;
        for (int i = 0; i < PB_NFIGS[r]; i++) {
            float fx = pbFigX(pbRodX, PB_NFIGS[r], i);
            fisCirculo(tela, fx, jy + joff, 20.0f, 0xFFF1521B);
            fisCirculo(tela, fx, jy + joff, 8.0f, 0xFFFFFFFF);
            float ix = pbFigX(pbIaX, PB_NFIGS[r], i);
            fisCirculo(tela, ix, iy + ioff, 20.0f, 0xFF2E6DB4);
            fisCirculo(tela, ix, iy + ioff, 8.0f, 0xFFFFFFFF);
        }
    }

    fisCirculo(tela, pbBallX, pbBallY, PB_BR, 0xFFFFFFFF);
    jogoCentro(tela, pbMsg, 24, 960, 906, 0xFFDCE6F3);
    char placar[64];
    snprintf(placar, sizeof(placar), "VOCE %d   x   %d IA", pbPtsJ, pbPtsIA);
    jogoCentro(tela, placar, 30, 960, 944, 0xFFFFFFFF);
    if (pbFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA SAIR", 22, 960, 990, 0xFFA9BCD6);
    else jogoBarra(tela, "ESQ/DIR MOVER VARAS   X CHUTAR   BOLINHA SAIR");
}

static const float SN_L = 320.0f, SN_R = 1600.0f, SN_T = 230.0f, SN_B = 830.0f;
static const float SN_RB = 18.0f;
static const uint32_t SN_CORES[16] = {
    0xFFFFFFFF, 0xFFFFD24A, 0xFF2E6DB4, 0xFFE23B2E, 0xFF8B5CD6,
    0xFFFF8A3D, 0xFF2F9E56, 0xFF8B2E3C, 0xFF101010,
    0xFFFFD24A, 0xFF2E6DB4, 0xFFE23B2E, 0xFF8B5CD6,
    0xFFFF8A3D, 0xFF2F9E56, 0xFF8B2E3C
};
static float snX[16], snY[16], snVX[16], snVY[16];
static bool snAtiva[16], snStripe[16], snPoteadaTiro[16];
static float snPoc[6][2];
static float snAng, snPower;
static bool snCarregando, snScratch;
static int snTacadas, snFim, bnModo, bnSel, bnTotal;
static int snTurno, snTiroDe, snCueSel;
static float snIaEspera;
static char snMsg[128];

static void snResetCue() {
    snX[0] = (bnModo == 1) ? SN_L + (SN_R - SN_L) * 0.25f : SN_L + 170.0f;
    snY[0] = (SN_T + SN_B) * 0.5f;
    for (int t = 0; t < 14; t++) {
        bool livre = true;
        for (int i = 1; i < bnTotal; i++) {
            if (!snAtiva[i]) continue;
            float dx = snX[0] - snX[i], dy = snY[0] - snY[i];
            if (dx * dx + dy * dy < 43.0f * 43.0f) { livre = false; break; }
        }
        if (livre) break;
        snY[0] += 44.0f;
        if (snY[0] > SN_B - 44.0f) { snY[0] = SN_T + 44.0f; snX[0] += 44.0f; }
    }
    snVX[0] = snVY[0] = 0.0f;
    snAtiva[0] = true;
}
static int snBolaDoJogador(int turno) {
    int base = (turno == 0) ? 0 : 5;
    for (int i = base; i < base + 5; i++) if (snAtiva[i]) return i;
    return -1;
}
static void snGarantirCue(int turno) {
    int base = (turno == 0) ? 0 : 5;
    if (snCueSel >= base && snCueSel < base + 5 && snAtiva[snCueSel]) return;
    snCueSel = snBolaDoJogador(turno);
}
static void snMontarPocos() {
    snPoc[0][0] = SN_L; snPoc[0][1] = SN_T;
    snPoc[1][0] = SN_R; snPoc[1][1] = SN_T;
    snPoc[2][0] = SN_L; snPoc[2][1] = SN_B;
    snPoc[3][0] = SN_R; snPoc[3][1] = SN_B;
    snPoc[4][0] = (SN_L + SN_R) * 0.5f; snPoc[4][1] = SN_T;
    snPoc[5][0] = (SN_L + SN_R) * 0.5f; snPoc[5][1] = SN_B;
}
static void snIniciar() {
    bnModo = 0; bnSel = 0; bnTotal = 0;
    snFim = 0; snTacadas = 0; snAng = 0.0f; snPower = 0.0f;
    snCarregando = false; snScratch = false;
    snTurno = -1; snTiroDe = -1; snIaEspera = 0.0f; snCueSel = -1;
    for (int i = 0; i < 16; i++) { snAtiva[i] = false; snStripe[i] = false; snPoteadaTiro[i] = false; snVX[i] = snVY[i] = 0.0f; }
    snMontarPocos();
    snprintf(snMsg, sizeof(snMsg), "ESCOLHA O JOGO");
}
static void snNovoJogo(int modo) {
    bnModo = modo;
    snMontarPocos();
    for (int i = 0; i < 16; i++) { snAtiva[i] = false; snStripe[i] = false; snPoteadaTiro[i] = false; snVX[i] = snVY[i] = 0.0f; }
    float meio = (SN_T + SN_B) * 0.5f;
    float midX = (SN_L + SN_R) * 0.5f;
    if (modo == 1) {
        bnTotal = 10;
        float parX = 140.0f;
        float backY = SN_B - SN_RB - 4.0f;
        float frontY = SN_T + SN_RB + 4.0f;
        float sideBotY = (meio + SN_B) * 0.5f;
        float sideTopY = (SN_T + meio) * 0.5f;
        snX[0] = midX - parX;        snY[0] = backY;
        snX[1] = midX + parX;        snY[1] = backY;
        snX[2] = SN_L + SN_RB + 4.0f; snY[2] = sideBotY;
        snX[3] = SN_R - SN_RB - 4.0f; snY[3] = sideBotY;
        snX[4] = midX;               snY[4] = SN_B - 110.0f;
        for (int i = 0; i < 5; i++) snAtiva[i] = true;
        for (int i = 5; i < 10; i++) {
            snX[i] = snX[i - 5];
            snY[i] = (SN_T + SN_B) - snY[i - 5];
            snAtiva[i] = true;
        }
        snTurno = 0;
        snCueSel = 0;
        snprintf(snMsg, sizeof(snMsg), "SINUCA - VOCE AZUL x IA AMARELA (5 CADA)");
    } else {
        bnTotal = 16;
        float baseX = SN_L + (SN_R - SN_L) * 0.64f;
        int idx = 1;
        for (int r = 0; r < 5; r++) {
            for (int k = 0; k <= r; k++, idx++) {
                snX[idx] = baseX + r * 34.0f;
                snY[idx] = meio + (k - r * 0.5f) * 38.0f;
                snAtiva[idx] = true;
                snStripe[idx] = (idx >= 9);
            }
        }
        snTurno = -1;
        snCueSel = -1;
        snResetCue();
        snprintf(snMsg, sizeof(snMsg), "BILHAR - LIMPE A MESA");
    }
    for (int i = bnTotal; i < 16; i++) snAtiva[i] = false;
    snAng = 0.0f; snPower = 0.0f; snCarregando = false; snScratch = false;
    snTiroDe = -1; snIaEspera = 0.0f;
    snTacadas = 0; snFim = 0;
}
static bool snTudoParado() {
    for (int i = 0; i < 16; i++) {
        if (!snAtiva[i]) continue;
        if (fabsf(snVX[i]) > 1.0f || fabsf(snVY[i]) > 1.0f) return false;
    }
    return true;
}
static void snFisica(float h) {
    for (int i = 0; i < bnTotal; i++) {
        if (!snAtiva[i]) continue;
        float sp = sqrtf(snVX[i] * snVX[i] + snVY[i] * snVY[i]);
        if (sp > 2400.0f) { snVX[i] *= 2400.0f / sp; snVY[i] *= 2400.0f / sp; sp = 2400.0f; }
        if (sp > 0.0f) {
            float ns = sp - 430.0f * h;
            if (ns < 0.0f) ns = 0.0f;
            snVX[i] *= ns / sp;
            snVY[i] *= ns / sp;
            if (ns < 5.0f) { snVX[i] = snVY[i] = 0.0f; }
        }
        snX[i] += snVX[i] * h;
        snY[i] += snVY[i] * h;

        bool encacapada = false;
        for (int p = 0; p < 6; p++) {
            float dxp = snX[i] - snPoc[p][0], dyp = snY[i] - snPoc[p][1];
            if (dxp * dxp + dyp * dyp < 30.0f * 30.0f) {
                snAtiva[i] = false;
                snVX[i] = snVY[i] = 0.0f;
                snPoteadaTiro[i] = true;
                if (bnModo == 1) {
                    snprintf(snMsg, sizeof(snMsg), i < 5 ? "AZUL NA CACAPA" : "AMARELA NA CACAPA");
                } else if (i == 0) {
                    snScratch = true;
                    snprintf(snMsg, sizeof(snMsg), "BRANCA NA CACAPA - FALTA");
                } else {
                    snprintf(snMsg, sizeof(snMsg), "BOLA %d NA CACAPA", i);
                }
                encacapada = true;
                break;
            }
        }
        if (encacapada) continue;

        if (snX[i] - SN_RB < SN_L) { snX[i] = SN_L + SN_RB; snVX[i] = fabsf(snVX[i]) * 0.88f; }
        if (snX[i] + SN_RB > SN_R) { snX[i] = SN_R - SN_RB; snVX[i] = -fabsf(snVX[i]) * 0.88f; }
        if (snY[i] - SN_RB < SN_T) { snY[i] = SN_T + SN_RB; snVY[i] = fabsf(snVY[i]) * 0.88f; }
        if (snY[i] + SN_RB > SN_B) { snY[i] = SN_B - SN_RB; snVY[i] = -fabsf(snVY[i]) * 0.88f; }
    }
    for (int i = 0; i < bnTotal; i++) {
        if (!snAtiva[i]) continue;
        for (int j = i + 1; j < bnTotal; j++) {
            if (!snAtiva[j]) continue;
            float dx = snX[j] - snX[i], dy = snY[j] - snY[i];
            float d2 = dx * dx + dy * dy;
            float rr = SN_RB * 2.0f;
            if (d2 < rr * rr && d2 > 0.001f) {
                float d = sqrtf(d2);
                float nx = dx / d, ny = dy / d;
                float over = (rr - d) * 0.5f;
                snX[i] -= nx * over; snY[i] -= ny * over;
                snX[j] += nx * over; snY[j] += ny * over;
                float p = (snVX[i] - snVX[j]) * nx + (snVY[i] - snVY[j]) * ny;
                if (p > 0.0f) {
                    float imp = p * 0.97f;
                    snVX[i] -= imp * nx; snVY[i] -= imp * ny;
                    snVX[j] += imp * nx; snVY[j] += imp * ny;
                }
            }
        }
    }
}
static void snAvaliarTiro() {
    int de = snTiroDe;
    snTiroDe = -1;
    if (de < 0) return;

    if (bnModo == 1) {
        int base = (de == 0) ? 0 : 5;
        int oppBase = (de == 0) ? 5 : 0;
        int matei = 0, doOutro = 0, restam = 0;
        for (int i = base; i < base + 5; i++) {
            if (snPoteadaTiro[i]) matei++;
            if (snAtiva[i]) restam++;
        }
        for (int i = oppBase; i < oppBase + 5; i++) if (snPoteadaTiro[i]) doOutro++;
        if (restam == 0) {
            snFim = 1;
            if (de == 0) snprintf(snMsg, sizeof(snMsg), "VOCE MATOU TODAS AS SUAS BOLAS!");
            else snprintf(snMsg, sizeof(snMsg), "IA MATOU TODAS AS DELA");
        } else {
            if (doOutro > 0) {
                snTurno = 1 - de;
                snIaEspera = 0.9f;
                if (de == 0) snprintf(snMsg, sizeof(snMsg), "FALTA! VEZ DA IA");
                else snprintf(snMsg, sizeof(snMsg), "IA FALTOU! SUA VEZ");
            } else if (matei > 0) {
                snTurno = de;
                if (de == 0) snprintf(snMsg, sizeof(snMsg), "BOA! CONTINUE MATANDO AS AZUIS");
                else snprintf(snMsg, sizeof(snMsg), "IA CONTINUA");
            } else {
                snTurno = 1 - de;
                snIaEspera = 0.9f;
                if (de == 0) snprintf(snMsg, sizeof(snMsg), "VEZ DA IA");
                else snprintf(snMsg, sizeof(snMsg), "SUA VEZ: MATE AS AZUIS");
            }
            if (snTurno == 0) snGarantirCue(0);
        }
    } else {
        if (snScratch) { snResetCue(); snScratch = false; }
        bool restam = false;
        for (int i = 1; i < bnTotal; i++) if (snAtiva[i]) { restam = true; break; }
        if (!restam) {
            snFim = 1;
            snprintf(snMsg, sizeof(snMsg), "MESA LIMPA EM %d TACADAS!", snTacadas);
        }
    }
    for (int i = 0; i < 16; i++) snPoteadaTiro[i] = false;
}
static void snIaTacar() {
    if (bnModo == 1) {
        int sel = -1;
        float melhor = 1e18f, melhorAng = 0.0f;
        for (int i = 5; i < 10; i++) {
            if (!snAtiva[i]) continue;
            for (int p = 0; p < 6; p++) {
                float dxp = snPoc[p][0] - snX[i], dyp = snPoc[p][1] - snY[i];
                float dp = sqrtf(dxp * dxp + dyp * dyp);
                if (dp < 60.0f) continue;
                if (dp < melhor) { melhor = dp; sel = i; melhorAng = atan2f(dyp, dxp); }
            }
        }
        if (sel < 0) {
            snFim = 1;
            snprintf(snMsg, sizeof(snMsg), "FIM DE JOGO");
            return;
        }
        float err = (float)((jogoRand() % 100) - 50) / 100.0f * 0.09f;
        snCueSel = sel;
        snAng = melhorAng + err;
        float sp = 520.0f + (float)(jogoRand() % 50) / 100.0f * 700.0f;
        snVX[sel] = cosf(snAng) * sp;
        snVY[sel] = sinf(snAng) * sp;
        for (int i = 0; i < 16; i++) snPoteadaTiro[i] = false;
        snScratch = false;
        snTiroDe = 1;
        snIaEspera = 0.0f;
        snprintf(snMsg, sizeof(snMsg), "IA TACOU");
        return;
    }
    if (!snAtiva[0]) snResetCue();
    float melhor = 1e18f, melhorAng = 0.0f;
    int alvo = -1;
    for (int i = 2; i < bnTotal; i++) {
        if (!snAtiva[i]) continue;
        if (bnModo == 1 && (i % 2) != 1) continue;
        for (int p = 0; p < 6; p++) {
            float dxp = snPoc[p][0] - snX[i], dyp = snPoc[p][1] - snY[i];
            float dp = sqrtf(dxp * dxp + dyp * dyp);
            if (dp < 1.0f) continue;
            float gx = snX[i] - dxp / dp * (SN_RB * 2.0f);
            float gy = snY[i] - dyp / dp * (SN_RB * 2.0f);
            float dxc = gx - snX[0], dyc = gy - snY[0];
            float d = sqrtf(dxc * dxc + dyc * dyc) + dp * 0.35f;
            if (d < melhor) { melhor = d; alvo = i; melhorAng = atan2f(dyc, dxc); }
        }
    }
    if (alvo < 0) {
        snFim = 1;
        snprintf(snMsg, sizeof(snMsg), "FIM DE JOGO");
        return;
    }
    float err = (float)((jogoRand() % 100) - 50) / 100.0f * 0.06f;
    snAng = melhorAng + err;
    float p = 0.55f + (float)(jogoRand() % 40) / 100.0f;
    float sp = 480.0f + p * 980.0f;
    snVX[0] = cosf(snAng) * sp;
    snVY[0] = sinf(snAng) * sp;
    for (int i = 0; i < 16; i++) snPoteadaTiro[i] = false;
    snScratch = false;
    snTiroDe = 1;
    snIaEspera = 0.0f;
    snprintf(snMsg, sizeof(snMsg), "IA TACOU");
}
static void snEntrada(uint32_t novos, uint32_t seg, float dt) {
    if (bnModo == 0) {
        if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoSairCaminhar = 1; return; }
        if (novos & ORBIS_PAD_BUTTON_LEFT) bnSel = 0;
        if (novos & ORBIS_PAD_BUTTON_RIGHT) bnSel = 1;
        if (novos & ORBIS_PAD_BUTTON_CROSS) snNovoJogo(bnSel ? 2 : 1);
        return;
    }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoSairCaminhar = 1; return; }
    if (snFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) snIniciar(); return; }
    if (dt > 0.033f) dt = 0.033f;

    bool parado = snTudoParado();
    if (parado && snTiroDe >= 0) {
        snAvaliarTiro();
        parado = snTudoParado();
    }
    if (!snFim && parado) {
        if (bnModo == 1) {
            if (snTurno == 1) {
                if (snIaEspera > 0.0f) snIaEspera -= dt;
                else snIaTacar();
            } else {
                snGarantirCue(0);
                if ((novos & ORBIS_PAD_BUTTON_UP) || (novos & ORBIS_PAD_BUTTON_DOWN)) {
                    int dir = (novos & ORBIS_PAD_BUTTON_DOWN) ? 1 : -1;
                    int nxt = snCueSel;
                    for (int t = 0; t < 5; t++) {
                        nxt += dir;
                        if (nxt < 0) nxt = 4;
                        if (nxt > 4) nxt = 0;
                        if (snAtiva[nxt]) { snCueSel = nxt; break; }
                    }
                    snCarregando = false;
                    snPower = 0.0f;
                }
                if (seg & ORBIS_PAD_BUTTON_LEFT) snAng -= 1.9f * dt;
                if (seg & ORBIS_PAD_BUTTON_RIGHT) snAng += 1.9f * dt;
                bool segX = (seg & ORBIS_PAD_BUTTON_CROSS) != 0;
                if (segX) {
                    snCarregando = true;
                    snPower += 1.4f * dt;
                    if (snPower > 1.0f) snPower = 1.0f;
                } else if (snCarregando) {
                    if (snPower > 0.06f && snCueSel >= 0 && snAtiva[snCueSel]) {
                        float p = snPower;
                        float sp = 420.0f + p * 1380.0f;
                        snVX[snCueSel] = cosf(snAng) * sp;
                        snVY[snCueSel] = sinf(snAng) * sp;
                        snTacadas++;
                        for (int i = 0; i < 16; i++) snPoteadaTiro[i] = false;
                        snScratch = false;
                        snTiroDe = 0;
                        snprintf(snMsg, sizeof(snMsg), "TACADA %d   FORCA %d%%", snTacadas, (int)(p * 100.0f));
                    }
                    snCarregando = false;
                    snPower = 0.0f;
                }
            }
        } else {
            if (seg & ORBIS_PAD_BUTTON_LEFT) snAng -= 1.9f * dt;
            if (seg & ORBIS_PAD_BUTTON_RIGHT) snAng += 1.9f * dt;
            bool segX = (seg & ORBIS_PAD_BUTTON_CROSS) != 0;
            if (segX) {
                snCarregando = true;
                snPower += 1.4f * dt;
                if (snPower > 1.0f) snPower = 1.0f;
            } else if (snCarregando) {
                if (snPower > 0.06f && snAtiva[0]) {
                    float p = snPower;
                    float sp = 420.0f + p * 1380.0f;
                    snVX[0] = cosf(snAng) * sp;
                    snVY[0] = sinf(snAng) * sp;
                    snTacadas++;
                    for (int i = 0; i < 16; i++) snPoteadaTiro[i] = false;
                    snScratch = false;
                    snTiroDe = 0;
                    snprintf(snMsg, sizeof(snMsg), "TACADA %d   FORCA %d%%", snTacadas, (int)(p * 100.0f));
                }
                snCarregando = false;
                snPower = 0.0f;
            }
        }
    }

    for (int k = 0; k < 4; k++) snFisica(dt * 0.25f);
}
static void snDesenharBola(uint32_t* tela, int i) {
    float r = SN_RB;
    if (bnModo == 1) {
        uint32_t cor = (i < 5) ? 0xFF2E6DB4 : 0xFFFFD24A;
        if (i == snCueSel) fisCirculo(tela, snX[i], snY[i], r + 3.0f, 0xFF101010);
        fisCirculo(tela, snX[i], snY[i], r, cor);
        fisCirculo(tela, snX[i] - 5.0f, snY[i] - 6.0f, 4.0f, 0xFFFFFFFF);
        return;
    }
    if (snStripe[i]) {
        fisCirculo(tela, snX[i], snY[i], r, 0xFFFFFFFF);
        fisCirculo(tela, snX[i], snY[i] - 7.0f, r - 1.0f, SN_CORES[i]);
        fisCirculo(tela, snX[i], snY[i] + 7.0f, r - 1.0f, SN_CORES[i]);
        preencherRet(tela, (int)(snX[i] - r + 1.0f), (int)(snY[i] - 3.0f), (int)(r * 2.0f - 2.0f), 6, 0xFFFFFFFF);
    } else {
        fisCirculo(tela, snX[i], snY[i], r, SN_CORES[i]);
    }
    char num[4];
    snprintf(num, sizeof(num), "%d", i);
    jogoCentro(tela, num, 14, (int)snX[i], (int)(snY[i] - 7.0f), i == 8 ? 0xFFFFFFFF : 0xFF101010);
}
static void snDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF1A1008);
    const char* titulo = bnModo == 0 ? "ESCOLHA O JOGO" : (bnModo == 1 ? "SINUCA" : "BILHAR");
    jogoCentro(tela, titulo, 44, 960, 24, 0xFFFFFFFF);
    preencherRet(tela, (int)SN_L - 28, (int)SN_T - 28, (int)(SN_R - SN_L) + 56, (int)(SN_B - SN_T) + 56, 0xFF5A3E2B);
    preencherRet(tela, (int)SN_L, (int)SN_T, (int)(SN_R - SN_L), (int)(SN_B - SN_T), 0xFF0E6B3A);
    for (int p = 0; p < 6; p++)
        fisCirculo(tela, snPoc[p][0], snPoc[p][1], 24.0f, 0xFF121212);

    for (int i = 15; i >= 0; i--) {
        if (!snAtiva[i]) continue;
        snDesenharBola(tela, i);
    }

    bool parado = snTudoParado();
    bool vezJogador = !(bnModo == 1 && snTurno == 1);
    int shooter = (bnModo == 1) ? snCueSel : 0;
    bool temShooter = (shooter >= 0 && shooter < 16 && snAtiva[shooter]);
    if (bnModo != 0 && temShooter && parado && !snFim && vezJogador) {
        float bx = snX[shooter], by = snY[shooter];
        float dirx = cosf(snAng), diry = sinf(snAng);
        for (int t = 1; t <= 22; t++) {
            float f = t / 22.0f;
            fisCirculo(tela, bx + dirx * (SN_RB + 12.0f + f * 190.0f),
                       by + diry * (SN_RB + 12.0f + f * 190.0f), 3.0f, 0xFFFFD166);
        }
        for (int t = 0; t < 26; t++) {
            float f = t / 25.0f;
            float px = bx - dirx * (SN_RB + 8.0f + f * (120.0f + snPower * 70.0f));
            float py = by - diry * (SN_RB + 8.0f + f * (120.0f + snPower * 70.0f));
            fisCirculo(tela, px, py, 4.0f, 0xFFB98A4A);
        }
    }

    char placar[64];
    if (bnModo == 1) {
        int azuis = 0, amarelas = 0;
        for (int i = 0; i < 5; i++) if (snAtiva[i]) azuis++;
        for (int i = 5; i < 10; i++) if (snAtiva[i]) amarelas++;
        jogoCentro(tela, snMsg, 22, 960, 866, 0xFFDCE6F3);
        snprintf(placar, sizeof(placar), "BOLAS NA MESA: AZUIS %d   AMARELAS %d", azuis, amarelas);
        jogoCentro(tela, placar, 24, 960, 898, 0xFFFFFFFF);
        snprintf(placar, sizeof(placar), "VEZ: %s", snTurno == 0 ? "VOCE (MATE AS AZUIS)" : "IA (MATE AS AMARELAS)");
        jogoCentro(tela, placar, 20, 960, 928, 0xFFFFD166);
    } else {
        jogoCentro(tela, snMsg, 22, 960, 878, 0xFFDCE6F3);
        snprintf(placar, sizeof(placar), "TACADAS %d", snTacadas);
        jogoCentro(tela, placar, 26, 960, 912, 0xFFFFD166);
    }

    preencherRet(tela, 660, 960, 600, 20, 0xFF0A1220);
    jogoBorda(tela, 660, 960, 600, 20, 0xFF9DB4CC, 2);
    preencherRet(tela, 662, 962, (int)(596.0f * snPower), 16, snCarregando ? 0xFFFFD166 : 0xFF2F9E56);
    desenharTexto(tela, "FORCA", 20, 560, 958, 0xFFDCE6F3);

    if (bnModo == 0) {
        preencherRet(tela, 560, 350, 800, 360, 0xF00A1A33);
        jogoBorda(tela, 560, 350, 800, 360, 0xFFFFD166, 3);
        jogoCentro(tela, "SINUCA OU BILHAR?", 34, 960, 378, 0xFFFFFFFF);
        const char* op0 = "SINUCA - 5 BOLAS CADA (SEM BRANCA)";
        const char* op1 = "BILHAR - 15 BOLAS";
        if (bnSel == 0) preencherRet(tela, 640, 452, 640, 64, 0xFF1C4A7A);
        else preencherRet(tela, 640, 452, 640, 64, 0xFF10233D);
        jogoCentro(tela, op0, 30, 960, 466, bnSel == 0 ? 0xFFFFD166 : 0xFFDCE6F3);
        if (bnSel == 1) preencherRet(tela, 640, 540, 640, 64, 0xFF1C4A7A);
        else preencherRet(tela, 640, 540, 640, 64, 0xFF10233D);
        jogoCentro(tela, op1, 30, 960, 554, bnSel == 1 ? 0xFFFFD166 : 0xFFDCE6F3);
        jogoCentro(tela, "SINUCA: SEM BOLA BRANCA - BATA DIRETO NA SUA BOLA.", 18, 960, 612, 0xFFA9BCD6);
        jogoCentro(tela, "SINUCA: VOCE E AZUL; A IA E AMARELA. MATE AS SUAS 5.", 18, 960, 634, 0xFFA9BCD6);
        jogoCentro(tela, "BILHAR: LIMPE TODAS AS BOLAS DA MESA.", 18, 960, 656, 0xFFA9BCD6);
        jogoCentro(tela, "ESQ/DIR ESCOLHER   X CONFIRMAR   BOLINHA SAIR", 20, 960, 686, 0xFFA9BCD6);
    } else if (snFim) {
        jogoCentro(tela, "X VOLTAR A ESCOLHA   BOLINHA SAIR", 22, 960, 1000, 0xFFA9BCD6);
    } else {
        jogoBarra(tela, "ESQ/DIR MIRAR   CIMA/BAIXO TROCAR BOLA   SEGURE X E SOLTE   BOLINHA SAIR");
    }
}

enum { ESP_VOLEI = 1, ESP_TENIS = 2, ESP_FUTEBOL = 3 };
static int espModo, espPtsJ, espPtsIA, espFim;
static float espBX, espBY, espVX, espVY;
static float espJX, espJY, espIAX, espIAY;
static float espEspera, espJCD, espIACD;
static char espMsg[96];

static const float ESPL = 320.0f, ESPR = 1600.0f, ESPT = 210.0f, ESPB = 860.0f;
static const float ESPBR = 16.0f;
static const float ESPNET = 960.0f;

static void espServir() {
    espBX = ESPNET;
    if (espModo == ESP_FUTEBOL) {
        espBY = (ESPT + ESPB) * 0.5f;
        espVX = 0.0f;
        espVY = 0.0f;
    } else {
        espBY = ESPT + 180.0f;
        espVX = (jogoRand() & 1) ? 150.0f : -150.0f;
        espVY = 180.0f;
    }
    espEspera = 0.8f;
    espJCD = 0.0f;
    espIACD = 0.0f;
    snprintf(espMsg, sizeof(espMsg), "PREPARAR...");
}
static void espPonto(int quem) {
    if (quem) espPtsJ++; else espPtsIA++;
    int alvo = (espModo == ESP_FUTEBOL) ? 5 : 7;
    if (espPtsJ >= alvo || espPtsIA >= alvo) {
        espFim = 1;
        if (espPtsJ > espPtsIA) snprintf(espMsg, sizeof(espMsg), "VOCE VENCEU %d x %d!", espPtsJ, espPtsIA);
        else snprintf(espMsg, sizeof(espMsg), "IA VENCEU %d x %d", espPtsJ, espPtsIA);
    } else espServir();
}
static void espIniciar(int modo) {
    espModo = modo;
    espPtsJ = espPtsIA = 0; espFim = 0;
    float meioY = (ESPT + ESPB) * 0.5f;
    espJY = espIAY = ESPB - 60.0f;
    espJX = ESPL + (ESPNET - ESPL) * 0.5f;
    espIAX = ESPNET + (ESPR - ESPNET) * 0.5f;
    if (modo == ESP_FUTEBOL) {
        espJX = ESPL + 300.0f; espJY = meioY;
        espIAX = ESPR - 300.0f; espIAY = meioY;
    }
    espServir();
}
static void espEntrada(uint32_t novos, uint32_t seg, float dt) {
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoSairCaminhar = 1; return; }
    if (espFim) { if (novos & ORBIS_PAD_BUTTON_CROSS) espIniciar(espModo); return; }
    if (dt > 0.033f) dt = 0.033f;
    if (espEspera > 0.0f) { espEspera -= dt; return; }

    if (espModo == ESP_FUTEBOL) {
        float mv = 520.0f * dt;
        if (seg & ORBIS_PAD_BUTTON_UP) espJY -= mv;
        if (seg & ORBIS_PAD_BUTTON_DOWN) espJY += mv;
        if (seg & ORBIS_PAD_BUTTON_LEFT) espJX -= mv;
        if (seg & ORBIS_PAD_BUTTON_RIGHT) espJX += mv;
        if (espJX < ESPL + 40.0f) espJX = ESPL + 40.0f;
        if (espJX > ESPR - 40.0f) espJX = ESPR - 40.0f;
        if (espJY < ESPT + 40.0f) espJY = ESPT + 40.0f;
        if (espJY > ESPB - 40.0f) espJY = ESPB - 40.0f;

        if (espJCD > 0.0f) espJCD -= dt;
        if (espIACD > 0.0f) espIACD -= dt;

        float passoIA = 430.0f * dt;
        if (espIAX < espBX - 8.0f) espIAX += passoIA;
        else if (espIAX > espBX + 8.0f) espIAX -= passoIA;
        if (espIAY < espBY - 8.0f) espIAY += passoIA;
        else if (espIAY > espBY + 8.0f) espIAY -= passoIA;
        if (espIAX < ESPL + 40.0f) espIAX = ESPL + 40.0f;
        if (espIAX > ESPR - 40.0f) espIAX = ESPR - 40.0f;
        if (espIAY < ESPT + 40.0f) espIAY = ESPT + 40.0f;
        if (espIAY > ESPB - 40.0f) espIAY = ESPB - 40.0f;

        if ((novos & ORBIS_PAD_BUTTON_CROSS) && espJCD <= 0.0f) {
            float dx = espBX - espJX, dy = espBY - espJY;
            if (dx * dx + dy * dy < 80.0f * 80.0f) {
                espVX = 980.0f;
                espVY = (espBY - espJY) * 4.0f + (float)((int)(jogoRand() % 120) - 60);
                espJCD = 0.35f;
                snprintf(espMsg, sizeof(espMsg), "CHUTE!");
            }
        }
        if (espIACD <= 0.0f) {
            float dx = espBX - espIAX, dy = espBY - espIAY;
            if (dx * dx + dy * dy < 80.0f * 80.0f) {
                espVX = -940.0f;
                espVY = (espBY - espIAY) * 3.6f + (float)((int)(jogoRand() % 100) - 50);
                espIACD = 0.7f;
            }
        }

        float fr = 1.0f - 0.9f * dt;
        espVX *= fr; espVY *= fr;
        espBX += espVX * dt; espBY += espVY * dt;
        if (fabsf(espVX) < 8.0f) espVX = 0.0f;
        if (fabsf(espVY) < 8.0f) espVY = 0.0f;

        float dxj = espBX - espJX, dyj = espBY - espJY;
        float dj = sqrtf(dxj * dxj + dyj * dyj);
        if (dj < 46.0f && dj > 0.01f) {
            espBX = espJX + dxj / dj * 46.0f;
            espBY = espJY + dyj / dj * 46.0f;
            espVX += dxj / dj * 90.0f;
            espVY += dyj / dj * 90.0f;
        }
        float dxi = espBX - espIAX, dyi = espBY - espIAY;
        float di = sqrtf(dxi * dxi + dyi * dyi);
        if (di < 46.0f && di > 0.01f) {
            espBX = espIAX + dxi / di * 46.0f;
            espBY = espIAY + dyi / di * 46.0f;
            espVX += dxi / di * 110.0f;
            espVY += dyi / di * 110.0f;
        }

        if (espBY - ESPBR < ESPT) { espBY = ESPT + ESPBR; espVY = fabsf(espVY) * 0.82f; }
        if (espBY + ESPBR > ESPB) { espBY = ESPB - ESPBR; espVY = -fabsf(espVY) * 0.82f; }

        float gol = 105.0f;
        float golMeioY = (ESPT + ESPB) * 0.5f;
        if (espBX - ESPBR < ESPL) {
            if (fabsf(espBY - golMeioY) < gol) { espPonto(0); return; }
            espBX = ESPL + ESPBR; espVX = fabsf(espVX) * 0.8f;
        }
        if (espBX + ESPBR > ESPR) {
            if (fabsf(espBY - golMeioY) < gol) { espPonto(1); return; }
            espBX = ESPR - ESPBR; espVX = -fabsf(espVX) * 0.8f;
        }
        return;
    }

    float mv = 0.0f;
    if (seg & ORBIS_PAD_BUTTON_LEFT) mv -= 1.0f;
    if (seg & ORBIS_PAD_BUTTON_RIGHT) mv += 1.0f;
    espJX += mv * 780.0f * dt;
    if (espJX < ESPL + 85.0f) espJX = ESPL + 85.0f;
    if (espJX > ESPNET - 85.0f) espJX = ESPNET - 85.0f;

    float alvoX = (espBX < ESPNET) ? (ESPNET + ESPR) * 0.5f : espBX;
    if (alvoX < ESPNET + 85.0f) alvoX = ESPNET + 85.0f;
    if (alvoX > ESPR - 85.0f) alvoX = ESPR - 85.0f;
    float passo = (espModo == ESP_VOLEI ? 500.0f : 560.0f) * dt;
    if (espIAX < alvoX - 5.0f) espIAX += passo;
    else if (espIAX > alvoX + 5.0f) espIAX -= passo;
    if (espIAX < ESPNET + 85.0f) espIAX = ESPNET + 85.0f;
    if (espIAX > ESPR - 85.0f) espIAX = ESPR - 85.0f;

    float grav = (espModo == ESP_VOLEI) ? 880.0f : 760.0f;
    espVY += grav * dt;
    espBX += espVX * dt;
    espBY += espVY * dt;

    if (espBX - ESPBR < ESPL) { espBX = ESPL + ESPBR; espVX = fabsf(espVX); }
    if (espBX + ESPBR > ESPR) { espBX = ESPR - ESPBR; espVX = -fabsf(espVX); }
    if (espBY - ESPBR < ESPT) { espBY = ESPT + ESPBR; espVY = fabsf(espVY); }

    float netTop = (espModo == ESP_VOLEI) ? ESPT + 250.0f : ESPT + 330.0f;
    if (espBX + ESPBR > ESPNET - 5.0f && espBX - ESPBR < ESPNET + 5.0f && espBY > netTop) {
        if (espVX > 0.0f) { espBX = ESPNET - 5.0f - ESPBR; espVX = -fabsf(espVX); }
        else { espBX = ESPNET + 5.0f + ESPBR; espVX = fabsf(espVX); }
    }

    if (espBX > espJX - 80.0f && espBX < espJX + 80.0f &&
        espBY + ESPBR > espJY - 12.0f && espBY - ESPBR < espJY + 12.0f && espVY > 0.0f) {
        espBY = espJY - 12.0f - ESPBR;
        espVY = -fabsf(espVY) * 1.02f - 250.0f;
        espVX = (espBX - espJX) * 3.0f;
        if (espVX > 700.0f) espVX = 700.0f;
        if (espVX < -700.0f) espVX = -700.0f;
    }
    if (espBX > espIAX - 80.0f && espBX < espIAX + 80.0f &&
        espBY + ESPBR > espIAY - 12.0f && espBY - ESPBR < espIAY + 12.0f && espVY > 0.0f) {
        espBY = espIAY - 12.0f - ESPBR;
        espVY = -fabsf(espVY) * 1.02f - 250.0f;
        espVX = (espBX - espIAX) * 2.6f;
        if (espVX > 700.0f) espVX = 700.0f;
        if (espVX < -700.0f) espVX = -700.0f;
    }

    if (espBY + ESPBR > ESPB) {
        if (espBX < ESPNET) espPonto(0); else espPonto(1);
    }
}
static void espDesenhar(uint32_t* tela) {
    preencherRet(tela, 0, 0, TELA_L, TELA_A, 0xFF0B1A0E);
    const char* titulo = espModo == ESP_VOLEI ? "VOLEI" : (espModo == ESP_TENIS ? "TENIS" : "FUTEBOL");
    jogoCentro(tela, titulo, 44, 960, 24, 0xFFFFFFFF);
    uint32_t branco = 0xFFFFFFFF;
    float meioY = (ESPT + ESPB) * 0.5f;

    if (espModo == ESP_FUTEBOL) {
        preencherRet(tela, (int)ESPL - 26, (int)ESPT - 26, (int)(ESPR - ESPL) + 52, (int)(ESPB - ESPT) + 52, 0xFF5A3E2B);
        preencherRet(tela, (int)ESPL, (int)ESPT, (int)(ESPR - ESPL), (int)(ESPB - ESPT), 0xFF2F8E4A);
        jogoBorda(tela, (int)ESPL, (int)ESPT, (int)(ESPR - ESPL), (int)(ESPB - ESPT), branco, 4);
        preencherRet(tela, (int)((ESPL + ESPR) * 0.5f) - 2, (int)ESPT, 4, (int)(ESPB - ESPT), branco);
        fisCirculo(tela, (ESPL + ESPR) * 0.5f, meioY, 7.0f, branco);
        preencherRet(tela, (int)ESPL - 22, (int)(meioY - 105), 22, 210, branco);
        preencherRet(tela, (int)ESPR, (int)(meioY - 105), 22, 210, branco);
        fisCirculo(tela, espJX, espJY, 26.0f, 0xFF2E6DB4);
        fisCirculo(tela, espJX, espJY, 10.0f, branco);
        fisCirculo(tela, espIAX, espIAY, 26.0f, 0xFFE23B2E);
        fisCirculo(tela, espIAX, espIAY, 10.0f, branco);
        fisCirculo(tela, espBX, espBY, ESPBR, branco);
    } else {
        uint32_t fundo = espModo == ESP_VOLEI ? 0xFFE8D6A8 : 0xFF2E7D5B;
        preencherRet(tela, (int)ESPL - 26, (int)ESPT - 26, (int)(ESPR - ESPL) + 52, (int)(ESPB - ESPT) + 52, 0xFF5A3E2B);
        preencherRet(tela, (int)ESPL, (int)ESPT, (int)(ESPR - ESPL), (int)(ESPB - ESPT), fundo);
        jogoBorda(tela, (int)ESPL, (int)ESPT, (int)(ESPR - ESPL), (int)(ESPB - ESPT), branco, 4);
        preencherRet(tela, (int)ESPL + 40, (int)ESPT + 40, (int)(ESPR - ESPL) - 80, 4, branco);
        preencherRet(tela, (int)ESPL + 40, (int)ESPB - 44, (int)(ESPR - ESPL) - 80, 4, branco);
        preencherRet(tela, (int)ESPL + 40, (int)ESPT + 40, 4, (int)(ESPB - ESPT) - 80, branco);
        preencherRet(tela, (int)ESPR - 44, (int)ESPT + 40, 4, (int)(ESPB - ESPT) - 80, branco);
        float netTop = (espModo == ESP_VOLEI) ? ESPT + 250.0f : ESPT + 330.0f;
        preencherRet(tela, (int)ESPNET - 5, (int)netTop, 10, (int)(ESPB - netTop), branco);
        preencherRet(tela, (int)ESPNET - 12, (int)netTop - 12, 24, 12, 0xFFBFC4CC);
        preencherRet(tela, (int)(espJX - 80), (int)(espJY - 12), 160, 24, 0xFF2E6DB4);
        preencherRet(tela, (int)(espIAX - 80), (int)(espIAY - 12), 160, 24, 0xFFE23B2E);
        fisCirculo(tela, espBX, espBY, ESPBR, espModo == ESP_VOLEI ? branco : 0xFFE6F04A);
    }

    jogoCentro(tela, espMsg, 24, 960, 900, 0xFFDCE6F3);
    char placar[64];
    snprintf(placar, sizeof(placar), "VOCE %d   x   %d IA", espPtsJ, espPtsIA);
    jogoCentro(tela, placar, 30, 960, 938, 0xFFFFFFFF);
    if (espFim) jogoCentro(tela, "X NOVO JOGO   BOLINHA SAIR", 22, 960, 984, 0xFFA9BCD6);
    else {
        const char* ajuda = espModo == ESP_FUTEBOL ? "DIRECIONAL MOVER   X CHUTAR   BOLINHA SAIR"
                                                  : "ESQ/DIR MOVER RACLETA   BOLINHA SAIR";
        jogoBarra(tela, ajuda);
    }
}

// ---------------------------------------------------------------------------
// Entrada/saida da mesa de jogos
// ---------------------------------------------------------------------------
static bool jogosIniciarJogo(int id) {
    jogoRng = (unsigned int)sceKernelGetProcessTime();
    if (id == JOGO_DAMA) { damaIniciar(); return true; }
    if (id == JOGO_XADREZ) { xadIniciar(); return true; }
    if (id == JOGO_TRUCO) { trucoIniciar(); return true; }
    if (id == JOGO_POKER) { pokerIniciar(); return true; }
    if (id == JOGO_DOMINO) { domIniciar(); return true; }
    if (id == JOGO_21) { v21Iniciar(); return true; }
    if (id == JOGO_GUERRA) { guerraIniciar(); return true; }
    if (id == JOGO_PESCARIA) { pescIniciar(); return true; }
    if (id == JOGO_MICO) { micoIniciar(); return true; }
    if (id == JOGO_BISCA) { tr2Iniciar(0); return true; }
    if (id == JOGO_SUECA) { tr2Iniciar(1); return true; }
    if (id == JOGO_ESCOPA) { escIniciar(); return true; }
    if (id == JOGO_MAUMAU) { mmIniciar(); return true; }
    if (id == JOGO_FODINHA) { fodIniciar(); return true; }
    if (id == JOGO_BURACO) { rumIniciar(0); return true; }
    if (id == JOGO_CACHETA) { rumIniciar(1); return true; }
    if (id == JOGO_BIRIBA) { rumIniciar(2); return true; }
    if (id == JOGO_PIFPAF) { rumIniciar(3); return true; }
    if (id == JOGO_CORACOES) { cqIniciar(0); return true; }
    if (id == JOGO_ESPADAS) { cqIniciar(1); return true; }
    if (id == JOGO_PACIENCIA) { pacIniciar(); return true; }
    if (id == JOGO_FREECELL) { fcIniciar(); return true; }
    return false;
}

static void jogosMenuEntrada(uint32_t novos) {
    if (novos & ORBIS_PAD_BUTTON_UP) jogoMenuSel = (jogoMenuSel + 3) % 4;
    if (novos & ORBIS_PAD_BUTTON_DOWN) jogoMenuSel = (jogoMenuSel + 1) % 4;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) return;
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        int id = JOGO_MENU_IDS[jogoMenuSel];
        if (id == JOGO_CARTAS_MENU) {
            cartasMenuSel = 0;
            cartasMenuTop = 0;
            cartasAviso[0] = 0;
            jogoAtual = JOGO_CARTAS_MENU;
        } else if (jogosIniciarJogo(id)) {
            jogoAtual = id;
        }
    }
}

static void cartasMenuEntrada(uint32_t novos) {
    if (novos & ORBIS_PAD_BUTTON_UP) { cartasMenuSel = (cartasMenuSel + CARTAS_TOTAL - 1) % CARTAS_TOTAL; cartasAviso[0] = 0; }
    if (novos & ORBIS_PAD_BUTTON_DOWN) { cartasMenuSel = (cartasMenuSel + 1) % CARTAS_TOTAL; cartasAviso[0] = 0; }
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) { jogoAtual = JOGO_MENU; return; }
    if (novos & ORBIS_PAD_BUTTON_CROSS) {
        int id = CARTAS_IDS[cartasMenuSel];
        if (jogosIniciarJogo(id)) jogoAtual = id;
        else snprintf(cartasAviso, sizeof(cartasAviso), "%s: EM BREVE", CARTAS_NOMES[cartasMenuSel]);
    }
}

static void desenharUIJogos(uint32_t* tela) {
    if (jogoAtual == JOGO_MENU) desenharUIMenuJogos(tela);
    else if (jogoAtual == JOGO_CARTAS_MENU) desenharUICartasMenu(tela);
    else if (jogoAtual == JOGO_DAMA) damaDesenhar(tela);
    else if (jogoAtual == JOGO_XADREZ) xadDesenhar(tela);
    else if (jogoAtual == JOGO_TRUCO) trucoDesenhar(tela);
    else if (jogoAtual == JOGO_POKER) pokerDesenhar(tela);
    else if (jogoAtual == JOGO_DOMINO) dominoDesenhar(tela);
    else if (jogoAtual == JOGO_21) v21Desenhar(tela);
    else if (jogoAtual == JOGO_GUERRA) guerraDesenhar(tela);
    else if (jogoAtual == JOGO_PESCARIA) pescDesenhar(tela);
    else if (jogoAtual == JOGO_MICO) micoDesenhar(tela);
    else if (jogoAtual == JOGO_BISCA) tr2Desenhar(tela);
    else if (jogoAtual == JOGO_SUECA) tr2Desenhar(tela);
    else if (jogoAtual == JOGO_ESCOPA) escDesenhar(tela);
    else if (jogoAtual == JOGO_MAUMAU) mmDesenhar(tela);
    else if (jogoAtual == JOGO_FODINHA) fodDesenhar(tela);
    else if (jogoAtual == JOGO_BURACO || jogoAtual == JOGO_CACHETA ||
             jogoAtual == JOGO_BIRIBA || jogoAtual == JOGO_PIFPAF) rumDesenhar(tela);
    else if (jogoAtual == JOGO_CORACOES || jogoAtual == JOGO_ESPADAS) cqDesenhar(tela);
    else if (jogoAtual == JOGO_PACIENCIA) pacDesenhar(tela);
    else if (jogoAtual == JOGO_FREECELL) fcDesenhar(tela);
    else if (jogoAtual == JOGO_PINGPONG) ppDesenhar(tela);
    else if (jogoAtual == JOGO_AIRHOCKEY) ahDesenhar(tela);
    else if (jogoAtual == JOGO_PEBOLIM) pbDesenhar(tela);
    else if (jogoAtual == JOGO_SINUCA) snDesenhar(tela);
    else if (jogoAtual == JOGO_VOLEI || jogoAtual == JOGO_TENIS ||
             jogoAtual == JOGO_FUTEBOL) espDesenhar(tela);
    else desenharUIMenuJogos(tela);
}

static void jogosEntrada(uint32_t novos, uint32_t segurados, float dt) {
    if (jogoFisico && (novos & ORBIS_PAD_BUTTON_CIRCLE)) { jogoSairCaminhar = 1; return; }
    if (jogoAtual == JOGO_PINGPONG) ppEntrada(novos, segurados, dt);
    else if (jogoAtual == JOGO_AIRHOCKEY) ahEntrada(novos, segurados, dt);
    else if (jogoAtual == JOGO_PEBOLIM) pbEntrada(novos, segurados, dt);
    else if (jogoAtual == JOGO_SINUCA) snEntrada(novos, segurados, dt);
    else if (jogoAtual == JOGO_VOLEI || jogoAtual == JOGO_TENIS ||
             jogoAtual == JOGO_FUTEBOL) espEntrada(novos, segurados, dt);
    else if (jogoAtual == JOGO_DAMA) damaEntrada(novos, dt);
    else if (jogoAtual == JOGO_XADREZ) xadEntrada(novos, dt);
    else if (jogoAtual == JOGO_TRUCO) trucoEntrada(novos, dt);
    else if (jogoAtual == JOGO_POKER) pokerEntrada(novos, dt);
    else if (jogoAtual == JOGO_DOMINO) dominoEntrada(novos, dt);
    else if (jogoAtual == JOGO_21) v21Entrada(novos, dt);
    else if (jogoAtual == JOGO_GUERRA) guerraEntrada(novos, dt);
    else if (jogoAtual == JOGO_PESCARIA) pescEntrada(novos, dt);
    else if (jogoAtual == JOGO_MICO) micoEntrada(novos, dt);
    else if (jogoAtual == JOGO_BISCA) tr2Entrada(novos, dt);
    else if (jogoAtual == JOGO_SUECA) tr2Entrada(novos, dt);
    else if (jogoAtual == JOGO_ESCOPA) escEntrada(novos, dt);
    else if (jogoAtual == JOGO_MAUMAU) mmEntrada(novos, dt);
    else if (jogoAtual == JOGO_FODINHA) fodEntrada(novos, dt);
    else if (jogoAtual == JOGO_BURACO || jogoAtual == JOGO_CACHETA ||
             jogoAtual == JOGO_BIRIBA || jogoAtual == JOGO_PIFPAF) rumEntrada(novos, dt);
    else if (jogoAtual == JOGO_CORACOES || jogoAtual == JOGO_ESPADAS) cqEntrada(novos, dt);
    else if (jogoAtual == JOGO_PACIENCIA) pacEntrada(novos, dt);
    else if (jogoAtual == JOGO_FREECELL) fcEntrada(novos, dt);
    else jogoAtual = JOGO_MENU;
}

static void executar() {
    float px = 0.0f;
    float pz = 6.8f;
    float yaw = 0.0f;
    float pitch = -0.04f;
    float tempo = 0.0f;
    float dialogoTimer = 0.0f;
    int dialogoAgente = -1;
    uint32_t botoesAntigos = 0;
    float fps = 60.0f;

    Camera cam;
    cam.x = px; cam.y = 1.62f; cam.z = pz; cam.yaw = yaw; cam.pitch = pitch;
    atualizarCamera(cam);

    bool threadsIniciadas = false;
    bool primeiroQuadro = true;

    uint64_t ultimoQuadro = sceKernelGetProcessTime();
    uint64_t proximoQuadro = ultimoQuadro + 16667;

    uint32_t* bufferQuadro = NULL;
    float msSuave = 0.0f;
    int quadrosJanela = 0;
    uint64_t janelaInicio = ultimoQuadro;
    float acumCena = 0.0f, acumRender = 0.0f, acumUpscale = 0.0f, acumHud = 0.0f;

    enum { MODO_JOGO, MODO_ESPELHO, MODO_TV, MODO_PICKER, MODO_RADIO_MENU,
           MODO_RADIO_LISTA, MODO_MUSICA, MODO_IMAGEM, MODO_JOGOS,
           MODO_OPCOES, MODO_DECOR_LISTA, MODO_DECOR_AJUSTE, MODO_DECOR_PLACE,
           MODO_EDIT_LISTA, MODO_EDIT_PINTAR, MODO_EDIT_CONSTRUIR, MODO_EDIT_OBJETOS };
    int modo = MODO_JOGO;
    AvatarCfg avatarCfg;
    carregarAvatar(avatarCfg);
    carregarDecoracoes();
    int avatarCat = 0;
    int tvSel = 0;
    int opcoesSel = 0;
    int decorSel = 0;
    int decorAjusteSel = 0;
    int editSel = 0;
    int editPintarSel = 0;
    int editConstruirSel = 0;
    int editObjSel = 0;
    int decorRetorno = MODO_DECOR_LISTA;
    int ajusteVolta = MODO_DECOR_LISTA;
    int construirVolta = MODO_EDIT_LISTA;
    float tempoEspelho = 0.0f;
    bool videoApresentando = false;
    float alturaSalto = 0.0f;
    float velocidadeSalto = 0.0f;
    unsigned char* imagemPixels = NULL;
    int imagemW = 0, imagemH = 0;
    bool imagemDesenhada = false;
    MenuLevel menuSalvo = menuAtual;

    for (;;) {
        OrbisPadData pad;
        if (scePadReadState(globalPadHandle, &pad) != 0) {
            sceKernelUsleep(16000);
            continue;
        }

        uint64_t agora = sceKernelGetProcessTime();
        float dtReal = (float)(agora - ultimoQuadro) / 1000000.0f;
        if (dtReal < 0.001f) dtReal = 0.001f;
        float dt = dtReal > 0.050f ? 0.050f : dtReal;
        ultimoQuadro = agora;
        fps = fps * 0.90f + (1.0f / dtReal) * 0.10f;

        if (tvMidia == TVMIDIA_AUDIO) {
            uint32_t assinatura = tvHashAudio(musicaTitulo, musicaEhRadio, (bool)comandoPausar, volumeGeral);
            if (!tvTexturaAtiva || assinatura != tvAssinaturaAudio) {
                tvAssinaturaAudio = assinatura;
                tvPainelAudio(musicaTitulo, musicaEhRadio, (bool)comandoPausar, volumeGeral);
            }
        }

        uint32_t botoes = pad.buttons;
        uint32_t novos = botoes & ~botoesAntigos;
        botoesAntigos = botoes;
        if (msgTimer > 0) msgTimer--;
        int modoAnterior = modo;

        bool videoBloqueiaJogo = false;
        if (videoRodando) {
            if (!videoApresentando) { definirApresentacaoVideo(true); videoApresentando = true; }

            if ((botoes & ORBIS_PAD_BUTTON_L2) && (novos & ORBIS_PAD_BUTTON_SQUARE)) {
                video_minimizado = !video_minimizado;
                if (!video_minimizado) pip_selecionado = false;
                novos &= ~ORBIS_PAD_BUTTON_SQUARE;
                botoes &= ~ORBIS_PAD_BUTTON_SQUARE;
            }
            if (video_minimizado && !menu_video_aberto && (novos & ORBIS_PAD_BUTTON_TRIANGLE)) pip_selecionado = true;

            atenderFimDeVideo();
        }
        if (videoRodando) {
            bool crossConsumido = false, circleConsumido = false;
            processarControlesMenuVideo(novos, &pad, crossConsumido, circleConsumido);
            if (crossConsumido) { novos &= ~ORBIS_PAD_BUTTON_CROSS; botoes &= ~ORBIS_PAD_BUTTON_CROSS; }
            if (circleConsumido) { novos &= ~ORBIS_PAD_BUTTON_CIRCLE; botoes &= ~ORBIS_PAD_BUTTON_CIRCLE; }

            if ((novos & ORBIS_PAD_BUTTON_CIRCLE) && !menu_video_aberto) {
                if (!video_minimizado) {
                    if (!fecharControlesPlayer()) { bloqueio_fechar_video = false; pararVideo(); }
                } else if (pip_selecionado) {
                    pip_selecionado = false;
                } else {
                    bloqueio_fechar_video = false;
                    pararVideo();
                }
                novos &= ~ORBIS_PAD_BUTTON_CIRCLE;
                botoes &= ~ORBIS_PAD_BUTTON_CIRCLE;
            }
            if (videoRodando) videoBloqueiaJogo = (!video_minimizado || menu_video_aberto);
        }
        if (videoApresentando && !videoRodando) {
            videoApresentando = false;
            modo = MODO_JOGO;
        }

        if (modo == MODO_JOGO && modoAnterior == MODO_JOGO && !videoBloqueiaJogo &&
            (novos & ORBIS_PAD_BUTTON_OPTIONS)) {
            modo = MODO_OPCOES;
            opcoesSel = 0;
        }

        if (modo == MODO_OPCOES && modoAnterior == MODO_OPCOES && !videoBloqueiaJogo) {
            if ((novos & ORBIS_PAD_BUTTON_CIRCLE) || (novos & ORBIS_PAD_BUTTON_OPTIONS)) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_UP) opcoesSel = (opcoesSel + 2) % 3;
            if (novos & ORBIS_PAD_BUTTON_DOWN) opcoesSel = (opcoesSel + 1) % 3;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (opcoesSel == 0) { decorSel = 0; modo = MODO_DECOR_LISTA; }
                else if (opcoesSel == 1) { editSel = 0; modo = MODO_EDIT_LISTA; }
                else modo = MODO_JOGO;
            }
        }

        if (modo == MODO_DECOR_LISTA && modoAnterior == MODO_DECOR_LISTA) {
            const int totalOp = DECOR_TIPOS + 3;
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_OPCOES;
            if (novos & ORBIS_PAD_BUTTON_OPTIONS) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_UP) decorSel = (decorSel + totalOp - 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_DOWN) decorSel = (decorSel + 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (decorSel < DECOR_TIPOS) {
                    if (decorTotal >= DECOR_MAX) {
                        snprintf(msgStatus, sizeof(msgStatus), "LIMITE DE %d ITENS DE DECORACAO", DECOR_MAX);
                        msgTimer = 150;
                    } else {
                        DecorItem& it = decorItens[decorTotal];
                        it.tipo = decorSel;
                        it.x = px + sinf(yaw) * 2.2f;
                        it.z = pz - cosf(yaw) * 2.2f;
                        if (it.x < -29.0f) it.x = -29.0f;
                        if (it.x > 29.0f) it.x = 29.0f;
                        if (it.z < -19.0f) it.z = -19.0f;
                        if (it.z > 59.0f) it.z = 59.0f;
                        it.rot = yaw;
                        it.cor = decorCorPadrao(decorSel);
                        decorEditando = decorTotal;
                        decorEditNovo = true;
                        decorTotal++;
                        decorRetorno = MODO_DECOR_LISTA;
                        modo = MODO_DECOR_PLACE;
                    }
                } else if (decorSel == DECOR_TIPOS) {
                    decorAjusteSel = 0;
                    ajusteVolta = MODO_DECOR_LISTA;
                    modo = MODO_DECOR_AJUSTE;
                } else if (decorSel == DECOR_TIPOS + 1) {
                    editConstruirSel = 0;
                    construirVolta = MODO_DECOR_LISTA;
                    modo = MODO_EDIT_CONSTRUIR;
                } else {
                    modo = MODO_OPCOES;
                }
            }
        }

        if (modo == MODO_DECOR_AJUSTE && modoAnterior == MODO_DECOR_AJUSTE) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = ajusteVolta;
            if (decorTotal > 0) {
                if (decorAjusteSel >= decorTotal) decorAjusteSel = decorTotal - 1;
                if (novos & ORBIS_PAD_BUTTON_UP) decorAjusteSel = (decorAjusteSel + decorTotal - 1) % decorTotal;
                if (novos & ORBIS_PAD_BUTTON_DOWN) decorAjusteSel = (decorAjusteSel + 1) % decorTotal;
                if (novos & ORBIS_PAD_BUTTON_CROSS) {
                    decorEditando = decorAjusteSel;
                    decorEditNovo = false;
                    decorAntes = decorItens[decorAjusteSel];
                    decorRetorno = MODO_DECOR_AJUSTE;
                    modo = MODO_DECOR_PLACE;
                }
                if (novos & ORBIS_PAD_BUTTON_SQUARE) {
                    for (int i = decorAjusteSel; i < decorTotal - 1; i++) decorItens[i] = decorItens[i + 1];
                    decorTotal--;
                    if (decorAjusteSel >= decorTotal && decorAjusteSel > 0) decorAjusteSel--;
                    salvarDecoracoes();
                }
            } else {
                decorAjusteSel = 0;
            }
        }

        if (modo == MODO_DECOR_PLACE && modoAnterior == MODO_DECOR_PLACE &&
            decorEditando >= 0 && decorEditando < decorTotal) {
            DecorItem& it = decorItens[decorEditando];
            float msx = (pad.leftStick.x - 128.0f) / 128.0f;
            float msy = (pad.leftStick.y - 128.0f) / 128.0f;
            if (fabsf(msx) < 0.18f) msx = 0.0f;
            if (fabsf(msy) < 0.18f) msy = 0.0f;
            float fx = sinf(yaw), fz = -cosf(yaw);
            float rx = cosf(yaw), rz = sinf(yaw);
            float passo = 3.0f * dt;
            it.x += (fx * (-msy) + rx * msx) * passo;
            it.z += (fz * (-msy) + rz * msx) * passo;
            if (botoes & ORBIS_PAD_BUTTON_UP) { it.x += fx * passo; it.z += fz * passo; }
            if (botoes & ORBIS_PAD_BUTTON_DOWN) { it.x -= fx * passo; it.z -= fz * passo; }
            if (botoes & ORBIS_PAD_BUTTON_LEFT) { it.x -= rx * passo; it.z -= rz * passo; }
            if (botoes & ORBIS_PAD_BUTTON_RIGHT) { it.x += rx * passo; it.z += rz * passo; }
            if (botoes & ORBIS_PAD_BUTTON_L1) it.rot -= 2.2f * dt;
            if (botoes & ORBIS_PAD_BUTTON_R1) it.rot += 2.2f * dt;
            if (it.x < -29.0f) it.x = -29.0f;
            if (it.x > 29.0f) it.x = 29.0f;
            if (it.z < -19.0f) it.z = -19.0f;
            if (it.z > 59.0f) it.z = 59.0f;
            if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
                if (it.tipo == EDIT_PAREDE || it.tipo == EDIT_JANELA)
                    it.cor = paletaCiclarCor(CORES_PAREDE, 8, 1, it.cor, 1);
                else if (it.tipo == EDIT_CHAO)
                    it.cor = paletaCiclarCor(CORES_CHAO, 8, 1, it.cor, 1);
                else if (it.tipo == EDIT_TETO)
                    it.cor = paletaCiclarCor(CORES_TETO, 6, 1, it.cor, 1);
                else
                    it.cor = paletaCiclarCor(DECOR_PALETA, 8, 0, it.cor, 1);
            }
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                const char* nomeItem = (it.tipo >= 0 && it.tipo < OBJ_TIPOS) ? OBJ_NOMES[it.tipo] : "ITEM";
                snprintf(msgStatus, sizeof(msgStatus), "%s COLOCADO", nomeItem);
                msgTimer = 90;
                decorEditando = -1;
                salvarDecoracoes();
                modo = decorRetorno;
            } else if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
                if (decorEditNovo) {
                    decorTotal--;
                } else {
                    decorItens[decorEditando] = decorAntes;
                }
                decorEditando = -1;
                modo = decorRetorno;
            }
        }

        if (modo == MODO_EDIT_LISTA && modoAnterior == MODO_EDIT_LISTA) {
            const int totalOp = 5;
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_OPCOES;
            if (novos & ORBIS_PAD_BUTTON_OPTIONS) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_UP) editSel = (editSel + totalOp - 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_DOWN) editSel = (editSel + 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (editSel == 0) { editPintarSel = 0; modo = MODO_EDIT_PINTAR; }
                else if (editSel == 1) { editConstruirSel = 0; construirVolta = MODO_EDIT_LISTA; modo = MODO_EDIT_CONSTRUIR; }
                else if (editSel == 2) { editObjSel = 0; modo = MODO_EDIT_OBJETOS; }
                else if (editSel == 3) {
                    decorAjusteSel = 0;
                    ajusteVolta = MODO_EDIT_LISTA;
                    modo = MODO_DECOR_AJUSTE;
                } else modo = MODO_OPCOES;
            }
        }

        if (modo == MODO_EDIT_PINTAR && modoAnterior == MODO_EDIT_PINTAR) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) { salvarDecoracoes(); modo = MODO_EDIT_LISTA; }
            if (novos & ORBIS_PAD_BUTTON_UP) editPintarSel = (editPintarSel + 3) % 4;
            if (novos & ORBIS_PAD_BUTTON_DOWN) editPintarSel = (editPintarSel + 1) % 4;
            if (editPintarSel == 3) {
                if (novos & ORBIS_PAD_BUTTON_CROSS) { salvarDecoracoes(); modo = MODO_EDIT_LISTA; }
            } else {
                const uint32_t* pal = (editPintarSel == 0) ? CORES_PAREDE : (editPintarSel == 1) ? CORES_CHAO : CORES_TETO;
                int n = (editPintarSel == 2) ? 6 : 8;
                uint32_t* alvo = (editPintarSel == 0) ? &corParedeEdit : (editPintarSel == 1) ? &corChaoEdit : &corTetoEdit;
                if (novos & ORBIS_PAD_BUTTON_LEFT) *alvo = paletaCiclar(pal, n, *alvo, -1);
                if (novos & ORBIS_PAD_BUTTON_RIGHT) *alvo = paletaCiclar(pal, n, *alvo, 1);
                if (novos & ORBIS_PAD_BUTTON_TRIANGLE) *alvo = 0;
                if (novos & ORBIS_PAD_BUTTON_CROSS) {
                    salvarDecoracoes();
                    snprintf(msgStatus, sizeof(msgStatus), "PINTURA SALVA");
                    msgTimer = 120;
                }
            }
        }

        if (modo == MODO_EDIT_CONSTRUIR && modoAnterior == MODO_EDIT_CONSTRUIR) {
            const int totalOp = 5;
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = construirVolta;
            if (novos & ORBIS_PAD_BUTTON_OPTIONS) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_UP) editConstruirSel = (editConstruirSel + totalOp - 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_DOWN) editConstruirSel = (editConstruirSel + 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (editConstruirSel == 4) {
                    modo = construirVolta;
                } else if (decorTotal >= DECOR_MAX) {
                    snprintf(msgStatus, sizeof(msgStatus), "LIMITE DE %d ITENS", DECOR_MAX);
                    msgTimer = 150;
                } else {
                    static const int mapa[4] = { EDIT_PAREDE, EDIT_CHAO, EDIT_TETO, EDIT_JANELA };
                    int tipo = mapa[editConstruirSel];
                    DecorItem& it = decorItens[decorTotal];
                    it.tipo = tipo;
                    it.x = px + sinf(yaw) * 2.6f;
                    it.z = pz - cosf(yaw) * 2.6f;
                    if (tipo == EDIT_CHAO || tipo == EDIT_TETO) {
                        it.x = floorf(it.x / 2.5f + 0.5f) * 2.5f;
                        it.z = floorf(it.z / 2.5f + 0.5f) * 2.5f;
                    }
                    if (it.x < -29.0f) it.x = -29.0f;
                    if (it.x > 29.0f) it.x = 29.0f;
                    if (it.z < -19.0f) it.z = -19.0f;
                    if (it.z > 59.0f) it.z = 59.0f;
                    it.rot = yaw;
                    it.cor = decorCorPadrao(tipo);
                    decorEditando = decorTotal;
                    decorEditNovo = true;
                    decorTotal++;
                    decorRetorno = MODO_EDIT_CONSTRUIR;
                    modo = MODO_DECOR_PLACE;
                }
            }
        }

        if (modo == MODO_EDIT_OBJETOS && modoAnterior == MODO_EDIT_OBJETOS) {
            const int totalOp = 9;
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_EDIT_LISTA;
            if (novos & ORBIS_PAD_BUTTON_OPTIONS) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_UP) editObjSel = (editObjSel + totalOp - 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_DOWN) editObjSel = (editObjSel + 1) % totalOp;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (editObjSel == 8) {
                    modo = MODO_EDIT_LISTA;
                } else if (decorTotal >= DECOR_MAX) {
                    snprintf(msgStatus, sizeof(msgStatus), "LIMITE DE %d ITENS", DECOR_MAX);
                    msgTimer = 150;
                } else {
                    int tipo = EDIT_CADEIRA + editObjSel;
                    DecorItem& it = decorItens[decorTotal];
                    it.tipo = tipo;
                    it.x = px + sinf(yaw) * 2.2f;
                    it.z = pz - cosf(yaw) * 2.2f;
                    if (it.x < -29.0f) it.x = -29.0f;
                    if (it.x > 29.0f) it.x = 29.0f;
                    if (it.z < -19.0f) it.z = -19.0f;
                    if (it.z > 59.0f) it.z = 59.0f;
                    it.rot = yaw;
                    it.cor = decorCorPadrao(tipo);
                    decorEditando = decorTotal;
                    decorEditNovo = true;
                    decorTotal++;
                    decorRetorno = MODO_EDIT_OBJETOS;
                    modo = MODO_DECOR_PLACE;
                }
            }
        }

        if (modo == MODO_ESPELHO && modoAnterior == MODO_ESPELHO) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
                salvarAvatar(avatarCfg);
                modo = MODO_JOGO;
            }
            if (novos & ORBIS_PAD_BUTTON_UP) avatarCat = (avatarCat + AV_TOTAL - 1) % AV_TOTAL;
            if (novos & ORBIS_PAD_BUTTON_DOWN) avatarCat = (avatarCat + 1) % AV_TOTAL;
            if (novos & ORBIS_PAD_BUTTON_LEFT) avatarCfg.v[avatarCat] = (avatarCfg.v[avatarCat] + PALETAS_N[avatarCat] - 1) % PALETAS_N[avatarCat];
            if (novos & ORBIS_PAD_BUTTON_RIGHT) avatarCfg.v[avatarCat] = (avatarCfg.v[avatarCat] + 1) % PALETAS_N[avatarCat];
            if (novos & ORBIS_PAD_BUTTON_CROSS) avatarCat = (avatarCat + 1) % AV_TOTAL;
        }
        tempoEspelho += dt;

        if (modo == MODO_TV && modoAnterior == MODO_TV && !videoBloqueiaJogo) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_SQUARE) {
                if (tvMidia == TVMIDIA_AUDIO) tocarMusicaNova("PARADO");
                if (imagemPixels) { stbi_image_free(imagemPixels); imagemPixels = NULL; }
                imagemW = imagemH = 0;
                imagemDesenhada = false;
                tvPararMidia();
            }
            if (novos & ORBIS_PAD_BUTTON_UP) tvSel = (tvSel + 2) % 3;
            if (novos & ORBIS_PAD_BUTTON_DOWN) tvSel = (tvSel + 1) % 3;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); videoApresentando = false; }
                if (tvSel == 0) { pickerOrigem = 0; listarPicker(PASTA_MIDIA); modo = MODO_PICKER; }
                else if (tvSel == 1) { pickerOrigem = 1; mostrarRaizesPicker(); modo = MODO_PICKER; }
                else { radioSel = 0; modo = MODO_RADIO_MENU; }
            }
        }

        if (modo == MODO_PICKER && modoAnterior == MODO_PICKER && !videoBloqueiaJogo) {
            bool subir = false;
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) subir = true;
            else if (totalItensPicker > 0) {
                if (novos & ORBIS_PAD_BUTTON_UP) selPicker = (selPicker + totalItensPicker - 1) % totalItensPicker;
                if (novos & ORBIS_PAD_BUTTON_DOWN) selPicker = (selPicker + 1) % totalItensPicker;
                if (novos & ORBIS_PAD_BUTTON_CROSS) {
                    ItemPicker it = itensPicker[selPicker];
                    if (it.tipo == ITEM_VOLTAR) subir = true;
                    else if (it.tipo == ITEM_PASTA) {
                        pickerMostrandoRaizes = 0;
                        listarPicker(it.caminho);
                    } else if (it.tipo == ITEM_RADIO) {
                        radioSel = 0;
                        modo = MODO_RADIO_MENU;
                    } else if (it.tipo == ITEM_VIDEO) {
                        if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); videoApresentando = false; }
                        if (tvMidia == TVMIDIA_AUDIO) tocarMusicaNova("PARADO");
                        tvPararMidia();
                        snprintf(tvUltimo, sizeof(tvUltimo), "%s", it.caminho);
                        iniciarVideoMP4(it.caminho);
                        if (videoRodando) {
                            video_minimizado = true;
                            pip_selecionado = false;
                            definirApresentacaoVideo(true);
                            videoApresentando = true;
                            modo = MODO_JOGO;
                        } else {
                            snprintf(msgStatus, sizeof(msgStatus), "NAO CONSEGUI ABRIR O VIDEO");
                            msgTimer = 180;
                        }
                    } else if (it.tipo == ITEM_IMAGEM) {
                        if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); videoApresentando = false; }
                        if (tvMidia == TVMIDIA_AUDIO) tocarMusicaNova("PARADO");
                        if (imagemPixels) { stbi_image_free(imagemPixels); imagemPixels = NULL; }
                        int w = 0, h = 0, canais = 0;
                        imagemPixels = stbi_load(it.caminho, &w, &h, &canais, 4);
                        if (imagemPixels) {
                            imagemW = w; imagemH = h;
                            imagemDesenhada = false;
                            snprintf(tvUltimo, sizeof(tvUltimo), "%s", it.caminho);
                            tvPainelImagem(imagemPixels, imagemW, imagemH);
                            tvMidia = TVMIDIA_IMAGEM;
                            modo = MODO_JOGO;
                        } else {
                            snprintf(msgStatus, sizeof(msgStatus), "NAO CONSEGUI ABRIR A IMAGEM");
                            msgTimer = 180;
                        }
                    } else if (it.tipo == ITEM_MUSICA) {
                        if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); videoApresentando = false; }
                        tocarMusicaNova(it.caminho);
                        snprintf(musicaTitulo, sizeof(musicaTitulo), "%s", it.nome);
                        musicaEhRadio = false;
                        tvAssinaturaAudio = 0;
                        tvMidia = TVMIDIA_AUDIO;
                        modo = MODO_JOGO;
                    }
                }
            }
            if (subir) {
                if (pickerMostrandoRaizes) modo = MODO_TV;
                else if (pickerOrigem == 0) {
                    if (strcmp(pickerPath, PASTA_MIDIA) == 0) modo = MODO_TV;
                    else { char pai[512]; caminhoPai(pickerPath, pai, sizeof(pai)); listarPicker(pai); }
                } else if (ehRaizPadrao(pickerPath)) {
                    mostrarRaizesPicker();
                } else {
                    char pai[512]; caminhoPai(pickerPath, pai, sizeof(pai)); listarPicker(pai);
                }
            }
        }

        if (modo == MODO_RADIO_MENU && modoAnterior == MODO_RADIO_MENU && !videoBloqueiaJogo) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_TV;
            if (novos & ORBIS_PAD_BUTTON_UP) radioSel = (radioSel + 2) % 3;
            if (novos & ORBIS_PAD_BUTTON_DOWN) radioSel = (radioSel + 1) % 3;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                radioCategoria = radioSel;
                buscarEstacoesRadio(RADIO_QUERIES[radioCategoria], radioCategoria == 2);
                radioSel = 0;
                modo = MODO_RADIO_LISTA;
            }
        }

        if (modo == MODO_RADIO_LISTA && modoAnterior == MODO_RADIO_LISTA && !videoBloqueiaJogo) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_RADIO_MENU;
            if (totalItens > 0 && radioSel >= totalItens) radioSel = totalItens - 1;
            if (totalItens > 0) {
                if (novos & ORBIS_PAD_BUTTON_UP) radioSel = (radioSel + totalItens - 1) % totalItens;
                if (novos & ORBIS_PAD_BUTTON_DOWN) radioSel = (radioSel + 1) % totalItens;
                if ((novos & ORBIS_PAD_BUTTON_CROSS) && radioSel < totalItens && strlen(linksAtuais[radioSel]) > 5) {
                    if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); videoApresentando = false; }
                    if (imagemPixels) { stbi_image_free(imagemPixels); imagemPixels = NULL; }
                    imagemW = imagemH = 0;
                    imagemDesenhada = false;
                    tocarMusicaNova(linksAtuais[radioSel]);
                    snprintf(musicaTitulo, sizeof(musicaTitulo), "%s", nomes[radioSel]);
                    musicaEhRadio = true;
                    tvAssinaturaAudio = 0;
                    tvMidia = TVMIDIA_AUDIO;
                    modo = MODO_JOGO;
                }
            }
        }

        if (modo == MODO_MUSICA && modoAnterior == MODO_MUSICA && !videoBloqueiaJogo) {
            if (novos & ORBIS_PAD_BUTTON_CROSS) comandoPausar = !comandoPausar;
            if (novos & ORBIS_PAD_BUTTON_UP) aumentarVolume();
            if (novos & ORBIS_PAD_BUTTON_DOWN) diminuirVolume();
            if (!musicaEhRadio) {
                if (novos & ORBIS_PAD_BUTTON_RIGHT) adiantarAudio();
                if (novos & ORBIS_PAD_BUTTON_LEFT) retrocederAudio();
            }
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_SQUARE) {
                tocarMusicaNova("PARADO");
                tvPararMidia();
                modo = MODO_JOGO;
            }
        }

        if (modo == MODO_IMAGEM && modoAnterior == MODO_IMAGEM && !videoBloqueiaJogo) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_JOGO;
            if (novos & ORBIS_PAD_BUTTON_SQUARE) {
                if (imagemPixels) { stbi_image_free(imagemPixels); imagemPixels = NULL; }
                imagemW = 0; imagemH = 0;
                imagemDesenhada = false;
                tvPararMidia();
                modo = MODO_JOGO;
            }
        }

        if (modo == MODO_JOGOS && modoAnterior == MODO_JOGOS && !videoBloqueiaJogo) {
            if (jogoAtual == JOGO_MENU) {
                if (novos & ORBIS_PAD_BUTTON_CIRCLE) modo = MODO_JOGO;
                else jogosMenuEntrada(novos);
            } else if (jogoAtual == JOGO_CARTAS_MENU) {
                cartasMenuEntrada(novos);
            } else {
                jogosEntrada(novos, botoes, dt);
                if (jogoSairCaminhar) {
                    jogoSairCaminhar = 0;
                    jogoFisico = 0;
                    modo = MODO_JOGO;
                }
            }
        }

        if (modo == MODO_PICKER || modo == MODO_RADIO_MENU || modo == MODO_RADIO_LISTA ||
            modo == MODO_MUSICA || modo == MODO_IMAGEM || modo == MODO_JOGOS) {
            uint32_t* telaUI = obterBufferVideo();
            if (!telaUI) break;
            if (!bufferQuadro) bufferQuadro = (uint32_t*)malloc((size_t)TELA_L * TELA_A * 4);
            if (!bufferQuadro) break;
            bool desenharUI = !(modo == MODO_IMAGEM && imagemDesenhada);
            if (desenharUI) {
                if (modo == MODO_PICKER) desenharUIPicker(bufferQuadro);
                else if (modo == MODO_RADIO_MENU) desenharUIRadioMenu(bufferQuadro, radioSel);
                else if (modo == MODO_RADIO_LISTA) desenharUIRadioLista(bufferQuadro, radioSel);
                else if (modo == MODO_MUSICA) desenharUIMusica(bufferQuadro, musicaTitulo, musicaEhRadio, (bool)comandoPausar, volumeGeral);
                else if (modo == MODO_JOGOS) desenharUIJogos(bufferQuadro);
                else { desenharUIImagem(bufferQuadro, imagemPixels, imagemW, imagemH, tvUltimo); imagemDesenhada = true; }
                if (msgTimer > 0 && msgStatus[0] != '\0') {
                    preencherRet(bufferQuadro, 0, 0, TELA_L, 90, 0xDD062348);
                    desenharTexto(bufferQuadro, msgStatus, 42, 40, 22, 0xFFFFE08A);
                }
            }
            memcpy(telaUI, bufferQuadro, (size_t)TELA_L * TELA_A * 4);
            if (primeiroQuadro) { submeterTela(); primeiroQuadro = false; }
            else submeterTelaSemPausa();
            tempo += dt;
            proximoQuadro += 16667;
            int64_t esperaUI = (int64_t)(proximoQuadro - sceKernelGetProcessTime());
            if (esperaUI > 0) sceKernelUsleep((uint32_t)esperaUI);
            else if (esperaUI < -50000) proximoQuadro = sceKernelGetProcessTime() + 16667;
            botoesAntigos = botoes;
            continue;
        }

        int agentePerto = -1;
        int interacao = 0;
        if (modo == MODO_JOGO && modoAnterior == MODO_JOGO && !videoBloqueiaJogo) {
            if (novos & ORBIS_PAD_BUTTON_CIRCLE) break;

            if (!videoRodando) {
                if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
                    if (tvMidia == TVMIDIA_IMAGEM && imagemPixels) modo = MODO_IMAGEM;
                    else if (tvMidia == TVMIDIA_AUDIO) modo = MODO_MUSICA;
                }
                if (novos & ORBIS_PAD_BUTTON_SQUARE) {
                    if (tvMidia == TVMIDIA_AUDIO) {
                        tocarMusicaNova("PARADO");
                        tvPararMidia();
                    } else if (tvMidia == TVMIDIA_IMAGEM) {
                        if (imagemPixels) { stbi_image_free(imagemPixels); imagemPixels = NULL; }
                        imagemW = imagemH = 0;
                        imagemDesenhada = false;
                        tvPararMidia();
                    }
                }
            }
            if ((novos & ORBIS_PAD_BUTTON_R1) && alturaSalto <= 0.001f) velocidadeSalto = 4.6f;

            float rx = (pad.rightStick.x - 128.0f) / 128.0f;
            float ry = (pad.rightStick.y - 128.0f) / 128.0f;
            if (fabsf(rx) < 0.18f) rx = 0.0f;
            if (fabsf(ry) < 0.18f) ry = 0.0f;

            yaw += rx * 2.2f * dt;
            pitch -= ry * 1.8f * dt;
            if (botoes & ORBIS_PAD_BUTTON_LEFT) yaw -= 1.8f * dt;
            if (botoes & ORBIS_PAD_BUTTON_RIGHT) yaw += 1.8f * dt;
            if (botoes & ORBIS_PAD_BUTTON_UP) pitch += 1.4f * dt;
            if (botoes & ORBIS_PAD_BUTTON_DOWN) pitch -= 1.4f * dt;
            if (pitch > 1.25f) pitch = 1.25f;
            if (pitch < -1.25f) pitch = -1.25f;

            float mx = (pad.leftStick.x - 128.0f) / 128.0f;
            float my = (pad.leftStick.y - 128.0f) / 128.0f;
            if (fabsf(mx) < 0.18f) mx = 0.0f;
            if (fabsf(my) < 0.18f) my = 0.0f;
            float avancar = -my;
            float lateral = mx;
            float forca = sqrtf(avancar * avancar + lateral * lateral);
            if (forca > 1.0f) { avancar /= forca; lateral /= forca; }
            float velocidade = (botoes & ORBIS_PAD_BUTTON_L2) ? 5.6f : 3.2f;
            float fx = sinf(yaw);
            float fz = -cosf(yaw);
            float rxdir = cosf(yaw);
            float rzdir = sinf(yaw);
            float vx = (fx * avancar + rxdir * lateral) * velocidade * dt;
            float vz = (fz * avancar + rzdir * lateral) * velocidade * dt;

            float nx = px + vx;
            float nz = pz + vz;
            if (posicaoLivre(nx, nz)) { px = nx; pz = nz; }
            else {
                if (posicaoLivre(nx, pz)) px = nx;
                if (posicaoLivre(px, nz)) pz = nz;
            }

            float menorDist = 3.0f;
            for (int i = 0; i < TOTAL_AGENTES; i++) {
                float dx = px - agentes[i].x;
                float dz = pz - agentes[i].z;
                float d = sqrtf(dx * dx + dz * dz);
                if (d < menorDist) { menorDist = d; agentePerto = i; }
            }
            float dxE = px - 7.9f;
            float dzE = pz + 0.1f;
            bool pertoEspelho = (dxE * dxE + dzE * dzE) <= 4.0f;
            float dxT = px - 3.8f;
            float dzT = pz + 18.2f;
            bool pertoTV = (dxT * dxT + dzT * dzT) <= 10.24f;
            float dxJ = px - 4.5f;
            float dzJ = pz - 14.3f;
            bool pertoJogos = (dxJ * dxJ + dzJ * dzJ) <= 7.29f;
            float dxPP = px - 20.0f, dzPP = pz + 10.6f;
            bool pertoPing = (dxPP * dxPP + dzPP * dzPP) <= 4.84f;
            float dxSN = px - 20.0f, dzSN = pz - 13.6f;
            bool pertoSinuca = (dxSN * dxSN + dzSN * dzSN) <= 4.84f;
            float dxAH = px + 20.0f, dzAH = pz + 10.6f;
            bool pertoAir = (dxAH * dxAH + dzAH * dzAH) <= 4.84f;
            float dxPB = px + 20.0f, dzPB = pz - 13.6f;
            bool pertoPebolim = (dxPB * dxPB + dzPB * dzPB) <= 4.84f;
            float dxVO = px + 15.0f, dzVO = pz - 24.5f;
            bool pertoVolei = (dxVO * dxVO + dzVO * dzVO) <= 4.84f;
            float dxTE = px - 4.0f, dzTE = pz - 24.5f;
            bool pertoTenis = (dxTE * dxTE + dzTE * dzTE) <= 4.84f;
            float dxFU = px - 0.0f, dzFU = pz - 42.0f;
            bool pertoFutebol = (dxFU * dxFU + dzFU * dzFU) <= 4.84f;
            if (pertoEspelho) interacao = 1;
            else if (pertoTV) interacao = 2;
            else if (pertoJogos) interacao = 3;
            else if (pertoPing) interacao = 4;
            else if (pertoSinuca) interacao = 5;
            else if (pertoAir) interacao = 6;
            else if (pertoPebolim) interacao = 7;
            else if (pertoVolei) interacao = 8;
            else if (pertoTenis) interacao = 9;
            else if (pertoFutebol) interacao = 10;
            if (novos & ORBIS_PAD_BUTTON_CROSS) {
                if (pertoEspelho || pertoTV || pertoJogos || pertoPing || pertoSinuca || pertoAir || pertoPebolim ||
                    pertoVolei || pertoTenis || pertoFutebol) {
                    if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); videoApresentando = false; }
                }
                if (pertoEspelho) {
                    modo = MODO_ESPELHO;
                    avatarCat = 0;
                    tempoEspelho = 0.0f;
                } else if (pertoTV) {
                    modo = MODO_TV;
                    tvSel = 0;
                } else if (pertoJogos) {
                    modo = MODO_JOGOS;
                    jogoAtual = JOGO_MENU;
                    jogoMenuSel = 0;
                    jogoFisico = 0;
                } else if (pertoPing) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_PINGPONG;
                    ppIniciar();
                } else if (pertoSinuca) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_SINUCA;
                    snIniciar();
                } else if (pertoAir) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_AIRHOCKEY;
                    ahIniciar();
                } else if (pertoPebolim) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_PEBOLIM;
                    pbIniciar();
                } else if (pertoVolei) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_VOLEI;
                    espIniciar(ESP_VOLEI);
                } else if (pertoTenis) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_TENIS;
                    espIniciar(ESP_TENIS);
                } else if (pertoFutebol) {
                    modo = MODO_JOGOS;
                    jogoFisico = 1; jogoSairCaminhar = 0;
                    jogoAtual = JOGO_FUTEBOL;
                    espIniciar(ESP_FUTEBOL);
                } else if (agentePerto >= 0) {
                    dialogoAgente = agentePerto;
                    dialogoTimer = 6.0f;
                }
            }
            if (dialogoTimer > 0.0f) dialogoTimer -= dt;
        }

        if (modo == MODO_JOGO) {
            if (velocidadeSalto != 0.0f || alturaSalto > 0.0f) {
                velocidadeSalto -= 13.5f * dt;
                alturaSalto += velocidadeSalto * dt;
                if (alturaSalto <= 0.0f) { alturaSalto = 0.0f; velocidadeSalto = 0.0f; }
            }
        }

        cam.x = px; cam.y = 1.62f + alturaSalto; cam.z = pz; cam.yaw = yaw; cam.pitch = pitch;
        atualizarCamera(cam);

        totalTriangulos = 0;
        triangulosDescartados = 0;
        if ((novos & ORBIS_PAD_BUTTON_L1) && modo == MODO_JOGO) cullingAtivo = !cullingAtivo;

        Camera camCena = cam;
        corFundoCima = 0xFFB9D9EE;
        corFundoBaixo = 0xFFE8F2F8;
        bool cenaEspelho = (modo == MODO_ESPELHO);
        if (cenaEspelho) {
            camCena.x = 0.0f; camCena.y = 1.05f; camCena.z = 2.9f;
            camCena.yaw = 0.0f; camCena.pitch = 0.0f;
            atualizarCamera(camCena);
            corFundoCima = 0xFF3A4356;
            corFundoBaixo = 0xFF1A2028;
        }
        uint64_t tCena = sceKernelGetProcessTime();
        if (cenaEspelho) construirEspelho(avatarCfg, camCena, tempoEspelho);
        else { construirCena(cam, tempo); desenharDecoracoes(cam, tempo); }
        uint64_t tCena1 = sceKernelGetProcessTime();
        desenharQuadroRender();
        uint64_t tRender = sceKernelGetProcessTime();

        uint32_t* tela = obterBufferVideo();
        if (!tela) break;
        if (!bufferQuadro) bufferQuadro = (uint32_t*)malloc((size_t)TELA_L * TELA_A * 4);
        if (!bufferQuadro) break;
        for (int y = 0; y < ALTURA; y++) {
            const uint32_t* origem = pixels + y * LARGURA;
            uint32_t* destino0 = bufferQuadro + (y * ESCALA_TELA) * TELA_L;
            for (int x = 0; x < LARGURA; x++) {
                uint32_t cor = origem[x];
                uint32_t* d = destino0 + x * ESCALA_TELA;
                d[0] = cor; d[1] = cor; d[2] = cor;
            }
            uint32_t* destino1 = destino0 + TELA_L;
            uint32_t* destino2 = destino1 + TELA_L;
            memcpy(destino1, destino0, TELA_L * sizeof(uint32_t));
            memcpy(destino2, destino0, TELA_L * sizeof(uint32_t));
        }

        uint64_t tUpscale = sceKernelGetProcessTime();
        if (modo == MODO_ESPELHO) {
            desenharUIEspelho(bufferQuadro, avatarCfg, avatarCat);
        } else {
            desenharHUD(bufferQuadro, cam, px, pz, agentePerto, dialogoAgente, dialogoTimer, fps, msSuave, interacao);
            if (modo == MODO_TV) desenharUITV(bufferQuadro, tvSel, tvUltimo);
            else if (modo == MODO_OPCOES) desenharUIOpcoes(bufferQuadro, opcoesSel);
            else if (modo == MODO_DECOR_LISTA) desenharUIDecorLista(bufferQuadro, decorSel, decorTotal);
            else if (modo == MODO_DECOR_AJUSTE) desenharUIDecorAjuste(bufferQuadro, decorAjusteSel, decorTotal);
            else if (modo == MODO_DECOR_PLACE && decorEditando >= 0 && decorEditando < decorTotal)
                desenharUIDecorPlace(bufferQuadro, decorItens[decorEditando]);
            else if (modo == MODO_EDIT_LISTA) desenharUIEditLista(bufferQuadro, editSel, decorTotal);
            else if (modo == MODO_EDIT_PINTAR) desenharUIEditPintar(bufferQuadro, editPintarSel);
            else if (modo == MODO_EDIT_CONSTRUIR) desenharUIEditConstruir(bufferQuadro, editConstruirSel);
            else if (modo == MODO_EDIT_OBJETOS) desenharUIEditObjetos(bufferQuadro, editObjSel);
        }
        if (videoApresentando && videoRodando && video_minimizado && !menu_video_aberto && modo == MODO_JOGO) {
            preencherRet(bufferQuadro, 30, 690, 1080, 44, 0xDD062348);
            desenharTexto(bufferQuadro, "VIDEO NA TV   TRIANGULO: MENU   L2+QUADRADO: TELA CHEIA   BOLINHA: DESLIGAR", 22, 48, 700, 0xFFDCE6F3);
        } else if (modo == MODO_JOGO && tvMidia == TVMIDIA_IMAGEM) {
            preencherRet(bufferQuadro, 30, 690, 1080, 44, 0xDD062348);
            desenharTexto(bufferQuadro, "IMAGEM NA TV   TRIANGULO: TELA CHEIA   QUADRADO: DESLIGAR A TV", 22, 48, 700, 0xFFDCE6F3);
        } else if (modo == MODO_JOGO && tvMidia == TVMIDIA_AUDIO) {
            preencherRet(bufferQuadro, 30, 690, 1180, 44, 0xDD062348);
            desenharTexto(bufferQuadro, "TOCANDO NA TV   TRIANGULO: CONTROLES   QUADRADO: PARAR", 22, 48, 700, 0xFFDCE6F3);
        }
        if (msgTimer > 0 && msgStatus[0] != '\0') {
            preencherRet(bufferQuadro, 0, 0, TELA_L, 62, 0xDD062348);
            desenharTexto(bufferQuadro, msgStatus, 26, 30, 14, 0xFFFFE08A);
        }

        if (videoApresentando && videoRodando) {
            if (video_minimizado && modo == MODO_JOGO) atualizarPipDaTV(cam);
            else if (video_minimizado) esconderPipDaTV();
        }

        memcpy(tela, bufferQuadro, (size_t)TELA_L * TELA_A * 4);
        if (videoApresentando) submeterTela();
        else if (primeiroQuadro) { submeterTela(); primeiroQuadro = false; }
        else submeterTelaSemPausa();
        uint64_t tFim = sceKernelGetProcessTime();

        float msCena = (float)(tCena1 - tCena) / 1000.0f;
        float msRender = (float)(tRender - tCena1) / 1000.0f;
        float msUpscale = (float)(tUpscale - tRender) / 1000.0f;
        float msHud = (float)(tFim - tUpscale) / 1000.0f;
        float msTotal = msCena + msRender + msUpscale + msHud;
        msSuave = msSuave * 0.90f + msTotal * 0.10f;
        acumCena += msCena; acumRender += msRender; acumUpscale += msUpscale; acumHud += msHud;
        quadrosJanela++;

        if (tFim - janelaInicio >= 1000000) {
            int n = quadrosJanela > 0 ? quadrosJanela : 1;
            FILE* log = fopen("/data/HyperNeiva/escritorio3d_log.txt", "w");
            if (log) {
                fprintf(log, "versao=%s\nfps=%.1f\ncpu=%d\nthreads=%d\ncull=%d\ntris=%d\ndrop=%d\ncena_ms=%.2f\nrender_ms=%.2f\nupscale_ms=%.2f\nhud_ms=%.2f\ntotal_ms=%.2f\npx=%.1f\npz=%.1f\nyaw=%.2f\nmodo=%d\nvideo=%d\npip=%d\ndecor=%d\nparede=%u\nchao=%u\nteto=%u\n",
                    HN_VERSAO,
                    fps, (renderFallback || totalThreadsRender == 0) ? 1 : 0, totalThreadsRender, cullingAtivo ? 1 : 0,
                    totalTriangulos, triangulosDescartados,
                    acumCena / n, acumRender / n, acumUpscale / n, acumHud / n,
                    (acumCena + acumRender + acumUpscale + acumHud) / n, px, pz, yaw, modo,
                    videoRodando ? 1 : 0, video_minimizado ? 1 : 0,
                    decorTotal, (unsigned)corParedeEdit, (unsigned)corChaoEdit, (unsigned)corTetoEdit);
                fclose(log);
            }
            quadrosJanela = 0; acumCena = 0.0f; acumRender = 0.0f; acumUpscale = 0.0f; acumHud = 0.0f;
            janelaInicio = tFim;
        }

        if (!threadsIniciadas) {
            iniciarThreadsRender();
            threadsIniciadas = true;
        }

        botoesAntigos = botoes;
        tempo += dt;

        if (videoApresentando) {
            proximoQuadro = sceKernelGetProcessTime();
        } else {
            proximoQuadro += 16667;
            int64_t espera = (int64_t)(proximoQuadro - sceKernelGetProcessTime());
            if (espera > 0) sceKernelUsleep((uint32_t)espera);
            else if (espera < -50000) proximoQuadro = sceKernelGetProcessTime() + 16667;
        }
    }

    if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); }
    if (videoApresentando) { definirApresentacaoVideo(false); videoApresentando = false; }
    if (imagemPixels) { stbi_image_free(imagemPixels); imagemPixels = NULL; }
    if (tvMidia == TVMIDIA_AUDIO) tocarMusicaNova("PARADO");
    tvPararMidia();
    menuAtual = menuSalvo;

    pararThreadsRender();

    for (int i = 0; i < 90; i++) {
        OrbisPadData pad;
        if (scePadReadState(globalPadHandle, &pad) == 0 && !(pad.buttons & (ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_CROSS))) break;
        sceKernelUsleep(16000);
    }
}

}

void abrirEscritorio3D() {
    escritorio3d::executar();
}
