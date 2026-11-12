#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef SO_NOSIGPIPE
#define SO_NOSIGPIPE 0x0800 
#endif

#include <orbis/Sysmodule.h>
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/UserService.h>

#include "controle_explorar.h"
#include "menu.h"
#include "explorar.h"
#include "stb_image.h" 
#include "audio.h"   
#include "bloco_de_notas.h"
#include "miniz.h" 
#include "video.h" // <--- CABEï¿½ALHO DO Vï¿½DEO ADICIONADO AQUI

extern void preencherOpcoesContexto(const char* nomeArquivo);
extern void acaoArquivo(int idxOpcao);
extern void iniciarEmulador(const char* romPath);

extern bool visualizandoMidiaImagem;
extern bool video_minimizado;
extern unsigned char* imgMidia;
extern int wM, hM;

extern const char* listaOpcoes[150];
extern int mapOpcoes[150];
extern int totalOpcoes;

extern int cd;
extern void preencherExplorerHome();
extern void preencherRoot();
extern void atualizarBarra(float progresso);
extern bool ehArquivoVideo(const char* nome);

extern float zoomMidia;
extern bool fullscreenMidia;
extern int panMidiaX, panMidiaY;
extern char caminhoNavegacaoMusicas[512];
static char caminhoMusicaTocando[512] = "";
// Tocar ou parar um áudio do Explorar (opção "tocar" do menu do X, em explorar.cpp).
void explorarTocarAudio(const char* caminhoArquivo) {
    if (strcmp(caminhoMusicaTocando, caminhoArquivo) == 0) { tocarMusicaNova("PARADO"); strcpy(caminhoMusicaTocando, ""); sprintf(msgStatus, "Musica Parada"); msgTimer = 90; }
    else { tocarMusicaNova(caminhoArquivo); snprintf(caminhoMusicaTocando, sizeof(caminhoMusicaTocando), "%s", caminhoArquivo); sprintf(msgStatus, "Reproduzindo Audio"); msgTimer = 90; }
}

extern int offEsq;
extern int off;

extern char caminhoImagemAberta[512];

extern unsigned char* imgPreview;
extern int wP, hP, cP;

extern char msgStatus[128];
extern int msgTimer;

extern char baseRaiz[256];
extern bool esperandoNomePasta;
extern bool esperandoRenomear;
extern bool marcados[3000];
extern bool marcadosEsq[3000];
extern bool selecionandoMidiaElemento;

extern void listarDiretorio(const char*);
extern void listarDiretorioEsq(const char*);

static void carregarPreviewArquivo(const char* caminhoAbsoluto) {
    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
    char tempPathAbs[512]; strcpy(tempPathAbs, caminhoAbsoluto);
    for (int i = 0; tempPathAbs[i]; i++) { tempPathAbs[i] = tolower(tempPathAbs[i]); }
    if (strstr(tempPathAbs, ".png") || strstr(tempPathAbs, ".jpg") || strstr(tempPathAbs, ".jpeg") || strstr(tempPathAbs, ".bmp")) {
        imgPreview = stbi_load(caminhoAbsoluto, &wP, &hP, &cP, 4);
    }
    else if (strstr(tempPathAbs, ".xavatar")) {
        mz_zip_archive zip_archive; memset(&zip_archive, 0, sizeof(zip_archive));
        if (mz_zip_reader_init_file(&zip_archive, caminhoAbsoluto, 0)) {
            const char* tempPath = "/data/HyperNeiva/configuracao/temporario/temp_preview.png";
            bool extraiu = false;
            mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
            for (mz_uint i = 0; i < num_files; i++) {
                mz_zip_archive_file_stat file_stat;
                if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
                if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;
                char tempExtZip[256]; strcpy(tempExtZip, file_stat.m_filename);
                for (int k = 0; tempExtZip[k]; k++) tempExtZip[k] = tolower(tempExtZip[k]);
                if (strstr(tempExtZip, ".png") || strstr(tempExtZip, ".jpg") || strstr(tempExtZip, ".jpeg")) {
                    if (mz_zip_reader_extract_to_file(&zip_archive, i, tempPath, 0)) { extraiu = true; break; }
                }
            }
            if (extraiu) imgPreview = stbi_load(tempPath, &wP, &hP, &cP, 4);
            mz_zip_reader_end(&zip_archive);
        }
    }
}

char caminhoPkgAtual[512] = ""; bool servidorRodando = false;
extern bool hnEhParteCompactada(const char* nomeMinusculo);
extern void hnTituloPkg(const char* caminho, char* saida, int tamanho);
extern long long hnBytesFaltandoPkg(const char* caminho);

// O console pede cada PKG pelo content id: várias instalações podem correr juntas.
static struct { char id[40]; char caminho[512]; char titulo[128]; char* url; char* urlOriginal; long long tamanho; } pkgsServidor[32];
static int proximoPkgServidor = 0;

static int vagaDoPedido(const char* pedido) {
    for (int i = 0; i < 32; i++) if (pkgsServidor[i].id[0] && strstr(pedido, pkgsServidor[i].id)) return i;
    return -1;
}
static const char* caminhoDoPedido(const char* pedido) {
    for (int i = 0; i < 32; i++) if (pkgsServidor[i].id[0] && strstr(pedido, pkgsServidor[i].id)) return pkgsServidor[i].caminho;
    return caminhoPkgAtual;
}

// Entrega o PKG ao download do sistema (instalação de arquivo local). O sistema pede em
// pedaços ("bytes=X-Y"): responder do X até o fim do arquivo fazia vários pedidos lerem o
// arquivo inteiro ao mesmo tempo, e a instalação ia a 0,2 MB/s.
static volatile int pkgServidorPedidos = 0;
static void pkgServidorLog(int n, long long a, long long b, long long c, int head) {
    if (n > 40 && n % 200) return;
    FILE* lg = fopen("/data/HyperNeiva/pkg_servidor_log.txt", n == 1 ? "w" : "a");
    if (!lg) return;
    fprintf(lg, "#%d pedido %lld-%lld de %lld%s\n", n, a, b, c, head ? " (HEAD)" : "");
    fclose(lg);
}
static bool enviarTudoPkg(int fd, const char* dados, size_t n) {
    while (n > 0) { ssize_t k = send(fd, dados, n, 0); if (k <= 0) return false; dados += k; n -= (size_t)k; }
    return true;
}
// ---- "Pelo PS4" com link da internet: o sistema recusa link longo (o do GitHub tem 900+
// letras: 0x80990033), mas aceita o do app. O app busca cada pedaço pedido no link de verdade.
extern int httpCtxId;
extern int skipSslCallback(int libsslId, unsigned int verifyErr, void* const sslCert[], int certNum, void* userArg);
extern bool hnResolverLink(const char* url, char* saida, size_t tam);
static pthread_mutex_t pkgWebTrava = PTHREAD_MUTEX_INITIALIZER;
static int pkgWebTarefa[32];
static bool pkgWebEnviar(int fd, int vaga, long long ini, long long fim) {
    long long total = fim - ini + 1;
    char* buffer = (char*)malloc(256 * 1024);
    if (!buffer) return false;
    bool ok = false;
    for (int tentativa = 0; tentativa < 2 && !ok; tentativa++) {
        char url[4096];
        pthread_mutex_lock(&pkgWebTrava); snprintf(url, sizeof(url), "%s", pkgsServidor[vaga].url ? pkgsServidor[vaga].url : ""); pthread_mutex_unlock(&pkgWebTrava);
        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
        if (tpl < 0) break;
        sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
        ((int (*)(int, size_t))(void*)sceHttpSetResponseHeaderMaxSize)(tpl, 32768);
        ((int (*)(int, int))(void*)sceHttpSetAutoRedirect)(tpl, 1);
        int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
        int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
        int32_t status = 0;
        if (req >= 0) {
            char faixa[80]; snprintf(faixa, sizeof(faixa), "bytes=%lld-%lld", ini, fim);
            sceHttpAddRequestHeader(req, "Range", faixa, 1);
            if (sceHttpSendRequest(req, NULL, 0) >= 0) sceHttpGetStatusCode(req, &status);
            if (status == 206 || (status == 200 && ini == 0)) {
                long long enviado = 0; int n;
                while (enviado < total && (n = sceHttpReadData(req, buffer, (int)(total - enviado < 256 * 1024 ? total - enviado : 256 * 1024))) > 0) {
                    if (!enviarTudoPkg(fd, buffer, (size_t)n)) break;
                    enviado += n;
                }
                ok = enviado == total;
            }
        }
        if (req >= 0) sceHttpDeleteRequest(req);
        if (conn >= 0) sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tpl);
        FILE* lg = fopen("/data/HyperNeiva/pkg_servidor_log.txt", "a");
        if (lg) {
            OrbisBgftTaskProgress pr; memset(&pr, 0, sizeof(pr));
            int rp = pkgWebTarefa[vaga] > 0 ? sceBgftServiceDownloadGetProgress(pkgWebTarefa[vaga], &pr) : -1;
            fprintf(lg, "web %lld-%lld status %d %s | sistema rc 0x%08X %u/%u bytes, total %u/%u, erro 0x%08X\n", ini, fim, (int)status, ok ? "ok" : "falhou",
                (unsigned)rp, pr.transferred, pr.length, pr.transferredTotal, pr.lengthTotal, (unsigned)pr.errorResult);
            fclose(lg);
        }
        // O link assinado do GitHub vence em ~1 h: resolve de novo a partir do original.
        if (!ok && tentativa == 0 && (status == 403 || status == 404 || status == 410 || status == 400) && pkgsServidor[vaga].urlOriginal) {
            char novo[4096];
            if (hnResolverLink(pkgsServidor[vaga].urlOriginal, novo, sizeof(novo))) {
                pthread_mutex_lock(&pkgWebTrava); free(pkgsServidor[vaga].url); pkgsServidor[vaga].url = strdup(novo); pthread_mutex_unlock(&pkgWebTrava);
            }
            else break;
        }
        else break;
    }
    free(buffer);
    return ok;
}

// ---- Ponte rápida: baixa o arquivo adiantado em 2 conexões, em blocos de 8 MB, para um
// arquivo temporário; o PS4 lê do disco o que já chegou. Antes, cada pedaço pedido pelo PS4
// era buscado na hora, numa conexão só, no ritmo do sistema.
#define PONTE_BLOCO (8LL * 1024 * 1024)
#define PONTE_TRABALHADORES 2
static struct {
    volatile bool ativa, parar;
    int vaga, fd, blocos;
    long long tamanho;
    volatile uint8_t* estado;          // 0 falta, 1 baixando, 2 pronto
    volatile int prioridade;           // bloco que o PS4 está pedindo
    volatile int rodando;
    volatile long long baixados;
    uint64_t inicioMs;
    char arquivo[300];
} ponte;
static pthread_mutex_t ponteTrava = PTHREAD_MUTEX_INITIALIZER;
static uint64_t ponteAgoraMs() { return sceKernelGetProcessTime() / 1000; }
static void ponteLog(const char* formato, long long a, long long b, long long c, long long d) {
    FILE* lg = fopen("/data/HyperNeiva/pkg_servidor_log.txt", "a");
    if (!lg) return;
    fprintf(lg, "[%llu ms] ", (unsigned long long)(ponteAgoraMs() - ponte.inicioMs));
    fprintf(lg, formato, a, b, c, d); fputc('\n', lg); fclose(lg);
}
// Baixa um bloco do link para o arquivo. Renova o link assinado se ele vencer.
static bool ponteBaixarBloco(int b, char* buffer) {
    long long ini = (long long)b * PONTE_BLOCO, fim = ini + PONTE_BLOCO - 1;
    if (fim > ponte.tamanho - 1) fim = ponte.tamanho - 1;
    long long total = fim - ini + 1;
    for (int tentativa = 0; tentativa < 4 && !ponte.parar; tentativa++) {
        char url[4096];
        pthread_mutex_lock(&pkgWebTrava); snprintf(url, sizeof(url), "%s", pkgsServidor[ponte.vaga].url ? pkgsServidor[ponte.vaga].url : ""); pthread_mutex_unlock(&pkgWebTrava);
        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
        if (tpl < 0) { sceKernelUsleep(500000); continue; }
        sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
        ((int (*)(int, size_t))(void*)sceHttpSetResponseHeaderMaxSize)(tpl, 32768);
        ((int (*)(int, int))(void*)sceHttpSetAutoRedirect)(tpl, 1);
        int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
        int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
        int32_t status = 0; long long feito = 0;
        if (req >= 0) {
            ((int (*)(int, unsigned int))(void*)sceHttpSetRecvTimeOut)(req, 30u * 1000000u);
            char faixa[80]; snprintf(faixa, sizeof(faixa), "bytes=%lld-%lld", ini, fim);
            sceHttpAddRequestHeader(req, "Range", faixa, 1);
            if (sceHttpSendRequest(req, NULL, 0) >= 0) sceHttpGetStatusCode(req, &status);
            if (status == 206) {
                int n;
                while (feito < total && !ponte.parar && (n = sceHttpReadData(req, buffer, (int)(total - feito < 1024 * 1024 ? total - feito : 1024 * 1024))) > 0) {
                    if ((ssize_t)sceKernelPwrite(ponte.fd, buffer, (size_t)n, (off_t)(ini + feito)) != n) break;
                    feito += n;
                }
            }
        }
        if (req >= 0) sceHttpDeleteRequest(req);
        if (conn >= 0) sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tpl);
        if (feito == total) { __sync_add_and_fetch(&ponte.baixados, total); return true; }
        ponteLog("bloco %lld falhou: status %lld, %lld de %lld bytes", b, status, feito, total);
        if ((status == 403 || status == 404 || status == 410 || status == 400) && pkgsServidor[ponte.vaga].urlOriginal) {
            char novo[4096];
            if (hnResolverLink(pkgsServidor[ponte.vaga].urlOriginal, novo, sizeof(novo))) {
                pthread_mutex_lock(&pkgWebTrava); free(pkgsServidor[ponte.vaga].url); pkgsServidor[ponte.vaga].url = strdup(novo); pthread_mutex_unlock(&pkgWebTrava);
            }
        }
        sceKernelUsleep(300000);
    }
    return false;
}
static void* ponteTrabalhador(void*) {
    char* buffer = (char*)malloc(1024 * 1024);
    while (buffer && !ponte.parar) {
        int b = -1;
        pthread_mutex_lock(&ponteTrava);
        // primeiro o bloco que o PS4 pediu e os seguintes; depois qualquer um que falte
        for (int i = ponte.prioridade; i < ponte.blocos && b < 0; i++) if (ponte.estado[i] == 0) b = i;
        for (int i = 0; i < ponte.blocos && b < 0; i++) if (ponte.estado[i] == 0) b = i;
        if (b >= 0) ponte.estado[b] = 1;
        pthread_mutex_unlock(&ponteTrava);
        if (b < 0) break;
        bool ok = ponteBaixarBloco(b, buffer);
        pthread_mutex_lock(&ponteTrava); ponte.estado[b] = ok ? 2 : 0; pthread_mutex_unlock(&ponteTrava);
        if (!ok) sceKernelUsleep(1000000);
        if (ok && (b % 8 == 0 || b == ponte.blocos - 1)) {
            uint64_t ms = ponteAgoraMs() - ponte.inicioMs;
            ponteLog("internet: %lld MB de %lld MB, %lld KB/s (bloco %lld)", ponte.baixados >> 20, ponte.tamanho >> 20, ms ? ponte.baixados * 1000 / (long long)ms / 1024 : 0, b);
        }
    }
    free(buffer);
    __sync_sub_and_fetch(&ponte.rodando, 1);
    return NULL;
}
static void ponteParar() {
    if (!ponte.ativa) return;
    ponte.parar = true;
    for (int i = 0; i < 300 && ponte.rodando > 0; i++) sceKernelUsleep(20000);
    if (ponte.fd >= 0) sceKernelClose(ponte.fd);
    remove(ponte.arquivo);
    free((void*)ponte.estado); ponte.estado = NULL;
    ponte.ativa = false;
}
static bool ponteIniciar(int vaga, const char* contentId, long long tamanho) {
    ponteParar();
    mkdir("/data/pkg", 0777); mkdir("/data/pkg/ponte", 0777);
    snprintf(ponte.arquivo, sizeof(ponte.arquivo), "/data/pkg/ponte/%s.part", contentId);
    ponte.fd = sceKernelOpen(ponte.arquivo, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (ponte.fd < 0) return false;
    ponte.tamanho = tamanho; ponte.vaga = vaga;
    ponte.blocos = (int)((tamanho + PONTE_BLOCO - 1) / PONTE_BLOCO);
    ponte.estado = (volatile uint8_t*)calloc(ponte.blocos, 1);
    if (!ponte.estado) { sceKernelClose(ponte.fd); ponte.fd = -1; return false; }
    ponte.prioridade = 0; ponte.baixados = 0; ponte.parar = false; ponte.inicioMs = ponteAgoraMs();
    ponte.ativa = true;
    FILE* lg = fopen("/data/HyperNeiva/pkg_servidor_log.txt", "w"); if (lg) fclose(lg);
    ponteLog("ponte: %lld MB em %lld blocos, %lld conexoes", tamanho >> 20, ponte.blocos, PONTE_TRABALHADORES, 0);
    for (int i = 0; i < PONTE_TRABALHADORES; i++) {
        pthread_t t;
        __sync_add_and_fetch(&ponte.rodando, 1);
        pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256 * 1024);
        int criou = pthread_create(&t, &at, ponteTrabalhador, NULL); pthread_attr_destroy(&at);
        if (criou == 0) pthread_detach(t); else __sync_sub_and_fetch(&ponte.rodando, 1);
    }
    return true;
}
// Entrega ao PS4 o trecho pedido, do disco, esperando cada bloco chegar.
static bool ponteEnviar(int fd, long long ini, long long fim) {
    char* buffer = (char*)malloc(1024 * 1024);
    if (!buffer) return false;
    uint64_t t0 = ponteAgoraMs();
    long long pos = ini;
    bool ok = true;
    while (pos <= fim && ok) {
        int b = (int)(pos / PONTE_BLOCO);
        ponte.prioridade = b;
        int espera = 0;
        while (ponte.ativa && !ponte.parar && ponte.estado[b] != 2 && espera < 6000) { sceKernelUsleep(20000); espera++; }   // até 2 min
        if (!ponte.ativa || ponte.estado[b] != 2) { ok = false; break; }
        long long fimBloco = ((long long)b + 1) * PONTE_BLOCO - 1; if (fimBloco > fim) fimBloco = fim;
        while (pos <= fimBloco && ok) {
            size_t n = (size_t)(fimBloco - pos + 1 < 1024 * 1024 ? fimBloco - pos + 1 : 1024 * 1024);
            ssize_t lidos = (ssize_t)sceKernelPread(ponte.fd, buffer, n, (off_t)pos);
            if (lidos <= 0 || !enviarTudoPkg(fd, buffer, (size_t)lidos)) { ok = false; break; }
            pos += lidos;
        }
    }
    free(buffer);
    uint64_t ms = ponteAgoraMs() - t0;
    if (fim - ini > 1024 * 1024) ponteLog("PS4 leu %lld KB em %lld ms (%lld KB/s) %s", (pos - ini) >> 10, (long long)ms, ms ? ((pos - ini) * 1000 / (long long)ms) >> 10 : 0, ok ? 0 : 1);
    return ok;
}

static void* handle_client(void* arg) {
    int client_fd = *(int*)arg; free(arg); int set = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
    int bufSock = 1024 * 1024; setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &bufSock, sizeof(bufSock));
    char buffer_req[4096]; memset(buffer_req, 0, sizeof(buffer_req));
    // o pedido pode chegar em partes: lê até o fim do cabeçalho
    int lido = 0;
    while (lido < (int)sizeof(buffer_req) - 1) {
        int k = recv(client_fd, buffer_req + lido, sizeof(buffer_req) - 1 - lido, 0);
        if (k <= 0) break;
        lido += k; buffer_req[lido] = 0;
        if (strstr(buffer_req, "\r\n\r\n")) break;
    }
    int vagaWeb = lido > 0 ? vagaDoPedido(buffer_req) : -1;
    if (vagaWeb >= 0 && pkgsServidor[vagaWeb].url) {
        bool head = strncmp(buffer_req, "HEAD", 4) == 0;
        long long fsize = pkgsServidor[vagaWeb].tamanho, ini = 0, fim = fsize - 1;
        const char* r = strstr(buffer_req, "Range: bytes="); if (!r) r = strstr(buffer_req, "range: bytes=");
        bool parcial = false;
        if (r) { long long a = -1, b = -1; int k = sscanf(r + 13, "%lld-%lld", &a, &b); if (k >= 1 && a >= 0) { ini = a; if (k == 2 && b >= a) fim = b; parcial = true; } }
        if (fim > fsize - 1) fim = fsize - 1;
        if (ini > fim) ini = fim;
        char cab[512];
        if (parcial) snprintf(cab, sizeof(cab), "HTTP/1.1 206 Partial Content\r\nContent-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n", ini, fim, fsize, fim - ini + 1);
        else snprintf(cab, sizeof(cab), "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n", fsize);
        int pedido = __sync_add_and_fetch(&pkgServidorPedidos, 1);
        pkgServidorLog(pedido, ini, fim, fsize, head);
        if (enviarTudoPkg(client_fd, cab, strlen(cab)) && !head) {
            if (ponte.ativa && ponte.vaga == vagaWeb) ponteEnviar(client_fd, ini, fim);
            else pkgWebEnviar(client_fd, vagaWeb, ini, fim);
        }
    }
    else if (strlen(caminhoPkgAtual) > 0 && lido > 0) {
        bool is_head_request = (strncmp(buffer_req, "HEAD", 4) == 0);
        FILE* f = fopen(caminhoDoPedido(buffer_req), "rb");
        if (f) {
            fseeko(f, 0, SEEK_END); long long fsize = (long long)ftello(f); fseeko(f, 0, SEEK_SET);
            long long ini = 0, fim = fsize - 1;
            const char* range_str = strstr(buffer_req, "Range: bytes="); if (!range_str) range_str = strstr(buffer_req, "range: bytes=");
            bool parcial = false;
            if (range_str) {
                long long a = -1, b = -1;
                int lidos = sscanf(range_str + 13, "%lld-%lld", &a, &b);
                if (lidos >= 1 && a >= 0) { ini = a; if (lidos == 2 && b >= a) fim = b; parcial = true; }
            }
            if (fim > fsize - 1) fim = fsize - 1;
            if (ini > fim) ini = fim;
            long long total = fim - ini + 1;
            char header[512];
            if (parcial) snprintf(header, sizeof(header), "HTTP/1.1 206 Partial Content\r\nContent-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n", ini, fim, fsize, total);
            else snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n", fsize);
            int pedido = __sync_add_and_fetch(&pkgServidorPedidos, 1);
            pkgServidorLog(pedido, ini, fim, fsize, is_head_request);
            bool ok = enviarTudoPkg(client_fd, header, strlen(header));
            if (ok && !is_head_request) {
                const size_t BLOCO = 1024 * 1024;
                char* file_buffer = (char*)malloc(BLOCO);
                fseeko(f, ini, SEEK_SET);
                long long falta = parcial ? total : fsize;
                while (file_buffer && falta > 0) {
                    size_t pedir = falta < (long long)BLOCO ? (size_t)falta : BLOCO;
                    size_t n = fread(file_buffer, 1, pedir, f);
                    if (n == 0 || !enviarTudoPkg(client_fd, file_buffer, n)) break;
                    falta -= (long long)n;
                }
                free(file_buffer);
            }
            fclose(f);
        }
        else { char* not_found = (char*)"HTTP/1.1 404 Not Found\r\n\r\n"; send(client_fd, not_found, strlen(not_found), 0); }
    } close(client_fd); return NULL;
}

static void* threadServidorHTTP(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); addr.sin_port = htons(8080);
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return NULL; listen(server_fd, 10);
    while (1) { int client_fd = accept(server_fd, NULL, NULL); if (client_fd < 0) continue; int* pclient = (int*)malloc(sizeof(int)); *pclient = client_fd; pthread_t tid; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256 * 1024); if (pthread_create(&tid, &at, handle_client, pclient) == 0) pthread_detach(tid); else { close(client_fd); free(pclient); } pthread_attr_destroy(&at); }
    return NULL;
}

static void ligarServidorSeNecessario() {
    if (!servidorRodando) { pthread_t tid; pthread_create(&tid, NULL, threadServidorHTTP, NULL); servidorRodando = true; }
}

static void instalarPkgPeloServidor(const char* caminhoAbsoluto);
static void pkgLog(const char* formato, int a, const char* b) {
    FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (registro) { fprintf(registro, formato, a, b); fputc('\n', registro); fclose(registro); }
}
// Instala direto do arquivo (sceAppInstUtilAppInstallPkg): sem passar pelo download do
// sistema, que fica lento (0,2 MB/s) enquanto um app está aberto. Se o sistema recusar,
// cai no jeito antigo (download do sistema servido pelo próprio app).
static char pkgDiretoCaminho[512];
static volatile bool pkgDiretoRodando = false;
static void* threadPkgDireto(void*) {
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL);
    static bool iniciado = false;
    if (!iniciado) { int r0 = sceAppInstUtilInitialize(); pkgLog("INSTALAR DIRETO: inicializar 0x%08X %s", r0, ""); iniciado = true; }
    // O instalador só aceita o caminho completo do sistema: /data/... dava 0x80020012.
    char completo[540];
    if (!strncmp(pkgDiretoCaminho, "/data/", 6)) snprintf(completo, sizeof(completo), "/user%s", pkgDiretoCaminho);
    else snprintf(completo, sizeof(completo), "%s", pkgDiretoCaminho);
    int r = sceAppInstUtilAppInstallPkg(completo, NULL);
    pkgLog("INSTALAR DIRETO: 0x%08X %s", r, completo);
    if (r < 0 && strcmp(completo, pkgDiretoCaminho) != 0) {
        r = sceAppInstUtilAppInstallPkg(pkgDiretoCaminho, NULL);
        pkgLog("INSTALAR DIRETO (sem /user): 0x%08X %s", r, pkgDiretoCaminho);
    }
    if (r >= 0) {
        snprintf(msgStatus, sizeof(msgStatus), "INSTALANDO DIRETO NO PS4... VEJA NA TELA INICIAL");
        msgTimer = 500;
    }
    else {
        snprintf(msgStatus, sizeof(msgStatus), "INSTALACAO DIRETA RECUSADA (0x%08X): USANDO O DOWNLOAD DO SISTEMA", (unsigned)r);
        msgTimer = 400;
        instalarPkgPeloServidor(pkgDiretoCaminho);
    }
    pkgDiretoRodando = false;
    return NULL;
}
void instalarPkgLocal(const char* caminhoAbsoluto) {
    extern void conquistaGanhar(const char* chave);
    conquistaGanhar("pkg");
    long long faltamAntes = hnBytesFaltandoPkg(caminhoAbsoluto);
    if (faltamAntes > 0) {
        snprintf(msgStatus, sizeof(msgStatus), "PKG INCOMPLETO: FALTAM %.2f GB. BAIXE E EXTRAIA TODAS AS PARTES", (double)faltamAntes / 1073741824.0);
        msgTimer = 600;
        return;
    }
    if (pkgDiretoRodando) { snprintf(msgStatus, sizeof(msgStatus), "ESPERE A INSTALACAO ATUAL COMECAR"); msgTimer = 200; return; }
    snprintf(pkgDiretoCaminho, sizeof(pkgDiretoCaminho), "%s", caminhoAbsoluto);
    snprintf(msgStatus, sizeof(msgStatus), "INSTALANDO DIRETO DO ARQUIVO..."); msgTimer = 300;
    pkgDiretoRodando = true;
    pthread_t t;
    if (pthread_create(&t, NULL, threadPkgDireto, NULL) == 0) pthread_detach(t);
    else { pkgDiretoRodando = false; instalarPkgPeloServidor(caminhoAbsoluto); }
}

// "Pelo PS4" com link da internet que o sistema recusou: registra o download apontando para o app.
bool hnPkgWebRegistrar(const char* contentId, const char* urlOriginal, const char* urlFinal, const char* titulo, unsigned long long tamanho) {
    if (!contentId || strlen(contentId) != 36 || !tamanho) return false;
    if (!caminhoPkgAtual[0]) snprintf(caminhoPkgAtual, sizeof(caminhoPkgAtual), "web");
    ligarServidorSeNecessario();
    int vaga = -1;
    for (int i = 0; i < 32; i++) if (strcmp(pkgsServidor[i].id, contentId) == 0) vaga = i;
    if (vaga < 0) { vaga = proximoPkgServidor; proximoPkgServidor = (proximoPkgServidor + 1) % 32; }
    pthread_mutex_lock(&pkgWebTrava);
    snprintf(pkgsServidor[vaga].id, sizeof(pkgsServidor[vaga].id), "%s", contentId);
    pkgsServidor[vaga].caminho[0] = 0;
    snprintf(pkgsServidor[vaga].titulo, sizeof(pkgsServidor[vaga].titulo), "%s", titulo);
    free(pkgsServidor[vaga].url); pkgsServidor[vaga].url = strdup(urlFinal);
    free(pkgsServidor[vaga].urlOriginal); pkgsServidor[vaga].urlOriginal = strdup(urlOriginal ? urlOriginal : urlFinal);
    pkgsServidor[vaga].tamanho = (long long)tamanho;
    pthread_mutex_unlock(&pkgWebTrava);
    int32_t userId = 0; sceUserServiceGetInitialUser(&userId);
    static char urlPkg[160]; snprintf(urlPkg, sizeof(urlPkg), "http://127.0.0.1:8080/%s.pkg", contentId);
    static char id[40]; snprintf(id, sizeof(id), "%s", contentId);
    OrbisBgftTaskId tarefaAntiga = -1; sceBgftServiceIntDownloadFindActiveTask(id, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga);
    if (tarefaAntiga != -1) sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga);
    OrbisBgftDownloadParam params; memset(&params, 0, sizeof(params));
    params.userId = userId; params.entitlementType = 5; params.id = id; params.contentUrl = urlPkg; params.contentName = pkgsServidor[vaga].titulo; params.playgoScenarioId = "0"; params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM; params.packageType = "PS4GD"; params.packageSubType = "";
    uint64_t t64 = tamanho; memcpy(&params.packageSize, &t64, sizeof(t64));
    OrbisBgftTaskId taskId = -1;
    if (!ponteIniciar(vaga, contentId, (long long)tamanho)) pkgLog("PONTE RAPIDA: nao abriu o temporario 0x%08X %s", 0, contentId);
    // Registro normal (o da loja) primeiro: com o de teste, o PS4 ficava em "Calculando".
    // A tarefa da tentativa anterior (guardada em arquivo) sai antes: com ela no sistema o
    // registro normal dava 0x80990015 e o de teste travava o app.
    {
        FILE* fa = fopen("/data/HyperNeiva/configuracao/ponte_tarefa.txt", "r");
        char idAntigo[48] = ""; int tarefaVelha = -1;
        if (fa) { if (fscanf(fa, "%47s %d", idAntigo, &tarefaVelha) != 2) tarefaVelha = -1; fclose(fa); }
        if (tarefaVelha >= 0 && !strcmp(idAntigo, contentId)) { int u = sceBgftServiceIntDownloadUnregisterTask(tarefaVelha); pkgLog("PONTE: removeu a tarefa antiga 0x%08X %s", u, contentId); }
    }
    // Registro de teste: o "normal" (da loja) derrubava o app uns 5 s depois de começar.
    int res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);
    pkgLog("PELO PS4 PELA PONTE: registro 0x%08X %s", res, contentId);
    if ((unsigned)res == 0x80990015u) {
        // já existe: acha, remove e registra de novo
        OrbisBgftTaskId existente = -1;
        sceBgftServiceIntDownloadFindActiveTask(id, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &existente);
        if (existente != -1) sceBgftServiceIntDownloadUnregisterTask(existente);
        taskId = -1; res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);
        pkgLog("PELO PS4 PELA PONTE: registro (de novo) 0x%08X %s", res, contentId);
        if (res != 0) {
            snprintf(msgStatus, sizeof(msgStatus), "JA HA UM DOWNLOAD DESTE PKG NO PS4: APAGUE EM NOTIFICACOES > DOWNLOADS");
            msgTimer = 600;
        }
    }
    else if (res != 0 || taskId < 0) {
        if (res != 0) { params.option = ORBIS_BGFT_TASK_OPT_NONE; params.packageType = NULL; params.packageSubType = NULL; taskId = -1; res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId); }
    }
    pkgLog("PELO PS4 PELA PONTE: registro 0x%08X %s", res, contentId);
    if (res == 0 && taskId >= 0) {
        FILE* fa = fopen("/data/HyperNeiva/configuracao/ponte_tarefa.txt", "w");
        if (fa) { fprintf(fa, "%s %d\n", contentId, (int)taskId); fclose(fa); }
    }
    if (res != 0 || taskId < 0) ponteParar();
    if (res != 0 || taskId < 0) return false;
    pkgWebTarefa[vaga] = (int)taskId;
    extern void dlRegistrarTarefaPs4(int id, const char* nome); dlRegistrarTarefaPs4((int)taskId, pkgsServidor[vaga].titulo);
    sceBgftServiceDownloadStartTask(taskId);
    return true;
}

static void instalarPkgPeloServidor(const char* caminhoAbsoluto) {
    // PKG cortado sempre falha no console: melhor dizer o motivo antes de tentar.
    long long faltam = hnBytesFaltandoPkg(caminhoAbsoluto);
    if (faltam > 0) {
        snprintf(msgStatus, sizeof(msgStatus), "PKG INCOMPLETO: FALTAM %.2f GB. BAIXE E EXTRAIA TODAS AS PARTES", (double)faltam / 1073741824.0);
        msgTimer = 600;
        return;
    }
    sprintf(msgStatus, "ANALISANDO FILA DE DOWNLOADS..."); atualizarBarra(0.2f);
    strcpy(caminhoPkgAtual, caminhoAbsoluto); ligarServidorSeNecessario();
    char contentId[40]; memset(contentId, 0, sizeof(contentId)); uint64_t fileSize = 0; FILE* f = fopen(caminhoAbsoluto, "rb");
    if (f) { fseek(f, 0x40, SEEK_SET); fread(contentId, 1, 36, f); fseek(f, 0, SEEK_END); fileSize = (uint64_t)ftello(f); fclose(f); }
    else { strcpy(contentId, "UP0000-000000000_00-0000000000000000"); }
    // Reinstalar o mesmo pacote reaproveita a vaga; o título continua vivo para a notificação.
    int vaga = -1;
    for (int i = 0; i < 32; i++) if (strcmp(pkgsServidor[i].id, contentId) == 0) vaga = i;
    if (vaga < 0) { vaga = proximoPkgServidor; proximoPkgServidor = (proximoPkgServidor + 1) % 32; }
    snprintf(pkgsServidor[vaga].id, sizeof(pkgsServidor[vaga].id), "%s", contentId);
    snprintf(pkgsServidor[vaga].caminho, sizeof(pkgsServidor[vaga].caminho), "%s", caminhoAbsoluto);
    pthread_mutex_lock(&pkgWebTrava); free(pkgsServidor[vaga].url); pkgsServidor[vaga].url = NULL; free(pkgsServidor[vaga].urlOriginal); pkgsServidor[vaga].urlOriginal = NULL; pthread_mutex_unlock(&pkgWebTrava);
    hnTituloPkg(caminhoAbsoluto, pkgsServidor[vaga].titulo, sizeof(pkgsServidor[vaga].titulo));
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL); sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
    static void* bgftHeap = NULL;
    if (!bgftHeap) { sceAppInstUtilInitialize(); OrbisBgftInitParams bgftInit; memset(&bgftInit, 0, sizeof(OrbisBgftInitParams)); bgftInit.heapSize = 2 * 1024 * 1024; bgftHeap = memalign(4096, bgftInit.heapSize); bgftInit.heap = bgftHeap; sceBgftServiceIntInit(&bgftInit); }
    OrbisBgftTaskId tarefaAntiga = -1; sceBgftServiceIntDownloadFindActiveTask(contentId, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga); if (tarefaAntiga != -1) { sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga); }
    int32_t userId = 0; sceUserServiceGetInitialUser(&userId);
    char urlPkg[1024]; sprintf(urlPkg, "http://127.0.0.1:8080/%s.pkg", contentId);
    OrbisBgftDownloadParam params; memset(&params, 0, sizeof(OrbisBgftDownloadParam)); params.userId = userId; params.entitlementType = 5; params.id = contentId; params.contentUrl = urlPkg; params.contentName = pkgsServidor[vaga].titulo; params.playgoScenarioId = "0"; params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM; params.packageType = "PS4GD"; params.packageSubType = ""; memcpy(&params.packageSize, &fileSize, sizeof(fileSize));
    OrbisBgftTaskId taskId = -1; int res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId); if (res != 0) { params.option = ORBIS_BGFT_TASK_OPT_NONE; params.packageType = NULL; params.packageSubType = NULL; taskId = -1; res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId); }
    if (res == 0 && taskId >= 0) { extern volatile int hnTarefaPkg; hnTarefaPkg = (int)taskId; extern void dlRegistrarTarefaPs4(int id, const char* nome); dlRegistrarTarefaPs4((int)taskId, pkgsServidor[vaga].titulo); sceBgftServiceDownloadStartTask(taskId); sprintf(msgStatus, "DOWNLOAD INICIADO! Verifique as Notificacoes."); atualizarBarra(1.0f); }
    else { sprintf(msgStatus, "ERRO: 0x%08X", res); atualizarBarra(0.0f); }
    FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (registro) { fprintf(registro, "INSTALAR PELO DOWNLOAD res=0x%08X tarefa=%d tamanho=%llu %s\n", (unsigned int)res, (int)taskId, (unsigned long long)fileSize, caminhoAbsoluto); fclose(registro); }
    msgTimer = 400;
}

void acaoL2_Explorar() {
    if (visualizandoMidiaImagem) return;
    painelDuplo = !painelDuplo;
    if (painelDuplo) {
        painelAtivo = 0;
        menuAtualEsq = MENU_EXPLORAR_HOME;
        selEsq = 0;
        offEsq = 0;
        extern int totalItensEsq;
        totalItensEsq = 0; // Mï¿½GICA 1: Forï¿½a redesenhar o menu ao abrir!
    }
    else {
        painelAtivo = 1;
    }
}

void alternarPainelAtivo() { if (visualizandoMidiaImagem) return; if (painelDuplo && !showOpcoes) painelAtivo = (painelAtivo == 0) ? 1 : 0; }

void acaoCross_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;

    if (mAtual == MENU_LISTA_CORES && showOpcoes) { acaoArquivo(selOpcao); return; }
    if ((mAtual == MENU_EXPLORAR || visualizandoMidiaImagem) && showOpcoes) { acaoArquivo(selOpcao); return; }
    if (visualizandoMidiaImagem) { fullscreenMidia = !fullscreenMidia; if (!fullscreenMidia) { zoomMidia = 1.0f; panMidiaX = 0; panMidiaY = 0; } return; }

    int sAtual = ehEsq ? selEsq : sel; char (*nItems)[256] = ehEsq ? nomesEsq : nomes; char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

    if (mAtual == MENU_EXPLORAR_HOME) {
        char tempBase[256];
        if (sAtual == 0) strcpy(tempBase, "/data/HyperNeiva"); 
        else if (sAtual == 1) strcpy(tempBase, "/"); 
        else if (sAtual == 2) strcpy(tempBase, "/mnt/usb0"); 
        else if (sAtual == 3) strcpy(tempBase, "/mnt/usb1");
        
        if (ehEsq) listarDiretorioEsq(tempBase); else { strcpy(baseRaiz, tempBase); listarDiretorio(baseRaiz); }
    }
    else if (mAtual == MENU_EXPLORAR) {
        if (nItems[sAtual][0] == '[') {
            char pL[128]; strncpy(pL, &nItems[sAtual][1], strlen(nItems[sAtual]) - 2); pL[strlen(nItems[sAtual]) - 2] = '\0';
            char nP[256]; sprintf(nP, "%s/%s", pExplorar, pL);
            if (ehEsq) listarDiretorioEsq(nP); else listarDiretorio(nP);
        }
        else {
            char caminhoArquivo[512]; sprintf(caminhoArquivo, "%s/%s", pExplorar, nItems[sAtual]);
            char nomeBlindado[256]; strcpy(nomeBlindado, nItems[sAtual]);
            for (int i = 0; nomeBlindado[i]; i++) { nomeBlindado[i] = tolower(nomeBlindado[i]); }

            if (strstr(nomeBlindado, ".pkg")) { instalarPkgLocal(caminhoArquivo); }
            else if (strstr(nomeBlindado, ".torrent")) { extern void adicionarTorrent(const char* origem); adicionarTorrent(caminhoArquivo); }
            else if (strstr(nomeBlindado, ".self") || strstr(nomeBlindado, ".elf") || strstr(nomeBlindado, ".prx")) {
                preencherOpcoesContexto(nItems[sAtual]);
                showOpcoes = true; selOpcao = totalOpcoes - 2; // Foca no "DEFINIR NUCLEO" ou "DETALHES"
            }
            else if (strstr(nomeBlindado, ".bin") || strstr(nomeBlindado, ".md") || strstr(nomeBlindado, ".smd") || strstr(nomeBlindado, ".gen") || strstr(nomeBlindado, ".sfc") || strstr(nomeBlindado, ".smc")) {
                snprintf(msgStatus, sizeof(msgStatus), "ROM Detectada!"); msgTimer = 120;
                iniciarEmulador(caminhoArquivo);
            }
            else if (strstr(nomeBlindado, ".zip") || strstr(nomeBlindado, ".rar") || strstr(nomeBlindado, ".7z") || hnEhParteCompactada(nomeBlindado)) {
                for (int i = 0; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
                listaOpcoes[0] = "extrair aqui"; 
                mapOpcoes[0] = 7;
                totalOpcoes = 1;
                showOpcoes = true; selOpcao = 0; 
            }
            else if (strstr(nomeBlindado, ".png") || strstr(nomeBlindado, ".jpg") || strstr(nomeBlindado, ".jpeg") || strstr(nomeBlindado, ".bmp") || strstr(nomeBlindado, ".xavatar")) {

                if (selecionandoMidiaElemento) return;

                extern void abrirOpcoesImagem(const char*, bool); abrirOpcoesImagem(caminhoArquivo, true);
            }
            else if (strstr(nomeBlindado, ".obj") || strstr(nomeBlindado, ".stl")) {
                extern bool modelo3dAbrir(const char* caminho);
                modelo3dAbrir(caminhoArquivo);
            }
            else if (strstr(nomeBlindado, ".mp3") || strstr(nomeBlindado, ".wav") || strstr(nomeBlindado, ".m4a") || strstr(nomeBlindado, ".ogg") ||
                     strstr(nomeBlindado, ".flac") || strstr(nomeBlindado, ".aac") || strstr(nomeBlindado, ".opus") || strstr(nomeBlindado, ".oga")) {
                // Menu: tocar (mp3/wav, os que o player toca) ou transcrever com a IA (gera .txt ao lado).
                extern void abrirOpcoesAudio(const char* caminho, bool tocavel, bool tocando);
                bool tocavel = strstr(nomeBlindado, ".mp3") || strstr(nomeBlindado, ".wav");
                abrirOpcoesAudio(caminhoArquivo, tocavel, strcmp(caminhoMusicaTocando, caminhoArquivo) == 0);
            }
            else if (strstr(nomeBlindado, ".pdf") || strstr(nomeBlindado, ".cbz")) {
                extern void abrirLeitor(const char* caminho);
                abrirLeitor(caminhoArquivo);
            }
            // ==========================================================
            // LOGICA PARA REPRODUZIR Vï¿½DEO MP4 ADICIONADA AQUI
            // ==========================================================
            else if (ehArquivoVideo(nomeBlindado)) {
                extern char ultimo_video_tocado[512];
                if (!(videoRodando && strcmp(caminhoArquivo, ultimo_video_tocado) == 0)) {
                    iniciarVideoMP4(caminhoArquivo);
                }
            }
            else if (strstr(nomeBlindado, ".txt") || strstr(nomeBlindado, ".xml") || strstr(nomeBlindado, ".json") || strstr(nomeBlindado, ".ini") || strstr(nomeBlindado, ".cfg") || strstr(nomeBlindado, ".log") || strstr(nomeBlindado, ".cpp") || strstr(nomeBlindado, ".h")) {
                extern void editarArquivoExistente(const char* pPasta, const char* nArquivo);
                editarArquivoExistente(pExplorar, nItems[sAtual]);
                if (ehEsq) menuAtualEsq = MENU_NOTEPAD; else menuAtual = MENU_NOTEPAD;
            }
        }
    }
    else if (mAtual == MENU_LISTA_CORES) {
        char coreName[128]; strcpy(coreName, nomes[sel]);
        char fullPath[512];
        bool achou = false;
        const char* pastas[] = {"/data/retroarch/cores", "/mnt/usb0/cores", "/mnt/usb1/cores", "/data/self/retroarch/cores"};
        for (int i = 0; i < 4; i++) {
            sprintf(fullPath, "%s/%s", pastas[i], coreName);
            FILE* f = fopen(fullPath, "rb");
            if (f) { fclose(f); achou = true; break; }
        }
        if (achou) {
            extern char caminhoCoreSelecionado[512];
            extern char romParaCarregar[512];
            strcpy(caminhoCoreSelecionado, fullPath);
            sprintf(msgStatus, "NUCLEO ATIVO: %s", coreName);
            msgTimer = 240; 
            
            // SE TIVER JOGO PENDENTE, COMEÇA AGORA!
            if (strlen(romParaCarregar) > 0) {
                extern void iniciarEmulador(const char* rom);
                iniciarEmulador(romParaCarregar);
                romParaCarregar[0] = '\0'; // Limpa para a proxima
            } else {
                extern void preencherMenuEmulador();
                preencherMenuEmulador();
            }
        } else {
            sprintf(msgStatus, "ERRO: %s NAO ENCONTRADO NO PS4!", coreName);
            msgTimer = 400;
        }
    }
}

void acaoCircle_Explorar() {
    // ==========================================================
    // PARAR O Vï¿½DEO SE ESTIVER RODANDO AO APERTAR BOLINHA
    // ==========================================================
    if (videoRodando && !video_minimizado) {
        pararVideo();
        return;
    }

    if (visualizandoMidiaImagem) {
        visualizandoMidiaImagem = false;
        fullscreenMidia = false; zoomMidia = 1.0f; panMidiaX = 0; panMidiaY = 0;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        return;
    }

    if (esperandoNomePasta || esperandoRenomear) return;
    if (showOpcoes) { showOpcoes = false; return; }

    bool ehEsq = (painelDuplo && painelAtivo == 0);
    extern void voltarNavegacao();

    if (ehEsq && painelDuplo) {
        // No painel esquerdo, o comportamento de 'voltar' é local e manual (não usa a pilha nav global)
        MenuLevel mAtualEsq = menuAtualEsq;
        char* pExplorarEsq = pathExplorarEsq;

        if (mAtualEsq == MENU_EXPLORAR_HOME) {
            painelDuplo = false;
            painelAtivo = 1;
        }
        else if (mAtualEsq == MENU_EXPLORAR) {
            if (strcmp(pExplorarEsq, "/data/HyperNeiva") == 0 || strcmp(pExplorarEsq, "/") == 0 || strcmp(pExplorarEsq, "/mnt/usb0") == 0 || strcmp(pExplorarEsq, "/mnt/usb1") == 0) {
                menuAtualEsq = MENU_EXPLORAR_HOME;
                extern int totalItensEsq;
                totalItensEsq = 0; 
                selEsq = 0; offEsq = 0;
            }
            else {
                char temp[256]; strcpy(temp, pExplorarEsq); char* last = strrchr(temp, '/');
                if (last) {
                    if (last == temp) strcpy(temp, "/"); else *last = '\0';
                    listarDiretorioEsq(temp);
                }
            }
        }
    }
    else {
        // Painel Principal ou Navegação Global via Pilha
        voltarNavegacao();
    }
}


void acaoTriangle_Explorar() {
    if (visualizandoMidiaImagem) return;
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
    if (mAtual == MENU_EXPLORAR) {
        if (!showOpcoes) {
            int sAtual = ehEsq ? selEsq : sel; char (*nItems)[256] = ehEsq ? nomesEsq : nomes;
            preencherOpcoesContexto(nItems[sAtual]);
        } showOpcoes = !showOpcoes; selOpcao = 0;
    }
}

void acaoR1_Explorar() {
    if (visualizandoMidiaImagem) return;
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;
    int sAtual = ehEsq ? selEsq : sel;
    if (mAtual == MENU_EXPLORAR) {
        if (cd <= 0) {
            bool* mItems = ehEsq ? marcadosEsq : marcados;
            mItems[sAtual] = !mItems[sAtual];
            cd = 12;
        }
    }
}