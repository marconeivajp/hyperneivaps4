#include "libretro_bridge.h"
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
void bridge_set_pausado(bool p) { bridge_pausado = p; }
bool bridge_esta_pausado() { return bridge_pausado; }

static bool retro_environment_cb(unsigned cmd, void *data) {
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
    { ORBIS_PAD_BUTTON_CROSS, 0 },    // B
    { ORBIS_PAD_BUTTON_CIRCLE, 0 },   // A
    { ORBIS_PAD_BUTTON_OPTIONS, 0 },  // SELECT
    { ORBIS_PAD_BUTTON_TOUCH_PAD, 0 },// START
    { ORBIS_PAD_BUTTON_UP, 0 },       // UP
    { ORBIS_PAD_BUTTON_DOWN, 0 },     // DOWN
    { ORBIS_PAD_BUTTON_LEFT, 0 },     // LEFT
    { ORBIS_PAD_BUTTON_RIGHT, 0 },    // RIGHT
    { ORBIS_PAD_BUTTON_SQUARE, 0 },   // Y (Turbo)
    { ORBIS_PAD_BUTTON_TRIANGLE, 0 }, // X (Turbo)
    { ORBIS_PAD_BUTTON_L1, 0 },       // L
    { ORBIS_PAD_BUTTON_R1, 0 },       // R
    { ORBIS_PAD_BUTTON_L2, 0 },       // L2
    { ORBIS_PAD_BUTTON_R2, 0 },       // R2
    { 0, 0 }, { 0, 0 }
};

static int16_t retro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;
    
    // V33: Agora usamos o buffer global preenchido uma unica vez por frame
    unsigned int btn = gEmuPadButtons;
    if (id < 16) {
        for (int m = 0; m < 2; m++) { // V55: Suporte a mapeamento duplo
            int target = gba_to_ps4_map[id][m];
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
    if (ext && (strcasecmp(ext, ".gba") == 0)) {
        ehGBA = true;
    }

    if (ehGBA) {
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

    retro_set_environment_ptr(retro_environment_cb);
    retro_set_video_refresh_ptr(retro_video_refresh_cb);
    retro_set_audio_sample_ptr(retro_audio_sample_cb);
    retro_set_audio_sample_batch_ptr(retro_audio_sample_batch_cb);
    retro_set_input_poll_ptr(retro_input_poll_cb);
    retro_set_input_state_ptr(retro_input_state_cb);

    retro_init_ptr();
    
    struct retro_game_info game = { caminho_rom, NULL, 0, NULL };
    if (!retro_load_game_ptr(&game)) {
        parar_bridge("ERRO: O MOTOR RECUSOU O JOGO!");
        return false;
    }

    snprintf(msgStatus, sizeof(msgStatus), "Emulador Pronto!"); msgTimer = 120;
    return true;
}

static uint32_t bridge_frame_count = 0;
void bridge_atualizar() {
    if (!retro_run_ptr) return;
    bridge_frame_count++;
    if (bridge_frame_count % 60 == 0) {
        if (msgTimer < 2) {
            snprintf(msgStatus, sizeof(msgStatus), "EMULADOR EM EXECUCAO [F:%u]", bridge_frame_count);
            msgTimer = 2; 
        }
    }
    retro_run_ptr();
}

void bridge_finalizar() {
    if (retro_unload_game_ptr) retro_unload_game_ptr();
    if (retro_deinit_ptr) retro_deinit_ptr();
    if (core_handle) {
        dlclose(core_handle);
        core_handle = NULL;
    }
    static_core_active = false;
    retro_run_ptr = NULL;
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
