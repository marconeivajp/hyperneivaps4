#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <orbis/Pad.h>
#include "graphics.h"
#include "video.h"

extern bool videoRodando, video_pausado, video_minimizado, exibir_botao_pular, bloqueio_fechar_video;
extern int frame_skip, video_volume, comando_trocar_video, pip_x, pip_y, pip_w, pip_h, total_audios, total_legendas;
extern double fps_video, video_time_current, video_time_total, tempo_pulo_alvo, video_fps_multiplier;
extern char info_nome[128], info_ext[16], info_res[32], info_aspect[32], info_fps[16], info_data_cria[32], info_data_mod[32];
extern void pararVideo(), pularParaTempo(double t), salvarMarker(double i, double f, bool global), removerMarker(int idx), salvarTodosMarkers();

struct TrackInfo { char nome[64]; };
extern TrackInfo audios_encontrados[10], legendas_encontradas[10];
struct VideoMarker { char nome[128]; double inicio; double fim; };
extern VideoMarker markers[50];
extern int total_markers;

// =========================================================================
// ESCUDO DO PIP: Coloque o nome real das variáveis dos seus apps aqui!
// =========================================================================
extern bool menuL2_Aberto;
extern bool visualizandoPDF; // Ajustado de visualizandoMidiaPDF para visualizandoPDF
extern bool visualizandoMidiaTexto;
extern bool visualizandoMidiaImagem;
extern bool visualizandoPic1;
extern bool bloco_notas_aberto;
extern bool instrumento_aberto;
extern bool teste_controle_aberto;
// =========================================================================

enum VideoMenuState { V_MAIN, V_PIP, V_SKIP_VIDEOS, V_SKIP_LIST, V_SKIP_ADD, V_SKIP_EDIT, V_INFO, V_TRACKS };
VideoMenuState vState = V_MAIN;

bool menu_video_aberto = false;
int menu_video_sel = 0;
int offVideoOpcao = 0;
bool editando_opcao = false;
int backup_pip_x, backup_pip_y, backup_pip_w, backup_pip_h;
bool pip_selecionado = false;

char unique_videos[21][128]; int num_unique_videos = 0;
char selected_marker_video[128]; int visible_markers[50]; int num_visible_markers = 0;
int selected_marker_idx = -1;
double cfg_skip_inicio = 0, cfg_skip_fim = 0;
bool cfg_skip_global = false;

void processarControlesMenuVideo(unsigned int btn, OrbisPadData* pData, bool& cross_consumido, bool& circle_consumido) {
    if (!videoRodando) return;

    // Se QUALQUER um desses menus/apps estiver aberto, o bloqueio_hud é verdadeiro!
    bool bloqueio_hud = (menuL2_Aberto || visualizandoPDF || visualizandoMidiaTexto ||
        visualizandoMidiaImagem || visualizandoPic1 || bloco_notas_aberto ||
        instrumento_aberto || teste_controle_aberto);

    // O PiP SÓ pega a Seta Esquerda se a tela estiver limpa (bloqueio_hud for falso)
    if (video_minimizado && !menu_video_aberto && !bloqueio_hud) {
        if (btn & ORBIS_PAD_BUTTON_LEFT) {
            pip_selecionado = true;
            pData->buttons &= ~ORBIS_PAD_BUTTON_LEFT;
        }
        if (pip_selecionado && (btn & ORBIS_PAD_BUTTON_RIGHT)) {
            pip_selecionado = false;
            pData->buttons &= ~ORBIS_PAD_BUTTON_RIGHT;
        }
    }
    else if (menu_video_aberto || !video_minimizado || bloqueio_hud) {
        pip_selecionado = false;
    }

    if (!menu_video_aberto || vState == V_MAIN) {
        if (!video_minimizado || pip_selecionado) {
            if (btn & ORBIS_PAD_BUTTON_L1) { comando_trocar_video = -1; cross_consumido = true; }
            if (btn & ORBIS_PAD_BUTTON_R1) { comando_trocar_video = 1;  cross_consumido = true; }
        }
    }

    if (!menu_video_aberto && (btn & ORBIS_PAD_BUTTON_CROSS)) {
        if (exibir_botao_pular && (!video_minimizado || pip_selecionado)) {
            pularParaTempo(tempo_pulo_alvo);
            cross_consumido = true;
        }
        else if (video_minimizado && pip_selecionado) {
            video_minimizado = false;
            pip_selecionado = false;
            cross_consumido = true;
        }
    }

    if (btn & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (!video_minimizado) { menu_video_aberto = !menu_video_aberto; vState = V_MAIN; menu_video_sel = 0; offVideoOpcao = 0; editando_opcao = false; }
    }

    if (menu_video_aberto) {
        if (!editando_opcao) {
            if (btn & ORBIS_PAD_BUTTON_UP)   menu_video_sel--;
            if (btn & ORBIS_PAD_BUTTON_DOWN) menu_video_sel++;
        }
        if (vState == V_MAIN) {
            static int last_volume = 100;
            if (menu_video_sel < 0)  { menu_video_sel = 10; offVideoOpcao = 10 - 9; if (offVideoOpcao < 0) offVideoOpcao = 0; }
            if (menu_video_sel > 10) { menu_video_sel = 0; offVideoOpcao = 0; }
            if (menu_video_sel < offVideoOpcao) offVideoOpcao = menu_video_sel;
            if (menu_video_sel >= offVideoOpcao + 10) offVideoOpcao = menu_video_sel - 9;
            if (pData->buttons & ORBIS_PAD_BUTTON_RIGHT) {
                if (menu_video_sel == 1) frame_skip++;
                if (menu_video_sel == 2) video_fps_multiplier += 0.25f;
                if (menu_video_sel == 3) { video_volume += 5; if (video_volume > 100) video_volume = 100; }
            }
            if (pData->buttons & ORBIS_PAD_BUTTON_LEFT) {
                if (menu_video_sel == 1) { frame_skip--; if (frame_skip < 0) frame_skip = 0; }
                if (menu_video_sel == 2) { video_fps_multiplier -= 0.25f; if (video_fps_multiplier < 0.25f) video_fps_multiplier = 0.25f; }
                if (menu_video_sel == 3) { video_volume -= 5; if (video_volume < 0) video_volume = 0; }
            }
            if (btn & ORBIS_PAD_BUTTON_CROSS) {
                cross_consumido = true;
                if (menu_video_sel == 0) { vState = V_INFO; menu_video_sel = 0; }
                if (menu_video_sel == 4) { video_pausado = !video_pausado; menu_video_aberto = false; }
                if (menu_video_sel == 5) { if (video_volume > 0) { last_volume = video_volume; video_volume = 0; } else { video_volume = last_volume > 0 ? last_volume : 100; } }
                if (menu_video_sel == 6) { video_minimizado = true; menu_video_aberto = false; }
                if (menu_video_sel == 7) { vState = V_PIP; menu_video_sel = 0; editando_opcao = false; }
                if (menu_video_sel == 8) {
                    num_unique_videos = 0; strcpy(unique_videos[num_unique_videos++], info_nome);
                    for (int i = 0; i < total_markers; i++) {
                        bool found = false;
                        for (int j = 0; j < num_unique_videos; j++) { if (strcmp(unique_videos[j], markers[i].nome) == 0) { found = true; break; } }
                        if (!found && num_unique_videos < 20) strcpy(unique_videos[num_unique_videos++], markers[i].nome);
                    }
                    vState = V_SKIP_VIDEOS; menu_video_sel = 0;
                }
                if (menu_video_sel == 9) { vState = V_TRACKS; menu_video_sel = 0; }
                if (menu_video_sel == 10) { bloqueio_fechar_video = false; pararVideo(); menu_video_aberto = false; }
            }
        }
        else if (vState == V_PIP) {
            if (!editando_opcao) {
                if (menu_video_sel < 0) menu_video_sel = 3; if (menu_video_sel > 3) menu_video_sel = 0;
                if (btn & ORBIS_PAD_BUTTON_CROSS) {
                    cross_consumido = true;
                    if (menu_video_sel == 3) { video_minimizado = !video_minimizado; menu_video_aberto = false; }
                    else { editando_opcao = true; backup_pip_x = pip_x; backup_pip_y = pip_y; backup_pip_w = pip_w; backup_pip_h = pip_h; }
                }
                if (btn & ORBIS_PAD_BUTTON_CIRCLE) { vState = V_MAIN; circle_consumido = true; }
            }
            else {
                if (menu_video_sel == 0) {
                    if (pData->buttons & ORBIS_PAD_BUTTON_RIGHT) pip_x += 10;
                    if (pData->buttons & ORBIS_PAD_BUTTON_LEFT)  pip_x -= 10;
                    if (pData->buttons & ORBIS_PAD_BUTTON_DOWN)  pip_y += 10;
                    if (pData->buttons & ORBIS_PAD_BUTTON_UP)    pip_y -= 10;
                }
                else if (menu_video_sel == 1) {
                    if (pData->buttons & ORBIS_PAD_BUTTON_RIGHT) { pip_w += 16; pip_h += 9; }
                    if (pData->buttons & ORBIS_PAD_BUTTON_LEFT) { pip_w -= 16; pip_h -= 9; }
                }
                else if (menu_video_sel == 2) {
                    if (pData->buttons & ORBIS_PAD_BUTTON_RIGHT) pip_w += 10;
                    if (pData->buttons & ORBIS_PAD_BUTTON_LEFT)  pip_w -= 10;
                    if (pData->buttons & ORBIS_PAD_BUTTON_DOWN)  pip_h += 10;
                    if (pData->buttons & ORBIS_PAD_BUTTON_UP)    pip_h -= 10;
                }
                if (btn & ORBIS_PAD_BUTTON_CROSS) { editando_opcao = false; cross_consumido = true; }
                if (btn & ORBIS_PAD_BUTTON_CIRCLE) { pip_x = backup_pip_x; pip_y = backup_pip_y; pip_w = backup_pip_w; pip_h = backup_pip_h; editando_opcao = false; circle_consumido = true; }
            }
        }
        else if (vState == V_SKIP_VIDEOS) {
            if (menu_video_sel < 0) menu_video_sel = num_unique_videos; if (menu_video_sel > num_unique_videos) menu_video_sel = 0;
            if (btn & ORBIS_PAD_BUTTON_CROSS) {
                cross_consumido = true;
                if (menu_video_sel == 0) { vState = V_SKIP_ADD; menu_video_sel = 0; cfg_skip_inicio = video_time_current; cfg_skip_fim = video_time_current + 90; }
                else { strcpy(selected_marker_video, unique_videos[menu_video_sel - 1]); vState = V_SKIP_LIST; menu_video_sel = 0; }
            }
            if (btn & ORBIS_PAD_BUTTON_CIRCLE) { vState = V_MAIN; circle_consumido = true; }
        }
        else if (vState == V_SKIP_LIST) {
            num_visible_markers = 0;
            for (int i = 0; i < total_markers; i++) { if (strcmp(markers[i].nome, selected_marker_video) == 0) visible_markers[num_visible_markers++] = i; }
            if (menu_video_sel < 0) menu_video_sel = num_visible_markers - 1;
            if (menu_video_sel >= num_visible_markers && num_visible_markers > 0) menu_video_sel = 0;

            if (btn & ORBIS_PAD_BUTTON_CROSS && num_visible_markers > 0) {
                cross_consumido = true;
                selected_marker_idx = visible_markers[menu_video_sel];
                cfg_skip_inicio = markers[selected_marker_idx].inicio;
                cfg_skip_fim = markers[selected_marker_idx].fim;
                vState = V_SKIP_EDIT; menu_video_sel = 0; editando_opcao = false;
            }
            if (btn & ORBIS_PAD_BUTTON_CIRCLE) { vState = V_SKIP_VIDEOS; circle_consumido = true; }
        }
        else if (vState == V_SKIP_EDIT) {
            if (!editando_opcao) {
                if (menu_video_sel < 0) menu_video_sel = 3; if (menu_video_sel > 3) menu_video_sel = 0;
                if (btn & ORBIS_PAD_BUTTON_CROSS) {
                    cross_consumido = true;
                    if (menu_video_sel == 0 || menu_video_sel == 1) editando_opcao = true;
                    if (menu_video_sel == 2) { markers[selected_marker_idx].inicio = cfg_skip_inicio; markers[selected_marker_idx].fim = cfg_skip_fim; salvarTodosMarkers(); vState = V_SKIP_LIST; menu_video_sel = 0; }
                    if (menu_video_sel == 3) { removerMarker(selected_marker_idx); vState = V_SKIP_LIST; menu_video_sel = 0; }
                }
                if (btn & ORBIS_PAD_BUTTON_CIRCLE) { vState = V_SKIP_LIST; circle_consumido = true; }
            }
            else {
                if (pData->buttons & ORBIS_PAD_BUTTON_RIGHT) { if (menu_video_sel == 0) cfg_skip_inicio += 1; else cfg_skip_fim += 1; }
                if (pData->buttons & ORBIS_PAD_BUTTON_LEFT) { if (menu_video_sel == 0) cfg_skip_inicio -= 1; else cfg_skip_fim -= 1; }
                if (btn & ORBIS_PAD_BUTTON_CROSS) { editando_opcao = false; cross_consumido = true; }
            }
        }
        else if (vState == V_SKIP_ADD) {
            if (!editando_opcao) {
                if (menu_video_sel < 0) menu_video_sel = 3; if (menu_video_sel > 3) menu_video_sel = 0;
                if (btn & ORBIS_PAD_BUTTON_CROSS) {
                    cross_consumido = true;
                    if (menu_video_sel == 0) { cfg_skip_inicio = video_time_current; editando_opcao = true; }
                    if (menu_video_sel == 1) { cfg_skip_fim = video_time_current; editando_opcao = true; }
                    if (menu_video_sel == 2) cfg_skip_global = !cfg_skip_global;
                    if (menu_video_sel == 3) { salvarMarker(cfg_skip_inicio, cfg_skip_fim, cfg_skip_global); vState = V_SKIP_VIDEOS; }
                }
                if (btn & ORBIS_PAD_BUTTON_CIRCLE) { vState = V_SKIP_VIDEOS; circle_consumido = true; }
            }
            else {
                if (pData->buttons & ORBIS_PAD_BUTTON_RIGHT) { if (menu_video_sel == 0) cfg_skip_inicio += 1; else cfg_skip_fim += 1; }
                if (pData->buttons & ORBIS_PAD_BUTTON_LEFT) { if (menu_video_sel == 0) cfg_skip_inicio -= 1; else cfg_skip_fim -= 1; }
                if (btn & ORBIS_PAD_BUTTON_CROSS) { editando_opcao = false; cross_consumido = true; }
            }
        }
        else if (btn & (ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_CROSS)) { vState = V_MAIN; circle_consumido = true; cross_consumido = true; }
    }
}

void desenharMenuPlayerVideo(uint32_t* p) {
    if (!videoRodando) return;

    if (video_minimizado && pip_selecionado) {
        uint32_t corBorda = 0xFF00FF00;
        for (int t = 0; t < 4; t++) {
            int bx = pip_x - t; int by = pip_y - t;
            int bw = pip_w + (t * 2); int bh = pip_h + (t * 2);
            for (int x = bx; x < bx + bw; x++) {
                if (x >= 0 && x < 1920 && by >= 0 && by < 1080) p[by * 1920 + x] = corBorda;
                if (x >= 0 && x < 1920 && (by + bh - 1) >= 0 && (by + bh - 1) < 1080) p[(by + bh - 1) * 1920 + x] = corBorda;
            }
            for (int y = by; y < by + bh; y++) {
                if (bx >= 0 && bx < 1920 && y >= 0 && y < 1080) p[y * 1920 + bx] = corBorda;
                if ((bx + bw - 1) >= 0 && (bx + bw - 1) < 1920 && y >= 0 && y < 1080) p[y * 1920 + (bx + bw - 1)] = corBorda;
            }
        }
    }

    if (exibir_botao_pular) {
        if (video_minimizado) {
            int bw = pip_w * 0.35f; int bh = pip_h * 0.15f;
            int bx = pip_x + pip_w - bw - 5; int by = pip_y + pip_h - bh - 5;
            for (int yy = by; yy < by + bh; yy++) {
                for (int xx = bx; xx < bx + bw; xx++) {
                    if (xx >= 0 && xx < 1920 && yy >= 0 && yy < 1080) p[yy * 1920 + xx] = 0xCC00AAFF;
                }
            }
            desenharTexto(p, "X PULAR", 14, bx + 10, by + (bh / 2) + 6, 0xFFFFFFFF);
        }
        else {
            for (int by = 850; by < 930; by++) for (int bx = 1550; bx < 1880; bx++) p[by * 1920 + bx] = 0xCC00AAFF;
            desenharTexto(p, "PRESSIONE (X) PULAR", 22, 1580, 890, 0xFFFFFFFF);
        }
    }

    if (menu_video_aberto) {
        int mx = 25, my = 80, mw = 290, mh = 530;
        for (int ry = my; ry < my + mh; ry++) for (int rx = mx; rx < mx + mw; rx++) p[ry * 1920 + rx] = 0xEE0D0D1A;

        static int marquee_offset = 0; static int marquee_timer  = 0;
        const char* nome_full = info_nome; int nome_len = (int)strlen(nome_full);
        char nome_disp[20];
        if (nome_len > 18) {
            marquee_timer++; if (marquee_timer >= 45) { marquee_timer = 0; marquee_offset++; if (marquee_offset > nome_len - 18) marquee_offset = 0; }
            strncpy(nome_disp, nome_full + marquee_offset, 18); nome_disp[18] = 0;
        } else { marquee_offset = 0; marquee_timer = 0; strncpy(nome_disp, nome_full, 19); nome_disp[19] = 0; }

        char hud_t[64];
        sprintf(hud_t, "%02.0f:%02.0f / %02.0f:%02.0f  FPS:%.0f",
            floor(video_time_current / 60), fmod(video_time_current, 60),
            floor(video_time_total / 60), fmod(video_time_total, 60), fps_video);
        desenharTexto(p, nome_disp, 22, mx + 8, my + 10, 0xFF00FFFF);
        desenharTexto(p, hud_t,     20, mx + 8, my + 38, 0xFFDDDDDD);
        for (int rx = mx; rx < mx + mw; rx++) p[(my + 62) * 1920 + rx] = 0xFF333355;

        if (vState == V_MAIN) {
            char opts[11][48];
            sprintf(opts[0],  "%c Informacoes",   (menu_video_sel == 0  ? '>' : ' '));
            sprintf(opts[1],  "%c SkipFr: %d",     (menu_video_sel == 1  ? '>' : ' '), frame_skip);
            sprintf(opts[2],  "%c Veloc: %.2fx",   (menu_video_sel == 2  ? '>' : ' '), video_fps_multiplier);
            sprintf(opts[3],  "%c Vol: %d%%",      (menu_video_sel == 3  ? '>' : ' '), video_volume);
            sprintf(opts[4],  "%c %s Video",       (menu_video_sel == 4  ? '>' : ' '), video_pausado ? "Retomar" : "Pausar");
            sprintf(opts[5],  "%c %s Audio",       (menu_video_sel == 5  ? '>' : ' '), video_volume == 0 ? "Desmutar" : "Mutar");
            sprintf(opts[6],  "%c Minimizar PiP",  (menu_video_sel == 6  ? '>' : ' '));
            sprintf(opts[7],  "%c Editar PIP",     (menu_video_sel == 7  ? '>' : ' '));
            sprintf(opts[8],  "%c Abertura",       (menu_video_sel == 8  ? '>' : ' '));
            sprintf(opts[9],  "%c Faixas",         (menu_video_sel == 9  ? '>' : ' '));
            sprintf(opts[10], "%c Fechar Video",   (menu_video_sel == 10 ? '>' : ' '));

            int maxVis = (mh - 70) / 40;
            for (int i = 0; i < maxVis; i++) {
                int gIdx = i + offVideoOpcao; if (gIdx >= 11) break;
                uint32_t cor = (gIdx == menu_video_sel) ? 0xFF00FFFF : 0xFFFFFFFF;
                desenharTexto(p, opts[gIdx], 23, mx + 10, my + 70 + (i * 40), cor);
            }
        }
        else if (vState == V_PIP) {
            desenharTexto(p, "EDITAR PIP", 18, mx+8, my+70, 0xFF00AAFF);
            uint32_t cor0 = (menu_video_sel == 0) ? (editando_opcao ? 0xFF00FF00 : 0xFF00FFFF) : 0xFFFFFFFF;
            uint32_t cor1 = (menu_video_sel == 1) ? (editando_opcao ? 0xFF00FF00 : 0xFF00FFFF) : 0xFFFFFFFF;
            uint32_t cor2 = (menu_video_sel == 2) ? (editando_opcao ? 0xFF00FF00 : 0xFF00FFFF) : 0xFFFFFFFF;
            char b[128];
            sprintf(b, "Pos X:%d Y:%d", pip_x, pip_y); desenharTexto(p, b, 17, mx+8, my+100, cor0);
            sprintf(b, "Tam W:%d H:%d", pip_w, pip_h); desenharTexto(p, b, 17, mx+8, my+130, cor1);
            sprintf(b, "Livre W:%d H:%d", pip_w, pip_h); desenharTexto(p, b, 17, mx+8, my+160, cor2);
            desenharTexto(p, "[ATIVAR/DESATIVAR PIP]", 17, mx+8, my+210, (menu_video_sel == 3 ? 0xFF00FFFF : 0xFFFFFFFF));
        }
        else if (vState == V_SKIP_VIDEOS) {
            desenharTexto(p, "MARKERS DE PULO", 18, mx+8, my+70, 0xFFFF00FF);
            desenharTexto(p, "+ ATUAL", 18, mx+8, my+105, (menu_video_sel == 0) ? 0xFF00FF00 : 0xFFFFFFFF);
            for (int i = 0; i < num_unique_videos; i++) desenharTexto(p, unique_videos[i], 16, mx+8, my+140+(i*30), (menu_video_sel==(i+1))?0xFF00FFFF:0xFFFFFFFF);
        }
        else if (vState == V_SKIP_LIST) {
            char b[128]; sprintf(b, "Markers: %s", selected_marker_video);
            desenharTexto(p, b, 15, mx+8, my+70, 0xFFFF00FF);
            for (int i = 0; i < num_visible_markers; i++) {
                char m[128]; sprintf(m, "%.0fs > %.0fs", markers[visible_markers[i]].inicio, markers[visible_markers[i]].fim);
                desenharTexto(p, m, 18, mx+8, my+100+(i*30), (menu_video_sel==i)?0xFF00FFFF:0xFFFFFFFF);
            }
        }
        else if (vState == V_SKIP_EDIT) {
            desenharTexto(p, "EDITAR MARKER", 18, mx+8, my+70, 0xFFFF00FF);
            char b[128];
            sprintf(b, "Inicio: %.0fs", cfg_skip_inicio); desenharTexto(p, b, 17, mx+8, my+105, (menu_video_sel==0?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            sprintf(b, "Pular para: %.0fs", cfg_skip_fim); desenharTexto(p, b, 17, mx+8, my+135, (menu_video_sel==1?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            desenharTexto(p, "[SALVAR]", 18, mx+8, my+185, (menu_video_sel==2?0xFF00FFFF:0xFF00FF00));
            desenharTexto(p, "[EXCLUIR]", 18, mx+8, my+215, (menu_video_sel==3?0xFF00FFFF:0xFFFF0000));
        }
        else if (vState == V_SKIP_ADD) {
            desenharTexto(p, "NOVO MARKER", 18, mx+8, my+70, 0xFFFF00FF);
            char b[128];
            sprintf(b, "Inicio: %.0fs", cfg_skip_inicio); desenharTexto(p, b, 17, mx+8, my+105, (menu_video_sel==0?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            sprintf(b, "Pular para: %.0fs", cfg_skip_fim); desenharTexto(p, b, 17, mx+8, my+135, (menu_video_sel==1?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            sprintf(b, "%s", cfg_skip_global?"Global (pasta)":"Este video"); desenharTexto(p, b, 17, mx+8, my+165, (menu_video_sel==2?0xFF00FFFF:0xFFFFFFFF));
            desenharTexto(p, "[SALVAR MARKER]", 18, mx+8, my+215, (menu_video_sel==3?0xFF00FFFF:0xFF00FF00));
        }
        else if (vState == V_INFO) {
            desenharTexto(p, "INFORMACOES", 18, mx+8, my+70, 0xFFFFFF00);
            char b[128];
            sprintf(b, "Nome: %s", info_nome); desenharTexto(p, b, 14, mx+8, my+100, 0xFFFFFFFF);
            sprintf(b, "Res:  %s", info_res);  desenharTexto(p, b, 14, mx+8, my+125, 0xFFFFFFFF);
            sprintf(b, "FPS:  %.2f", fps_video); desenharTexto(p, b, 14, mx+8, my+150, 0xFFFFFFFF);
            sprintf(b, "Dur:  %02d:%02d", (int)video_time_total/60, (int)video_time_total%60); desenharTexto(p, b, 14, mx+8, my+175, 0xFFFFFFFF);
        }
        else if (vState == V_TRACKS) {
            desenharTexto(p, "FAIXAS", 18, mx+8, my+70, 0xFF00FFAA);
            int yy = my+100;
            desenharTexto(p, "Audios:", 16, mx+8, yy, 0xFFFFFFFF); yy += 24;
            for (int i = 0; i < total_audios; i++) { desenharTexto(p, audios_encontrados[i].nome, 15, mx+16, yy, 0xFF00FFFF); yy += 22; }
            yy += 10;
            desenharTexto(p, "Legendas:", 16, mx+8, yy, 0xFFFFFFFF); yy += 24;
            for (int i = 0; i < total_legendas; i++) { desenharTexto(p, legendas_encontradas[i].nome, 15, mx+16, yy, 0xFF00FFFF); yy += 22; }
        }
    }
}