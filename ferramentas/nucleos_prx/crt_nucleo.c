// Início de módulo para núcleos .prx do Hyper Neiva.
// Substitui o crtlib.o do OpenOrbis, que gera relocação dentro do código (TEXTREL)
// e percorre variáveis próprias em vez da tabela de construtores do lld.
#include <stddef.h>

typedef void (*funcao_init)(void);

// O lld define estes símbolos em volta da seção .init_array.
// Com visibilidade oculta e -fPIC, o acesso vira lea relativo ao RIP, sem relocação.
extern funcao_init __init_array_start[] __attribute__((visibility("hidden")));
extern funcao_init __init_array_end[] __attribute__((visibility("hidden")));

// Mesmos bytes do .data.sce_module_param do crtlib.o.
__attribute__((section(".data.sce_module_param"), used, aligned(8)))
static unsigned char parametroModulo[0x18] = {
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xbf, 0xf4, 0x13, 0x3c, 0x01, 0x00, 0x00, 0x00,
    0x51, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
};

__attribute__((visibility("hidden"))) void* __dso_handle = 0;
__attribute__((used)) void* _sceLibc = 0;

// O PS4 não chama module_start nem roda o .init_array de um .prx carregado pelo app:
// sem isto, os objetos globais do C++ (std::map, std::set...) ficavam zerados e o
// snes9x, o stella e o desmume caíam. A ponte chama hn_rodar_construtores pelo alocador.
int hn_module_start_rodou = 0;
static volatile int construtoresRodaram = 0;

// Roda os construtores globais uma vez só.
int hn_rodar_construtores(void) {
    if (construtoresRodaram) return 0;
    construtoresRodaram = 1;
    int total = 0;
    for (funcao_init* f = __init_array_start; f < __init_array_end; f++)
        if (*f) { (*f)(); total++; }
    return total;
}

__attribute__((visibility("hidden")))
int module_start(size_t argc, const void* argp) {
    (void)argc; (void)argp;
    hn_module_start_rodou = 1;
    hn_rodar_construtores();
    return 0;
}

__attribute__((visibility("hidden")))
int module_stop(size_t argc, const void* argp) {
    (void)argc; (void)argp;
    return 0;
}

void _init(void) {}
void _fini(void) {}

// A zstd (genesis_plus_gx, mednafen_saturn) declara estas funções de rastreamento como opcionais.
// O create-fself não aceita símbolo fraco sem definição, então ficam vazias aqui.
__attribute__((weak, visibility("hidden"))) unsigned long long ZSTD_trace_decompress_begin(const void* dctx) { (void)dctx; return 0; }
__attribute__((weak, visibility("hidden"))) void ZSTD_trace_decompress_end(unsigned long long ctx, const void* trace) { (void)ctx; (void)trace; }
