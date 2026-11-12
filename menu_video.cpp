#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <orbis/Pad.h>
#include <orbis/libkernel.h>
#include "graphics.h"
#include "video.h"
#include "stb_image.h"
#include "icones_embutidos.h"

extern bool videoRodando, video_pausado, video_minimizado, exibir_botao_pular, bloqueio_fechar_video, pCross;
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
// ESCUDO DO PIP
// =========================================================================
extern bool menuL2_Aberto;
extern bool visualizandoPDF; 
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

// =========================================================================
// ÍCONES EMBUTIDOS NO EXECUTÁVEL
// =========================================================================
struct IconeCarregado { unsigned char* rgba; int largura, altura; uint32_t* escalado; int tamanho; };
static IconeCarregado iconesCarregados[TOTAL_ICONES_EMBUTIDOS];

int idIcone(const char* nome) {
    for (int i = 0; i < TOTAL_ICONES_EMBUTIDOS; i++) {
        if (strcmp(iconesEmbutidos[i].nome, nome) == 0) return i;
    }
    return -1;
}

static uint32_t* iconeNoTamanho(int id, int tamanho) {
    IconeCarregado* ic = &iconesCarregados[id];
    if (!ic->rgba) {
        int canais = 0;
        ic->rgba = stbi_load_from_memory(iconesEmbutidos[id].png, iconesEmbutidos[id].tamanho, &ic->largura, &ic->altura, &canais, 4);
        if (!ic->rgba) return NULL;
    }
    if (ic->escalado && ic->tamanho == tamanho) return ic->escalado;

    free(ic->escalado);
    ic->escalado = (uint32_t*)malloc((size_t)tamanho * tamanho * 4);
    if (!ic->escalado) return NULL;
    ic->tamanho = tamanho;

    // Média da área de origem de cada pixel, com alfa pré-multiplicado para não sujar a borda.
    for (int y = 0; y < tamanho; y++) {
        int y0 = y * ic->altura / tamanho, y1 = (y + 1) * ic->altura / tamanho;
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < tamanho; x++) {
            int x0 = x * ic->largura / tamanho, x1 = (x + 1) * ic->largura / tamanho;
            if (x1 <= x0) x1 = x0 + 1;
            unsigned int r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int sy = y0; sy < y1; sy++) for (int sx = x0; sx < x1; sx++) {
                const unsigned char* px = ic->rgba + ((size_t)sy * ic->largura + sx) * 4;
                r += px[0] * px[3]; g += px[1] * px[3]; b += px[2] * px[3]; a += px[3]; n++;
            }
            unsigned int alfa = a / n;
            if (a == 0) { ic->escalado[y * tamanho + x] = 0; continue; }
            ic->escalado[y * tamanho + x] = (alfa << 24) | ((r / a) << 16) | ((g / a) << 8) | (b / a);
        }
    }
    return ic->escalado;
}

void desenharIcone(uint32_t* tela, int id, int tamanho, int posX, int posY) {
    if (id < 0 || id >= TOTAL_ICONES_EMBUTIDOS || tamanho <= 0) return;
    uint32_t* icone = iconeNoTamanho(id, tamanho);
    if (!icone) return;

    for (int y = 0; y < tamanho; y++) {
        int py = posY + y;
        if (py < 0 || py >= 1080) continue;
        for (int x = 0; x < tamanho; x++) {
            int px = posX + x;
            if (px < 0 || px >= 1920) continue;
            uint32_t cor = icone[y * tamanho + x];
            uint32_t a = cor >> 24;
            if (a < 6) continue;
            if (a > 248) { tela[py * 1920 + px] = cor | 0xFF000000; continue; }
            uint32_t fundo = tela[py * 1920 + px], inv = 255 - a;
            uint32_t r = (((cor >> 16) & 255) * a + ((fundo >> 16) & 255) * inv) / 255;
            uint32_t g = (((cor >> 8) & 255) * a + ((fundo >> 8) & 255) * inv) / 255;
            uint32_t b = ((cor & 255) * a + (fundo & 255) * inv) / 255;
            tela[py * 1920 + px] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
}

// =========================================================================
// CONTROLES DO PLAYER EM TELA CHEIA
// =========================================================================
#define TOQUE_DUPLO_US 450000ULL
#define SEGURAR_US 300000ULL
#define CONTROLES_VISIVEIS_US 4000000ULL
#define CURSOR_ESPERA_US 10000000ULL

enum IconePlayer { IC_ANTERIOR, IC_VOLTAR, IC_PLAY, IC_AVANCAR, IC_PROXIMO, IC_VOLUME, IC_MUDO, IC_PIP, IC_MENU, TOTAL_IC };
static const char* rotulosPlayer[TOTAL_IC] = { "Video anterior", "Voltar 10s", "Reproduzir", "Adiantar 10s", "Proximo video", "Volume", "Mudo", "Tela pequena", "Menu do player" };

struct GestoGatilho { uint64_t ultimoToque, inicioAperto; bool modoPulo, segurando; };
static GestoGatilho gestoR2, gestoL2;
static uint64_t ultimoL1 = 0, ultimoR1 = 0;

static volatile uint64_t controlesAteUs = 0;
static volatile bool focoNosIcones = false;
static volatile int iconeFoco = IC_PLAY;
static volatile bool editandoVolume = false;
static volatile bool cursorAtivo = false;
static volatile double cursorTempo = 0.0;
static uint64_t cursorMovidoUs = 0, cursorInicioUs = 0, ultimoTickUs = 0, repeticaoVolumeUs = 0;
static int direcaoXAnterior = 0, direcaoYAnterior = 0;
static double multiplicadorSalvo = 1.0;
static int volumeAntesMudo = 100;
static char avisoControles[64] = "";
static volatile uint64_t avisoAteUs = 0;

static void mostrarControles(uint64_t duracaoUs) {
    uint64_t ate = sceKernelGetProcessTime() + duracaoUs;
    if (ate > controlesAteUs) controlesAteUs = ate;
}

void avisarPlayer(const char* texto) {
    snprintf(avisoControles, sizeof(avisoControles), "%s", texto);
    avisoAteUs = sceKernelGetProcessTime() + 2000000ULL;
    mostrarControles(2000000ULL);
}

bool playerControlesVisiveis() {
    return !menu_video_aberto && !video_minimizado && sceKernelGetProcessTime() < controlesAteUs;
}

static void confirmarCursor() {
    if (!cursorAtivo) return;
    cursorAtivo = false;
    pularParaTempo(cursorTempo);
}

static void liberarGatilhos() {
    if (gestoR2.segurando) video_fps_multiplier = multiplicadorSalvo;
    if (gestoL2.segurando) confirmarCursor();
    memset(&gestoR2, 0, sizeof(gestoR2));
    memset(&gestoL2, 0, sizeof(gestoL2));
}

void reiniciarControlesPlayer() {
    liberarGatilhos();
    extern volatile bool video_legenda_visivel;
    video_legenda_visivel = false;
    controlesAteUs = 0; focoNosIcones = false; iconeFoco = IC_PLAY; editandoVolume = false; cursorAtivo = false;
    ultimoL1 = 0; ultimoR1 = 0; direcaoXAnterior = 0; direcaoYAnterior = 0; avisoAteUs = 0;
}

bool fecharControlesPlayer() {
    if (!playerControlesVisiveis()) return false;
    // Com o cursor movido, a Bolinha só desiste da posição e a barra continua aberta.
    if (cursorAtivo) { cursorAtivo = false; avisarPlayer("Cancelado"); return true; }
    editandoVolume = false; focoNosIcones = false; controlesAteUs = 0;
    return true;
}

static void pularSegundos(double segundos) {
    pularParaTempo(video_time_current + segundos);
    char texto[16]; snprintf(texto, sizeof(texto), "%+.0fs", segundos);
    avisarPlayer(texto);
}

static void alternarPausa() {
    video_pausado = !video_pausado;
    avisarPlayer(video_pausado ? "Pausado" : "Reproduzindo");
}

static void alternarMudo() {
    if (video_volume > 0) { volumeAntesMudo = video_volume; video_volume = 0; avisarPlayer("Mudo"); }
    else { video_volume = volumeAntesMudo > 0 ? volumeAntesMudo : 100; avisarPlayer("Som ligado"); }
}

// R2 e L2: segurar é a ação contínua. Dois toques pulam 10 s e, depois disso, cada toque seguido
// pula de novo.
static void processarGatilho(GestoGatilho* g, bool apertou, bool segurado, double direcao, uint64_t agora, double passou) {
    if (apertou) {
        if (g->ultimoToque && agora - g->ultimoToque < TOQUE_DUPLO_US) {
            g->modoPulo = true;
            pularSegundos(direcao * 10.0);
        }
        else g->modoPulo = false;
        g->ultimoToque = agora;
        g->inicioAperto = agora;
    }

    if (segurado && !g->modoPulo && !g->segurando && agora - g->inicioAperto > SEGURAR_US) {
        g->segurando = true;
        if (direcao > 0) {
            multiplicadorSalvo = video_fps_multiplier;
            video_fps_multiplier = 2.0;
            avisarPlayer("Velocidade 2x");
        }
        else {
            cursorAtivo = true;
            cursorTempo = video_time_current;
            avisarPlayer("Voltando");
        }
    }

    if (g->segurando && segurado) {
        mostrarControles(1500000ULL);
        if (direcao < 0) {
            double alvo = cursorTempo - passou * 5.0;
            cursorTempo = alvo < 0.0 ? 0.0 : alvo;
            cursorMovidoUs = agora;
        }
    }

    if (g->segurando && !segurado) {
        g->segurando = false;
        g->ultimoToque = 0;
        if (direcao > 0) video_fps_multiplier = multiplicadorSalvo;
        else confirmarCursor();
    }
}

static void trocarVideoComToqueDuplo(uint64_t* ultimo, int direcao, const char* dica, uint64_t agora) {
    if (*ultimo && agora - *ultimo < TOQUE_DUPLO_US) {
        comando_trocar_video = direcao;
        *ultimo = 0;
    }
    else {
        *ultimo = agora;
        if (!video_minimizado) avisarPlayer(dica);
    }
}

static void ativarIconePlayer(int icone, bool& cross_consumido) {
    switch (icone) {
    case IC_ANTERIOR: comando_trocar_video = -1; break;
    case IC_PROXIMO: comando_trocar_video = 1; break;
    case IC_VOLTAR: pularSegundos(-10.0); break;
    case IC_AVANCAR: pularSegundos(10.0); break;
    case IC_PLAY: alternarPausa(); break;
    case IC_VOLUME: editandoVolume = !editandoVolume; break;
    case IC_MUDO: alternarMudo(); break;
    case IC_PIP:
        fecharControlesPlayer();
        video_minimizado = true;
        pCross = true;
        cross_consumido = false;
        return;
    case IC_MENU:
        fecharControlesPlayer();
        menu_video_aberto = true; vState = V_MAIN; menu_video_sel = 0; offVideoOpcao = 0; editando_opcao = false;
        break;
    }
    cross_consumido = true;
}

static void processarControlesTelaCheia(unsigned int btn, OrbisPadData* pData, bool& cross_consumido) {
    uint64_t agora = sceKernelGetProcessTime();
    double passou = ultimoTickUs ? (double)(agora - ultimoTickUs) / 1000000.0 : 0.0;
    if (passou > 0.2) passou = 0.2;
    ultimoTickUs = agora;

    processarGatilho(&gestoR2, (btn & ORBIS_PAD_BUTTON_R2) != 0, (pData->buttons & ORBIS_PAD_BUTTON_R2) != 0, 1.0, agora, passou);
    processarGatilho(&gestoL2, (btn & pData->buttons & ORBIS_PAD_BUTTON_L2) != 0, (pData->buttons & ORBIS_PAD_BUTTON_L2) != 0, -1.0, agora, passou);

    int dirX = 0, dirY = 0;
    if ((pData->buttons & ORBIS_PAD_BUTTON_LEFT) || pData->leftStick.x < 70) dirX = -1;
    else if ((pData->buttons & ORBIS_PAD_BUTTON_RIGHT) || pData->leftStick.x > 186) dirX = 1;
    if ((pData->buttons & ORBIS_PAD_BUTTON_UP) || pData->leftStick.y < 70) dirY = -1;
    else if ((pData->buttons & ORBIS_PAD_BUTTON_DOWN) || pData->leftStick.y > 186) dirY = 1;
    int toqueX = dirX != direcaoXAnterior ? dirX : 0;
    int toqueY = dirY != direcaoYAnterior ? dirY : 0;
    direcaoXAnterior = dirX; direcaoYAnterior = dirY;

    bool apertouX = (btn & ORBIS_PAD_BUTTON_CROSS) != 0 && !cross_consumido;

    if (!playerControlesVisiveis()) {
        if (cursorAtivo && !gestoL2.segurando) cursorAtivo = false;
        if (toqueX || toqueY) {
            focoNosIcones = false; editandoVolume = false; cursorAtivo = false;
            mostrarControles(CONTROLES_VISIVEIS_US);
        }
        else if (apertouX) { alternarPausa(); cross_consumido = true; }
        return;
    }

    if (dirX || dirY || apertouX) mostrarControles(CONTROLES_VISIVEIS_US);

    if (!focoNosIcones) {
        if (toqueY > 0) { cursorAtivo = false; focoNosIcones = true; iconeFoco = IC_PLAY; return; }

        if (dirX) {
            if (!cursorAtivo) { cursorAtivo = true; cursorTempo = video_time_current; cursorInicioUs = agora; }
            if (toqueX) { cursorInicioUs = agora; cursorTempo = cursorTempo + dirX * 5.0; }
            double tempoSegurado = (double)(agora - cursorInicioUs) / 1000000.0;
            double velocidade = tempoSegurado < 0.3 ? 0.0 : tempoSegurado < 1.5 ? 20.0 : tempoSegurado < 4.0 ? 60.0 : 180.0;
            double alvo = cursorTempo + dirX * velocidade * passou;
            if (alvo < 0.0) alvo = 0.0;
            if (video_time_total > 0.0 && alvo > video_time_total) alvo = video_time_total;
            cursorTempo = alvo;
            cursorMovidoUs = agora;
            mostrarControles(CURSOR_ESPERA_US);
        }

        if (apertouX) {
            if (cursorAtivo) confirmarCursor(); else alternarPausa();
            cross_consumido = true;
        }
        return;
    }

    if (editandoVolume) {
        if (dirX && (toqueX || agora - repeticaoVolumeUs > 120000ULL)) {
            repeticaoVolumeUs = agora;
            int volume = video_volume + dirX * 5;
            video_volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
        }
        if (toqueY < 0 || apertouX) { editandoVolume = false; if (apertouX) cross_consumido = true; }
        return;
    }

    extern volatile bool video_legenda_visivel;
    extern volatile int video_legenda_altura;
    if (toqueY > 0 && video_legenda_altura > 0) {
        video_legenda_visivel = !video_legenda_visivel;
        mostrarControles(CURSOR_ESPERA_US);
    }
    if (toqueY < 0) {
        // Primeiro fecha a mensagem; só depois sobe para a barra de tempo.
        if (video_legenda_visivel) video_legenda_visivel = false;
        else focoNosIcones = false;
        return;
    }
    if (video_legenda_visivel) mostrarControles(CURSOR_ESPERA_US);
    if (toqueX) {
        int proximo = iconeFoco + toqueX;
        iconeFoco = proximo < 0 ? 0 : (proximo >= TOTAL_IC ? TOTAL_IC - 1 : proximo);
    }
    if (apertouX) ativarIconePlayer(iconeFoco, cross_consumido);
}

static void retanguloPlayer(uint32_t* tela, int x, int y, int largura, int altura, uint32_t cor) {
    for (int py = y; py < y + altura; py++) {
        if (py < PLAYER_FAIXA_Y || py >= 1080) continue;
        for (int px = x; px < x + largura; px++) {
            if (px >= 0 && px < 1920) tela[py * 1920 + px] = cor;
        }
    }
}

static void circuloPlayer(uint32_t* tela, int cx, int cy, int raio, uint32_t cor) {
    for (int y = -raio; y <= raio; y++) for (int x = -raio; x <= raio; x++) {
        if (x * x + y * y <= raio * raio) retanguloPlayer(tela, cx + x, cy + y, 1, 1, cor);
    }
}

static void formatarTempoPlayer(double segundos, char* texto, int tamanho) {
    int total = segundos > 0.0 ? (int)segundos : 0;
    if (total >= 3600) snprintf(texto, tamanho, "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
    else snprintf(texto, tamanho, "%02d:%02d", total / 60, total % 60);
}

// Desenha na faixa em RAM que o video.cpp monta por cima do vídeo.
void desenharControlesPlayer(uint32_t* tela) {
    extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
    static const char* nomesIcones[TOTAL_IC] = { "p_anterior", "p_voltar", "p_play", "p_avancar", "p_proximo", "p_volume", "p_mudo", "p_pip", "p_tres_pontos" };
    const uint32_t corDestaque = 0xFF29B6F6;

    if (info_nome[0]) desenharTexto(tela, info_nome, 26, 60, 872, 0xFFFFFFFF);
    if (sceKernelGetProcessTime() < avisoAteUs) desenharTexto(tela, avisoControles, 30, 1560, 870, corDestaque);

    bool usandoCursor = cursorAtivo;
    double tempo = usandoCursor ? cursorTempo : video_time_current;
    double total = video_time_total > 0.0 ? video_time_total : 1.0;
    double fracao = tempo / total;
    if (fracao < 0.0) fracao = 0.0;
    if (fracao > 1.0) fracao = 1.0;

    const int barraX = 210, barraLargura = 1500, barraY = 930;
    bool focoBarra = !focoNosIcones;
    int espessura = focoBarra ? 10 : 6;
    int preenchido = (int)(barraLargura * fracao);
    retanguloPlayer(tela, barraX, barraY - espessura / 2, barraLargura, espessura, 0xFF5A5A5A);
    retanguloPlayer(tela, barraX, barraY - espessura / 2, preenchido, espessura, corDestaque);
    circuloPlayer(tela, barraX + preenchido, barraY, focoBarra ? 14 : 9, 0xFFFFFFFF);

    char textoTempo[24], textoTotal[24];
    formatarTempoPlayer(tempo, textoTempo, sizeof(textoTempo));
    formatarTempoPlayer(video_time_total, textoTotal, sizeof(textoTotal));
    desenharTexto(tela, textoTempo, 28, 60, 914, usandoCursor ? corDestaque : 0xFFFFFFFF);
    if (usandoCursor && !gestoL2.segurando) desenharTexto(tela, "X confirma     O cancela", 26, 800, 872, 0xFFFFFFFF);
    desenharTexto(tela, textoTotal, 28, 1735, 914, 0xFFCCCCCC);

    const int tamanhoIcone = 52, passo = 104;
    int inicioX = (1920 - TOTAL_IC * passo) / 2, iconeY = 968;
    for (int i = 0; i < TOTAL_IC; i++) {
        int x = inicioX + i * passo + (passo - tamanhoIcone) / 2;
        const char* nome = nomesIcones[i];
        if (i == IC_PLAY && !video_pausado) nome = "p_pause";
        if (i == IC_VOLUME && video_volume == 0) nome = "p_mudo";

        if (focoNosIcones && i == iconeFoco) {
            retanguloPlayer(tela, x - 16, iconeY - 10, tamanhoIcone + 32, tamanhoIcone + 20, 0xFF3A3F46);
            retanguloPlayer(tela, x - 16, iconeY + tamanhoIcone + 8, tamanhoIcone + 32, 4, corDestaque);
        }
        desenharIcone(tela, idIcone(nome), tamanhoIcone, x, iconeY);
    }

    if (focoNosIcones) {
        char rotulo[40];
        if (iconeFoco == IC_VOLUME) snprintf(rotulo, sizeof(rotulo), "%s %d%%%s", rotulosPlayer[IC_VOLUME], video_volume, editandoVolume ? "  < >" : "");
        else if (iconeFoco == IC_PLAY) snprintf(rotulo, sizeof(rotulo), "%s", video_pausado ? "Reproduzir" : "Pausar");
        else snprintf(rotulo, sizeof(rotulo), "%s", rotulosPlayer[iconeFoco]);
        int rotuloX = inicioX + iconeFoco * passo + passo / 2 - (int)strlen(rotulo) * 5;
        desenharTexto(tela, rotulo, 20, rotuloX, 1042, 0xFFFFFFFF);
    }
}


// =========================================================================
// LEGENDAS
// Fontes: arquivo .srt/.ass/.vtt com o mesmo nome do vídeo (lido inteiro pelo FFmpeg, na abertura)
// ou faixa de texto dentro do vídeo. Os pacotes da faixa chegam pela thread de leitura do
// video.cpp (legendaPacote), sem reler o arquivo. Legenda em imagem (PGS, DVD) não é suportada.
// =========================================================================
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <pthread.h>

struct LegendaFala { double ini, fim; char texto[300]; };
#define LEG_MAX_FALAS 6000
#define LEG_MAX_OPCOES 12
struct LegendaOpcao { char nome[96]; int stream; char arquivo[600]; bool suportada; };   // stream: -2 desligada, -1 arquivo
static LegendaOpcao legOpcoes[LEG_MAX_OPCOES];
static int legTotalOpcoes = 0, legEscolhida = 0;
static LegendaFala* legFalas = NULL;
static int legTotalFalas = 0;
static pthread_mutex_t legTrava = PTHREAD_MUTEX_INITIALIZER;      // falas
static pthread_mutex_t legTravaDec = PTHREAD_MUTEX_INITIALIZER;   // decodificador da faixa interna
static AVCodecContext* legDecoder = NULL;
static AVFormatContext* legFormato = NULL;
static volatile int legStreamAtiva = -1;
static double legBaseTempo = 0.0;
static volatile double legAtraso = 0.0;
static volatile unsigned legVersao = 1;

static bool legUtf8Valido(const char* s) {
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        int n = *p < 0x80 ? 0 : (*p >> 5) == 6 ? 1 : (*p >> 4) == 14 ? 2 : (*p >> 3) == 30 ? 3 : -1;
        if (n < 0) return false;
        p++;
        for (int k = 0; k < n; k++, p++) if ((*p >> 6) != 2) return false;
    }
    return true;
}

// Texto do FFmpeg -> linhas simples: tira os campos do ASS, {\tags}, <i> e afins.
static void legLimpar(const char* bruto, bool ass, char* saida, int cap) {
    const char* p = bruto;
    if (ass) {   // ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text
        int virgulas = 0; const char* q = p;
        while (*q && virgulas < 8) { if (*q == ',') virgulas++; q++; }
        if (virgulas == 8) p = q;
    }
    char tmp[600]; int o = 0;
    while (*p && o < (int)sizeof(tmp) - 2) {
        if (*p == '{') { const char* f = strchr(p, '}'); if (f) { p = f + 1; continue; } }
        if (*p == '<') { const char* f = strchr(p, '>'); if (f && f - p < 60) { p = f + 1; continue; } }
        if (*p == '\\' && (p[1] == 'N' || p[1] == 'n')) { tmp[o++] = '\n'; p += 2; continue; }
        if (*p == '\\' && p[1] == 'h') { tmp[o++] = ' '; p += 2; continue; }
        if (*p == '\r') { p++; continue; }
        tmp[o++] = *p++;
    }
    tmp[o] = 0;
    while (o > 0 && (tmp[o - 1] == '\n' || tmp[o - 1] == ' ')) tmp[--o] = 0;
    const char* ini = tmp; while (*ini == '\n' || *ini == ' ') ini++;
    if (legUtf8Valido(ini)) { snprintf(saida, cap, "%s", ini); return; }
    // Arquivo em Latin-1 (comum em .srt brasileiro): converte para UTF-8.
    int k = 0;
    for (const unsigned char* c = (const unsigned char*)ini; *c && k < cap - 3; c++) {
        if (*c < 0x80) saida[k++] = (char)*c;
        else { saida[k++] = (char)(0xC0 | (*c >> 6)); saida[k++] = (char)(0x80 | (*c & 0x3F)); }
    }
    saida[k] = 0;
}

static void legAdicionar(double ini, double fim, const char* texto) {
    if (!texto[0] || fim <= ini) return;
    pthread_mutex_lock(&legTrava);
    if (!legFalas) legFalas = (LegendaFala*)malloc(sizeof(LegendaFala) * LEG_MAX_FALAS);
    if (!legFalas || legTotalFalas >= LEG_MAX_FALAS) { pthread_mutex_unlock(&legTrava); return; }
    int pos = legTotalFalas;
    while (pos > 0 && legFalas[pos - 1].ini > ini) pos--;
    // Depois de uma busca a leitura passa de novo pelas mesmas falas: não repete.
    for (int k = pos - 1; k >= 0 && legFalas[k].ini > ini - 0.02; k--)
        if (!strcmp(legFalas[k].texto, texto)) { pthread_mutex_unlock(&legTrava); return; }
    memmove(&legFalas[pos + 1], &legFalas[pos], sizeof(LegendaFala) * (legTotalFalas - pos));
    legFalas[pos].ini = ini; legFalas[pos].fim = fim;
    snprintf(legFalas[pos].texto, sizeof(legFalas[pos].texto), "%s", texto);
    legTotalFalas++;
    pthread_mutex_unlock(&legTrava);
}

static AVCodecContext* legAbrirDecoder(AVStream* st) {
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) return NULL;
    AVCodecContext* dec = avcodec_alloc_context3(codec);
    if (!dec) return NULL;
    if (avcodec_parameters_to_context(dec, st->codecpar) < 0) { avcodec_free_context(&dec); return NULL; }
    dec->pkt_timebase = st->time_base;
    // Sem isto o FFmpeg recusa fala que não é UTF-8 (.srt em Latin-1); o legLimpar converte.
    dec->sub_charenc_mode = FF_SUB_CHARENC_MODE_IGNORE;
    if (avcodec_open2(dec, codec, NULL) < 0) { avcodec_free_context(&dec); return NULL; }
    return dec;
}

static void legDecodificar(AVCodecContext* dec, AVStream* st, AVPacket* pkt, double base) {
    AVSubtitle sub; int tem = 0;
    memset(&sub, 0, sizeof(sub));
    if (avcodec_decode_subtitle2(dec, &sub, &tem, pkt) < 0 || !tem) return;
    double tb = av_q2d(st->time_base);
    double pts = pkt->pts != AV_NOPTS_VALUE ? pkt->pts * tb : (sub.pts != AV_NOPTS_VALUE ? sub.pts / (double)AV_TIME_BASE : 0.0);
    pts -= base;
    double ini = pts + sub.start_display_time / 1000.0, fim;
    if (sub.end_display_time > sub.start_display_time && sub.end_display_time != UINT32_MAX) fim = pts + sub.end_display_time / 1000.0;
    else if (pkt->duration > 0) fim = pts + pkt->duration * tb;
    else fim = ini + 4.0;
    char texto[300] = "";
    for (unsigned i = 0; i < sub.num_rects; i++) {
        AVSubtitleRect* r = sub.rects[i];
        char limpo[300] = "";
        if (r->type == SUBTITLE_ASS && r->ass) legLimpar(r->ass, true, limpo, sizeof(limpo));
        else if (r->type == SUBTITLE_TEXT && r->text) legLimpar(r->text, false, limpo, sizeof(limpo));
        if (!limpo[0]) continue;
        size_t n = strlen(texto);
        snprintf(texto + n, sizeof(texto) - n, "%s%s", n ? "\n" : "", limpo);
    }
    avsubtitle_free(&sub);
    legAdicionar(ini, fim, texto);
}

static bool legCarregarArquivo(const char* caminho) {
    AVFormatContext* fc = NULL;
    if (avformat_open_input(&fc, caminho, NULL, NULL) != 0) return false;
    avformat_find_stream_info(fc, NULL);
    int s = av_find_best_stream(fc, AVMEDIA_TYPE_SUBTITLE, -1, -1, NULL, 0);
    AVCodecContext* dec = s >= 0 ? legAbrirDecoder(fc->streams[s]) : NULL;
    if (dec) {
        AVPacket* pkt = av_packet_alloc();
        while (pkt && av_read_frame(fc, pkt) >= 0) {
            if (pkt->stream_index == s) legDecodificar(dec, fc->streams[s], pkt, 0.0);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
        avcodec_free_context(&dec);
    }
    avformat_close_input(&fc);
    return legTotalFalas > 0;
}

// Troca a legenda. Faixa interna: volta a leitura ao ponto atual para pegar as falas de agora
// (a leitura anda uns 40 s à frente da imagem).
static void legEscolher(int i, bool buscar) {
    if (i < 0 || i >= legTotalOpcoes || !legOpcoes[i].suportada) return;
    pthread_mutex_lock(&legTravaDec);
    legStreamAtiva = -1;
    if (legDecoder) avcodec_free_context(&legDecoder);
    pthread_mutex_unlock(&legTravaDec);
    pthread_mutex_lock(&legTrava);
    legTotalFalas = 0;
    legEscolhida = i;
    legVersao++;
    pthread_mutex_unlock(&legTrava);
    LegendaOpcao* o = &legOpcoes[i];
    if (o->stream == -1) {
        if (!legCarregarArquivo(o->arquivo)) avisarPlayer("Legenda: arquivo sem falas");
    }
    else if (o->stream >= 0 && legFormato && o->stream < (int)legFormato->nb_streams) {
        pthread_mutex_lock(&legTravaDec);
        legDecoder = legAbrirDecoder(legFormato->streams[o->stream]);
        if (legDecoder) legStreamAtiva = o->stream;
        pthread_mutex_unlock(&legTravaDec);
        if (!legDecoder) avisarPlayer("Legenda: sem decodificador");
        else if (buscar) pularParaTempo(video_time_current);
    }
}

void legendaFecharVideo() {
    pthread_mutex_lock(&legTravaDec);
    legStreamAtiva = -1;
    if (legDecoder) avcodec_free_context(&legDecoder);
    legFormato = NULL;
    pthread_mutex_unlock(&legTravaDec);
    pthread_mutex_lock(&legTrava);
    legTotalFalas = 0; legTotalOpcoes = 0; legEscolhida = 0; legVersao++;
    pthread_mutex_unlock(&legTrava);
    total_legendas = 0;
}

static bool legIdiomaPortugues(const char* l) { return l && (!strncasecmp(l, "por", 3) || !strncasecmp(l, "pt", 2) || !strcasecmp(l, "pob")); }

// Chamada pelo video.cpp na abertura, antes das threads: monta as opções e escolhe a padrão.
void legendaAbrirVideo(AVFormatContext* fc, const char* caminho, double base, bool remoto) {
    legendaFecharVideo();
    legFormato = fc;
    legBaseTempo = base;
    legAtraso = 0.0;
    LegendaOpcao* o = &legOpcoes[legTotalOpcoes++];
    memset(o, 0, sizeof(*o)); snprintf(o->nome, sizeof(o->nome), "Desligada"); o->stream = -2; o->suportada = true;
    int padrao = 0;
    if (!remoto && caminho && caminho[0]) {
        static const char* sufixos[] = { ".srt", ".pt-BR.srt", ".pt.srt", ".por.srt", ".ptbr.srt", ".SRT", ".ass", ".ssa", ".vtt" };
        char base2[600]; snprintf(base2, sizeof(base2), "%s", caminho);
        char* ponto = strrchr(base2, '.'); char* barra = strrchr(base2, '/');
        if (ponto && (!barra || ponto > barra)) *ponto = 0;
        for (unsigned k = 0; k < sizeof(sufixos) / sizeof(sufixos[0]) && legTotalOpcoes < LEG_MAX_OPCOES; k++) {
            char arq[640]; snprintf(arq, sizeof(arq), "%s%s", base2, sufixos[k]);
            FILE* f = fopen(arq, "rb");
            if (!f) continue;
            fclose(f);
            o = &legOpcoes[legTotalOpcoes++];
            memset(o, 0, sizeof(*o));
            const char* nome = strrchr(arq, '/');
            snprintf(o->nome, sizeof(o->nome), "Arquivo %s", nome ? nome + 1 : arq);
            snprintf(o->arquivo, sizeof(o->arquivo), "%s", arq);
            o->stream = -1; o->suportada = true;
            if (!padrao) padrao = legTotalOpcoes - 1;
        }
    }
    int portugues = 0;
    for (unsigned i = 0; fc && i < fc->nb_streams && legTotalOpcoes < LEG_MAX_OPCOES; i++) {
        AVStream* st = fc->streams[i];
        if (st->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) continue;
        AVDictionaryEntry* lang = av_dict_get(st->metadata, "language", NULL, 0);
        AVDictionaryEntry* titulo = av_dict_get(st->metadata, "title", NULL, 0);
        enum AVCodecID id = st->codecpar->codec_id;
        bool imagem = id == AV_CODEC_ID_HDMV_PGS_SUBTITLE || id == AV_CODEC_ID_DVD_SUBTITLE || id == AV_CODEC_ID_DVB_SUBTITLE || id == AV_CODEC_ID_XSUB;
        bool temDecoder = avcodec_find_decoder(id) != NULL;
        o = &legOpcoes[legTotalOpcoes++];
        memset(o, 0, sizeof(*o));
        snprintf(o->nome, sizeof(o->nome), "Faixa %d: %s%s%s%s", legTotalOpcoes - 1, lang ? lang->value : "?", titulo ? " - " : "", titulo ? titulo->value : "",
            imagem ? " (imagem, sem suporte)" : (!temDecoder ? " (sem suporte)" : ""));
        o->stream = (int)i; o->suportada = !imagem && temDecoder;
        if (o->suportada && !portugues && legIdiomaPortugues(lang ? lang->value : NULL)) portugues = legTotalOpcoes - 1;
        if (total_legendas < 10) { snprintf(legendas_encontradas[total_legendas].nome, sizeof(legendas_encontradas[0].nome), "%s", o->nome); total_legendas++; }
    }
    if (!padrao) padrao = portugues;
    legEscolhida = 0;
    if (padrao) legEscolher(padrao, false);
}

// Thread de leitura do video.cpp.
bool legendaQuerStream(int indice) { return indice >= 0 && indice == legStreamAtiva; }
void legendaPacote(AVStream* st, AVPacket* pkt) {
    pthread_mutex_lock(&legTravaDec);
    if (legDecoder && st->index == legStreamAtiva) legDecodificar(legDecoder, st, pkt, legBaseTempo);
    pthread_mutex_unlock(&legTravaDec);
}

// Fala do momento: índice + 1, ou 0 sem fala.
static int legFalaEm(double t) {
    int lo = 0, hi = legTotalFalas - 1, ult = -1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (legFalas[m].ini <= t) { ult = m; lo = m + 1; } else hi = m - 1; }
    for (int k = ult; k >= 0 && k > ult - 30; k--) if (legFalas[k].fim > t) return k + 1;
    return 0;
}

// Identifica o que o apresentador deve mostrar agora (muda quando a fala muda).
unsigned legendaIdEm(double t) {
    if (legEscolhida == 0 || !legFalas) return 0;
    pthread_mutex_lock(&legTrava);
    int f = legFalas ? legFalaEm(t + legAtraso) : 0;
    unsigned id = f ? (unsigned)f * 2654435761u ^ legVersao : 0;
    pthread_mutex_unlock(&legTrava);
    return id;
}

// Desenha a fala por cima do vídeo no buffer de saída. Só escreve (ler a tela é lento): o texto
// com contorno é montado numa máscara em RAM quando muda.
void legendaDesenhar(uint32_t* tela, double t, bool controles) {
    extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
    extern int medirLarguraTexto(const char* texto, int tam);
    static char atual[300] = "";
    static uint32_t* mascara = NULL;
    static int altura = 0;
    char texto[300] = "";
    if (legEscolhida != 0 && legFalas) {
        pthread_mutex_lock(&legTrava);
        int f = legFalaEm(t + legAtraso);
        if (f) snprintf(texto, sizeof(texto), "%s", legFalas[f - 1].texto);
        pthread_mutex_unlock(&legTrava);
    }
    const int TAM = 46, PASSO = 60, MAXL = 4;
    if (strcmp(texto, atual) != 0) {
        snprintf(atual, sizeof(atual), "%s", texto);
        if (!mascara) mascara = (uint32_t*)malloc((size_t)1920 * (MAXL * PASSO + 20) * 4);
        if (!mascara) return;
        // Quebra em linhas que caibam em 1700 px.
        char linhas[MAXL][160]; int n = 0;
        const char* p = atual;
        while (*p && n < MAXL) {
            const char* fim = strchr(p, '\n'); if (!fim) fim = p + strlen(p);
            char linha[300]; int len = (int)(fim - p); if (len > 299) len = 299;
            memcpy(linha, p, len); linha[len] = 0;
            char* resto = linha;
            while (*resto && n < MAXL) {
                int corte = (int)strlen(resto);
                char tmp[300]; snprintf(tmp, sizeof(tmp), "%s", resto);
                while (corte > 0 && medirLarguraTexto(tmp, TAM) > 1700) {
                    int k = corte - 1; while (k > 0 && resto[k] != ' ') k--;
                    if (k <= 0) break;
                    corte = k; tmp[corte] = 0;
                }
                snprintf(linhas[n++], sizeof(linhas[0]), "%s", tmp);
                resto += corte; while (*resto == ' ') resto++;
            }
            p = *fim ? fim + 1 : fim;
        }
        altura = n ? n * PASSO + 20 : 0;
        memset(mascara, 0, (size_t)1920 * (MAXL * PASSO + 20) * 4);
        for (int i = 0; i < n; i++) {
            int x = (1920 - medirLarguraTexto(linhas[i], TAM)) / 2, y = 8 + i * PASSO;
            for (int dy = -3; dy <= 3; dy += 3) for (int dx = -3; dx <= 3; dx += 3)
                if (dx || dy) desenharTexto(mascara, linhas[i], TAM, x + dx, y + dy, 0xFF000000);
            desenharTexto(mascara, linhas[i], TAM, x, y, 0xFFFFFFFF);
        }
    }
    if (!atual[0] || !altura || !mascara) return;
    int base = controles ? PLAYER_FAIXA_Y - 12 : 1080 - 50;
    int topo = base - altura;
    if (topo < 0) topo = 0;
    for (int y = 0; y < altura && topo + y < 1080; y++) {
        const uint32_t* m = mascara + (size_t)y * 1920;
        uint32_t* d = tela + (size_t)(topo + y) * 1920;
        for (int x = 0; x < 1920; x++) if (m[x]) d[x] = m[x];
    }
}

static void legDesenharMenu(uint32_t* p, int mx, int my, int mw) {
    extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
    (void)mw;
    desenharTexto(p, "LEGENDAS", 18, mx + 8, my + 70, 0xFF00FFAA);
    int yy = my + 102;
    for (int i = 0; i < legTotalOpcoes && i < 10; i++) {
        char b[120];
        snprintf(b, sizeof(b), "%c%s %s", menu_video_sel == i ? '>' : ' ', legEscolhida == i ? "*" : " ", legOpcoes[i].nome);
        uint32_t cor = !legOpcoes[i].suportada ? 0xFF777777 : (menu_video_sel == i ? 0xFF00FFFF : 0xFFFFFFFF);
        desenharTexto(p, b, 15, mx + 8, yy, cor); yy += 30;
    }
    char b[64]; snprintf(b, sizeof(b), "%c Atraso: %+.2fs  < >", menu_video_sel == legTotalOpcoes ? '>' : ' ', (double)legAtraso);
    desenharTexto(p, b, 16, mx + 8, yy + 8, menu_video_sel == legTotalOpcoes ? 0xFF00FFFF : 0xFFFFFFFF);
    if (legEscolhida > 0) {
        snprintf(b, sizeof(b), "%d falas carregadas", legTotalFalas);
        desenharTexto(p, b, 14, mx + 8, yy + 44, 0xFF999999);
    }
}

static void legProcessarMenu(unsigned int btn, bool& cross_consumido, bool& circle_consumido) {
    int total = legTotalOpcoes + 1;
    if (menu_video_sel < 0) menu_video_sel = total - 1;
    if (menu_video_sel >= total) menu_video_sel = 0;
    if (menu_video_sel == legTotalOpcoes) {
        if (btn & ORBIS_PAD_BUTTON_RIGHT) legAtraso = legAtraso + 0.25;
        if (btn & ORBIS_PAD_BUTTON_LEFT) legAtraso = legAtraso - 0.25;
    }
    if (btn & ORBIS_PAD_BUTTON_CROSS) {
        cross_consumido = true;
        if (menu_video_sel < legTotalOpcoes) {
            if (!legOpcoes[menu_video_sel].suportada) avisarPlayer("Legenda em imagem: sem suporte");
            else { legEscolher(menu_video_sel, true); menu_video_aberto = false; avisarPlayer(menu_video_sel == 0 ? "Legenda desligada" : "Legenda ligada"); }
        }
    }
    if (btn & ORBIS_PAD_BUTTON_CIRCLE) { vState = V_MAIN; menu_video_sel = 9; circle_consumido = true; }
}

static const char* legNomeCurto() { return legEscolhida > 0 && legEscolhida < legTotalOpcoes ? "Ligada" : (legTotalOpcoes > 1 ? "Desligada" : "Nenhuma"); }

void processarControlesMenuVideo(unsigned int btn, OrbisPadData* pData, bool& cross_consumido, bool& circle_consumido) {
    if (!videoRodando) return;

    bool bloqueio_hud = (menuL2_Aberto || visualizandoPDF || visualizandoMidiaTexto ||
        visualizandoMidiaImagem || visualizandoPic1 || bloco_notas_aberto ||
        instrumento_aberto || teste_controle_aberto);

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

    // L1 e R1 trocam de vídeo só no toque duplo, para não trocar sem querer.
    if (!menu_video_aberto && (!video_minimizado || pip_selecionado)) {
        uint64_t agoraToque = sceKernelGetProcessTime();
        if (btn & ORBIS_PAD_BUTTON_L1) trocarVideoComToqueDuplo(&ultimoL1, -1, "L1 de novo: anterior", agoraToque);
        if (btn & ORBIS_PAD_BUTTON_R1) trocarVideoComToqueDuplo(&ultimoR1, 1, "R1 de novo: proximo", agoraToque);
    }

    if (!menu_video_aberto && (btn & ORBIS_PAD_BUTTON_CROSS)) {
        if (video_minimizado && pip_selecionado) {
            video_minimizado = false;
            pip_selecionado = false;
            cross_consumido = true;
        }
        else if (exibir_botao_pular && (!video_minimizado || pip_selecionado)) {
            pularParaTempo(tempo_pulo_alvo);
            cross_consumido = true;
        }
    }

    if (btn & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (menu_video_aberto || !video_minimizado || pip_selecionado) {
            menu_video_aberto = !menu_video_aberto; vState = V_MAIN; menu_video_sel = 0; offVideoOpcao = 0; editando_opcao = false;
            pData->buttons &= ~ORBIS_PAD_BUTTON_TRIANGLE;
        }
    }

    if (menu_video_aberto) {
        if (!editando_opcao) {
            if (btn & ORBIS_PAD_BUTTON_UP)   menu_video_sel--;
            if (btn & ORBIS_PAD_BUTTON_DOWN) menu_video_sel++;
        }
        if (vState == V_MAIN) {
            static int last_volume = 100;
            // AGORA TEMOS 13 OPÇÕES (0 a 12)
            if (menu_video_sel < 0)  { menu_video_sel = 12; offVideoOpcao = 12 - 9; if (offVideoOpcao < 0) offVideoOpcao = 0; }
            if (menu_video_sel > 12) { menu_video_sel = 0; offVideoOpcao = 0; }
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
            if (btn & ORBIS_PAD_BUTTON_CIRCLE) { menu_video_aberto = false; circle_consumido = true; }
            if (btn & ORBIS_PAD_BUTTON_CROSS) {
                cross_consumido = true;
                if (menu_video_sel == 0) { vState = V_INFO; menu_video_sel = 0; }
                if (menu_video_sel == 4) { video_pausado = !video_pausado; menu_video_aberto = false; }
                if (menu_video_sel == 5) { if (video_volume > 0) { last_volume = video_volume; video_volume = 0; } else { video_volume = last_volume > 0 ? last_volume : 100; } }
                if (menu_video_sel == 6) { video_minimizado = !video_minimizado; menu_video_aberto = false; if (video_minimizado) { pCross = true; cross_consumido = false; } }
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
                if (menu_video_sel == 9) { vState = V_TRACKS; menu_video_sel = legEscolhida; }
                if (menu_video_sel == 10) { mostrar_timeline = !mostrar_timeline; menu_video_aberto = false; } // NOVA OPÇÃO TIMELINE!
                if (menu_video_sel == 11) {
                    extern bool video_repetir, video_telegram, video_repetir_telegram;
                    video_repetir = !video_repetir;
                    if (video_telegram) video_repetir_telegram = video_repetir;
                }
                if (menu_video_sel == 12) { bloqueio_fechar_video = false; pararVideo(); menu_video_aberto = false; } // FECHAR PASSOU PARA O 11
            }
        }
        else if (vState == V_PIP) {
            if (!editando_opcao) {
                if (menu_video_sel < 0) menu_video_sel = 3; if (menu_video_sel > 3) menu_video_sel = 0;
                if (btn & ORBIS_PAD_BUTTON_CROSS) {
                    cross_consumido = true;
                    if (menu_video_sel == 3) { video_minimizado = !video_minimizado; menu_video_aberto = false; if (video_minimizado) { pCross = true; cross_consumido = false; } }
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
        else if (vState == V_TRACKS) legProcessarMenu(btn, cross_consumido, circle_consumido);
        else if (btn & (ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_CROSS)) { vState = V_MAIN; circle_consumido = true; cross_consumido = true; }
    }

    // Por último: assim o X usado pelo menu acima não vira pausa aqui.
    if (!menu_video_aberto && !video_minimizado) processarControlesTelaCheia(btn, pData, cross_consumido);
    else liberarGatilhos();
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
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            desenharTexto(p, "X PULAR", 14, bx + 10, by + (bh / 2) + 6, 0xFFFFFFFF);
        }
        else {
            for (int by = 850; by < 930; by++) for (int bx = 1550; bx < 1880; bx++) p[by * 1920 + bx] = 0xCC00AAFF;
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            desenharTexto(p, "PRESSIONE (X) PULAR", 22, 1580, 890, 0xFFFFFFFF);
        }
    }

    if (menu_video_aberto) {
        int mx = 25, my = 80, mw = 290, mh = 560; // Aumentei a altura para caber 12 itens
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
            
        extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
        desenharTexto(p, nome_disp, 22, mx + 8, my + 10, 0xFF00FFFF);
        desenharTexto(p, hud_t,     20, mx + 8, my + 38, 0xFFDDDDDD);
        for (int rx = mx; rx < mx + mw; rx++) p[(my + 62) * 1920 + rx] = 0xFF333355;

        if (vState == V_MAIN) {
            extern bool video_repetir;
            char opts[13][48];
            sprintf(opts[0],  "%c Informacoes",   (menu_video_sel == 0  ? '>' : ' '));
            sprintf(opts[1],  "%c SkipFr: %d",    (menu_video_sel == 1  ? '>' : ' '), frame_skip);
            sprintf(opts[2],  "%c Veloc: %.2fx",  (menu_video_sel == 2  ? '>' : ' '), video_fps_multiplier);
            sprintf(opts[3],  "%c Vol: %d%%",     (menu_video_sel == 3  ? '>' : ' '), video_volume);
            sprintf(opts[4],  "%c %s Video",      (menu_video_sel == 4  ? '>' : ' '), video_pausado ? "Retomar" : "Pausar");
            sprintf(opts[5],  "%c %s Audio",      (menu_video_sel == 5  ? '>' : ' '), video_volume == 0 ? "Desmutar" : "Mutar");
            sprintf(opts[6],  "%c %s PiP",     (menu_video_sel == 6  ? '>' : ' '), video_minimizado ? "Restaurar" : "Minimizar");
            sprintf(opts[7],  "%c Editar PIP",    (menu_video_sel == 7  ? '>' : ' '));
            sprintf(opts[8],  "%c Abertura",      (menu_video_sel == 8  ? '>' : ' '));
            sprintf(opts[9],  "%c Legendas: %s",  (menu_video_sel == 9  ? '>' : ' '), legNomeCurto());
            sprintf(opts[10], "%c %s Timeline",   (menu_video_sel == 10 ? '>' : ' '), mostrar_timeline ? "Ocultar" : "Mostrar"); // TIMELINE AQUI!
            sprintf(opts[11], "%c Repetir: %s",   (menu_video_sel == 11 ? '>' : ' '), video_repetir ? "Ligado" : "Desligado");
            sprintf(opts[12], "%c Fechar Video",  (menu_video_sel == 12 ? '>' : ' '));

            int maxVis = (mh - 70) / 40;
            for (int i = 0; i < maxVis; i++) {
                int gIdx = i + offVideoOpcao; if (gIdx >= 13) break;
                uint32_t cor = (gIdx == menu_video_sel) ? 0xFF00FFFF : 0xFFFFFFFF;
                desenharTexto(p, opts[gIdx], 23, mx + 10, my + 70 + (i * 40), cor);
            }
        }
        else if (vState == V_PIP) {
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
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
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            desenharTexto(p, "MARKERS DE PULO", 18, mx+8, my+70, 0xFFFF00FF);
            desenharTexto(p, "+ ATUAL", 18, mx+8, my+105, (menu_video_sel == 0) ? 0xFF00FF00 : 0xFFFFFFFF);
            for (int i = 0; i < num_unique_videos; i++) desenharTexto(p, unique_videos[i], 16, mx+8, my+140+(i*30), (menu_video_sel==(i+1))?0xFF00FFFF:0xFFFFFFFF);
        }
        else if (vState == V_SKIP_LIST) {
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            char b[128]; sprintf(b, "Markers: %s", selected_marker_video);
            desenharTexto(p, b, 15, mx+8, my+70, 0xFFFF00FF);
            for (int i = 0; i < num_visible_markers; i++) {
                char m[128]; sprintf(m, "%.0fs > %.0fs", markers[visible_markers[i]].inicio, markers[visible_markers[i]].fim);
                desenharTexto(p, m, 18, mx+8, my+100+(i*30), (menu_video_sel==i)?0xFF00FFFF:0xFFFFFFFF);
            }
        }
        else if (vState == V_SKIP_EDIT) {
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            desenharTexto(p, "EDITAR MARKER", 18, mx+8, my+70, 0xFFFF00FF);
            char b[128];
            sprintf(b, "Inicio: %.0fs", cfg_skip_inicio); desenharTexto(p, b, 17, mx+8, my+105, (menu_video_sel==0?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            sprintf(b, "Pular para: %.0fs", cfg_skip_fim); desenharTexto(p, b, 17, mx+8, my+135, (menu_video_sel==1?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            desenharTexto(p, "[SALVAR]", 18, mx+8, my+185, (menu_video_sel==2?0xFF00FFFF:0xFF00FF00));
            desenharTexto(p, "[EXCLUIR]", 18, mx+8, my+215, (menu_video_sel==3?0xFF00FFFF:0xFFFF0000));
        }
        else if (vState == V_SKIP_ADD) {
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            desenharTexto(p, "NOVO MARKER", 18, mx+8, my+70, 0xFFFF00FF);
            char b[128];
            sprintf(b, "Inicio: %.0fs", cfg_skip_inicio); desenharTexto(p, b, 17, mx+8, my+105, (menu_video_sel==0?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            sprintf(b, "Pular para: %.0fs", cfg_skip_fim); desenharTexto(p, b, 17, mx+8, my+135, (menu_video_sel==1?(editando_opcao?0xFF00FF00:0xFF00FFFF):0xFFFFFFFF));
            sprintf(b, "%s", cfg_skip_global?"Global (pasta)":"Este video"); desenharTexto(p, b, 17, mx+8, my+165, (menu_video_sel==2?0xFF00FFFF:0xFFFFFFFF));
            desenharTexto(p, "[SALVAR MARKER]", 18, mx+8, my+215, (menu_video_sel==3?0xFF00FFFF:0xFF00FF00));
        }
        else if (vState == V_INFO) {
            extern void desenharTexto(uint32_t* buffer, const char* texto, int tamanho, int x, int y, uint32_t corCorpo);
            desenharTexto(p, "INFORMACOES", 18, mx+8, my+70, 0xFFFFFF00);
            char b[128];
            sprintf(b, "Nome: %s", info_nome); desenharTexto(p, b, 14, mx+8, my+100, 0xFFFFFFFF);
            sprintf(b, "Res:  %s", info_res);  desenharTexto(p, b, 14, mx+8, my+125, 0xFFFFFFFF);
            sprintf(b, "FPS:  %.2f", fps_video); desenharTexto(p, b, 14, mx+8, my+150, 0xFFFFFFFF);
            sprintf(b, "Dur:  %02d:%02d", (int)video_time_total/60, (int)video_time_total%60); desenharTexto(p, b, 14, mx+8, my+175, 0xFFFFFFFF);
        }
        else if (vState == V_TRACKS) legDesenharMenu(p, mx, my, mw);
    }
}