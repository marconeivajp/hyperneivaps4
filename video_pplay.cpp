#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

#include <mpv/client.h>
#include <mpv/render.h>

extern bool videoRodando;
extern bool video_pausado;
extern char msgStatus[128];
extern int msgTimer;
extern unsigned int msgStatusColor;
extern double video_time_current;
extern double video_time_total;
extern char ultimo_video_tocado[512];

static mpv_handle *mpv_ctx = NULL;
static mpv_render_context *mpv_render = NULL;

// ==========================================
// SISTEMA DE LOG DO MPV
// ==========================================
static void salvar_log_mpv(const char* msg) {
    FILE* f = fopen("/data/HyperNeiva/mpv_log.txt", "a");
    if (f) {
        fprintf(f, "[MPV] %s", msg);
        fclose(f);
    }
}

void checar_eventos_mpv() {
    if (!mpv_ctx) return;
    while (1) {
        mpv_event *event = mpv_wait_event(mpv_ctx, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            mpv_event_log_message *msg = (mpv_event_log_message *)event->data;
            char buf[512];
            snprintf(buf, sizeof(buf), "[%s] %s", msg->prefix, msg->text);
            salvar_log_mpv(buf);
        }

        switch (event->event_id) {
            case MPV_EVENT_END_FILE:
                videoRodando = false;
                break;
            case MPV_EVENT_PROPERTY_CHANGE: {
                mpv_event_property *prop = (mpv_event_property *)event->data;
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    if (strcmp(prop->name, "time-pos") == 0) video_time_current = *(double *)prop->data;
                    else if (strcmp(prop->name, "duration") == 0) video_time_total = *(double *)prop->data;
                }
                break;
            }
            default: break;
        }
    }
}

// ==========================================
// MOTOR PPLAY (MPV) - MODO ESTABILIDADE TOTAL
// ==========================================
void pararVideoMPV() {
    videoRodando = false;
    if (mpv_render) {
        mpv_render_context_free(mpv_render);
        mpv_render = NULL;
    }
    if (mpv_ctx) {
        mpv_terminate_destroy(mpv_ctx);
        mpv_ctx = NULL;
    }
}

void iniciarVideoMPV(const char* caminho) {
    if (videoRodando) pararVideoMPV();

    remove("/data/HyperNeiva/mpv_log.txt");
    salvar_log_mpv("Iniciando modo Estabilidade Total...\n");

    strcpy(ultimo_video_tocado, caminho);
    
    mpv_ctx = mpv_create();
    if (!mpv_ctx) return;

    mpv_request_log_messages(mpv_ctx, "info");

    // CONFIGURAÇÕES DE COMPATIBILIDADE PS4
    mpv_set_option_string(mpv_ctx, "hwdec", "no"); 
    mpv_set_option_string(mpv_ctx, "ao", "null"); 
    // REMOVEMOS QUALQUER VO OVERRIDE - DEIXA O RENDER_CONTEXT CONTROLAR
    
    mpv_set_option_string(mpv_ctx, "vd-lavc-threads", "1"); 
    mpv_set_option_string(mpv_ctx, "cache", "yes");
    mpv_set_option_string(mpv_ctx, "demuxer-max-bytes", "8M"); // Muito pequeno para testar

    if (mpv_initialize(mpv_ctx) < 0) {
        salvar_log_mpv("Falha no mpv_initialize\n");
        pararVideoMPV();
        return;
    }

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_SW},
        {MPV_RENDER_PARAM_INVALID, NULL}
    };

    if (mpv_render_context_create(&mpv_render, mpv_ctx, params) < 0) {
        salvar_log_mpv("Falha no mpv_render_context_create\n");
        pararVideoMPV();
        return;
    }

    mpv_observe_property(mpv_ctx, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_ctx, 0, "duration", MPV_FORMAT_DOUBLE);

    const char *cmd[] = {"loadfile", caminho, NULL};
    mpv_command(mpv_ctx, cmd);

    videoRodando = true;
    video_pausado = false;
    snprintf(msgStatus, sizeof(msgStatus), "pPlay Engine Ativo! [Final Test]");
    msgTimer = 180;
}

void atualizarVideoMPV(uint32_t* tela) {
    if (!videoRodando || !mpv_ctx || !mpv_render || !tela) return;

    checar_eventos_mpv();

    // RENDERIZAMOS DIRETO NA TELA - SE CRASHAR AQUI, O PROBLEMA É O RENDER_CONTEXT_RENDER
    int size[2] = {1920, 1080};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_SW_FORMAT, (void *)"bgra"},
        {MPV_RENDER_PARAM_SW_STRIDE, (void *)(intptr_t)(1920 * 4)},
        {MPV_RENDER_PARAM_SW_POINTER, tela},
        {MPV_RENDER_PARAM_INVALID, NULL}
    };

    uint64_t flags = mpv_render_context_update(mpv_render);
    if (flags & MPV_RENDER_UPDATE_FRAME) {
        // Logamos antes de tentar renderizar
        // salvar_log_mpv("Tentando renderizar frame...\n");
        if (mpv_render_context_render(mpv_render, params) < 0) {
            // salvar_log_mpv("Erro na renderizacao de frame.\n");
        }
    }
}

void executarAcaoMPV(int id) {
    if (!mpv_ctx) return;
    if (id == 800) { // Pause
        video_pausado = !video_pausado;
        int p = video_pausado ? 1 : 0;
        mpv_set_property(mpv_ctx, "pause", MPV_FORMAT_FLAG, &p);
    }
    if (id == 802 || id == 803) { // Volume
        extern int video_volume;
        double vol = (double)video_volume;
        mpv_set_property(mpv_ctx, "volume", MPV_FORMAT_DOUBLE, &vol);
    }
    if (id == 804) { // Seek +10
        const char *cmd[] = {"seek", "10", NULL};
        mpv_command(mpv_ctx, cmd);
    }
    if (id == 805) { // Seek -10
        const char *cmd[] = {"seek", "-10", NULL};
        mpv_command(mpv_ctx, cmd);
    }
}