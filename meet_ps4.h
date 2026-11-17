// =========================================================================
// HYPER NEIVA MEET: o PS4 é o servidor da chamada (HTTPS + WebSocket na porta 8443).
// Salas abertas ou privadas (com código), chat, áudio e vídeo. Os navegadores tentam WebRTC
// direto entre si; quando não dá (e sempre com o PS4), a mídia passa pelo PS4.
// Incluído pelo network.cpp (PS4) e pelo teste do PC (MEET_PC).
// =========================================================================
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "meet_pagina.h"

#ifdef _WIN32
#ifndef NOCRYPT
#define NOCRYPT
#endif
#include <winsock2.h>
#undef X509_NAME
#undef X509_EXTENSIONS
#include <ws2tcpip.h>
typedef SOCKET MeetSock;
#define MEET_INVALIDO INVALID_SOCKET
static void meetFechar(MeetSock s) { closesocket(s); }
static void meetDormirMs(int ms) { Sleep(ms); }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
typedef int MeetSock;
#define MEET_INVALIDO (-1)
static void meetFechar(MeetSock s) { close(s); }
static void meetDormirMs(int ms) { usleep(ms * 1000); }
#endif

// ---- OpenSSL 1.1.1 (PS4) / 3.x (PC): só as funções usadas, com tipos opacos ----
extern "C" {
typedef struct ssl_st SSL; typedef struct ssl_ctx_st SSL_CTX; typedef struct ssl_method_st SSL_METHOD;
typedef struct x509_st X509; typedef struct evp_pkey_st EVP_PKEY; typedef struct evp_md_st EVP_MD;
typedef struct asn1_string_st ASN1_INTEGER; typedef struct asn1_string_st ASN1_TIME; typedef struct X509_name_st X509_NAME;
typedef struct ec_key_st EC_KEY;
const SSL_METHOD* TLS_server_method(void);
SSL_CTX* SSL_CTX_new(const SSL_METHOD*);
int SSL_CTX_use_certificate(SSL_CTX*, X509*);
int SSL_CTX_use_PrivateKey(SSL_CTX*, EVP_PKEY*);
SSL* SSL_new(SSL_CTX*);
int SSL_set_fd(SSL*, int);
int SSL_accept(SSL*);
int SSL_read(SSL*, void*, int);
int SSL_write(SSL*, const void*, int);
int SSL_pending(const SSL*);
void SSL_free(SSL*);
int SSL_get_error(const SSL*, int);
EVP_PKEY* EVP_PKEY_new(void);
int EVP_PKEY_assign(EVP_PKEY*, int, void*);
EC_KEY* EC_KEY_new_by_curve_name(int);
int EC_KEY_generate_key(EC_KEY*);
void EC_KEY_set_asn1_flag(EC_KEY*, int);
X509* X509_new(void);
int X509_set_version(X509*, long);
ASN1_INTEGER* X509_get_serialNumber(X509*);
int ASN1_INTEGER_set(ASN1_INTEGER*, long);
ASN1_TIME* X509_getm_notBefore(const X509*);
ASN1_TIME* X509_getm_notAfter(const X509*);
ASN1_TIME* X509_gmtime_adj(ASN1_TIME*, long);
int X509_set_pubkey(X509*, EVP_PKEY*);
X509_NAME* X509_get_subject_name(const X509*);
int X509_NAME_add_entry_by_txt(X509_NAME*, const char*, int, const unsigned char*, int, int, int);
int X509_set_issuer_name(X509*, X509_NAME*);
int X509_sign(X509*, EVP_PKEY*, const EVP_MD*);
const EVP_MD* EVP_sha256(void);
int i2d_X509(X509*, unsigned char**);
X509* d2i_X509(X509**, const unsigned char**, long);
int i2d_PrivateKey(EVP_PKEY*, unsigned char**);
EVP_PKEY* d2i_AutoPrivateKey(EVP_PKEY**, const unsigned char**, long);
unsigned char* SHA1(const unsigned char*, size_t, unsigned char*);
void RAND_seed(const void*, int);
int RAND_bytes(unsigned char*, int);
void CRYPTO_free(void*, const char*, int);
}

// Ganchos da plataforma (o PS4 participa da sala; no PC são vazios).
static void meetPs4Midia(int sala, int de, int tipo, const uint8_t* dados, int n);
static void meetPs4Texto(int sala, const char* quem, const char* texto, bool sistema);
static void meetLog(const char* formato, ...);

#define MEET_PORTA 8443
#ifndef MEET_ENDERECO
#define MEET_ENDERECO INADDR_ANY   // no teste do PC: INADDR_LOOPBACK
#endif
#define MEET_MAX_SALAS 8
#define MEET_MAX_PESSOAS 8
#define MEET_FILA_MAX (1536 * 1024)
#define MEET_MAX_ESPERA 6

struct MeetMsg { uint8_t* dados; int n; bool video; MeetMsg* prox; };
struct MeetCliente {
    MeetSock fd; SSL* ssl; int id, sala; char nome[24];
    pthread_mutex_t trava; MeetMsg* cabeca; MeetMsg* cauda; int bytes;
    uint32_t p2p;            // ids com quem já fala direto (WebRTC): a mídia não passa pelo PS4
    volatile bool vivo;
    bool dono;               // entrou com o código de dono: entra direto e aprova os pedidos
    bool micOn, camOn;       // estado que o navegador anuncia (ícones na grade)
    volatile int decisao;    // pedido de entrada: 0 esperando, 1 aceito, 2 recusado
};
// Salas são permanentes: ficam em configuracao/meet_salas.txt e o link (com o código) vale sempre.
struct MeetSala {
    bool usada, privada, aprovacao; char nome[32], chave[12], dono[12];
    bool ps4; char ps4Nome[24]; bool ps4Mic;
    MeetCliente* pessoas[MEET_MAX_PESSOAS]; MeetCliente* espera[MEET_MAX_ESPERA]; int proximoId;
};
static MeetSala meetSalas[MEET_MAX_SALAS];
static pthread_mutex_t meetTrava = PTHREAD_MUTEX_INITIALIZER;
static SSL_CTX* meetCtx = NULL;
static volatile bool meetLigado = false;
static char meetErro[160] = "";
static char meetPasta[300] = "";

// ---------------------------------------------------------------- certificado próprio
static bool meetLerArquivo(const char* c, unsigned char** d, long* n) {
    FILE* f = fopen(c, "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    *d = *n > 0 ? (unsigned char*)malloc(*n) : NULL;
    bool ok = *d && fread(*d, 1, *n, f) == (size_t)*n; fclose(f);
    if (!ok) { free(*d); *d = NULL; }
    return ok;
}
static bool meetGravarArquivo(const char* c, const unsigned char* d, int n) {
    FILE* f = fopen(c, "wb"); if (!f) return false;
    bool ok = fwrite(d, 1, n, f) == (size_t)n; if (fclose(f) != 0) ok = false; return ok;
}
// Gera (uma vez) um certificado EC P-256 autoassinado e guarda em pastaConfig.
static bool meetCertificado(const char* pastaConfig, X509** cert, EVP_PKEY** chave) {
    char cc[300], ck[300];
    snprintf(cc, sizeof(cc), "%s/meet_cert.der", pastaConfig); snprintf(ck, sizeof(ck), "%s/meet_chave.der", pastaConfig);
    unsigned char* d; long n;
    if (meetLerArquivo(cc, &d, &n)) { const unsigned char* p = d; *cert = d2i_X509(NULL, &p, n); free(d); }
    if (*cert && meetLerArquivo(ck, &d, &n)) { const unsigned char* p = d; *chave = d2i_AutoPrivateKey(NULL, &p, n); free(d); }
    if (*cert && *chave) return true;
    // entropia: relógio e endereços (o PS4 não tem /dev/urandom para o OpenSSL)
    unsigned char sem[64]; uint64_t t = 0;
    for (int i = 0; i < 64; i++) { volatile int x = i * 7919; t = t * 6364136223846793005ULL + (uint64_t)(uintptr_t)&x + (uint64_t)clock() + (uint64_t)time(NULL); sem[i] = (unsigned char)(t >> 33); }
    RAND_seed(sem, sizeof(sem));
    EC_KEY* ec = EC_KEY_new_by_curve_name(415 /* NID_X9_62_prime256v1 */);
    if (!ec || !EC_KEY_generate_key(ec)) { snprintf(meetErro, sizeof(meetErro), "nao gerou a chave"); return false; }
    EC_KEY_set_asn1_flag(ec, 1 /* OPENSSL_EC_NAMED_CURVE */);
    *chave = EVP_PKEY_new(); EVP_PKEY_assign(*chave, 408 /* EVP_PKEY_EC */, ec);
    *cert = X509_new();
    X509_set_version(*cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(*cert), (long)time(NULL));
    X509_gmtime_adj(X509_getm_notBefore(*cert), -86400);
    X509_gmtime_adj(X509_getm_notAfter(*cert), 10L * 365 * 86400);
    X509_set_pubkey(*cert, *chave);
    X509_NAME* nome = X509_get_subject_name(*cert);
    X509_NAME_add_entry_by_txt(nome, "CN", 0x1001 /* MBSTRING_ASC */, (const unsigned char*)"Hyper Neiva PS4", -1, -1, 0);
    X509_NAME_add_entry_by_txt(nome, "O", 0x1001, (const unsigned char*)"Hyper Neiva", -1, -1, 0);
    X509_set_issuer_name(*cert, nome);
    if (!X509_sign(*cert, *chave, EVP_sha256())) { snprintf(meetErro, sizeof(meetErro), "nao assinou o certificado"); return false; }
    unsigned char* der = NULL; int nd = i2d_X509(*cert, &der);
    if (nd > 0) { meetGravarArquivo(cc, der, nd); CRYPTO_free(der, "", 0); }
    der = NULL; nd = i2d_PrivateKey(*chave, &der);
    if (nd > 0) { meetGravarArquivo(ck, der, nd); CRYPTO_free(der, "", 0); }
    meetLog("MEET: certificado novo gerado");
    return true;
}

// ---------------------------------------------------------------- utilidades
static void meetJsonEsc(const char* in, char* out, int tam) {
    int k = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p && k < tam - 7; p++) {
        if (*p == '"' || *p == '\\') { out[k++] = '\\'; out[k++] = (char)*p; }
        else if (*p < 0x20) k += snprintf(out + k, tam - k, "\\u%04x", *p);
        else out[k++] = (char)*p;
    }
    out[k] = 0;
}
static void meetUrlDec(const char* in, char* out, int tam) {
    int k = 0;
    for (const char* p = in; *p && *p != '&' && *p != ' ' && k < tam - 1; p++) {
        if (*p == '%' && p[1] && p[2]) { char h[3] = { p[1], p[2], 0 }; out[k++] = (char)strtol(h, NULL, 16); p += 2; }
        else if (*p == '+') out[k++] = ' ';
        else out[k++] = *p;
    }
    out[k] = 0;
}
static bool meetParam(const char* q, const char* nome, char* out, int tam) {
    out[0] = 0; if (!q) return false;
    size_t n = strlen(nome);
    for (const char* p = q; p && *p; ) {
        if (!strncmp(p, nome, n) && p[n] == '=') { meetUrlDec(p + n + 1, out, tam); return true; }
        p = strchr(p, '&'); if (p) p++;
    }
    return false;
}
// Nome de sala: letras, números, - e _ (vira parte do link).
static void meetSlug(const char* in, char* out, int tam) {
    int k = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p && k < tam - 1; p++) {
        unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out[k++] = (char)c;
        else if (c >= 'A' && c <= 'Z') out[k++] = (char)(c + 32);
        else if (c == ' ') out[k++] = '-';
    }
    out[k] = 0;
}
static void meetCodigo(char* out) {
    static const char* al = "abcdefghjkmnpqrstuvwxyz23456789";
    unsigned char r[8]; if (RAND_bytes(r, 8) != 1) for (int i = 0; i < 8; i++) r[i] = (unsigned char)(rand() ^ (int)time(NULL));
    for (int i = 0; i < 8; i++) out[i] = al[r[i] % 31];
    out[8] = 0;
}
static int meetAcharSala(const char* nome) { for (int i = 0; i < MEET_MAX_SALAS; i++) if (meetSalas[i].usada && !strcmp(meetSalas[i].nome, nome)) return i; return -1; }
// Grava as salas (nome, privada, código, aprovação, código do dono). Chame com meetTrava.
static void meetSalvarSalasTravado() {
    if (!meetPasta[0]) return;
    char c[340]; snprintf(c, sizeof(c), "%s/meet_salas.txt", meetPasta);
    FILE* f = fopen(c, "w"); if (!f) return;
    for (int i = 0; i < MEET_MAX_SALAS; i++) if (meetSalas[i].usada)
        fprintf(f, "%s\t%d\t%s\t%d\t%s\n", meetSalas[i].nome, meetSalas[i].privada ? 1 : 0, meetSalas[i].chave[0] ? meetSalas[i].chave : "-", meetSalas[i].aprovacao ? 1 : 0, meetSalas[i].dono);
    fclose(f);
}
static int meetCarregarSalas() {
    char c[340]; snprintf(c, sizeof(c), "%s/meet_salas.txt", meetPasta);
    FILE* f = fopen(c, "r"); if (!f) return 0;
    char l[200]; int n = 0;
    pthread_mutex_lock(&meetTrava);
    while (fgets(l, sizeof(l), f) && n < MEET_MAX_SALAS) {
        char nome[40] = "", chave[16] = "", dono[16] = ""; int pv = 0, ap = 0;
        if (sscanf(l, "%39[^\t]\t%d\t%15[^\t]\t%d\t%15s", nome, &pv, chave, &ap, dono) < 4 || !nome[0] || meetAcharSala(nome) >= 0) continue;
        MeetSala* s = &meetSalas[n++];
        memset(s, 0, sizeof(MeetSala));
        s->usada = true; s->privada = pv != 0; s->aprovacao = ap != 0; s->proximoId = 1;
        snprintf(s->nome, sizeof(s->nome), "%s", nome);
        if (strcmp(chave, "-")) snprintf(s->chave, sizeof(s->chave), "%s", chave);
        if (dono[0]) snprintf(s->dono, sizeof(s->dono), "%s", dono); else meetCodigo(s->dono);
        if (s->privada && !s->chave[0]) meetCodigo(s->chave);
    }
    pthread_mutex_unlock(&meetTrava);
    fclose(f);
    return n;
}
// Cria a sala (ou devolve a que já existe com o nome). Chame com meetTrava.
static int meetCriarSalaTravado(const char* nomePedido, bool privada, bool aprovacao, bool* nova = NULL) {
    if (nova) *nova = false;
    char nome[32]; meetSlug(nomePedido, nome, sizeof(nome));
    if (!nome[0]) return -1;
    int i = meetAcharSala(nome);
    if (i >= 0) return i;
    for (i = 0; i < MEET_MAX_SALAS; i++) if (!meetSalas[i].usada) break;
    if (i == MEET_MAX_SALAS) return -1;
    memset(&meetSalas[i], 0, sizeof(MeetSala));
    meetSalas[i].usada = true; meetSalas[i].privada = privada; meetSalas[i].aprovacao = aprovacao; meetSalas[i].proximoId = 1;
    snprintf(meetSalas[i].nome, sizeof(meetSalas[i].nome), "%s", nome);
    if (privada) meetCodigo(meetSalas[i].chave);
    meetCodigo(meetSalas[i].dono);
    if (nova) *nova = true;
    meetSalvarSalasTravado();
    return i;
}
static int meetPessoasNaSala(int s) { int n = meetSalas[s].ps4 ? 1 : 0; for (int k = 0; k < MEET_MAX_PESSOAS; k++) if (meetSalas[s].pessoas[k]) n++; return n; }
static int meetNovoId(int s) { int id = meetSalas[s].proximoId++; if (meetSalas[s].proximoId > 200) meetSalas[s].proximoId = 1; return id; }

// ---------------------------------------------------------------- fila de saída e WebSocket
static void meetEnfileirar(MeetCliente* c, const uint8_t* cab, int ncab, const uint8_t* dados, int n, bool video) {
    pthread_mutex_lock(&c->trava);
    if (video && c->bytes > MEET_FILA_MAX / 2) { pthread_mutex_unlock(&c->trava); return; }   // cliente lento: pula quadro
    if (c->bytes > MEET_FILA_MAX) { pthread_mutex_unlock(&c->trava); return; }
    MeetMsg* m = (MeetMsg*)malloc(sizeof(MeetMsg));
    if (m) m->dados = (uint8_t*)malloc(ncab + n);
    if (!m || !m->dados) { free(m); pthread_mutex_unlock(&c->trava); return; }
    memcpy(m->dados, cab, ncab); if (n) memcpy(m->dados + ncab, dados, n);
    m->n = ncab + n; m->video = video; m->prox = NULL;
    if (c->cauda) c->cauda->prox = m; else c->cabeca = m;
    c->cauda = m; c->bytes += m->n;
    pthread_mutex_unlock(&c->trava);
}
// Quadro WebSocket do servidor (sem máscara). op: 1 texto, 2 binário.
static void meetEnviarWs(MeetCliente* c, int op, const uint8_t* dados, int n, bool video) {
    uint8_t cab[10]; int k = 0;
    cab[k++] = (uint8_t)(0x80 | op);
    if (n < 126) cab[k++] = (uint8_t)n;
    else if (n < 65536) { cab[k++] = 126; cab[k++] = (uint8_t)(n >> 8); cab[k++] = (uint8_t)n; }
    else { cab[k++] = 127; for (int i = 7; i >= 0; i--) cab[k++] = (uint8_t)(i < 4 ? ((uint32_t)n >> (i * 8)) : 0); }
    meetEnfileirar(c, cab, k, dados, n, video);
}
static void meetEnviarTexto(MeetCliente* c, const char* t) { meetEnviarWs(c, 1, (const uint8_t*)t, (int)strlen(t), false); }
// Texto para todos da sala (menos 'exceto'). Chame com meetTrava.
static void meetTodosTexto(int s, const char* t, int exceto) {
    for (int k = 0; k < MEET_MAX_PESSOAS; k++) { MeetCliente* c = meetSalas[s].pessoas[k]; if (c && c->id != exceto) meetEnviarTexto(c, t); }
}
// Mídia de 'de' para a sala: [tipo][de][dados]. Pula quem já recebe direto por WebRTC.
static void meetRepassarMidia(int s, int de, int tipo, const uint8_t* dados, int n) {
    uint8_t cab[2] = { (uint8_t)tipo, (uint8_t)de };
    pthread_mutex_lock(&meetTrava);
    MeetCliente* origem = NULL;
    for (int k = 0; k < MEET_MAX_PESSOAS; k++) if (meetSalas[s].pessoas[k] && meetSalas[s].pessoas[k]->id == de) origem = meetSalas[s].pessoas[k];
    for (int k = 0; k < MEET_MAX_PESSOAS; k++) {
        MeetCliente* c = meetSalas[s].pessoas[k];
        if (!c || c->id == de) continue;
        if (origem && (origem->p2p & (1u << (c->id & 31)))) continue;
        uint8_t q[10]; int kk = 0; int tot = n + 2;
        q[kk++] = 0x82;
        if (tot < 126) q[kk++] = (uint8_t)tot; else if (tot < 65536) { q[kk++] = 126; q[kk++] = (uint8_t)(tot >> 8); q[kk++] = (uint8_t)tot; }
        else { q[kk++] = 127; for (int i = 7; i >= 0; i--) q[kk++] = (uint8_t)(i < 4 ? ((uint32_t)tot >> (i * 8)) : 0); }
        uint8_t junto[12]; memcpy(junto, q, kk); memcpy(junto + kk, cab, 2);
        meetEnfileirar(c, junto, kk + 2, dados, n, tipo == 1);
    }
    bool ps4 = meetSalas[s].ps4;
    pthread_mutex_unlock(&meetTrava);
    if (ps4 && de != 0) meetPs4Midia(s, de, tipo, dados, n);
}
// O PS4 fala na sala (áudio do microfone, chat).
static void meetPs4EnviarMidia(int s, int tipo, const uint8_t* dados, int n) { if (s >= 0 && s < MEET_MAX_SALAS && meetSalas[s].usada) meetRepassarMidia(s, 0, tipo, dados, n); }
static void meetPs4EnviarChat(int s, const char* texto) {
    if (s < 0 || s >= MEET_MAX_SALAS || !meetSalas[s].usada) return;
    char e1[60], e2[1200], j[1400];
    pthread_mutex_lock(&meetTrava);
    meetJsonEsc(meetSalas[s].ps4Nome, e1, sizeof(e1)); meetJsonEsc(texto, e2, sizeof(e2));
    snprintf(j, sizeof(j), "{\"t\":\"chat\",\"id\":0,\"nome\":\"%s\",\"m\":\"%s\"}", e1, e2);
    meetTodosTexto(s, j, -1);
    pthread_mutex_unlock(&meetTrava);
    meetPs4Texto(s, meetSalas[s].ps4Nome, texto, false);
}
static void meetExpulsar(int s, int id) {
    pthread_mutex_lock(&meetTrava);
    for (int k = 0; k < MEET_MAX_PESSOAS; k++) { MeetCliente* c = meetSalas[s].pessoas[k]; if (c && c->id == id) { meetEnviarTexto(c, "{\"t\":\"expulso\"}"); c->vivo = false; } }
    pthread_mutex_unlock(&meetTrava);
}
// Texto para os donos (navegadores com o código de dono) da sala. Chame com meetTrava.
static void meetDonosTexto(int s, const char* t) {
    for (int k = 0; k < MEET_MAX_PESSOAS; k++) { MeetCliente* c = meetSalas[s].pessoas[k]; if (c && c->dono) meetEnviarTexto(c, t); }
}
static void meetJsonPedido(MeetCliente* c, char* j, int tam) { char en[60]; meetJsonEsc(c->nome, en, sizeof(en)); snprintf(j, tam, "{\"t\":\"pedido\",\"id\":%d,\"nome\":\"%s\"}", c->id, en); }
// Aceita (ok) ou recusa um pedido de entrada. Devolve false se o pedido já não existe.
static bool meetDecidir(int s, int id, bool ok) {
    if (s < 0 || s >= MEET_MAX_SALAS) return false;
    bool achou = false;
    pthread_mutex_lock(&meetTrava);
    for (int e = 0; e < MEET_MAX_ESPERA; e++) { MeetCliente* c = meetSalas[s].espera[e]; if (c && c->id == id && c->decisao == 0) { c->decisao = ok ? 1 : 2; achou = true; } }
    if (achou) { char j[60]; snprintf(j, sizeof(j), "{\"t\":\"pedido_fim\",\"id\":%d}", id); meetDonosTexto(s, j); }
    pthread_mutex_unlock(&meetTrava);
    return achou;
}
// Primeiro pedido esperando na sala (para a tela do PS4). Devolve o id ou -1.
static int meetPrimeiroPedido(int s, char* nome, int tam) {
    int id = -1;
    pthread_mutex_lock(&meetTrava);
    for (int e = 0; e < MEET_MAX_ESPERA && id < 0; e++) { MeetCliente* c = meetSalas[s].espera[e]; if (c && c->decisao == 0) { id = c->id; snprintf(nome, tam, "%s", c->nome); } }
    pthread_mutex_unlock(&meetTrava);
    return id;
}
static int meetQuantosPedidos(int s) { int n = 0; pthread_mutex_lock(&meetTrava); for (int e = 0; e < MEET_MAX_ESPERA; e++) if (meetSalas[s].espera[e] && meetSalas[s].espera[e]->decisao == 0) n++; pthread_mutex_unlock(&meetTrava); return n; }
// O PS4 liga ou desliga o microfone: os navegadores mostram o ícone.
static void meetPs4Estado(int s, bool mic) {
    if (s < 0 || s >= MEET_MAX_SALAS) return;
    pthread_mutex_lock(&meetTrava);
    if (meetSalas[s].usada) { meetSalas[s].ps4Mic = mic; char j[80]; snprintf(j, sizeof(j), "{\"t\":\"estado\",\"id\":0,\"mic\":%s,\"cam\":false}", mic ? "true" : "false"); meetTodosTexto(s, j, -1); }
    pthread_mutex_unlock(&meetTrava);
}
// Apaga a sala de vez: todos saem e o link deixa de valer.
static void meetApagarSala(int s) {
    if (s < 0 || s >= MEET_MAX_SALAS) return;
    pthread_mutex_lock(&meetTrava);
    if (meetSalas[s].usada) {
        meetTodosTexto(s, "{\"t\":\"erro\",\"m\":\"A sala foi apagada no PS4.\"}", -1);
        for (int k = 0; k < MEET_MAX_PESSOAS; k++) if (meetSalas[s].pessoas[k]) meetSalas[s].pessoas[k]->vivo = false;
        for (int e = 0; e < MEET_MAX_ESPERA; e++) if (meetSalas[s].espera[e]) meetSalas[s].espera[e]->decisao = 2;
        meetSalas[s].usada = false; meetSalas[s].ps4 = false;
        meetSalvarSalasTravado();
    }
    pthread_mutex_unlock(&meetTrava);
}

// ---------------------------------------------------------------- E/S de cada conexão
static bool meetEscreverTudo(SSL* ssl, const uint8_t* d, int n) {
    while (n > 0) { int k = SSL_write(ssl, d, n); if (k <= 0) return false; d += k; n -= k; }
    return true;
}
static bool meetDescarregar(MeetCliente* c) {
    for (;;) {
        pthread_mutex_lock(&c->trava);
        MeetMsg* m = c->cabeca;
        if (m) { c->cabeca = m->prox; if (!c->cabeca) c->cauda = NULL; c->bytes -= m->n; }
        pthread_mutex_unlock(&c->trava);
        if (!m) return true;
        bool ok = meetEscreverTudo(c->ssl, m->dados, m->n);
        free(m->dados); free(m);
        if (!ok) return false;
    }
}
static bool meetPronto(MeetCliente* c, int ms) {
    if (SSL_pending(c->ssl) > 0) return true;
#ifdef _WIN32
    fd_set f; FD_ZERO(&f); FD_SET(c->fd, &f); timeval tv = { 0, ms * 1000 };
    return select(0, &f, NULL, NULL, &tv) > 0;
#else
    struct pollfd p = { c->fd, POLLIN, 0 };
    return poll(&p, 1, ms) > 0;
#endif
}
static int meetLerExato(MeetCliente* c, uint8_t* d, int n) {
    int feito = 0;
    while (feito < n) {
        if (!meetPronto(c, 1000)) { if (!c->vivo) return -1; continue; }
        int k = SSL_read(c->ssl, d + feito, n - feito);
        if (k <= 0) return -1;
        feito += k;
    }
    return feito;
}

static void meetResposta(SSL* ssl, const char* status, const char* tipo, const char* corpo, int n) {
    char cab[400];
    int k = snprintf(cab, sizeof(cab), "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", status, tipo, n);
    meetEscreverTudo(ssl, (const uint8_t*)cab, k);
    if (n) meetEscreverTudo(ssl, (const uint8_t*)corpo, n);
}

// Onde começa o valor de "chave" no JSON (aceita espaços em volta dos dois-pontos).
static const char* meetJsonValor(const char* t, const char* chave) {
    char k[40]; snprintf(k, sizeof(k), "\"%s\"", chave);
    for (const char* p = strstr(t, k); p; p = strstr(p + 1, k)) {
        const char* v = p + strlen(k);
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        if (*v != ':') continue;
        v++;
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        return v;
    }
    return NULL;
}
// Lê uma string JSON (com \", \\, \n e \uXXXX, inclusive pares de emoji) em UTF-8.
static bool meetJsonTexto(const char* t, const char* chave, char* out, int tam) {
    out[0] = 0;
    const char* v = meetJsonValor(t, chave);
    if (!v || *v != '"') return false;
    int k = 0;
    for (v++; *v && *v != '"' && k < tam - 5; v++) {
        if (*v != '\\' || !v[1]) { out[k++] = *v; continue; }
        v++;
        if (*v == 'u' && v[1] && v[2] && v[3] && v[4]) {
            char h[5] = { v[1], v[2], v[3], v[4], 0 }; unsigned cp = (unsigned)strtoul(h, NULL, 16); v += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && v[1] == '\\' && v[2] == 'u') { char h2[5] = { v[3], v[4], v[5], v[6], 0 }; unsigned lo = (unsigned)strtoul(h2, NULL, 16); if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); v += 6; } }
            if (cp < 0x80) out[k++] = (char)cp;
            else if (cp < 0x800) { out[k++] = (char)(0xC0 | (cp >> 6)); out[k++] = (char)(0x80 | (cp & 63)); }
            else if (cp < 0x10000) { out[k++] = (char)(0xE0 | (cp >> 12)); out[k++] = (char)(0x80 | ((cp >> 6) & 63)); out[k++] = (char)(0x80 | (cp & 63)); }
            else { out[k++] = (char)(0xF0 | (cp >> 18)); out[k++] = (char)(0x80 | ((cp >> 12) & 63)); out[k++] = (char)(0x80 | ((cp >> 6) & 63)); out[k++] = (char)(0x80 | (cp & 63)); }
        }
        else out[k++] = *v == 'n' || *v == 'r' || *v == 't' ? ' ' : *v;
    }
    out[k] = 0;
    return true;
}

// Mensagem de texto (JSON) vinda de um navegador.
static void meetTextoDoCliente(MeetCliente* c, const char* t) {
    int s = c->sala;
    char tipo[16]; meetJsonTexto(t, "t", tipo, sizeof(tipo));
    if (!strcmp(tipo, "chat")) {
        char txt[1100];
        if (!meetJsonTexto(t, "m", txt, sizeof(txt)) || !txt[0]) return;
        char e1[60], e2[2200], j[2400];
        meetJsonEsc(c->nome, e1, sizeof(e1)); meetJsonEsc(txt, e2, sizeof(e2));
        snprintf(j, sizeof(j), "{\"t\":\"chat\",\"id\":%d,\"nome\":\"%s\",\"m\":\"%s\"}", c->id, e1, e2);
        pthread_mutex_lock(&meetTrava); meetTodosTexto(s, j, -1); bool ps4 = meetSalas[s].ps4; pthread_mutex_unlock(&meetTrava);
        if (ps4) meetPs4Texto(s, c->nome, txt, false);
    }
    else if (!strcmp(tipo, "rtc")) {
        // sinalização do WebRTC: repassa para 'para', trocando o destino pela origem
        const char* p = meetJsonValor(t, "para"); if (!p) return;
        int para = atoi(p);
        size_t n = strlen(t);
        char* j = (char*)malloc(n + 32); if (!j) return;
        snprintf(j, n + 32, "{\"de\":%d,%s", c->id, t + 1);
        pthread_mutex_lock(&meetTrava);
        for (int k = 0; k < MEET_MAX_PESSOAS; k++) { MeetCliente* o = meetSalas[s].pessoas[k]; if (o && o->id == para) meetEnviarTexto(o, j); }
        pthread_mutex_unlock(&meetTrava);
        free(j);
    }
    else if (!strcmp(tipo, "p2p")) {
        const char* p = meetJsonValor(t, "com"); if (!p) return;
        int com = atoi(p); const char* vok = meetJsonValor(t, "ok"); bool ok = vok && !strncmp(vok, "true", 4);
        pthread_mutex_lock(&meetTrava);
        if (ok) c->p2p |= 1u << (com & 31); else c->p2p &= ~(1u << (com & 31));
        pthread_mutex_unlock(&meetTrava);
    }
    else if (!strcmp(tipo, "estado")) {
        const char* vm = meetJsonValor(t, "mic"); const char* vc = meetJsonValor(t, "cam");
        pthread_mutex_lock(&meetTrava);
        if (vm) c->micOn = !strncmp(vm, "true", 4);
        if (vc) c->camOn = !strncmp(vc, "true", 4);
        char j[100]; snprintf(j, sizeof(j), "{\"t\":\"estado\",\"id\":%d,\"mic\":%s,\"cam\":%s}", c->id, c->micOn ? "true" : "false", c->camOn ? "true" : "false");
        meetTodosTexto(s, j, c->id);
        pthread_mutex_unlock(&meetTrava);
    }
    else if (!strcmp(tipo, "aprovar") && c->dono) {
        const char* p = meetJsonValor(t, "id"); if (!p) return;
        const char* vok = meetJsonValor(t, "ok");
        meetDecidir(s, atoi(p), vok && !strncmp(vok, "true", 4));
    }
}

// Lê um quadro WebSocket do navegador (tira a máscara). Devolve o tamanho ou -1; buf tem 1 MB.
static int meetLerQuadro(MeetCliente* c, uint8_t* buf, int* op) {
    uint8_t h[14];
    if (meetLerExato(c, h, 2) != 2) return -1;
    *op = h[0] & 15; bool mascara = (h[1] & 0x80) != 0; uint64_t n = h[1] & 127;
    if (n == 126) { if (meetLerExato(c, h + 2, 2) != 2) return -1; n = ((uint64_t)h[2] << 8) | h[3]; }
    else if (n == 127) { if (meetLerExato(c, h + 2, 8) != 8) return -1; n = 0; for (int i = 2; i < 10; i++) n = (n << 8) | h[i]; }
    if (n > 1024 * 1024 - 1) return -1;
    uint8_t m[4] = { 0, 0, 0, 0 };
    if (mascara && meetLerExato(c, m, 4) != 4) return -1;
    if (n && meetLerExato(c, buf, (int)n) != (int)n) return -1;
    for (uint64_t i = 0; i < n; i++) buf[i] ^= m[i & 3];
    buf[n] = 0;
    return (int)n;
}
static void meetSessaoWs(MeetCliente* c, uint8_t* buf) {
    while (buf && c->vivo && meetLigado) {
        if (!meetDescarregar(c)) break;
        if (!meetPronto(c, 15)) continue;
        int op = 0, n = meetLerQuadro(c, buf, &op);
        if (n < 0 || op == 8) break;
        if (op == 9) { meetEnviarWs(c, 10, buf, n, false); continue; }
        if (op == 1) meetTextoDoCliente(c, (const char*)buf);
        else if (op == 2 && n >= 2 && (buf[0] == 1 || buf[0] == 2)) meetRepassarMidia(c->sala, c->id, buf[0], buf + 2, n - 2);
    }
}

static void meetBase64(const unsigned char* in, int n, char* out) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int k = 0;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        out[k++] = t[(v >> 18) & 63]; out[k++] = t[(v >> 12) & 63];
        out[k++] = i + 1 < n ? t[(v >> 6) & 63] : '='; out[k++] = i + 2 < n ? t[v & 63] : '=';
    }
    out[k] = 0;
}

static void* meetAtender(void* arg) {
    MeetCliente* c = (MeetCliente*)arg;
    c->ssl = SSL_new(meetCtx);
    SSL_set_fd(c->ssl, (int)c->fd);
    if (SSL_accept(c->ssl) <= 0) { SSL_free(c->ssl); meetFechar(c->fd); free(c); return NULL; }
    char req[4096]; int lido = 0;
    while (lido < (int)sizeof(req) - 1) {
        int k = SSL_read(c->ssl, req + lido, (int)sizeof(req) - 1 - lido);
        if (k <= 0) break;
        lido += k; req[lido] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    req[lido] = 0;
    char metodo[8] = "", alvo[600] = "";
    sscanf(req, "%7s %599s", metodo, alvo);
    char caminho[600]; snprintf(caminho, sizeof(caminho), "%s", alvo);
    char* q = strchr(caminho, '?'); if (q) *q++ = 0;
    bool ws = false;
    if (!strcmp(caminho, "/") || !strcmp(caminho, "/meet") || !strncmp(caminho, "/meet/", 6)) {
        if (!strcmp(caminho, "/meet/salas")) {
            char j[2048] = "["; int k = 1;
            pthread_mutex_lock(&meetTrava);
            for (int i = 0; i < MEET_MAX_SALAS; i++) if (meetSalas[i].usada && !meetSalas[i].privada)
                k += snprintf(j + k, sizeof(j) - k, "%s{\"nome\":\"%s\",\"pessoas\":%d,\"aprovacao\":%s}", k > 1 ? "," : "", meetSalas[i].nome, meetPessoasNaSala(i), meetSalas[i].aprovacao ? "true" : "false");
            pthread_mutex_unlock(&meetTrava);
            snprintf(j + k, sizeof(j) - k, "]");
            meetResposta(c->ssl, "200 OK", "application/json", j, (int)strlen(j));
        }
        else if (!strcmp(caminho, "/meet/criar")) {
            char nome[64], pv[4], ap[4]; meetParam(q, "nome", nome, sizeof(nome)); meetParam(q, "privada", pv, sizeof(pv)); meetParam(q, "aprovacao", ap, sizeof(ap));
            pthread_mutex_lock(&meetTrava);
            bool nova = false;
            int s = meetCriarSalaTravado(nome, pv[0] == '1', ap[0] == '1', &nova);
            char j[200];
            if (s < 0) snprintf(j, sizeof(j), "{\"erro\":\"Nome invalido ou limite de salas (apague salas antigas no PS4)\"}");
            else if (!nova) snprintf(j, sizeof(j), "{\"erro\":\"Ja existe uma sala com esse nome. Escolha outro.\"}");
            else snprintf(j, sizeof(j), "{\"sala\":\"%s\",\"k\":\"%s\",\"d\":\"%s\"}", meetSalas[s].nome, meetSalas[s].chave, meetSalas[s].dono);
            pthread_mutex_unlock(&meetTrava);
            if (s >= 0) meetLog("MEET: sala %s criada pelo navegador", nome);
            meetResposta(c->ssl, "200 OK", "application/json", j, (int)strlen(j));
        }
        else meetResposta(c->ssl, "200 OK", "text/html; charset=utf-8", MEET_PAGINA, (int)sizeof(MEET_PAGINA) - 1);
    }
    else if (!strncmp(caminho, "/ws/", 4) && strstr(req, "Sec-WebSocket-Key:")) {
        char sala[64], k[16], d[16], nome[64];
        meetUrlDec(caminho + 4, sala, sizeof(sala)); meetParam(q, "k", k, sizeof(k)); meetParam(q, "d", d, sizeof(d)); meetParam(q, "nome", nome, sizeof(nome));
        const char* kp = strstr(req, "Sec-WebSocket-Key:") + 18; while (*kp == ' ') kp++;
        char chaveWs[80]; int n = 0; while (kp[n] && kp[n] != '\r' && n < 60) { chaveWs[n] = kp[n]; n++; } chaveWs[n] = 0;
        strcat(chaveWs, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        unsigned char sha[20]; SHA1((const unsigned char*)chaveWs, strlen(chaveWs), sha);
        char aceita[40]; meetBase64(sha, 20, aceita);
        char r[300]; int nr = snprintf(r, sizeof(r), "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", aceita);
        meetEscreverTudo(c->ssl, (const uint8_t*)r, nr);
        ws = true;
        // entra na sala
        pthread_mutex_init(&c->trava, NULL);
        c->vivo = true;
        snprintf(c->nome, sizeof(c->nome), "%.20s", nome[0] ? nome : "Convidado");
        c->micOn = c->camOn = true;
        uint8_t* buf = (uint8_t*)malloc(1024 * 1024);
        pthread_mutex_lock(&meetTrava);
        int s = meetAcharSala(sala);
        const char* erro = NULL;
        bool dono = s >= 0 && d[0] && !strcmp(d, meetSalas[s].dono);
        if (!buf) erro = "Sem memoria no PS4.";
        else if (s < 0) erro = "Sala nao existe (o link esta errado ou a sala foi apagada no PS4).";
        else if (meetSalas[s].privada && !dono && strcmp(k, meetSalas[s].chave) != 0) erro = "Sala privada: o link precisa do codigo.";
        c->dono = dono;
        // sala com aprovação: espera o dono (PS4 ou navegador com o código de dono) aceitar
        bool esperar = !erro && meetSalas[s].aprovacao && !dono;
        if (esperar) {
            int e = 0; while (e < MEET_MAX_ESPERA && meetSalas[s].espera[e]) e++;
            if (e == MEET_MAX_ESPERA) erro = "Muitos pedidos esperando. Tente de novo daqui a pouco.";
            else {
                c->sala = s; c->id = meetNovoId(s); c->decisao = 0; meetSalas[s].espera[e] = c;
                char j[120]; meetJsonPedido(c, j, sizeof(j)); meetDonosTexto(s, j);
            }
        }
        bool ps4Esp = !erro && esperar && meetSalas[s].ps4;
        pthread_mutex_unlock(&meetTrava);
        if (!erro && esperar) {
            bool alguem = false;
            pthread_mutex_lock(&meetTrava);
            alguem = meetSalas[s].ps4; for (int i = 0; i < MEET_MAX_PESSOAS; i++) if (meetSalas[s].pessoas[i] && meetSalas[s].pessoas[i]->dono) alguem = true;
            pthread_mutex_unlock(&meetTrava);
            meetEnviarTexto(c, alguem ? "{\"t\":\"aguardando\",\"m\":\"Esperando o dono da sala aprovar a sua entrada...\"}" : "{\"t\":\"aguardando\",\"m\":\"O dono da sala ainda nao esta. Assim que ele entrar, ve o seu pedido.\"}");
            if (ps4Esp) meetPs4Texto(s, c->nome, "pediu para entrar (X aceita, O recusa)", true);
            meetLog("MEET: %s pediu para entrar na sala %s", c->nome, meetSalas[s].nome);
            time_t ini = time(NULL);
            while (c->decisao == 0 && c->vivo && meetLigado && time(NULL) - ini < 1800) {
                if (!meetDescarregar(c)) break;
                if (!meetPronto(c, 100)) continue;
                int op = 0, n = meetLerQuadro(c, buf, &op);
                if (n < 0 || op == 8) break;
                if (op == 9) meetEnviarWs(c, 10, buf, n, false);
            }
            pthread_mutex_lock(&meetTrava);
            for (int e = 0; e < MEET_MAX_ESPERA; e++) if (meetSalas[s].espera[e] == c) meetSalas[s].espera[e] = NULL;
            int dec = c->decisao;
            if (dec == 0) { char j[60]; snprintf(j, sizeof(j), "{\"t\":\"pedido_fim\",\"id\":%d}", c->id); meetDonosTexto(s, j); }
            pthread_mutex_unlock(&meetTrava);
            if (dec == 2) erro = "O dono da sala nao aprovou a sua entrada.";
            else if (dec == 0) c->vivo = false;   // desistiu ou caiu: só encerra
        }
        int vaga = -1;
        if (!erro && c->vivo) {
            pthread_mutex_lock(&meetTrava);
            if (!meetSalas[s].usada) erro = "A sala foi apagada no PS4.";
            else { for (int i = 0; i < MEET_MAX_PESSOAS; i++) if (!meetSalas[s].pessoas[i]) { vaga = i; break; } if (vaga < 0) erro = "Sala cheia."; }
            if (erro) pthread_mutex_unlock(&meetTrava);
        }
        if (erro) {
            char j[200]; snprintf(j, sizeof(j), "{\"t\":\"erro\",\"m\":\"%s\"}", erro);
            meetEnviarTexto(c, j); meetDescarregar(c);
        }
        else if (c->vivo) {
            // (meetTrava continua travada desde a procura da vaga)
            c->sala = s; if (!esperar) c->id = meetNovoId(s);
            meetSalas[s].pessoas[vaga] = c;
            char lista[1800]; int kl = 0; char en[60];
            if (meetSalas[s].ps4) { meetJsonEsc(meetSalas[s].ps4Nome, en, sizeof(en)); kl += snprintf(lista + kl, sizeof(lista) - kl, "{\"id\":0,\"nome\":\"%s\",\"ps4\":true,\"mic\":%s,\"cam\":false}", en, meetSalas[s].ps4Mic ? "true" : "false"); }
            for (int i = 0; i < MEET_MAX_PESSOAS; i++) { MeetCliente* o = meetSalas[s].pessoas[i]; if (o && o != c) { meetJsonEsc(o->nome, en, sizeof(en)); kl += snprintf(lista + kl, sizeof(lista) - kl, "%s{\"id\":%d,\"nome\":\"%s\",\"mic\":%s,\"cam\":%s}", kl ? "," : "", o->id, en, o->micOn ? "true" : "false", o->camOn ? "true" : "false"); } }
            lista[kl] = 0;
            char ped[800]; int kp = 0; ped[0] = 0;
            if (dono) for (int e = 0; e < MEET_MAX_ESPERA; e++) { MeetCliente* o = meetSalas[s].espera[e]; if (o && o->decisao == 0) { meetJsonEsc(o->nome, en, sizeof(en)); kp += snprintf(ped + kp, sizeof(ped) - kp, "%s{\"id\":%d,\"nome\":\"%s\"}", kp ? "," : "", o->id, en); } }
            char j[2900]; snprintf(j, sizeof(j), "{\"t\":\"bemvindo\",\"id\":%d,\"privada\":%s,\"aprovacao\":%s,\"dono\":%s,\"k\":\"%s\",\"pessoas\":[%s],\"pedidos\":[%s]}",
                c->id, meetSalas[s].privada ? "true" : "false", meetSalas[s].aprovacao ? "true" : "false", dono ? "true" : "false", meetSalas[s].chave, lista, ped);
            meetEnviarTexto(c, j);
            meetJsonEsc(c->nome, en, sizeof(en));
            snprintf(j, sizeof(j), "{\"t\":\"entrou\",\"id\":%d,\"nome\":\"%s\"}", c->id, en);
            meetTodosTexto(s, j, c->id);
            bool ps4 = meetSalas[s].ps4;
            pthread_mutex_unlock(&meetTrava);
            if (ps4) meetPs4Texto(s, c->nome, "entrou", true);
            meetLog("MEET: %s entrou na sala %s%s", c->nome, meetSalas[s].nome, dono ? " (dono)" : "");
            meetSessaoWs(c, buf);
            // sai da sala (a sala continua salva: o link vale da próxima vez)
            pthread_mutex_lock(&meetTrava);
            for (int i = 0; i < MEET_MAX_PESSOAS; i++) if (meetSalas[s].pessoas[i] == c) meetSalas[s].pessoas[i] = NULL;
            for (int i = 0; i < MEET_MAX_PESSOAS; i++) { MeetCliente* o = meetSalas[s].pessoas[i]; if (o) o->p2p &= ~(1u << (c->id & 31)); }
            snprintf(j, sizeof(j), "{\"t\":\"saiu\",\"id\":%d,\"nome\":\"%s\"}", c->id, en);
            meetTodosTexto(s, j, -1);
            ps4 = meetSalas[s].ps4;
            pthread_mutex_unlock(&meetTrava);
            if (ps4) meetPs4Texto(s, c->nome, "saiu", true);
        }
        free(buf);
        // espera ninguém mais estar enfileirando (os outros usam meetTrava) e limpa a fila
        pthread_mutex_lock(&meetTrava); pthread_mutex_unlock(&meetTrava);
        pthread_mutex_lock(&c->trava);
        for (MeetMsg* m = c->cabeca; m; ) { MeetMsg* p = m->prox; free(m->dados); free(m); m = p; }
        c->cabeca = c->cauda = NULL;
        pthread_mutex_unlock(&c->trava);
        pthread_mutex_destroy(&c->trava);
    }
    else {
        static const char* nf = "Hyper Neiva Meet: abra /meet";
        meetResposta(c->ssl, "404 Not Found", "text/plain", nf, (int)strlen(nf));
    }
    (void)ws;
    SSL_free(c->ssl);   // sem SSL_shutdown: o nome colide com o da biblioteca SSL da Sony
    meetFechar(c->fd);
    free(c);
    return NULL;
}

static MeetSock meetServidor = MEET_INVALIDO;
static void* meetAceitar(void*) {
    while (meetLigado) {
        struct sockaddr_in de; socklen_t t = sizeof(de);
        MeetSock fd = accept(meetServidor, (struct sockaddr*)&de, &t);
        if (fd == MEET_INVALIDO) { if (!meetLigado) break; meetDormirMs(50); continue; }
        int um = 1; setsockopt(fd, IPPROTO_TCP, 1 /* TCP_NODELAY */, (const char*)&um, sizeof(um));
        MeetCliente* c = (MeetCliente*)calloc(1, sizeof(MeetCliente));
        if (!c) { meetFechar(fd); continue; }
        c->fd = fd; c->sala = -1;
        pthread_t th; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256 * 1024);
        if (pthread_create(&th, &at, meetAtender, c) == 0) pthread_detach(th); else { meetFechar(fd); free(c); }
        pthread_attr_destroy(&at);
    }
    return NULL;
}
// Liga o servidor do Meet (uma vez). pastaConfig guarda o certificado.
static bool meetIniciar(const char* pastaConfig) {
    if (meetLigado) return true;
    snprintf(meetPasta, sizeof(meetPasta), "%s", pastaConfig);
    X509* cert = NULL; EVP_PKEY* chave = NULL;
    if (!meetCertificado(pastaConfig, &cert, &chave)) { meetLog("MEET: sem certificado (%s)", meetErro); return false; }
    meetCtx = SSL_CTX_new(TLS_server_method());
    if (!meetCtx || SSL_CTX_use_certificate(meetCtx, cert) != 1 || SSL_CTX_use_PrivateKey(meetCtx, chave) != 1) { snprintf(meetErro, sizeof(meetErro), "TLS nao iniciou"); meetLog("MEET: %s", meetErro); return false; }
    meetServidor = socket(AF_INET, SOCK_STREAM, 0);
    int um = 1; setsockopt(meetServidor, SOL_SOCKET, SO_REUSEADDR, (const char*)&um, sizeof(um));
    struct sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(MEET_PORTA); a.sin_addr.s_addr = htonl(MEET_ENDERECO);
    if (bind(meetServidor, (struct sockaddr*)&a, sizeof(a)) != 0 || listen(meetServidor, 16) != 0) { snprintf(meetErro, sizeof(meetErro), "porta %d ocupada", MEET_PORTA); meetLog("MEET: %s", meetErro); meetFechar(meetServidor); return false; }
    int salvas = meetCarregarSalas();
    meetLigado = true;
    pthread_t th; if (pthread_create(&th, NULL, meetAceitar, NULL) == 0) pthread_detach(th);
    meetLog("MEET: servidor ligado na porta %d (%d salas salvas)", MEET_PORTA, salvas);
    return true;
}
// O PS4 abre (ou entra em) uma sala como participante.
// Se a sala já existe (salva), mantém o tipo e o código: o link continua o mesmo.
static int meetPs4AbrirSala(const char* nome, bool privada, bool aprovacao, const char* nomePs4, bool micLigado = true) {
    pthread_mutex_lock(&meetTrava);
    int s = meetCriarSalaTravado(nome, privada, aprovacao);
    if (s >= 0 && (meetSalas[s].privada != privada || meetSalas[s].aprovacao != aprovacao)) {
        // o PS4 mudou o tipo de uma sala salva: a privada mantém o código que já tinha
        meetSalas[s].privada = privada; meetSalas[s].aprovacao = aprovacao;
        if (privada && !meetSalas[s].chave[0]) meetCodigo(meetSalas[s].chave);
        meetSalvarSalasTravado();
    }
    if (s >= 0) {
        meetSalas[s].ps4 = true; meetSalas[s].ps4Mic = micLigado;
        snprintf(meetSalas[s].ps4Nome, sizeof(meetSalas[s].ps4Nome), "%.20s", nomePs4 && nomePs4[0] ? nomePs4 : "PS4");
        char en[60], j[200]; meetJsonEsc(meetSalas[s].ps4Nome, en, sizeof(en));
        snprintf(j, sizeof(j), "{\"t\":\"entrou\",\"id\":0,\"nome\":\"%s\",\"ps4\":true}", en);
        meetTodosTexto(s, j, -1);
    }
    pthread_mutex_unlock(&meetTrava);
    return s;
}
// O PS4 encerra a chamada: todos saem, mas a sala continua salva (o link vale de novo depois).
static void meetPs4FecharSala(int s) {
    if (s < 0 || s >= MEET_MAX_SALAS) return;
    pthread_mutex_lock(&meetTrava);
    if (meetSalas[s].usada) {
        meetTodosTexto(s, "{\"t\":\"erro\",\"m\":\"O PS4 encerrou a chamada. O link continua valendo para a proxima.\"}", -1);
        for (int k = 0; k < MEET_MAX_PESSOAS; k++) if (meetSalas[s].pessoas[k]) meetSalas[s].pessoas[k]->vivo = false;
        for (int e = 0; e < MEET_MAX_ESPERA; e++) if (meetSalas[s].espera[e]) meetSalas[s].espera[e]->decisao = 2;
        meetSalas[s].ps4 = false;
    }
    pthread_mutex_unlock(&meetTrava);
}
