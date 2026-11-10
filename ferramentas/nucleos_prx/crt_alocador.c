// Memória dos núcleos .prx: o módulo usa o malloc do app principal (heap grande da libc do PS4)
// em vez da malloc da musl estática, que pede páginas novas ao kernel e falhava em núcleos
// que alocam muito (snes9x, desmume). Linkado antes da libc.a, substitui malloc e família.
#include <stddef.h>
#include <string.h>

typedef struct {
    void* (*malloc)(size_t);
    void  (*free)(void*);
    void* (*calloc)(size_t, size_t);
    void* (*realloc)(void*, size_t);
    void* (*memalign)(size_t, size_t);
    void  (*registrar)(const char*, long);   // log da ponte; alocações grandes aparecem nele
} HnAlocador;

static HnAlocador alocador;

// Reserva para o que o módulo aloca antes da ponte entregar o alocador (construtores globais).
#define RESERVA_TAMANHO (4u * 1024u * 1024u)
static unsigned char reserva[RESERVA_TAMANHO] __attribute__((aligned(64)));
static size_t reservaUsada = 0;

static int naReserva(const void* p) {
    return (const unsigned char*)p >= reserva && (const unsigned char*)p < reserva + RESERVA_TAMANHO;
}

// Cada bloco da reserva guarda o tamanho 16 bytes antes, para o realloc saber quanto copiar.
static void* reservaAlocar(size_t tamanho, size_t alinhamento) {
    if (alinhamento < 16) alinhamento = 16;
    size_t inicio = (reservaUsada + 16 + alinhamento - 1) & ~(alinhamento - 1);
    if (inicio + tamanho > RESERVA_TAMANHO) return NULL;
    *(size_t*)(reserva + inicio - 16) = tamanho;
    reservaUsada = inicio + tamanho;
    return reserva + inicio;
}

__attribute__((visibility("default")))
void hn_definir_alocador(const HnAlocador* novo, size_t tamanhoEstrutura) {
    if (!novo) return;
    memset(&alocador, 0, sizeof(alocador));
    memcpy(&alocador, novo, tamanhoEstrutura < sizeof(alocador) ? tamanhoEstrutura : sizeof(alocador));
    extern int hn_module_start_rodou;
    extern int hn_rodar_construtores(void);
    if (alocador.registrar) alocador.registrar("module_start rodou", hn_module_start_rodou);
    // Os construtores rodam aqui, já com a memória do app.
    int total = hn_rodar_construtores();
    if (alocador.registrar) alocador.registrar("construtores rodados pela ponte", total);
}

// Registra alocações de 64 KB ou mais (antes e depois), para achar onde um núcleo cai.
static void registrarGrande(const char* etapa, size_t tamanho) {
    if (alocador.registrar && tamanho >= 65536) alocador.registrar(etapa, (long)tamanho);
}

void* malloc(size_t tamanho) {
    if (alocador.malloc) {
        registrarGrande("malloc", tamanho);
        void* p = alocador.malloc(tamanho ? tamanho : 1);
        registrarGrande(p ? "malloc ok" : "malloc NULL", tamanho);
        return p;
    }
    return reservaAlocar(tamanho, 16);
}

void free(void* p) {
    if (!p || naReserva(p)) return;
    if (alocador.free) alocador.free(p);
}

void* calloc(size_t n, size_t tamanho) {
    if (tamanho && n > (size_t)-1 / tamanho) return NULL;
    if (alocador.calloc) {
        registrarGrande("calloc", n * tamanho);
        void* p = alocador.calloc(n ? n : 1, tamanho ? tamanho : 1);
        registrarGrande(p ? "calloc ok" : "calloc NULL", n * tamanho);
        return p;
    }
    void* p = reservaAlocar(n * tamanho, 16);
    if (p) memset(p, 0, n * tamanho);
    return p;
}

void* realloc(void* p, size_t tamanho) {
    if (!p) return malloc(tamanho);
    if (naReserva(p)) {
        size_t antigo = *(size_t*)((unsigned char*)p - 16);
        void* novo = malloc(tamanho);
        if (novo) memcpy(novo, p, antigo < tamanho ? antigo : tamanho);
        return novo;
    }
    if (alocador.realloc) return alocador.realloc(p, tamanho);
    return NULL;
}

void* memalign(size_t alinhamento, size_t tamanho) {
    if (alocador.memalign) {
        registrarGrande("memalign", tamanho);
        void* p = alocador.memalign(alinhamento, tamanho ? tamanho : 1);
        registrarGrande(p ? "memalign ok" : "memalign NULL", tamanho);
        return p;
    }
    return reservaAlocar(tamanho, alinhamento);
}

void* aligned_alloc(size_t alinhamento, size_t tamanho) { return memalign(alinhamento, tamanho); }

int posix_memalign(void** saida, size_t alinhamento, size_t tamanho) {
    void* p = memalign(alinhamento, tamanho);
    if (!p) return 12; // ENOMEM
    *saida = p;
    return 0;
}

void* valloc(size_t tamanho) { return memalign(16384, tamanho); }
