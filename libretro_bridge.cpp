#include "libretro_bridge.h"
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include "jailbreak.h"
#include "graphics.h"
#include "audio.h"

extern "C" {
    void retro_init(void);
    void retro_deinit(void);
    unsigned retro_api_version(void);
    void retro_get_system_info(struct retro_system_info *info);
    void retro_get_system_av_info(struct retro_system_av_info *info);
    void retro_set_environment(retro_environment_t cb);
    void retro_set_video_refresh(retro_video_refresh_t cb);
    void retro_set_audio_sample(retro_audio_sample_t cb);
    void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);
    void retro_set_input_poll(retro_input_poll_t cb);
    void retro_set_input_state(retro_input_state_t cb);
    void retro_set_controller_port_device(unsigned port, unsigned device);
    void retro_reset(void);
    void retro_run(void);
    size_t retro_serialize_size(void);
    bool retro_serialize(void *data, size_t size);
    bool retro_unserialize(const void *data, size_t size);
    void retro_cheat_reset(void);
    void retro_cheat_set(unsigned index, bool enabled, const char *code);
    bool retro_load_game(const struct retro_game_info *game);
    bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
    void retro_unload_game(void);
    unsigned retro_get_region(void);
    void *retro_get_memory_data(unsigned id);
    size_t retro_get_memory_size(unsigned id);
}

static void* core_handle = NULL;
static bool static_core_active = false;
// Núcleo em .prx separado, baixado fora do PKG. -1 quando não há módulo carregado.
static int modulo_nucleo = -1;

void (*retro_init_ptr)(void) = NULL;
void (*retro_deinit_ptr)(void) = NULL;
unsigned (*retro_api_version_ptr)(void) = NULL;
void (*retro_get_system_info_ptr)(struct retro_system_info *info) = NULL;
void (*retro_get_system_av_info_ptr)(struct retro_system_av_info *info) = NULL;
void (*retro_set_environment_ptr)(retro_environment_t) = NULL;
void (*retro_set_video_refresh_ptr)(retro_video_refresh_t) = NULL;
void (*retro_set_audio_sample_ptr)(retro_audio_sample_t) = NULL;
void (*retro_set_audio_sample_batch_ptr)(retro_audio_sample_batch_t) = NULL;
void (*retro_set_input_poll_ptr)(retro_input_poll_t) = NULL;
void (*retro_set_input_state_ptr)(retro_input_state_t) = NULL;
void (*retro_set_controller_port_device_ptr)(unsigned, unsigned) = NULL;
void (*retro_reset_ptr)(void) = NULL;
void (*retro_run_ptr)(void) = NULL;
size_t (*retro_serialize_size_ptr)(void) = NULL;
bool (*retro_serialize_ptr)(void*, size_t) = NULL;
bool (*retro_unserialize_ptr)(const void*, size_t) = NULL;
void (*retro_cheat_reset_ptr)(void) = NULL;
void (*retro_cheat_set_ptr)(unsigned, bool, const char*) = NULL;
bool (*retro_load_game_ptr)(const struct retro_game_info*) = NULL;
bool (*retro_load_game_special_ptr)(unsigned, const struct retro_game_info*, size_t) = NULL;
void (*retro_unload_game_ptr)(void) = NULL;
unsigned (*retro_get_region_ptr)(void) = NULL;
void* (*retro_get_memory_data_ptr)(unsigned) = NULL;
size_t (*retro_get_memory_size_ptr)(unsigned) = NULL;

extern int globalPadHandle;
int gEmuPixelFormat = 2; // Default para RGB565 por segurança

static bool bridge_pausado = false;
static void* romPendente = NULL;
static void* romEmUso = NULL;
static size_t romPendenteTamanho = 0;

// ROM lida de um compactado: fica na memória até o jogo fechar.
void bridge_rom_em_memoria(void* dados, size_t tamanho) {
    free(romPendente);
    romPendente = dados;
    romPendenteTamanho = tamanho;
}

static void registrarEmulador(const char* etapa, const char* caminho, size_t tamanho) {
    FILE* f = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (f) { fprintf(f, "EMULADOR %s %zu %s\n", etapa, tamanho, caminho); fclose(f); }
}
// Etapas do núcleo no log: quando o app cai, a última linha mostra onde foi.
static void etapaNucleo(const char* etapa, long valor) {
    FILE* f = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (f) { fprintf(f, "ETAPA %s %ld\n", etapa, valor); fclose(f); }
}
static uint8_t ambientePedido[256];
static uint64_t bridge_periodo_us = 16667, bridge_proximo_quadro = 0;
extern void definirTaxaAudioEmulador(double taxa);
void bridge_set_pausado(bool p) { bridge_pausado = p; }
bool bridge_esta_pausado() { return bridge_pausado; }

// Os núcleos escrevem log por aqui.
#ifndef RETRO_ENVIRONMENT_GET_LOG_INTERFACE
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
#endif
#ifndef RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31
#endif
// Mesma forma da struct retro_log_callback do libretro.h (o cabeçalho reduzido da ponte não a tem).
struct ponte_log_callback { void (*log)(int nivel, const char* formato, ...); };
// O log dos núcleos vai para nucleo_log.txt (no máximo 400 linhas por jogo, para não encher o disco).
static int linhasLogNucleo = 0;
static void retro_log_vazio(int nivel, const char* formato, ...) {
    if (!formato || linhasLogNucleo >= 400) return;
    linhasLogNucleo++;
    FILE* f = fopen("/data/HyperNeiva/nucleo_log.txt", "a");
    if (!f) return;
    fprintf(f, "[%d] ", nivel);
    va_list argumentos;
    va_start(argumentos, formato);
    vfprintf(f, formato, argumentos);
    va_end(argumentos);
    fclose(f);
}

static bool retro_environment_cb(unsigned cmd, void *data) {
    // Cada comando pedido pelo núcleo aparece uma vez no log.
    unsigned numero = cmd & 0xFFFF;
    if (numero < 256 && !ambientePedido[numero]) { ambientePedido[numero] = 1; etapaNucleo("ambiente", (long)cmd); }
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format fmt = *(enum retro_pixel_format*)data;
            gEmuPixelFormat = (int)fmt;
            return true; // Aceitamos qualquer formato que o núcleo sugerir
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            const char **dir = (const char**)data;
            *dir = "/data/retroarch/system";
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            bool *b = (bool*)data;
            *b = true;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            mkdir("/data/retroarch/saves", 0777);
            *(const char**)data = "/data/retroarch/saves";
            return true;
        }
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            ((struct ponte_log_callback*)data)->log = retro_log_vazio;
            return true;
        }
    }
    return false;
}

static void retro_video_refresh_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (data && !bridge_pausado) desenharBufferEmulador(data, width, height, pitch);
}

static void retro_audio_sample_cb(int16_t left, int16_t right) {
    enviarAmostraAudio(left, right);
}

static size_t retro_audio_sample_batch_cb(const int16_t *data, size_t frames) {
    for (size_t i = 0; i < frames; i++) enviarAmostraAudio(data[i * 2], data[i * 2 + 1]);
    return frames;
}

static void retro_input_poll_cb(void) {}

uint32_t gEmuPadButtons = 0; 
uint8_t gEmuAnaLX=128, gEmuAnaLY=128, gEmuAnaRX=128, gEmuAnaRY=128; // Marcadores V54
int gba_to_ps4_map[16][2] = {
    // A posição é o número do botão no libretro: 0 é B, 8 é A.
    { ORBIS_PAD_BUTTON_SQUARE, ORBIS_PAD_BUTTON_TRIANGLE },  // B
    { 0, 0 },                                                // Y (não existe no GBA)
    { ORBIS_PAD_BUTTON_OPTIONS, 0 },  // SELECT
    { ORBIS_PAD_BUTTON_TOUCH_PAD, 0 },// START
    { ORBIS_PAD_BUTTON_UP, PS4_ANALOG_L_UP },       // UP
    { ORBIS_PAD_BUTTON_DOWN, PS4_ANALOG_L_DOWN },   // DOWN
    { ORBIS_PAD_BUTTON_LEFT, PS4_ANALOG_L_LEFT },   // LEFT
    { ORBIS_PAD_BUTTON_RIGHT, PS4_ANALOG_L_RIGHT }, // RIGHT
    { ORBIS_PAD_BUTTON_CROSS, ORBIS_PAD_BUTTON_CIRCLE },     // A
    { 0, 0 },                                                // X (não existe no GBA)
    { ORBIS_PAD_BUTTON_L1, 0 },       // L
    { ORBIS_PAD_BUTTON_R1, 0 },       // R
    { ORBIS_PAD_BUTTON_L2, 0 },       // L2
    { ORBIS_PAD_BUTTON_R2, 0 },       // R2
    { 0, 0 }, { 0, 0 }
};

// Layout do RetroArch no PS4 para os outros sistemas (SNES, Mega Drive, Saturn...):
// X = B, Bolinha = A, Quadrado = Y, Triângulo = X. O mapa acima, configurável, fica para o GBA.
static const int mapaPadraoRetro[16][2] = {
    { ORBIS_PAD_BUTTON_CROSS, 0 },    // B
    { ORBIS_PAD_BUTTON_SQUARE, 0 },   // Y
    { ORBIS_PAD_BUTTON_OPTIONS, 0 },  // SELECT
    { ORBIS_PAD_BUTTON_TOUCH_PAD, 0 },// START
    { ORBIS_PAD_BUTTON_UP, PS4_ANALOG_L_UP },
    { ORBIS_PAD_BUTTON_DOWN, PS4_ANALOG_L_DOWN },
    { ORBIS_PAD_BUTTON_LEFT, PS4_ANALOG_L_LEFT },
    { ORBIS_PAD_BUTTON_RIGHT, PS4_ANALOG_L_RIGHT },
    { ORBIS_PAD_BUTTON_CIRCLE, 0 },   // A
    { ORBIS_PAD_BUTTON_TRIANGLE, 0 }, // X
    { ORBIS_PAD_BUTTON_L1, 0 },       // L
    { ORBIS_PAD_BUTTON_R1, 0 },       // R
    { ORBIS_PAD_BUTTON_L2, 0 },       // L2
    { ORBIS_PAD_BUTTON_R2, 0 },       // R2
    { ORBIS_PAD_BUTTON_L3, 0 },       // L3
    { ORBIS_PAD_BUTTON_R3, 0 },       // R3
};
static bool usarMapaPadrao = false;

static int16_t retro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    // Analógicos (N64 e outros): índice 0 é o esquerdo, 1 o direito; id 0 é X, 1 é Y.
    if (port == 0 && device == RETRO_DEVICE_ANALOG && index < 2 && id < 2) {
        uint8_t bruto = index == 0 ? (id == 0 ? gEmuAnaLX : gEmuAnaLY) : (id == 0 ? gEmuAnaRX : gEmuAnaRY);
        int valor = ((int)bruto - 128) * 256;
        if (valor > -3000 && valor < 3000) valor = 0; // zona morta
        if (valor > 32767) valor = 32767;
        return (int16_t)valor;
    }
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;

    // V33: Agora usamos o buffer global preenchido uma unica vez por frame
    unsigned int btn = gEmuPadButtons;
    if (id < 16) {
        for (int m = 0; m < 2; m++) { // V55: Suporte a mapeamento duplo
            int target = usarMapaPadrao ? mapaPadraoRetro[id][m] : gba_to_ps4_map[id][m];
            if (target == 0) continue;
            
            // V54: Checagem de Analógicos
            if (target & 0xF0000) {
                bool act = false;
                if (target == PS4_ANALOG_L_UP)    act = (gEmuAnaLY < 50);
                else if (target == PS4_ANALOG_L_DOWN)  act = (gEmuAnaLY > 200);
                else if (target == PS4_ANALOG_L_LEFT)  act = (gEmuAnaLX < 50);
                else if (target == PS4_ANALOG_L_RIGHT) act = (gEmuAnaLX > 200);
                else if (target == PS4_ANALOG_R_UP)    act = (gEmuAnaRY < 50);
                else if (target == PS4_ANALOG_R_DOWN)  act = (gEmuAnaRY > 200);
                else if (target == PS4_ANALOG_R_LEFT)  act = (gEmuAnaRX < 50);
                else if (target == PS4_ANALOG_R_RIGHT) act = (gEmuAnaRX > 200);
                if (act) return 1;
            } else {
                if (btn & target) return 1;
            }
        }
    }
    return 0;
}

extern char msgStatus[128];
extern int msgTimer;
#include "explorar.h"
extern void parar_bridge(const char* msg);
#include "stb_image.h"

extern unsigned char* backImg;
extern unsigned char* imgMidia;
extern unsigned char* imgPaginaAtual;

void liberarMemoriaParaEmulador() {
    if (backImg) { stbi_image_free(backImg); backImg = NULL; }
    if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
    if (imgPaginaAtual) { stbi_image_free(imgPaginaAtual); imgPaginaAtual = NULL; }
}

extern void desligarInterfaceGrafica();
extern void ligarInterfaceGrafica();
extern void liberarSpriteSheetParaEmu(); 
extern void limpezaProfundaRAM();

// Núcleos separados, baixados fora do PKG, ficam em /data/HyperNeiva/nucleos/<nome>_libretro.prx.
// A extensão escolhe o núcleo; quando ela serve a vários sistemas, a pasta decide.
const char* bridge_nucleo_para_rom(const char* rom) {
    const char* ponto = strrchr(rom, '.');
    if (!ponto || strlen(ponto) > 8) return NULL;
    char ext[12], caminho[600];
    snprintf(ext, sizeof(ext), " %s ", ponto);
    for (int i = 0; ext[i]; i++) ext[i] = (char)tolower((unsigned char)ext[i]);
    snprintf(caminho, sizeof(caminho), "%s", rom);
    for (int i = 0; caminho[i]; i++) caminho[i] = (char)tolower((unsigned char)caminho[i]);
    bool saturn = strstr(caminho, "saturn") != NULL;
    bool atari = strstr(caminho, "atari") != NULL;
    bool ps1 = strstr(caminho, "/ps1") || strstr(caminho, "playstation");
    if (strstr(" .gba .gb .gbc ", ext)) return "mgba";
    if (strstr(" .n64 .z64 .v64 ", ext)) return "parallel_n64";
    if (strstr(" .nes .fds .unf .unif ", ext)) return "fceumm";
    if (strstr(" .sfc .smc .fig .swc ", ext)) return "snes9x";
    if (strstr(" .nds ", ext)) return "desmume2015";
    if (strstr(" .a26 ", ext)) return "stella2014";
    if (strstr(" .md .gen .smd .sms .gg .sg ", ext)) return "genesis_plus_gx";
    if (ps1 && strstr(" .cue .chd .pbp .iso .bin .m3u .img ", ext)) return "pcsx_rearmed";
    // Saturn: Yabause (leve, vídeo em software). O mednafen_saturn é pesado demais para o PS4.
    if (strstr(" .cue .chd .ccd .iso .mds .m3u ", ext)) return saturn ? "yabause" : NULL;
    if (strstr(" .bin ", ext)) {
        if (atari) return "stella2014";
        if (saturn) return NULL;
        return "genesis_plus_gx";
    }
    return NULL;
}

static void registrarNucleo(const char* formato, const char* texto, int valor) {
    FILE* f = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (!f) return;
    fprintf(f, formato, texto, valor);
    fputc('\n', f);
    fclose(f);
}

static void* alocarAlinhadoNucleo(size_t alinhamento, size_t tamanho) {
    void* p = NULL;
    if (alinhamento < sizeof(void*)) alinhamento = sizeof(void*);
    return posix_memalign(&p, alinhamento, tamanho) == 0 ? p : NULL;
}

// Registrador de crash: grava sinal, endereço acessado e instrução (RIP) antes do app fechar.
// O sigaction vem do libkernel, com a estrutura do FreeBSD (não a da musl dos cabeçalhos).
struct HnSigactionFreeBsd { void (*acao)(int, void*, void*); int flags; uint32_t mascara[4]; };
extern "C" int hnSigaction(int, const void*, void*) __asm__("sigaction");
static uint64_t hnBaseNucleo = 0;
static void hnRegistrarCrash(int sinal, void* info, void* contexto) {
    // FreeBSD amd64: siginfo.si_addr fica em +24; ucontext.uc_mcontext em +16, e nele mc_rip é o 21º campo de 8 bytes.
    uint64_t endereco = info ? *(uint64_t*)((char*)info + 24) : 0;
    uint64_t rip = contexto ? *(uint64_t*)((char*)contexto + 16 + 20 * 8) : 0;
    uint64_t rsp = contexto ? *(uint64_t*)((char*)contexto + 16 + 23 * 8) : 0;
    char texto[256];
    int n = snprintf(texto, sizeof(texto), "CRASH sinal=%d endereco=%llx rip=%llx rsp=%llx base_nucleo=%llx rip_no_nucleo=%llx\n",
                     sinal, (unsigned long long)endereco, (unsigned long long)rip, (unsigned long long)rsp,
                     (unsigned long long)hnBaseNucleo,
                     (unsigned long long)(hnBaseNucleo && rip >= hnBaseNucleo ? rip - hnBaseNucleo : 0));
    int fd = open("/data/HyperNeiva/zip_debug_log.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd >= 0) { write(fd, texto, (size_t)n); close(fd); }
    HnSigactionFreeBsd padrao;
    memset(&padrao, 0, sizeof(padrao));
    hnSigaction(sinal, &padrao, NULL);   // volta ao padrão e deixa o sistema fechar o app
}
static void hnInstalarRegistradorCrash() {
    static bool instalado = false;
    if (instalado) return;
    instalado = true;
    HnSigactionFreeBsd acao;
    memset(&acao, 0, sizeof(acao));
    acao.acao = hnRegistrarCrash;
    acao.flags = 0x40;                 // SA_SIGINFO
    const int sinais[] = { 4, 8, 10, 11 }; // SIGILL, SIGFPE, SIGBUS, SIGSEGV
    for (int i = 0; i < 4; i++) hnSigaction(sinais[i], &acao, NULL);
}

// Carrega o núcleo de um .prx e pega cada função pelo nome. Devolve false se faltar algo.
// Núcleos com threads próprias (Yabause) ficam carregados depois do primeiro uso: descarregar
// o módulo com as threads dele dormindo deixaria código apagado sob elas.
static int moduloPersistente = -1;
static char caminhoPersistente[200] = "";

static bool carregarNucleoPrx(const char* caminho) {
    etapaNucleo(caminho, 0);
    if (moduloPersistente >= 0 && strcmp(caminhoPersistente, caminho) == 0) {
        modulo_nucleo = moduloPersistente;
        etapaNucleo("modulo reaproveitado", modulo_nucleo);
    }
    else modulo_nucleo = sceKernelLoadStartModule(caminho, 0, NULL, 0, NULL, NULL);
    if (modulo_nucleo >= 0 && moduloPersistente < 0 && strstr(caminho, "yabause")) {
        moduloPersistente = modulo_nucleo;
        snprintf(caminhoPersistente, sizeof(caminhoPersistente), "%s", caminho);
    }
    registrarNucleo("NUCLEO %s modulo=0x%08X", caminho, modulo_nucleo);
    if (modulo_nucleo < 0) { modulo_nucleo = -1; return false; }
    #define PEGAR_FUNCAO(n) \
        if (sceKernelDlsym(modulo_nucleo, #n, (void**)&n##_ptr) != 0 || !n##_ptr) { \
            registrarNucleo("NUCLEO sem a funcao %s (%d)", #n, 0); \
            sceKernelStopUnloadModule(modulo_nucleo, 0, NULL, 0, NULL, NULL); \
            modulo_nucleo = -1; \
            return false; \
        }
    PEGAR_FUNCAO(retro_init); PEGAR_FUNCAO(retro_deinit); PEGAR_FUNCAO(retro_api_version);
    PEGAR_FUNCAO(retro_get_system_info); PEGAR_FUNCAO(retro_get_system_av_info); PEGAR_FUNCAO(retro_set_environment);
    PEGAR_FUNCAO(retro_set_video_refresh); PEGAR_FUNCAO(retro_set_audio_sample); PEGAR_FUNCAO(retro_set_audio_sample_batch);
    PEGAR_FUNCAO(retro_set_input_poll); PEGAR_FUNCAO(retro_set_input_state); PEGAR_FUNCAO(retro_set_controller_port_device);
    PEGAR_FUNCAO(retro_reset); PEGAR_FUNCAO(retro_run); PEGAR_FUNCAO(retro_serialize_size); PEGAR_FUNCAO(retro_serialize);
    PEGAR_FUNCAO(retro_unserialize); PEGAR_FUNCAO(retro_cheat_reset); PEGAR_FUNCAO(retro_cheat_set);
    PEGAR_FUNCAO(retro_load_game); PEGAR_FUNCAO(retro_unload_game); PEGAR_FUNCAO(retro_get_region);
    PEGAR_FUNCAO(retro_get_memory_data); PEGAR_FUNCAO(retro_get_memory_size);
    #undef PEGAR_FUNCAO
    // O núcleo usa a memória do app (heap grande da libc do PS4), não páginas novas do kernel.
    struct AlocadorNucleo {
        void* (*malloc)(size_t);
        void  (*free)(void*);
        void* (*calloc)(size_t, size_t);
        void* (*realloc)(void*, size_t);
        void* (*memalign)(size_t, size_t);
        void  (*registrar)(const char*, long);
    };
    void (*definirAlocador)(const AlocadorNucleo*, size_t) = NULL;
    if (sceKernelDlsym(modulo_nucleo, "hn_definir_alocador", (void**)&definirAlocador) == 0 && definirAlocador) {
        AlocadorNucleo alocador = { malloc, free, calloc, realloc, alocarAlinhadoNucleo, etapaNucleo };
        definirAlocador(&alocador, sizeof(alocador));
        registrarNucleo("NUCLEO %s usa a memoria do app (%d)", caminho, 1);
    }
    registrarNucleo("NUCLEO %s carregado (%d funcoes)", caminho, 24);
    {
        FILE* f = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
        if (f) { fprintf(f, "NUCLEO retro_run=%llx retro_init=%llx\n", (unsigned long long)(uintptr_t)retro_run_ptr, (unsigned long long)(uintptr_t)retro_init_ptr); fclose(f); }
    }
    hnInstalarRegistradorCrash();
    return true;
}

bool bridge_iniciar(const char* caminho_core, const char* caminho_rom) {
    limpezaProfundaRAM();
    snprintf(msgStatus, sizeof(msgStatus), "Limpando RAM e Correndo..."); msgTimer = 120;
    bridge_finalizar(); 
    liberarMemoriaParaEmulador();
    liberarSpriteSheetParaEmu(); 
    desligarInterfaceGrafica();  

    struct jbc_cred cred;
    jbc_get_cred(&cred);
    jbc_jailbreak_cred(&cred);
    jbc_set_cred(&cred);

    core_handle = NULL;
    static_core_active = false;

    bool ehGBA = false;
    const char* ext = strrchr(caminho_rom, '.');
    if (ext && (strcasecmp(ext, ".gba") == 0 || strcasecmp(ext, ".gb") == 0 || strcasecmp(ext, ".gbc") == 0)) {
        ehGBA = true;
    }

    // Núcleo separado: com o .prx no PS4, ele é usado. O GBA ainda tem o embutido como reserva.
    const char* nucleo = bridge_nucleo_para_rom(caminho_rom);
    char caminhoPrx[200] = "";
    if (nucleo) snprintf(caminhoPrx, sizeof(caminhoPrx), "/data/HyperNeiva/nucleos/%s_libretro.prx", nucleo);
    usarMapaPadrao = !ehGBA;
    struct stat infoPrx;
    if (nucleo && stat(caminhoPrx, &infoPrx) == 0 && carregarNucleoPrx(caminhoPrx)) {
        printf("[BRIDGE] Usando nucleo %s\n", caminhoPrx);
        snprintf(msgStatus, sizeof(msgStatus), "NUCLEO: %s", nucleo);
        msgTimer = 240;
    }
    else if (!ehGBA && nucleo && (!caminho_core || !caminho_core[0])) {
        char erro[200];
        snprintf(erro, sizeof(erro), "FALTA O NUCLEO %s_libretro.prx EM /data/HyperNeiva/nucleos", nucleo);
        registrarNucleo("NUCLEO %s ausente (%d)", caminhoPrx, 0);
        parar_bridge(erro);
        return false;
    }
    else if (ehGBA) {
        printf("[BRIDGE] Usando modo ESTATICO para GBA\n");
        retro_init_ptr = retro_init;
        retro_deinit_ptr = retro_deinit;
        retro_api_version_ptr = retro_api_version;
        retro_get_system_info_ptr = retro_get_system_info;
        retro_get_system_av_info_ptr = retro_get_system_av_info;
        retro_set_environment_ptr = retro_set_environment;
        retro_set_video_refresh_ptr = retro_set_video_refresh;
        retro_set_audio_sample_ptr = retro_set_audio_sample;
        retro_set_audio_sample_batch_ptr = retro_set_audio_sample_batch;
        retro_set_input_poll_ptr = retro_set_input_poll;
        retro_set_input_state_ptr = retro_set_input_state;
        retro_set_controller_port_device_ptr = retro_set_controller_port_device;
        retro_reset_ptr = retro_reset;
        retro_run_ptr = retro_run;
        retro_serialize_size_ptr = retro_serialize_size;
        retro_serialize_ptr = retro_serialize;
        retro_unserialize_ptr = retro_unserialize;
        retro_cheat_reset_ptr = retro_cheat_reset;
        retro_cheat_set_ptr = retro_cheat_set;
        retro_load_game_ptr = retro_load_game;
        retro_unload_game_ptr = retro_unload_game;
        retro_get_region_ptr = retro_get_region;
        retro_get_memory_data_ptr = retro_get_memory_data;
        retro_get_memory_size_ptr = retro_get_memory_size;
        static_core_active = true;
    } else {
        printf("[BRIDGE] Usando modo DINAMICO para: %s\n", caminho_core);
        core_handle = dlopen(caminho_core, RTLD_LAZY);
        if (!core_handle) {
            char erro[256];
            snprintf(erro, sizeof(erro), "ERRO DLOPEN: %s", dlerror());
            parar_bridge(erro);
            return false;
        }

        #define CARREGAR_SIMBOLO(n) \
            n##_ptr = (decltype(n##_ptr))dlsym(core_handle, #n); \
            if (!n##_ptr) { \
                char erro[128]; \
                snprintf(erro, sizeof(erro), "SIMBOLO %s NAO ACHADO", #n); \
                parar_bridge(erro); \
                return false; \
            }

        CARREGAR_SIMBOLO(retro_init);
        CARREGAR_SIMBOLO(retro_deinit);
        CARREGAR_SIMBOLO(retro_api_version);
        CARREGAR_SIMBOLO(retro_get_system_info);
        CARREGAR_SIMBOLO(retro_get_system_av_info);
        CARREGAR_SIMBOLO(retro_set_environment);
        CARREGAR_SIMBOLO(retro_set_video_refresh);
        CARREGAR_SIMBOLO(retro_set_audio_sample);
        CARREGAR_SIMBOLO(retro_set_audio_sample_batch);
        CARREGAR_SIMBOLO(retro_set_input_poll);
        CARREGAR_SIMBOLO(retro_set_input_state);
        CARREGAR_SIMBOLO(retro_set_controller_port_device);
        CARREGAR_SIMBOLO(retro_reset);
        CARREGAR_SIMBOLO(retro_run);
        CARREGAR_SIMBOLO(retro_serialize_size);
        CARREGAR_SIMBOLO(retro_serialize);
        CARREGAR_SIMBOLO(retro_unserialize);
        CARREGAR_SIMBOLO(retro_cheat_reset);
        CARREGAR_SIMBOLO(retro_cheat_set);
        CARREGAR_SIMBOLO(retro_load_game);
        CARREGAR_SIMBOLO(retro_unload_game);
        CARREGAR_SIMBOLO(retro_get_region);
        CARREGAR_SIMBOLO(retro_get_memory_data);
        CARREGAR_SIMBOLO(retro_get_memory_size);
    }

    memset(ambientePedido, 0, sizeof(ambientePedido));
    linhasLogNucleo = 0;
    gEmuPixelFormat = 0; // padrão do libretro (0RGB1555) até o núcleo pedir outro
    etapaNucleo("set_environment", 0);
    retro_set_environment_ptr(retro_environment_cb);
    etapaNucleo("set_callbacks", 0);
    retro_set_video_refresh_ptr(retro_video_refresh_cb);
    retro_set_audio_sample_ptr(retro_audio_sample_cb);
    retro_set_audio_sample_batch_ptr(retro_audio_sample_batch_cb);
    retro_set_input_poll_ptr(retro_input_poll_cb);
    retro_set_input_state_ptr(retro_input_state_cb);

    etapaNucleo("retro_init", 0);
    retro_init_ptr();
    etapaNucleo("retro_init ok", 0);
    
    romEmUso = romPendente;
    size_t tamanhoRom = romPendenteTamanho;
    romPendente = NULL;
    romPendenteTamanho = 0;
    // Núcleos sem need_fullpath (stella, fceumm...) esperam os bytes da ROM, não só o caminho.
    struct retro_system_info infoSistema;
    memset(&infoSistema, 0, sizeof(infoSistema));
    retro_get_system_info_ptr(&infoSistema);
    etapaNucleo("need_fullpath", infoSistema.need_fullpath ? 1 : 0);
    // Núcleos com need_fullpath (genesis_plus_gx, desmume, saturn) só abrem pelo caminho:
    // a ROM extraída do compactado vai para um arquivo temporário.
    static char caminhoTemporario[600];
    if (romEmUso && infoSistema.need_fullpath) {
        const char* base = strrchr(caminho_rom, '/');
        mkdir("/data/HyperNeiva/rom_temp", 0777);
        snprintf(caminhoTemporario, sizeof(caminhoTemporario), "/data/HyperNeiva/rom_temp/%s", base ? base + 1 : caminho_rom);
        FILE* temporario = fopen(caminhoTemporario, "wb");
        bool gravou = temporario && fwrite(romEmUso, 1, tamanhoRom, temporario) == tamanhoRom;
        if (temporario) fclose(temporario);
        registrarEmulador(gravou ? "temporario" : "temporario falhou", caminhoTemporario, tamanhoRom);
        if (gravou) caminho_rom = caminhoTemporario;
    }
    if (!romEmUso && !infoSistema.need_fullpath) {
        FILE* arquivoRom = fopen(caminho_rom, "rb");
        if (arquivoRom) {
            fseek(arquivoRom, 0, SEEK_END);
            long tamanho = ftell(arquivoRom);
            fseek(arquivoRom, 0, SEEK_SET);
            if (tamanho > 0 && tamanho <= 128L * 1024 * 1024) {
                romEmUso = malloc((size_t)tamanho);
                if (romEmUso && fread(romEmUso, 1, (size_t)tamanho, arquivoRom) == (size_t)tamanho) tamanhoRom = (size_t)tamanho;
                else { free(romEmUso); romEmUso = NULL; }
            }
            fclose(arquivoRom);
        }
    }
    registrarEmulador("carregando", caminho_rom, tamanhoRom);
    struct retro_game_info game = { caminho_rom, romEmUso, tamanhoRom, NULL };
    if (!retro_load_game_ptr(&game)) {
        parar_bridge("ERRO: O MOTOR RECUSOU O JOGO!");
        return false;
    }
    registrarEmulador("carregado", caminho_rom, tamanhoRom);
    // Declara um controle padrão na porta 1 (o N64 ficava "sem nada plugado" sem isto).
    if (retro_set_controller_port_device_ptr) retro_set_controller_port_device_ptr(0, RETRO_DEVICE_JOYPAD);

    struct retro_system_av_info info;
    memset(&info, 0, sizeof(info));
    retro_get_system_av_info_ptr(&info);
    etapaNucleo("largura", (long)info.geometry.base_width);
    etapaNucleo("altura", (long)info.geometry.base_height);
    etapaNucleo("audio", (long)info.sample_rate);
    bridge_periodo_us = (uint64_t)(1000000.0 / (info.fps > 1.0 ? info.fps : 60.0));
    bridge_proximo_quadro = 0;
    definirTaxaAudioEmulador(info.sample_rate);

    snprintf(msgStatus, sizeof(msgStatus), "Emulador Pronto!"); msgTimer = 120;
    return true;
}

static uint32_t bridge_frame_count = 0;

// Contador de FPS e velocidade, ligado no menu de pausa.
bool gMostrarFps = false;
static void desenharContadorFps() {
    static uint64_t inicioSegundo = 0;
    static int quadrosNoSegundo = 0, fpsMedido = 0;
    uint64_t agora = sceKernelGetProcessTime();
    if (inicioSegundo == 0) inicioSegundo = agora;
    quadrosNoSegundo++;
    if (agora - inicioSegundo >= 1000000) {
        fpsMedido = (int)((uint64_t)quadrosNoSegundo * 1000000 / (agora - inicioSegundo));
        quadrosNoSegundo = 0;
        inicioSegundo = agora;
    }
    if (!gMostrarFps) return;
    uint32_t* tela = obterBufferVideo();
    if (!tela) return;
    int fpsConsole = bridge_periodo_us ? (int)((1000000 + bridge_periodo_us / 2) / bridge_periodo_us) : 60;
    int velocidade = fpsConsole ? fpsMedido * 100 / fpsConsole : 0;
    char texto[80];
    snprintf(texto, sizeof(texto), "FPS %d / %d   VELOCIDADE %d%%", fpsMedido, fpsConsole, velocidade);
    for (int y = 12; y < 62; y++) for (int x = 12; x < 620; x++) tela[y * 1920 + x] = 0xFF000000;
    desenharTexto(tela, texto, 30, 24, 20, velocidade >= 95 ? 0xFF00FF00 : (velocidade >= 60 ? 0xFFFFFF00 : 0xFFFF4040));
}

void bridge_atualizar() {
    if (!retro_run_ptr) return;
    // O núcleo roda o mais rápido que puder: quem marca a velocidade do jogo é este relógio.
    uint64_t agora = sceKernelGetProcessTime();
    if (bridge_proximo_quadro > agora) {
        sceKernelUsleep((unsigned int)(bridge_proximo_quadro - agora));
        bridge_proximo_quadro += bridge_periodo_us;
    }
    else bridge_proximo_quadro = agora + bridge_periodo_us;
    bridge_frame_count++;
    if (bridge_frame_count % 60 == 0) {
        if (msgTimer < 2) {
            snprintf(msgStatus, sizeof(msgStatus), "EMULADOR EM EXECUCAO [F:%u]", bridge_frame_count);
            msgTimer = 2; 
        }
    }
    if (bridge_frame_count <= 2) etapaNucleo("retro_run", (long)bridge_frame_count);
    retro_run_ptr();
    if (bridge_frame_count <= 2) etapaNucleo("retro_run ok", (long)bridge_frame_count);
    desenharContadorFps();
}

void bridge_finalizar() {
    // Fechar o jogo e abrir outro chamam esta função duas vezes seguidas.
    // Zerar os ponteiros impede encerrar o núcleo de novo (deinit duplo travava com CE-34878-0).
    if (retro_unload_game_ptr) retro_unload_game_ptr();
    if (retro_deinit_ptr) retro_deinit_ptr();
    retro_unload_game_ptr = NULL;
    retro_deinit_ptr = NULL;
    if (core_handle) {
        dlclose(core_handle);
        core_handle = NULL;
    }
    if (modulo_nucleo >= 0) {
        if (modulo_nucleo != moduloPersistente) sceKernelStopUnloadModule(modulo_nucleo, 0, NULL, 0, NULL, NULL);
        modulo_nucleo = -1;
    }
    static_core_active = false;
    retro_run_ptr = NULL;
    free(romEmUso);
    romEmUso = NULL;
}

// V33: PERSISTENCIA (SAVES)
bool bridge_salvar_state(const char* path) {
    if (!retro_serialize_ptr || !retro_serialize_size_ptr) return false;
    size_t sz = retro_serialize_size_ptr();
    if (sz == 0) return false;
    void* data = malloc(sz);
    if (!data) return false;
    bool ok = retro_serialize_ptr(data, sz);
    if (ok) {
        FILE* f = fopen(path, "wb");
        if (f) { fwrite(data, 1, sz, f); fclose(f); }
        else ok = false;
    }
    free(data);
    return ok;
}

bool bridge_carregar_state(const char* path) {
    if (!retro_unserialize_ptr || !retro_serialize_size_ptr) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); size_t szFile = ftell(f); fseek(f, 0, SEEK_SET);
    size_t szCore = retro_serialize_size_ptr();
    if (szFile != szCore) { fclose(f); return false; }
    void* data = malloc(szCore);
    if (!data) { fclose(f); return false; }
    fread(data, 1, szCore, f); fclose(f);
    bool ok = retro_unserialize_ptr(data, szCore);
    free(data);
    return ok;
}

void parar_bridge(const char* msg) {
    extern uint32_t msgStatusColor;
    msgStatusColor = 0xFFFF0000;
    snprintf(msgStatus, sizeof(msgStatus), "%s", msg);
    msgTimer = 999999;
    menuAtual = MENU_ERRO_CRITICO;
    bridge_finalizar();
    ligarInterfaceGrafica();
}
