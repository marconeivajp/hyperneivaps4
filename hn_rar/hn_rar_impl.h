#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
/* Leitura de RAR 1.5 a 5 com o libarchive (licenca BSD, veja hn_rar/COPYING).
   Entra por miniz.c porque a lista de objetos do build.bat nao pode mudar. */
#define PLATFORM_CONFIG_H "config_hn.h"

#include "archive_platform.h"
#include "archive_check_magic.c"
#include "archive_virtual.c"
#include "archive_util.c"
#include "archive_string.c"
#include "archive_string_sprintf.c"
#include "archive_rb.c"
#include "archive_entry.c"
#include "archive_entry_sparse.c"
#include "archive_entry_xattr.c"
#include "archive_entry_stat.c"
#include "archive_entry_copy_stat.c"
#include "archive_acl.c"
#include "archive_options.c"
#include "archive_read_set_options.c"
#include "archive_read.c"
#include "archive_ppmd7.c"
#include "archive_read_support_format_rar.c"
#include "archive_blake2s_ref.c"
#include "archive_blake2sp_ref.c"
#define parse_filter parse_filter_rar5
#include "archive_read_support_format_rar5.c"
#undef parse_filter

#include <stdio.h>

/* Usado so para nomes temporarios, que a leitura de RAR nao cria. */
int archive_random(void* destino, size_t tamanho) {
    unsigned char* p = (unsigned char*)destino; size_t i;
    for (i = 0; i < tamanho; i++) p[i] = (unsigned char)rand();
    return ARCHIVE_OK;
}

/* Interface pequena para explorar_utilitarios.cpp, no mesmo jeito do hn7z_*. */
struct HnRarS;
typedef struct { struct HnRarS* h; FILE* f; long long base; } HnVol;
typedef struct HnRarS { struct archive* a; HnVol* vols; int n; long long lido; long long total; char buf[65536]; } HnRar;

static la_ssize_t hnrar_cb_ler(struct archive* a, void* d, const void** out) {
    HnVol* v = (HnVol*)d; (void)a;
    size_t n = fread(v->h->buf, 1, sizeof(v->h->buf), v->f);
    v->h->lido += (long long)n; *out = v->h->buf;
    return (la_ssize_t)n;
}
static la_int64_t hnrar_cb_pular(struct archive* a, void* d, la_int64_t n) {
    HnVol* v = (HnVol*)d; (void)a;
    if (fseeko(v->f, (off_t)n, SEEK_CUR) != 0) return 0;
    v->h->lido += n;
    return n;
}
static la_int64_t hnrar_cb_buscar(struct archive* a, void* d, la_int64_t pos, int origem) {
    HnVol* v = (HnVol*)d; (void)a;
    if (fseeko(v->f, (off_t)pos, origem) != 0) return ARCHIVE_FATAL;
    long long agora = (long long)ftello(v->f);
    v->h->lido = v->base + agora;
    return agora;
}
static int hnrar_cb_trocar(struct archive* a, void* antigo, void* novo) {
    HnVol* v = (HnVol*)novo; (void)a; (void)antigo;
    if (v) { fseeko(v->f, 0, SEEK_SET); v->h->lido = v->base; }
    return ARCHIVE_OK;
}

void hnrar_fechar(void* p) {
    HnRar* h = (HnRar*)p;
    int i;
    if (!h) return;
    if (h->a) archive_read_free(h->a);
    for (i = 0; i < h->n; i++) if (h->vols[i].f) fclose(h->vols[i].f);
    free(h->vols);
    free(h);
}

/* lista: caminhos em blocos de "passo" bytes, na ordem dos volumes. */
void* hnrar_abrir_lista(const char* lista, int total, int passo) {
    HnRar* h = (HnRar*)calloc(1, sizeof(HnRar));
    int i;
    if (!h) return NULL;
    h->vols = (HnVol*)calloc((size_t)total, sizeof(HnVol));
    h->a = archive_read_new();
    if (!h->vols || !h->a) { hnrar_fechar(h); return NULL; }
    for (i = 0; i < total; i++) {
        FILE* f = fopen(lista + (size_t)i * passo, "rb");
        if (!f) { hnrar_fechar(h); return NULL; }
        h->vols[i].f = f; h->vols[i].h = h; h->vols[i].base = h->total; h->n = i + 1;
        fseeko(f, 0, SEEK_END); h->total += (long long)ftello(f); fseeko(f, 0, SEEK_SET);
    }
    archive_read_support_format_rar(h->a);
    archive_read_support_format_rar5(h->a);
    archive_read_set_read_callback(h->a, hnrar_cb_ler);
    archive_read_set_skip_callback(h->a, hnrar_cb_pular);
    archive_read_set_seek_callback(h->a, hnrar_cb_buscar);
    archive_read_set_switch_callback(h->a, hnrar_cb_trocar);
    archive_read_set_callback_data(h->a, &h->vols[0]);
    for (i = 1; i < total; i++) archive_read_add_callback_data(h->a, &h->vols[i], (unsigned int)i);
    if (archive_read_open1(h->a) != ARCHIVE_OK) { hnrar_fechar(h); return NULL; }
    return h;
}

void* hnrar_abrir(const char* caminho) { return hnrar_abrir_lista(caminho, 1, 0); }

/* 1 = achou item, 0 = acabou, -1 = erro. */
int hnrar_proximo(void* p, char* nome, int tamanhoNome, int* ehPasta) {
    HnRar* h = (HnRar*)p; struct archive_entry* e = NULL;
    int r = archive_read_next_header(h->a, &e);
    if (r == ARCHIVE_EOF) return 0;
    if (r < ARCHIVE_WARN) return -1;
    const char* n = archive_entry_pathname_utf8(e);
    if (!n) n = archive_entry_pathname(e);
    if (!n) return -1;
    snprintf(nome, (size_t)tamanhoNome, "%s", n);
    *ehPasta = archive_entry_filetype(e) == AE_IFDIR;
    return 1;
}

/* Bytes lidos do item atual; 0 no fim e negativo em erro. */
long hnrar_ler(void* p, void* destino, long tamanho) {
    return (long)archive_read_data(((HnRar*)p)->a, destino, (size_t)tamanho);
}

int hnrar_progresso(void* p) {
    HnRar* h = (HnRar*)p;
    return h->total > 0 ? (int)(h->lido * 100 / h->total) : 0;
}

const char* hnrar_erro(void* p) {
    const char* e = archive_error_string(((HnRar*)p)->a);
    return e ? e : "";
}

#pragma clang diagnostic pop
