#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

// Cabeçalhos oficiais do MPV (Irás precisar de ter a pasta 'mpv' nos teus includes)
#include <mpv/client.h>
#include <mpv/render_gl.h>

extern bool videoRodando;
extern char msgStatus[128];
extern int msgTimer;
extern unsigned int msgStatusColor;

static mpv_handle *mpv_ctx = NULL;

// Função para registar os eventos do MPV (como o pPlay faz no mpv.cpp)
static void checar_eventos_mpv() {
    while (mpv_ctx) {
        mpv_event *event = mpv_wait_event(mpv_ctx, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        if (event->event_id == MPV_EVENT_END_FILE) {
            // O vídeo acabou!
            videoRodando = false;
        }
    }
}

// A PONTE: Iniciar o motor do pPlay dentro do Hyper Neiva
void iniciarVideoNoMotorPPlay(const char* caminho) {
    if (mpv_ctx) {
        mpv_terminate_destroy(mpv_ctx);
        mpv_ctx = NULL;
    }

    strcpy(msgStatus, "Iniciando Motor pPlay (libmpv)...");
    msgTimer = 300;
    msgStatusColor = 0xFFFFAA00; // Laranja pPlay

    // 1. Cria a instância (Extrato direto da lógica do pPlay)
    mpv_ctx = mpv_create();
    if (!mpv_ctx) {
        strcpy(msgStatus, "ERRO FATAL: Falha ao criar libmpv!");
        msgStatusColor = 0xFFFF0000;
        return;
    }

    // 2. Configurações de Hardware do PS4 (Iguais às do Cpasjuste)
    // Ativa a descodificação por hardware
    mpv_set_option_string(mpv_ctx, "hwdec", "auto");
    // Configura o output de vídeo
    mpv_set_option_string(mpv_ctx, "vo", "gpu");
    // Define o cache para evitar engasgos de disco/rede
    mpv_set_option_string(mpv_ctx, "cache", "yes");
    mpv_set_option_string(mpv_ctx, "demuxer-max-bytes", "64M");

    // 3. Inicializa o motor
    if (mpv_initialize(mpv_ctx) < 0) {
        strcpy(msgStatus, "ERRO FATAL: mpv_initialize falhou!");
        msgStatusColor = 0xFFFF0000;
        mpv_terminate_destroy(mpv_ctx);
        mpv_ctx = NULL;
        return;
    }

    // 4. Carrega e reproduz o ficheiro!
    const char *cmd[] = {"loadfile", caminho, NULL};
    mpv_command(mpv_ctx, cmd);

    videoRodando = true;
    
    strcpy(msgStatus, "pPlay Engine Ativo! Pressione O para sair.");
    msgTimer = 300;
    msgStatusColor = 0xFF00FF00;

    // Loop de espera enquanto o vídeo roda
    while (videoRodando) {
        checar_eventos_mpv();
        sceKernelUsleep(16000); // Aguarda ~1 frame (60fps)
    }

    // Limpeza ao sair
    if (mpv_ctx) {
        mpv_terminate_destroy(mpv_ctx);
        mpv_ctx = NULL;
    }
}