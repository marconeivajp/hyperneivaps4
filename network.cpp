#include "network.h"
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <stddef.h>

int netPoolId = -1;
int httpCtxId = -1;
int sslCtxId = -1;

// Ignora erros de verificação de certificado SSL (necessário para alguns scrapers)
int skipSslCallback(int libsslId, unsigned int verifyErr, void* const sslCert[], int certNum, void* userArg) {
    return 1;
}

// Inicializa todos os módulos de rede do OrbisOS
void initNetwork() {
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);

    sceNetInit();
    netPoolId = sceNetPoolCreate("netPool", 16384, 0);
    sslCtxId = sceSslInit(256 * 1024);
    httpCtxId = sceHttpInit(netPoolId, sslCtxId, 128 * 1024);
}

// =========================================================================
// CONTROLE REMOTO E ENVIO DE MÍDIA PELO NAVEGADOR (porta 8000)
// O celular ou o PC abre http://<ip-do-ps4>:8000, que serve uma página com controle
// virtual, envio de fotos e vídeos e teclado. Os botões entram em hnPadLer, a mídia
// abre em remotoAtender (thread principal) e o texto vai para o teclado do sistema.
// O trecho entre REMOTO-INICIO e REMOTO-FIM também compila no PC (teste com -DHN_PC).
// =========================================================================
// REMOTO-INICIO
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "remoto_pagina.h"

#ifndef REMOTO_RAIZ
#define REMOTO_RAIZ "/data/HyperNeiva"
#endif
#ifndef REMOTO_PASTA
#define REMOTO_PASTA "/data/HyperNeiva/transmitidos"
#endif
#define REMOTO_PORTA 8000
#ifndef REMOTO_ESCUTA
#define REMOTO_ESCUTA INADDR_ANY   // o teste no PC passa INADDR_LOOPBACK
#endif

// Funções do lado do PS4 (ou do teste no PC), definidas depois deste trecho.
static uint64_t remotoAgoraMs();
static void remotoAviso(const char* texto);
static bool remotoTecladoAberto();
static void remotoLog(const char* formato, ...);
static void remotoDormir(int ms);
bool remotoMeuIp(char* saida, int tamanho);

#ifdef HN_PC
#define HN_SO_REUSEADDR SO_REUSEADDR
#ifdef SO_REUSEPORT
#define HN_SO_REUSEPORT SO_REUSEPORT
#else
#define HN_SO_REUSEPORT SO_REUSEADDR
#endif
#define HN_SO_RCVTIMEO SO_RCVTIMEO
#define HN_SO_SNDTIMEO SO_SNDTIMEO
#define HN_SO_NOSIGPIPE 0
#define HN_IP_ADD_MEMBERSHIP IP_ADD_MEMBERSHIP
#define HN_IP_MULTICAST_TTL IP_MULTICAST_TTL
#else
// Os cabeçalhos do OpenOrbis trazem os números do Linux (SO_RCVTIMEO 20, IP_ADD_MEMBERSHIP 35),
// mas o kernel do PS4 é FreeBSD: com eles o timeout e o multicast não pegam.
#define HN_SO_REUSEADDR 0x0004
#define HN_SO_REUSEPORT 0x0200
#define HN_SO_NOSIGPIPE 0x0800
#define HN_SO_SNDTIMEO 0x1005
#define HN_SO_RCVTIMEO 0x1006
#define HN_IP_MULTICAST_TTL 10
#define HN_IP_ADD_MEMBERSHIP 12
#endif

// Sem SIGPIPE (fecharia o app) e com tempo máximo de espera na leitura e na escrita.
static void remotoPrepararSocket(int fd, int segundos) {
    int sim = 1;
    if (HN_SO_NOSIGPIPE) setsockopt(fd, SOL_SOCKET, HN_SO_NOSIGPIPE, (void*)&sim, sizeof(sim));
    struct timeval tv; tv.tv_sec = segundos; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, HN_SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, HN_SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
}

static pthread_mutex_t remotoTrava = PTHREAD_MUTEX_INITIALIZER;

// Thread solta com pilha de 512 KB. A pilha padrão do PS4 é pequena: a transcrição de áudio
// (buffer de 64 KB na pilha) derrubava o app logo ao começar.
static bool remotoCriarThread(void* (*funcao)(void*), void* arg) {
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 512 * 1024);
    pthread_t th;
    bool ok = pthread_create(&th, &at, funcao, arg) == 0;
    pthread_attr_destroy(&at);
    if (ok) pthread_detach(th);
    return ok;
}
static uint32_t remotoSegurados = 0;        // botões que o celular está segurando agora
static uint64_t remotoAteMs[32];             // um toque rápido continua visível até este instante
static uint64_t remotoUltimoBotaoMs = 0;
static uint8_t remotoAnalog[4] = { 128, 128, 128, 128 };
static uint64_t remotoUltimoAnalogMs = 0;
static char remotoMidia[512];
static bool remotoTemMidia = false;
static char remotoTexto[2048];
static bool remotoTemTexto = false;
static volatile int remotoConexoes = 0;
static volatile bool remotoLigado = false;
static volatile bool remotoJaConectou = false;

// Toque rápido (desce e sobe antes do próximo quadro) precisa durar alguns quadros para o menu ver.
#define REMOTO_TOQUE_MS 80
// Sem notícia do celular por este tempo, solta tudo: a conexão pode ter caído com o dedo no botão.
#define REMOTO_SEGURAR_MS 1500
#define REMOTO_ANALOG_MS 700

uint32_t remotoBotoes(uint8_t analog[4], bool* temAnalog) {
    uint64_t agora = remotoAgoraMs();
    pthread_mutex_lock(&remotoTrava);
    if (remotoSegurados && agora - remotoUltimoBotaoMs > REMOTO_SEGURAR_MS) remotoSegurados = 0;
    uint32_t m = remotoSegurados;
    for (int i = 0; i < 32; i++) if (remotoAteMs[i] > agora) m |= 1u << i;
    *temAnalog = remotoUltimoAnalogMs && agora - remotoUltimoAnalogMs < REMOTO_ANALOG_MS;
    if (*temAnalog) memcpy(analog, remotoAnalog, 4);
    pthread_mutex_unlock(&remotoTrava);
    return m;
}

static void remotoDefinirBotoes(uint32_t segurados, uint32_t tocados) {
    uint64_t agora = remotoAgoraMs();
    pthread_mutex_lock(&remotoTrava);
    uint32_t novos = tocados | (segurados & ~remotoSegurados);
    for (int i = 0; i < 32; i++) if (novos & (1u << i)) remotoAteMs[i] = agora + REMOTO_TOQUE_MS;
    remotoSegurados = segurados;
    remotoUltimoBotaoMs = agora;
    pthread_mutex_unlock(&remotoTrava);
}

static void remotoDefinirAnalog(const int v[4]) {
    pthread_mutex_lock(&remotoTrava);
    bool centro = true;
    for (int i = 0; i < 4; i++) {
        int x = v[i] < 0 ? 0 : (v[i] > 255 ? 255 : v[i]);
        remotoAnalog[i] = (uint8_t)x;
        if (x < 118 || x > 138) centro = false;
    }
    // Analógico no centro devolve o controle físico na hora.
    remotoUltimoAnalogMs = centro ? 0 : remotoAgoraMs();
    pthread_mutex_unlock(&remotoTrava);
}

bool remotoPegarMidia(char* saida, int tamanho) {
    pthread_mutex_lock(&remotoTrava);
    bool tem = remotoTemMidia;
    if (tem) { snprintf(saida, tamanho, "%s", remotoMidia); remotoTemMidia = false; }
    pthread_mutex_unlock(&remotoTrava);
    return tem;
}

bool remotoPegarTexto(char* saida, int tamanho) {
    pthread_mutex_lock(&remotoTrava);
    bool tem = remotoTemTexto;
    if (tem) { snprintf(saida, tamanho, "%s", remotoTexto); remotoTemTexto = false; }
    pthread_mutex_unlock(&remotoTrava);
    return tem;
}

static void remotoPedirMidia(const char* caminho) {
    pthread_mutex_lock(&remotoTrava);
    snprintf(remotoMidia, sizeof(remotoMidia), "%s", caminho);
    remotoTemMidia = true;
    pthread_mutex_unlock(&remotoTrava);
}

// 0 = imagem, 1 = vídeo, 2 = outro.
int remotoTipoArquivo(const char* nome) {
    const char* p = strrchr(nome, '.');
    if (!p) return 2;
    static const char* imagens[] = { ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tga", ".psd", ".pic", ".pnm", ".ppm", NULL };
    static const char* videos[] = { ".mp4", ".m4v", ".mov", ".mkv", ".webm", ".avi", ".3gp", ".ts", ".m2ts", ".mts", ".flv", ".wmv", ".mpg", ".mpeg", ".ogv", ".mp3", ".m4a", ".aac", ".wav", ".flac", ".ogg", ".opus", NULL };
    for (int i = 0; imagens[i]; i++) if (strcasecmp(p, imagens[i]) == 0) return 0;
    for (int i = 0; videos[i]; i++) if (strcasecmp(p, videos[i]) == 0) return 1;
    return 2;
}

static bool remotoEnviarTudo(int fd, const void* dados, size_t tamanho) {
    const char* p = (const char*)dados;
    while (tamanho > 0) {
        ssize_t n = send(fd, p, tamanho, 0);
        if (n <= 0) { if (n < 0 && errno == EINTR) continue; return false; }
        p += n; tamanho -= (size_t)n;
    }
    return true;
}

static bool remotoResponder(int fd, const char* status, const char* tipo, const char* corpo, size_t tamanho) {
    char cab[512];
    int n = snprintf(cab, sizeof(cab),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
        "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\n", status, tipo, tamanho);
    if (!remotoEnviarTudo(fd, cab, (size_t)n)) return false;
    return tamanho == 0 || remotoEnviarTudo(fd, corpo, tamanho);
}

static bool remotoJson(int fd, const char* status, const char* json) {
    return remotoResponder(fd, status, "application/json; charset=utf-8", json, strlen(json));
}

static int remotoHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Valor de um parâmetro da query (?a=1&b=2), já decodificado.
static bool remotoParam(const char* query, const char* nome, char* saida, int tamanho) {
    size_t lenNome = strlen(nome);
    const char* p = query;
    while (p && *p) {
        if (strncmp(p, nome, lenNome) == 0 && p[lenNome] == '=') {
            p += lenNome + 1;
            int n = 0;
            while (*p && *p != '&' && n < tamanho - 1) {
                if (*p == '%' && remotoHex(p[1]) >= 0 && remotoHex(p[2]) >= 0) { saida[n++] = (char)(remotoHex(p[1]) * 16 + remotoHex(p[2])); p += 3; }
                else if (*p == '+') { saida[n++] = ' '; p++; }
                else saida[n++] = *p++;
            }
            saida[n] = '\0';
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

// Nome vindo do celular vira nome de arquivo seguro, sem pasta e sem caracteres proibidos.
static void remotoNomeSeguro(const char* entrada, char* saida, int tamanho) {
    const char* base = entrada;
    for (const char* p = entrada; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    while (*base == '.' || *base == ' ') base++;
    int n = 0;
    for (const char* p = base; *p && n < tamanho - 1 && n < 180; p++) {
        unsigned char c = (unsigned char)*p;
        saida[n++] = (c < 32 || strchr(":*?\"<>|", c)) ? '_' : (char)c;
    }
    // Não deixa um caractere UTF-8 cortado no meio.
    while (n > 0 && ((unsigned char)saida[n - 1] & 0xC0) == 0x80) n--;
    if (n > 0 && ((unsigned char)saida[n - 1] & 0xC0) == 0xC0) n--;
    saida[n] = '\0';
    if (!saida[0]) snprintf(saida, tamanho, "arquivo");
}

static void remotoJsonTexto(const char* entrada, char* saida, int tamanho) {
    int n = 0;
    for (const char* p = entrada; *p && n < tamanho - 7; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { saida[n++] = '\\'; saida[n++] = (char)c; }
        else if (c < 32) n += snprintf(saida + n, tamanho - n, "\\u%04x", c);
        else saida[n++] = (char)c;
    }
    saida[n] = '\0';
}

struct RemotoArquivo { char nome[192]; long long tamanho; long long quando; };

static int remotoCompararArquivos(const void* a, const void* b) {
    long long qa = ((const RemotoArquivo*)a)->quando, qb = ((const RemotoArquivo*)b)->quando;
    return qa < qb ? 1 : (qa > qb ? -1 : 0);
}

static void remotoResponderLista(int fd) {
    static const int MAXIMO = 200;
    RemotoArquivo* lista = (RemotoArquivo*)malloc(sizeof(RemotoArquivo) * MAXIMO);
    int total = 0;
    DIR* d = lista ? opendir(REMOTO_PASTA) : NULL;
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) && total < MAXIMO) {
            if (e->d_name[0] == '.') continue;
            size_t len = strlen(e->d_name);
            if (len > 6 && strcmp(e->d_name + len - 6, ".parte") == 0) continue;
            char caminho[512]; struct stat st;
            snprintf(caminho, sizeof(caminho), "%s/%s", REMOTO_PASTA, e->d_name);
            if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            if (strlen(e->d_name) >= sizeof(lista[total].nome)) continue;
            memcpy(lista[total].nome, e->d_name, strlen(e->d_name) + 1);
            lista[total].tamanho = (long long)st.st_size;
            lista[total].quando = (long long)st.st_mtime;
            total++;
        }
        closedir(d);
    }
    if (total > 1) qsort(lista, total, sizeof(RemotoArquivo), remotoCompararArquivos);
    size_t cap = 64 + (size_t)total * 420;
    char* json = (char*)malloc(cap);
    if (!json) { free(lista); remotoJson(fd, "500 Internal Server Error", "[]"); return; }
    size_t n = 0;
    json[n++] = '[';
    for (int i = 0; i < total; i++) {
        char nome[400]; remotoJsonTexto(lista[i].nome, nome, sizeof(nome));
        n += snprintf(json + n, cap - n, "%s{\"n\":\"%s\",\"t\":%lld}", i ? "," : "", nome, lista[i].tamanho);
    }
    json[n++] = ']'; json[n] = '\0';
    remotoResponder(fd, "200 OK", "application/json; charset=utf-8", json, n);
    free(json); free(lista);
}

struct RemotoConexao { int fd; char buf[16384]; int len; };

static int remotoReceber(RemotoConexao* c) {
    if (c->len >= (int)sizeof(c->buf) - 1) return -1;
    ssize_t n = recv(c->fd, c->buf + c->len, sizeof(c->buf) - 1 - c->len, 0);
    if (n > 0) { c->len += (int)n; c->buf[c->len] = '\0'; }
    return (int)n;
}

// Tira os primeiros n bytes do buffer (pedido já tratado).
static void remotoConsumir(RemotoConexao* c, int n) {
    if (n >= c->len) { c->len = 0; c->buf[0] = '\0'; return; }
    memmove(c->buf, c->buf + n, c->len - n);
    c->len -= n; c->buf[c->len] = '\0';
}

// Recebe o corpo direto num arquivo. Devolve false se a conexão caiu.
// Cria a pasta e as de cima que faltarem.
static bool remotoCriarPastas(const char* caminho) {
    char parcial[1024]; snprintf(parcial, sizeof(parcial), "%s", caminho);
    for (char* q = parcial + 1; *q; q++) if (*q == '/') { *q = '\0'; mkdir(parcial, 0777); *q = '/'; }
    return mkdir(parcial, 0777) == 0 || errno == EEXIST;
}

static bool remotoReceberArquivo(RemotoConexao* c, long long tamanho, const char* pasta, const char* nome, char* erro, int tamErro) {
    remotoCriarPastas(pasta);
    char final_[1024], parte[1040];
    snprintf(final_, sizeof(final_), "%s%s%s", pasta, pasta[strlen(pasta) - 1] == '/' ? "" : "/", nome);
    snprintf(parte, sizeof(parte), "%s.parte", final_);
    FILE* f = fopen(parte, "wb");
    if (!f) snprintf(erro, tamErro, "nao abriu o arquivo (errno %d)", errno);
    const int BLOCO = 512 * 1024;
    char* bloco = (char*)malloc(BLOCO);
    long long recebido = 0;
    int ultimoPct = -1;
    uint64_t inicio = remotoAgoraMs(), ultimoAviso = 0;
    bool ok = true;

    int jaTem = c->len < tamanho ? c->len : (int)tamanho;
    if (f && jaTem > 0 && fwrite(c->buf, 1, jaTem, f) != (size_t)jaTem) { snprintf(erro, tamErro, "disco cheio?"); fclose(f); f = NULL; remove(parte); }
    recebido = jaTem;
    remotoConsumir(c, jaTem);

    while (recebido < tamanho) {
        long long falta = tamanho - recebido;
        int pedir = falta < BLOCO ? (int)falta : BLOCO;
        ssize_t n = bloco ? recv(c->fd, bloco, pedir, 0) : -1;
        if (n <= 0) { if (n < 0 && errno == EINTR) continue; ok = false; break; }
        // Arquivo com erro: continua lendo para responder ao celular, mas descarta.
        if (f && fwrite(bloco, 1, (size_t)n, f) != (size_t)n) { snprintf(erro, tamErro, "disco cheio?"); fclose(f); f = NULL; remove(parte); }
        recebido += n;
        int pct = (int)(recebido * 100 / tamanho);
        uint64_t agora = remotoAgoraMs();
        if (pct != ultimoPct && agora - ultimoAviso > 250) {
            ultimoPct = pct; ultimoAviso = agora;
            double seg = (double)(agora - inicio) / 1000.0;
            char aviso[128];
            snprintf(aviso, sizeof(aviso), "CELULAR: RECEBENDO %d%% (%.1f MB/s)", pct, seg > 0 ? (double)recebido / 1048576.0 / seg : 0.0);
            remotoAviso(aviso);
        }
    }
    free(bloco);
    if (f) {
        if (fclose(f) != 0 && !erro[0]) snprintf(erro, tamErro, "falha ao gravar");
        if (!ok || erro[0]) remove(parte);
        else { remove(final_); if (rename(parte, final_) != 0) snprintf(erro, tamErro, "falha ao renomear (errno %d)", errno); }
    }
    remotoLog("ENVIO %s %lld de %lld bytes em %llu ms %s", nome, recebido, tamanho,
        (unsigned long long)(remotoAgoraMs() - inicio), ok ? (erro[0] ? erro : "ok") : "conexao caiu");
    if (!ok) remotoAviso("CELULAR: ENVIO INTERROMPIDO");
    return ok;
}


// =========================================================================
// FONTE REMOTA: o player lê um vídeo que está em outro lugar, em blocos, sem esperar o
// arquivo inteiro. Os blocos vêm do navegador (vídeo escolhido no celular ou no PC) ou de
// um servidor HTTP com Range (DLNA). Só existe uma fonte por vez.
// =========================================================================
#define FONTE_BLOCO (512 * 1024)
#define FONTE_SLOTS 32
#define FONTE_ADIANTE 8

struct FonteSlot { long long idx; uint8_t* dados; int tam; int estado; bool enviado; uint64_t pedidoEm; uint64_t usadoEm; };
enum { FONTE_NADA = 0, FONTE_NAVEGADOR = 1, FONTE_HTTP = 2 };

static struct {
    pthread_mutex_t trava;
    int tipo;
    unsigned id;
    long long total;          // -1 = ainda não sabe
    long long pos;
    volatile bool parar;
    bool semRange;            // servidor HTTP que não aceita Range: lê em sequência
    bool falhou;
    FonteSlot s[FONTE_SLOTS];
    char nome[200];
    char url[2048];
    uint64_t ultimoContato;   // navegador: último pedido de blocos recebido
} fonte = { PTHREAD_MUTEX_INITIALIZER };

static void fonteLimparTravado() {
    for (int i = 0; i < FONTE_SLOTS; i++) {
        if (fonte.s[i].dados) free(fonte.s[i].dados);
        memset(&fonte.s[i], 0, sizeof(FonteSlot));
        fonte.s[i].idx = -1;
    }
    fonte.pos = 0; fonte.total = -1; fonte.semRange = false; fonte.falhou = false;
    fonte.nome[0] = '\0'; fonte.url[0] = '\0';
}

void remotoFonteFechar() {
    pthread_mutex_lock(&fonte.trava);
    fonte.parar = true;
    fonte.tipo = FONTE_NADA;
    fonte.id++;
    fonteLimparTravado();
    pthread_mutex_unlock(&fonte.trava);
}

// Cada leitura do player fica presa à fonte que ela abriu: uma fonte nova (outro vídeo
// chegando) não pode ser fechada nem lida pelo vídeo antigo que está saindo.
void remotoFontePararLeitura(unsigned id) { if (fonte.id == id) fonte.parar = true; }

void remotoFonteFecharSe(unsigned id) {
    pthread_mutex_lock(&fonte.trava);
    bool mesma = fonte.id == id;
    pthread_mutex_unlock(&fonte.trava);
    if (mesma) remotoFonteFechar();
}

unsigned remotoFonteIdAtual() { return fonte.tipo != FONTE_NADA ? fonte.id : 0; }

static FonteSlot* fonteAchar(long long idx) {
    for (int i = 0; i < FONTE_SLOTS; i++) if (fonte.s[i].estado && fonte.s[i].idx == idx) return &fonte.s[i];
    return NULL;
}

// Escolhe uma vaga para o bloco: livre, ou o pronto mais antigo fora da janela de leitura.
static FonteSlot* fonteVaga(long long atual) {
    FonteSlot* melhor = NULL;
    for (int i = 0; i < FONTE_SLOTS; i++) {
        FonteSlot* s = &fonte.s[i];
        if (!s->estado) return s;
        if (s->estado == 2 && (s->idx < atual || s->idx > atual + FONTE_ADIANTE) && (!melhor || s->usadoEm < melhor->usadoEm)) melhor = s;
    }
    return melhor;
}

static void fontePedir(long long idx, long long atual) {
    if (idx < 0 || (fonte.total >= 0 && idx * (long long)FONTE_BLOCO >= fonte.total)) return;
    FonteSlot* s = fonteAchar(idx);
    if (s) return;
    s = fonteVaga(atual);
    if (!s) return;
    if (!s->dados) { s->dados = (uint8_t*)malloc(FONTE_BLOCO); if (!s->dados) return; }
    s->idx = idx; s->estado = 1; s->enviado = false; s->tam = 0;
    s->pedidoEm = remotoAgoraMs(); s->usadoEm = s->pedidoEm;
}

static bool fonteGuardarTravado(long long idx, const uint8_t* dados, int tam) {
    FonteSlot* s = fonteAchar(idx);
    if (!s && fonte.semRange) {
        // Sem Range os blocos chegam em ordem: guarda sem pedido.
        long long atual = fonte.pos / FONTE_BLOCO;
        s = fonteVaga(atual);
        if (s && !s->dados) s->dados = (uint8_t*)malloc(FONTE_BLOCO);
        if (!s || !s->dados) return false;
        s->idx = idx; s->estado = 1;
    }
    if (!s || s->estado == 2) return false;
    if (tam > FONTE_BLOCO) tam = FONTE_BLOCO;
    memcpy(s->dados, dados, tam);
    s->tam = tam; s->estado = 2; s->usadoEm = remotoAgoraMs();
    return true;
}

// Leitura do player. Devolve bytes lidos, 0 no fim, -1 em erro ou quando o vídeo fecha.
int remotoFonteLer(unsigned id, uint8_t* destino, int tamanho) {
    uint64_t inicio = remotoAgoraMs();
    for (;;) {
        pthread_mutex_lock(&fonte.trava);
        if (fonte.id != id || fonte.parar || fonte.tipo == FONTE_NADA || fonte.falhou) { pthread_mutex_unlock(&fonte.trava); return -1; }
        if (fonte.total >= 0 && fonte.pos >= fonte.total) { pthread_mutex_unlock(&fonte.trava); return 0; }
        long long b = fonte.pos / FONTE_BLOCO;
        if (!fonte.semRange) for (int k = 0; k <= FONTE_ADIANTE; k++) fontePedir(b + k, b);
        FonteSlot* s = fonteAchar(b);
        uint64_t agora = remotoAgoraMs();
        if (s && s->estado == 2) {
            int off = (int)(fonte.pos - b * FONTE_BLOCO);
            int n = s->tam - off;
            if (n <= 0) {
                // Bloco curto: fim do arquivo quando o tamanho não é conhecido.
                pthread_mutex_unlock(&fonte.trava);
                return 0;
            }
            if (n > tamanho) n = tamanho;
            memcpy(destino, s->dados + off, n);
            fonte.pos += n;
            s->usadoEm = agora;
            pthread_mutex_unlock(&fonte.trava);
            return n;
        }
        // Pedido sem resposta: pede de novo (a conexão do celular pode ter caído).
        for (int i = 0; i < FONTE_SLOTS; i++) {
            FonteSlot* p = &fonte.s[i];
            if (p->estado == 1 && p->enviado && agora - p->pedidoEm > 8000) { p->enviado = false; p->pedidoEm = agora; }
        }
        pthread_mutex_unlock(&fonte.trava);
        if (agora - inicio > 30000) { remotoLog("FONTE tempo esgotado no bloco %lld", b); return -1; }
        remotoDormir(5);
    }
}

// SEEK_SET/CUR/END. Devolve a posição nova ou -1.
long long remotoFonteSaltar(unsigned id, long long posicao, int origem) {
    pthread_mutex_lock(&fonte.trava);
    if (fonte.id != id) { pthread_mutex_unlock(&fonte.trava); return -1; }
    long long nova = origem == SEEK_SET ? posicao : origem == SEEK_CUR ? fonte.pos + posicao : (fonte.total >= 0 ? fonte.total + posicao : -1);
    bool ok = nova >= 0;
    if (ok && fonte.semRange) {
        // Sem Range só dá para andar dentro do que ainda está na memória ou para a frente.
        long long b = nova / FONTE_BLOCO;
        FonteSlot* s = fonteAchar(b);
        long long menor = -1;
        for (int i = 0; i < FONTE_SLOTS; i++) if (fonte.s[i].estado == 2 && (menor < 0 || fonte.s[i].idx < menor)) menor = fonte.s[i].idx;
        if (!s && menor >= 0 && b < menor) ok = false;
    }
    if (ok) fonte.pos = nova;
    pthread_mutex_unlock(&fonte.trava);
    return ok ? nova : -1;
}

// Tamanho total; espera o primeiro contato do servidor HTTP.
long long remotoFonteTamanho(unsigned id) {
    uint64_t inicio = remotoAgoraMs();
    for (;;) {
        pthread_mutex_lock(&fonte.trava);
        if (fonte.id != id) { pthread_mutex_unlock(&fonte.trava); return -1; }
        long long t = fonte.total;
        bool espera = fonte.tipo == FONTE_HTTP && t < 0 && !fonte.semRange && !fonte.falhou && !fonte.parar;
        pthread_mutex_unlock(&fonte.trava);
        if (!espera || remotoAgoraMs() - inicio > 15000) return t;
        remotoDormir(10);
    }
}

// ---- Fonte no navegador: a página pergunta quais blocos mandar e manda cada um ----
unsigned remotoFonteAbrirNavegador(const char* nome, long long total) {
    pthread_mutex_lock(&fonte.trava);
    fonte.id++;
    fonteLimparTravado();
    fonte.tipo = FONTE_NAVEGADOR;
    fonte.total = total;
    fonte.parar = false;
    fonte.ultimoContato = remotoAgoraMs();
    snprintf(fonte.nome, sizeof(fonte.nome), "%s", nome);
    // Começo e fim já pedidos: o MP4 de celular costuma ter o índice no final.
    fontePedir(0, 0); fontePedir(1, 0);
    if (total > FONTE_BLOCO * 2) fontePedir((total - 1) / FONTE_BLOCO, 0);
    unsigned id = fonte.id;
    pthread_mutex_unlock(&fonte.trava);
    return id;
}

// Até 'maximo' blocos pedidos e ainda não enviados à página. -1 = fonte trocada ou fechada.
static int fontePendentes(unsigned id, long long* lista, int maximo) {
    pthread_mutex_lock(&fonte.trava);
    if (fonte.id != id || fonte.tipo != FONTE_NAVEGADOR) { pthread_mutex_unlock(&fonte.trava); return -1; }
    fonte.ultimoContato = remotoAgoraMs();
    int n = 0;
    // Primeiro o bloco que o player está esperando, depois os da frente.
    long long atual = fonte.pos / FONTE_BLOCO;
    for (int passo = 0; passo < 2; passo++) {
        for (int i = 0; i < FONTE_SLOTS && n < maximo; i++) {
            FonteSlot* s = &fonte.s[i];
            if (s->estado != 1 || s->enviado) continue;
            if (passo == 0 && s->idx != atual) continue;
            s->enviado = true; s->pedidoEm = remotoAgoraMs();
            lista[n++] = s->idx;
        }
    }
    pthread_mutex_unlock(&fonte.trava);
    return n;
}

static bool fonteEntregar(unsigned id, long long idx, const uint8_t* dados, int tam) {
    pthread_mutex_lock(&fonte.trava);
    bool ok = fonte.id == id && fonte.tipo == FONTE_NAVEGADOR && fonteGuardarTravado(idx, dados, tam);
    pthread_mutex_unlock(&fonte.trava);
    return ok;
}

// ---- Cliente HTTP mínimo (só IP numérico, sem https) ----
static bool remotoSepararUrl(const char* url, char* host, int tamHost, int* porta, char* caminho, int tamCaminho) {
    if (strncasecmp(url, "http://", 7) != 0) return false;
    const char* p = url + 7;
    const char* barra = strchr(p, '/');
    int len = barra ? (int)(barra - p) : (int)strlen(p);
    if (len <= 0 || len >= tamHost) return false;
    memcpy(host, p, len); host[len] = '\0';
    *porta = 80;
    char* dois = strrchr(host, ':');
    if (dois) { *porta = atoi(dois + 1); *dois = '\0'; }
    snprintf(caminho, tamCaminho, "%s", barra ? barra : "/");
    return true;
}

static int remotoConectar(const char* host, int porta) {
    struct sockaddr_in end; memset(&end, 0, sizeof(end));
    end.sin_family = AF_INET; end.sin_port = htons((uint16_t)porta);
    if (inet_pton(AF_INET, host, &end.sin_addr) != 1) return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    remotoPrepararSocket(s, 10);
    if (connect(s, (struct sockaddr*)&end, sizeof(end)) != 0) { close(s); return -1; }
    return s;
}

// Lê o cabeçalho da resposta (até \r\n\r\n). Sobra do corpo fica em 'sobra'.
static int remotoLerCabecalho(int s, char* cab, int tamCab, int* sobraIni, int* sobraTam) {
    int len = 0;
    for (;;) {
        if (len >= tamCab - 1) return -1;
        ssize_t n = recv(s, cab + len, tamCab - 1 - len, 0);
        if (n <= 0) return -1;
        len += (int)n; cab[len] = '\0';
        char* fim = strstr(cab, "\r\n\r\n");
        if (fim) { *sobraIni = (int)(fim - cab) + 4; *sobraTam = len - *sobraIni; return len; }
    }
}

static bool remotoCabValor(const char* cab, const char* nome, char* saida, int tam) {
    size_t ln = strlen(nome);
    for (const char* p = cab; *p; p++) {
        if ((p == cab || p[-1] == '\n') && strncasecmp(p, nome, ln) == 0 && p[ln] == ':') {
            p += ln + 1;
            while (*p == ' ' || *p == '\t') p++;
            int n = 0;
            while (*p && *p != '\r' && *p != '\n' && n < tam - 1) saida[n++] = *p++;
            saida[n] = '\0';
            return true;
        }
    }
    return false;
}

static bool remotoReceberN(int s, uint8_t* destino, int n) {
    int lidos = 0;
    while (lidos < n) {
        ssize_t r = recv(s, destino + lidos, n - lidos, 0);
        if (r <= 0) { if (r < 0 && errno == EINTR) continue; return false; }
        lidos += (int)r;
    }
    return true;
}

// Thread do HTTP: busca os blocos pedidos com Range. Servidor sem Range vira leitura em sequência.
static void* fonteTrabalhadorHttp(void* arg) {
    unsigned id = (unsigned)(uintptr_t)arg;
    uint8_t* buf = (uint8_t*)malloc(FONTE_BLOCO);
    int s = -1, falhas = 0;
    char host[128] = "", caminho[2048] = "";
    int porta = 80;
    while (buf) {
        pthread_mutex_lock(&fonte.trava);
        if (fonte.id != id || fonte.parar || fonte.falhou || fonte.semRange) { pthread_mutex_unlock(&fonte.trava); break; }
        // O bloco que o player espera vem antes dos da frente.
        long long atual = fonte.pos / FONTE_BLOCO;
        FonteSlot* escolhido = NULL;
        for (int i = 0; i < FONTE_SLOTS; i++) {
            FonteSlot* sl = &fonte.s[i];
            if (sl->estado != 1 || sl->enviado) continue;
            if (!escolhido || sl->idx == atual || (escolhido->idx != atual && sl->idx < escolhido->idx)) escolhido = sl;
        }
        long long idx = -1;
        if (escolhido) { escolhido->enviado = true; escolhido->pedidoEm = remotoAgoraMs(); idx = escolhido->idx; }
        char url[2048]; snprintf(url, sizeof(url), "%s", fonte.url);
        pthread_mutex_unlock(&fonte.trava);
        if (idx < 0) { remotoDormir(5); continue; }

        char h2[128], c2[2048]; int p2;
        if (!remotoSepararUrl(url, h2, sizeof(h2), &p2, c2, sizeof(c2))) { pthread_mutex_lock(&fonte.trava); if (fonte.id == id) fonte.falhou = true; pthread_mutex_unlock(&fonte.trava); remotoLog("FONTE url invalida %s", url); break; }
        if (strcmp(h2, host) || p2 != porta || strcmp(c2, caminho)) { if (s >= 0) close(s); s = -1; snprintf(host, sizeof(host), "%s", h2); porta = p2; snprintf(caminho, sizeof(caminho), "%s", c2); }
        if (s < 0) s = remotoConectar(host, porta);
        bool certo = false;
        if (s >= 0) {
            long long a = idx * (long long)FONTE_BLOCO, b = a + FONTE_BLOCO - 1;
            char pedido[2600];
            int n = snprintf(pedido, sizeof(pedido), "GET %s HTTP/1.1\r\nHost: %s:%d\r\nRange: bytes=%lld-%lld\r\nUser-Agent: HyperNeiva/PS4\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n", caminho, host, porta, a, b);
            char cab[8192]; int ini = 0, sobra = 0;
            if (remotoEnviarTudo(s, pedido, n) && remotoLerCabecalho(s, cab, sizeof(cab), &ini, &sobra) > 0) {
                int status = 0; sscanf(cab, "HTTP/%*s %d", &status);
                char v[512];
                long long tamCorpo = remotoCabValor(cab, "Content-Length", v, sizeof(v)) ? strtoll(v, NULL, 10) : -1;
                bool fecha = remotoCabValor(cab, "Connection", v, sizeof(v)) && strncasecmp(v, "close", 5) == 0;
                if (remotoCabValor(cab, "Transfer-Encoding", v, sizeof(v)) && strstr(v, "chunked")) status = -1;
                if (status >= 300 && status < 400 && remotoCabValor(cab, "Location", v, sizeof(v))) {
                    pthread_mutex_lock(&fonte.trava);
                    if (fonte.id == id) { snprintf(fonte.url, sizeof(fonte.url), "%s", v); FonteSlot* sl = fonteAchar(idx); if (sl) sl->enviado = false; }
                    pthread_mutex_unlock(&fonte.trava);
                    remotoLog("FONTE redirecionado para %s", v);
                    close(s); s = -1; continue;
                }
                if (status == 206 && tamCorpo > 0 && tamCorpo <= FONTE_BLOCO) {
                    memcpy(buf, cab + ini, sobra);
                    if (remotoReceberN(s, buf + sobra, (int)tamCorpo - sobra)) {
                        long long total = -1;
                        if (remotoCabValor(cab, "Content-Range", v, sizeof(v))) { char* barra = strchr(v, '/'); if (barra && barra[1] != '*') total = strtoll(barra + 1, NULL, 10); }
                        pthread_mutex_lock(&fonte.trava);
                        if (fonte.id == id) { if (total > 0) fonte.total = total; fonteGuardarTravado(idx, buf, (int)tamCorpo); }
                        pthread_mutex_unlock(&fonte.trava);
                        certo = true; falhas = 0;
                    }
                    if (fecha) { close(s); s = -1; }
                }
                else if (status == 200) {
                    // Sem Range: esta thread passa a ler o arquivo do começo, em ordem.
                    pthread_mutex_lock(&fonte.trava);
                    if (fonte.id == id) { fonte.semRange = true; fonte.total = tamCorpo; for (int i = 0; i < FONTE_SLOTS; i++) if (fonte.s[i].estado == 1) fonte.s[i].estado = 0; }
                    pthread_mutex_unlock(&fonte.trava);
                    remotoLog("FONTE servidor sem Range: leitura em sequencia (%lld bytes)", tamCorpo);
                    long long idxSeq = 0, recebidoTotal = 0;
                    int noBloco = sobra < FONTE_BLOCO ? sobra : FONTE_BLOCO;
                    memcpy(buf, cab + ini, noBloco);
                    bool fim = false;
                    while (!fim) {
                        while (noBloco < FONTE_BLOCO) {
                            if (tamCorpo >= 0 && recebidoTotal + noBloco >= tamCorpo) { fim = true; break; }
                            ssize_t r = recv(s, buf + noBloco, FONTE_BLOCO - noBloco, 0);
                            if (r <= 0) { fim = true; break; }
                            noBloco += (int)r;
                        }
                        // Espera o player andar se a memória estiver cheia de blocos à frente.
                        for (;;) {
                            pthread_mutex_lock(&fonte.trava);
                            bool vivo = fonte.id == id && !fonte.parar;
                            bool cabe = idxSeq < fonte.pos / FONTE_BLOCO + FONTE_SLOTS - 2;
                            if (vivo && cabe) { fonteGuardarTravado(idxSeq, buf, noBloco); if (fim && fonte.total < 0) fonte.total = recebidoTotal + noBloco; }
                            pthread_mutex_unlock(&fonte.trava);
                            if (!vivo) { fim = true; break; }
                            if (cabe) break;
                            remotoDormir(20);
                        }
                        recebidoTotal += noBloco; idxSeq++; noBloco = 0;
                    }
                    close(s); s = -1;
                    break;
                }
                else {
                    remotoLog("FONTE HTTP %d no bloco %lld", status, idx);
                    if (status >= 400 || status < 0) { pthread_mutex_lock(&fonte.trava); if (fonte.id == id) fonte.falhou = true; pthread_mutex_unlock(&fonte.trava); }
                    close(s); s = -1;
                }
            }
            else { close(s); s = -1; }
        }
        if (!certo) {
            pthread_mutex_lock(&fonte.trava);
            if (fonte.id == id) { FonteSlot* sl = fonteAchar(idx); if (sl && sl->estado == 1) sl->enviado = false; }
            if (++falhas > 6 && fonte.id == id) { fonte.falhou = true; remotoLog("FONTE desistiu: %s", url); }
            pthread_mutex_unlock(&fonte.trava);
            remotoDormir(300);
        }
    }
    if (s >= 0) close(s);
    free(buf);
    return NULL;
}

bool remotoFonteAbrirHttp(const char* url) {
    char h[128], c[2048]; int p;
    if (!remotoSepararUrl(url, h, sizeof(h), &p, c, sizeof(c))) return false;
    struct in_addr teste;
    if (inet_pton(AF_INET, h, &teste) != 1) return false;
    pthread_mutex_lock(&fonte.trava);
    fonte.id++;
    fonteLimparTravado();
    fonte.tipo = FONTE_HTTP;
    fonte.parar = false;
    snprintf(fonte.url, sizeof(fonte.url), "%s", url);
    const char* nome = strrchr(c, '/');
    snprintf(fonte.nome, sizeof(fonte.nome), "%s", nome && nome[1] ? nome + 1 : "dlna");
    fontePedir(0, 0); fontePedir(1, 0);
    unsigned id = fonte.id;
    pthread_mutex_unlock(&fonte.trava);
    for (int i = 0; i < 2; i++) {
        pthread_t t;
        if (pthread_create(&t, NULL, fonteTrabalhadorHttp, (void*)(uintptr_t)id) == 0) pthread_detach(t);
    }
    return true;
}

// =========================================================================
// ARQUIVOS DO PS4 PELA PÁGINA (gerenciador PC | PS4)
// =========================================================================
static bool remotoCaminhoValido(const char* p) {
    if (!p || p[0] != '/') return false;
    // Nada de "/../": a página sempre manda caminho absoluto já resolvido.
    for (const char* q = p; *q; q++) if (q[0] == '/' && q[1] == '.' && q[2] == '.' && (q[3] == '/' || q[3] == '\0')) return false;
    return true;
}

static bool remotoApagarTudo(const char* caminho) {
    struct stat st;
    if (lstat(caminho, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return remove(caminho) == 0;
    DIR* d = opendir(caminho);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char filho[1024]; snprintf(filho, sizeof(filho), "%s/%s", caminho, e->d_name);
            remotoApagarTudo(filho);
        }
        closedir(d);
    }
    return rmdir(caminho) == 0;
}

static void remotoListarPasta(int fd, const char* pasta) {
    DIR* d = opendir(pasta);
    if (!d) { remotoJson(fd, "404 Not Found", "{\"erro\":\"pasta nao abriu\"}"); return; }
    size_t cap = 65536, n = 0;
    char* json = (char*)malloc(cap);
    if (!json) { closedir(d); remotoJson(fd, "500 Internal Server Error", "[]"); return; }
    json[n++] = '[';
    struct dirent* e;
    bool primeiro = true;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char caminho[1024]; snprintf(caminho, sizeof(caminho), "%s%s%s", pasta, pasta[strlen(pasta) - 1] == '/' ? "" : "/", e->d_name);
        struct stat st; bool temStat = stat(caminho, &st) == 0;
        bool dir = temStat && S_ISDIR(st.st_mode);
        char nome[600]; remotoJsonTexto(e->d_name, nome, sizeof(nome));
        if (n + 800 > cap) { cap *= 2; char* novo = (char*)realloc(json, cap); if (!novo) break; json = novo; }
        n += snprintf(json + n, cap - n, "%s{\"n\":\"%s\",\"d\":%d,\"t\":%lld,\"m\":%lld}", primeiro ? "" : ",", nome, dir ? 1 : 0,
            temStat && !dir ? (long long)st.st_size : 0LL, temStat ? (long long)st.st_mtime : 0LL);
        primeiro = false;
    }
    closedir(d);
    json[n++] = ']';
    remotoResponder(fd, "200 OK", "application/json; charset=utf-8", json, n);
    free(json);
}

static const char* remotoTipoMime(const char* nome) {
    const char* p = strrchr(nome, '.');
    if (!p) return "application/octet-stream";
    if (!strcasecmp(p, ".mp4") || !strcasecmp(p, ".m4v")) return "video/mp4";
    if (!strcasecmp(p, ".webm")) return "video/webm";
    if (!strcasecmp(p, ".mkv")) return "video/x-matroska";
    if (!strcasecmp(p, ".mov")) return "video/quicktime";
    if (!strcasecmp(p, ".mp3")) return "audio/mpeg";
    if (!strcasecmp(p, ".jpg") || !strcasecmp(p, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(p, ".png")) return "image/png";
    if (!strcasecmp(p, ".gif")) return "image/gif";
    if (!strcasecmp(p, ".txt") || !strcasecmp(p, ".log") || !strcasecmp(p, ".ini")) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

// Arquivo do PS4 para o navegador, com Range: dá para assistir no PC sem baixar tudo.
static bool remotoServirArquivo(int fd, const char* caminho, const char* cab, bool anexo) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return remotoJson(fd, "404 Not Found", "{\"erro\":\"nao abriu\"}");
    fseeko(f, 0, SEEK_END); long long total = (long long)ftello(f);
    long long ini = 0, fim = total - 1;
    char v[128]; bool parcial = false;
    if (remotoCabValor(cab, "Range", v, sizeof(v)) && strncmp(v, "bytes=", 6) == 0) {
        char* traco = strchr(v + 6, '-');
        if (traco) {
            if (v[6] == '-') { long long ult = strtoll(v + 7, NULL, 10); ini = total - ult; if (ini < 0) ini = 0; }
            else { ini = strtoll(v + 6, NULL, 10); if (traco[1]) fim = strtoll(traco + 1, NULL, 10); }
            if (fim >= total) fim = total - 1;
            parcial = true;
        }
    }
    if (total == 0) { ini = 0; fim = -1; parcial = false; }
    if (ini > fim + 1 || ini >= total + (total == 0)) { fclose(f); return remotoResponder(fd, "416 Range Not Satisfiable", "text/plain", "", 0); }
    const char* nome = strrchr(caminho, '/'); nome = nome ? nome + 1 : caminho;
    char nomeUrl[700]; int k = 0;
    for (const unsigned char* q = (const unsigned char*)nome; *q && k < 690; q++) {
        if (isalnum(*q) || strchr("._-", *q)) nomeUrl[k++] = (char)*q;
        else k += snprintf(nomeUrl + k, sizeof(nomeUrl) - k, "%%%02X", *q);
    }
    nomeUrl[k] = '\0';
    char resp[1200];
    long long tam = fim - ini + 1;
    int n = snprintf(resp, sizeof(resp), "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lld\r\nAccept-Ranges: bytes\r\n%s%s%s"
        "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n",
        parcial ? "206 Partial Content" : "200 OK", remotoTipoMime(nome), tam,
        anexo ? "Content-Disposition: attachment; filename*=UTF-8''" : "", anexo ? nomeUrl : "", anexo ? "\r\n" : "");
    if (parcial) n += snprintf(resp + n, sizeof(resp) - n, "Content-Range: bytes %lld-%lld/%lld\r\n", ini, fim, total);
    n += snprintf(resp + n, sizeof(resp) - n, "\r\n");
    bool ok = remotoEnviarTudo(fd, resp, n);
    if (ok && strncmp(cab, "HEAD", 4) != 0 && tam > 0) {
        fseeko(f, ini, SEEK_SET);
        char* bloco = (char*)malloc(256 * 1024);
        long long falta = tam;
        while (ok && bloco && falta > 0) {
            size_t pedir = falta < 256 * 1024 ? (size_t)falta : 256 * 1024;
            size_t lidos = fread(bloco, 1, pedir, f);
            if (lidos == 0) { ok = false; break; }
            ok = remotoEnviarTudo(fd, bloco, lidos);
            falta -= (long long)lidos;
        }
        free(bloco);
        if (!bloco) ok = false;
    }
    fclose(f);
    return ok;
}

static bool remotoLerCorpo(RemotoConexao* c, long long tam, char* saida, int max) {
    if (tam > max - 1) return false;
    while (c->len < tam) if (remotoReceber(c) <= 0) return false;
    memcpy(saida, c->buf, (size_t)tam); saida[tam] = '\0';
    remotoConsumir(c, (int)tam);
    return true;
}

// =========================================================================
// DLNA / UPnP: o PS4 aparece como "Hyper Neiva (PS4)" nos apps que transmitem para TV
// (BubbleUPnP, Web Video Caster, VLC, Windows "Transmitir para dispositivo").
// =========================================================================
static char dlnaUuid[64] = "";
static char dlnaUrl[2048] = "";
static char dlnaMeta[2048] = "";        // DIDL do vídeo atual (devolvido no GetMediaInfo)
static int dlnaEstado = 0;              // 0 sem mídia, 1 parado, 2 tocando, 3 pausado, 4 carregando
static volatile int dlnaComando = 0;    // pedido para a thread principal (ver remotoAtender)
static double dlnaAlvoSeek = 0;
static bool dlnaEhImagem = false;

int remotoDlnaComando(double* alvo, char* url, int tamUrl, bool* imagem) {
    pthread_mutex_lock(&remotoTrava);
    int c = dlnaComando; dlnaComando = 0;
    *alvo = dlnaAlvoSeek; *imagem = dlnaEhImagem;
    snprintf(url, tamUrl, "%s", dlnaUrl);
    pthread_mutex_unlock(&remotoTrava);
    return c;
}
void remotoDlnaDefinirEstado(int estado) { dlnaEstado = estado; }

static void dlnaPrepararUuid() {
    if (dlnaUuid[0]) return;
    char caminho[300]; snprintf(caminho, sizeof(caminho), "%s/../configuracao/dlna_uuid.txt", REMOTO_PASTA);
    FILE* f = fopen(caminho, "r");
    if (f) { if (fgets(dlnaUuid, sizeof(dlnaUuid), f)) dlnaUuid[strcspn(dlnaUuid, "\r\n")] = '\0'; fclose(f); }
    if (strlen(dlnaUuid) < 30) {
        unsigned long long semente = remotoAgoraMs() ^ ((unsigned long long)(uintptr_t)&semente << 7);
        unsigned int partes[4];
        for (int i = 0; i < 4; i++) { semente = semente * 6364136223846793005ULL + 1442695040888963407ULL; partes[i] = (unsigned int)(semente >> 32); }
        snprintf(dlnaUuid, sizeof(dlnaUuid), "uuid:%08x-%04x-4%03x-a%03x-%04x%08x", partes[0], partes[1] & 0xFFFF, partes[1] >> 20, partes[2] & 0xFFF, partes[2] >> 16, partes[3]);
        f = fopen(caminho, "w");
        if (f) { fprintf(f, "%s\n", dlnaUuid); fclose(f); }
    }
}

static void dlnaFormatarTempo(double s, char* saida, int tam) {
    if (s < 0) s = 0;
    long t = (long)s;
    snprintf(saida, tam, "%ld:%02ld:%02ld", t / 3600, (t / 60) % 60, t % 60);
}

static double dlnaLerTempo(const char* t) {
    int h = 0, m = 0; double s = 0;
    if (sscanf(t, "%d:%d:%lf", &h, &m, &s) == 3) return h * 3600 + m * 60 + s;
    return atof(t);
}

// Texto de um elemento XML (<Nome>valor</Nome>), com &amp; &lt; &gt; &quot; &apos; decodificados.
static bool dlnaXmlValor(const char* xml, const char* nome, char* saida, int tam) {
    char abre[96]; snprintf(abre, sizeof(abre), "<%s", nome);
    const char* p = xml;
    while ((p = strstr(p, abre))) {
        char depois = p[strlen(abre)];
        if (depois == '>' || depois == ' ' || depois == '/') break;
        p++;
    }
    if (!p) return false;
    p = strchr(p, '>');
    if (!p) return false;
    if (p[-1] == '/') { saida[0] = '\0'; return true; }
    p++;
    int n = 0;
    while (*p && *p != '<' && n < tam - 1) {
        if (*p == '&') {
            if (!strncmp(p, "&amp;", 5)) { saida[n++] = '&'; p += 5; continue; }
            if (!strncmp(p, "&lt;", 4)) { saida[n++] = '<'; p += 4; continue; }
            if (!strncmp(p, "&gt;", 4)) { saida[n++] = '>'; p += 4; continue; }
            if (!strncmp(p, "&quot;", 6)) { saida[n++] = '"'; p += 6; continue; }
            if (!strncmp(p, "&apos;", 6)) { saida[n++] = '\''; p += 6; continue; }
        }
        saida[n++] = *p++;
    }
    saida[n] = '\0';
    return true;
}

static void dlnaXmlEscapar(const char* entrada, char* saida, int tam) {
    int n = 0;
    for (const char* p = entrada; *p && n < tam - 7; p++) {
        if (*p == '&') n += snprintf(saida + n, tam - n, "&amp;");
        else if (*p == '<') n += snprintf(saida + n, tam - n, "&lt;");
        else if (*p == '>') n += snprintf(saida + n, tam - n, "&gt;");
        else if (*p == '"') n += snprintf(saida + n, tam - n, "&quot;");
        else saida[n++] = *p;
    }
    saida[n] = '\0';
}

static const char* DLNA_SERVICOS[3][3] = {
    { "AVTransport", "avt", "urn:upnp-org:serviceId:AVTransport" },
    { "RenderingControl", "rc", "urn:upnp-org:serviceId:RenderingControl" },
    { "ConnectionManager", "cm", "urn:upnp-org:serviceId:ConnectionManager" },
};

static bool dlnaDescricao(int fd) {
    dlnaPrepararUuid();
    char xml[4096];
    int n = snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion><device>"
        "<deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
        "<friendlyName>Hyper Neiva (PS4)</friendlyName><manufacturer>Hyper Neiva</manufacturer>"
        "<modelName>Hyper Neiva PS4</modelName><modelNumber>1</modelNumber><UDN>%s</UDN>"
        "<dlna:X_DLNADOC>DMR-1.50</dlna:X_DLNADOC><serviceList>", dlnaUuid);
    for (int i = 0; i < 3; i++) {
        n += snprintf(xml + n, sizeof(xml) - n,
            "<service><serviceType>urn:schemas-upnp-org:service:%s:1</serviceType><serviceId>%s</serviceId>"
            "<SCPDURL>/dlna/%s.xml</SCPDURL><controlURL>/dlna/%s</controlURL><eventSubURL>/dlna/%s/evento</eventSubURL></service>",
            DLNA_SERVICOS[i][0], DLNA_SERVICOS[i][2], DLNA_SERVICOS[i][1], DLNA_SERVICOS[i][1], DLNA_SERVICOS[i][1]);
    }
    n += snprintf(xml + n, sizeof(xml) - n, "</serviceList></device></root>");
    return remotoResponder(fd, "200 OK", "text/xml; charset=\"utf-8\"", xml, n);
}

// Descrição de cada serviço (SCPD): ação = lista de argumentos "nome:in|out:variavel".
static bool dlnaScpd(int fd, int servico) {
    static const char* avt[] = {
        "SetAVTransportURI", "InstanceID:in:A_ARG_TYPE_InstanceID", "CurrentURI:in:AVTransportURI", "CurrentURIMetaData:in:AVTransportURIMetaData", NULL,
        "SetNextAVTransportURI", "InstanceID:in:A_ARG_TYPE_InstanceID", "NextURI:in:AVTransportURI", "NextURIMetaData:in:AVTransportURIMetaData", NULL,
        "GetMediaInfo", "InstanceID:in:A_ARG_TYPE_InstanceID", "NrTracks:out:NumberOfTracks", "MediaDuration:out:CurrentMediaDuration", "CurrentURI:out:AVTransportURI", "CurrentURIMetaData:out:AVTransportURIMetaData", "NextURI:out:AVTransportURI", "NextURIMetaData:out:AVTransportURIMetaData", "PlayMedium:out:PlaybackStorageMedium", "RecordMedium:out:PlaybackStorageMedium", "WriteStatus:out:RecordMediumWriteStatus", NULL,
        "GetTransportInfo", "InstanceID:in:A_ARG_TYPE_InstanceID", "CurrentTransportState:out:TransportState", "CurrentTransportStatus:out:TransportStatus", "CurrentSpeed:out:TransportPlaySpeed", NULL,
        "GetPositionInfo", "InstanceID:in:A_ARG_TYPE_InstanceID", "Track:out:CurrentTrack", "TrackDuration:out:CurrentTrackDuration", "TrackMetaData:out:CurrentTrackMetaData", "TrackURI:out:CurrentTrackURI", "RelTime:out:RelativeTimePosition", "AbsTime:out:AbsoluteTimePosition", "RelCount:out:RelativeCounterPosition", "AbsCount:out:AbsoluteCounterPosition", NULL,
        "GetDeviceCapabilities", "InstanceID:in:A_ARG_TYPE_InstanceID", "PlayMedia:out:PossiblePlaybackStorageMedia", "RecMedia:out:PossibleRecordStorageMedia", "RecQualityModes:out:PossibleRecordQualityModes", NULL,
        "GetTransportSettings", "InstanceID:in:A_ARG_TYPE_InstanceID", "PlayMode:out:CurrentPlayMode", "RecQualityMode:out:CurrentRecordQualityMode", NULL,
        "Stop", "InstanceID:in:A_ARG_TYPE_InstanceID", NULL,
        "Play", "InstanceID:in:A_ARG_TYPE_InstanceID", "Speed:in:TransportPlaySpeed", NULL,
        "Pause", "InstanceID:in:A_ARG_TYPE_InstanceID", NULL,
        "Seek", "InstanceID:in:A_ARG_TYPE_InstanceID", "Unit:in:A_ARG_TYPE_SeekMode", "Target:in:A_ARG_TYPE_SeekTarget", NULL,
        "Next", "InstanceID:in:A_ARG_TYPE_InstanceID", NULL,
        "Previous", "InstanceID:in:A_ARG_TYPE_InstanceID", NULL,
        "GetCurrentTransportActions", "InstanceID:in:A_ARG_TYPE_InstanceID", "Actions:out:CurrentTransportActions", NULL,
        NULL };
    static const char* rc[] = {
        "GetVolume", "InstanceID:in:A_ARG_TYPE_InstanceID", "Channel:in:A_ARG_TYPE_Channel", "CurrentVolume:out:Volume", NULL,
        "SetVolume", "InstanceID:in:A_ARG_TYPE_InstanceID", "Channel:in:A_ARG_TYPE_Channel", "DesiredVolume:in:Volume", NULL,
        "GetMute", "InstanceID:in:A_ARG_TYPE_InstanceID", "Channel:in:A_ARG_TYPE_Channel", "CurrentMute:out:Mute", NULL,
        "SetMute", "InstanceID:in:A_ARG_TYPE_InstanceID", "Channel:in:A_ARG_TYPE_Channel", "DesiredMute:in:Mute", NULL,
        "ListPresets", "InstanceID:in:A_ARG_TYPE_InstanceID", "CurrentPresetNameList:out:PresetNameList", NULL,
        "SelectPreset", "InstanceID:in:A_ARG_TYPE_InstanceID", "PresetName:in:A_ARG_TYPE_PresetName", NULL,
        NULL };
    static const char* cm[] = {
        "GetProtocolInfo", "Source:out:SourceProtocolInfo", "Sink:out:SinkProtocolInfo", NULL,
        "GetCurrentConnectionIDs", "ConnectionIDs:out:CurrentConnectionIDs", NULL,
        "GetCurrentConnectionInfo", "ConnectionID:in:A_ARG_TYPE_ConnectionID", "RcsID:out:A_ARG_TYPE_RcsID", "AVTransportID:out:A_ARG_TYPE_AVTransportID", "ProtocolInfo:out:A_ARG_TYPE_ProtocolInfo", "PeerConnectionManager:out:A_ARG_TYPE_ConnectionManager", "PeerConnectionID:out:A_ARG_TYPE_ConnectionID", "Direction:out:A_ARG_TYPE_Direction", "Status:out:A_ARG_TYPE_ConnectionStatus", NULL,
        NULL };
    // Variáveis: nome:tipo:evento (ui4, i4, ui2, string, boolean).
    static const char* varAvt[] = { "TransportState:string:1", "TransportStatus:string:0", "PlaybackStorageMedium:string:0", "RecordStorageMedium:string:0",
        "PossiblePlaybackStorageMedia:string:0", "PossibleRecordStorageMedia:string:0", "CurrentPlayMode:string:0", "TransportPlaySpeed:string:0",
        "RecordMediumWriteStatus:string:0", "CurrentRecordQualityMode:string:0", "PossibleRecordQualityModes:string:0", "NumberOfTracks:ui4:0",
        "CurrentTrack:ui4:0", "CurrentTrackDuration:string:0", "CurrentMediaDuration:string:0", "CurrentTrackMetaData:string:0", "CurrentTrackURI:string:0",
        "AVTransportURI:string:0", "AVTransportURIMetaData:string:0", "NextAVTransportURI:string:0", "NextAVTransportURIMetaData:string:0",
        "RelativeTimePosition:string:0", "AbsoluteTimePosition:string:0", "RelativeCounterPosition:i4:0", "AbsoluteCounterPosition:i4:0",
        "CurrentTransportActions:string:0", "LastChange:string:1", "A_ARG_TYPE_SeekMode:string:0", "A_ARG_TYPE_SeekTarget:string:0", "A_ARG_TYPE_InstanceID:ui4:0", NULL };
    static const char* varRc[] = { "PresetNameList:string:0", "LastChange:string:1", "Volume:ui2:0", "Mute:boolean:0", "A_ARG_TYPE_Channel:string:0",
        "A_ARG_TYPE_InstanceID:ui4:0", "A_ARG_TYPE_PresetName:string:0", NULL };
    static const char* varCm[] = { "SourceProtocolInfo:string:1", "SinkProtocolInfo:string:1", "CurrentConnectionIDs:string:1", "A_ARG_TYPE_ConnectionStatus:string:0",
        "A_ARG_TYPE_ConnectionManager:string:0", "A_ARG_TYPE_Direction:string:0", "A_ARG_TYPE_ProtocolInfo:string:0", "A_ARG_TYPE_ConnectionID:i4:0",
        "A_ARG_TYPE_AVTransportID:i4:0", "A_ARG_TYPE_RcsID:i4:0", NULL };
    const char** acoes = servico == 0 ? avt : servico == 1 ? rc : cm;
    const char** vars = servico == 0 ? varAvt : servico == 1 ? varRc : varCm;
    size_t cap = 24000, n = 0;
    char* xml = (char*)malloc(cap);
    if (!xml) return false;
    n += snprintf(xml + n, cap - n, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion><actionList>");
    for (int i = 0; acoes[i]; ) {
        n += snprintf(xml + n, cap - n, "<action><name>%s</name><argumentList>", acoes[i++]);
        for (; acoes[i]; i++) {
            char a[96]; snprintf(a, sizeof(a), "%s", acoes[i]);
            char* d1 = strchr(a, ':'); char* d2 = d1 ? strchr(d1 + 1, ':') : NULL;
            if (!d1 || !d2) continue;
            *d1 = '\0'; *d2 = '\0';
            n += snprintf(xml + n, cap - n, "<argument><name>%s</name><direction>%s</direction><relatedStateVariable>%s</relatedStateVariable></argument>", a, d1 + 1, d2 + 1);
        }
        i++;
        n += snprintf(xml + n, cap - n, "</argumentList></action>");
    }
    n += snprintf(xml + n, cap - n, "</actionList><serviceStateTable>");
    for (int i = 0; vars[i]; i++) {
        char a[96]; snprintf(a, sizeof(a), "%s", vars[i]);
        char* d1 = strchr(a, ':'); char* d2 = d1 ? strchr(d1 + 1, ':') : NULL;
        if (!d1 || !d2) continue;
        *d1 = '\0'; *d2 = '\0';
        n += snprintf(xml + n, cap - n, "<stateVariable sendEvents=\"%s\"><name>%s</name><dataType>%s</dataType>", d2[1] == '1' ? "yes" : "no", a, d1 + 1);
        if (!strcmp(a, "Volume")) n += snprintf(xml + n, cap - n, "<allowedValueRange><minimum>0</minimum><maximum>100</maximum><step>1</step></allowedValueRange>");
        n += snprintf(xml + n, cap - n, "</stateVariable>");
    }
    n += snprintf(xml + n, cap - n, "</serviceStateTable></scpd>");
    bool ok = remotoResponder(fd, "200 OK", "text/xml; charset=\"utf-8\"", xml, n);
    free(xml);
    return ok;
}

static bool dlnaResponderSoap(int fd, int servico, const char* acao, const char* corpo) {
    char xml[6144];
    int n = snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:%sResponse xmlns:u=\"urn:schemas-upnp-org:service:%s:1\">%s</u:%sResponse></s:Body></s:Envelope>",
        acao, DLNA_SERVICOS[servico][0], corpo, acao);
    return remotoResponder(fd, "200 OK", "text/xml; charset=\"utf-8\"", xml, n);
}

static bool dlnaErroSoap(int fd, int codigo, const char* texto) {
    char xml[1024];
    int n = snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail><UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">"
        "<errorCode>%d</errorCode><errorDescription>%s</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>", codigo, texto);
    return remotoResponder(fd, "500 Internal Server Error", "text/xml; charset=\"utf-8\"", xml, n);
}

// Lidas do player: o PS4 define no lado dele, o teste no PC usa zeros.
static double remotoTempoAtual();
static double remotoTempoTotal();
static bool remotoVideoTocando();
static bool remotoVideoPausado();

static bool dlnaControle(int fd, int servico, const char* soapAction, const char* corpo) {
    char acao[64] = "";
    const char* h = strchr(soapAction, '#');
    if (h) { int n = 0; for (h++; *h && *h != '"' && n < 63; h++) acao[n++] = *h; acao[n] = '\0'; }
    if (servico == 0) {
        if (!strcmp(acao, "SetAVTransportURI")) {
            char url[2048] = "", meta[2048] = "";
            dlnaXmlValor(corpo, "CurrentURI", url, sizeof(url));
            dlnaXmlValor(corpo, "CurrentURIMetaData", meta, sizeof(meta));
            if (strncasecmp(url, "http://", 7) != 0) {
                remotoLog("DLNA url recusada: %s", url);
                remotoAviso("DLNA: SO LINK HTTP (SEM HTTPS)");
                return dlnaErroSoap(fd, 716, "Resource not found");
            }
            // Imagem: pela classe do DIDL ou pelo tipo no protocolInfo.
            bool imagem = strstr(meta, "imageItem") || strstr(meta, ":image/") || remotoTipoArquivo(url) == 0;
            pthread_mutex_lock(&remotoTrava);
            snprintf(dlnaUrl, sizeof(dlnaUrl), "%s", url);
            snprintf(dlnaMeta, sizeof(dlnaMeta), "%s", meta);
            dlnaEhImagem = imagem;
            dlnaEstado = 1;
            pthread_mutex_unlock(&remotoTrava);
            remotoLog("DLNA SetAVTransportURI imagem=%d %s", imagem ? 1 : 0, url);
            return dlnaResponderSoap(fd, 0, acao, "");
        }
        if (!strcmp(acao, "Play")) {
            if (!dlnaUrl[0]) return dlnaErroSoap(fd, 701, "Transition not available");
            pthread_mutex_lock(&remotoTrava); dlnaComando = 1; pthread_mutex_unlock(&remotoTrava);
            return dlnaResponderSoap(fd, 0, acao, "");
        }
        if (!strcmp(acao, "Pause")) { pthread_mutex_lock(&remotoTrava); dlnaComando = 2; pthread_mutex_unlock(&remotoTrava); return dlnaResponderSoap(fd, 0, acao, ""); }
        if (!strcmp(acao, "Stop")) { pthread_mutex_lock(&remotoTrava); dlnaComando = 3; pthread_mutex_unlock(&remotoTrava); return dlnaResponderSoap(fd, 0, acao, ""); }
        if (!strcmp(acao, "Seek")) {
            char unidade[32] = "", alvo[64] = "";
            dlnaXmlValor(corpo, "Unit", unidade, sizeof(unidade));
            dlnaXmlValor(corpo, "Target", alvo, sizeof(alvo));
            pthread_mutex_lock(&remotoTrava); dlnaAlvoSeek = dlnaLerTempo(alvo); dlnaComando = 4; pthread_mutex_unlock(&remotoTrava);
            return dlnaResponderSoap(fd, 0, acao, "");
        }
        if (!strcmp(acao, "SetNextAVTransportURI") || !strcmp(acao, "Next") || !strcmp(acao, "Previous")) return dlnaResponderSoap(fd, 0, acao, "");
        if (!strcmp(acao, "GetTransportInfo")) {
            const char* estados[] = { "NO_MEDIA_PRESENT", "STOPPED", "PLAYING", "PAUSED_PLAYBACK", "TRANSITIONING" };
            int e = dlnaEstado;
            if (e >= 2 && e <= 3 && !dlnaEhImagem) e = !remotoVideoTocando() ? 1 : (remotoVideoPausado() ? 3 : 2);
            char r[256]; snprintf(r, sizeof(r), "<CurrentTransportState>%s</CurrentTransportState><CurrentTransportStatus>OK</CurrentTransportStatus><CurrentSpeed>1</CurrentSpeed>", estados[e]);
            return dlnaResponderSoap(fd, 0, acao, r);
        }
        if (!strcmp(acao, "GetPositionInfo") || !strcmp(acao, "GetMediaInfo")) {
            char dur[32], rel[32], urlX[2600], metaX[2600];
            dlnaFormatarTempo(remotoTempoTotal(), dur, sizeof(dur));
            dlnaFormatarTempo(remotoTempoAtual(), rel, sizeof(rel));
            dlnaXmlEscapar(dlnaUrl, urlX, sizeof(urlX));
            dlnaXmlEscapar(dlnaMeta, metaX, sizeof(metaX));
            char r[6000];
            if (acao[3] == 'P') snprintf(r, sizeof(r), "<Track>1</Track><TrackDuration>%s</TrackDuration><TrackMetaData>%s</TrackMetaData><TrackURI>%s</TrackURI>"
                "<RelTime>%s</RelTime><AbsTime>%s</AbsTime><RelCount>2147483647</RelCount><AbsCount>2147483647</AbsCount>", dur, metaX, urlX, rel, rel);
            else snprintf(r, sizeof(r), "<NrTracks>%d</NrTracks><MediaDuration>%s</MediaDuration><CurrentURI>%s</CurrentURI><CurrentURIMetaData>%s</CurrentURIMetaData>"
                "<NextURI></NextURI><NextURIMetaData></NextURIMetaData><PlayMedium>NETWORK</PlayMedium><RecordMedium>NOT_IMPLEMENTED</RecordMedium><WriteStatus>NOT_IMPLEMENTED</WriteStatus>",
                dlnaUrl[0] ? 1 : 0, dur, urlX, metaX);
            return dlnaResponderSoap(fd, 0, acao, r);
        }
        if (!strcmp(acao, "GetDeviceCapabilities")) return dlnaResponderSoap(fd, 0, acao, "<PlayMedia>NETWORK</PlayMedia><RecMedia>NOT_IMPLEMENTED</RecMedia><RecQualityModes>NOT_IMPLEMENTED</RecQualityModes>");
        if (!strcmp(acao, "GetTransportSettings")) return dlnaResponderSoap(fd, 0, acao, "<PlayMode>NORMAL</PlayMode><RecQualityMode>NOT_IMPLEMENTED</RecQualityMode>");
        if (!strcmp(acao, "GetCurrentTransportActions")) return dlnaResponderSoap(fd, 0, acao, "<Actions>Play,Pause,Stop,Seek</Actions>");
    }
    else if (servico == 1) {
        if (!strcmp(acao, "GetVolume")) return dlnaResponderSoap(fd, 1, acao, "<CurrentVolume>100</CurrentVolume>");
        if (!strcmp(acao, "GetMute")) return dlnaResponderSoap(fd, 1, acao, "<CurrentMute>0</CurrentMute>");
        if (!strcmp(acao, "SetVolume") || !strcmp(acao, "SetMute") || !strcmp(acao, "SelectPreset")) return dlnaResponderSoap(fd, 1, acao, "");
        if (!strcmp(acao, "ListPresets")) return dlnaResponderSoap(fd, 1, acao, "<CurrentPresetNameList>FactoryDefaults</CurrentPresetNameList>");
    }
    else {
        if (!strcmp(acao, "GetProtocolInfo")) return dlnaResponderSoap(fd, 2, acao,
            "<Source></Source><Sink>http-get:*:video/mp4:*,http-get:*:video/x-matroska:*,http-get:*:video/x-mkv:*,http-get:*:video/webm:*,"
            "http-get:*:video/quicktime:*,http-get:*:video/x-msvideo:*,http-get:*:video/avi:*,http-get:*:video/mpeg:*,http-get:*:video/mp2t:*,"
            "http-get:*:video/vnd.dlna.mpeg-tts:*,http-get:*:video/x-flv:*,http-get:*:video/3gpp:*,http-get:*:video/x-ms-wmv:*,http-get:*:video/*:*,"
            "http-get:*:image/jpeg:*,http-get:*:image/png:*,http-get:*:image/gif:*,http-get:*:image/bmp:*</Sink>");
        if (!strcmp(acao, "GetCurrentConnectionIDs")) return dlnaResponderSoap(fd, 2, acao, "<ConnectionIDs>0</ConnectionIDs>");
        if (!strcmp(acao, "GetCurrentConnectionInfo")) return dlnaResponderSoap(fd, 2, acao,
            "<RcsID>0</RcsID><AVTransportID>0</AVTransportID><ProtocolInfo></ProtocolInfo><PeerConnectionManager></PeerConnectionManager>"
            "<PeerConnectionID>-1</PeerConnectionID><Direction>Input</Direction><Status>OK</Status>");
    }
    remotoLog("DLNA acao nao tratada: servico %d %s", servico, acao);
    return dlnaErroSoap(fd, 401, "Invalid Action");
}

// ---- SSDP: anuncia o PS4 na rede e responde às buscas dos apps ----
static void ssdpEnviar(int s, const struct sockaddr_in* destino, const char* tipo, bool resposta, const char* ip) {
    char usn[256];
    if (!strcmp(tipo, dlnaUuid)) snprintf(usn, sizeof(usn), "%s", dlnaUuid);
    else snprintf(usn, sizeof(usn), "%s::%s", dlnaUuid, tipo);
    char msg[1024];
    int n;
    if (resposta) n = snprintf(msg, sizeof(msg), "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=1800\r\nEXT:\r\nLOCATION: http://%s:%d/dlna/desc.xml\r\n"
        "SERVER: PS4/1.0 UPnP/1.0 HyperNeiva/1.0\r\nST: %s\r\nUSN: %s\r\nContent-Length: 0\r\n\r\n", ip, REMOTO_PORTA, tipo, usn);
    else n = snprintf(msg, sizeof(msg), "NOTIFY * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nCACHE-CONTROL: max-age=1800\r\nLOCATION: http://%s:%d/dlna/desc.xml\r\n"
        "NT: %s\r\nNTS: ssdp:alive\r\nSERVER: PS4/1.0 UPnP/1.0 HyperNeiva/1.0\r\nUSN: %s\r\nContent-Length: 0\r\n\r\n", ip, REMOTO_PORTA, tipo, usn);
    sendto(s, msg, n, 0, (const struct sockaddr*)destino, sizeof(*destino));
}

static void ssdpTipos(const char* tipos[6]) {
    tipos[0] = "upnp:rootdevice"; tipos[1] = dlnaUuid; tipos[2] = "urn:schemas-upnp-org:device:MediaRenderer:1";
    tipos[3] = "urn:schemas-upnp-org:service:AVTransport:1"; tipos[4] = "urn:schemas-upnp-org:service:RenderingControl:1";
    tipos[5] = "urn:schemas-upnp-org:service:ConnectionManager:1";
}

static void* remotoThreadSsdp(void*) {
    dlnaPrepararUuid();
    const char* tipos[6]; ssdpTipos(tipos);
    int s = -1;
    for (int tentativa = 0; tentativa < 10 && s < 0; tentativa++) {
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) { remotoDormir(2000); continue; }
        int sim = 1;
        setsockopt(s, SOL_SOCKET, HN_SO_REUSEADDR, (void*)&sim, sizeof(sim));
        setsockopt(s, SOL_SOCKET, HN_SO_REUSEPORT, (void*)&sim, sizeof(sim));
        struct sockaddr_in end; memset(&end, 0, sizeof(end));
        end.sin_family = AF_INET; end.sin_port = htons(1900); end.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (struct sockaddr*)&end, sizeof(end)) != 0) {
            remotoLog("SSDP bind 1900 falhou errno=%d (tentativa %d)", errno, tentativa);
            close(s); s = -1; remotoDormir(3000); continue;
        }
    }
    if (s < 0) {
        // Sem a porta 1900 ainda dá para anunciar: os apps acham pelo NOTIFY.
        s = socket(AF_INET, SOCK_DGRAM, 0);
        remotoLog("SSDP sem porta 1900: so anuncios");
    }
    if (s < 0) return NULL;
    struct ip_mreq grupo; memset(&grupo, 0, sizeof(grupo));
    inet_pton(AF_INET, "239.255.255.250", &grupo.imr_multiaddr);
    grupo.imr_interface.s_addr = htonl(INADDR_ANY);
    int rc = setsockopt(s, IPPROTO_IP, HN_IP_ADD_MEMBERSHIP, (void*)&grupo, sizeof(grupo));
    unsigned char ttl = 4;
    setsockopt(s, IPPROTO_IP, HN_IP_MULTICAST_TTL, (void*)&ttl, sizeof(ttl));
    remotoLog("SSDP multicast rc=%d errno=%d", rc, rc ? errno : 0);
    remotoPrepararSocket(s, 1);
    struct sockaddr_in multicast; memset(&multicast, 0, sizeof(multicast));
    multicast.sin_family = AF_INET; multicast.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &multicast.sin_addr);
    uint64_t proximoAnuncio = 0;
    int anunciosRapidos = 3;
    char buf[2048];
    for (;;) {
        uint64_t agora = remotoAgoraMs();
        char ip[32] = "";
        if (agora >= proximoAnuncio && remotoMeuIp(ip, sizeof(ip))) {
            for (int i = 0; i < 6; i++) ssdpEnviar(s, &multicast, tipos[i], false, ip);
            proximoAnuncio = agora + (anunciosRapidos-- > 0 ? 3000 : 60000);
        }
        struct sockaddr_in de; socklen_t len = sizeof(de);
        ssize_t n = recvfrom(s, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&de, &len);
        if (n <= 0) { if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) remotoDormir(1000); continue; }
        buf[n] = '\0';
        if (strncmp(buf, "M-SEARCH", 8) != 0) continue;
        char st[256] = "";
        remotoCabValor(buf, "ST", st, sizeof(st));
        if (!remotoMeuIp(ip, sizeof(ip))) continue;
        int respondidas = 0;
        for (int i = 0; i < 6; i++) {
            if (!strcmp(st, "ssdp:all") || !strcmp(st, tipos[i])) { ssdpEnviar(s, &de, tipos[i], true, ip); respondidas++; }
        }
        char quem[32] = ""; inet_ntop(AF_INET, &de.sin_addr, quem, sizeof(quem));
        if (respondidas) remotoLog("SSDP M-SEARCH %s de %s: %d respostas", st, quem, respondidas);
    }
    return NULL;
}

// =========================================================================
// Rotas novas: fluxo de vídeo, arquivos do PS4 e DLNA. Devolve true em *tratou se a rota é daqui.
// =========================================================================
static bool remotoRotasTela(RemotoConexao* c, const char* alvo, const char* query, long long tamCorpo, bool* tratou);

static bool remotoRotasExtras(RemotoConexao* c, const char* metodo, const char* alvo, const char* query, const char* cab, long long tamCorpo, bool* tratou) {
    {
        bool tela = false;
        bool r = remotoRotasTela(c, alvo, query, tamCorpo, &tela);
        if (tela) { *tratou = true; return r; }
    }
    *tratou = true;
    char p[1024] = "";
    // ---- Vídeo do navegador tocando enquanto chega ----
    if (!strcmp(alvo, "/fluxo/abrir")) {
        char bruto[512] = "", nome[200], t[32] = "0";
        remotoParam(query, "n", bruto, sizeof(bruto)); remotoNomeSeguro(bruto, nome, sizeof(nome));
        remotoParam(query, "t", t, sizeof(t));
        long long total = strtoll(t, NULL, 10);
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        if (total <= 0) return remotoJson(c->fd, "400 Bad Request", "{\"ok\":false}");
        unsigned id = remotoFonteAbrirNavegador(nome, total);
        char caminho[300]; snprintf(caminho, sizeof(caminho), "remoto://fluxo/%s", nome);
        remotoPedirMidia(caminho);
        remotoLog("FLUXO aberto id=%u %s %lld bytes", id, nome, total);
        char json[64]; snprintf(json, sizeof(json), "{\"ok\":true,\"id\":%u}", id);
        return remotoJson(c->fd, "200 OK", json);
    }
    if (!strcmp(alvo, "/fluxo/pedido")) {
        char t[32] = "0"; remotoParam(query, "id", t, sizeof(t));
        unsigned id = (unsigned)strtoul(t, NULL, 10);
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        long long lista[FONTE_SLOTS];
        int n = 0;
        // Espera até 10 s por um pedido do player (long polling).
        for (int i = 0; i < 1000; i++) {
            n = fontePendentes(id, lista, FONTE_SLOTS);
            if (n != 0) break;
            remotoDormir(10);
        }
        if (n < 0) return remotoJson(c->fd, "200 OK", "{\"fim\":true}");
        char json[1200]; int k = snprintf(json, sizeof(json), "{\"b\":[");
        for (int i = 0; i < n; i++) k += snprintf(json + k, sizeof(json) - k, "%s%lld", i ? "," : "", lista[i]);
        snprintf(json + k, sizeof(json) - k, "],\"tam\":%d}", FONTE_BLOCO);
        return remotoJson(c->fd, "200 OK", json);
    }
    if (!strcmp(alvo, "/fluxo/bloco")) {
        char t[32] = "0", b[32] = "-1";
        remotoParam(query, "id", t, sizeof(t)); remotoParam(query, "b", b, sizeof(b));
        if (tamCorpo < 0 || tamCorpo > FONTE_BLOCO) return false;
        uint8_t* dados = (uint8_t*)malloc(FONTE_BLOCO + 1);
        if (!dados) return false;
        int jaTem = c->len < tamCorpo ? c->len : (int)tamCorpo;
        memcpy(dados, c->buf, jaTem); remotoConsumir(c, jaTem);
        bool ok = jaTem == tamCorpo || remotoReceberN(c->fd, dados + jaTem, (int)tamCorpo - jaTem);
        if (ok) fonteEntregar((unsigned)strtoul(t, NULL, 10), strtoll(b, NULL, 10), dados, (int)tamCorpo);
        free(dados);
        if (!ok) return false;
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }

    // ---- Arquivos do PS4 ----
    if (!strncmp(alvo, "/fs/", 4)) {
        remotoParam(query, "p", p, sizeof(p));
        if (!remotoCaminhoValido(p)) { char lixo[64]; remotoLerCorpo(c, tamCorpo < 60 ? tamCorpo : 0, lixo, sizeof(lixo)); return remotoJson(c->fd, "400 Bad Request", "{\"ok\":false,\"erro\":\"caminho invalido\"}"); }
        if (!strcmp(alvo, "/fs/enviar")) {
            const char* barra = strrchr(p, '/');
            char nome[256]; snprintf(nome, sizeof(nome), "%s", barra ? barra + 1 : p);
            char pasta[1024]; snprintf(pasta, sizeof(pasta), "%.*s", (int)(barra ? barra - p : 0), p);
            if (!pasta[0]) snprintf(pasta, sizeof(pasta), "/");
            char erro[160] = "";
            if (!remotoReceberArquivo(c, tamCorpo, pasta, nome, erro, sizeof(erro))) return false;
            if (erro[0]) { char json[300], e2[200]; remotoJsonTexto(erro, e2, sizeof(e2)); snprintf(json, sizeof(json), "{\"ok\":false,\"erro\":\"%s\"}", e2); return remotoJson(c->fd, "500 Internal Server Error", json); }
            return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
        }
        char lixo[64];
        if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        if (!strcmp(alvo, "/fs/lista")) { remotoListarPasta(c->fd, p); return true; }
        if (!strcmp(alvo, "/fs/baixar")) { char dl[8] = ""; remotoParam(query, "dl", dl, sizeof(dl)); return remotoServirArquivo(c->fd, p, cab, dl[0] == '1'); }
        if (!strcmp(alvo, "/fs/pasta")) {
            bool ok = remotoCriarPastas(p);
            return remotoJson(c->fd, "200 OK", ok ? "{\"ok\":true}" : "{\"ok\":false,\"erro\":\"nao criou\"}");
        }
        if (!strcmp(alvo, "/fs/apagar")) {
            if (strlen(p) < 6) return remotoJson(c->fd, "403 Forbidden", "{\"ok\":false,\"erro\":\"pasta protegida\"}");
            bool ok = remotoApagarTudo(p);
            remotoLog("FS apagar %s -> %d", p, ok ? 1 : 0);
            return remotoJson(c->fd, "200 OK", ok ? "{\"ok\":true}" : "{\"ok\":false,\"erro\":\"nao apagou\"}");
        }
        if (!strcmp(alvo, "/fs/renomear")) {
            char para[1024] = ""; remotoParam(query, "para", para, sizeof(para));
            if (!remotoCaminhoValido(para)) return remotoJson(c->fd, "400 Bad Request", "{\"ok\":false}");
            bool ok = rename(p, para) == 0;
            return remotoJson(c->fd, "200 OK", ok ? "{\"ok\":true}" : "{\"ok\":false,\"erro\":\"nao renomeou\"}");
        }
        if (!strcmp(alvo, "/fs/abrir")) {
            int tipo = remotoTipoArquivo(p);
            if (tipo == 2) return remotoJson(c->fd, "200 OK", "{\"ok\":false,\"erro\":\"tipo nao abre\"}");
            remotoPedirMidia(p);
            return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
        }
        if (!strcmp(alvo, "/fs/info")) {
            struct stat st;
            if (stat(p, &st) != 0) return remotoJson(c->fd, "404 Not Found", "{\"ok\":false}");
            char json[128]; snprintf(json, sizeof(json), "{\"ok\":true,\"d\":%d,\"t\":%lld}", S_ISDIR(st.st_mode) ? 1 : 0, (long long)st.st_size);
            return remotoJson(c->fd, "200 OK", json);
        }
        return remotoJson(c->fd, "404 Not Found", "{\"ok\":false}");
    }

    // ---- DLNA ----
    if (!strncmp(alvo, "/dlna/", 6)) {
        char* corpo = (char*)malloc(65536);
        if (!corpo) return false;
        bool ok = remotoLerCorpo(c, tamCorpo, corpo, 65536);
        if (!ok) { free(corpo); return false; }
        int servico = -1;
        for (int i = 0; i < 3; i++) if (!strncmp(alvo + 6, DLNA_SERVICOS[i][1], strlen(DLNA_SERVICOS[i][1]))) servico = i;
        const char* resto = servico >= 0 ? alvo + 6 + strlen(DLNA_SERVICOS[servico][1]) : "";
        if (!strcmp(alvo, "/dlna/desc.xml")) ok = dlnaDescricao(c->fd);
        else if (servico >= 0 && !strcmp(resto, ".xml")) ok = dlnaScpd(c->fd, servico);
        else if (servico >= 0 && !strcmp(resto, "/evento")) {
            // Assinatura aceita sem mandar eventos: os apps passam a consultar a posição.
            dlnaPrepararUuid();
            char r[300];
            int n = snprintf(r, sizeof(r), "HTTP/1.1 200 OK\r\nSID: %s-%d\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n", dlnaUuid, servico);
            ok = strcmp(metodo, "UNSUBSCRIBE") == 0 ? remotoResponder(c->fd, "200 OK", "text/plain", "", 0) : remotoEnviarTudo(c->fd, r, n);
        }
        else if (servico >= 0 && !resto[0] && !strcmp(metodo, "POST")) {
            char soap[256] = ""; remotoCabValor(cab, "SOAPACTION", soap, sizeof(soap));
            ok = dlnaControle(c->fd, servico, soap, corpo);
        }
        else ok = remotoJson(c->fd, "404 Not Found", "{\"ok\":false}");
        free(corpo);
        return ok;
    }
    *tratou = false;
    return true;
}



// =========================================================================
// IA: Gemini, ChatGPT (OpenAI) e Groq, com várias contas (ex.: "Gemini Trabalho" e "Gemini Lazer").
// Perguntar (com a tela ou um arquivo junto), gerar imagem, transcrever a voz, ler em voz alta
// (inclusive arquivos inteiros: PDF, TXT, DOCX, ODT, EPUB). A chave fica só no PS4.
// O que um provedor não faz (o Groq não desenha nem fala) vai para outra conta que faça.
// =========================================================================
#include "miniz.h"
#ifndef IA_BASE
#define IA_BASE "https://generativelanguage.googleapis.com/v1beta"
#endif
#ifndef IA_BASE_OPENAI
#define IA_BASE_OPENAI "https://api.openai.com/v1"
#endif
#ifndef IA_BASE_GROQ
#define IA_BASE_GROQ "https://api.groq.com/openai/v1"
#endif
#define IA_PASTA REMOTO_RAIZ "/ia"
#define IA_ARQ_CONTAS REMOTO_RAIZ "/configuracao/ia_contas.txt"
#define IA_ARQ_CONFIG REMOTO_RAIZ "/configuracao/ia_config.txt"
#define IA_ARQ_GEMINI_ANTIGO REMOTO_RAIZ "/configuracao/gemini_chave.txt"
#define IA_MAX_MSG 40
#define IA_MAX_CONTAS 16

enum { PROV_GEMINI, PROV_OPENAI, PROV_GROQ, PROV_N };
static const char* IA_PROV_ID[PROV_N] = { "gemini", "openai", "groq" };
static const char* IA_PROV_NOME[PROV_N] = { "Gemini (Google)", "ChatGPT (OpenAI)", "Groq" };
static const char* IA_PROV_CURTO[PROV_N] = { "Gemini", "ChatGPT", "Groq" };
static const char* IA_PROV_MODELOS[PROV_N][5] = {
    { "gemini-3.5-flash", "gemini-3.5-flash-lite", "gemini-3.1-flash-lite", NULL },
    { "gpt-5-mini", "gpt-5", "gpt-5-nano", "gpt-4o-mini", NULL },
    { "llama-3.3-70b-versatile", "openai/gpt-oss-120b", "meta-llama/llama-4-scout-17b-16e-instruct", "llama-3.1-8b-instant", NULL },
};
static const char* IA_PROV_VOZES[PROV_N][10] = {
    { "Kore", "Puck", "Zephyr", "Aoede", "Leda", "Charon", NULL },
    { "alloy", "coral", "nova", "onyx", "sage", "shimmer", "echo", "fable", "ash", NULL },
    { NULL },
};
static const char* IA_PROV_SITE[PROV_N] = { "aistudio.google.com", "platform.openai.com", "console.groq.com" };
static const bool IA_PROV_IMAGEM[PROV_N] = { true, true, false };
static const bool IA_PROV_VOZ[PROV_N] = { true, true, false };
static const char* IA_MODELOS_IMG_GEMINI[] = { "gemini-3.1-flash-image", "gemini-3-pro-image-preview" };
static const char* IA_ASPECTOS[] = { "16:9", "1:1", "9:16", "4:3", "3:4" };
#define IA_N(v) ((int)(sizeof(v) / sizeof(v[0])))
static int iaNModelos(int p) { int n = 0; while (n < 5 && IA_PROV_MODELOS[p][n]) n++; return n; }
static int iaNVozes(int p) { int n = 0; while (n < 10 && IA_PROV_VOZES[p][n]) n++; return n; }

// Plataforma (PS4 ou teste no PC).
static int iaHttpPost(const char* url, const char* cabecalhos, const char* tipo, const char* corpo, size_t tam, char** resp, size_t* tamResp);
static void iaTocarPcm(const int16_t* pcm, int amostras, int taxa);
static bool iaTocandoAgora();
static void iaPararVoz();
static bool iaTelaJpeg(uint8_t** jpeg, int* tam);
struct IaBuf;
static bool iaTextoPdf(const char* caminho, int pagina, IaBuf* saida);

struct IaConta { int prov; char nome[64]; char modelo[96]; char chave[300]; };
struct IaMsg { char quem; char* texto; char img[256]; };
static struct {
    pthread_mutex_t trava;
    volatile bool ocupado, lendo, pararLeitura;
    char tarefa[128];
    char erro[400];
    IaConta contas[IA_MAX_CONTAS];
    int nContas, contaAtiva;
    char voz[PROV_N][24];
    int aspecto, modeloImg;
    bool falar;
    unsigned versao;
    IaMsg msgs[IA_MAX_MSG];
    int nMsgs;
    bool carregado;
} ia = { PTHREAD_MUTEX_INITIALIZER };

// ---- buffer que cresce ----
struct IaBuf { char* d; size_t n, cap; };
static bool iaMais(IaBuf* b, const char* s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nova = b->cap ? b->cap : 4096;
        while (nova < b->n + n + 1) nova *= 2;
        char* x = (char*)realloc(b->d, nova);
        if (!x) return false;
        b->d = x; b->cap = nova;
    }
    if (n) memcpy(b->d + b->n, s, n);
    b->n += n; b->d[b->n] = '\0';
    return true;
}
static bool iaTxt(IaBuf* b, const char* s) { return iaMais(b, s, strlen(s)); }
static bool iaJsonTxt(IaBuf* b, const char* s) {
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        char t[8];
        if (*p == '"' || *p == '\\') { t[0] = '\\'; t[1] = (char)*p; if (!iaMais(b, t, 2)) return false; }
        else if (*p == '\n') { if (!iaMais(b, "\\n", 2)) return false; }
        else if (*p == '\r') continue;
        else if (*p == '\t') { if (!iaMais(b, "\\t", 2)) return false; }
        else if (*p < 32) { snprintf(t, sizeof(t), "\\u%04x", *p); if (!iaTxt(b, t)) return false; }
        else if (!iaMais(b, (const char*)p, 1)) return false;
    }
    return true;
}
static const char IA_B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static bool iaBase64(IaBuf* b, const uint8_t* d, size_t n) {
    char q[4];
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        q[0] = IA_B64[(v >> 18) & 63]; q[1] = IA_B64[(v >> 12) & 63];
        q[2] = i + 1 < n ? IA_B64[(v >> 6) & 63] : '='; q[3] = i + 2 < n ? IA_B64[v & 63] : '=';
        if (!iaMais(b, q, 4)) return false;
    }
    return true;
}
static int iaB64Val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}
static size_t iaDeBase64(const char* in, size_t n, uint8_t* out) {
    size_t o = 0; uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < n; i++) {
        int v = iaB64Val((unsigned char)in[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (uint8_t)(acc >> bits); }
    }
    return o;
}

// ---- JSON (só o necessário) ----
static void iaUtf8(IaBuf* b, uint32_t cp) {
    char t[4]; int n;
    if (cp < 0x80) { t[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { t[0] = (char)(0xC0 | (cp >> 6)); t[1] = (char)(0x80 | (cp & 63)); n = 2; }
    else if (cp < 0x10000) { t[0] = (char)(0xE0 | (cp >> 12)); t[1] = (char)(0x80 | ((cp >> 6) & 63)); t[2] = (char)(0x80 | (cp & 63)); n = 3; }
    else { t[0] = (char)(0xF0 | (cp >> 18)); t[1] = (char)(0x80 | ((cp >> 12) & 63)); t[2] = (char)(0x80 | ((cp >> 6) & 63)); t[3] = (char)(0x80 | (cp & 63)); n = 4; }
    iaMais(b, t, n);
}
static uint32_t iaHex4(const char* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) { int h = remotoHex(p[i]); if (h < 0) return 0xFFFD; v = v * 16 + (uint32_t)h; }
    return v;
}
// p aponta para a aspa de abertura. Devolve a string decodificada (malloc) e onde ela termina.
static char* iaJsonString(const char* p, const char** fim) {
    IaBuf b = { NULL, 0, 0 };
    iaMais(&b, "", 0);
    if (*p != '"') { if (fim) *fim = p; return b.d; }
    p++;
    const char* ini = p;
    while (*p && *p != '"') {
        if (*p != '\\') { p++; continue; }
        if (p > ini) iaMais(&b, ini, p - ini);
        p++;
        char e = *p;
        if (e == 'n') iaMais(&b, "\n", 1);
        else if (e == 't') iaMais(&b, "\t", 1);
        else if (e == 'r' || e == 'b' || e == 'f') {}
        else if (e == 'u') {
            uint32_t cp = iaHex4(p + 1);
            p += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && p[1] == '\\' && p[2] == 'u') {
                uint32_t lo = iaHex4(p + 3);
                if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
            }
            iaUtf8(&b, cp);
        }
        else if (e) iaMais(&b, &e, 1);
        if (*p) p++;
        ini = p;
    }
    if (p > ini) iaMais(&b, ini, p - ini);
    if (fim) *fim = *p ? p + 1 : p;
    return b.d;
}
static const char* iaJsonAchar(const char* ini, const char* lim, const char* chave) {
    char pad[64]; snprintf(pad, sizeof(pad), "\"%s\"", chave);
    size_t lp = strlen(pad);
    for (const char* p = ini; p && p < lim; ) {
        p = strstr(p, pad);
        if (!p || p >= lim) return NULL;
        const char* q = p + lp;
        while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++;
        if (*q == ':') { q++; while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++; return q; }
        p = q;
    }
    return NULL;
}

struct IaResultado { IaBuf texto; uint8_t* dados; size_t tamDados; char mime[96]; char erro[400]; };
static void iaResultadoLimpar(IaResultado* r) { free(r->texto.d); free(r->dados); memset(r, 0, sizeof(*r)); }
static void iaErroGenerico(const char* json, int status, const char* prov, IaResultado* r) {
    const char* fim = json + strlen(json);
    const char* m = iaJsonAchar(json, fim, "message");
    if (m && *m == '"') { char* t = iaJsonString(m, NULL); snprintf(r->erro, sizeof(r->erro), "Erro do %s (HTTP %d): %s", prov, status, t); free(t); }
    else snprintf(r->erro, sizeof(r->erro), "Resposta inesperada do %s (HTTP %d).", prov, status);
}
static bool iaSemResposta(const char* json, int status, const char* prov, IaResultado* r) {
    if (json && json[0]) return false;
    if (status < 0) snprintf(r->erro, sizeof(r->erro), "Sem conexão com o %s (erro 0x%08X). Veja a internet do PS4.", prov, (unsigned)status);
    else snprintf(r->erro, sizeof(r->erro), "O %s respondeu vazio (HTTP %d).", prov, status);
    return true;
}
static void iaGuardarDados(IaResultado* r, const char* d) {
    const char* fim = strchr(d + 1, '"');
    size_t n = fim ? (size_t)(fim - d - 1) : 0;
    r->dados = (uint8_t*)malloc(n / 4 * 3 + 4);
    if (r->dados) r->tamDados = iaDeBase64(d + 1, n, r->dados);
}

// Gemini: candidates[0].content.parts[] (texto em várias partes e/ou inlineData).
static void iaLerGemini(const char* json, int status, IaResultado* r) {
    memset(r, 0, sizeof(*r));
    if (iaSemResposta(json, status, "Gemini", r)) return;
    const char* fimJson = json + strlen(json);
    const char* cand = strstr(json, "\"candidates\"");
    if (!cand) {
        const char* bloq = iaJsonAchar(json, fimJson, "blockReason");
        if (bloq && *bloq == '"') { char* t = iaJsonString(bloq, NULL); snprintf(r->erro, sizeof(r->erro), "O Gemini recusou o pedido (%s).", t); free(t); }
        else iaErroGenerico(json, status, "Gemini", r);
        return;
    }
    const char* lim = strstr(cand, "\"usageMetadata\"");
    if (!lim) lim = fimJson;
    for (const char* p = cand; ; ) {
        const char* v = iaJsonAchar(p, lim, "text");
        if (!v) break;
        const char* fim = v;
        if (*v == '"') { char* t = iaJsonString(v, &fim); if (r->texto.n) iaTxt(&r->texto, "\n"); iaTxt(&r->texto, t); free(t); }
        p = fim > v ? fim : v + 1;
    }
    const char* inl = strstr(cand, "\"inlineData\"");
    if (!inl || inl > lim) inl = strstr(cand, "\"inline_data\"");
    if (inl && inl < lim) {
        const char* mt = iaJsonAchar(inl, lim, "mimeType");
        if (!mt) mt = iaJsonAchar(inl, lim, "mime_type");
        if (mt && *mt == '"') { char* t = iaJsonString(mt, NULL); snprintf(r->mime, sizeof(r->mime), "%s", t); free(t); }
        const char* d = iaJsonAchar(inl, lim, "data");
        if (d && *d == '"') iaGuardarDados(r, d);
    }
    if (!r->texto.n && !r->tamDados) {
        const char* motivo = iaJsonAchar(cand, lim, "finishReason");
        char* t = motivo && *motivo == '"' ? iaJsonString(motivo, NULL) : NULL;
        snprintf(r->erro, sizeof(r->erro), "O Gemini não respondeu%s%s%s.", t ? " (motivo: " : "", t ? t : "", t ? ")" : "");
        free(t);
    }
}

// OpenAI e Groq (mesmo formato): choices[0].message.content, data[0].b64_json ou {"text":...}.
static void iaLerOpenAI(const char* json, int status, const char* prov, IaResultado* r) {
    memset(r, 0, sizeof(*r));
    if (iaSemResposta(json, status, prov, r)) return;
    const char* fimJson = json + strlen(json);
    if (status >= 400 || strstr(json, "\"error\"")) { iaErroGenerico(json, status, prov, r); return; }
    const char* ch = strstr(json, "\"choices\"");
    if (ch) {
        const char* msg = strstr(ch, "\"message\"");
        const char* c = msg ? iaJsonAchar(msg, fimJson, "content") : NULL;
        if (c && *c == '"') { char* t = iaJsonString(c, NULL); iaTxt(&r->texto, t); free(t); }
        if (!r->texto.n) {
            const char* recusa = msg ? iaJsonAchar(msg, fimJson, "refusal") : NULL;
            if (recusa && *recusa == '"') { char* t = iaJsonString(recusa, NULL); snprintf(r->erro, sizeof(r->erro), "O %s recusou: %s", prov, t); free(t); }
            else snprintf(r->erro, sizeof(r->erro), "O %s não respondeu.", prov);
        }
        return;
    }
    const char* b = iaJsonAchar(json, fimJson, "b64_json");
    if (b && *b == '"') { iaGuardarDados(r, b); snprintf(r->mime, sizeof(r->mime), "image/png"); return; }
    const char* t = iaJsonAchar(json, fimJson, "text");
    if (t && *t == '"') { char* s = iaJsonString(t, NULL); iaTxt(&r->texto, s); free(s); return; }
    iaErroGenerico(json, status, prov, r);
}

// ---- contas e configuração ----
static void iaSalvarContasTravado() {
    remotoCriarPastas(REMOTO_RAIZ "/configuracao");
    FILE* f = fopen(IA_ARQ_CONTAS, "w");
    if (!f) return;
    fprintf(f, "# provedor|nome|modelo|chave (Hyper Neiva: IA)\n");
    for (int i = 0; i < ia.nContas; i++) fprintf(f, "%s|%s|%s|%s\n", IA_PROV_ID[ia.contas[i].prov], ia.contas[i].nome, ia.contas[i].modelo, ia.contas[i].chave);
    fclose(f);
}
static void iaSalvarConfigTravado() {
    FILE* f = fopen(IA_ARQ_CONFIG, "w");
    if (!f) return;
    fprintf(f, "conta=%d\naspecto=%d\nfalar=%d\nmodelo_img=%d\n", ia.contaAtiva, ia.aspecto, ia.falar ? 1 : 0, ia.modeloImg);
    for (int p = 0; p < PROV_N; p++) if (ia.voz[p][0]) fprintf(f, "voz_%s=%s\n", IA_PROV_ID[p], ia.voz[p]);
    fclose(f);
}
static void iaLimparTexto(char* s, bool semBarra) {
    char* a = s; while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n' || *a == '"' || *a == '\'') a++;
    size_t n = strlen(a);
    while (n > 0 && (a[n - 1] == ' ' || a[n - 1] == '\t' || a[n - 1] == '\r' || a[n - 1] == '\n' || a[n - 1] == '"' || a[n - 1] == '\'')) n--;
    memmove(s, a, n); s[n] = 0;
    for (char* p = s; *p; p++) if (*p == '\n' || *p == '\r' || (semBarra && *p == '|')) *p = ' ';
}
static int iaProvPorId(const char* id) { for (int p = 0; p < PROV_N; p++) if (!strcasecmp(id, IA_PROV_ID[p])) return p; return -1; }

static void iaCarregarTravado() {
    if (ia.carregado) return;
    ia.carregado = true;
    ia.nContas = 0; ia.contaAtiva = 0;
    FILE* f = fopen(IA_ARQ_CONTAS, "r");
    if (f) {
        char linha[700];
        while (fgets(linha, sizeof(linha), f) && ia.nContas < IA_MAX_CONTAS) {
            if (linha[0] == '#') continue;
            linha[strcspn(linha, "\r\n")] = 0;
            char* campos[4] = { linha, NULL, NULL, NULL };
            for (int i = 1; i < 4; i++) { char* b = campos[i - 1] ? strchr(campos[i - 1], '|') : NULL; if (b) { *b = 0; campos[i] = b + 1; } }
            if (!campos[3]) continue;
            int p = iaProvPorId(campos[0]);
            if (p < 0 || strlen(campos[3]) < 10) continue;
            IaConta* c = &ia.contas[ia.nContas++];
            c->prov = p;
            snprintf(c->nome, sizeof(c->nome), "%s", campos[1][0] ? campos[1] : IA_PROV_CURTO[p]);
            snprintf(c->modelo, sizeof(c->modelo), "%s", campos[2]);
            snprintf(c->chave, sizeof(c->chave), "%s", campos[3]);
        }
        fclose(f);
    }
    else {
        // Primeira vez: a chave do Gemini (versões 3.38 a 3.40) e a do ChatGPT viram contas.
        FILE* g = fopen(IA_ARQ_GEMINI_ANTIGO, "r");
        char chave[300] = "";
        if (g) { if (fgets(chave, sizeof(chave), g)) iaLimparTexto(chave, true); fclose(g); }
        if (strlen(chave) >= 20) { IaConta* c = &ia.contas[ia.nContas++]; c->prov = PROV_GEMINI; snprintf(c->nome, sizeof(c->nome), "Gemini"); c->modelo[0] = 0; snprintf(c->chave, sizeof(c->chave), "%s", chave); }
        FILE* o = fopen(REMOTO_RAIZ "/configuracao/chaves_api.txt", "r");
        if (o) {
            char linha[700];
            while (fgets(linha, sizeof(linha), o)) {
                if (strncmp(linha, "chatgpt=", 8)) continue;
                char* v = linha + 8; iaLimparTexto(v, true);
                if (strlen(v) >= 20 && ia.nContas < IA_MAX_CONTAS) { IaConta* c = &ia.contas[ia.nContas++]; c->prov = PROV_OPENAI; snprintf(c->nome, sizeof(c->nome), "ChatGPT"); c->modelo[0] = 0; snprintf(c->chave, sizeof(c->chave), "%s", v); }
            }
            fclose(o);
        }
        if (ia.nContas) { iaSalvarContasTravado(); remove(IA_ARQ_GEMINI_ANTIGO); remove(REMOTO_RAIZ "/configuracao/gemini_config.txt"); remotoLog("IA: %d conta(s) criada(s) a partir das chaves antigas", ia.nContas); }
    }
    FILE* c = fopen(IA_ARQ_CONFIG, "r");
    if (c) {
        char linha[200];
        while (fgets(linha, sizeof(linha), c)) {
            linha[strcspn(linha, "\r\n")] = 0;
            char* igual = strchr(linha, '='); if (!igual) continue;
            *igual = 0; const char* v = igual + 1;
            if (!strcmp(linha, "conta")) ia.contaAtiva = atoi(v);
            else if (!strcmp(linha, "aspecto")) ia.aspecto = atoi(v);
            else if (!strcmp(linha, "falar")) ia.falar = atoi(v) != 0;
            else if (!strcmp(linha, "modelo_img")) ia.modeloImg = atoi(v);
            else if (!strncmp(linha, "voz_", 4)) { int p = iaProvPorId(linha + 4); if (p >= 0) snprintf(ia.voz[p], sizeof(ia.voz[p]), "%s", v); }
        }
        fclose(c);
    }
    if (ia.contaAtiva < 0 || ia.contaAtiva >= ia.nContas) ia.contaAtiva = 0;
    if (ia.aspecto < 0 || ia.aspecto >= IA_N(IA_ASPECTOS)) ia.aspecto = 0;
    if (ia.modeloImg < 0 || ia.modeloImg >= IA_N(IA_MODELOS_IMG_GEMINI)) ia.modeloImg = 0;
    for (int p = 0; p < PROV_N; p++) if (!ia.voz[p][0] && iaNVozes(p)) snprintf(ia.voz[p], sizeof(ia.voz[p]), "%s", IA_PROV_VOZES[p][0]);
}
void iaCarregar() { pthread_mutex_lock(&ia.trava); iaCarregarTravado(); pthread_mutex_unlock(&ia.trava); }

static const char* iaModeloDe(const IaConta* c) { return c->modelo[0] ? c->modelo : IA_PROV_MODELOS[c->prov][0]; }

// Conta para uma tarefa: a escolhida, ou a primeira que saiba fazer (o Groq não desenha nem fala).
enum { CAP_TEXTO, CAP_IMAGEM, CAP_VOZ };
static bool iaContaPara(int cap, IaConta* saida, bool* outra) {
    pthread_mutex_lock(&ia.trava);
    iaCarregarTravado();
    int achada = -1;
    for (int passo = 0; passo < 2 && achada < 0; passo++)
        for (int i = 0; i < ia.nContas && achada < 0; i++) {
            int k = passo == 0 ? ia.contaAtiva : i;
            if (k >= ia.nContas) continue;
            int p = ia.contas[k].prov;
            bool sabe = cap == CAP_TEXTO || (cap == CAP_IMAGEM && IA_PROV_IMAGEM[p]) || (cap == CAP_VOZ && IA_PROV_VOZ[p]);
            if (sabe) achada = k;
            if (passo == 0) break;
        }
    if (achada >= 0) { *saida = ia.contas[achada]; if (outra) *outra = achada != ia.contaAtiva; }
    pthread_mutex_unlock(&ia.trava);
    return achada >= 0;
}

// ---- conversa e estado ----
static void iaMsgAdicionar(char quem, const char* texto, const char* img) {
    pthread_mutex_lock(&ia.trava);
    if (ia.nMsgs == IA_MAX_MSG) { free(ia.msgs[0].texto); memmove(ia.msgs, ia.msgs + 1, sizeof(IaMsg) * (IA_MAX_MSG - 1)); ia.nMsgs--; }
    IaMsg* m = &ia.msgs[ia.nMsgs++];
    m->quem = quem;
    m->texto = strdup(texto ? texto : "");
    snprintf(m->img, sizeof(m->img), "%s", img ? img : "");
    ia.versao++;
    pthread_mutex_unlock(&ia.trava);
}
void iaLimparConversa() {
    pthread_mutex_lock(&ia.trava);
    for (int i = 0; i < ia.nMsgs; i++) free(ia.msgs[i].texto);
    ia.nMsgs = 0; ia.erro[0] = 0; ia.versao++;
    pthread_mutex_unlock(&ia.trava);
}
static void iaDefinirTarefa(const char* t) { pthread_mutex_lock(&ia.trava); snprintf(ia.tarefa, sizeof(ia.tarefa), "%s", t); ia.versao++; pthread_mutex_unlock(&ia.trava); }
static void iaDefinirErro(const char* e) { pthread_mutex_lock(&ia.trava); snprintf(ia.erro, sizeof(ia.erro), "%s", e); ia.versao++; pthread_mutex_unlock(&ia.trava); remotoLog("IA erro: %s", e); }

static int iaPostar(const char* url, const char* cab, const char* tipo, IaBuf* corpo, char** resp, size_t* tam) {
    uint64_t ini = remotoAgoraMs();
    int status = iaHttpPost(url, cab, tipo, corpo->d, corpo->n, resp, tam);
    const char* q = strstr(url, "://"); const char* caminho = q ? strchr(q + 3, '/') : url;
    remotoLog("IA %s HTTP %d, envio %u bytes, resposta %u bytes, %llu ms", caminho ? caminho : url, status, (unsigned)corpo->n, (unsigned)*tam, (unsigned long long)(remotoAgoraMs() - ini));
    return status;
}
static void iaCabecalho(const IaConta* c, char* cab, int tam) {
    if (c->prov == PROV_GEMINI) snprintf(cab, tam, "x-goog-api-key: %s\n", c->chave);
    else snprintf(cab, tam, "Authorization: Bearer %s\n", c->chave);
}
static const char* iaBase(int prov) { return prov == PROV_OPENAI ? IA_BASE_OPENAI : IA_BASE_GROQ; }

static const char* IA_SISTEMA = "Você é a assistente do Hyper Neiva, um app para PS4 com mídia, jogos, emuladores, Telegram e downloads. "
    "Responda em português do Brasil, direto e curto (até 150 palavras, sem tabelas). O texto aparece na TV, então evite markdown.";

// Pergunta com a conversa anterior; jpeg (opcional) vai junto.
static void iaChat(const IaConta* c, const char* pergunta, const uint8_t* jpeg, int tamJpeg, IaResultado* r) {
    char cab[400]; iaCabecalho(c, cab, sizeof(cab));
    IaBuf b = { NULL, 0, 0 };
    pthread_mutex_lock(&ia.trava);
    int inicio = ia.nMsgs - 1 - 12; if (inicio < 0) inicio = 0;
    int fimHist = ia.nMsgs - 1;   // a última é a própria pergunta
    if (c->prov == PROV_GEMINI) {
        iaTxt(&b, "{\"systemInstruction\":{\"parts\":[{\"text\":\""); iaJsonTxt(&b, IA_SISTEMA); iaTxt(&b, "\"}]},\"contents\":[");
        for (int i = inicio; i < fimHist; i++) {
            iaTxt(&b, ia.msgs[i].quem == 'u' ? "{\"role\":\"user\",\"parts\":[{\"text\":\"" : "{\"role\":\"model\",\"parts\":[{\"text\":\"");
            iaJsonTxt(&b, ia.msgs[i].texto); iaTxt(&b, "\"}]},");
        }
    }
    else {
        iaTxt(&b, "{\"model\":\""); iaJsonTxt(&b, iaModeloDe(c)); iaTxt(&b, "\",\"messages\":[{\"role\":\"system\",\"content\":\""); iaJsonTxt(&b, IA_SISTEMA); iaTxt(&b, "\"},");
        for (int i = inicio; i < fimHist; i++) {
            iaTxt(&b, ia.msgs[i].quem == 'u' ? "{\"role\":\"user\",\"content\":\"" : "{\"role\":\"assistant\",\"content\":\"");
            iaJsonTxt(&b, ia.msgs[i].texto); iaTxt(&b, "\"},");
        }
    }
    pthread_mutex_unlock(&ia.trava);
    if (c->prov == PROV_GEMINI) {
        iaTxt(&b, "{\"role\":\"user\",\"parts\":[");
        if (jpeg) { iaTxt(&b, "{\"inline_data\":{\"mime_type\":\"image/jpeg\",\"data\":\""); iaBase64(&b, jpeg, tamJpeg); iaTxt(&b, "\"}},"); }
        iaTxt(&b, "{\"text\":\""); iaJsonTxt(&b, pergunta); iaTxt(&b, "\"}]}]}");
        char url[300]; snprintf(url, sizeof(url), "%s/models/%s:generateContent", IA_BASE, iaModeloDe(c));
        char* resp = NULL; size_t tam = 0;
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        iaLerGemini(resp, st, r); free(resp);
    }
    else {
        if (jpeg) {
            iaTxt(&b, "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\""); iaJsonTxt(&b, pergunta);
            iaTxt(&b, "\"},{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/jpeg;base64,"); iaBase64(&b, jpeg, tamJpeg); iaTxt(&b, "\"}}]}]}");
        }
        else { iaTxt(&b, "{\"role\":\"user\",\"content\":\""); iaJsonTxt(&b, pergunta); iaTxt(&b, "\"}]}"); }
        char url[300]; snprintf(url, sizeof(url), "%s/chat/completions", iaBase(c->prov));
        char* resp = NULL; size_t tam = 0;
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        iaLerOpenAI(resp, st, IA_PROV_CURTO[c->prov], r);
        if (r->erro[0] && jpeg && c->prov == PROV_GROQ) snprintf(r->erro + strlen(r->erro), sizeof(r->erro) - strlen(r->erro), " (para imagens no Groq use o modelo llama-4-scout)");
        free(resp);
    }
    free(b.d);
}

static void iaImagem(const IaConta* c, const char* prompt, IaResultado* r) {
    char cab[400]; iaCabecalho(c, cab, sizeof(cab));
    IaBuf b = { NULL, 0, 0 };
    char* resp = NULL; size_t tam = 0; char url[300];
    if (c->prov == PROV_GEMINI) {
        iaTxt(&b, "{\"contents\":[{\"parts\":[{\"text\":\""); iaJsonTxt(&b, prompt);
        iaTxt(&b, "\"}]}],\"generationConfig\":{\"responseModalities\":[\"TEXT\",\"IMAGE\"],\"imageConfig\":{\"aspectRatio\":\""); iaTxt(&b, IA_ASPECTOS[ia.aspecto]); iaTxt(&b, "\"}}}");
        snprintf(url, sizeof(url), "%s/models/%s:generateContent", IA_BASE, IA_MODELOS_IMG_GEMINI[ia.modeloImg]);
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        iaLerGemini(resp, st, r);
    }
    else {
        const char* a = IA_ASPECTOS[ia.aspecto];
        const char* tamanho = (!strcmp(a, "16:9") || !strcmp(a, "4:3")) ? "1536x1024" : (!strcmp(a, "1:1") ? "1024x1024" : "1024x1536");
        iaTxt(&b, "{\"model\":\"gpt-image-1\",\"n\":1,\"size\":\""); iaTxt(&b, tamanho); iaTxt(&b, "\",\"prompt\":\""); iaJsonTxt(&b, prompt); iaTxt(&b, "\"}");
        snprintf(url, sizeof(url), "%s/images/generations", iaBase(c->prov));
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        iaLerOpenAI(resp, st, IA_PROV_CURTO[c->prov], r);
    }
    free(resp); free(b.d);
}

// Texto -> PCM 16 bits mono. Devolve a taxa (0 = falhou, erro em r).
static int iaVoz(const IaConta* c, const char* texto, IaResultado* r) {
    char cab[400]; iaCabecalho(c, cab, sizeof(cab));
    IaBuf b = { NULL, 0, 0 };
    char* resp = NULL; size_t tam = 0; char url[300];
    int taxa = 0;
    char voz[24];
    pthread_mutex_lock(&ia.trava); snprintf(voz, sizeof(voz), "%s", ia.voz[c->prov]); pthread_mutex_unlock(&ia.trava);
    if (c->prov == PROV_GEMINI) {
        iaTxt(&b, "{\"contents\":[{\"parts\":[{\"text\":\""); iaJsonTxt(&b, texto);
        iaTxt(&b, "\"}]}],\"generationConfig\":{\"responseModalities\":[\"AUDIO\"],\"speechConfig\":{\"voiceConfig\":{\"prebuiltVoiceConfig\":{\"voiceName\":\""); iaTxt(&b, voz); iaTxt(&b, "\"}}}}}");
        snprintf(url, sizeof(url), "%s/models/gemini-3.1-flash-tts-preview:generateContent", IA_BASE);
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        iaLerGemini(resp, st, r);
        if (r->tamDados && !strncmp(r->mime, "audio/", 6)) {
            taxa = 24000;
            const char* t = strstr(r->mime, "rate="); if (t) taxa = atoi(t + 5);
            if (taxa < 8000 || taxa > 48000) taxa = 24000;
        }
        else if (!r->erro[0]) snprintf(r->erro, sizeof(r->erro), "O Gemini não devolveu áudio.");
    }
    else {
        // OpenAI: "pcm" = 24 kHz, 16 bits, mono, sem cabeçalho.
        iaTxt(&b, "{\"model\":\"gpt-4o-mini-tts\",\"response_format\":\"pcm\",\"voice\":\""); iaTxt(&b, voz); iaTxt(&b, "\",\"input\":\""); iaJsonTxt(&b, texto); iaTxt(&b, "\"}");
        snprintf(url, sizeof(url), "%s/audio/speech", iaBase(c->prov));
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        memset(r, 0, sizeof(*r));
        if (st == 200 && tam > 100 && resp && resp[0] != '{') { r->dados = (uint8_t*)resp; r->tamDados = tam; resp = NULL; taxa = 24000; }
        else if (resp) iaErroGenerico(resp, st, IA_PROV_CURTO[c->prov], r);
        else snprintf(r->erro, sizeof(r->erro), "Sem conexão com o %s.", IA_PROV_CURTO[c->prov]);
    }
    free(resp); free(b.d);
    return taxa;
}

// =========================================================================
// TRANSCREVER UM ARQUIVO DE ÁUDIO (Explorar > X > Transcrever): o texto vai para um .txt ao lado.
// =========================================================================
static const char* iaMimeAudio(const char* caminho) {
    const char* e = strrchr(caminho, '.');
    if (!e) return NULL;
    static const char* tab[][2] = { { ".mp3", "audio/mpeg" }, { ".wav", "audio/wav" }, { ".m4a", "audio/mp4" }, { ".aac", "audio/aac" }, { ".ogg", "audio/ogg" },
        { ".oga", "audio/ogg" }, { ".opus", "audio/ogg" }, { ".flac", "audio/flac" }, { ".webm", "audio/webm" }, { ".mp4", "video/mp4" }, { NULL, NULL } };
    for (int i = 0; tab[i][0]; i++) if (!strcasecmp(e, tab[i][0])) return tab[i][1];
    return NULL;
}
bool iaArquivoTranscrevivel(const char* caminho) { return iaMimeAudio(caminho) != NULL; }

// Áudio -> texto. mic = gravação do microfone (português, curta); senão arquivo (idioma automático).
static void iaTranscreverDados(const IaConta* c, const uint8_t* dados, size_t n, const char* mime, const char* nome, bool mic, IaResultado* r) {
    char cab[400]; iaCabecalho(c, cab, sizeof(cab));
    IaBuf b = { NULL, 0, 0 };
    char* resp = NULL; size_t tam = 0; char url[300];
    if (c->prov == PROV_GEMINI) {
        iaTxt(&b, "{\"contents\":[{\"parts\":[{\"inline_data\":{\"mime_type\":\""); iaTxt(&b, mime); iaTxt(&b, "\",\"data\":\""); iaBase64(&b, dados, n);
        iaTxt(&b, mic ? "\"}},{\"text\":\"Transcreva exatamente o que foi dito, em português do Brasil. Responda só com a transcrição, sem comentários.\"}]}]}"
                      : "\"}},{\"text\":\"Transcreva todo o áudio, palavra por palavra, no idioma em que foi falado. Separe em parágrafos quando mudar o assunto ou quem fala. Responda só com a transcrição.\"}]}]}");
        snprintf(url, sizeof(url), "%s/models/%s:generateContent", IA_BASE, iaModeloDe(c));
        int st = iaPostar(url, cab, "application/json", &b, &resp, &tam);
        iaLerGemini(resp, st, r);
    }
    else {
        const char* lim = "----HyperNeivaAudio7f3a";
        const char* modelo = c->prov == PROV_GROQ ? "whisper-large-v3-turbo" : "gpt-4o-mini-transcribe";
        char t[800];
        int k = snprintf(t, sizeof(t), "--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n%s\r\n", lim, modelo);
        if (mic) k += snprintf(t + k, sizeof(t) - k, "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\npt\r\n", lim);
        snprintf(t + k, sizeof(t) - k, "--%s\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
            lim, lim, nome, mime);
        iaTxt(&b, t); iaMais(&b, (const char*)dados, n);
        snprintf(t, sizeof(t), "\r\n--%s--\r\n", lim); iaTxt(&b, t);
        char tipo[100]; snprintf(tipo, sizeof(tipo), "multipart/form-data; boundary=%s", lim);
        snprintf(url, sizeof(url), "%s/audio/transcriptions", iaBase(c->prov));
        int st = iaPostar(url, cab, tipo, &b, &resp, &tam);
        iaLerOpenAI(resp, st, IA_PROV_CURTO[c->prov], r);
    }
    free(resp); free(b.d);
}

static void* iaThreadTranscreverArquivo(void* arg) {
    char* caminho = (char*)arg;
    const char* nome = strrchr(caminho, '/'); nome = nome ? nome + 1 : caminho;
    const char* mime = iaMimeAudio(caminho);
    IaConta c;
    char aviso[200];
    if (!iaContaPara(CAP_TEXTO, &c, NULL)) { iaDefinirErro("Nenhuma conta de IA. Cadastre em Chaves de API > Inteligência Artificial."); remotoAviso("TRANSCREVER: CADASTRE UMA CONTA DE IA"); goto fim; }
    {
        snprintf(aviso, sizeof(aviso), "Transcrevendo %s com %s...", nome, c.nome);
        iaDefinirTarefa(aviso);
        remotoAviso("TRANSCREVENDO COM A IA... (AVISO QUANDO TERMINAR)");
        FILE* f = fopen(caminho, "rb");
        IaBuf d = { NULL, 0, 0 };
        char* t = (char*)malloc(65536);
        if (f && t) { size_t n; while ((n = fread(t, 1, 65536, f)) > 0 && d.n <= 20u * 1024 * 1024) iaMais(&d, t, n); }
        if (f) fclose(f);
        free(t);
        if (!d.n) { iaDefinirErro("Não abri o áudio."); remotoAviso("TRANSCREVER: NAO ABRI O AUDIO"); free(d.d); goto fim; }
        // Gemini aceita até ~20 MB junto do pedido; OpenAI e Groq, 25 MB por arquivo.
        if (d.n > 20u * 1024 * 1024) { iaDefinirErro("Áudio grande demais para transcrever (máximo 20 MB)."); remotoAviso("TRANSCREVER: AUDIO MAIOR QUE 20 MB"); free(d.d); goto fim; }
        IaResultado r;
        iaTranscreverDados(&c, (const uint8_t*)d.d, d.n, mime, nome, false, &r);
        free(d.d);
        if (!r.texto.n) { iaDefinirErro(r.erro[0] ? r.erro : "A IA não devolveu texto."); remotoAviso("TRANSCREVER: FALHOU (VEJA A TELA DA IA)"); iaResultadoLimpar(&r); goto fim; }
        // nome.mp3 -> nome.txt; se já existir, nome (transcrição).txt.
        char destino[600];
        snprintf(destino, sizeof(destino), "%s", caminho);
        char* ponto = strrchr(destino, '.'); char* barra = strrchr(destino, '/');
        if (ponto && (!barra || ponto > barra)) *ponto = 0;
        size_t base = strlen(destino);
        snprintf(destino + base, sizeof(destino) - base, ".txt");
        FILE* existe = fopen(destino, "rb");
        if (existe) { fclose(existe); snprintf(destino + base, sizeof(destino) - base, " (transcrição).txt"); }
        FILE* o = fopen(destino, "wb");
        if (o) { fwrite(r.texto.d, 1, r.texto.n, o); fputc('\n', o); fclose(o); }
        const char* nomeTxt = strrchr(destino, '/'); nomeTxt = nomeTxt ? nomeTxt + 1 : destino;
        if (!o) { iaDefinirErro("Não consegui gravar o .txt (pasta só de leitura?)."); remotoAviso("TRANSCREVER: NAO GRAVEI O TXT"); }
        else {
            char m[400]; snprintf(m, sizeof(m), "Transcrição de %s salva em %s.", nome, nomeTxt);
            iaMsgAdicionar('m', m, "");
            snprintf(aviso, sizeof(aviso), "TRANSCRICAO SALVA: %s", nomeTxt);
            remotoAviso(aviso);
            remotoLog("IA: transcrição de %s -> %s (%u letras)", caminho, destino, (unsigned)r.texto.n);
        }
        iaResultadoLimpar(&r);
    }
fim:
    free(caminho);
    pthread_mutex_lock(&ia.trava); ia.tarefa[0] = 0; ia.ocupado = false; ia.versao++; pthread_mutex_unlock(&ia.trava);
    return NULL;
}
bool iaTranscreverArquivo(const char* caminho) {
    pthread_mutex_lock(&ia.trava);
    iaCarregarTravado();
    if (ia.ocupado) { pthread_mutex_unlock(&ia.trava); remotoAviso("A IA AINDA ESTA TRABALHANDO"); return false; }
    ia.ocupado = true; ia.erro[0] = 0; ia.versao++;
    pthread_mutex_unlock(&ia.trava);
    char* c = strdup(caminho);
    remotoLog("IA: transcrever %s", caminho);
    if (c && remotoCriarThread(iaThreadTranscreverArquivo, c)) return true;
    free(c);
    pthread_mutex_lock(&ia.trava); ia.ocupado = false; pthread_mutex_unlock(&ia.trava);
    return false;
}

// =========================================================================
// CAPAS DAS MÚSICAS: a capa que vem dentro do MP3 (ID3) ou, sem ela, uma busca no iTunes e
// no Deezer (as duas sem chave) pelo artista e título das tags ou pelo nome do arquivo.
// Ficam em midia/imagens/Musicas/<nome>.jpg.
// =========================================================================
#ifndef CAPA_ITUNES
#define CAPA_ITUNES "https://itunes.apple.com"
#endif
#ifndef CAPA_DEEZER
#define CAPA_DEEZER "https://api.deezer.com"
#endif
#define CAPA_PASTA REMOTO_RAIZ "/midia/imagens/Musicas"
static int iaHttpGet(const char* url, char** resp, size_t* tam);   // plataforma

struct Id3 { char titulo[200], artista[200]; uint8_t* imagem; size_t tamImagem; char* letra; };
// Texto de um quadro ID3 (primeiro byte = codificação) em UTF-8, com as quebras de linha.
static void id3Decodificar(const uint8_t* d, size_t n, IaBuf* saida);
static void id3Texto(const uint8_t* d, size_t n, char* saida, int tam) {
    saida[0] = 0;
    if (n < 2) return;
    IaBuf b = { NULL, 0, 0 };
    id3Decodificar(d, n, &b);
    snprintf(saida, tam, "%s", b.d ? b.d : ""); free(b.d);
    iaLimparTexto(saida, false);
}
static void id3Decodificar(const uint8_t* d, size_t n, IaBuf* saida) {
    IaBuf& b = *saida;
    iaMais(&b, "", 0);
    if (n < 1) return;
    int enc = d[0]; d++; n--;
    if (enc == 0) for (size_t i = 0; i < n && d[i]; i++) iaUtf8(&b, d[i]);
    else if (enc == 3) { size_t k = 0; while (k < n && d[k]) k++; iaMais(&b, (const char*)d, k); }
    else {
        bool be = enc == 2;
        size_t i = 0;
        if (enc == 1 && n >= 2) { if (d[0] == 0xFE && d[1] == 0xFF) be = true; i = 2; }
        for (; i + 1 < n; i += 2) {
            uint32_t u = be ? (d[i] << 8 | d[i + 1]) : (d[i + 1] << 8 | d[i]);
            if (!u) break;
            if (u >= 0xD800 && u < 0xDC00 && i + 3 < n) { uint32_t lo = be ? (d[i + 2] << 8 | d[i + 3]) : (d[i + 3] << 8 | d[i + 2]); u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
            iaUtf8(&b, u);
        }
    }
}
// Lê título, artista e a imagem (APIC/PIC) da etiqueta ID3v2 do começo do MP3.
static bool id3Ler(const char* caminho, Id3* t) {
    memset(t, 0, sizeof(*t));
    FILE* f = fopen(caminho, "rb");
    if (!f) return false;
    uint8_t h[10];
    bool ok = fread(h, 1, 10, f) == 10 && !memcmp(h, "ID3", 3);
    int ver = h[3];
    size_t tam = ok ? ((size_t)(h[6] & 127) << 21 | (size_t)(h[7] & 127) << 14 | (size_t)(h[8] & 127) << 7 | (h[9] & 127)) : 0;
    if (!ok || tam > 8u * 1024 * 1024 || ver < 2 || ver > 4) { fclose(f); return false; }
    uint8_t* d = (uint8_t*)malloc(tam);
    if (!d || fread(d, 1, tam, f) != tam) { free(d); fclose(f); return false; }
    fclose(f);
    size_t p = 0;
    if ((h[5] & 0x40) && ver >= 3 && tam > 4) { size_t ext = ver == 4 ? ((size_t)(d[0] & 127) << 21 | (d[1] & 127) << 14 | (d[2] & 127) << 7 | (d[3] & 127)) : ((size_t)d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3]) + 4; p = ext; }
    int lid = ver == 2 ? 3 : 4, lcab = ver == 2 ? 6 : 10;
    while (p + lcab <= tam && d[p]) {
        char id[5] = { 0 }; memcpy(id, d + p, lid);
        size_t n;
        if (ver == 2) n = (size_t)d[p + 3] << 16 | d[p + 4] << 8 | d[p + 5];
        else if (ver == 4) n = (size_t)(d[p + 4] & 127) << 21 | (d[p + 5] & 127) << 14 | (d[p + 6] & 127) << 7 | (d[p + 7] & 127);
        else n = (size_t)d[p + 4] << 24 | d[p + 5] << 16 | d[p + 6] << 8 | d[p + 7];
        const uint8_t* c = d + p + lcab;
        if (n == 0 || p + lcab + n > tam) break;
        if (!strcmp(id, "TIT2") || !strcmp(id, "TT2")) id3Texto(c, n, t->titulo, sizeof(t->titulo));
        else if (!strcmp(id, "TPE1") || !strcmp(id, "TP1")) id3Texto(c, n, t->artista, sizeof(t->artista));
        else if ((!strcmp(id, "USLT") || !strcmp(id, "ULT")) && !t->letra && n > 5) {
            // codificação, idioma (3), descrição terminada em zero e a letra.
            int enc = c[0];
            size_t i = 4;
            if (enc == 1 || enc == 2) { while (i + 1 < n && (c[i] || c[i + 1])) i += 2; i += 2; }
            else { while (i < n && c[i]) i++; i++; }
            if (i < n) {
                uint8_t* tmp = (uint8_t*)malloc(n - i + 1);
                if (tmp) {
                    tmp[0] = (uint8_t)enc; memcpy(tmp + 1, c + i, n - i);
                    IaBuf b = { NULL, 0, 0 };
                    id3Decodificar(tmp, n - i + 1, &b);
                    free(tmp);
                    if (b.d && b.d[0]) t->letra = b.d; else free(b.d);
                }
            }
        }
        else if ((!strcmp(id, "APIC") || !strcmp(id, "PIC")) && !t->imagem && n > 16) {
            size_t i = 1;
            if (ver == 2) i += 3; else { while (i < n && c[i]) i++; i++; }   // formato (PIC) ou tipo MIME (APIC)
            i++;                                                             // tipo da imagem
            if (c[0] == 1 || c[0] == 2) { while (i + 1 < n && (c[i] || c[i + 1])) i += 2; i += 2; }
            else { while (i < n && c[i]) i++; i++; }                         // descrição
            if (i < n) { t->tamImagem = n - i; t->imagem = (uint8_t*)malloc(t->tamImagem); if (t->imagem) memcpy(t->imagem, c + i, t->tamImagem); }
        }
        p += lcab + n;
    }
    free(d);
    return t->titulo[0] || t->artista[0] || t->imagem || t->letra;
}

// "01 - Artista - Música (Official Video) [320kbps].mp3" -> "Artista Música".
static void capaTermoDoArquivo(const char* nomeArquivo, char* saida, int tam) {
    char s[300]; snprintf(s, sizeof(s), "%s", nomeArquivo);
    char* ponto = strrchr(s, '.'); if (ponto) *ponto = 0;
    char limpo[300]; int n = 0, nivel = 0;
    for (char* p = s; *p && n < 290; p++) {
        if (*p == '(' || *p == '[' || *p == '{') { nivel++; continue; }
        if (*p == ')' || *p == ']' || *p == '}') { if (nivel) nivel--; continue; }
        if (nivel) continue;
        limpo[n++] = (*p == '_' || *p == '-' || *p == '.') ? ' ' : *p;
    }
    limpo[n] = 0;
    char* q = limpo; while (*q == ' ' || isdigit((unsigned char)*q)) q++;   // número da faixa
    if (!*q) q = limpo;
    // Espaços repetidos.
    int o = 0; bool espaco = false;
    for (; *q && o < tam - 1; q++) { if (*q == ' ') { if (!espaco && o) saida[o++] = ' '; espaco = true; } else { saida[o++] = *q; espaco = false; } }
    while (o > 0 && saida[o - 1] == ' ') o--;
    saida[o] = 0;
}
static void capaUrlCodificar(const char* s, char* saida, int tam) {
    int o = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p && o < tam - 4; p++) {
        if (isalnum(*p) || strchr("-_.~", *p)) saida[o++] = (char)*p;
        else if (*p == ' ') saida[o++] = '+';
        else o += snprintf(saida + o, tam - o, "%%%02X", *p);
    }
    saida[o] = 0;
}
static void capaCaminhoCache(const char* nomeArquivo, char* saida, int tam) {
    char base[260]; snprintf(base, sizeof(base), "%s", nomeArquivo);
    char* ponto = strrchr(base, '.'); if (ponto) *ponto = 0;
    snprintf(saida, tam, CAPA_PASTA "/%s.jpg", base);
}

// Procura a capa na internet. Devolve a imagem baixada (malloc) ou NULL.
static uint8_t* capaBuscarOnline(const char* termo, size_t* tamImg, char* fonte, int tamFonte) {
    char q[600]; capaUrlCodificar(termo, q, sizeof(q));
    char url[900];
    char* resp = NULL; size_t tam = 0;
    char imagemUrl[600] = "";
    // iTunes: artworkUrl100 (troca 100x100 por 600x600).
    snprintf(url, sizeof(url), CAPA_ITUNES "/search?media=music&entity=song&limit=1&term=%s", q);
    int st = iaHttpGet(url, &resp, &tam);
    if (st == 200 && resp) {
        const char* v = iaJsonAchar(resp, resp + tam, "artworkUrl100");
        if (v && *v == '"') {
            char* u = iaJsonString(v, NULL);
            char* x = strstr(u, "100x100");
            if (x) { char tmp[600]; snprintf(tmp, sizeof(tmp), "%.*s600x600%s", (int)(x - u), u, x + 7); snprintf(imagemUrl, sizeof(imagemUrl), "%s", tmp); }
            else snprintf(imagemUrl, sizeof(imagemUrl), "%s", u);
            free(u);
            snprintf(fonte, tamFonte, "iTunes");
        }
    }
    free(resp); resp = NULL;
    // Deezer: cover_xl (1000x1000) ou cover_big.
    if (!imagemUrl[0]) {
        snprintf(url, sizeof(url), CAPA_DEEZER "/search?limit=1&q=%s", q);
        st = iaHttpGet(url, &resp, &tam);
        if (st == 200 && resp) {
            const char* v = iaJsonAchar(resp, resp + tam, "cover_xl");
            if (!v || *v != '"') v = iaJsonAchar(resp, resp + tam, "cover_big");
            if (v && *v == '"') { char* u = iaJsonString(v, NULL); snprintf(imagemUrl, sizeof(imagemUrl), "%s", u); free(u); snprintf(fonte, tamFonte, "Deezer"); }
        }
        free(resp); resp = NULL;
    }
    if (!imagemUrl[0]) return NULL;
    st = iaHttpGet(imagemUrl, &resp, &tam);
    if (st != 200 || tam < 500) { free(resp); return NULL; }
    *tamImg = tam;
    return (uint8_t*)resp;
}

// Fila de um item só: vale o último pedido (a música em que o usuário parou).
static pthread_mutex_t capaTrava = PTHREAD_MUTEX_INITIALIZER;
// Fila curta: o último pedido sai primeiro (é a música em que o usuário parou).
#define CAPA_FILA 16
static char capaFila[CAPA_FILA][512];
static int capaFilaN = 0;
static volatile bool capaThreadViva = false;
volatile bool musicaCapaNova = false;
volatile unsigned musicaCapaVersao = 0;   // os temas da biblioteca recarregam as capas quando muda
static char capaTentadas[64][128];
static int capaNTentadas = 0;

static bool capaJaTentou(const char* nome) {
    for (int i = 0; i < capaNTentadas && i < 64; i++) if (!strcmp(capaTentadas[i], nome)) return true;
    return false;
}
// =========================================================================
// CAPAS DE FILMES E SÉRIES: TMDB, a mesma busca do Hyper Neiva de PC, com a chave "tmdb"
// de Chaves de API. Ficam em midia/imagens/Videos/<nome>.jpg. Pedidos entram na fila das
// capas de música com o prefixo "V|".
// =========================================================================
#ifndef TMDB_API
#define TMDB_API "https://api.themoviedb.org/3"
#endif
#ifndef TMDB_IMG
#define TMDB_IMG "https://image.tmdb.org/t/p/w500"
#endif
#define CAPA_VIDEO_PASTA REMOTO_RAIZ "/midia/imagens/Videos"

static bool tmdbChave(char* chave, int tam) {
    chave[0] = 0;
    FILE* f = fopen(REMOTO_RAIZ "/configuracao/chaves_api.txt", "r");
    if (!f) return false;
    char l[512];
    while (fgets(l, sizeof(l), f)) {
        if (strncasecmp(l, "tmdb=", 5) != 0) continue;
        char* v = l + 5; while (*v == ' ') v++;
        snprintf(chave, tam, "%s", v);
        chave[strcspn(chave, "\r\n ")] = 0;
        break;
    }
    fclose(f);
    return chave[0] != 0;
}

static bool videoExtensao(const char* nome) {
    const char* p = strrchr(nome, '.');
    if (!p) return false;
    static const char* exts[] = { ".mp4", ".mkv", ".avi", ".mov", ".wmv", ".m4v", ".webm", ".ts", ".flv", ".mpg", ".mpeg", ".3gp", ".m2ts" };
    for (unsigned i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) if (!strcasecmp(p, exts[i])) return true;
    return false;
}

// "Serie.Nome.S01E02.1080p.WEB-DL.mkv" -> "Serie Nome", série; "Filme (2010) 720p.mp4" -> "Filme", ano 2010.
static void videoTermo(const char* nome, bool arquivo, char* termo, int tam, bool* serie, int* ano) {
    char s[300]; snprintf(s, sizeof(s), "%s", nome);
    if (arquivo) { char* ponto = strrchr(s, '.'); if (ponto) *ponto = 0; }
    for (char* c = s; *c; c++) if (*c == '.' || *c == '_' || *c == '(' || *c == ')' || *c == '[' || *c == ']' || *c == '{' || *c == '}' || *c == '-') *c = ' ';
    static const char* paradas[] = { "480p", "576p", "720p", "1080p", "2160p", "4k", "uhd", "x264", "x265", "h264", "h265", "hevc", "avc", "web", "webrip", "webdl",
        "bluray", "blu", "brrip", "bdrip", "dvdrip", "hdtv", "hdrip", "dublado", "legendado", "dual", "audio", "nacional", "hdr", "aac", "ac3", "dts", "10bit",
        "temporada", "season", "episodio", "ep", "completo", "complete", "torrent", NULL };
    *serie = false; *ano = 0;
    int o = 0, palavras = 0;
    termo[0] = 0;
    char* ctx = NULL;
    for (char* w = strtok_r(s, " ", &ctx); w; w = strtok_r(NULL, " ", &ctx)) {
        char m[64]; snprintf(m, sizeof(m), "%s", w);
        for (char* c = m; *c; c++) *c = (char)tolower((unsigned char)*c);
        int n = (int)strlen(m);
        // S01E02, S01, 1x02
        if (m[0] == 's' && isdigit((unsigned char)m[1])) { int k = 1; while (isdigit((unsigned char)m[k])) k++; if (m[k] == 0 || m[k] == 'e') { *serie = true; break; } }
        { int k = 0; while (isdigit((unsigned char)m[k])) k++; if (k > 0 && m[k] == 'x' && isdigit((unsigned char)m[k + 1])) { *serie = true; break; } }
        if (n == 4 && isdigit((unsigned char)m[0]) && isdigit((unsigned char)m[1]) && isdigit((unsigned char)m[2]) && isdigit((unsigned char)m[3]) && palavras > 0) {
            int a = atoi(m); if (a >= 1900 && a <= 2099) { *ano = a; break; }
        }
        bool parar = false;
        for (int i = 0; paradas[i]; i++) if (!strcmp(m, paradas[i])) parar = true;
        if (parar && palavras > 0) break;
        if (palavras == 0 && n <= 3 && isdigit((unsigned char)m[0]) && isdigit((unsigned char)m[n - 1])) continue;   // "01 - Nome"
        o += snprintf(termo + o, tam - o, "%s%s", o ? " " : "", w);
        if (o >= tam) break;
        palavras++;
    }
}

static void videoCaminhoCache(const char* nome, bool arquivo, char* saida, int tam) {
    char base[260]; snprintf(base, sizeof(base), "%s", nome);
    if (arquivo) { char* ponto = strrchr(base, '.'); if (ponto) *ponto = 0; }
    snprintf(saida, tam, CAPA_VIDEO_PASTA "/%s.jpg", base);
}

// Primeiro "poster_path" que não seja null (aceita espaço depois dos dois pontos).
static void tmdbPoster(const char* resp, char* poster, int tam) {
    poster[0] = 0;
    for (const char* p = strstr(resp, "\"poster_path\""); p; p = strstr(p + 1, "\"poster_path\"")) {
        const char* v = p + 13;
        while (*v == ' ' || *v == ':') v++;
        if (*v != '"') continue;
        v++;
        const char* f = strchr(v, '"');
        if (f && f - v > 1 && f - v < tam) { memcpy(poster, v, f - v); poster[f - v] = 0; return; }
    }
}

// Busca o pôster no TMDB e grava no cache. Roda na thread das capas.
static void videoBuscarCapa(const char* caminho) {
    const char* nome = strrchr(caminho, '/'); nome = nome ? nome + 1 : caminho;
    struct stat st;
    bool arquivo = !(stat(caminho, &st) == 0 && S_ISDIR(st.st_mode));
    char chave[128];
    if (!tmdbChave(chave, sizeof(chave))) { remotoLog("CAPA VIDEO %s: sem chave tmdb (Chaves de API)", nome); return; }
    char termo[300]; bool serie = false; int ano = 0;
    videoTermo(nome, arquivo, termo, sizeof(termo), &serie, &ano);
    if (!termo[0]) return;
    char q[700]; capaUrlCodificar(termo, q, sizeof(q));
    char url[1200];
    if (serie) snprintf(url, sizeof(url), TMDB_API "/search/tv?api_key=%s&language=pt-BR&query=%s", chave, q);
    else if (ano) snprintf(url, sizeof(url), TMDB_API "/search/movie?api_key=%s&language=pt-BR&query=%s&year=%d", chave, q, ano);
    else snprintf(url, sizeof(url), TMDB_API "/search/multi?api_key=%s&language=pt-BR&query=%s", chave, q);
    char* resp = NULL; size_t tam = 0;
    int stt = iaHttpGet(url, &resp, &tam);
    char poster[200] = "";
    if (stt == 200 && resp) {
        tmdbPoster(resp, poster, sizeof(poster));
    }
    free(resp); resp = NULL;
    // Com ano e nada achado, tenta a busca geral.
    if (!poster[0] && (serie || ano)) {
        snprintf(url, sizeof(url), TMDB_API "/search/multi?api_key=%s&language=pt-BR&query=%s", chave, q);
        stt = iaHttpGet(url, &resp, &tam);
        if (stt == 200 && resp) {
            tmdbPoster(resp, poster, sizeof(poster));
        }
        free(resp); resp = NULL;
    }
    if (!poster[0]) { remotoLog("CAPA VIDEO %s: \"%s\" sem resultado (HTTP %d)", nome, termo, stt); return; }
    snprintf(url, sizeof(url), TMDB_IMG "%s", poster);
    stt = iaHttpGet(url, &resp, &tam);
    if (stt != 200 || tam < 500) { free(resp); remotoLog("CAPA VIDEO %s: imagem falhou (HTTP %d)", nome, stt); return; }
    char destino[600]; videoCaminhoCache(nome, arquivo, destino, sizeof(destino));
    remotoCriarPastas(CAPA_VIDEO_PASTA);
    FILE* o = fopen(destino, "wb");
    if (o) { fwrite(resp, 1, tam, o); fclose(o); }
    free(resp);
    musicaCapaVersao++;
    remotoLog("CAPA VIDEO %s: \"%s\"%s%s -> TMDB (%u bytes)", nome, termo, serie ? " (serie)" : "", ano ? " (ano)" : "", (unsigned)tam);
}
static void* capaThread(void*) {
    for (;;) {
        char caminho[512];
        pthread_mutex_lock(&capaTrava);
        caminho[0] = 0;
        if (capaFilaN > 0) { capaFilaN--; snprintf(caminho, sizeof(caminho), "%s", capaFila[capaFilaN]); }
        if (!caminho[0]) { capaThreadViva = false; pthread_mutex_unlock(&capaTrava); return NULL; }
        pthread_mutex_unlock(&capaTrava);
        if (!strncmp(caminho, "V|", 2)) { videoBuscarCapa(caminho + 2); remotoDormir(300); continue; }
        const char* nome = strrchr(caminho, '/'); nome = nome ? nome + 1 : caminho;
        Id3 t; id3Ler(caminho, &t); free(t.letra);
        char termo[420];
        if (t.artista[0] || t.titulo[0]) snprintf(termo, sizeof(termo), "%s %s", t.artista, t.titulo);
        else capaTermoDoArquivo(nome, termo, sizeof(termo));
        free(t.imagem);
        char fonte[16] = "";
        size_t tam = 0;
        uint8_t* img = termo[0] ? capaBuscarOnline(termo, &tam, fonte, sizeof(fonte)) : NULL;
        char destino[600]; capaCaminhoCache(nome, destino, sizeof(destino));
        if (img) {
            remotoCriarPastas(CAPA_PASTA);
            FILE* f = fopen(destino, "wb");
            if (f) { fwrite(img, 1, tam, f); fclose(f); }
            free(img);
            musicaCapaNova = true;
            musicaCapaVersao++;
            remotoLog("CAPA %s: \"%s\" -> %s (%u bytes)", nome, termo, fonte, (unsigned)tam);
        }
        else remotoLog("CAPA %s: \"%s\" sem resultado", nome, termo);
        remotoDormir(400);   // o iTunes aceita uns 20 pedidos por minuto
    }
}

// ---- Letra da música ----
// Vem de um .lrc com o mesmo nome ao lado do arquivo ou da etiqueta USLT do MP3.
// Linhas com [mm:ss.xx] viram letra sincronizada; sem tempo, a letra fica parada.
struct LetraLinha { float t; char txt[200]; };
#define LETRA_MAX 600
static LetraLinha* letraLinhas = NULL;
static int letraN = 0;
static bool letraSincronizada = false;
static char letraDe[512] = "";
static char letraFonte[16] = "";

static int letraOrdenar(const void* a, const void* b) {
    float x = ((const LetraLinha*)a)->t, y = ((const LetraLinha*)b)->t;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void letraInterpretar(const char* texto) {
    letraN = 0; letraSincronizada = false;
    if (!letraLinhas) letraLinhas = (LetraLinha*)malloc(sizeof(LetraLinha) * LETRA_MAX);
    if (!letraLinhas || !texto) return;
    const char* p = texto;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
    while (*p && letraN < LETRA_MAX) {
        const char* fim = p;
        while (*fim && *fim != '\n' && *fim != '\r') fim++;
        float tempos[8]; int nt = 0; bool meta = false;
        const char* q = p;
        while (q < fim && *q == '[') {
            const char* fecha = (const char*)memchr(q, ']', fim - q);
            if (!fecha) break;
            int mm = 0, lidos = 0; float ss = 0;
            if (sscanf(q, "[%d:%f]%n", &mm, &ss, &lidos) >= 2 && lidos > 0 && q + lidos - 1 == fecha) { if (nt < 8) tempos[nt++] = mm * 60 + ss; }
            else if (isalpha((unsigned char)q[1])) meta = true;   // [ar:...], [ti:...], [offset:...]
            else break;
            q = fecha + 1;
        }
        while (q < fim && (*q == ' ' || *q == '\t')) q++;
        int n = (int)(fim - q); if (n > 199) n = 199;
        while (n > 0 && (q[n - 1] == ' ' || q[n - 1] == '\t')) n--;
        if (!meta) {
            if (nt == 0) { letraLinhas[letraN].t = -1; memcpy(letraLinhas[letraN].txt, q, n); letraLinhas[letraN].txt[n] = 0; letraN++; }
            for (int i = 0; i < nt && letraN < LETRA_MAX; i++) {
                letraLinhas[letraN].t = tempos[i]; memcpy(letraLinhas[letraN].txt, q, n); letraLinhas[letraN].txt[n] = 0; letraN++;
                letraSincronizada = true;
            }
        }
        p = fim;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
    if (letraSincronizada) {
        int k = 0;
        for (int i = 0; i < letraN; i++) if (letraLinhas[i].t >= 0) letraLinhas[k++] = letraLinhas[i];
        letraN = k;
        qsort(letraLinhas, letraN, sizeof(LetraLinha), letraOrdenar);
    }
    else {
        int ini = 0; while (ini < letraN && !letraLinhas[ini].txt[0]) ini++;
        if (ini) { memmove(letraLinhas, letraLinhas + ini, sizeof(LetraLinha) * (letraN - ini)); letraN -= ini; }
        while (letraN > 0 && !letraLinhas[letraN - 1].txt[0]) letraN--;
    }
}

// .lrc em UTF-8 ou Latin-1.
static char* letraLerArquivo(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long tam = ftell(f); fseek(f, 0, SEEK_SET);
    if (tam <= 0 || tam > 512 * 1024) { fclose(f); return NULL; }
    char* d = (char*)malloc(tam + 1);
    if (!d) { fclose(f); return NULL; }
    tam = (long)fread(d, 1, tam, f); fclose(f); d[tam] = 0;
    bool utf8 = true;
    for (long i = 0; i < tam && utf8; i++) {
        unsigned char ch = (unsigned char)d[i];
        int seg = ch < 0x80 ? 0 : (ch >> 5) == 6 ? 1 : (ch >> 4) == 14 ? 2 : (ch >> 3) == 30 ? 3 : -1;
        if (seg < 0) utf8 = false;
        for (int k = 1; k <= seg && utf8; k++) if (i + k >= tam || ((unsigned char)d[i + k] >> 6) != 2) utf8 = false;
        if (seg > 0) i += seg;
    }
    if (utf8) return d;
    IaBuf b = { NULL, 0, 0 }; iaMais(&b, "", 0);
    for (long i = 0; i < tam; i++) iaUtf8(&b, (unsigned char)d[i]);
    free(d);
    return b.d;
}

// Prepara a letra da música (guarda a última). Devolve quantas linhas tem.
int letraCarregar(const char* caminhoMusica) {
    if (!caminhoMusica || !caminhoMusica[0] || !strcmp(caminhoMusica, "PARADO") || !strncmp(caminhoMusica, "http", 4)) { letraN = 0; letraDe[0] = 0; return 0; }
    if (!strcmp(letraDe, caminhoMusica)) return letraN;
    snprintf(letraDe, sizeof(letraDe), "%s", caminhoMusica);
    letraN = 0; letraFonte[0] = 0; letraSincronizada = false;
    char lrc[600]; snprintf(lrc, sizeof(lrc), "%s", caminhoMusica);
    char* ponto = strrchr(lrc, '.'); char* barra = strrchr(lrc, '/');
    if (ponto && (!barra || ponto > barra)) *ponto = 0;
    size_t k = strlen(lrc);
    if (k + 5 < sizeof(lrc)) memcpy(lrc + k, ".lrc", 5);
    char* texto = letraLerArquivo(lrc);
    if (texto) { letraInterpretar(texto); free(texto); snprintf(letraFonte, sizeof(letraFonte), ".lrc"); }
    if (letraN == 0) {
        Id3 t;
        id3Ler(caminhoMusica, &t);
        free(t.imagem);
        if (t.letra) { letraInterpretar(t.letra); free(t.letra); snprintf(letraFonte, sizeof(letraFonte), "ID3"); }
    }
    return letraN;
}
// Caminho da capa pronta para a música (ou "" e um pedido em segundo plano).
// Põe um pedido na fila das capas (o último pedido sai primeiro).
static void capaPedir(const char* pedido, const char* nome) {
    pthread_mutex_lock(&capaTrava);
    bool naFila = false;
    for (int i = 0; i < capaFilaN; i++) if (!strcmp(capaFila[i], pedido)) naFila = true;
    // Fila cheia: não marca como tentada, para pedir de novo quando houver vaga.
    if (!naFila && !capaJaTentou(nome) && capaFilaN < CAPA_FILA) {
        snprintf(capaTentadas[capaNTentadas % 64], 128, "%s", nome); capaNTentadas++;
        snprintf(capaFila[capaFilaN++], 512, "%s", pedido);
        if (!capaThreadViva) {
            capaThreadViva = true;
            if (!remotoCriarThread(capaThread, NULL)) capaThreadViva = false;
        }
    }
    pthread_mutex_unlock(&capaTrava);
}

bool musicaCapaCaminho(const char* caminhoMusica, char* saida, int tam) {
    saida[0] = 0;
    if (!caminhoMusica || !caminhoMusica[0] || !strcmp(caminhoMusica, "PARADO")) return false;
    const char* nome = strrchr(caminhoMusica, '/'); nome = nome ? nome + 1 : caminhoMusica;
    char cache[600]; capaCaminhoCache(nome, cache, sizeof(cache));
    FILE* f = fopen(cache, "rb");
    if (f) { fclose(f); snprintf(saida, tam, "%s", cache); return true; }
    // Capa dentro do MP3: copia para o cache e usa, sem internet.
    Id3 t;
    bool temId3 = id3Ler(caminhoMusica, &t);
    free(t.letra); t.letra = NULL;
    if (temId3 && t.imagem && t.tamImagem > 100) {
        remotoCriarPastas(CAPA_PASTA);
        FILE* o = fopen(cache, "wb");
        if (o) { fwrite(t.imagem, 1, t.tamImagem, o); fclose(o); free(t.imagem); snprintf(saida, tam, "%s", cache); return true; }
    }
    free(t.imagem);
    capaPedir(caminhoMusica, nome);
    return false;
}

// Capa de filme ou série (arquivo de vídeo ou pasta de série): cache ou pedido ao TMDB.
bool videoCapaCaminho(const char* caminho, char* saida, int tam) {
    saida[0] = 0;
    if (!caminho || !caminho[0]) return false;
    const char* nome = strrchr(caminho, '/'); nome = nome ? nome + 1 : caminho;
    struct stat st;
    bool pasta = stat(caminho, &st) == 0 && S_ISDIR(st.st_mode);
    if (!pasta && !videoExtensao(nome)) return false;
    char cache[600]; videoCaminhoCache(nome, !pasta, cache, sizeof(cache));
    FILE* f = fopen(cache, "rb");
    if (f) { fclose(f); snprintf(saida, tam, "%s", cache); return true; }
    char chave[128];
    if (!tmdbChave(chave, sizeof(chave))) return false;
    char pedido[520]; snprintf(pedido, sizeof(pedido), "V|%s", caminho);
    capaPedir(pedido, nome);
    return false;
}

// ---- texto de arquivos (para ler em voz alta ou perguntar sobre ele) ----
static void iaXmlParaTexto(const char* x, size_t n, IaBuf* out, const char* const* quebras) {
    size_t i = 0;
    while (i < n) {
        if (x[i] == '<') {
            size_t f = i; while (f < n && x[f] != '>') f++;
            // <script> e <style> somem com o conteúdo.
            if (!strncasecmp(x + i, "<script", 7) || !strncasecmp(x + i, "<style", 6)) {
                const char* fecha = !strncasecmp(x + i, "<script", 7) ? "</script" : "</style";
                size_t k = f; while (k + 8 < n && strncasecmp(x + k, fecha, strlen(fecha))) k++;
                f = k; while (f < n && x[f] != '>') f++;
            }
            else for (int q = 0; quebras[q]; q++) if (!strncasecmp(x + i, quebras[q], strlen(quebras[q]))) { iaTxt(out, "\n"); break; }
            i = f + 1;
            continue;
        }
        if (x[i] == '&') {
            size_t f = i; while (f < n && f - i < 10 && x[f] != ';') f++;
            if (f < n && x[f] == ';') {
                char ent[12]; size_t ln = f - i - 1; if (ln > 10) ln = 10; memcpy(ent, x + i + 1, ln); ent[ln] = 0;
                uint32_t cp = 0;
                if (!strcmp(ent, "amp")) cp = '&'; else if (!strcmp(ent, "lt")) cp = '<'; else if (!strcmp(ent, "gt")) cp = '>';
                else if (!strcmp(ent, "quot")) cp = '"'; else if (!strcmp(ent, "apos")) cp = '\''; else if (!strcmp(ent, "nbsp")) cp = ' ';
                else if (ent[0] == '#') cp = (uint32_t)(ent[1] == 'x' || ent[1] == 'X' ? strtol(ent + 2, NULL, 16) : strtol(ent + 1, NULL, 10));
                if (cp) { iaUtf8(out, cp); i = f + 1; continue; }
            }
        }
        iaMais(out, x + i, 1);
        i++;
    }
}
static bool iaZipArquivo(mz_zip_archive* z, const char* nome, IaBuf* out, const char* const* quebras) {
    size_t n = 0;
    void* d = mz_zip_reader_extract_file_to_heap(z, nome, &n, 0);
    if (!d) return false;
    iaXmlParaTexto((const char*)d, n, out, quebras);
    mz_free(d);
    return true;
}
// Texto puro: aceita UTF-8 e, se não for, trata como Latin-1 (arquivos antigos do Windows).
static void iaTextoPuro(const char* d, size_t n, IaBuf* out) {
    size_t i = 0;
    if (n >= 3 && (unsigned char)d[0] == 0xEF && (unsigned char)d[1] == 0xBB && (unsigned char)d[2] == 0xBF) i = 3;
    while (i < n) {
        unsigned char c = (unsigned char)d[i];
        int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        bool ok = extra >= 0 && i + extra < n;
        for (int k = 1; ok && k <= extra; k++) if (((unsigned char)d[i + k] & 0xC0) != 0x80) ok = false;
        if (ok) { iaMais(out, d + i, extra + 1); i += extra + 1; }
        else { iaUtf8(out, c); i++; }
    }
}
// pagina: só para PDF (0 = o documento todo).
static bool iaTextoArquivo(const char* caminho, int pagina, IaBuf* out, char* erro, int tamErro) {
    const char* ext = strrchr(caminho, '.');
    char e[12] = ""; if (ext) { snprintf(e, sizeof(e), "%s", ext + 1); for (char* p = e; *p; p++) *p = (char)tolower((unsigned char)*p); }
    static const char* const qDoc[] = { "</w:p>", "<w:br", "<w:tab", NULL };
    static const char* const qOdt[] = { "</text:p>", "</text:h>", "<text:line-break", "<text:tab", NULL };
    static const char* const qHtml[] = { "</p", "<br", "</div", "</li", "</h1", "</h2", "</h3", "</h4", "</tr", "</title", NULL };
    if (!strcmp(e, "pdf")) {
        if (!iaTextoPdf(caminho, pagina, out)) { snprintf(erro, tamErro, "Não consegui ler o PDF."); return false; }
    }
    else if (!strcmp(e, "docx") || !strcmp(e, "odt") || !strcmp(e, "epub")) {
        mz_zip_archive z; memset(&z, 0, sizeof(z));
        if (!mz_zip_reader_init_file(&z, caminho, 0)) { snprintf(erro, tamErro, "Arquivo .%s corrompido.", e); return false; }
        bool ok = false;
        if (!strcmp(e, "docx")) ok = iaZipArquivo(&z, "word/document.xml", out, qDoc);
        else if (!strcmp(e, "odt")) ok = iaZipArquivo(&z, "content.xml", out, qOdt);
        else {
            // EPUB: os capítulos (xhtml/html) na ordem do arquivo.
            for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&z) && out->n < 400000; i++) {
                char nome[512]; mz_zip_reader_get_filename(&z, i, nome, sizeof(nome));
                const char* x = strrchr(nome, '.');
                if (x && (!strcasecmp(x, ".xhtml") || !strcasecmp(x, ".html") || !strcasecmp(x, ".htm")) && iaZipArquivo(&z, nome, out, qHtml)) { iaTxt(out, "\n"); ok = true; }
            }
        }
        mz_zip_reader_end(&z);
        if (!ok) { snprintf(erro, tamErro, "Não achei o texto dentro do .%s.", e); return false; }
    }
    else {
        static const char* textos[] = { "txt", "md", "srt", "ass", "ini", "cfg", "json", "xml", "csv", "log", "html", "htm", "nfo", "lrc", "c", "cpp", "h", "py", "js", NULL };
        bool conhecido = false;
        for (int i = 0; textos[i]; i++) if (!strcmp(e, textos[i])) conhecido = true;
        if (!conhecido) { snprintf(erro, tamErro, "Leio em voz alta PDF, TXT, DOCX, ODT, EPUB e HTML (este é .%s).", e[0] ? e : "sem extensão"); return false; }
        FILE* f = fopen(caminho, "rb");
        if (!f) { snprintf(erro, tamErro, "Não abri o arquivo."); return false; }
        IaBuf bruto = { NULL, 0, 0 };
        char t[16384]; size_t n;
        while ((n = fread(t, 1, sizeof(t), f)) > 0 && bruto.n < 2000000) iaMais(&bruto, t, n);
        fclose(f);
        if (!strcmp(e, "html") || !strcmp(e, "htm")) iaXmlParaTexto(bruto.d ? bruto.d : "", bruto.n, out, qHtml);
        else iaTextoPuro(bruto.d ? bruto.d : "", bruto.n, out);
        free(bruto.d);
    }
    // Junta espaços e linhas em branco repetidos.
    size_t o = 0; int brancos = 0, quebras = 0;
    for (size_t i = 0; i < out->n; i++) {
        char c = out->d[i] == '\r' || out->d[i] == '\t' ? ' ' : out->d[i];
        if (c == '\n') { if (++quebras <= 2) out->d[o++] = c; brancos = 0; continue; }
        if (c == ' ') { if (!brancos++ && !quebras) out->d[o++] = c; continue; }
        brancos = 0; quebras = 0; out->d[o++] = c;
    }
    out->n = o; if (out->d) out->d[o] = 0;
    if (!out->n) { snprintf(erro, tamErro, "O arquivo não tem texto para ler (PDF escaneado é só imagem)."); return false; }
    return true;
}

// ---- trabalhos em segundo plano ----
static void iaGravarWav(const char* caminho, const uint8_t* pcm, size_t n, int taxa) {
    FILE* f = fopen(caminho, "wb");
    if (!f) return;
    uint8_t h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0 };
    uint32_t v;
    v = (uint32_t)(36 + n); memcpy(h + 4, &v, 4);
    v = (uint32_t)taxa; memcpy(h + 24, &v, 4);
    v = (uint32_t)taxa * 2; memcpy(h + 28, &v, 4);
    h[32] = 2; h[34] = 16;
    memcpy(h + 36, "data", 4);
    v = (uint32_t)n; memcpy(h + 40, &v, 4);
    fwrite(h, 1, 44, f); fwrite(pcm, 1, n, f);
    fclose(f);
}
// Sem marcações que a voz leria em voz alta.
static char* iaTextoParaFala(const char* t, size_t n) {
    char* s = (char*)malloc(n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) if (!strchr("*#`_>|", t[i])) s[o++] = t[i];
    while (o > 0 && ((unsigned char)s[o - 1] & 0xC0) == 0x80) o--;
    if (o > 0 && ((unsigned char)s[o - 1] & 0xC0) == 0xC0) o--;
    s[o] = 0;
    return s;
}
// Lê um texto de qualquer tamanho: em pedaços de ~1200 letras, gerando o próximo enquanto o atual toca.
static bool iaLerEmVoz(const char* texto, const char* titulo) {
    IaConta c; bool outra = false;
    if (!iaContaPara(CAP_VOZ, &c, &outra)) { iaDefinirErro("Para ler em voz alta, cadastre uma conta Gemini ou ChatGPT (o Groq não fala)."); return false; }
    size_t total = strlen(texto), pos = 0;
    int parte = 0, partes = (int)(total / 1200) + 1;
    pthread_mutex_lock(&ia.trava); ia.lendo = true; ia.pararLeitura = false; ia.versao++; pthread_mutex_unlock(&ia.trava);
    bool ok = true;
    while (pos < total && !ia.pararLeitura) {
        size_t fim = pos + 1200;
        if (fim >= total) fim = total;
        else {
            size_t corte = fim;
            while (corte > pos + 600 && !strchr(".!?\n", texto[corte - 1])) corte--;
            if (corte > pos + 600) fim = corte;
            else { while (fim > pos && texto[fim] != ' ') fim--; if (fim == pos) fim = pos + 1200; }
        }
        char tarefa[128];
        snprintf(tarefa, sizeof(tarefa), "Lendo %s em voz alta (%d de %d)%s...", titulo, ++parte, partes, outra ? " com outra conta" : "");
        iaDefinirTarefa(tarefa);
        char* fala = iaTextoParaFala(texto + pos, fim - pos);
        IaResultado r;
        int taxa = iaVoz(&c, fala, &r);
        free(fala);
        if (!taxa) { iaDefinirErro(r.erro); iaResultadoLimpar(&r); ok = false; break; }
        // Espera o pedaço anterior acabar de tocar.
        while (iaTocandoAgora() && !ia.pararLeitura) remotoDormir(50);
        if (ia.pararLeitura) { iaResultadoLimpar(&r); break; }
        if (parte == 1) { remotoCriarPastas(IA_PASTA); iaGravarWav(IA_PASTA "/resposta.wav", r.dados, r.tamDados, taxa); }
        iaTocarPcm((const int16_t*)r.dados, (int)(r.tamDados / 2), taxa);
        iaResultadoLimpar(&r);
        pos = fim;
        while (pos < total && (texto[pos] == ' ' || texto[pos] == '\n')) pos++;
    }
    while (iaTocandoAgora() && !ia.pararLeitura) remotoDormir(100);
    pthread_mutex_lock(&ia.trava); ia.lendo = false; ia.versao++; pthread_mutex_unlock(&ia.trava);
    return ok;
}

enum { IA_PERGUNTAR = 1, IA_IMAGEM, IA_FALAR, IA_OUVIR, IA_LER_ARQUIVO, IA_PERGUNTAR_ARQUIVO };
struct IaTrabalho { int tipo; char* texto; bool comTela; char caminho[512]; int pagina; };

static void* iaThread(void* arg) {
    IaTrabalho* t = (IaTrabalho*)arg;
    IaConta c; bool outra = false;
    pthread_mutex_lock(&ia.trava); ia.erro[0] = 0; ia.versao++; pthread_mutex_unlock(&ia.trava);
    if (!iaContaPara(CAP_TEXTO, &c, NULL)) {
        iaDefinirErro("Nenhuma conta de IA. Cadastre em Chaves de API > Inteligência Artificial (Gemini, ChatGPT ou Groq).");
        goto fim;
    }
    if (t->tipo == IA_OUVIR) {
        iaDefinirTarefa("Entendendo o que você falou...");
        FILE* f = fopen(t->caminho, "rb");
        IaBuf audio = { NULL, 0, 0 };
        if (f) { char b[16384]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) iaMais(&audio, b, n); fclose(f); }
        if (audio.n < 2000) { free(audio.d); iaDefinirErro("Gravação muito curta."); goto fim; }
        IaResultado r;
        iaTranscreverDados(&c, (const uint8_t*)audio.d, audio.n, "audio/wav", "pergunta.wav", true, &r);
        free(audio.d);
        if (!r.texto.n) { iaDefinirErro(r.erro[0] ? r.erro : "Não entendi o áudio."); iaResultadoLimpar(&r); goto fim; }
        iaLimparTexto(r.texto.d, false);
        free(t->texto); t->texto = strdup(r.texto.d);
        iaResultadoLimpar(&r);
        t->tipo = IA_PERGUNTAR;
    }
    if (t->tipo == IA_LER_ARQUIVO || t->tipo == IA_PERGUNTAR_ARQUIVO) {
        const char* nome = strrchr(t->caminho, '/'); nome = nome ? nome + 1 : t->caminho;
        char tarefa[300]; snprintf(tarefa, sizeof(tarefa), "Abrindo %s...", nome); iaDefinirTarefa(tarefa);
        IaBuf texto = { NULL, 0, 0 }; char erro[200] = "";
        if (!iaTextoArquivo(t->caminho, t->pagina, &texto, erro, sizeof(erro))) { iaDefinirErro(erro); free(texto.d); goto fim; }
        if (t->tipo == IA_LER_ARQUIVO) {
            char titulo[200];
            if (t->pagina > 0) snprintf(titulo, sizeof(titulo), "a página %d", t->pagina); else snprintf(titulo, sizeof(titulo), "%.150s", nome);
            iaLerEmVoz(texto.d, titulo);
            free(texto.d);
            goto fim;
        }
        // Pergunta sobre o arquivo: o texto vai junto (até ~60 mil letras).
        if (texto.n > 60000) { texto.n = 60000; while (texto.n > 0 && ((unsigned char)texto.d[texto.n] & 0xC0) == 0x80) texto.n--; texto.d[texto.n] = 0; }
        IaBuf p = { NULL, 0, 0 };
        iaTxt(&p, "Conteúdo do arquivo \""); iaTxt(&p, nome); iaTxt(&p, "\":\n\n"); iaTxt(&p, texto.d); iaTxt(&p, "\n\nPedido: "); iaTxt(&p, t->texto[0] ? t->texto : "Resuma este arquivo.");
        free(texto.d);
        char visivel[400]; snprintf(visivel, sizeof(visivel), "[%s] %s", nome, t->texto[0] ? t->texto : "Resuma este arquivo.");
        iaMsgAdicionar('u', visivel, "");
        iaDefinirTarefa("Lendo o arquivo e pensando...");
        IaResultado r;
        iaChat(&c, p.d, NULL, 0, &r);
        free(p.d);
        if (r.texto.n) { iaMsgAdicionar('m', r.texto.d, ""); if (ia.falar) iaLerEmVoz(r.texto.d, "a resposta"); }
        else iaDefinirErro(r.erro);
        iaResultadoLimpar(&r);
        goto fim;
    }
    if (t->tipo == IA_PERGUNTAR) {
        uint8_t* jpeg = NULL; int tamJpeg = 0;
        if (t->comTela && !iaTelaJpeg(&jpeg, &tamJpeg)) { iaDefinirErro("Não consegui pegar a imagem da tela."); goto fim; }
        iaMsgAdicionar('u', t->texto, t->comTela ? "tela" : "");
        char tarefa[160]; snprintf(tarefa, sizeof(tarefa), "%s (%s)...", t->comTela ? "Olhando a tela e pensando" : "Pensando", c.nome);
        iaDefinirTarefa(tarefa);
        IaResultado r;
        iaChat(&c, t->texto, jpeg, tamJpeg, &r);
        free(jpeg);
        if (r.texto.n) { iaMsgAdicionar('m', r.texto.d, ""); if (ia.falar) iaLerEmVoz(r.texto.d, "a resposta"); }
        else iaDefinirErro(r.erro);
        iaResultadoLimpar(&r);
    }
    else if (t->tipo == IA_IMAGEM) {
        if (!iaContaPara(CAP_IMAGEM, &c, &outra)) { iaDefinirErro("Para gerar imagens, cadastre uma conta Gemini ou ChatGPT (o Groq não desenha)."); goto fim; }
        iaMsgAdicionar('u', t->texto, "");
        char tarefa[160]; snprintf(tarefa, sizeof(tarefa), "Desenhando com %s%s (pode levar 30 s)...", c.nome, outra ? ", porque a conta escolhida não desenha" : "");
        iaDefinirTarefa(tarefa);
        IaResultado r;
        iaImagem(&c, t->texto, &r);
        if (r.tamDados) {
            remotoCriarPastas(IA_PASTA);
            char caminho[256];
            snprintf(caminho, sizeof(caminho), IA_PASTA "/imagem_%llu.%s", (unsigned long long)remotoAgoraMs(), strstr(r.mime, "jpeg") ? "jpg" : "png");
            FILE* f = fopen(caminho, "wb");
            if (f) { fwrite(r.dados, 1, r.tamDados, f); fclose(f); }
            iaMsgAdicionar('m', r.texto.n ? r.texto.d : "Pronto, a imagem está na tela.", caminho);
            remotoPedirMidia(caminho);
        }
        else iaDefinirErro(r.erro[0] ? r.erro : "A IA não devolveu imagem.");
        iaResultadoLimpar(&r);
    }
    else if (t->tipo == IA_FALAR) iaLerEmVoz(t->texto, "a resposta");

fim:
    free(t->texto);
    free(t);
    pthread_mutex_lock(&ia.trava); ia.tarefa[0] = 0; ia.ocupado = false; ia.versao++; pthread_mutex_unlock(&ia.trava);
    return NULL;
}

bool iaComecar(int tipo, const char* texto, bool comTela, const char* caminho, int pagina) {
    pthread_mutex_lock(&ia.trava);
    iaCarregarTravado();
    if (ia.ocupado) { pthread_mutex_unlock(&ia.trava); return false; }
    ia.ocupado = true; ia.versao++;
    pthread_mutex_unlock(&ia.trava);
    IaTrabalho* t = (IaTrabalho*)calloc(1, sizeof(IaTrabalho));
    t->tipo = tipo; t->texto = strdup(texto ? texto : ""); t->comTela = comTela; t->pagina = pagina;
    if (caminho) snprintf(t->caminho, sizeof(t->caminho), "%s", caminho);
    pthread_attr_t at; pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 512 * 1024);
    pthread_t th;
    bool ok = pthread_create(&th, &at, iaThread, t) == 0;
    pthread_attr_destroy(&at);
    if (ok) pthread_detach(th);
    else { free(t->texto); free(t); pthread_mutex_lock(&ia.trava); ia.ocupado = false; pthread_mutex_unlock(&ia.trava); }
    return ok;
}
// Para a leitura em voz alta (e a voz que estiver tocando).
void iaPararLeitura() { ia.pararLeitura = true; iaPararVoz(); }

static char* iaUltimaResposta() {
    pthread_mutex_lock(&ia.trava);
    char* r = NULL;
    for (int i = ia.nMsgs - 1; i >= 0 && !r; i--) if (ia.msgs[i].quem == 'm' && ia.msgs[i].texto[0]) r = strdup(ia.msgs[i].texto);
    pthread_mutex_unlock(&ia.trava);
    return r;
}

// ---- contas: criar, editar, apagar, escolher ----
// id < 0 cria. Chave vazia ao editar mantém a atual. Devolve erro ou NULL.
const char* iaContaSalvar(int id, int prov, const char* nomeBruto, const char* modeloBruto, const char* chaveBruta, int* idFinal) {
    char nome[64], modelo[96], chave[300];
    snprintf(nome, sizeof(nome), "%s", nomeBruto ? nomeBruto : ""); iaLimparTexto(nome, true);
    snprintf(modelo, sizeof(modelo), "%s", modeloBruto ? modeloBruto : ""); iaLimparTexto(modelo, true);
    snprintf(chave, sizeof(chave), "%s", chaveBruta ? chaveBruta : ""); iaLimparTexto(chave, true);
    for (char* p = chave; *p; p++) if (*p == ' ') return "a chave não pode ter espaços";
    if (prov < 0 || prov >= PROV_N) return "provedor inválido";
    pthread_mutex_lock(&ia.trava);
    iaCarregarTravado();
    const char* erro = NULL;
    if (id < 0) {
        if (ia.nContas >= IA_MAX_CONTAS) erro = "limite de 16 contas";
        else if (strlen(chave) < 20) erro = "chave curta demais (confira se colou inteira)";
        else { id = ia.nContas++; memset(&ia.contas[id], 0, sizeof(IaConta)); if (ia.nContas == 1) ia.contaAtiva = 0; }
    }
    else if (id >= ia.nContas) erro = "conta não existe";
    else if (chave[0] && strlen(chave) < 20) erro = "chave curta demais (confira se colou inteira)";
    if (!erro) {
        IaConta* c = &ia.contas[id];
        if (c->prov != prov) c->modelo[0] = 0;
        c->prov = prov;
        snprintf(c->nome, sizeof(c->nome), "%s", nome[0] ? nome : (c->nome[0] ? c->nome : IA_PROV_CURTO[prov]));
        if (modeloBruto) snprintf(c->modelo, sizeof(c->modelo), "%s", modelo);
        if (chave[0]) snprintf(c->chave, sizeof(c->chave), "%s", chave);
        iaSalvarContasTravado(); iaSalvarConfigTravado();
        ia.versao++;
        if (idFinal) *idFinal = id;
        remotoLog("IA: conta %d (%s, %s) salva", id, c->nome, IA_PROV_ID[prov]);
    }
    pthread_mutex_unlock(&ia.trava);
    return erro;
}
void iaContaApagar(int id) {
    pthread_mutex_lock(&ia.trava);
    iaCarregarTravado();
    if (id >= 0 && id < ia.nContas) {
        memset(ia.contas[id].chave, 0, sizeof(ia.contas[id].chave));
        memmove(ia.contas + id, ia.contas + id + 1, sizeof(IaConta) * (ia.nContas - id - 1));
        ia.nContas--;
        if (ia.contaAtiva > id || ia.contaAtiva >= ia.nContas) ia.contaAtiva = ia.contaAtiva > 0 ? ia.contaAtiva - 1 : 0;
        iaSalvarContasTravado(); iaSalvarConfigTravado();
        ia.versao++;
    }
    pthread_mutex_unlock(&ia.trava);
}
void iaContaUsar(int id) {
    pthread_mutex_lock(&ia.trava);
    iaCarregarTravado();
    if (id >= 0 && id < ia.nContas) { ia.contaAtiva = id; iaSalvarConfigTravado(); ia.versao++; }
    pthread_mutex_unlock(&ia.trava);
}

static void iaContasJson(IaBuf* j) {
    iaTxt(j, "[");
    for (int i = 0; i < ia.nContas; i++) {
        const IaConta* c = &ia.contas[i];
        size_t n = strlen(c->chave);
        char t[160];
        snprintf(t, sizeof(t), "%s{\"id\":%d,\"prov\":\"%s\",\"provNome\":\"%s\",\"ativa\":%s,\"fim\":\"%s\",\"nome\":\"", i ? "," : "", i, IA_PROV_ID[c->prov], IA_PROV_CURTO[c->prov],
            i == ia.contaAtiva ? "true" : "false", n > 4 ? c->chave + n - 4 : "");
        iaTxt(j, t); iaJsonTxt(j, c->nome); iaTxt(j, "\",\"modelo\":\""); iaJsonTxt(j, c->modelo); iaTxt(j, "\",\"modeloUsado\":\""); iaJsonTxt(j, iaModeloDe(c)); iaTxt(j, "\"}");
    }
    iaTxt(j, "]");
}

// ---- rotas da página (aba IA e contas) ----
static bool iaRotas(RemotoConexao* c, const char* alvo, const char* query, long long tamCorpo, bool* tratou) {
    *tratou = true;
    iaCarregar();
    if (!strcmp(alvo, "/ia/estado")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        IaBuf j = { NULL, 0, 0 };
        char t[400];
        pthread_mutex_lock(&ia.trava);
        int prov = ia.nContas ? ia.contas[ia.contaAtiva].prov : PROV_GEMINI;
        snprintf(t, sizeof(t), "{\"versao\":%u,\"ocupado\":%s,\"lendo\":%s,\"contaAtiva\":%d,\"prov\":\"%s\",\"voz\":\"%s\",\"aspecto\":%d,\"modeloImg\":%d,\"falar\":%s,\"tarefa\":\"",
            ia.versao, ia.ocupado ? "true" : "false", ia.lendo ? "true" : "false", ia.contaAtiva, IA_PROV_ID[prov], ia.voz[prov], ia.aspecto, ia.modeloImg, ia.falar ? "true" : "false");
        iaTxt(&j, t); iaJsonTxt(&j, ia.tarefa); iaTxt(&j, "\",\"erro\":\""); iaJsonTxt(&j, ia.erro); iaTxt(&j, "\",\"contas\":");
        iaContasJson(&j);
        iaTxt(&j, ",\"msgs\":[");
        for (int i = 0; i < ia.nMsgs; i++) {
            snprintf(t, sizeof(t), "%s{\"q\":\"%c\",\"t\":\"", i ? "," : "", ia.msgs[i].quem);
            iaTxt(&j, t); iaJsonTxt(&j, ia.msgs[i].texto); iaTxt(&j, "\",\"img\":\""); iaJsonTxt(&j, ia.msgs[i].img); iaTxt(&j, "\"}");
        }
        pthread_mutex_unlock(&ia.trava);
        iaTxt(&j, "]}");
        bool ok = remotoResponder(c->fd, "200 OK", "application/json; charset=utf-8", j.d, j.n);
        free(j.d);
        return ok;
    }
    if (!strcmp(alvo, "/ia/opcoes")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        IaBuf j = { NULL, 0, 0 };
        iaTxt(&j, "{\"provedores\":[");
        for (int p = 0; p < PROV_N; p++) {
            char t[200];
            snprintf(t, sizeof(t), "%s{\"id\":\"%s\",\"nome\":\"%s\",\"site\":\"%s\",\"imagem\":%s,\"voz\":%s,\"modelos\":[", p ? "," : "", IA_PROV_ID[p], IA_PROV_NOME[p], IA_PROV_SITE[p],
                IA_PROV_IMAGEM[p] ? "true" : "false", IA_PROV_VOZ[p] ? "true" : "false");
            iaTxt(&j, t);
            for (int m = 0; m < iaNModelos(p); m++) { iaTxt(&j, m ? ",\"" : "\""); iaTxt(&j, IA_PROV_MODELOS[p][m]); iaTxt(&j, "\""); }
            iaTxt(&j, "],\"vozes\":[");
            for (int v = 0; v < iaNVozes(p); v++) { iaTxt(&j, v ? ",\"" : "\""); iaTxt(&j, IA_PROV_VOZES[p][v]); iaTxt(&j, "\""); }
            iaTxt(&j, "]}");
        }
        iaTxt(&j, "],\"modelosImg\":[");
        for (int i = 0; i < IA_N(IA_MODELOS_IMG_GEMINI); i++) { iaTxt(&j, i ? ",\"" : "\""); iaTxt(&j, IA_MODELOS_IMG_GEMINI[i]); iaTxt(&j, "\""); }
        iaTxt(&j, "],\"aspectos\":[");
        for (int i = 0; i < IA_N(IA_ASPECTOS); i++) { iaTxt(&j, i ? ",\"" : "\""); iaTxt(&j, IA_ASPECTOS[i]); iaTxt(&j, "\""); }
        iaTxt(&j, "]}");
        bool ok = remotoResponder(c->fd, "200 OK", "application/json; charset=utf-8", j.d, j.n);
        free(j.d);
        return ok;
    }
    if (!strncmp(alvo, "/ia/", 4)) {
        if (tamCorpo > 16000) return false;
        char* corpo = (char*)malloc(16384);
        if (!corpo || !remotoLerCorpo(c, tamCorpo, corpo, 16384)) { free(corpo); return false; }
        char v[600] = "", r[300] = "{\"ok\":true}";
        const char* ocupada = "{\"ok\":false,\"erro\":\"a IA ainda está trabalhando\"}";
        if (!strcmp(alvo, "/ia/perguntar") || !strcmp(alvo, "/ia/imagem")) {
            char tela[4] = ""; remotoParam(query, "tela", tela, sizeof(tela));
            if (!corpo[0]) snprintf(r, sizeof(r), "{\"ok\":false,\"erro\":\"escreva alguma coisa\"}");
            else if (!iaComecar(alvo[4] == 'p' ? IA_PERGUNTAR : IA_IMAGEM, corpo, tela[0] == '1', NULL, 0)) snprintf(r, sizeof(r), "%s", ocupada);
        }
        else if (!strcmp(alvo, "/ia/arquivo/ler") || !strcmp(alvo, "/ia/arquivo/perguntar")) {
            char p[1024] = "", pg[12] = "0";
            remotoParam(query, "p", p, sizeof(p)); remotoParam(query, "pagina", pg, sizeof(pg));
            if (!remotoCaminhoValido(p)) snprintf(r, sizeof(r), "{\"ok\":false,\"erro\":\"caminho inválido\"}");
            else if (!iaComecar(alvo[12] == 'l' ? IA_LER_ARQUIVO : IA_PERGUNTAR_ARQUIVO, corpo, false, p, atoi(pg))) snprintf(r, sizeof(r), "%s", ocupada);
        }
        else if (!strcmp(alvo, "/ia/falar")) {
            char* u = iaUltimaResposta();
            if (!u) snprintf(r, sizeof(r), "{\"ok\":false,\"erro\":\"ainda não há resposta\"}");
            else if (!iaComecar(IA_FALAR, u, false, NULL, 0)) snprintf(r, sizeof(r), "%s", ocupada);
            free(u);
        }
        else if (!strcmp(alvo, "/ia/parar")) iaPararLeitura();
        else if (!strcmp(alvo, "/ia/limpar")) iaLimparConversa();
        else if (!strcmp(alvo, "/ia/conta/salvar")) {
            char id[12] = "-1", prov[16] = "gemini", nome[128] = "", modelo[128] = "";
            remotoParam(query, "id", id, sizeof(id)); remotoParam(query, "prov", prov, sizeof(prov));
            bool temNome = remotoParam(query, "nome", nome, sizeof(nome)), temModelo = remotoParam(query, "modelo", modelo, sizeof(modelo));
            int idFinal = -1;
            const char* erro = iaContaSalvar(atoi(id), iaProvPorId(prov), temNome ? nome : NULL, temModelo ? modelo : NULL, corpo, &idFinal);
            if (erro) { IaBuf j = { NULL, 0, 0 }; iaTxt(&j, "{\"ok\":false,\"erro\":\""); iaJsonTxt(&j, erro); iaTxt(&j, "\"}"); snprintf(r, sizeof(r), "%s", j.d); free(j.d); }
            else snprintf(r, sizeof(r), "{\"ok\":true,\"id\":%d}", idFinal);
        }
        else if (!strcmp(alvo, "/ia/conta/apagar")) { remotoParam(query, "id", v, sizeof(v)); iaContaApagar(atoi(v)); }
        else if (!strcmp(alvo, "/ia/conta/usar")) { remotoParam(query, "id", v, sizeof(v)); iaContaUsar(atoi(v)); }
        else if (!strcmp(alvo, "/ia/config")) {
            pthread_mutex_lock(&ia.trava);
            int prov = ia.nContas ? ia.contas[ia.contaAtiva].prov : PROV_GEMINI;
            if (remotoParam(query, "modelo", v, sizeof(v)) && ia.nContas) { iaLimparTexto(v, true); snprintf(ia.contas[ia.contaAtiva].modelo, sizeof(ia.contas[0].modelo), "%s", v); iaSalvarContasTravado(); }
            if (remotoParam(query, "voz", v, sizeof(v))) for (int k = 0; k < iaNVozes(prov); k++) if (!strcmp(v, IA_PROV_VOZES[prov][k])) snprintf(ia.voz[prov], sizeof(ia.voz[prov]), "%s", v);
            if (remotoParam(query, "aspecto", v, sizeof(v)) && atoi(v) >= 0 && atoi(v) < IA_N(IA_ASPECTOS)) ia.aspecto = atoi(v);
            if (remotoParam(query, "modeloImg", v, sizeof(v)) && atoi(v) >= 0 && atoi(v) < IA_N(IA_MODELOS_IMG_GEMINI)) ia.modeloImg = atoi(v);
            if (remotoParam(query, "falar", v, sizeof(v))) ia.falar = v[0] == '1';
            ia.versao++;
            iaSalvarConfigTravado();
            pthread_mutex_unlock(&ia.trava);
        }
        else snprintf(r, sizeof(r), "{\"ok\":false}");
        free(corpo);
        return remotoJson(c->fd, "200 OK", r);
    }
    *tratou = false;
    return true;
}

// =========================================================================
// CHAVES DE API por categoria: Inteligência Artificial (contas, acima), Bancos de imagens,
// Serviços e Outras. Cada serviço junta os seus campos (o Dropbox tem 3, o Telegram 2) e grava
// no arquivo que o recurso já lê. Só os 4 últimos caracteres saem do PS4.
// =========================================================================
#ifndef TELEGRAM_RAIZ
#define TELEGRAM_RAIZ "/data/HyperNeivaTelegram"
#endif
enum { CH_ARQUIVO, CH_LINHA, CH_ROTULO, CH_NOMEVALOR };
struct ChaveDef { const char* id; const char* nome; const char* uso; int tipo; const char* arquivo; const char* rotulo; int linha; };
#define CH_CAPAS REMOTO_RAIZ "/configuracao/chaves_capas.txt"
#define CH_API REMOTO_RAIZ "/configuracao/chaves_api.txt"
#define CH_DROPBOX REMOTO_RAIZ "/configuracao/dropbox_token.txt"
static const ChaveDef CHAVES[] = {
    { "tmdb", "Chave", "", CH_NOMEVALOR, CH_API, "tmdb", 0 },
    { "thegamesdb", "Chave", "", CH_NOMEVALOR, CH_CAPAS, "thegamesdb", 0 },
    { "mobygames", "Chave", "", CH_NOMEVALOR, CH_CAPAS, "mobygames", 0 },
    { "steamgriddb", "Chave", "", CH_NOMEVALOR, CH_CAPAS, "steamgriddb", 0 },
    { "igdb_id", "Client ID", "", CH_NOMEVALOR, CH_CAPAS, "igdb_id", 0 },
    { "igdb_secret", "Client Secret", "", CH_NOMEVALOR, CH_CAPAS, "igdb_secret", 0 },
    { "telegram_id", "api_id", "número", CH_LINHA, TELEGRAM_RAIZ "/api.txt", NULL, 0 },
    { "telegram_hash", "api_hash", "32 letras e números", CH_LINHA, TELEGRAM_RAIZ "/api.txt", NULL, 1 },
    { "dropbox_token", "Refresh token", "", CH_ROTULO, CH_DROPBOX, "Token refresh:", 0 },
    { "dropbox_appkey", "App key", "opcional", CH_ROTULO, CH_DROPBOX, "Appkey:", 0 },
    { "dropbox_secret", "App secret", "opcional", CH_ROTULO, CH_DROPBOX, "App secret:", 0 },
    { "google", "Chave", "", CH_NOMEVALOR, CH_API, "google", 0 },
};
#define CHAVES_FIXAS ((int)(sizeof(CHAVES) / sizeof(CHAVES[0])))
struct ChaveServico { const char* id; const char* nome; const char* categoria; const char* uso; const char* campos[4]; };
static const ChaveServico SERVICOS[] = {
    { "tmdb", "TMDB", "imagens", "Capas de filmes e séries (themoviedb.org)", { "tmdb", NULL } },
    { "steamgriddb", "SteamGridDB", "imagens", "Capas da biblioteca de jogos", { "steamgriddb", NULL } },
    { "thegamesdb", "TheGamesDB", "imagens", "Capas da biblioteca de jogos", { "thegamesdb", NULL } },
    { "mobygames", "MobyGames", "imagens", "Capas da biblioteca de jogos", { "mobygames", NULL } },
    { "igdb", "IGDB (Twitch)", "imagens", "Capas da biblioteca de jogos", { "igdb_id", "igdb_secret", NULL } },
    { "telegram", "Telegram", "servicos", "Entrar no Telegram (my.telegram.org)", { "telegram_id", "telegram_hash", NULL } },
    { "dropbox", "Dropbox", "servicos", "Baixar e enviar pelo Dropbox", { "dropbox_token", "dropbox_appkey", "dropbox_secret", NULL } },
    { "google", "Google", "servicos", "Chave genérica do Google (como no Hyper Neiva de PC)", { "google", NULL } },
};
#define N_SERVICOS ((int)(sizeof(SERVICOS) / sizeof(SERVICOS[0])))
static const char* CATEGORIAS[][2] = { { "ia", "Inteligência Artificial" }, { "imagens", "Bancos de imagens" }, { "servicos", "Serviços" }, { "outras", "Outras chaves" } };
#define N_CATEGORIAS 4

static const ChaveDef* chaveAchar(const char* id) { for (int i = 0; i < CHAVES_FIXAS; i++) if (!strcmp(CHAVES[i].id, id)) return &CHAVES[i]; return NULL; }

static char* chaveLerArquivo(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return NULL;
    IaBuf b = { NULL, 0, 0 };
    char t[4096]; size_t n;
    while ((n = fread(t, 1, sizeof(t), f)) > 0 && b.n < 65536) iaMais(&b, t, n);
    fclose(f);
    if (!b.d) iaMais(&b, "", 0);
    return b.d;
}
static void chaveLimpar(char* s) { iaLimparTexto(s, false); }
static bool chaveAcharLinha(const char* texto, const char* nome, bool comIgual, char* saida, int tam) {
    size_t ln = strlen(nome);
    for (const char* p = texto; p && *p; ) {
        const char* fim = strchr(p, '\n'); if (!fim) fim = p + strlen(p);
        const char* q = p; while (q < fim && (*q == ' ' || *q == '\t')) q++;
        bool bate = comIgual ? (!strncmp(q, nome, ln) && q[ln] == '=') : !strncasecmp(q, nome, ln);
        if (bate) {
            q += comIgual ? ln + 1 : ln;
            int n = (int)(fim - q); if (n >= tam) n = tam - 1;
            memcpy(saida, q, n); saida[n] = 0; chaveLimpar(saida);
            return true;
        }
        p = *fim ? fim + 1 : NULL;
    }
    return false;
}
bool chaveLer(const ChaveDef* d, char* saida, int tam) {
    saida[0] = 0;
    char* t = chaveLerArquivo(d->arquivo);
    if (!t) return false;
    if (d->tipo == CH_ARQUIVO) { snprintf(saida, tam, "%s", t); chaveLimpar(saida); }
    else if (d->tipo == CH_LINHA) {
        const char* p = t;
        for (int i = 0; i < d->linha && p; i++) { p = strchr(p, '\n'); if (p) p++; }
        if (p) { int n = (int)strcspn(p, "\r\n"); if (n >= tam) n = tam - 1; memcpy(saida, p, n); saida[n] = 0; chaveLimpar(saida); }
    }
    else if (d->tipo == CH_ROTULO) {
        bool rotulado = strstr(t, "Token refresh:") || strstr(t, "Appkey:") || strstr(t, "App secret:");
        if (rotulado) chaveAcharLinha(t, d->rotulo, false, saida, tam);
        else if (!strcmp(d->rotulo, "Token refresh:")) { snprintf(saida, tam, "%s", t); chaveLimpar(saida); }
    }
    else chaveAcharLinha(t, d->rotulo, true, saida, tam);
    free(t);
    return saida[0] != 0;
}
static bool chaveGravarTexto(const char* caminho, const char* texto) {
    char pasta[300]; snprintf(pasta, sizeof(pasta), "%s", caminho);
    char* barra = strrchr(pasta, '/'); if (barra) { *barra = 0; remotoCriarPastas(pasta); }
    if (!texto[0]) { remove(caminho); return true; }
    FILE* f = fopen(caminho, "wb");
    if (!f) return false;
    fputs(texto, f);
    return fclose(f) == 0;
}
const char* chaveGravar(const ChaveDef* d, const char* valorBruto) {
    char valor[2048]; snprintf(valor, sizeof(valor), "%s", valorBruto ? valorBruto : "");
    chaveLimpar(valor);
    if (strchr(valor, '\n') || strchr(valor, '\r')) return "a chave não pode ter quebra de linha";
    if (valor[0] && !strcmp(d->id, "telegram_id") && strspn(valor, "0123456789") != strlen(valor)) return "o api_id do Telegram é só número";
    if (valor[0] && !strcmp(d->id, "telegram_hash") && (strlen(valor) != 32 || strspn(valor, "0123456789abcdefABCDEF") != 32)) return "o api_hash do Telegram tem 32 letras e números (0-9, a-f)";
    IaBuf novo = { NULL, 0, 0 };
    iaMais(&novo, "", 0);
    if (d->tipo == CH_ARQUIVO) { if (valor[0]) { iaTxt(&novo, valor); iaTxt(&novo, "\n"); } }
    else if (d->tipo == CH_LINHA) {
        char a[2][256] = { "", "" };
        for (int i = 0; i < CHAVES_FIXAS; i++) if (CHAVES[i].tipo == CH_LINHA && !strcmp(CHAVES[i].arquivo, d->arquivo)) chaveLer(&CHAVES[i], a[CHAVES[i].linha], 256);
        snprintf(a[d->linha], 256, "%s", valor);
        if (a[0][0] || a[1][0]) { iaTxt(&novo, a[0]); iaTxt(&novo, "\n"); iaTxt(&novo, a[1]); iaTxt(&novo, "\n"); }
    }
    else if (d->tipo == CH_ROTULO) {
        const char* rotulos[3] = { "Appkey:", "App secret:", "Token refresh:" };
        for (int r = 0; r < 3; r++) {
            char v[2048] = "";
            if (!strcmp(rotulos[r], d->rotulo)) snprintf(v, sizeof(v), "%s", valor);
            else for (int i = 0; i < CHAVES_FIXAS; i++) if (CHAVES[i].tipo == CH_ROTULO && !strcmp(CHAVES[i].rotulo, rotulos[r])) chaveLer(&CHAVES[i], v, sizeof(v));
            if (v[0]) { iaTxt(&novo, rotulos[r]); iaTxt(&novo, " "); iaTxt(&novo, v); iaTxt(&novo, "\n"); }
        }
    }
    else {
        char* t = chaveLerArquivo(d->arquivo);
        size_t ln = strlen(d->rotulo);
        bool trocou = false;
        for (const char* p = t; p && *p; ) {
            const char* fim = strchr(p, '\n'); size_t n = fim ? (size_t)(fim - p) : strlen(p);
            if (!strncmp(p, d->rotulo, ln) && p[ln] == '=') {
                if (valor[0] && !trocou) { iaTxt(&novo, d->rotulo); iaTxt(&novo, "="); iaTxt(&novo, valor); iaTxt(&novo, "\n"); }
                trocou = true;
            }
            else if (n > 0 && !(n == 1 && p[0] == '\r')) { iaMais(&novo, p, n); iaTxt(&novo, "\n"); }
            p = fim ? fim + 1 : NULL;
        }
        free(t);
        if (!trocou && valor[0]) { iaTxt(&novo, d->rotulo); iaTxt(&novo, "="); iaTxt(&novo, valor); iaTxt(&novo, "\n"); }
    }
    bool ok = chaveGravarTexto(d->arquivo, novo.d);
    free(novo.d);
    remotoLog("CHAVE %s %s", d->id, valor[0] ? "gravada" : "apagada");
    return ok ? NULL : "não consegui gravar o arquivo";
}
// Chaves extras (do usuário) ficam em chaves_api.txt como nome=valor, fora das reservadas.
static bool chaveReservada(const char* nome) {
    for (int i = 0; i < CHAVES_FIXAS; i++) if (CHAVES[i].tipo == CH_NOMEVALOR && !strcmp(CHAVES[i].arquivo, CH_API) && !strcmp(CHAVES[i].rotulo, nome)) return true;
    return !strcmp(nome, "chatgpt");   // virou conta de IA
}
static int chavesOutras(char nomes[][64], int max) {
    char* t = chaveLerArquivo(CH_API);
    int n = 0;
    for (const char* p = t; p && *p && n < max; ) {
        const char* fim = strchr(p, '\n'); if (!fim) fim = p + strlen(p);
        const char* igual = (const char*)memchr(p, '=', fim - p);
        if (igual && igual > p && igual - p < 63) {
            char nome[64]; memcpy(nome, p, igual - p); nome[igual - p] = 0;
            if (!chaveReservada(nome)) snprintf(nomes[n++], 64, "%s", nome);
        }
        p = *fim ? fim + 1 : NULL;
    }
    free(t);
    return n;
}
static bool chaveNomeValido(const char* s) {
    if (!s[0] || strlen(s) > 40) return false;
    for (const char* p = s; *p; p++) if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == '.')) return false;
    return !chaveReservada(s);
}
static ChaveDef chaveExtra(const char* nome) { ChaveDef d = { nome, "Chave", "", CH_NOMEVALOR, CH_API, nome, 0 }; return d; }
static void chaveFim(const char* v, char* fim, int tam) { size_t n = strlen(v); snprintf(fim, tam, "%s", n > 4 ? v + n - 4 : (n ? "****" : "")); }

static void chaveCampoJson(IaBuf* j, const ChaveDef* d, bool primeiro) {
    char v[2048], fim[8];
    chaveLer(d, v, sizeof(v)); chaveFim(v, fim, sizeof(fim));
    iaTxt(j, primeiro ? "{\"id\":\"" : ",{\"id\":\""); iaJsonTxt(j, d->id);
    iaTxt(j, "\",\"nome\":\""); iaJsonTxt(j, d->nome); iaTxt(j, "\",\"dica\":\""); iaJsonTxt(j, d->uso);
    iaTxt(j, "\",\"tem\":"); iaTxt(j, v[0] ? "true" : "false"); iaTxt(j, ",\"fim\":\""); iaJsonTxt(j, fim); iaTxt(j, "\"}");
}

static bool chavesRotas(RemotoConexao* c, const char* alvo, const char* query, long long tamCorpo, bool* tratou) {
    *tratou = true;
    if (!strcmp(alvo, "/chaves/lista")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        IaBuf j = { NULL, 0, 0 };
        iaTxt(&j, "{\"categorias\":[");
        for (int k = 0; k < N_CATEGORIAS; k++) {
            iaTxt(&j, k ? ",{\"id\":\"" : "{\"id\":\""); iaTxt(&j, CATEGORIAS[k][0]); iaTxt(&j, "\",\"nome\":\""); iaJsonTxt(&j, CATEGORIAS[k][1]); iaTxt(&j, "\",\"servicos\":[");
            bool primeiro = true;
            for (int s = 0; s < N_SERVICOS; s++) {
                if (strcmp(SERVICOS[s].categoria, CATEGORIAS[k][0])) continue;
                iaTxt(&j, primeiro ? "{\"id\":\"" : ",{\"id\":\""); primeiro = false;
                iaTxt(&j, SERVICOS[s].id); iaTxt(&j, "\",\"nome\":\""); iaJsonTxt(&j, SERVICOS[s].nome); iaTxt(&j, "\",\"uso\":\""); iaJsonTxt(&j, SERVICOS[s].uso); iaTxt(&j, "\",\"campos\":[");
                for (int f = 0; SERVICOS[s].campos[f]; f++) chaveCampoJson(&j, chaveAchar(SERVICOS[s].campos[f]), f == 0);
                iaTxt(&j, "]}");
            }
            if (!strcmp(CATEGORIAS[k][0], "outras")) {
                char outras[32][64]; int n = chavesOutras(outras, 32);
                for (int i = 0; i < n; i++) {
                    ChaveDef d = chaveExtra(outras[i]);
                    iaTxt(&j, primeiro ? "{\"id\":\"" : ",{\"id\":\""); primeiro = false;
                    iaJsonTxt(&j, outras[i]); iaTxt(&j, "\",\"nome\":\""); iaJsonTxt(&j, outras[i]); iaTxt(&j, "\",\"uso\":\"Chave extra (sua)\",\"extra\":true,\"campos\":[");
                    chaveCampoJson(&j, &d, true);
                    iaTxt(&j, "]}");
                }
            }
            iaTxt(&j, "]}");
        }
        iaTxt(&j, "],\"contas\":");
        pthread_mutex_lock(&ia.trava); iaCarregarTravado(); iaContasJson(&j); pthread_mutex_unlock(&ia.trava);
        iaTxt(&j, "}");
        bool ok = remotoResponder(c->fd, "200 OK", "application/json; charset=utf-8", j.d, j.n);
        free(j.d);
        return ok;
    }
    if (!strcmp(alvo, "/chaves/salvar")) {
        if (tamCorpo > 4000) return false;
        char corpo[4100]; if (!remotoLerCorpo(c, tamCorpo, corpo, sizeof(corpo))) return false;
        char id[64] = ""; remotoParam(query, "id", id, sizeof(id));
        const ChaveDef* d = chaveAchar(id);
        ChaveDef extra;
        if (!d) {
            if (!chaveNomeValido(id)) return remotoJson(c->fd, "200 OK", "{\"ok\":false,\"erro\":\"nome inválido: use letras, números, _ - .\"}");
            extra = chaveExtra(id); d = &extra;
        }
        const char* erro = chaveGravar(d, corpo);
        if (!erro) return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
        IaBuf j = { NULL, 0, 0 }; iaTxt(&j, "{\"ok\":false,\"erro\":\""); iaJsonTxt(&j, erro); iaTxt(&j, "\"}");
        bool ok = remotoJson(c->fd, "200 OK", j.d); free(j.d);
        return ok;
    }
    *tratou = false;
    return true;
}

// =========================================================================
// TOUCHPAD, TELA DO PS4 NO NAVEGADOR E TELA DO PC NO PS4
// =========================================================================
// Touchpad: até dois dedos, em coordenadas do touchpad do DS4 (0..1919 x 0..941).
static int remotoToqueN = 0;
static uint16_t remotoToqueXY[4];
static uint64_t remotoToqueEm = 0;

int remotoToque(uint16_t xy[4]) {
    pthread_mutex_lock(&remotoTrava);
    int n = remotoToqueN;
    if (n && remotoAgoraMs() - remotoToqueEm > 600) n = remotoToqueN = 0;   // dedo "esquecido" pela rede
    memcpy(xy, remotoToqueXY, sizeof(remotoToqueXY));
    pthread_mutex_unlock(&remotoTrava);
    return n;
}

// Quadro mais novo da tela do PS4 em JPEG. Quem captura (lado do PS4) só trabalha com alguém vendo.
static uint8_t* telaJpeg = NULL;
static int telaTam = 0;
static unsigned telaSeq = 0;
volatile int remotoEspectadores = 0;
static volatile int remotoTelaEscala = 3;   // 3 = rápida (640x360), 2 = nítida (960x540)
static volatile uint64_t remotoUltimoPedidoTela = 0;

void remotoPublicarQuadro(const uint8_t* dados, int tam) {
    uint8_t* copia = (uint8_t*)malloc(tam);
    if (!copia) return;
    memcpy(copia, dados, tam);
    pthread_mutex_lock(&remotoTrava);
    free(telaJpeg);
    telaJpeg = copia; telaTam = tam; telaSeq++;
    pthread_mutex_unlock(&remotoTrava);
}

bool remotoAlguemVendo() { return remotoEspectadores > 0 || remotoAgoraMs() - remotoUltimoPedidoTela < 3000; }

// Som do PS4 para o navegador: a saída do app (48 kHz estéreo) entra aqui já misturada,
// cai para 24 kHz e fica num anel de 2 s. Cada ouvinte segue com a própria posição.
#define SOM_TAXA 24000
#define SOM_ANEL (SOM_TAXA * 2)          // em quadros estéreo
static int16_t remotoSomAnel[SOM_ANEL * 2];
static volatile uint64_t remotoSomEscrito = 0;   // quadros já escritos desde o começo
volatile int remotoOuvintes = 0;

void remotoSomEmpurrar(const int16_t* s48, int amostras) {
    if (remotoOuvintes <= 0) return;
    uint64_t w = remotoSomEscrito;
    for (int i = 0; i + 1 < amostras; i += 2) {
        int k = (int)(w % SOM_ANEL);
        remotoSomAnel[k * 2] = (int16_t)((s48[i * 2] + s48[i * 2 + 2]) / 2);
        remotoSomAnel[k * 2 + 1] = (int16_t)((s48[i * 2 + 1] + s48[i * 2 + 3]) / 2);
        w++;
    }
    __sync_synchronize();
    remotoSomEscrito = w;
}

// Copia do quadro mais novo que 'depoisDe'; espera até 'esperaMs'.
static uint8_t* telaPegar(unsigned depoisDe, int esperaMs, int* tam, unsigned* seq) {
    uint64_t ini = remotoAgoraMs();
    for (;;) {
        pthread_mutex_lock(&remotoTrava);
        if (telaJpeg && telaSeq != depoisDe) {
            uint8_t* c = (uint8_t*)malloc(telaTam);
            if (c) { memcpy(c, telaJpeg, telaTam); *tam = telaTam; *seq = telaSeq; }
            pthread_mutex_unlock(&remotoTrava);
            return c;
        }
        pthread_mutex_unlock(&remotoTrava);
        if ((int)(remotoAgoraMs() - ini) >= esperaMs) return NULL;
        remotoDormir(10);
    }
}

static void remotoComecarCaptura();
static bool remotoEspelhoQuadro(const uint8_t* jpeg, int tam);
static void remotoEspelhoParar();

// ---- PC controlado pelo PS4 (FTP > [PC]) ----
// O HyperNeivaPC.exe só escuta em 127.0.0.1: ninguém da rede fala com ele. Por isso é ele que
// busca aqui o que o PS4 quer (GET /pc/pedidos, que espera até 700 ms por um pedido) e devolve
// o resultado (POST /pc/resposta). Os pedidos só nascem nas telas do PS4.
static pthread_mutex_t pcTrava = PTHREAD_MUTEX_INITIALIZER;
static char pcFila[16384];
static int pcFilaN = 0;
static int pcDx = 0, pcDy = 0, pcRoda = 0;
static volatile uint64_t pcVistoMs = 0;
static char pcNome[64] = "", pcEstado[160] = "";
static char* pcResposta = NULL;
static int pcRespostaId = 0, pcUltimoId = 0;

void pcPedir(const char* linha) {
    pthread_mutex_lock(&pcTrava);
    int n = (int)strlen(linha);
    if (pcFilaN + n + 2 < (int)sizeof(pcFila)) { memcpy(pcFila + pcFilaN, linha, n); pcFilaN += n; pcFila[pcFilaN++] = '\n'; pcFila[pcFilaN] = 0; }
    pthread_mutex_unlock(&pcTrava);
}
bool pcOnline() { return pcVistoMs && remotoAgoraMs() - pcVistoMs < 4000; }
void pcMover(int dx, int dy, int roda) {
    pthread_mutex_lock(&pcTrava);
    pcDx += dx; pcDy += dy; pcRoda += roda;
    pthread_mutex_unlock(&pcTrava);
}
int pcListar(const char* pasta) {
    pthread_mutex_lock(&pcTrava);
    int id = ++pcUltimoId;
    pthread_mutex_unlock(&pcTrava);
    char l[700]; snprintf(l, sizeof(l), "listar\t%d\t%s", id, pasta);
    pcPedir(l);
    return id;
}
char* pcPegarResposta(int id) {
    pthread_mutex_lock(&pcTrava);
    char* r = NULL;
    if (pcResposta && pcRespostaId == id) { r = pcResposta; pcResposta = NULL; }
    pthread_mutex_unlock(&pcTrava);
    return r;
}
void pcInfo(char* nome, int tn, char* estado, int te) {
    pthread_mutex_lock(&pcTrava);
    snprintf(nome, tn, "%s", pcNome); snprintf(estado, te, "%s", pcEstado);
    pthread_mutex_unlock(&pcTrava);
}

static bool pcRotas(RemotoConexao* c, const char* alvo, const char* query, long long tamCorpo, bool* tratou) {
    *tratou = true;
    if (!strcmp(alvo, "/pc/pedidos")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        char nome[64] = "", estado[160] = "";
        remotoParam(query, "pc", nome, sizeof(nome));
        remotoParam(query, "estado", estado, sizeof(estado));
        pthread_mutex_lock(&pcTrava);
        if (!pcVistoMs || remotoAgoraMs() - pcVistoMs > 4000) remotoLog("PC conectado: %s", nome);
        pcVistoMs = remotoAgoraMs();
        if (nome[0]) snprintf(pcNome, sizeof(pcNome), "%s", nome);
        snprintf(pcEstado, sizeof(pcEstado), "%s", estado);
        pthread_mutex_unlock(&pcTrava);
        char* corpo = (char*)malloc(sizeof(pcFila) + 128);
        if (!corpo) return false;
        int n = 0;
        uint64_t ini = remotoAgoraMs();
        for (;;) {
            pthread_mutex_lock(&pcTrava);
            if (pcDx || pcDy) n += snprintf(corpo + n, 64, "mover\t%d\t%d\n", pcDx, pcDy);
            if (pcRoda) n += snprintf(corpo + n, 64, "rolar\t%d\n", pcRoda);
            pcDx = pcDy = pcRoda = 0;
            if (pcFilaN) { memcpy(corpo + n, pcFila, pcFilaN); n += pcFilaN; pcFilaN = 0; }
            pthread_mutex_unlock(&pcTrava);
            if (n || remotoAgoraMs() - ini > 700) break;
            remotoDormir(8);
        }
        corpo[n] = 0;
        bool ok = remotoResponder(c->fd, "200 OK", "text/plain; charset=utf-8", corpo, n);
        free(corpo);
        pcVistoMs = remotoAgoraMs();
        return ok;
    }
    if (!strcmp(alvo, "/pc/resposta")) {
        if (tamCorpo < 0 || tamCorpo > 1024 * 1024) return false;
        char* corpo = (char*)malloc((size_t)tamCorpo + 1);
        if (!corpo) return false;
        int jaTem = c->len < tamCorpo ? c->len : (int)tamCorpo;
        memcpy(corpo, c->buf, jaTem); remotoConsumir(c, jaTem);
        if (jaTem < tamCorpo && !remotoReceberN(c->fd, (uint8_t*)corpo + jaTem, (int)tamCorpo - jaTem)) { free(corpo); return false; }
        corpo[tamCorpo] = 0;
        char v[16] = ""; remotoParam(query, "id", v, sizeof(v));
        pthread_mutex_lock(&pcTrava);
        free(pcResposta); pcResposta = corpo; pcRespostaId = atoi(v);
        pthread_mutex_unlock(&pcTrava);
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    *tratou = false;
    return true;
}

static bool remotoRotasTela(RemotoConexao* c, const char* alvo, const char* query, long long tamCorpo, bool* tratou) {
    if (!strncmp(alvo, "/ia/", 4)) return iaRotas(c, alvo, query, tamCorpo, tratou);
    if (!strncmp(alvo, "/pc/", 4)) return pcRotas(c, alvo, query, tamCorpo, tratou);
    if (!strncmp(alvo, "/chaves/", 8)) return chavesRotas(c, alvo, query, tamCorpo, tratou);
    *tratou = true;
    if (!strcmp(alvo, "/tp")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        char v[96] = ""; int d[5] = { 0, 0, 0, 0, 0 };
        remotoParam(query, "d", v, sizeof(v));
        sscanf(v, "%d,%d,%d,%d,%d", &d[0], &d[1], &d[2], &d[3], &d[4]);
        pthread_mutex_lock(&remotoTrava);
        remotoToqueN = d[0] < 0 ? 0 : (d[0] > 2 ? 2 : d[0]);
        for (int i = 0; i < 4; i++) {
            int lim = (i % 2) ? 941 : 1919;
            remotoToqueXY[i] = (uint16_t)(d[i + 1] < 0 ? 0 : (d[i + 1] > lim ? lim : d[i + 1]));
        }
        remotoToqueEm = remotoAgoraMs();
        pthread_mutex_unlock(&remotoTrava);
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    if (!strcmp(alvo, "/tela.jpg") || !strcmp(alvo, "/tela.mjpg")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        remotoUltimoPedidoTela = remotoAgoraMs();
        { char q[16] = ""; remotoParam(query, "q", q, sizeof(q)); remotoTelaEscala = !strcmp(q, "nitida") ? 2 : 3; }
        remotoComecarCaptura();
        int tam = 0; unsigned seq = 0;
        if (!strcmp(alvo, "/tela.jpg")) {
            uint8_t* q = telaPegar(0, 3000, &tam, &seq);
            if (!q) return remotoJson(c->fd, "503 Service Unavailable", "{\"ok\":false}");
            bool ok = remotoResponder(c->fd, "200 OK", "image/jpeg", (const char*)q, tam);
            free(q);
            return ok;
        }
        // MJPEG: um JPEG atrás do outro na mesma resposta; o <img> do navegador mostra como vídeo.
        const char* cab = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=hnquadro\r\n"
            "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
        if (!remotoEnviarTudo(c->fd, cab, strlen(cab))) return false;
        __sync_fetch_and_add(&remotoEspectadores, 1);
        remotoLog("TELA: espectador entrou (%d)", remotoEspectadores);
        bool ok = true;
        while (ok) {
            uint8_t* q = telaPegar(seq, 5000, &tam, &seq);
            if (!q) continue;
            char parte[160];
            int n = snprintf(parte, sizeof(parte), "--hnquadro\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", tam);
            ok = remotoEnviarTudo(c->fd, parte, n) && remotoEnviarTudo(c->fd, q, tam) && remotoEnviarTudo(c->fd, "\r\n", 2);
            free(q);
        }
        __sync_fetch_and_sub(&remotoEspectadores, 1);
        remotoLog("TELA: espectador saiu (%d)", remotoEspectadores);
        return false;
    }
    if (!strcmp(alvo, "/tela.pcm")) {
        // PCM cru (16 bits, estéreo, 24 kHz) sem fim; o navegador toca com WebAudio.
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        const char* cab = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nX-Taxa: 24000\r\n"
            "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
        if (!remotoEnviarTudo(c->fd, cab, strlen(cab))) return false;
        __sync_fetch_and_add(&remotoOuvintes, 1);
        remotoLog("SOM: ouvinte entrou (%d)", remotoOuvintes);
        int16_t* bloco = (int16_t*)malloc(SOM_TAXA / 5 * 4);   // até 200 ms por envio
        uint64_t pos = remotoSomEscrito;
        uint64_t silencioDesde = remotoAgoraMs();
        bool ok = bloco != NULL;
        while (ok) {
            uint64_t w = remotoSomEscrito;
            if (w - pos > SOM_ANEL - SOM_TAXA / 2) pos = w;   // ficou para trás: pula para o agora
            int n = (int)(w - pos);
            if (n > SOM_TAXA / 5) n = SOM_TAXA / 5;
            if (n <= 0) {
                // Som parado (app sem saída de áudio): manda silêncio para o navegador não travar.
                if (remotoAgoraMs() - silencioDesde < 500) { remotoDormir(20); continue; }
                n = SOM_TAXA / 10;
                memset(bloco, 0, n * 4);
                silencioDesde = remotoAgoraMs();
                ok = remotoEnviarTudo(c->fd, bloco, n * 4);
                continue;
            }
            for (int i = 0; i < n; i++) {
                int k = (int)((pos + i) % SOM_ANEL);
                bloco[i * 2] = remotoSomAnel[k * 2]; bloco[i * 2 + 1] = remotoSomAnel[k * 2 + 1];
            }
            pos += n;
            silencioDesde = remotoAgoraMs();
            ok = remotoEnviarTudo(c->fd, bloco, n * 4);
            remotoDormir(20);
        }
        free(bloco);
        __sync_fetch_and_sub(&remotoOuvintes, 1);
        remotoLog("SOM: ouvinte saiu (%d)", remotoOuvintes);
        return false;
    }
    if (!strcmp(alvo, "/espelho/quadro")) {
        if (tamCorpo <= 0 || tamCorpo > 8 * 1024 * 1024) return false;
        uint8_t* jpeg = (uint8_t*)malloc((size_t)tamCorpo);
        if (!jpeg) return false;
        int jaTem = c->len < tamCorpo ? c->len : (int)tamCorpo;
        memcpy(jpeg, c->buf, jaTem); remotoConsumir(c, jaTem);
        bool ok = jaTem == tamCorpo || remotoReceberN(c->fd, jpeg + jaTem, (int)tamCorpo - jaTem);
        bool aceito = ok && remotoEspelhoQuadro(jpeg, (int)tamCorpo);
        free(jpeg);
        if (!ok) return false;
        // 410: o usuário fechou a tela do PC no PS4 (Bolinha); o programa do PC para de mandar.
        return aceito ? remotoJson(c->fd, "200 OK", "{\"ok\":true}") : remotoJson(c->fd, "410 Gone", "{\"ok\":false}");
    }
    if (!strcmp(alvo, "/espelho/parar")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        remotoEspelhoParar();
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    if (!strcmp(alvo, "/HyperNeivaPC.exe")) {
        char lixo[64]; if (!remotoLerCorpo(c, tamCorpo, lixo, sizeof(lixo))) return false;
        return remotoServirArquivo(c->fd, REMOTO_PASTA "/../HyperNeivaPC.exe", "GET", true);
    }
    *tratou = false;
    return true;
}

void remotoIniciarDlna() {
#ifdef HN_PC_SEM_SSDP
    return;
#endif
    pthread_t t;
    if (pthread_create(&t, NULL, remotoThreadSsdp, NULL) == 0) pthread_detach(t);
}

// Trata um pedido. Devolve false para fechar a conexão.
static bool remotoTratarPedido(RemotoConexao* c) {
    char* fimCab;
    while (!(fimCab = strstr(c->buf, "\r\n\r\n"))) {
        int n = remotoReceber(c);
        if (n <= 0) return false;
    }
    int tamCab = (int)(fimCab - c->buf) + 4;
    char metodo[16] = "", alvo[1024] = "";
    sscanf(c->buf, "%15s %1023s", metodo, alvo);
    long long tamCorpo = 0;
    for (char* p = c->buf; p < fimCab; p++) {
        if ((p == c->buf || p[-1] == '\n') && strncasecmp(p, "Content-Length:", 15) == 0) { tamCorpo = strtoll(p + 15, NULL, 10); break; }
    }
    if (tamCorpo < 0) return false;
    // Clientes como o curl esperam 1 s pelo "100 Continue" antes de mandar o corpo.
    bool continuar = false;
    for (char* p = c->buf; p < fimCab; p++) {
        if ((p == c->buf || p[-1] == '\n') && strncasecmp(p, "Expect:", 7) == 0) { continuar = true; break; }
    }
    char cab[8192];
    int tamCopia = tamCab < (int)sizeof(cab) - 1 ? tamCab : (int)sizeof(cab) - 1;
    memcpy(cab, c->buf, tamCopia); cab[tamCopia] = '\0';
    remotoConsumir(c, tamCab);
    if (continuar && tamCorpo > 0 && c->len == 0) remotoEnviarTudo(c->fd, "HTTP/1.1 100 Continue\r\n\r\n", 25);

    char* query = strchr(alvo, '?');
    if (query) *query++ = '\0'; else query = (char*)"";

    if (!remotoJaConectou && strncmp(alvo, "/dlna/", 6) != 0) { remotoJaConectou = true; remotoAviso("CELULAR CONECTADO AO HYPER NEIVA"); }

    bool tratou = false;
    bool resultado = remotoRotasExtras(c, metodo, alvo, query, cab, tamCorpo, &tratou);
    if (tratou) return resultado;

    // Envio de arquivo: o corpo vai direto para o disco.
    if (strcmp(alvo, "/enviar") == 0) {
        char bruto[512] = "", nome[200], erro[160] = "";
        remotoParam(query, "n", bruto, sizeof(bruto));
        remotoNomeSeguro(bruto, nome, sizeof(nome));
        if (tamCorpo <= 0) return remotoJson(c->fd, "400 Bad Request", "{\"ok\":false,\"erro\":\"arquivo vazio\"}");
        if (!remotoReceberArquivo(c, tamCorpo, REMOTO_PASTA, nome, erro, sizeof(erro))) return false;
        if (erro[0]) {
            char json[300], e2[200]; remotoJsonTexto(erro, e2, sizeof(e2));
            snprintf(json, sizeof(json), "{\"ok\":false,\"erro\":\"%s\"}", e2);
            remotoAviso("CELULAR: FALHA AO GRAVAR O ARQUIVO");
            return remotoJson(c->fd, "500 Internal Server Error", json);
        }
        int tipo = remotoTipoArquivo(nome);
        char caminho[512]; snprintf(caminho, sizeof(caminho), "%s/%s", REMOTO_PASTA, nome);
        if (tipo != 2) remotoPedirMidia(caminho);
        else remotoAviso("CELULAR: ARQUIVO SALVO EM TRANSMITIDOS");
        char json[128];
        snprintf(json, sizeof(json), "{\"ok\":true,\"tipo\":\"%s\"}", tipo == 0 ? "imagem" : (tipo == 1 ? "video" : "outro"));
        return remotoJson(c->fd, "200 OK", json);
    }

    // Os outros pedidos têm corpo pequeno (texto do teclado, no máximo).
    if (tamCorpo > (long long)sizeof(c->buf) - 1) return false;
    while (c->len < tamCorpo) if (remotoReceber(c) <= 0) return false;
    char corpo[sizeof(c->buf)];
    memcpy(corpo, c->buf, (size_t)tamCorpo); corpo[tamCorpo] = '\0';
    remotoConsumir(c, (int)tamCorpo);

    if (strcmp(alvo, "/b") == 0) {
        char m[24] = "0", t[24] = "0";
        remotoParam(query, "m", m, sizeof(m)); remotoParam(query, "t", t, sizeof(t));
        remotoDefinirBotoes((uint32_t)strtoul(m, NULL, 10), (uint32_t)strtoul(t, NULL, 10));
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    if (strcmp(alvo, "/a") == 0) {
        char v[64] = ""; int a[4] = { 128, 128, 128, 128 };
        remotoParam(query, "v", v, sizeof(v));
        sscanf(v, "%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3]);
        remotoDefinirAnalog(a);
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    if (strcmp(alvo, "/t") == 0) {
        if (!remotoTecladoAberto() || !corpo[0]) return remotoJson(c->fd, "200 OK", "{\"ok\":false}");
        pthread_mutex_lock(&remotoTrava);
        size_t lenTexto = strlen(corpo);
        if (lenTexto > sizeof(remotoTexto) - 1) lenTexto = sizeof(remotoTexto) - 1;
        memcpy(remotoTexto, corpo, lenTexto); remotoTexto[lenTexto] = '\0';
        remotoTemTexto = true;
        pthread_mutex_unlock(&remotoTrava);
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    if (strcmp(alvo, "/estado") == 0) {
        return remotoJson(c->fd, "200 OK", remotoTecladoAberto() ? "{\"teclado\":true}" : "{\"teclado\":false}");
    }
    if (strcmp(alvo, "/lista") == 0) { remotoResponderLista(c->fd); return true; }
    if (strcmp(alvo, "/abrir") == 0 || strcmp(alvo, "/apagar") == 0) {
        char bruto[512] = "", nome[200], caminho[512];
        remotoParam(query, "n", bruto, sizeof(bruto));
        remotoNomeSeguro(bruto, nome, sizeof(nome));
        snprintf(caminho, sizeof(caminho), "%s/%s", REMOTO_PASTA, nome);
        struct stat st;
        if (stat(caminho, &st) != 0) return remotoJson(c->fd, "404 Not Found", "{\"ok\":false,\"erro\":\"nao existe\"}");
        if (alvo[1] == 'a' && alvo[2] == 'p') { remove(caminho); return remotoJson(c->fd, "200 OK", "{\"ok\":true}"); }
        if (remotoTipoArquivo(nome) == 2) return remotoJson(c->fd, "200 OK", "{\"ok\":false,\"erro\":\"tipo nao abre\"}");
        remotoPedirMidia(caminho);
        return remotoJson(c->fd, "200 OK", "{\"ok\":true}");
    }
    if (strcmp(alvo, "/") == 0 || strcmp(alvo, "/index.html") == 0) {
        return remotoResponder(c->fd, "200 OK", "text/html; charset=utf-8", REMOTO_PAGINA, sizeof(REMOTO_PAGINA) - 1);
    }
    if (strcmp(alvo, "/favicon.ico") == 0) return remotoResponder(c->fd, "204 No Content", "image/x-icon", "", 0);
    return remotoJson(c->fd, "404 Not Found", "{\"ok\":false}");
}

static void* remotoThreadConexao(void* arg) {
    RemotoConexao* c = (RemotoConexao*)arg;
    int sim = 1;
    setsockopt(c->fd, IPPROTO_TCP, 1 /* TCP_NODELAY */, (void*)&sim, sizeof(sim));
    // Conexão parada fecha sozinha: libera a thread se o celular sumir da rede.
    remotoPrepararSocket(c->fd, 30);
    while (remotoTratarPedido(c)) {}
    close(c->fd);
    free(c);
    __sync_fetch_and_sub(&remotoConexoes, 1);
    return NULL;
}

static void* remotoThreadServidor(void*) {
    int servidor = -1;
    for (int tentativa = 0; tentativa < 30 && servidor < 0; tentativa++) {
        servidor = socket(AF_INET, SOCK_STREAM, 0);
        int sim = 1;
        setsockopt(servidor, SOL_SOCKET, HN_SO_REUSEADDR, (void*)&sim, sizeof(sim));
        struct sockaddr_in end; memset(&end, 0, sizeof(end));
        end.sin_family = AF_INET; end.sin_addr.s_addr = htonl(REMOTO_ESCUTA); end.sin_port = htons(REMOTO_PORTA);
        if (servidor < 0 || bind(servidor, (struct sockaddr*)&end, sizeof(end)) != 0 || listen(servidor, 16) != 0) {
            remotoLog("SERVIDOR bind/listen falhou errno=%d (tentativa %d)", errno, tentativa);
            if (servidor >= 0) close(servidor);
            servidor = -1;
            remotoDormir(2000);
        }
    }
    if (servidor < 0) { remotoLigado = false; return NULL; }
    remotoLog("SERVIDOR ouvindo na porta %d", REMOTO_PORTA);
    for (;;) {
        int fd = accept(servidor, NULL, NULL);
        if (fd < 0) { if (errno != EINTR) remotoDormir(100); continue; }
        if (remotoConexoes >= 24) { close(fd); continue; }
        RemotoConexao* c = (RemotoConexao*)malloc(sizeof(RemotoConexao));
        if (!c) { close(fd); continue; }
        c->fd = fd; c->len = 0; c->buf[0] = '\0';
        __sync_fetch_and_add(&remotoConexoes, 1);
        pthread_t t;
        pthread_attr_t at; pthread_attr_init(&at);
        pthread_attr_setstacksize(&at, 256 * 1024);
        pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&t, &at, remotoThreadConexao, c) != 0) { close(fd); free(c); __sync_fetch_and_sub(&remotoConexoes, 1); }
        pthread_attr_destroy(&at);
    }
    return NULL;
}

void remotoIniciar() {
    if (remotoLigado) return;
    remotoLigado = true;
    mkdir(REMOTO_PASTA, 0777);
    remotoIniciarDlna();
    pthread_t t;
    if (pthread_create(&t, NULL, remotoThreadServidor, NULL) == 0) pthread_detach(t);
    else remotoLigado = false;
}

// IP desta máquina na rede local (sem mandar pacote: o connect de UDP só escolhe a rota).
bool remotoMeuIp(char* saida, int tamanho) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return false;
    struct sockaddr_in fora; memset(&fora, 0, sizeof(fora));
    fora.sin_family = AF_INET; fora.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &fora.sin_addr);
    bool ok = false;
    if (connect(s, (struct sockaddr*)&fora, sizeof(fora)) == 0) {
        struct sockaddr_in eu; socklen_t len = sizeof(eu);
        if (getsockname(s, (struct sockaddr*)&eu, &len) == 0 && eu.sin_addr.s_addr != 0) {
            ok = inet_ntop(AF_INET, &eu.sin_addr, saida, tamanho) != NULL;
        }
    }
    close(s);
    return ok;
}
// REMOTO-FIM

#ifndef HN_PC
#include <stdarg.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/AudioIn.h>
#include "menu.h"
#include "ImeDialog.h"
#include "stb_image.h"

extern char msgStatus[128];
extern int msgTimer;

static uint64_t remotoAgoraMs() { return sceKernelGetProcessTime() / 1000; }
static void remotoDormir(int ms) { sceKernelUsleep((unsigned)ms * 1000); }

extern bool videoRodando, video_pausado;
extern double video_time_current, video_time_total;
static double remotoTempoAtual() { return videoRodando ? video_time_current : 0.0; }
static double remotoTempoTotal() { return videoRodando ? video_time_total : 0.0; }
static bool remotoVideoTocando() { return videoRodando; }
static bool remotoVideoPausado() { return video_pausado; }

// ---- Leitura do player (FFmpeg) a partir da fonte remota ----
extern "C" {
#include <libavformat/avformat.h>
}
static AVIOContext* remotoAvio = NULL;
static unsigned remotoAvioFonte = 0;

static int remotoAvioLer(void* opaco, uint8_t* destino, int tamanho) {
    int r = remotoFonteLer((unsigned)(uintptr_t)opaco, destino, tamanho);
    if (r == 0) return AVERROR_EOF;
    if (r < 0) return AVERROR(EIO);
    return r;
}

static int64_t remotoAvioSaltar(void* opaco, int64_t posicao, int origem) {
    unsigned id = (unsigned)(uintptr_t)opaco;
    if (origem & AVSEEK_SIZE) return remotoFonteTamanho(id);
    return remotoFonteSaltar(id, posicao, origem & ~AVSEEK_FORCE);
}

// Chamadas de telegramAbrirFluxo / telegramPararFluxo / telegramFecharFluxo (extra.cpp),
// que o player já usa para vídeo que ainda está chegando.
AVIOContext* remotoAbrirFluxo(const char* caminho) {
    if (strncmp(caminho, "remoto://", 9) != 0) return NULL;
    unsigned id = remotoFonteIdAtual();
    if (!id) return NULL;
    if (remotoAvio) { av_freep(&remotoAvio->buffer); avio_context_free(&remotoAvio); }
    unsigned char* memoria = (unsigned char*)av_malloc(65536);
    if (!memoria) return NULL;
    remotoAvio = avio_alloc_context(memoria, 65536, 0, (void*)(uintptr_t)id, remotoAvioLer, NULL, remotoAvioSaltar);
    if (!remotoAvio) { av_free(memoria); return NULL; }
    remotoAvioFonte = id;
    remotoLog("PLAYER abriu fonte %u: %s", id, caminho);
    return remotoAvio;
}

void remotoPararFluxo() { if (remotoAvio) remotoFontePararLeitura(remotoAvioFonte); }

void remotoFecharFluxo() {
    if (!remotoAvio) return;
    av_freep(&remotoAvio->buffer);
    avio_context_free(&remotoAvio);
    remotoFonteFecharSe(remotoAvioFonte);
    remotoAvioFonte = 0;
}

static void remotoAviso(const char* texto) {
    snprintf(msgStatus, sizeof(msgStatus), "%s", texto);
    msgTimer = 80;
}

static void remotoLog(const char* formato, ...) {
    FILE* f = fopen("/data/HyperNeiva/remoto_log.txt", "a");
    if (!f) return;
    va_list args; va_start(args, formato);
    vfprintf(f, formato, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

// ---- Teclado do sistema: guarda o campo aberto para entregar o texto do celular ----
static pthread_mutex_t imeTrava = PTHREAD_MUTEX_INITIALIZER;
static uint16_t* imeBuffer = NULL;
static uint32_t imeLimite = 0;
static volatile bool imeAtivo = false;        // Init deu certo e o resultado ainda não foi lido
static volatile bool imeAberto = false;       // última leitura do estado: RUNNING
static bool imeEntregando = false;            // Abort pedido, texto esperando o fim do diálogo
static bool imeForcarOk = false;              // próximo GetResult devolve OK
static uint16_t imeTextoPendente[2048];
static int imeTamPendente = 0;

static bool remotoTecladoAberto() { return imeAtivo && imeAberto && !imeEntregando; }

extern "C" int32_t hnImeInit(const OrbisImeDialogSetting* param, OrbisImeSettingsExtended* extendido) {
    int32_t rc = sceImeDialogInit(param, extendido);
    pthread_mutex_lock(&imeTrava);
    if (rc >= 0 && param) {
        imeBuffer = (uint16_t*)param->inputTextBuffer;
        imeLimite = param->maxTextLength;
        imeAtivo = true; imeAberto = true;
        imeEntregando = false; imeForcarOk = false;
    }
    pthread_mutex_unlock(&imeTrava);
    return rc;
}

extern "C" OrbisDialogStatus hnImeStatus() {
    OrbisDialogStatus real = sceImeDialogGetStatus();
    pthread_mutex_lock(&imeTrava);
    imeAberto = (real == ORBIS_DIALOG_STATUS_RUNNING);
    if (imeEntregando) {
        if (real == ORBIS_DIALOG_STATUS_RUNNING) { pthread_mutex_unlock(&imeTrava); return ORBIS_DIALOG_STATUS_RUNNING; }
        // O diálogo fechou: o texto do celular entra no campo como se tivesse sido digitado.
        if (imeBuffer) {
            int n = imeTamPendente;
            if (imeLimite > 0 && n > (int)imeLimite) n = (int)imeLimite;
            memcpy(imeBuffer, imeTextoPendente, n * sizeof(uint16_t));
            imeBuffer[n] = 0;
        }
        imeEntregando = false;
        imeForcarOk = true;
    }
    pthread_mutex_unlock(&imeTrava);
    return real;
}

extern "C" int32_t hnImeResult(OrbisDialogResult* resultado) {
    int32_t rc = sceImeDialogGetResult(resultado);
    pthread_mutex_lock(&imeTrava);
    if (imeForcarOk && resultado) { resultado->endstatus = ORBIS_DIALOG_OK; rc = 0; }
    imeForcarOk = false;
    imeAtivo = false; imeAberto = false;
    pthread_mutex_unlock(&imeTrava);
    return rc;
}

// UTF-8 para UTF-16, que é o que o teclado do PS4 grava no campo.
static int remotoUtf8ParaUtf16(const char* s, uint16_t* saida, int maximo) {
    int n = 0;
    const unsigned char* p = (const unsigned char*)s;
    while (*p && n < maximo - 2) {
        uint32_t cp; int extra;
        if (*p < 0x80) { cp = *p; extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
        else { p++; continue; }
        p++;
        bool ok = true;
        for (int i = 0; i < extra; i++) { if ((*p & 0xC0) != 0x80) { ok = false; break; } cp = (cp << 6) | (*p++ & 0x3F); }
        if (!ok) continue;
        if (cp == '\r') continue;
        if (cp >= 0x10000) { cp -= 0x10000; saida[n++] = (uint16_t)(0xD800 + (cp >> 10)); saida[n++] = (uint16_t)(0xDC00 + (cp & 0x3FF)); }
        else saida[n++] = (uint16_t)cp;
    }
    saida[n] = 0;
    return n;
}

static void remotoEntregarTexto(const char* texto) {
    pthread_mutex_lock(&imeTrava);
    bool pode = imeAtivo && imeAberto && !imeEntregando;
    if (pode) {
        imeTamPendente = remotoUtf8ParaUtf16(texto, imeTextoPendente, 2048);
        imeEntregando = true;
    }
    pthread_mutex_unlock(&imeTrava);
    if (!pode) { remotoAviso("CELULAR: ABRA UM CAMPO DE TEXTO NO PS4"); return; }
    // Fecha o teclado do sistema; o texto entra quando o estado virar "terminado".
    int rc = ((int (*)())sceImeDialogAbort)();
    remotoLog("TEXTO %d letras, abort rc=0x%08X", imeTamPendente, (unsigned)rc);
    if (rc < 0) {
        pthread_mutex_lock(&imeTrava); imeEntregando = false; pthread_mutex_unlock(&imeTrava);
        remotoAviso("CELULAR: O TECLADO DO PS4 NAO ACEITOU O TEXTO");
    }
}

// ---- Controle: botões do celular somados aos do controle físico ----
// ---- QR do link na tela (canto de baixo): nas telas de FTP ou com L3+R3 ----
#include "remoto_qr.h"
static int remotoQrModo = 1;      // 1 = automático (telas de FTP), 2 = sempre, 0 = escondido

// O QR aparece nas telas de FTP (onde o celular e o PC entram em jogo); L3 + R3 esconde ou mostra.
static bool remotoQrVisivel() {
    extern MenuLevel menuAtual;
    if (!remotoLigado) return false;
    if (remotoQrModo == 2) return true;
    if (remotoQrModo == 0) return false;
    return menuAtual == MENU_BAIXAR_FTP_SERVIDORES || menuAtual == MENU_BAIXAR_FTP_EDITAR_SERVIDOR || menuAtual == MENU_BAIXAR_FTP_LISTA ||
           menuAtual == MENU_BAIXAR_FTP_UPLOAD_RAIZES || menuAtual == MENU_BAIXAR_FTP_UPLOAD;
}

void remotoDesenharQr(uint32_t* p) {
    extern void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);
    if (!p || !remotoQrVisivel()) return;
    static char url[64] = "";
    static uint8_t mod[QR_MAX_LADO * QR_MAX_LADO];
    static int lado = 0;
    static uint64_t ultimaVez = 0;
    uint64_t agora = remotoAgoraMs();
    if (!lado || agora - ultimaVez > 10000) {
        ultimaVez = agora;
        char ip[32], novo[64];
        if (remotoMeuIp(ip, sizeof(ip))) {
            snprintf(novo, sizeof(novo), "http://%s:%d", ip, REMOTO_PORTA);
            if (strcmp(novo, url) != 0) { snprintf(url, sizeof(url), "%s", novo); lado = qrGerar(url, mod); }
        }
    }
    if (!lado) return;
    const int esc = 7, caixa = (lado + 8) * esc;
    int x0 = 1920 - caixa - 60, y0 = 1080 - caixa - 120;
    for (int y = y0 - 70; y < y0 + caixa + 90; y++)
        for (int x = x0 - 40; x < x0 + caixa + 20; x++)
            if (x >= 0 && x < 1920 && y >= 0 && y < 1080) p[y * 1920 + x] = 0xF0101418;
    for (int y = 0; y < caixa; y++)
        for (int x = 0; x < caixa; x++) {
            int mx = x / esc - 4, my = y / esc - 4;
            bool escuro = mx >= 0 && my >= 0 && mx < lado && my < lado && mod[my * lado + mx];
            p[(y0 + y) * 1920 + x0 + x] = escuro ? 0xFF000000 : 0xFFFFFFFF;
        }
    desenharTexto(p, "Controle pelo celular", 26, x0 - 30, y0 - 60, 0xFFFFFFFF);
    desenharTexto(p, url, 22, x0 - 30, y0 + caixa + 14, 0xFF8ACBFF);
    desenharTexto(p, "L3 + R3 mostra / esconde", 18, x0 - 30, y0 + caixa + 50, 0xFFAAAAAA);
}

// ---- Tela do PC controlada pelo controle do PS4 ----
// Enquanto a tela do PC está aberta pelo FTP > [PC], o controle vira mouse e teclado do PC;
// para o app só sobra a Bolinha (que fecha a tela).
extern volatile bool remotoEspelhando;
static volatile bool pcControlando = false;
static bool pcImeAberto = false;
static bool iaAbrirTecladoBase(int32_t usuario, const char* titulo, int limite, const char* inicial);
static void pcTraduzirControle(OrbisPadData* d) {
    static uint32_t antes = 0;
    static uint64_t ultimo = 0;
    static float restoX = 0, restoY = 0, restoR = 0;
    static int toqueX = -1, toqueY = -1;
    uint64_t agora = remotoAgoraMs();
    float dt = ultimo ? (agora - ultimo) / 1000.0f : 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    ultimo = agora;
    uint32_t b = d->buttons, novos = b & ~antes, soltos = antes & ~b;
    if (!pcImeAberto) {
        // Analógico esquerdo: a velocidade cresce com o quadrado da inclinação (até ~1500 px/s).
        int ax = d->leftStick.x - 128, ay = d->leftStick.y - 128;
        if (abs(ax) > 18) restoX += (ax > 0 ? 1 : -1) * (float)(ax * ax) / 16129.0f * 1500.0f * dt;
        if (abs(ay) > 18) restoY += (ay > 0 ? 1 : -1) * (float)(ay * ay) / 16129.0f * 1500.0f * dt;
        int mx = (int)restoX, my = (int)restoY;
        restoX -= mx; restoY -= my;
        // Touchpad: arrasta como o de notebook.
        if (d->touch.fingers > 0) {
            int tx = d->touch.touch[0].x, ty = d->touch.touch[0].y;
            if (toqueX >= 0) { mx += (tx - toqueX) * 3 / 2; my += (ty - toqueY) * 3 / 2; }
            toqueX = tx; toqueY = ty;
        }
        else toqueX = toqueY = -1;
        // Analógico direito: rolar a página.
        int ry = d->rightStick.y - 128;
        if (abs(ry) > 30) restoR += -(float)ry / 127.0f * 10.0f * dt;
        int roda = (int)restoR; restoR -= roda;
        if (mx || my || roda) pcMover(mx, my, roda);
        static const struct { uint32_t bt; const char* cmd; } mapa[] = {
            { ORBIS_PAD_BUTTON_CROSS, "botao\tesq" }, { ORBIS_PAD_BUTTON_SQUARE, "botao\tdir" }, { ORBIS_PAD_BUTTON_R3, "botao\tmeio" },
            { ORBIS_PAD_BUTTON_UP, "tecla\t38" }, { ORBIS_PAD_BUTTON_DOWN, "tecla\t40" }, { ORBIS_PAD_BUTTON_LEFT, "tecla\t37" }, { ORBIS_PAD_BUTTON_RIGHT, "tecla\t39" },
            { ORBIS_PAD_BUTTON_R1, "tecla\t13" }, { ORBIS_PAD_BUTTON_L1, "tecla\t8" }, { ORBIS_PAD_BUTTON_L2, "tecla\t27" }, { ORBIS_PAD_BUTTON_R2, "tecla\t9" },
            { ORBIS_PAD_BUTTON_OPTIONS, "tecla\t91" } };
        for (unsigned i = 0; i < sizeof(mapa) / sizeof(mapa[0]); i++) {
            char l[48];
            if (novos & mapa[i].bt) { snprintf(l, sizeof(l), "%s\t1", mapa[i].cmd); pcPedir(l); }
            if (soltos & mapa[i].bt) { snprintf(l, sizeof(l), "%s\t0", mapa[i].cmd); pcPedir(l); }
        }
        if (novos & ORBIS_PAD_BUTTON_TRIANGLE) {
            extern int32_t global_uId;
            if (iaAbrirTecladoBase(global_uId, "Digitar no PC", 1000, NULL)) pcImeAberto = true;
        }
    }
    antes = b;
    d->buttons = b & ORBIS_PAD_BUTTON_CIRCLE;
    d->leftStick.x = d->leftStick.y = d->rightStick.x = d->rightStick.y = 128;
    d->analogButtons.l2 = d->analogButtons.r2 = 0;
    d->touch.fingers = 0;
}

static int hnPadLerInterno(int handle, OrbisPadData* dados);

// L3 + R3 juntos (fora dos jogos) mostra ou esconde o QR.
int hnPadLer(int handle, OrbisPadData* dados) {
    int rc = hnPadLerInterno(handle, dados);
    if (rc == 0) {
        extern bool emuladorAtivo;
        static uint32_t antes = 0;
        const uint32_t combo = ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_R3;
        if (!emuladorAtivo && (dados->buttons & combo) == combo && (antes & combo) != combo) remotoQrModo = remotoQrVisivel() ? 0 : 2;
        antes = dados->buttons;
        if (pcControlando && remotoEspelhando) pcTraduzirControle(dados);
    }
    return rc;
}

static int hnPadLerInterno(int handle, OrbisPadData* dados) {
    int rc = scePadReadState(handle, dados);
    uint8_t analog[4]; bool temAnalog = false;
    uint32_t botoes = remotoLigado ? remotoBotoes(analog, &temAnalog) : 0;
    uint16_t toque[4];
    int dedos = remotoLigado ? remotoToque(toque) : 0;
    if (!botoes && !temAnalog && !dedos) return rc;
    if (rc != 0) {
        // Sem controle físico ligado, o celular sozinho comanda.
        memset(dados, 0, sizeof(*dados));
        dados->leftStick.x = dados->leftStick.y = dados->rightStick.x = dados->rightStick.y = 128;
        dados->connected = 1;
        rc = 0;
    }
    dados->buttons |= botoes;
    if (botoes & ORBIS_PAD_BUTTON_L2) dados->analogButtons.l2 = 255;
    if (botoes & ORBIS_PAD_BUTTON_R2) dados->analogButtons.r2 = 255;
    if (temAnalog) {
        dados->leftStick.x = analog[0]; dados->leftStick.y = analog[1];
        dados->rightStick.x = analog[2]; dados->rightStick.y = analog[3];
    }
    if (dedos) {
        dados->touch.fingers = (uint8_t)dedos;
        for (int i = 0; i < dedos; i++) {
            dados->touch.touch[i].x = toque[i * 2];
            dados->touch.touch[i].y = toque[i * 2 + 1];
            dados->touch.touch[i].finger = (uint8_t)(100 + i);
        }
    }
    return rc;
}

static void remotoAbrirMidia(const char* caminho);
static void remotoAtenderDlna();


// ---- Tela do PS4 para o navegador ----
#define STBIW_ASSERT(x)
#include "stb_image_write.h"
extern uint32_t* graficosTelaExibida();
static volatile bool capturaRodando = false;

struct JpegSaida { uint8_t* dados; int tam; int cap; };
static void jpegEscrever(void* ctx, void* dados, int tam) {
    JpegSaida* s = (JpegSaida*)ctx;
    if (s->tam + tam > s->cap) {
        int nova = (s->tam + tam) * 2;
        uint8_t* n = (uint8_t*)realloc(s->dados, nova);
        if (!n) return;
        s->dados = n; s->cap = nova;
    }
    memcpy(s->dados + s->tam, dados, tam);
    s->tam += tam;
}

// Lê a tela que está sendo exibida (sem esperar o desenho: um quadro com "rasgo" não faz mal)
// em 960x540 e manda em JPEG. Só roda enquanto alguém estiver vendo.
static void* remotoThreadCaptura(void*) {
    extern void graficosCapturaLigar(bool ligar, int escala);
    extern int graficosCapturaEscala();
    extern unsigned graficosCapturaVoltas();
    extern const uint8_t* graficosCapturaRgb();
    uint8_t* rgb = (uint8_t*)malloc(960 * 540 * 3);
    JpegSaida saida = { NULL, 0, 0 };
    remotoLog("TELA: captura ligada");
    int escalaPedida = remotoTelaEscala;
    graficosCapturaLigar(true, escalaPedida);
    extern int graficosCapturaMs();
    unsigned visto = graficosCapturaVoltas();
    uint64_t ultimo = 0, relogio = remotoAgoraMs();
    int enviados = 0, msJpeg = 0;
    while (rgb && remotoAlguemVendo()) {
        // Quem desenha copia o quadro pronto em faixas, antes do flip: nunca pega um quadro pela metade.
        if (escalaPedida != remotoTelaEscala) { escalaPedida = remotoTelaEscala; graficosCapturaLigar(true, escalaPedida); }
        unsigned v = graficosCapturaVoltas();
        const uint8_t* fonte = graficosCapturaRgb();
        int e = graficosCapturaEscala();
        // Rápida: até uns 15 quadros por segundo; nítida: uns 10.
        if (v == visto || !fonte || (int)(remotoAgoraMs() - ultimo) < (e == 3 ? 60 : 90)) { remotoDormir(5); continue; }
        visto = v;
        ultimo = remotoAgoraMs();
        const int W = 1920 / e, H = 1080 / e;
        memcpy(rgb, fonte, W * H * 3);
        saida.tam = 0;
        stbi_write_jpg_to_func(jpegEscrever, &saida, W, H, 3, rgb, e == 3 ? 60 : 70);
        if (saida.tam > 0) remotoPublicarQuadro(saida.dados, saida.tam);
        enviados++; msJpeg += (int)(remotoAgoraMs() - ultimo);
        if (remotoAgoraMs() - relogio >= 5000) {
            // Para medir atraso: quadros por segundo, faixa copiada por flip e custo do JPEG.
            remotoLog("TELA: %dx%d %.1f quadros/s, leitura %d ms, jpeg %d ms, %d bytes", 1920 / e, 1080 / e, enviados * 1000.0 / (remotoAgoraMs() - relogio), graficosCapturaMs(), enviados ? msJpeg / enviados : 0, saida.tam);
            relogio = remotoAgoraMs(); enviados = 0; msJpeg = 0;
        }
    }
    graficosCapturaLigar(false, 3);
    free(rgb); free(saida.dados);
    remotoLog("TELA: captura desligada");
    capturaRodando = false;
    return NULL;
}

static void remotoComecarCaptura() {
    if (capturaRodando) return;
    capturaRodando = true;
    if (!remotoCriarThread(remotoThreadCaptura, NULL)) capturaRodando = false;
}

// ---- IA no PS4: HTTPS, voz, microfone, imagem da tela, texto de PDF e as telas da IA e das Chaves ----
extern "C" int sceHttpSetRequestContentLength(int reqId, uint64_t contentLength);
#include <mupdf/fitz.h>

static int iaHttpPost(const char* url, const char* cabecalhos, const char* tipo, const char* corpo, size_t tam, char** resp, size_t* tamResp) {
    *resp = NULL; *tamResp = 0;
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0 (PS4)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return tpl;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, url, (unsigned long long)tam) : -1;
    int status = req < 0 ? (conn < 0 ? conn : req) : -1;
    if (req >= 0) {
        // Imagem, voz e arquivos grandes levam até alguns minutos.
        ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(req, 240u * 1000000u);
        sceHttpSetSendTimeOut(req, 120u * 1000000u);
        sceHttpSetConnectTimeOut(req, 20u * 1000000u);
        sceHttpAddRequestHeader(req, "Content-Type", tipo, 1);
        // "Nome: valor" por linha (chave do Gemini ou Bearer da OpenAI/Groq).
        for (const char* p = cabecalhos; p && *p; ) {
            const char* fim = strchr(p, '\n'); size_t n = fim ? (size_t)(fim - p) : strlen(p);
            const char* dois = (const char*)memchr(p, ':', n);
            if (dois) {
                char nome[64], valor[400];
                snprintf(nome, sizeof(nome), "%.*s", (int)(dois - p), p);
                const char* v = dois + 1; while (*v == ' ') v++;
                snprintf(valor, sizeof(valor), "%.*s", (int)(p + n - v), v);
                sceHttpAddRequestHeader(req, nome, valor, 1);
            }
            p = fim ? fim + 1 : NULL;
        }
        sceHttpSetRequestContentLength(req, tam);
        int rc = sceHttpSendRequest(req, corpo, tam);
        if (rc >= 0) {
            sceHttpGetStatusCode(req, &status);
            IaBuf r = { NULL, 0, 0 };
            char bloco[16384];
            int n;
            while ((n = sceHttpReadData(req, bloco, sizeof(bloco))) > 0) if (!iaMais(&r, bloco, n)) break;
            *resp = r.d; *tamResp = r.n;
        }
        else status = rc;
        sceHttpDeleteRequest(req);
    }
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return status;
}

// GET simples (busca e imagem das capas das músicas).
static int iaHttpGet(const char* url, char** resp, size_t* tamResp) {
    *resp = NULL; *tamResp = 0;
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0 (PS4)", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return tpl;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
    int status = req < 0 ? (conn < 0 ? conn : req) : -1;
    if (req >= 0) {
        ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(req, 30u * 1000000u);
        sceHttpSetConnectTimeOut(req, 15u * 1000000u);
        sceHttpAddRequestHeader(req, "Accept", "*/*", 1);
        int rc = sceHttpSendRequest(req, NULL, 0);
        if (rc >= 0) {
            sceHttpGetStatusCode(req, &status);
            IaBuf r = { NULL, 0, 0 };
            char bloco[16384];
            int n;
            while ((n = sceHttpReadData(req, bloco, sizeof(bloco))) > 0 && r.n < 8u * 1024 * 1024) if (!iaMais(&r, bloco, n)) break;
            *resp = r.d; *tamResp = r.n;
        }
        else status = rc;
        sceHttpDeleteRequest(req);
    }
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return status;
}

void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);
int medirLarguraTexto(const char* texto, int tam);
// Painel da letra na tela de Músicas (Quadrado mostra ou esconde).
static bool letraEscondida = false;
void letraAlternar() {
    letraEscondida = !letraEscondida;
    if (!letraEscondida && letraN == 0) { snprintf(msgStatus, sizeof(msgStatus), "SEM LETRA: PONHA UM .LRC COM O MESMO NOME DA MUSICA"); msgTimer = 180; }
}

static void letraCortar(const char* s, int tamFonte, int largura, char* saida, int cap) {
    snprintf(saida, cap, "%s", s);
    if (medirLarguraTexto(saida, tamFonte) <= largura) return;
    int n = (int)strlen(saida);
    while (n > 0) {
        n--;
        while (n > 0 && ((unsigned char)saida[n] & 0xC0) == 0x80) n--;   // não corta no meio de um caractere
        saida[n] = 0;
        char t[260]; snprintf(t, sizeof(t), "%s...", saida);
        if (medirLarguraTexto(t, tamFonte) <= largura) { snprintf(saida, cap, "%s", t); return; }
    }
}

// Painel da letra em qualquer retângulo. 'fundo' pinta um fundo opaco: a tela do menu é
// memória sem cache, e escurecer lendo os pixels custava dezenas de ms por quadro.
void letraDesenharPainel(uint32_t* p, const char* alvo, int x0, int y0, int w, int h, bool fundo) {
    extern char musicaAtual[256];
    extern volatile float audioTempoAtual;
    if (letraEscondida || letraCarregar(alvo) <= 0) return;
    bool tocandoEsta = !strcmp(alvo, musicaAtual);
    if (fundo) {
        for (int y = y0; y < y0 + h && y < 1080; y++) {
            uint32_t* l = p + y * 1920;
            for (int x = x0; x < x0 + w && x < 1920; x++) l[x] = 0xFF0E0E16;
        }
        for (int x = x0; x < x0 + w && x < 1920; x++) p[y0 * 1920 + x] = 0xFF7A68FF;
    }
    char titulo[300];
    const char* nome = strrchr(alvo, '/'); nome = nome ? nome + 1 : alvo;
    char cab[300]; snprintf(cab, sizeof(cab), "Letra (%s)  -  %s", letraFonte, nome);
    letraCortar(cab, 26, w - 48, titulo, sizeof(titulo));
    desenharTexto(p, titulo, 26, x0 + 24, y0 + 22, 0xFFA49CE6);
    int atual = -1;
    if (letraSincronizada && tocandoEsta) {
        float t = audioTempoAtual + 0.25f;
        for (int i = 0; i < letraN && letraLinhas[i].t <= t; i++) atual = i;
    }
    const int passo = 46, topo = y0 + 80, visiveis = (h - 110) / passo;
    int primeira = atual >= 0 ? atual - visiveis / 2 : 0;
    if (primeira > letraN - visiveis) primeira = letraN - visiveis;
    if (primeira < 0) primeira = 0;
    for (int k = 0; k < visiveis && primeira + k < letraN; k++) {
        int i = primeira + k;
        uint32_t cor = i == atual ? 0xFFFFD54A : (atual >= 0 && i < atual ? 0xFF8C8CA6 : 0xFFE6E6F0);
        char linha[260];
        letraCortar(letraLinhas[i].txt, i == atual ? 32 : 28, w - 48, linha, sizeof(linha));
        if (!fundo) desenharTexto(p, linha, i == atual ? 32 : 28, x0 + 26, topo + k * passo + 2, 0xFF000000);   // sombra sobre a capa
        desenharTexto(p, linha, i == atual ? 32 : 28, x0 + 24, topo + k * passo, cor);
    }
    if (!letraSincronizada || !tocandoEsta)
        desenharTexto(p, letraSincronizada ? "Toque a musica para acompanhar a letra" : "Letra sem tempo (.lrc sincroniza)", 22, x0 + 24, y0 + h - 34, 0xFF8C8CA6);
}

bool letraVisivel(const char* alvo) { return !letraEscondida && letraCarregar(alvo) > 0; }

void letraDesenhar(uint32_t* p) {
    extern bool showOpcoes;
    extern bool musicaPlayerAberto;
    if (letraEscondida || menuAtual != MENU_MUSICAS || showOpcoes || musicaPlayerAberto) return;
    extern char musicaAtual[256];
    extern char caminhosMusicasMenu[3000][256];
    // A letra da música selecionada; quando é a que está tocando, acompanha o tempo.
    const char* alvo = (sel >= 0 && sel < 3000) ? caminhosMusicasMenu[sel] : "";
    int na = (int)strlen(alvo);
    bool arquivo = na > 4 && (!strcasecmp(alvo + na - 4, ".mp3") || !strcasecmp(alvo + na - 4, ".wav"));
    if (!arquivo) alvo = musicaAtual;
    letraDesenharPainel(p, alvo, 1250, 120, 640, 820, true);
}

// Título e artista das etiquetas ID3 (para o player de música).
bool musicaInfo(const char* caminho, char* titulo, int tt, char* artista, int ta) {
    Id3 t;
    bool ok = id3Ler(caminho, &t);
    free(t.imagem); free(t.letra);
    snprintf(titulo, tt, "%s", ok ? t.titulo : "");
    snprintf(artista, ta, "%s", ok ? t.artista : "");
    return ok && (t.titulo[0] || t.artista[0]);
}

// Voz da IA: entra na mesma saída de som do app (audio.cpp chama misturarFalaIA a cada bloco de
// 256 amostras). Uma porta própria falhava no console (0x809B0001): o PS4 recusava a segunda
// porta MAIN enquanto a do player de música estava aberta.
static int16_t* volatile falaPcm = NULL;
static volatile int falaAmostras = 0, falaTaxa = 24000;
static volatile double falaPos = 0;
static volatile bool iaTocando = false, iaPararFala = false;

// Som de outros módulos (áudio do Telegram) na mesma saída do app: uma segunda porta MAIN
// falha no console (0x809B0001) enquanto a da música está aberta. Anel de 2 s, 48 kHz estéreo.
#define APP_SOM_ANEL (48000 * 2)
static int16_t appSomAnel[APP_SOM_ANEL * 2];
static volatile uint64_t appSomEscrito = 0, appSomLido = 0;

// Espera vaga no anel (o produtor anda no ritmo do som). Devolve false se mandaram parar.
bool appSomEmpurrar(const int16_t* estereo, int quadros, volatile bool* parar) {
    int feito = 0;
    while (feito < quadros) {
        uint64_t livres = APP_SOM_ANEL - (appSomEscrito - appSomLido);
        if (livres == 0) { if (parar && *parar) return false; remotoDormir(5); continue; }
        int n = quadros - feito;
        if ((uint64_t)n > livres) n = (int)livres;
        uint64_t w = appSomEscrito;
        for (int i = 0; i < n; i++) {
            int k = (int)((w + i) % APP_SOM_ANEL);
            appSomAnel[k * 2] = estereo[(feito + i) * 2];
            appSomAnel[k * 2 + 1] = estereo[(feito + i) * 2 + 1];
        }
        __sync_synchronize();
        appSomEscrito = w + n;
        feito += n;
    }
    return true;
}
void appSomEsperarFim(volatile bool* parar) { while (appSomLido < appSomEscrito && !(parar && *parar)) remotoDormir(10); }
void appSomLimpar() { appSomLido = appSomEscrito; }

static void appSomMisturar(int16_t* saida, int amostras) {
    uint64_t r = appSomLido, w = appSomEscrito;
    int n = (int)(w - r);
    if (n <= 0) return;
    if (n > amostras) n = amostras;
    for (int i = 0; i < n; i++) {
        int k = (int)((r + i) % APP_SOM_ANEL);
        for (int c = 0; c < 2; c++) {
            int v = saida[i * 2 + c] + appSomAnel[k * 2 + c];
            saida[i * 2 + c] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        }
    }
    __sync_synchronize();
    appSomLido = r + n;
}

static void misturarFalaIAVoz(int16_t* saida, int amostras);
void misturarFalaIA(int16_t* saida, int amostras) {
    appSomMisturar(saida, amostras);
    misturarFalaIAVoz(saida, amostras);
    remotoSomEmpurrar(saida, amostras);   // último passo da mistura: é o som que sai do PS4
}
static void misturarFalaIAVoz(int16_t* saida, int amostras) {
    if (!iaTocando) return;
    if (iaPararFala) { iaTocando = false; return; }
    int16_t* pcm = falaPcm;
    int total = falaAmostras;
    double pos = falaPos, salto = (double)falaTaxa / 48000.0;
    if (!pcm) { iaTocando = false; return; }
    for (int i = 0; i < amostras; i++) {
        int k = (int)pos;
        if (k >= total) { iaTocando = false; break; }
        int v = pcm[k] + pcm[k] / 4;
        for (int c = 0; c < 2; c++) {
            int s = saida[i * 2 + c] / 2 + v;   // a música baixa enquanto a voz fala
            saida[i * 2 + c] = (int16_t)(s > 32767 ? 32767 : (s < -32768 ? -32768 : s));
        }
        pos += salto;
    }
    falaPos = pos;
}
static void iaTocarPcm(const int16_t* pcm, int amostras, int taxa) {
    int16_t* copia = (int16_t*)malloc((size_t)amostras * 2);
    if (!copia) return;
    memcpy(copia, pcm, (size_t)amostras * 2);
    iaTocando = false;
    sceKernelUsleep(20000);   // um bloco de áudio (5 ms) termina antes de soltar a fala anterior
    int16_t* velha = falaPcm;
    falaPcm = copia; falaAmostras = amostras; falaTaxa = taxa; falaPos = 0;
    iaPararFala = false;
    __sync_synchronize();
    iaTocando = true;
    free(velha);
    remotoLog("IA: tocando %d amostras a %d Hz (%.1f s)", amostras, taxa, (double)amostras / taxa);
}
static bool iaTocandoAgora() { return iaTocando; }
static void iaPararVoz() { iaPararFala = true; }

// Texto de um PDF (uma página ou o documento todo) com o mupdf que o leitor já usa.
static bool iaTextoPdf(const char* caminho, int pagina, IaBuf* saida) {
    fz_context* ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!ctx) return false;
    bool ok = false;
    fz_document* doc = NULL;
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, caminho);
        int n = fz_count_pages(ctx, doc);
        int a = pagina > 0 ? pagina - 1 : 0, b = pagina > 0 ? pagina : n;
        if (b > n) b = n;
        for (int i = a; i < b && saida->n < 400000; i++) {
            fz_page* pg = fz_load_page(ctx, doc, i);
            fz_stext_page* st = fz_new_stext_page_from_page(ctx, pg, NULL);
            fz_buffer* buf = fz_new_buffer_from_stext_page(ctx, st);
            unsigned char* d = NULL;
            size_t len = fz_buffer_storage(ctx, buf, &d);
            if (len) iaMais(saida, (const char*)d, len);
            iaTxt(saida, "\n");
            fz_drop_buffer(ctx, buf);
            fz_drop_stext_page(ctx, st);
            fz_drop_page(ctx, pg);
        }
        ok = true;
    }
    fz_catch(ctx) { ok = false; }
    if (doc) fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    return ok;
}

// Imagem da tela em JPEG. Com a tela da IA aberta, vale a foto tirada antes de abrir.
static uint8_t* iaTelaAntes = NULL;
static int iaTelaAntesTam = 0;
static bool iaCapturarTela(uint8_t** jpeg, int* tam) {
    extern uint32_t* graficosTelaExibida();
    uint32_t* tela = graficosTelaExibida();
    if (!tela) return false;
    const int W = 960, H = 540;
    uint8_t* rgb = (uint8_t*)malloc(W * H * 3);
    uint32_t* linha = (uint32_t*)malloc(1920 * 4);
    if (!rgb || !linha) { free(rgb); free(linha); return false; }
    for (int y = 0; y < H; y++) {
        memcpy(linha, tela + (size_t)(y * 2) * 1920, 1920 * 4);
        for (int x = 0; x < W; x++) { uint32_t px = linha[x * 2]; uint8_t* d = rgb + (y * W + x) * 3; d[0] = (uint8_t)(px >> 16); d[1] = (uint8_t)(px >> 8); d[2] = (uint8_t)px; }
    }
    JpegSaida s = { NULL, 0, 0 };
    stbi_write_jpg_to_func(jpegEscrever, &s, W, H, 3, rgb, 80);
    free(rgb); free(linha);
    if (s.tam <= 0) { free(s.dados); return false; }
    *jpeg = s.dados; *tam = s.tam;
    return true;
}
static bool iaTelaJpeg(uint8_t** jpeg, int* tam) {
    extern MenuLevel menuAtual;
    if ((menuAtual == MENU_IA || menuAtual == MENU_CHAVES) && iaTelaAntes) {
        *jpeg = (uint8_t*)malloc(iaTelaAntesTam);
        if (!*jpeg) return false;
        memcpy(*jpeg, iaTelaAntes, iaTelaAntesTam); *tam = iaTelaAntesTam;
        return true;
    }
    return iaCapturarTela(jpeg, tam);
}

// Microfone (fone no controle ou câmera): grava em WAV 16 kHz até soltar o botão (máx. 60 s).
static volatile bool iaGravando = false, iaPararGravacao = false;
static volatile int iaGravacaoSegundos = 0;
static void* iaThreadGravar(void*) {
    extern int32_t global_uId;
    typedef int (*AbrirMic)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef int (*LerMic)(int32_t, void*);
    typedef int (*FecharMic)(int32_t);
    int mic = ((AbrirMic)sceAudioInOpen)(global_uId, 0, 0, 256, 16000, 0);
    int micPrimeiro = mic;
    if (mic < 0) mic = ((AbrirMic)sceAudioInOpen)(255, 0, 0, 256, 16000, 0);
    remotoLog("IA mic: abrir usuario=%d rc=0x%08X, sistema rc=0x%08X", (int)global_uId, (unsigned)micPrimeiro, (unsigned)mic);
    if (mic < 0) {
        char e[160]; snprintf(e, sizeof(e), "Microfone não encontrado (%x). Ligue um fone com microfone no controle.", (unsigned)mic);
        iaDefinirErro(e);
        iaGravando = false;
        return NULL;
    }
    const int maximo = 16000 * 60;
    int16_t* pcm = (int16_t*)malloc((size_t)maximo * 2);
    int n = 0, erros = 0;
    static int16_t lido[256];
    while (pcm && !iaPararGravacao && n < maximo) {
        int r = ((LerMic)sceAudioInInput)(mic, lido);
        if (r < 0) {
            if (erros == 0) remotoLog("IA mic: leitura falhou rc=0x%08X depois de %d amostras", (unsigned)r, n);
            if (++erros > 300) break;
            sceKernelUsleep(5000); continue;
        }
        erros = 0;
        int k = (r == 0 || r > 256) ? 256 : r;
        if (n + k > maximo) k = maximo - n;
        memcpy(pcm + n, lido, (size_t)k * 2);
        n += k;
        iaGravacaoSegundos = n / 16000;
    }
    ((FecharMic)sceAudioInClose)(mic);
    iaGravando = false;
    remotoLog("IA mic: %d amostras (%.1f s), %d erros seguidos no fim, parado=%d", n, n / 16000.0, erros, iaPararGravacao ? 1 : 0);
    if (!pcm) return NULL;
    int pico = 0;
    for (int i = 0; i < n; i++) { int v = pcm[i] < 0 ? -pcm[i] : pcm[i]; if (v > pico) pico = v; }
    remotoLog("IA mic: pico do sinal %d de 32767", pico);
    if (n < 16000 / 2) {
        free(pcm);
        iaDefinirErro(erros > 300 ? "O microfone não respondeu. Ligue um fone com microfone no controle (ou a câmera do PS4)." : "Gravação muito curta: segure o botão enquanto fala.");
        return NULL;
    }
    remotoCriarPastas(IA_PASTA);
    iaGravarWav(IA_PASTA "/pergunta.wav", (const uint8_t*)pcm, (size_t)n * 2, 16000);
    free(pcm);
    if (!iaComecar(IA_OUVIR, "", false, IA_PASTA "/pergunta.wav", 0)) iaDefinirErro("A IA ainda está trabalhando.");
    return NULL;
}
static void iaGravarIniciar() {
    if (iaGravando || ia.ocupado) return;
    iaPararGravacao = false; iaGravando = true; iaGravacaoSegundos = 0;
    if (!remotoCriarThread(iaThreadGravar, NULL)) iaGravando = false;
}

// Triângulo no leitor de PDF (página atual) ou no visualizador de texto: lê em voz alta.
// Apertar de novo durante a leitura para.
void iaLerTelaAberta() {
    extern bool visualizandoPDF, visualizandoMidiaTexto;
    extern char caminhoLeituraAtual[512];
    extern int pdfPaginaAtual, totalLinhasTexto;
    extern char* linhasTexto[5000];
    if (ia.lendo || iaTocando) { iaPararLeitura(); remotoAviso("LEITURA PARADA"); return; }
    bool ok = false;
    if (visualizandoPDF && caminhoLeituraAtual[0]) ok = iaComecar(IA_LER_ARQUIVO, "", false, caminhoLeituraAtual, pdfPaginaAtual);
    else if (visualizandoMidiaTexto) {
        IaBuf t = { NULL, 0, 0 };
        for (int i = 0; i < totalLinhasTexto && t.n < 300000; i++) if (linhasTexto[i]) { iaTxt(&t, linhasTexto[i]); iaTxt(&t, "\n"); }
        if (t.n) ok = iaComecar(IA_FALAR, t.d, false, NULL, 0);
        free(t.d);
    }
    else return;
    remotoAviso(ok ? "LENDO EM VOZ ALTA (TRIANGULO PARA)" : "A IA AINDA ESTA TRABALHANDO");
}

// ---- utilidades das telas ----
static uint16_t iaImeTexto[2049];
static void iaUtf16ParaUtf8(const uint16_t* s, char* out, int tam) {
    int o = 0;
    for (int i = 0; s[i] && o < tam - 5; i++) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp < 0xDC00 && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00); i++; }
        if (cp < 0x80) out[o++] = (char)cp;
        else if (cp < 0x800) { out[o++] = (char)(0xC0 | (cp >> 6)); out[o++] = (char)(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { out[o++] = (char)(0xE0 | (cp >> 12)); out[o++] = (char)(0x80 | ((cp >> 6) & 63)); out[o++] = (char)(0x80 | (cp & 63)); }
        else { out[o++] = (char)(0xF0 | (cp >> 18)); out[o++] = (char)(0x80 | ((cp >> 12) & 63)); out[o++] = (char)(0x80 | ((cp >> 6) & 63)); out[o++] = (char)(0x80 | (cp & 63)); }
    }
    out[o] = 0;
}
// Abre o teclado do PS4. Devolve false se não abriu.
static bool iaAbrirTecladoBase(int32_t usuario, const char* titulo, int limite, const char* inicial) {
    static uint16_t titulo16[96];
    int n = 0;
    for (const unsigned char* t = (const unsigned char*)titulo; *t && n < 95; t++) titulo16[n++] = *t < 128 ? *t : '?';
    titulo16[n] = 0;
    memset(iaImeTexto, 0, sizeof(iaImeTexto));
    if (inicial) { int k = 0; for (const unsigned char* t = (const unsigned char*)inicial; *t && k < limite; t++) iaImeTexto[k++] = *t; }
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    param.userId = usuario;
    param.maxTextLength = (uint32_t)limite;
    param.inputTextBuffer = (wchar_t*)iaImeTexto;
    param.title = (wchar_t*)titulo16;
    param.type = (OrbisImeType)0;
    return hnImeInit(&param, NULL) >= 0;
}
// Teclado terminou? Devolve 1 com texto, 0 cancelado, -1 ainda aberto.
static int iaTecladoResultado(char* texto, int tam) {
    int status = (int)hnImeStatus();
    if (status == 0 || status == 1) return -1;
    OrbisDialogResult resultado; memset(&resultado, 0, sizeof(resultado));
    hnImeResult(&resultado);
    int botao = *(int32_t*)&resultado;
    iaUtf16ParaUtf8(iaImeTexto, texto, tam);
    sceImeDialogTerm();
    return botao == 0 && texto[0] ? 1 : 0;
}
static void iaRetangulo(uint32_t* p, int x0, int y0, int x1, int y1, uint32_t cor) {
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) p[y * 1920 + x] = cor;
}

// ---- Tela da IA (EXTRA > Inteligência Artificial) ----
enum { IA_ITEM_PERGUNTAR, IA_ITEM_TELA, IA_ITEM_MIC, IA_ITEM_IMAGEM, IA_ITEM_FALAR, IA_ITEM_CONTA, IA_ITEM_MODELO, IA_ITEM_VOZ,
       IA_ITEM_ASPECTO, IA_ITEM_SEMPRE_VOZ, IA_ITEM_CONTAS, IA_ITEM_LIMPAR, IA_TOTAL_ITENS };
static int iaSel = 0, iaRolagem = 0;
static int iaImeModo = 0;     // item que abriu o teclado + 1
void chavesAbrirEm(int categoria);

static void iaGuardarTelaAntes() {
    free(iaTelaAntes); iaTelaAntes = NULL;
    uint8_t* j = NULL; int t = 0;
    if (iaCapturarTela(&j, &t)) { iaTelaAntes = j; iaTelaAntesTam = t; }
}
void iaAbrir() {
    extern MenuLevel menuAtual;
    iaCarregar();
    iaGuardarTelaAntes();
    menuAtual = MENU_IA;
    iaRolagem = 0;
}

// Provedor que fala (a conta escolhida, ou a primeira que saiba).
static int iaProvVoz() { IaConta c; return iaContaPara(CAP_VOZ, &c, NULL) ? c.prov : -1; }

static void iaRotuloItem(int i, char* s, int tam) {
    pthread_mutex_lock(&ia.trava);
    const IaConta* c = ia.nContas ? &ia.contas[ia.contaAtiva] : NULL;
    switch (i) {
    case IA_ITEM_PERGUNTAR: snprintf(s, tam, "Perguntar"); break;
    case IA_ITEM_TELA: snprintf(s, tam, "Perguntar sobre a tela de antes"); break;
    case IA_ITEM_MIC: snprintf(s, tam, iaGravando ? "Gravando... %d s (solte para enviar)" : "Falar no microfone (segure)", (int)iaGravacaoSegundos); break;
    case IA_ITEM_IMAGEM: snprintf(s, tam, "Gerar imagem"); break;
    case IA_ITEM_FALAR: snprintf(s, tam, (ia.lendo || iaTocando) ? "Parar a voz" : "Ler a resposta em voz alta"); break;
    case IA_ITEM_CONTA: if (c) snprintf(s, tam, "Conta: %s (%s)", c->nome, IA_PROV_CURTO[c->prov]); else snprintf(s, tam, "Conta: nenhuma (X cadastra)"); break;
    case IA_ITEM_MODELO: snprintf(s, tam, "Modelo: %s", c ? iaModeloDe(c) : "-"); break;
    case IA_ITEM_SEMPRE_VOZ: snprintf(s, tam, "Responder sempre em voz: %s", ia.falar ? "Sim" : "Não"); break;
    case IA_ITEM_ASPECTO: snprintf(s, tam, "Formato da imagem: %s", IA_ASPECTOS[ia.aspecto]); break;
    case IA_ITEM_CONTAS: snprintf(s, tam, "Contas e chaves de IA (%d)", ia.nContas); break;
    case IA_ITEM_LIMPAR: snprintf(s, tam, "Limpar conversa"); break;
    default: s[0] = 0; break;
    }
    pthread_mutex_unlock(&ia.trava);
    if (i == IA_ITEM_VOZ) { int p = iaProvVoz(); snprintf(s, tam, p >= 0 ? "Voz: %s (%s)" : "Voz: precisa de conta Gemini ou ChatGPT", p >= 0 ? ia.voz[p] : "", p >= 0 ? IA_PROV_CURTO[p] : ""); }
}

static void iaAtualizarTeclado() {
    if (!iaImeModo) return;
    static char texto[6200];
    int r = iaTecladoResultado(texto, sizeof(texto));
    if (r < 0) return;
    int modo = iaImeModo; iaImeModo = 0;
    if (r == 0) return;
    if (modo == IA_ITEM_MODELO + 1) {
        pthread_mutex_lock(&ia.trava);
        if (ia.nContas) { iaLimparTexto(texto, true); snprintf(ia.contas[ia.contaAtiva].modelo, sizeof(ia.contas[0].modelo), "%s", texto); iaSalvarContasTravado(); ia.versao++; }
        pthread_mutex_unlock(&ia.trava);
        return;
    }
    int tipo = modo == IA_ITEM_IMAGEM + 1 ? IA_IMAGEM : IA_PERGUNTAR;
    if (!iaComecar(tipo, texto, modo == IA_ITEM_TELA + 1, NULL, 0)) iaDefinirErro("A IA ainda está trabalhando. Espere terminar.");
    iaRolagem = 0;
}

bool iaControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    iaAtualizarTeclado();
    if (iaImeModo) return false;
    uint32_t novos = botoes & ~antigos;
    bool segurandoMic = (botoes & ORBIS_PAD_BUTTON_SQUARE) || (iaSel == IA_ITEM_MIC && (botoes & ORBIS_PAD_BUTTON_CROSS));
    if (segurandoMic && !iaGravando && ((novos & ORBIS_PAD_BUTTON_SQUARE) || (iaSel == IA_ITEM_MIC && (novos & ORBIS_PAD_BUTTON_CROSS)))) {
        if (ia.ocupado) iaDefinirErro("A IA ainda está trabalhando. Espere terminar.");
        else iaGravarIniciar();
        return false;
    }
    if (!segurandoMic && iaGravando) iaPararGravacao = true;
    if (iaGravando) return false;
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) return true;
    if (novos & ORBIS_PAD_BUTTON_UP) iaSel = (iaSel + IA_TOTAL_ITENS - 1) % IA_TOTAL_ITENS;
    if (novos & ORBIS_PAD_BUTTON_DOWN) iaSel = (iaSel + 1) % IA_TOTAL_ITENS;
    if (novos & (ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_L2)) iaRolagem += 4;
    if (novos & (ORBIS_PAD_BUTTON_R1 | ORBIS_PAD_BUTTON_R2)) { iaRolagem -= 4; if (iaRolagem < 0) iaRolagem = 0; }
    if (novos & ORBIS_PAD_BUTTON_TRIANGLE) { if (iaAbrirTecladoBase(usuario, "Pergunte para a IA", 2000, NULL)) iaImeModo = IA_ITEM_PERGUNTAR + 1; }
    int lado = (novos & ORBIS_PAD_BUTTON_RIGHT) ? 1 : (novos & ORBIS_PAD_BUTTON_LEFT) ? -1 : 0;
    bool x = (novos & ORBIS_PAD_BUTTON_CROSS) != 0;
    if (!lado && !x) return false;
    int d = lado ? lado : 1;
    bool mudou = false;
    pthread_mutex_lock(&ia.trava);
    if (iaSel == IA_ITEM_CONTA && ia.nContas > 1 && lado) { ia.contaAtiva = (ia.contaAtiva + d + ia.nContas) % ia.nContas; mudou = true; }
    else if (iaSel == IA_ITEM_MODELO && ia.nContas && lado) {
        IaConta* c = &ia.contas[ia.contaAtiva];
        int n = iaNModelos(c->prov), atual = -1;
        for (int i = 0; i < n; i++) if (!strcmp(iaModeloDe(c), IA_PROV_MODELOS[c->prov][i])) atual = i;
        snprintf(c->modelo, sizeof(c->modelo), "%s", IA_PROV_MODELOS[c->prov][(atual + d + n) % n]);
        iaSalvarContasTravado(); mudou = true;
    }
    else if (iaSel == IA_ITEM_SEMPRE_VOZ) { ia.falar = !ia.falar; mudou = true; }
    else if (iaSel == IA_ITEM_ASPECTO) { ia.aspecto = (ia.aspecto + d + IA_N(IA_ASPECTOS)) % IA_N(IA_ASPECTOS); mudou = true; }
    if (mudou) { ia.versao++; iaSalvarConfigTravado(); }
    pthread_mutex_unlock(&ia.trava);
    if (iaSel == IA_ITEM_VOZ) {
        int p = iaProvVoz();
        if (p >= 0) {
            pthread_mutex_lock(&ia.trava);
            int n = iaNVozes(p), atual = 0;
            for (int i = 0; i < n; i++) if (!strcmp(ia.voz[p], IA_PROV_VOZES[p][i])) atual = i;
            snprintf(ia.voz[p], sizeof(ia.voz[p]), "%s", IA_PROV_VOZES[p][(atual + d + n) % n]);
            ia.versao++; iaSalvarConfigTravado();
            pthread_mutex_unlock(&ia.trava);
        }
        return false;
    }
    if (!x || mudou) return false;
    switch (iaSel) {
    case IA_ITEM_PERGUNTAR: if (iaAbrirTecladoBase(usuario, "Pergunte para a IA", 2000, NULL)) iaImeModo = IA_ITEM_PERGUNTAR + 1; break;
    case IA_ITEM_TELA: if (iaAbrirTecladoBase(usuario, "Pergunte sobre a tela", 2000, NULL)) iaImeModo = IA_ITEM_TELA + 1; break;
    case IA_ITEM_IMAGEM: if (iaAbrirTecladoBase(usuario, "Descreva a imagem", 2000, NULL)) iaImeModo = IA_ITEM_IMAGEM + 1; break;
    case IA_ITEM_MODELO: if (ia.nContas && iaAbrirTecladoBase(usuario, "Nome do modelo", 90, iaModeloDe(&ia.contas[ia.contaAtiva]))) iaImeModo = IA_ITEM_MODELO + 1; break;
    case IA_ITEM_CONTA: case IA_ITEM_CONTAS: chavesAbrirEm(0); break;
    case IA_ITEM_LIMPAR: iaLimparConversa(); iaRolagem = 0; break;
    case IA_ITEM_FALAR:
        if (ia.lendo || iaTocando) iaPararLeitura();
        else {
            char* u = iaUltimaResposta();
            if (!u) iaDefinirErro("Ainda não há resposta para ler.");
            else if (!iaComecar(IA_FALAR, u, false, NULL, 0)) iaDefinirErro("A IA ainda está trabalhando.");
            free(u);
        }
        break;
    }
    return false;
}

static int iaQuebrar(const char* texto, int tam, int largura, char linhas[][256], int max) {
    extern int medirLarguraTexto(const char* texto, int tam);
    int n = 0;
    const char* p = texto;
    while (*p && n < max) {
        if (*p == '\n') { linhas[n++][0] = 0; p++; continue; }
        char atual[256] = ""; int len = 0;
        const char* ultimoEspaco = NULL; int lenNoEspaco = 0;
        while (*p && *p != '\n' && len < 250) {
            atual[len] = *p; atual[len + 1] = 0;
            if (*p == ' ') { ultimoEspaco = p; lenNoEspaco = len; }
            if (medirLarguraTexto(atual, tam) > largura) {
                if (ultimoEspaco) { atual[lenNoEspaco] = 0; p = ultimoEspaco + 1; }
                else { while (len > 0 && ((unsigned char)atual[len] & 0xC0) == 0x80) len--; atual[len] = 0; }
                break;
            }
            len++; p++;
        }
        if (len >= 250) atual[len] = 0;
        snprintf(linhas[n++], 256, "%s", atual);
        if (*p == '\n') p++;
    }
    return n;
}

void iaRenderizar(uint32_t* p) {
    extern void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF101418;
    desenharTexto(p, "Inteligência Artificial", 44, 60, 40, 0xFFFFFFFF);
    for (int i = 0; i < IA_TOTAL_ITENS; i++) {
        int y = 120 + i * 62;
        bool s = i == iaSel;
        if (s) iaRetangulo(p, 50, y, 620, y + 54, 0xFF3D8BFD);
        char r[200]; iaRotuloItem(i, r, sizeof(r));
        desenharTexto(p, r, 26, 70, y + 12, s ? 0xFFFFFFFF : (i >= IA_ITEM_CONTA ? 0xFFB8C2CC : 0xFFE8EDF2));
    }
    iaRetangulo(p, 660, 110, 1880, 990, 0xFF1B2128);
    static char linhas[400][256];
    static char quem[400];
    static unsigned versaoFeita = ~0u;
    static int total = 0;
    pthread_mutex_lock(&ia.trava);
    if (versaoFeita != ia.versao) {
        versaoFeita = ia.versao; total = 0;
        for (int m = 0; m < ia.nMsgs && total < 380; m++) {
            quem[total] = ia.msgs[m].quem;
            snprintf(linhas[total++], 256, "%s", ia.msgs[m].quem == 'u' ? (strcmp(ia.msgs[m].img, "tela") == 0 ? "Você (sobre a tela):" : "Você:") : "IA:");
            int n = iaQuebrar(ia.msgs[m].texto, 26, 1150, linhas + total, 380 - total);
            for (int k = 0; k < n; k++) quem[total + k] = ia.msgs[m].quem == 'u' ? 'U' : 'M';
            total += n;
            if (ia.msgs[m].img[0] && strcmp(ia.msgs[m].img, "tela") != 0 && total < 380) {
                const char* nome = strrchr(ia.msgs[m].img, '/');
                quem[total] = 'I'; snprintf(linhas[total++], 256, "[imagem salva: %s]", nome ? nome + 1 : ia.msgs[m].img);
            }
            if (total < 380) { quem[total] = ' '; linhas[total++][0] = 0; }
        }
    }
    int cabem = 22;
    int maxRolagem = total > cabem ? total - cabem : 0;
    if (iaRolagem > maxRolagem) iaRolagem = maxRolagem;
    int primeira = total - cabem - iaRolagem; if (primeira < 0) primeira = 0;
    for (int i = 0; i < cabem && primeira + i < total; i++) {
        char q = quem[primeira + i];
        uint32_t cor = q == 'u' ? 0xFF7AA7FF : q == 'm' ? 0xFF4FD1A5 : q == 'U' ? 0xFFD6E2F0 : q == 'I' ? 0xFFE88EE8 : 0xFFE8EDF2;
        desenharTexto(p, linhas[primeira + i], 26, 690, 130 + i * 38, cor);
    }
    bool semConta = ia.nContas == 0;
    char rod[480];
    uint32_t corRod = 0xFF8A96A3;
    if (iaGravando) { snprintf(rod, sizeof(rod), "Gravando %d s... solte o botão para enviar", (int)iaGravacaoSegundos); corRod = 0xFFFF6B6B; }
    else if (ia.ocupado && ia.tarefa[0]) { snprintf(rod, sizeof(rod), "%s", ia.tarefa); corRod = 0xFFFFE08A; }
    else if (ia.erro[0]) { snprintf(rod, sizeof(rod), "%s", ia.erro); corRod = 0xFFFF6B6B; }
    else snprintf(rod, sizeof(rod), "X escolhe   Esquerda/Direita muda   Quadrado (segure) fala   Triângulo pergunta   L1/R1 rola   O volta");
    pthread_mutex_unlock(&ia.trava);
    if (!total) {
        desenharTexto(p, semConta ? "Cadastre uma conta: X em \"Contas e chaves de IA\" (Gemini, ChatGPT ou Groq)." : "Pergunte qualquer coisa: X em Perguntar, Triângulo de atalho,", 26, 690, 150, 0xFF8A96A3);
        desenharTexto(p, semConta ? "Pelo celular é mais fácil: aba Chaves da página do controle." : "ou segure Quadrado e fale no microfone do fone do controle.", 26, 690, 188, 0xFF8A96A3);
        desenharTexto(p, "Num PDF ou texto aberto, Triângulo lê em voz alta.", 26, 690, 226, 0xFF8A96A3);
    }
    desenharTexto(p, rod, 24, 60, 1010, corRod);
}

// ---- Tela "Chaves de API" (EXTRA): categoria > serviço ou conta > campos ----
static int chNivel = 0, chCat = 0, chItem = 0, chSel[3] = { 0, 0, 0 };
static int chImeModo = 0;                     // 1 valor de campo, 2 nome de chave nova, 3 nome da conta, 4 modelo, 5 chave da conta
static char chImeAlvo[64] = "";
static char chAviso[240] = "";
static uint64_t chApagarEm = 0;
static IaConta chRascunho;                    // conta sendo criada (ainda sem chave)
static int chContaId = -1;                    // conta aberta no nível 2 (-1 = rascunho)

void chavesAbrirEm(int categoria) {
    extern MenuLevel menuAtual;
    iaCarregar();
    chNivel = categoria >= 0 ? 1 : 0; chCat = categoria >= 0 ? categoria : 0;
    chSel[0] = chCat; chSel[1] = 0; chSel[2] = 0;
    chAviso[0] = 0;
    menuAtual = MENU_CHAVES;
}
void chavesAbrir() { chavesAbrirEm(-1); }

// Itens do nível 1 da categoria atual. Para IA: contas + "Adicionar". Outras: serviços (+ extras + "Adicionar").
static int chItensNivel1(int* servicos, char extras[][64], int* nExtras) {
    int n = 0; *nExtras = 0;
    if (chCat == 0) { pthread_mutex_lock(&ia.trava); n = ia.nContas; pthread_mutex_unlock(&ia.trava); return n + 1; }
    for (int s = 0; s < N_SERVICOS; s++) if (!strcmp(SERVICOS[s].categoria, CATEGORIAS[chCat][0])) servicos[n++] = s;
    if (chCat == 3) { *nExtras = chavesOutras(extras, 32); return *nExtras + 1; }
    return n;
}
static const char* chCampoDoNivel2(int i, ChaveDef* extra) {
    int serv[16]; char ex[32][64]; int nex;
    chItensNivel1(serv, ex, &nex);
    if (chCat == 3) { if (chItem < nex) { snprintf(chImeAlvo, sizeof(chImeAlvo), "%s", ex[chItem]); *extra = chaveExtra(chImeAlvo); return chImeAlvo; } return NULL; }
    const char* id = SERVICOS[serv[chItem]].campos[i];
    return id;
}
static int chNCampos() {
    if (chCat == 0) return 6;      // provedor, nome, modelo, chave, usar, apagar
    if (chCat == 3) return 1;
    int serv[16]; char ex[32][64]; int nex; chItensNivel1(serv, ex, &nex);
    int n = 0; while (n < 4 && SERVICOS[serv[chItem]].campos[n]) n++;
    return n;
}
static IaConta chContaAtual() {
    IaConta c = chRascunho;
    pthread_mutex_lock(&ia.trava);
    if (chContaId >= 0 && chContaId < ia.nContas) c = ia.contas[chContaId];
    pthread_mutex_unlock(&ia.trava);
    return c;
}

static void chAtualizarTeclado(int32_t usuario) {
    if (!chImeModo) return;
    static char texto[3200];
    int r = iaTecladoResultado(texto, sizeof(texto));
    if (r < 0) return;
    int modo = chImeModo; chImeModo = 0;
    if (r == 0) return;
    if (modo == 2) {
        chaveLimpar(texto);
        if (!chaveNomeValido(texto)) { snprintf(chAviso, sizeof(chAviso), "Nome inválido: use letras, números, _ - ."); return; }
        snprintf(chImeAlvo, sizeof(chImeAlvo), "%s", texto);
        char titulo[96]; snprintf(titulo, sizeof(titulo), "Chave: %s", texto);
        if (iaAbrirTecladoBase(usuario, titulo, 1000, NULL)) chImeModo = 1;
        return;
    }
    if (modo == 1) {
        const ChaveDef* d = chaveAchar(chImeAlvo);
        ChaveDef extra = chaveExtra(chImeAlvo);
        const char* erro = chaveGravar(d ? d : &extra, texto);
        snprintf(chAviso, sizeof(chAviso), erro ? "Não salvei: %s." : "Salva.", erro);
        return;
    }
    // Contas de IA.
    IaConta c = chContaAtual();
    if (chContaId < 0) {
        if (modo == 3) { iaLimparTexto(texto, true); snprintf(chRascunho.nome, sizeof(chRascunho.nome), "%s", texto); return; }
        if (modo == 4) { iaLimparTexto(texto, true); snprintf(chRascunho.modelo, sizeof(chRascunho.modelo), "%s", texto); return; }
        int id = -1;
        const char* erro = iaContaSalvar(-1, chRascunho.prov, chRascunho.nome, chRascunho.modelo, texto, &id);
        if (erro) snprintf(chAviso, sizeof(chAviso), "Não salvei: %s.", erro);
        else { chContaId = id; snprintf(chAviso, sizeof(chAviso), "Conta %s criada.", chRascunho.nome[0] ? chRascunho.nome : IA_PROV_CURTO[chRascunho.prov]); }
        return;
    }
    const char* erro = iaContaSalvar(chContaId, c.prov, modo == 3 ? texto : NULL, modo == 4 ? texto : NULL, modo == 5 ? texto : "", NULL);
    snprintf(chAviso, sizeof(chAviso), erro ? "Não salvei: %s." : "Salvo.", erro);
}

bool chavesControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    chAtualizarTeclado(usuario);
    if (chImeModo) return false;
    uint32_t novos = botoes & ~antigos;
    int serv[16]; char ex[32][64]; int nex = 0;
    int total = chNivel == 0 ? N_CATEGORIAS : chNivel == 1 ? chItensNivel1(serv, ex, &nex) : chNCampos();
    if (novos & ORBIS_PAD_BUTTON_CIRCLE) {
        chAviso[0] = 0; chApagarEm = 0;
        if (chNivel == 0) return true;
        chNivel--;
        return false;
    }
    int& sel = chSel[chNivel];
    if (sel >= total) sel = total > 0 ? total - 1 : 0;
    if (novos & ORBIS_PAD_BUTTON_UP) { sel = (sel + total - 1) % total; chApagarEm = 0; }
    if (novos & ORBIS_PAD_BUTTON_DOWN) { sel = (sel + 1) % total; chApagarEm = 0; }
    bool x = (novos & ORBIS_PAD_BUTTON_CROSS) != 0, quadrado = (novos & ORBIS_PAD_BUTTON_SQUARE) != 0;
    int lado = (novos & ORBIS_PAD_BUTTON_RIGHT) ? 1 : (novos & ORBIS_PAD_BUTTON_LEFT) ? -1 : 0;
    if (chNivel == 0) { if (x) { chCat = sel; chNivel = 1; chSel[1] = 0; chAviso[0] = 0; } return false; }
    if (chNivel == 1) {
        if (!x) return false;
        if (chCat == 0) {
            if (sel < total - 1) chContaId = sel;
            else { memset(&chRascunho, 0, sizeof(chRascunho)); chRascunho.prov = PROV_GEMINI; chContaId = -1; }
            chItem = sel; chNivel = 2; chSel[2] = chContaId < 0 ? 0 : 3; chAviso[0] = 0;
            return false;
        }
        if (chCat == 3 && sel == total - 1) { if (iaAbrirTecladoBase(usuario, "Nome da chave nova", 40, NULL)) chImeModo = 2; return false; }
        chItem = sel; chNivel = 2; chSel[2] = 0; chAviso[0] = 0;
        return false;
    }
    // Nível 2.
    if (chCat == 0) {
        IaConta c = chContaAtual();
        if (sel == 0 && (lado || x)) {
            int d = lado ? lado : 1, novo = (c.prov + d + PROV_N) % PROV_N;
            if (chContaId < 0) { chRascunho.prov = novo; chRascunho.modelo[0] = 0; }
            else { const char* e = iaContaSalvar(chContaId, novo, NULL, "", "", NULL); if (e) snprintf(chAviso, sizeof(chAviso), "%s", e); }
        }
        else if (sel == 1 && x) { if (iaAbrirTecladoBase(usuario, "Nome da conta (ex.: Gemini Trabalho)", 60, c.nome)) chImeModo = 3; }
        else if (sel == 2 && lado) {
            int n = iaNModelos(c.prov), atual = -1;
            for (int i = 0; i < n; i++) if (!strcmp(iaModeloDe(&c), IA_PROV_MODELOS[c.prov][i])) atual = i;
            const char* m = IA_PROV_MODELOS[c.prov][(atual + lado + n) % n];
            if (chContaId < 0) snprintf(chRascunho.modelo, sizeof(chRascunho.modelo), "%s", m);
            else iaContaSalvar(chContaId, c.prov, NULL, m, "", NULL);
        }
        else if (sel == 2 && x) { if (iaAbrirTecladoBase(usuario, "Nome do modelo", 90, iaModeloDe(&c))) chImeModo = 4; }
        else if (sel == 3 && x) { char t[96]; snprintf(t, sizeof(t), "Chave da API (%s)", IA_PROV_SITE[c.prov]); if (iaAbrirTecladoBase(usuario, t, 290, NULL)) chImeModo = 5; }
        else if (sel == 4 && x) {
            if (chContaId < 0) snprintf(chAviso, sizeof(chAviso), "Digite a chave primeiro.");
            else { iaContaUsar(chContaId); snprintf(chAviso, sizeof(chAviso), "A IA agora usa %s.", c.nome); }
        }
        else if (sel == 5 && (x || quadrado) && chContaId >= 0) {
            uint64_t agora = remotoAgoraMs();
            if (chApagarEm && agora - chApagarEm < 3000) { iaContaApagar(chContaId); chApagarEm = 0; chNivel = 1; chSel[1] = 0; snprintf(chAviso, sizeof(chAviso), "Conta %s apagada.", c.nome); }
            else { chApagarEm = agora; snprintf(chAviso, sizeof(chAviso), "Aperte de novo para apagar a conta %s.", c.nome); }
        }
        return false;
    }
    ChaveDef extra;
    const char* id = chCampoDoNivel2(sel, &extra);
    if (!id) return false;
    const ChaveDef* d = chaveAchar(id);
    if (!d) d = &extra;
    if (x) {
        snprintf(chImeAlvo, sizeof(chImeAlvo), "%s", d->id);
        char titulo[96]; snprintf(titulo, sizeof(titulo), "%s", d->nome);
        if (iaAbrirTecladoBase(usuario, titulo, 1000, NULL)) chImeModo = 1;
    }
    if (quadrado) {
        uint64_t agora = remotoAgoraMs();
        if (chApagarEm && agora - chApagarEm < 3000) { const char* e = chaveGravar(d, ""); snprintf(chAviso, sizeof(chAviso), e ? "Não apaguei: %s." : "Apagada.", e); chApagarEm = 0; }
        else { chApagarEm = agora; snprintf(chAviso, sizeof(chAviso), "Aperte Quadrado de novo para apagar."); }
    }
    return false;
}

void chavesRenderizar(uint32_t* p) {
    extern void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF101418;
    char titulo[160] = "Chaves de API";
    if (chNivel >= 1) snprintf(titulo, sizeof(titulo), "Chaves de API  >  %s", CATEGORIAS[chCat][1]);
    desenharTexto(p, titulo, 40, 60, 40, 0xFFFFFFFF);
    int serv[16]; char ex[32][64]; int nex = 0;
    char linhas[24][200]; char dir[24][64]; int n = 0;
    char v[2048], fim[8];
    if (chNivel == 0) {
        for (int k = 0; k < N_CATEGORIAS; k++) {
            snprintf(linhas[n], 200, "%s", CATEGORIAS[k][1]);
            if (k == 0) { pthread_mutex_lock(&ia.trava); snprintf(dir[n], 64, "%d conta(s)", ia.nContas); pthread_mutex_unlock(&ia.trava); }
            else { int cfg = 0, tot = 0; for (int s = 0; s < N_SERVICOS; s++) if (!strcmp(SERVICOS[s].categoria, CATEGORIAS[k][0])) { tot++; if (chaveLer(chaveAchar(SERVICOS[s].campos[0]), v, sizeof(v))) cfg++; }
                   if (k == 3) { tot = chavesOutras(ex, 32); cfg = tot; } snprintf(dir[n], 64, "%d de %d", cfg, tot); }
            n++;
        }
    }
    else if (chNivel == 1) {
        int total = chItensNivel1(serv, ex, &nex);
        for (int i = 0; i < total && n < 24; i++) {
            dir[n][0] = 0;
            if (chCat == 0) {
                if (i == total - 1) snprintf(linhas[n], 200, "+ Adicionar conta (Gemini, ChatGPT ou Groq)");
                else { pthread_mutex_lock(&ia.trava); const IaConta* c = &ia.contas[i]; chaveFim(c->chave, fim, sizeof(fim));
                       snprintf(linhas[n], 200, "%s%s", c->nome, i == ia.contaAtiva ? "   (em uso)" : ""); snprintf(dir[n], 64, "%s ...%s", IA_PROV_CURTO[c->prov], fim); pthread_mutex_unlock(&ia.trava); }
            }
            else if (chCat == 3) {
                if (i == total - 1) snprintf(linhas[n], 200, "+ Adicionar chave");
                else { ChaveDef d = chaveExtra(ex[i]); chaveLer(&d, v, sizeof(v)); chaveFim(v, fim, sizeof(fim)); snprintf(linhas[n], 200, "%s", ex[i]); snprintf(dir[n], 64, "...%s", fim); }
            }
            else {
                const ChaveServico* s = &SERVICOS[serv[i]];
                int cfg = 0, tot = 0;
                for (int f = 0; s->campos[f]; f++) { tot++; if (chaveLer(chaveAchar(s->campos[f]), v, sizeof(v))) cfg++; }
                snprintf(linhas[n], 200, "%s", s->nome);
                snprintf(dir[n], 64, cfg == tot ? "configurado" : (cfg ? "%d de %d" : "vazio"), cfg, tot);
            }
            n++;
        }
    }
    else if (chCat == 0) {
        IaConta c = chContaAtual();
        chaveFim(c.chave, fim, sizeof(fim));
        snprintf(linhas[n], 200, "Provedor: %s", IA_PROV_NOME[c.prov]); dir[n++][0] = 0;
        snprintf(linhas[n], 200, "Nome: %s", c.nome[0] ? c.nome : IA_PROV_CURTO[c.prov]); dir[n++][0] = 0;
        snprintf(linhas[n], 200, "Modelo: %s", iaModeloDe(&c)); dir[n++][0] = 0;
        snprintf(linhas[n], 200, "Chave: %s", c.chave[0] ? "configurada" : "vazia"); snprintf(dir[n++], 64, "%s", c.chave[0] ? fim : "");
        snprintf(linhas[n], 200, "Usar esta conta na IA"); dir[n++][0] = 0;
        snprintf(linhas[n], 200, "Apagar esta conta"); dir[n++][0] = 0;
    }
    else {
        int total = chNCampos();
        for (int i = 0; i < total; i++) {
            ChaveDef extra; const char* id = chCampoDoNivel2(i, &extra);
            const ChaveDef* d = id ? chaveAchar(id) : NULL; if (!d) d = &extra;
            bool tem = chaveLer(d, v, sizeof(v)); chaveFim(v, fim, sizeof(fim));
            snprintf(linhas[n], 200, "%s%s%s%s", d->nome, d->uso[0] ? " (" : "", d->uso, d->uso[0] ? ")" : "");
            snprintf(dir[n++], 64, tem ? "...%s" : "vazia", fim);
        }
    }
    int sel = chSel[chNivel];
    for (int i = 0; i < n; i++) {
        int y = 140 + i * 66;
        if (i == sel) iaRetangulo(p, 50, y, 1300, y + 58, 0xFF3D8BFD);
        desenharTexto(p, linhas[i], 28, 75, y + 14, i == sel ? 0xFFFFFFFF : 0xFFE8EDF2);
        desenharTexto(p, dir[i], 24, 1000, y + 16, i == sel ? 0xFFFFFFFF : 0xFF8A96A3);
    }
    // Explicação à direita.
    iaRetangulo(p, 1340, 130, 1880, 980, 0xFF1B2128);
    const char* ajuda[8] = { NULL };
    if (chNivel == 0) { ajuda[0] = "Escolha a categoria."; ajuda[1] = "Na IA dá para ter várias contas"; ajuda[2] = "do mesmo provedor (trabalho, lazer)"; ajuda[3] = "e escolher qual usar."; }
    else if (chCat == 0 && chNivel == 2) { ajuda[0] = "Esquerda/Direita: provedor e modelo."; ajuda[1] = "X: digita nome, modelo ou chave."; ajuda[2] = "Chaves grátis:"; ajuda[3] = "aistudio.google.com (Gemini)"; ajuda[4] = "console.groq.com (Groq)"; ajuda[5] = "platform.openai.com (ChatGPT)"; }
    else if (chNivel == 2) { ajuda[0] = "X digita a chave."; ajuda[1] = "Quadrado duas vezes apaga."; }
    else { ajuda[0] = "X abre."; ajuda[1] = "Pelo celular é mais fácil:"; ajuda[2] = "aba Chaves da página do controle,"; ajuda[3] = "que também importa do PC."; }
    for (int i = 0; ajuda[i]; i++) desenharTexto(p, ajuda[i], 24, 1370, 160 + i * 40, 0xFFB8C2CC);
    desenharTexto(p, chAviso[0] ? chAviso : "X abre / edita   Quadrado (2x) apaga   O volta", 24, 60, 1010, chAviso[0] ? 0xFFFFE08A : 0xFF8A96A3);
}

// ---- Tela do PC no PS4 (o HyperNeivaPC.exe manda JPEGs) ----
volatile bool remotoEspelhando = false;
static unsigned char* espelhoPendente = NULL;
static int espelhoW = 0, espelhoH = 0;
static uint64_t espelhoUltimo = 0;
static bool espelhoRecusar = false;      // o usuário fechou com a Bolinha
static bool espelhoPedirParada = false;

static bool remotoEspelhoQuadro(const uint8_t* jpeg, int tam) {
    pthread_mutex_lock(&remotoTrava);
    // Depois de fechar, recusa até o PC parar de mandar por uns segundos.
    if (espelhoRecusar && remotoAgoraMs() - espelhoUltimo > 3000) espelhoRecusar = false;
    bool recusar = espelhoRecusar;
    espelhoUltimo = remotoAgoraMs();
    pthread_mutex_unlock(&remotoTrava);
    if (recusar) return false;
    int w = 0, h = 0, canais = 0;
    unsigned char* img = stbi_load_from_memory(jpeg, tam, &w, &h, &canais, 4);
    if (!img) return true;
    pthread_mutex_lock(&remotoTrava);
    if (espelhoPendente) stbi_image_free(espelhoPendente);
    espelhoPendente = img; espelhoW = w; espelhoH = h;
    pthread_mutex_unlock(&remotoTrava);
    return true;
}

static void remotoEspelhoParar() {
    pthread_mutex_lock(&remotoTrava);
    espelhoPedirParada = true;
    pthread_mutex_unlock(&remotoTrava);
}

// Thread principal: troca o quadro exibido e fecha quando o PC para de mandar.
static void remotoAtenderEspelho() {
    extern bool visualizandoMidiaImagem, fullscreenMidia, emuladorAtivo, bloqueio_fechar_video;
    extern unsigned char* imgMidia; extern int wM, hM, panMidiaX, panMidiaY; extern float zoomMidia;
    extern void pararVideo();
    if (remotoEspelhando && !visualizandoMidiaImagem) {
        // Fechado no controle (Bolinha): o viewer já liberou a imagem.
        remotoEspelhando = false;
        pthread_mutex_lock(&remotoTrava); espelhoRecusar = true; pthread_mutex_unlock(&remotoTrava);
        remotoLog("ESPELHO fechado no PS4");
        if (pcControlando) {
            pcControlando = false;
            pcPedir("botao	esq	0"); pcPedir("botao	dir	0");
            pcPedir("espelho	0");
        }
        return;
    }
    pthread_mutex_lock(&remotoTrava);
    unsigned char* novo = espelhoPendente; int w = espelhoW, h = espelhoH;
    espelhoPendente = NULL;
    bool parar = espelhoPedirParada || (remotoEspelhando && remotoAgoraMs() - espelhoUltimo > 5000);
    espelhoPedirParada = false;
    pthread_mutex_unlock(&remotoTrava);
    if (parar && remotoEspelhando) {
        if (novo) stbi_image_free(novo);
        remotoEspelhando = false;
        pcControlando = false;
        visualizandoMidiaImagem = false;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        remotoAviso("TELA DO PC ENCERRADA");
        return;
    }
    if (!novo) return;
    if (emuladorAtivo) { stbi_image_free(novo); return; }
    if (!remotoEspelhando) {
        if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); }
        remotoEspelhando = true;
        remotoAviso("TELA DO PC NO PS4 - [O] FECHA");
        remotoLog("ESPELHO comecou %dx%d", w, h);
    }
    if (imgMidia) stbi_image_free(imgMidia);
    imgMidia = novo; wM = w; hM = h;
    visualizandoMidiaImagem = true; fullscreenMidia = true; zoomMidia = 1.0f; panMidiaX = 0; panMidiaY = 0;
}

// ---- FTP > [PC] Tela do PC: pede a tela ao HyperNeivaPC.exe e liga o controle ----
static uint64_t pcTelaPedidaMs = 0;
static int iaTecladoResultado(char* texto, int tam);

void pcAbrirTela() {
    if (!pcOnline()) { remotoAviso("ABRA O HyperNeivaPC.exe NO PC (BAIXE NA PAGINA DO PS4, ABA ARQUIVOS)"); return; }
    pthread_mutex_lock(&remotoTrava); espelhoRecusar = false; pthread_mutex_unlock(&remotoTrava);
    pcPedir("espelho\t1\t1280\t15\t65");
    pcControlando = true;
    pcTelaPedidaMs = remotoAgoraMs();
    remotoAviso("TELA DO PC: ANALOGICO/TOUCHPAD = MOUSE  X CLIQUE  QUADRADO DIREITO  TRIANGULO DIGITAR  O SAIR");
}

static void pcAtender() {
    if (pcImeAberto) {
        char t[2048];
        int r = iaTecladoResultado(t, sizeof(t));
        if (r >= 0) {
            pcImeAberto = false;
            if (r == 1) {
                for (char* c = t; *c; c++) if (*c == '\t' || *c == '\n' || *c == '\r') *c = ' ';
                char l[2100]; snprintf(l, sizeof(l), "texto\t%s", t);
                pcPedir(l);
            }
        }
    }
    if (pcControlando && remotoEspelhando) pcTelaPedidaMs = 0;
    if (pcControlando && !remotoEspelhando && pcTelaPedidaMs && remotoAgoraMs() - pcTelaPedidaMs > 8000) {
        pcControlando = false; pcTelaPedidaMs = 0;
        remotoAviso("O PC NAO MANDOU A TELA. O HyperNeivaPC.exe ESTA ABERTO?");
    }
}

// ---- FTP > [PC] Arquivos do PC: navega no PC e copia para o PS4 ----
struct PcItem { char nome[200]; char caminho[512]; bool pasta; long long tam; };
#define PC_MAX_ITENS 3000
static PcItem* pcItens = NULL;
static int pcTotal = 0, pcSel = 0, pcOff = 0;
static char pcPasta[512] = "";
static int pcEsperando = 0;
static uint64_t pcEsperandoDesde = 0;
static char pcAvisoTela[300] = "";

static void pcPedirLista(const char* pasta) {
    snprintf(pcPasta, sizeof(pcPasta), "%s", pasta);
    pcEsperando = pcListar(pasta);
    pcEsperandoDesde = remotoAgoraMs();
    pcTotal = 0; pcSel = 0; pcOff = 0;
}

void pcArquivosAbrir() {
    extern MenuLevel menuAtual;
    if (!pcOnline()) { remotoAviso("ABRA O HyperNeivaPC.exe NO PC (BAIXE NA PAGINA DO PS4, ABA ARQUIVOS)"); return; }
    if (!pcItens) pcItens = (PcItem*)malloc(sizeof(PcItem) * PC_MAX_ITENS);
    if (!pcItens) return;
    menuAtual = MENU_PC_ARQUIVOS;
    pcAvisoTela[0] = 0;
    pcPedirLista("");
}

static void pcLerResposta() {
    if (!pcEsperando) return;
    char* r = pcPegarResposta(pcEsperando);
    if (!r) {
        if (remotoAgoraMs() - pcEsperandoDesde > 8000) { pcEsperando = 0; snprintf(pcAvisoTela, sizeof(pcAvisoTela), "O PC nao respondeu. O HyperNeivaPC.exe esta aberto?"); }
        return;
    }
    pcEsperando = 0; pcTotal = 0;
    char* linha = r;
    bool primeira = true;
    while (linha && *linha) {
        char* fim = strchr(linha, '\n'); if (fim) *fim = 0;
        char* campos[4] = { linha, NULL, NULL, NULL };
        for (int k = 1; k < 4; k++) { char* t = campos[k - 1] ? strchr(campos[k - 1], '\t') : NULL; if (t) { *t = 0; campos[k] = t + 1; } }
        if (primeira) {
            primeira = false;
            if (!strcmp(campos[0], "erro")) snprintf(pcAvisoTela, sizeof(pcAvisoTela), "PC: %s", campos[1] ? campos[1] : "erro");
        }
        else if (campos[1] && campos[2] && pcTotal < PC_MAX_ITENS) {
            PcItem* it = &pcItens[pcTotal++];
            it->pasta = campos[0][0] == 'd';
            snprintf(it->nome, sizeof(it->nome), "%s", campos[1]);
            snprintf(it->caminho, sizeof(it->caminho), "%s", campos[2]);
            it->tam = campos[3] ? atoll(campos[3]) : 0;
        }
        linha = fim ? fim + 1 : NULL;
    }
    free(r);
}

static const char* pcDestinoPara(const PcItem* it) {
    if (it->pasta) return "/data/HyperNeiva/baixado/pc";
    const char* p = strrchr(it->nome, '.');
    if (!p) return "/data/HyperNeiva/baixado/pc";
    static const char* videos[] = { ".mp4", ".mkv", ".avi", ".mov", ".wmv", ".m4v", ".webm", ".ts", ".flv", ".mpg", ".mpeg", ".srt", ".ass", ".vtt", NULL };
    static const char* sons[] = { ".mp3", ".wav", ".flac", ".m4a", ".ogg", ".aac", ".opus", ".lrc", NULL };
    static const char* imagens[] = { ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp", NULL };
    static const char* leitura[] = { ".pdf", ".txt", ".docx", ".odt", ".epub", ".cbz", ".md", NULL };
    if (!strcasecmp(p, ".pkg")) return "/data/pkg";
    for (int i = 0; videos[i]; i++) if (!strcasecmp(p, videos[i])) return "/data/HyperNeiva/midia/videos";
    for (int i = 0; sons[i]; i++) if (!strcasecmp(p, sons[i])) return "/data/HyperNeiva/Musicas";
    for (int i = 0; imagens[i]; i++) if (!strcasecmp(p, imagens[i])) return "/data/HyperNeiva/midia/imagens";
    for (int i = 0; leitura[i]; i++) if (!strcasecmp(p, leitura[i])) return "/data/HyperNeiva/midia/leitura";
    return "/data/HyperNeiva/baixado/pc";
}

static void pcCopiar(const PcItem* it) {
    const char* destino = pcDestinoPara(it);
    remotoCriarPastas(destino);
    char l[1200]; snprintf(l, sizeof(l), "copiar\t%s\t%s", it->caminho, destino);
    pcPedir(l);
    snprintf(pcAvisoTela, sizeof(pcAvisoTela), "Copiando %s para %s", it->nome, destino);
}

// Devolve true para sair da tela (Bolinha na lista de discos).
bool pcArquivosControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    (void)usuario;
    pcLerResposta();
    uint32_t n = botoes & ~antigos;
    if (n & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pcPasta[0]) return true;
        char pai[512]; snprintf(pai, sizeof(pai), "%s", pcPasta);
        size_t k = strlen(pai);
        if (k == 3 && pai[1] == ':') pai[0] = 0;            // "C:/" -> lista de discos
        else {
            while (k > 3 && pai[k - 1] == '/') pai[--k] = 0;
            char* b = strrchr(pai, '/');
            if (!b) pai[0] = 0;
            else { *b = 0; if (strlen(pai) == 2 && pai[1] == ':') strcat(pai, "/"); }
        }
        pcPedirLista(pai);
        return false;
    }
    if (n & ORBIS_PAD_BUTTON_TRIANGLE) { pcPedirLista(pcPasta); return false; }
    if (pcEsperando || pcTotal == 0) return false;
    if (n & ORBIS_PAD_BUTTON_UP) pcSel = (pcSel + pcTotal - 1) % pcTotal;
    if (n & ORBIS_PAD_BUTTON_DOWN) pcSel = (pcSel + 1) % pcTotal;
    if (n & ORBIS_PAD_BUTTON_L1) { pcSel -= 15; if (pcSel < 0) pcSel = 0; }
    if (n & ORBIS_PAD_BUTTON_R1) { pcSel += 15; if (pcSel >= pcTotal) pcSel = pcTotal - 1; }
    PcItem* it = &pcItens[pcSel];
    if (n & ORBIS_PAD_BUTTON_CROSS) { if (it->pasta) pcPedirLista(it->caminho); else pcCopiar(it); }
    else if (n & ORBIS_PAD_BUTTON_SQUARE) pcCopiar(it);
    return false;
}

void pcArquivosRenderizar(uint32_t* p) {
    pcLerResposta();
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF0F1216;
    char nome[64], estado[160];
    pcInfo(nome, sizeof(nome), estado, sizeof(estado));
    char t[700];
    snprintf(t, sizeof(t), "Arquivos do PC%s%s", nome[0] ? "  -  " : "", nome);
    desenharTexto(p, t, 40, 60, 40, 0xFFFFFFFF);
    snprintf(t, sizeof(t), "%s", pcPasta[0] ? pcPasta : "Discos e pastas do computador");
    desenharTexto(p, t, 24, 60, 100, 0xFF29B6F6);
    const int y0 = 160, passo = 44, visiveis = 17;
    if (pcEsperando) desenharTexto(p, "Pedindo a lista ao PC...", 28, 60, y0 + 10, 0xFFB8C2CC);
    else if (!pcTotal) desenharTexto(p, "Pasta vazia", 28, 60, y0 + 10, 0xFFB8C2CC);
    if (pcSel < pcOff) pcOff = pcSel;
    if (pcSel >= pcOff + visiveis) pcOff = pcSel - visiveis + 1;
    for (int k = 0; k < visiveis && pcOff + k < pcTotal && !pcEsperando; k++) {
        int i = pcOff + k, y = y0 + k * passo;
        PcItem* it = &pcItens[i];
        if (i == pcSel) for (int yy = y - 4; yy < y + passo - 6; yy++) for (int x = 50; x < 1870; x++) p[yy * 1920 + x] = 0xFF243040;
        uint32_t corIcone = it->pasta ? 0xFFE8B931 : 0xFF8A96A3;
        for (int yy = y + 6; yy < y + 26; yy++) for (int x = 64; x < 88; x++) p[yy * 1920 + x] = corIcone;
        desenharTexto(p, it->nome, 28, 104, y, i == pcSel ? 0xFFFFFFFF : 0xFFD0D6DC);
        if (!it->pasta) {
            char tam[32];
            if (it->tam >= 1024LL * 1024 * 1024) snprintf(tam, sizeof(tam), "%.1f GB", it->tam / 1073741824.0);
            else if (it->tam >= 1024 * 1024) snprintf(tam, sizeof(tam), "%.1f MB", it->tam / 1048576.0);
            else snprintf(tam, sizeof(tam), "%lld KB", it->tam / 1024);
            desenharTexto(p, tam, 24, 1700, y + 2, 0xFF8A96A3);
        }
    }
    if (pcTotal && !pcEsperando) {
        snprintf(t, sizeof(t), "Destino no PS4: %s", pcDestinoPara(&pcItens[pcSel]));
        desenharTexto(p, t, 24, 60, 930, 0xFFB8C2CC);
    }
    if (estado[0]) { snprintf(t, sizeof(t), "PC: %s", estado); desenharTexto(p, t, 24, 60, 966, 0xFFFFE08A); }
    else if (pcAvisoTela[0]) desenharTexto(p, pcAvisoTela, 24, 60, 966, 0xFFFFE08A);
    desenharTexto(p, "X abre a pasta / copia o arquivo   Quadrado copia (pasta inteira tambem)   Triangulo atualiza   L1/R1 pagina   O volta", 22, 60, 1020, 0xFF8A96A3);
}

// ---- Thread principal: abre o que chegou do celular ----
void remotoAtender() {
    if (!remotoLigado) return;
    // Mantém o estado do teclado em dia para a página saber se pode mandar texto.
    if (imeAtivo) hnImeStatus();

    char texto[2048];
    if (remotoPegarTexto(texto, sizeof(texto))) remotoEntregarTexto(texto);

    remotoAtenderDlna();
    remotoAtenderEspelho();
    pcAtender();
    char caminho[512];
    if (!remotoPegarMidia(caminho, sizeof(caminho))) return;
    remotoAbrirMidia(caminho);
}

static void remotoAbrirMidia(const char* caminho) {
    extern bool emuladorAtivo, video_minimizado, bloqueio_fechar_video, menu_video_aberto;
    extern bool visualizandoMidiaImagem, visualizandoMidiaTexto, visualizandoPic1, fullscreenMidia;
    extern unsigned char* imgMidia; extern int wM, hM, panMidiaX, panMidiaY; extern float zoomMidia;
    extern char caminhoImagemAberta[512];
    extern void pararVideo(), iniciarVideoMP4(const char* caminho);
    if (emuladorAtivo) { remotoAviso("CELULAR: FECHE O JOGO PARA VER A MIDIA"); return; }
    const char* nome = strrchr(caminho, '/');
    nome = nome ? nome + 1 : caminho;
    int tipo = remotoTipoArquivo(caminho);
    remotoLog("ABRIR tipo=%d %s", tipo, caminho);
    if (tipo == 0) {
        int w = 0, h = 0, canais = 0;
        unsigned char* img = stbi_load(caminho, &w, &h, &canais, 4);
        if (!img) { remotoAviso("CELULAR: NAO CONSEGUI ABRIR A IMAGEM"); remotoLog("stbi falhou: %s", stbi_failure_reason()); return; }
        if (videoRodando) { bloqueio_fechar_video = false; pararVideo(); }
        visualizandoMidiaTexto = false; visualizandoPic1 = false;
        if (imgMidia) stbi_image_free(imgMidia);
        imgMidia = img; wM = w; hM = h;
        visualizandoMidiaImagem = true; zoomMidia = 1.0f; fullscreenMidia = true; panMidiaX = 0; panMidiaY = 0;
        snprintf(caminhoImagemAberta, sizeof(caminhoImagemAberta), "%s", caminho);
        char aviso[128]; snprintf(aviso, sizeof(aviso), "CELULAR: %s", nome); remotoAviso(aviso);
    }
    else if (tipo == 1) {
        if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } }
        visualizandoMidiaTexto = false; visualizandoPic1 = false;
        menu_video_aberto = false;
        bloqueio_fechar_video = false;
        // Vídeo remoto: fecha o atual antes, senão o fechamento dele levaria a fonte nova junto.
        if (videoRodando) pararVideo();
        iniciarVideoMP4(caminho);
        video_minimizado = false;
        if (!videoRodando) remotoAviso("CELULAR: O VIDEO NAO ABRIU (VEJA remoto_log.txt)");
    }
}

// ---- DLNA na thread principal ----
static volatile bool dlnaBaixandoImagem = false;
static bool dlnaVideoNosso = false;
static char dlnaUrlTocando[2048] = "";

static void* dlnaThreadImagem(void*) {
    char url[2048];
    pthread_mutex_lock(&remotoTrava); snprintf(url, sizeof(url), "%s", dlnaUrl); pthread_mutex_unlock(&remotoTrava);
    char destino[300]; snprintf(destino, sizeof(destino), "%s/dlna_imagem.jpg", REMOTO_PASTA);
    bool ok = false;
    if (remotoFonteAbrirHttp(url)) {
        unsigned id = remotoFonteIdAtual();
        mkdir(REMOTO_PASTA, 0777);
        FILE* f = fopen(destino, "wb");
        uint8_t* buf = (uint8_t*)malloc(65536);
        long long total = 0;
        if (f && buf) {
            for (;;) {
                int n = remotoFonteLer(id, buf, 65536);
                if (n < 0) { total = -1; break; }
                if (n == 0) break;
                fwrite(buf, 1, n, f);
                total += n;
                if (total > 64LL * 1024 * 1024) { total = -1; break; }
            }
        }
        if (f) fclose(f);
        free(buf);
        remotoFonteFecharSe(id);
        ok = total > 0;
        remotoLog("DLNA imagem %lld bytes de %s", total, url);
    }
    if (ok) { remotoPedirMidia(destino); dlnaEstado = 2; }
    else { remotoAviso("DLNA: NAO CONSEGUI BAIXAR A IMAGEM"); dlnaEstado = 1; }
    dlnaBaixandoImagem = false;
    return NULL;
}

static void remotoAtenderDlna() {
    double alvo = 0; bool imagem = false; char url[2048];
    int comando = remotoDlnaComando(&alvo, url, sizeof(url), &imagem);
    // O vídeo do DLNA acabou ou foi fechado no controle: o app do celular vê "parado".
    if (dlnaVideoNosso && !videoRodando) { dlnaVideoNosso = false; if (dlnaEstado == 2 || dlnaEstado == 3) dlnaEstado = 1; }
    if (!comando) return;
    extern bool emuladorAtivo, bloqueio_fechar_video, video_minimizado, menu_video_aberto;
    extern bool visualizandoMidiaImagem; extern unsigned char* imgMidia;
    extern void pararVideo(), iniciarVideoMP4(const char* caminho), pularParaTempo(double tempo);
    remotoLog("DLNA comando %d imagem=%d", comando, imagem ? 1 : 0);
    if (comando == 1) {
        if (emuladorAtivo) { remotoAviso("DLNA: FECHE O JOGO PARA VER A MIDIA"); return; }
        if (imagem) {
            if (dlnaBaixandoImagem) return;
            if (dlnaVideoNosso && videoRodando) { bloqueio_fechar_video = false; pararVideo(); dlnaVideoNosso = false; }
            dlnaBaixandoImagem = true; dlnaEstado = 4;
            if (!remotoCriarThread(dlnaThreadImagem, NULL)) dlnaBaixandoImagem = false;
            return;
        }
        // Play depois de Pause continua de onde parou.
        if (dlnaVideoNosso && videoRodando && strcmp(url, dlnaUrlTocando) == 0) { video_pausado = false; dlnaEstado = 2; return; }
        if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } }
        bloqueio_fechar_video = false;
        if (videoRodando) pararVideo();
        if (!remotoFonteAbrirHttp(url)) { remotoAviso("DLNA: LINK NAO SUPORTADO (PRECISA SER http://IP)"); dlnaEstado = 1; return; }
        menu_video_aberto = false;
        iniciarVideoMP4("remoto://dlna/video");
        video_minimizado = false;
        dlnaVideoNosso = videoRodando;
        snprintf(dlnaUrlTocando, sizeof(dlnaUrlTocando), "%s", url);
        dlnaEstado = videoRodando ? 2 : 1;
        remotoAviso(videoRodando ? "DLNA: TOCANDO" : "DLNA: O VIDEO NAO ABRIU");
    }
    else if (comando == 2) { if (videoRodando) { video_pausado = true; dlnaEstado = 3; } }
    else if (comando == 3) {
        if (dlnaVideoNosso && videoRodando) { bloqueio_fechar_video = false; pararVideo(); }
        dlnaVideoNosso = false;
        dlnaEstado = 1;
    }
    else if (comando == 4) { if (videoRodando) pularParaTempo(alvo); }
}
#endif
