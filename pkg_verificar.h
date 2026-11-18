// =========================================================================
// Confere um PKG do PS4 pelos SHA256 que o próprio cabeçalho declara, lendo o arquivo uma vez
// só, em ordem (dá para alimentar durante uma cópia, sem ler o disco de novo).
//   0xFE0: SHA256 do cabeçalho (0..0xFE0)
//   0x160: SHA256 do "body" (body_offset 0x20, body_size 0x28)
//   0x440: SHA256 da imagem PFS inteira (pfs_image_offset 0x410, pfs_image_size 0x418) — os dados do jogo
//   0x460: SHA256 do começo assinado da PFS (pfs_signed_size 0x438)
// Hash zerado = a ferramenta que gerou o PKG não preencheu: não dá para conferir aquela parte,
// nunca é "corrompido" (ex.: um UP do Shovel Knight vem com 0x440 zerado).
// Incluído pelo extra.cpp (Telegram) e pelo teste do PC.
// =========================================================================
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef HN_SHA256_DECLARADO
#define HN_SHA256_DECLARADO
// OpenSSL 1.1.1 (libcrypto já linkada no app); estrutura igual à do openssl/sha.h.
extern "C" {
typedef struct SHA256state_st { unsigned int h[8]; unsigned int Nl, Nh; unsigned int data[16]; unsigned int num, md_len; } SHA256_CTX;
int SHA256_Init(SHA256_CTX* c);
int SHA256_Update(SHA256_CTX* c, const void* data, size_t len);
int SHA256_Final(unsigned char* md, SHA256_CTX* c);
}
#endif

struct HnPkgVerif {
    uint8_t cab[0x1000];
    long long pos;                 // bytes já alimentados
    bool cabLido, ehPkg;
    long long tamDeclarado;
    struct Regiao { bool usar; long long ini, fim; SHA256_CTX ctx; uint8_t esperado[32]; const char* nome; } reg[3];
    bool usarCab;
};

static bool hnPkgZerado(const uint8_t* h) { for (int i = 0; i < 32; i++) if (h[i]) return false; return true; }
static uint64_t hnPkgBe64(const uint8_t* p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v = (v << 8) | p[i]; return v; }
static uint32_t hnPkgBe32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static void hnPkgVerifIniciar(HnPkgVerif* v) { memset(v, 0, sizeof(*v)); }

// Passa um pedaço [ini, ini+n) do arquivo pelas regiões que ele toca.
static void hnPkgVerifRegioes(HnPkgVerif* v, long long ini, const uint8_t* d, size_t n) {
    long long fim = ini + (long long)n;
    for (int r = 0; r < 3; r++) {
        HnPkgVerif::Regiao* g = &v->reg[r];
        if (!g->usar) continue;
        long long a = ini > g->ini ? ini : g->ini, b = fim < g->fim ? fim : g->fim;
        if (a < b) SHA256_Update(&g->ctx, d + (a - ini), (size_t)(b - a));
    }
}

static void hnPkgVerifLerCabecalho(HnPkgVerif* v) {
    v->cabLido = true;
    const uint8_t* c = v->cab;
    v->ehPkg = c[0] == 0x7F && c[1] == 'C' && c[2] == 'N' && c[3] == 'T';
    if (!v->ehPkg) return;
    v->tamDeclarado = (long long)hnPkgBe64(c + 0x430);
    v->usarCab = !hnPkgZerado(c + 0xFE0);
    long long bodyIni = (long long)hnPkgBe64(c + 0x20), bodyTam = (long long)hnPkgBe64(c + 0x28);
    long long pfsIni = (long long)hnPkgBe64(c + 0x410), pfsTam = (long long)hnPkgBe64(c + 0x418);
    long long assinado = (long long)hnPkgBe32(c + 0x438);
    struct { long long ini, tam; int off; const char* nome; } def[3] = {
        { bodyIni, bodyTam, 0x160, "corpo" }, { pfsIni, pfsTam, 0x440, "dados do jogo (PFS)" }, { pfsIni, assinado, 0x460, "inicio da PFS" } };
    for (int r = 0; r < 3; r++) {
        HnPkgVerif::Regiao* g = &v->reg[r];
        g->nome = def[r].nome;
        memcpy(g->esperado, c + def[r].off, 32);
        g->usar = def[r].tam > 0 && def[r].ini >= 0 && !hnPkgZerado(g->esperado);
        g->ini = def[r].ini; g->fim = def[r].ini + def[r].tam;
        if (g->usar) SHA256_Init(&g->ctx);
    }
    hnPkgVerifRegioes(v, 0, v->cab, sizeof(v->cab));   // regiões que começassem antes de 0x1000
}

// Alimenta os próximos n bytes do arquivo, em ordem.
static void hnPkgVerifAlimentar(HnPkgVerif* v, const uint8_t* d, size_t n) {
    while (n > 0 && v->pos < (long long)sizeof(v->cab)) {
        size_t k = (size_t)((long long)sizeof(v->cab) - v->pos); if (k > n) k = n;
        memcpy(v->cab + v->pos, d, k);
        v->pos += (long long)k; d += k; n -= k;
        if (v->pos == (long long)sizeof(v->cab)) hnPkgVerifLerCabecalho(v);
    }
    if (n == 0) return;
    if (v->ehPkg) hnPkgVerifRegioes(v, v->pos, d, n);
    v->pos += (long long)n;
}

// 1 = conferido (inclusive os dados do jogo), 0 = corrompido (motivo diz onde), -1 = não dá para
// conferir de verdade (não é PKG, ou o PKG não traz o hash dos dados do jogo: só cabeçalho e
// começo batem, um estrago no meio não apareceria — então não dá para dizer "conferido").
static int hnPkgVerifTerminar(HnPkgVerif* v, char* motivo, int tam) {
    if (motivo && tam) motivo[0] = 0;
    if (!v->cabLido || !v->ehPkg) return -1;
    if (v->tamDeclarado > 0 && v->pos != v->tamDeclarado) {
        if (motivo) snprintf(motivo, tam, "tamanho %lld, o PKG diz %lld", v->pos, v->tamDeclarado);
        return 0;
    }
    int conferidos = 0;
    if (v->usarCab) {
        SHA256_CTX c; uint8_t h[32]; SHA256_Init(&c); SHA256_Update(&c, v->cab, 0xFE0); SHA256_Final(h, &c);
        if (memcmp(h, v->cab + 0xFE0, 32)) { if (motivo) snprintf(motivo, tam, "cabecalho"); return 0; }
        conferidos++;
    }
    for (int r = 0; r < 3; r++) {
        HnPkgVerif::Regiao* g = &v->reg[r];
        if (!g->usar) continue;
        uint8_t h[32]; SHA256_Final(h, &g->ctx);
        if (v->pos < g->fim || memcmp(h, g->esperado, 32)) { if (motivo) snprintf(motivo, tam, "%s", g->nome); return 0; }
        conferidos++;
    }
    (void)conferidos;
    return v->reg[1].usar ? 1 : -1;   // reg[1] = dados do jogo (PFS inteira)
}
