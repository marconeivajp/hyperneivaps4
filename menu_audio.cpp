#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include "menu_audio.h"
#include "menu.h"
#include "graphics.h"
#include "audio.h"      
#include "explorar.h"   
#include <string.h>
#include <stdio.h>

extern bool showOpcoes;
extern int selAudioOpcao;
extern int totalItens;

extern int audioX, audioY, audioW, audioH;
extern int fontTam, listBg;
extern bool editMode;
extern int editTarget;

uint32_t getSysColor(int index);
extern void desenharTextoAlinhado(uint32_t* p, const char* textoOriginal, int fTam, int xBase, int y, int maxW, uint32_t cor);

int offAudioOpcao = 0;
int totalAudioOpcoes = 11;

const char* listaOpcoesAudio[11] = {
    "PLAY / PAUSE", "PARAR", "PROXIMA FAIXA", "FAIXA ANTERIOR",
    "VOLUME +", "VOLUME -", "ADIANTAR 10s", "RETROCEDER 10s",
    "MODO REPRODUCAO", "---", "VOLTAR"
};

void desenharMenuAudio(uint32_t* p) {
    if ((menuAtual == MENU_AUDIO_OPCOES && showOpcoes) || (editMode && editTarget == 6)) {
        for (int my = 0; my < audioH; my++) {
            for (int mx = 0; mx < audioW; mx++) {
                int pxX = audioX + mx;
                int pyY = audioY + my;
                if (pxX >= 0 && pxX < 1920 && pyY >= 0 && pyY < 1080) {
                    p[pyY * 1920 + pxX] = getSysColor(listBg);
                }
            }
        }

        int maxVisible = (audioH - 50) / 45;
        if (maxVisible < 1) maxVisible = 1;

        for (int i = 0; i < maxVisible; i++) {
            int gIdx = i + offAudioOpcao;
            if (gIdx >= 11) break;

            uint32_t corOp = (gIdx == selAudioOpcao) ? 0xFFFFFF00 : 0xFFFFFFFF;

            if (gIdx == 4) {
                char txtVol[64]; sprintf(txtVol, "VOLUME + (%d%%)", volumeGeral);
                desenharTextoAlinhado(p, txtVol, fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
            else if (gIdx == 5) {
                char txtVol[64]; sprintf(txtVol, "VOLUME - (%d%%)", volumeGeral);
                desenharTextoAlinhado(p, txtVol, fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
            else if (gIdx == 8) {
                const char* textosModo[] = {
                    "MODO: LINEAR",
                    "MODO: REPETIR FAIXA",
                    "MODO: REPETIR PASTA",
                    "MODO: ALEATORIO PASTA",
                    "MODO: ALEATORIO TODAS"
                };
                desenharTextoAlinhado(p, textosModo[modoReproducao], fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
            else {
                desenharTextoAlinhado(p, listaOpcoesAudio[gIdx], fontTam, audioX, audioY + 50 + (i * 45), audioW, corOp);
            }
        }
    }
}

void abrirMenuAudioOpcoes() {
    menuAtual = MENU_AUDIO_OPCOES;
    selAudioOpcao = 0;
    offAudioOpcao = 0;
    showOpcoes = true;
}

void tratarSelecaoAudio(int op) {
    if (totalItens <= 0 && menuAtual == MENU_MUSICAS) return;

    switch (op) {
    case 0:
        if (strcmp(musicaAtual, "PARADO") == 0) {
            if (strlen(ultimaMusicaTocada) > 0) {
                tocarMusicaNova(ultimaMusicaTocada);
                sprintf(msgStatus, "REPRODUZINDO");
                msgTimer = 90;
            }
            else {
                sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA");
                msgTimer = 90;
            }
        }
        else {
            comandoPausar = !comandoPausar;
            salvarConfiguracaoAudio(); // SALVA O STATUS DE PAUSE NO HD!
            sprintf(msgStatus, comandoPausar ? "MUSICA PAUSADA" : "REPRODUZINDO");
            msgTimer = 90;
        }
        break;
    case 1:
        tocarMusicaNova("PARADO");
        sprintf(msgStatus, "MUSICA PARADA");
        msgTimer = 90;
        break;
    case 2:
        if (strcmp(musicaAtual, "PARADO") != 0 || strlen(ultimaMusicaTocada) > 0) {
            tocarProximaMusica();
            sprintf(msgStatus, "TOCANDO PROXIMA");
            msgTimer = 90;
        }
        else {
            sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA");
            msgTimer = 90;
        }
        break;
    case 3:
        if (strcmp(musicaAtual, "PARADO") != 0 || strlen(ultimaMusicaTocada) > 0) {
            tocarMusicaAnterior();
            sprintf(msgStatus, "TOCANDO ANTERIOR");
            msgTimer = 90;
        }
        else {
            sprintf(msgStatus, "NENHUMA MUSICA SELECIONADA");
            msgTimer = 90;
        }
        break;
    case 4: aumentarVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; break;
    case 5: diminuirVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; break;
    case 6: if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { adiantarAudio(); sprintf(msgStatus, "AVANCANDO 10s"); msgTimer = 60; } break;
    case 7: if (strcmp(musicaAtual, "PARADO") != 0 && strlen(musicaAtual) > 0) { retrocederAudio(); sprintf(msgStatus, "RETROCEDENDO 10s"); msgTimer = 60; } break;
    case 8: {
        modoReproducao++;
        if (modoReproducao > 4) modoReproducao = 0;

        const char* msgModos[] = { "LINEAR", "REPETIR FAIXA", "REPETIR PASTA", "ALEATORIO PASTA", "ALEATORIO TODAS" };
        sprintf(msgStatus, "MODO: %s", msgModos[modoReproducao]);
        msgTimer = 90;
        salvarConfiguracaoAudio();
        break;
    }
    case 10: showOpcoes = false; if (menuAtual == MENU_AUDIO_OPCOES) menuAtual = MENU_MUSICAS; break;
    default: sprintf(msgStatus, "FUNCAO EM DESENVOLVIMENTO"); msgTimer = 60; break;
    }
}

// =========================================================================
// PLAYER DE MÚSICA: tela "Tocando agora" no estilo do player de vídeo
// (capa, título, letra, barra de tempo e a mesma fileira de ícones).
// =========================================================================
#include "stb_image.h"
#include "dr_mp3.h"
#include "dr_wav.h"
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include <orbis/Pad.h>

extern void desenharIcone(uint32_t* tela, int id, int tamanho, int posX, int posY);
extern int idIcone(const char* nome);
extern int medirLarguraTexto(const char* texto, int tam);
extern bool musicaCapaCaminho(const char* caminhoMusica, char* saida, int tam);
extern bool musicaInfo(const char* caminho, char* titulo, int tt, char* artista, int ta);
extern void letraDesenharPainel(uint32_t* p, const char* alvo, int x0, int y0, int w, int h, bool fundo);
extern bool letraVisivel(const char* alvo);
extern void letraAlternar();
extern volatile unsigned musicaCapaVersao;

bool musicaPlayerAberto = false;
static bool mpCheio = false;
bool musicaPlayerCheio() { return musicaPlayerAberto && mpCheio; }

// Temas da tela cheia, guardados em configuracao/player_musica.txt.
#define MP_TEMAS 3
static const char* mpNomesTema[MP_TEMAS] = { "Capa", "Disco", "Centro" };
#define MP_CONFIG "/data/HyperNeiva/configuracao/player_musica.txt"
static int mpTema = -1;
static int mpTemaAtual() {
    if (mpTema < 0) {
        mpTema = 0;
        FILE* f = fopen(MP_CONFIG, "r");
        if (f) { int t = 0; if (fscanf(f, "%d", &t) == 1 && t >= 0 && t < MP_TEMAS) mpTema = t; fclose(f); }
    }
    return mpTema;
}
static void mpSalvarTema(int t) {
    mpTema = t;
    FILE* f = fopen(MP_CONFIG, "w");
    if (f) { fprintf(f, "%d\n", t); fclose(f); }
}

enum { MP_ANTERIOR, MP_VOLTAR, MP_PLAY, MP_AVANCAR, MP_PROXIMO, MP_VOLUME, MP_MUDO, MP_TAMANHO, MP_MENU, MP_TOTAL };
static const char* mpIcones[MP_TOTAL] = { "p_anterior", "p_voltar", "p_play", "p_avancar", "p_proximo", "p_volume", "p_mudo", "p_pip", "p_tres_pontos" };
static const char* mpRotulos[MP_TOTAL] = { "Faixa anterior", "Voltar 10s", "Reproduzir", "Adiantar 10s", "Proxima faixa", "Volume", "Mudo", "Tela", "Mais opcoes" };
// A tela pequena mostra menos ícones.
static const int mpIconesCheio[] = { MP_ANTERIOR, MP_VOLTAR, MP_PLAY, MP_AVANCAR, MP_PROXIMO, MP_VOLUME, MP_MUDO, MP_TAMANHO, MP_MENU };
static const int mpIconesPequeno[] = { MP_ANTERIOR, MP_VOLTAR, MP_PLAY, MP_AVANCAR, MP_PROXIMO, MP_TAMANHO, MP_MENU };
static int mpFoco = MP_PLAY;
static bool mpEditVolume = false;
static int mpVolumeAntesMudo = 100;
static char mpAviso[64] = "";
static int mpAvisoQuadros = 0;
static bool mpMenuAberto = false;
static int mpMenuSel = 0;
#define MP_MENU_MAX 8
// Rádio: o que toca é um link (http/https). O player muda ícones, textos e menu.
static bool mpRadio = false;
static bool mpEhRadio(const char* c) { return !strncmp(c, "http://", 7) || !strncmp(c, "https://", 8); }
extern bool radioInfo(const char* url, char* nome, int tamNome, char* capa, int tamCapa);
extern unsigned radioVersaoCapas();
extern void radioTrocarEstacao(int passo);
extern bool radioFavoritaAtual();
extern void radioFavoritarAtual();
static int mpMenuItens() { return mpRadio ? 8 : 6; }
static const int mpIconesRadioCheio[] = { MP_ANTERIOR, MP_PLAY, MP_PROXIMO, MP_VOLUME, MP_MUDO, MP_TAMANHO, MP_MENU };
static const int mpIconesRadioPequeno[] = { MP_ANTERIOR, MP_PLAY, MP_PROXIMO, MP_MUDO, MP_TAMANHO, MP_MENU };

static void mpAvisar(const char* t) { snprintf(mpAviso, sizeof(mpAviso), "%s", t); mpAvisoQuadros = 90; }

static int mpListaIcones(const int** lista) {
    if (mpRadio && mpCheio) { *lista = mpIconesRadioCheio; return (int)(sizeof(mpIconesRadioCheio) / sizeof(int)); }
    if (mpRadio) { *lista = mpIconesRadioPequeno; return (int)(sizeof(mpIconesRadioPequeno) / sizeof(int)); }
    if (mpCheio) { *lista = mpIconesCheio; return (int)(sizeof(mpIconesCheio) / sizeof(int)); }
    *lista = mpIconesPequeno; return (int)(sizeof(mpIconesPequeno) / sizeof(int));
}

void musicaPlayerAbrir(bool cheio) {
    musicaPlayerAberto = true; mpCheio = cheio; mpFoco = MP_PLAY; mpEditVolume = false; mpMenuAberto = false;
}

// ---- duração: o MP3 não diz quanto dura; conta os quadros numa thread à parte ----
static char mpDurDe[256] = "";
static volatile float mpDur = 0.0f;
static volatile bool mpDurCalculando = false;

static void* mpThreadDuracao(void* arg) {
    char* c = (char*)arg;
    float d = 0.0f;
    int n = (int)strlen(c);
    if (n > 4 && !strcasecmp(c + n - 4, ".wav")) {
        drwav* w = (drwav*)malloc(sizeof(drwav));
        if (w && drwav_init_file(w, c, NULL)) { if (w->sampleRate) d = (float)w->totalPCMFrameCount / w->sampleRate; drwav_uninit(w); }
        free(w);
    }
    else {
        drmp3* m = (drmp3*)malloc(sizeof(drmp3));
        if (m && drmp3_init_file(m, c, NULL)) { drmp3_uint64 q = drmp3_get_pcm_frame_count(m); if (m->sampleRate) d = (float)q / m->sampleRate; drmp3_uninit(m); }
        free(m);
    }
    if (!strcmp(c, mpDurDe)) mpDur = d;
    free(c);
    mpDurCalculando = false;
    return NULL;
}

static float mpDuracao(const char* c) {
    if (strcmp(c, mpDurDe) != 0 && !mpDurCalculando && c[0] && strcmp(c, "PARADO") != 0) {
        snprintf(mpDurDe, sizeof(mpDurDe), "%s", c);
        mpDur = 0.0f;
        char* copia = strdup(c);
        if (copia) {
            mpDurCalculando = true;
            pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 512 * 1024);
            pthread_t th;
            if (pthread_create(&th, &at, mpThreadDuracao, copia) == 0) pthread_detach(th);
            else { free(copia); mpDurCalculando = false; }
            pthread_attr_destroy(&at);
        }
    }
    return mpDur;
}

// ---- capa, fundo e textos da música atual (refeitos só quando ela ou a capa mudam) ----
static char mpDe[256] = "";
static unsigned mpCapaVista = 0;
static char mpTitulo[200] = "", mpArtista[200] = "", mpPasta[200] = "";
static unsigned char* mpCapa = NULL;
static int mpCapaW = 0, mpCapaH = 0;
static uint32_t* mpFundo = NULL;

static void mpPreparar(const char* c) {
    bool mesma = !strcmp(c, mpDe);
    mpRadio = mpEhRadio(c);
    unsigned versao = musicaCapaVersao + radioVersaoCapas() * 7919u;
    if (mesma && mpCapaVista == versao && mpFundo) return;
    snprintf(mpDe, sizeof(mpDe), "%s", c);
    mpCapaVista = versao;
    char capaRadio[300] = "";
    if (mpRadio) {
        char nome[200];
        if (!radioInfo(c, nome, sizeof(nome), capaRadio, sizeof(capaRadio))) snprintf(nome, sizeof(nome), "Radio");
        snprintf(mpTitulo, sizeof(mpTitulo), "%s", nome);
        snprintf(mpArtista, sizeof(mpArtista), "Radio ao vivo");
        const char* host = c + (c[4] == 's' ? 8 : 7);
        snprintf(mpPasta, sizeof(mpPasta), "%.*s", (int)strcspn(host, "/:?"), host);
    }
    else if (!mesma) {
        char t[200] = "", a[200] = "";
        musicaInfo(c, t, sizeof(t), a, sizeof(a));
        const char* nome = strrchr(c, '/'); nome = nome ? nome + 1 : c;
        if (t[0]) snprintf(mpTitulo, sizeof(mpTitulo), "%s", t);
        else { snprintf(mpTitulo, sizeof(mpTitulo), "%s", nome); char* ponto = strrchr(mpTitulo, '.'); if (ponto) *ponto = 0; }
        if (!c[0] || !strcmp(c, "PARADO")) snprintf(mpTitulo, sizeof(mpTitulo), "Nenhuma musica tocando");
        snprintf(mpArtista, sizeof(mpArtista), "%s", a[0] ? a : "Artista desconhecido");
        char pasta[256]; snprintf(pasta, sizeof(pasta), "%s", c);
        char* barra = strrchr(pasta, '/'); if (barra) *barra = 0;
        const char* p2 = strrchr(pasta, '/');
        snprintf(mpPasta, sizeof(mpPasta), "Pasta: %s", p2 ? p2 + 1 : pasta);
    }
    if (mpCapa) { stbi_image_free(mpCapa); mpCapa = NULL; }
    char cp[600];
    int comp = 0;
    if (mpRadio) { if (capaRadio[0]) mpCapa = stbi_load(capaRadio, &mpCapaW, &mpCapaH, &comp, 4); }
    else if (c[0] && strcmp(c, "PARADO") != 0 && musicaCapaCaminho(c, cp, sizeof(cp))) mpCapa = stbi_load(cp, &mpCapaW, &mpCapaH, &comp, 4);
    // Fundo: a capa ampliada e escurecida (mais escura embaixo, onde fica a barra). Montado em RAM
    // uma vez; a cada quadro só é copiado para a tela.
    if (!mpFundo) mpFundo = (uint32_t*)malloc(1920 * 1080 * 4);
    if (!mpFundo) return;
    int faixa = mpCapa ? mpCapaW * 1080 / 1920 : 0;
    if (faixa > mpCapaH) faixa = mpCapaH;
    int y0 = mpCapa ? (mpCapaH - faixa) / 2 : 0;
    for (int y = 0; y < 1080; y++) {
        uint32_t f = 110 - (uint32_t)(y * 80 / 1080);   // de 110/256 até 30/256
        uint32_t* d = mpFundo + y * 1920;
        if (mpCapa && faixa > 0) {
            const unsigned char* linha = mpCapa + (size_t)(y0 + y * faixa / 1080) * mpCapaW * 4;
            for (int x = 0; x < 1920; x++) {
                const unsigned char* px = linha + (size_t)(x * mpCapaW / 1920) * 4;
                d[x] = 0xFF000000 | ((px[0] * f >> 8) << 16) | ((px[1] * f >> 8) << 8) | (px[2] * f >> 8);
            }
        }
        else {
            uint32_t r = 24 * f >> 7, g = 22 * f >> 7, b = 44 * f >> 7;
            for (int x = 0; x < 1920; x++) d[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
}

static void mpRetangulo(uint32_t* p, int x, int y, int w, int h, uint32_t cor) {
    for (int py = y < 0 ? 0 : y; py < y + h && py < 1080; py++)
        for (int px = x < 0 ? 0 : x; px < x + w && px < 1920; px++) p[py * 1920 + px] = cor;
}

static void mpCirculo(uint32_t* p, int cx, int cy, int r, uint32_t cor) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r) mpRetangulo(p, cx + x, cy + y, 1, 1, cor);
}

static void mpCortar(const char* s, int tam, int largura, char* saida, int cap) {
    snprintf(saida, cap, "%s", s);
    if (medirLarguraTexto(saida, tam) <= largura) return;
    int n = (int)strlen(saida);
    while (n > 0) {
        n--;
        while (n > 0 && ((unsigned char)saida[n] & 0xC0) == 0x80) n--;
        saida[n] = 0;
        char t[260]; snprintf(t, sizeof(t), "%s...", saida);
        if (medirLarguraTexto(t, tam) <= largura) { snprintf(saida, cap, "%s", t); return; }
    }
}

static void mpTempo(double s, char* t, int n) {
    int v = s > 0 ? (int)s : 0;
    if (v >= 3600) snprintf(t, n, "%d:%02d:%02d", v / 3600, (v / 60) % 60, v % 60);
    else snprintf(t, n, "%02d:%02d", v / 60, v % 60);
}

static const char* mpNomesModo[] = { "Em sequencia", "Repetir faixa", "Repetir pasta", "Aleatorio na pasta", "Aleatorio em tudo" };
static int mpModo() { int m = modoReproducao; return m < 0 || m > 4 ? 0 : m; }

static void mpTextoCentro(uint32_t* p, const char* t, int tam, int cx, int y, int larg, uint32_t cor) {
    char c[300]; mpCortar(t, tam, larg, c, sizeof(c));
    desenharTexto(p, c, tam, cx - medirLarguraTexto(c, tam) / 2, y, cor);
}

// Barra de tempo e ícones, nos dois tamanhos.
static void mpBarraEIcones(uint32_t* p, int x, int largura, int barraY, int iconeY, int tamIcone, int passo, int tamTexto, const char* c) {
    const uint32_t destaque = 0xFF29B6F6;
    bool parado = !strcmp(musicaAtual, "PARADO");
    double tempo = parado ? 0.0 : audioTempoAtual;
    if (mpRadio) {
        // Ao vivo: sem duração nem busca.
        int espessura = mpCheio ? 8 : 6;
        mpRetangulo(p, x, barraY - espessura / 2, largura, espessura, parado ? 0xFF5A5A5A : 0xFFE53935);
        int tamAo = mpCheio ? 26 : tamTexto;
        int wAo = medirLarguraTexto("AO VIVO", tamAo) + 28;
        int xAo = mpCheio ? 60 : x;
        mpRetangulo(p, xAo, barraY - tamAo, wAo, tamAo + 18, parado ? 0xFF444444 : 0xFFE53935);
        desenharTexto(p, "AO VIVO", tamAo, xAo + 14, barraY - tamAo + 6, 0xFFFFFFFF);
        char tt[24]; mpTempo(tempo, tt, sizeof(tt));
        if (mpCheio) desenharTexto(p, tt, 28, 1735, barraY - 16, 0xFFCCCCCC);
        else desenharTexto(p, tt, tamTexto, x + largura - medirLarguraTexto(tt, tamTexto), barraY + 12, 0xFFCCCCCC);
    }
    double total = mpRadio ? 0 : mpDuracao(c);
    double fracao = total > 0 ? tempo / total : 0.0;
    if (fracao < 0) fracao = 0;
    if (fracao > 1) fracao = 1;
    if (!mpRadio) {
        int espessura = mpCheio ? 8 : 6;
        int preenchido = (int)(largura * fracao);
        mpRetangulo(p, x, barraY - espessura / 2, largura, espessura, 0xFF5A5A5A);
        mpRetangulo(p, x, barraY - espessura / 2, preenchido, espessura, destaque);
        mpCirculo(p, x + preenchido, barraY, mpCheio ? 11 : 8, 0xFFFFFFFF);
        char tt[24], tot[24];
        mpTempo(tempo, tt, sizeof(tt));
        if (total > 0) mpTempo(total, tot, sizeof(tot)); else snprintf(tot, sizeof(tot), "--:--");
        if (mpCheio) { desenharTexto(p, tt, 28, 60, barraY - 16, 0xFFFFFFFF); desenharTexto(p, tot, 28, 1735, barraY - 16, 0xFFCCCCCC); }
        else { desenharTexto(p, tt, tamTexto, x, barraY + 12, 0xFFFFFFFF); desenharTexto(p, tot, tamTexto, x + largura - medirLarguraTexto(tot, tamTexto), barraY + 12, 0xFFCCCCCC); }
    }

    const int* lista; int n = mpListaIcones(&lista);
    int inicioX = (mpCheio ? (1920 - n * passo) / 2 : x + (largura - n * passo) / 2);
    int focoPos = 0;
    for (int k = 0; k < n; k++) {
        int i = lista[k];
        int ix = inicioX + k * passo + (passo - tamIcone) / 2;
        const char* nome = mpIcones[i];
        if (i == MP_PLAY && !parado && !comandoPausar) nome = "p_pause";
        if (i == MP_VOLUME && volumeGeral == 0) nome = "p_mudo";
        if (i == mpFoco) {
            focoPos = k;
            mpRetangulo(p, ix - 12, iconeY - 8, tamIcone + 24, tamIcone + 16, 0xFF3A3F46);
            mpRetangulo(p, ix - 12, iconeY + tamIcone + 6, tamIcone + 24, 4, destaque);
        }
        desenharIcone(p, idIcone(nome), tamIcone, ix, iconeY);
    }
    char rotulo[48];
    if (mpFoco == MP_VOLUME) snprintf(rotulo, sizeof(rotulo), "Volume %d%%%s", volumeGeral, mpEditVolume ? "  < >" : "");
    else if (mpFoco == MP_PLAY) snprintf(rotulo, sizeof(rotulo), "%s", (parado || comandoPausar) ? "Reproduzir" : "Pausar");
    else if (mpFoco == MP_TAMANHO) snprintf(rotulo, sizeof(rotulo), "%s", mpCheio ? "Tela pequena" : "Tela cheia");
    else if (mpRadio && mpFoco == MP_ANTERIOR) snprintf(rotulo, sizeof(rotulo), "Estacao anterior");
    else if (mpRadio && mpFoco == MP_PROXIMO) snprintf(rotulo, sizeof(rotulo), "Proxima estacao");
    else snprintf(rotulo, sizeof(rotulo), "%s", mpRotulos[mpFoco]);
    int rx = inicioX + focoPos * passo + passo / 2 - medirLarguraTexto(rotulo, tamTexto) / 2;
    desenharTexto(p, rotulo, tamTexto, rx, iconeY + tamIcone + 16, 0xFFFFFFFF);
}

// Menu dos três pontinhos.
static void mpDesenharMenu(uint32_t* p, int x, int yBase) {
    if (!mpMenuAberto) return;
    const int w = 420, passo = 46, h = mpMenuItens() * passo + 24;
    int y = yBase - h;
    mpRetangulo(p, x, y, w, h, 0xFF16161F);
    mpRetangulo(p, x, y, w, 3, 0xFF29B6F6);
    char itens[MP_MENU_MAX][64];
    snprintf(itens[0], 64, "%s", mpCheio ? "Tela pequena" : "Tela cheia");
    if (mpRadio) {
        extern int temaMidiaTema(int midia);
        extern const char* bibliotecaNomeTema(int tema);
        snprintf(itens[1], 64, "Pesquisar radio");
        snprintf(itens[2], 64, "%s", radioFavoritaAtual() ? "Remover dos favoritos" : "Favoritar esta radio");
        snprintf(itens[3], 64, "Tema do player: %s", mpNomesTema[mpTemaAtual()]);
        snprintf(itens[4], 64, "Tema da lista: %s", bibliotecaNomeTema(temaMidiaTema(5)));
        snprintf(itens[5], 64, "Opcoes antigas da radio");
        snprintf(itens[6], 64, "Parar radio");
        snprintf(itens[7], 64, "Fechar player");
    }
    else {
    snprintf(itens[1], 64, "Modo: %s", mpNomesModo[mpModo()]);
    snprintf(itens[2], 64, "Tema do player: %s", mpNomesTema[mpTemaAtual()]);
    {
        extern int temaMidiaTema(int midia);
        extern const char* bibliotecaNomeTema(int tema);
        snprintf(itens[3], 64, "Tema da lista: %s", bibliotecaNomeTema(temaMidiaTema(3)));
    }
    snprintf(itens[4], 64, "Parar musica");
    snprintf(itens[5], 64, "Fechar player");
    }
    for (int i = 0; i < mpMenuItens(); i++) {
        int iy = y + 12 + i * passo;
        if (i == mpMenuSel) mpRetangulo(p, x + 8, iy - 2, w - 16, passo - 4, 0xFF2C3440);
        desenharTexto(p, itens[i], 26, x + 24, iy + 6, i == mpMenuSel ? 0xFF29B6F6 : 0xFFFFFFFF);
    }
}

// ---- tela cheia ----
static void mpRenderizarCheio(uint32_t* p, const char* c) {
    const uint32_t destaque = 0xFF29B6F6;
    bool parado = !strcmp(musicaAtual, "PARADO");
    if (mpFundo) memcpy(p, mpFundo, 1920 * 1080 * 4);
    else mpRetangulo(p, 0, 0, 1920, 1080, 0xFF101018);
    int tema = mpTemaAtual();
    const char* estado = parado ? "PARADO" : (comandoPausar ? "PAUSADO" : "TOCANDO AGORA");
    char t[300];
    if (tema == 2) {
        // Centro: capa no meio, textos centralizados e a letra embaixo.
        const int ct = 420, cx = (1920 - ct) / 2, cy = 90;
        mpRetangulo(p, cx - 5, cy - 5, ct + 10, ct + 10, 0xFF2A2F38);
        if (mpCapa) desenharRedimensionado(p, mpCapa, mpCapaW, mpCapaH, ct, ct, cx, cy);
        else { mpRetangulo(p, cx, cy, ct, ct, 0xFF1C1C28); desenharIcone(p, idIcone("p_volume"), 120, cx + (ct - 120) / 2, cy + (ct - 120) / 2); }
        mpTextoCentro(p, estado, 24, 960, 528, 1600, destaque);
        mpTextoCentro(p, mpTitulo, 50, 960, 562, 1600, 0xFFFFFFFF);
        char a[260];
        if (mpRadio) snprintf(a, sizeof(a), "%s  -  %s", mpArtista, mpPasta);
        else snprintf(a, sizeof(a), "%s  -  Modo: %s", mpArtista, mpNomesModo[mpModo()]);
        mpTextoCentro(p, a, 30, 960, 630, 1600, 0xFFDDDDDD);
        if (!mpRadio && c[0] && letraVisivel(c)) letraDesenharPainel(p, c, 460, 660, 1000, 250, false);
    }
    else {
        // Capa (quadrada) ou Disco (redondo, como um vinil), com os textos e a letra à direita.
        const int cx = 150, cy = 150, ct = 600;
        if (tema == 1) {
            mpCirculo(p, cx + ct / 2, cy + ct / 2, ct / 2 + 8, 0xFF050507);
            if (mpCapa) desenharDiscoRedondo(p, mpCapa, mpCapaW, mpCapaH, ct, ct, cx, cy);
            else mpCirculo(p, cx + ct / 2, cy + ct / 2, ct / 2, 0xFF1C1C28);
            mpCirculo(p, cx + ct / 2, cy + ct / 2, 70, 0xFF0B0B10);
            mpCirculo(p, cx + ct / 2, cy + ct / 2, 14, 0xFF2A2F38);
        }
        else {
            mpRetangulo(p, cx - 6, cy - 6, ct + 12, ct + 12, 0xFF2A2F38);
            if (mpCapa) desenharRedimensionado(p, mpCapa, mpCapaW, mpCapaH, ct, ct, cx, cy);
            else { mpRetangulo(p, cx, cy, ct, ct, 0xFF1C1C28); desenharIcone(p, idIcone("p_volume"), 160, cx + (ct - 160) / 2, cy + (ct - 160) / 2); }
        }
        desenharTexto(p, estado, 26, 820, 150, destaque);
        mpCortar(mpTitulo, 56, 1020, t, sizeof(t)); desenharTexto(p, t, 56, 820, 195, 0xFFFFFFFF);
        mpCortar(mpArtista, 36, 1020, t, sizeof(t)); desenharTexto(p, t, 36, 820, 275, 0xFFDDDDDD);
        mpCortar(mpPasta, 24, 1020, t, sizeof(t)); desenharTexto(p, t, 24, 820, 330, 0xFF9A9AB0);
        if (mpRadio) snprintf(t, sizeof(t), "Transmissao ao vivo  -  L1/R1 trocam de estacao");
        else snprintf(t, sizeof(t), "Modo: %s", mpNomesModo[mpModo()]);
        desenharTexto(p, t, 24, 820, 365, 0xFF9A9AB0);
        if (mpRadio) {}
        else if (c[0] && letraVisivel(c)) letraDesenharPainel(p, c, 796, 405, 1060, 440, false);
        else desenharTexto(p, "Sem letra: ponha um .lrc com o mesmo nome da musica  (Quadrado mostra/esconde)", 22, 820, 430, 0xFF8C8CA6);
    }
    mpBarraEIcones(p, 210, 1500, 930, 968, 52, 104, 20, c);
    if (mpAvisoQuadros > 0) { mpAvisoQuadros--; desenharTexto(p, mpAviso, 30, 1560, 866, destaque); }
    desenharTexto(p, mpRadio ? "L1/R1 estacao    Triangulo fecha    O volta para a tela pequena" : "L1/R1 faixa    L2/R2 -10s/+10s    Quadrado letra    Triangulo fecha    O volta para a tela pequena", 22, 60, 40, 0xFFB0B0C8);
    mpDesenharMenu(p, 1440, 900);
}

void musicaPlayerRenderizar(uint32_t* p) {
    bool parado = !strcmp(musicaAtual, "PARADO");
    const char* c = parado ? ultimaMusicaTocada : musicaAtual;
    mpPreparar(c);
    mpRenderizarCheio(p, c);
}

// ---- tela pequena: um cartão no canto, por cima da lista de músicas ----
void musicaPlayerMiniDesenhar(uint32_t* p) {
    if (!musicaPlayerAberto || mpCheio || showOpcoes) return;
    const uint32_t destaque = 0xFF29B6F6;
    bool parado = !strcmp(musicaAtual, "PARADO");
    const char* c = parado ? ultimaMusicaTocada : musicaAtual;
    mpPreparar(c);
    const int x = 1140, y = 690, w = 740, h = 360;
    mpRetangulo(p, x, y, w, h, 0xFF12121C);
    mpRetangulo(p, x, y, w, 3, destaque);
    const int ct = 150;
    if (mpCapa) desenharRedimensionado(p, mpCapa, mpCapaW, mpCapaH, ct, ct, x + 20, y + 22);
    else { mpRetangulo(p, x + 20, y + 22, ct, ct, 0xFF1C1C28); desenharIcone(p, idIcone("p_volume"), 70, x + 20 + 40, y + 22 + 40); }
    char t[300];
    desenharTexto(p, parado ? "PARADO" : (comandoPausar ? "PAUSADO" : "TOCANDO AGORA"), 18, x + 190, y + 24, destaque);
    if (mpAvisoQuadros > 0) { mpAvisoQuadros--; desenharTexto(p, mpAviso, 18, x + w - 20 - medirLarguraTexto(mpAviso, 18), y + 24, destaque); }
    mpCortar(mpTitulo, 32, w - 210, t, sizeof(t)); desenharTexto(p, t, 32, x + 190, y + 52, 0xFFFFFFFF);
    mpCortar(mpArtista, 24, w - 210, t, sizeof(t)); desenharTexto(p, t, 24, x + 190, y + 98, 0xFFDDDDDD);
    if (mpRadio) snprintf(t, sizeof(t), "%s", mpPasta);
    else snprintf(t, sizeof(t), "Modo: %s", mpNomesModo[mpModo()]);
    desenharTexto(p, t, 20, x + 190, y + 136, 0xFF9A9AB0);
    mpBarraEIcones(p, x + 20, w - 40, y + 196, y + 252, 40, 96, 18, c);
    mpDesenharMenu(p, x + w - 420, y - 6);
}

static void mpAlternarPausa() {
    if (!strcmp(musicaAtual, "PARADO")) {
        if (ultimaMusicaTocada[0]) { tocarMusicaNova(ultimaMusicaTocada); mpAvisar("Reproduzindo"); }
        return;
    }
    comandoPausar = !comandoPausar;
    salvarConfiguracaoAudio();
    mpAvisar(comandoPausar ? "Pausado" : "Reproduzindo");
}

static void mpAtivarMenu(int i) {
    if (mpRadio) {
        switch (i) {
        case 0: mpCheio = !mpCheio; mpMenuAberto = false; break;
        case 1: { mpMenuAberto = false; musicaPlayerAberto = false; extern void exibirTecladoVirtual(int tipo); exibirTecladoVirtual(10); break; }
        case 2: radioFavoritarAtual(); mpAvisar(radioFavoritaAtual() ? "Favoritada" : "Fora dos favoritos"); break;
        case 3: mpSalvarTema((mpTemaAtual() + 1) % MP_TEMAS); if (!mpCheio) { mpCheio = true; } break;
        case 4: { extern int temaMidiaTema(int midia); extern void temaMidiaDefinir(int midia, int tema); temaMidiaDefinir(5, (temaMidiaTema(5) + 1) % 5); break; }
        case 5: { mpMenuAberto = false; musicaPlayerAberto = false; extern void preencherMenuOpcoesRadio(); preencherMenuOpcoesRadio(); break; }
        case 6: tocarMusicaNova("PARADO"); mpMenuAberto = false; mpAvisar("Parado"); break;
        case 7: mpMenuAberto = false; musicaPlayerAberto = false; break;
        }
        return;
    }
    switch (i) {
    case 0: mpCheio = !mpCheio; mpMenuAberto = false; break;
    case 1: modoReproducao = (mpModo() + 1) % 5; salvarConfiguracaoAudio(); break;
    case 2: mpSalvarTema((mpTemaAtual() + 1) % MP_TEMAS); if (!mpCheio) { mpCheio = true; } break;
    case 3: { extern int temaMidiaTema(int midia); extern void temaMidiaDefinir(int midia, int tema); temaMidiaDefinir(3, (temaMidiaTema(3) + 1) % 5); break; }
    case 4: tocarMusicaNova("PARADO"); mpMenuAberto = false; mpAvisar("Parado"); break;
    case 5: mpMenuAberto = false; musicaPlayerAberto = false; break;
    }
}

// Devolve true quando o player fecha (volta para a lista de músicas).
bool musicaPlayerControles(uint32_t botoes, uint32_t antigos) {
    uint32_t n = botoes & ~antigos;
    if (mpMenuAberto) {
        if (n & ORBIS_PAD_BUTTON_UP) mpMenuSel = (mpMenuSel + mpMenuItens() - 1) % mpMenuItens();
        if (n & ORBIS_PAD_BUTTON_DOWN) mpMenuSel = (mpMenuSel + 1) % mpMenuItens();
        if (n & ORBIS_PAD_BUTTON_CROSS) mpAtivarMenu(mpMenuSel);
        if (n & (ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_TRIANGLE)) mpMenuAberto = false;
        return !musicaPlayerAberto;
    }
    if (n & ORBIS_PAD_BUTTON_CIRCLE) {
        if (mpEditVolume) { mpEditVolume = false; return false; }
        if (mpCheio) { mpCheio = false; return false; }   // da tela cheia volta para a pequena
        musicaPlayerAberto = false;
        return true;
    }
    if (n & ORBIS_PAD_BUTTON_TRIANGLE) { musicaPlayerAberto = false; return true; }
    if (mpRadio) {
        if (n & ORBIS_PAD_BUTTON_L1) { radioTrocarEstacao(-1); mpAvisar("Estacao anterior"); }
        if (n & ORBIS_PAD_BUTTON_R1) { radioTrocarEstacao(1); mpAvisar("Proxima estacao"); }
    }
    else {
        if (n & ORBIS_PAD_BUTTON_L1) { tocarMusicaAnterior(); mpAvisar("Faixa anterior"); }
        if (n & ORBIS_PAD_BUTTON_R1) { tocarProximaMusica(); mpAvisar("Proxima faixa"); }
        if (n & ORBIS_PAD_BUTTON_L2) { retrocederAudio(); mpAvisar("-10s"); }
        if (n & ORBIS_PAD_BUTTON_R2) { adiantarAudio(); mpAvisar("+10s"); }
        if (n & ORBIS_PAD_BUTTON_SQUARE) letraAlternar();
    }
    if (mpEditVolume) {
        if (n & ORBIS_PAD_BUTTON_RIGHT) aumentarVolume();
        if (n & ORBIS_PAD_BUTTON_LEFT) diminuirVolume();
        if (n & (ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN)) mpEditVolume = false;
        return false;
    }
    const int* lista; int total = mpListaIcones(&lista);
    int pos = 0;
    for (int k = 0; k < total; k++) if (lista[k] == mpFoco) pos = k;
    if (lista[pos] != mpFoco) mpFoco = MP_PLAY;
    if ((n & ORBIS_PAD_BUTTON_LEFT) && pos > 0) mpFoco = lista[pos - 1];
    if ((n & ORBIS_PAD_BUTTON_RIGHT) && pos < total - 1) mpFoco = lista[pos + 1];
    if (n & ORBIS_PAD_BUTTON_CROSS) {
        switch (mpFoco) {
        case MP_ANTERIOR: if (mpRadio) { radioTrocarEstacao(-1); mpAvisar("Estacao anterior"); } else { tocarMusicaAnterior(); mpAvisar("Faixa anterior"); } break;
        case MP_VOLTAR: retrocederAudio(); mpAvisar("-10s"); break;
        case MP_PLAY: mpAlternarPausa(); break;
        case MP_AVANCAR: adiantarAudio(); mpAvisar("+10s"); break;
        case MP_PROXIMO: if (mpRadio) { radioTrocarEstacao(1); mpAvisar("Proxima estacao"); } else { tocarProximaMusica(); mpAvisar("Proxima faixa"); } break;
        case MP_VOLUME: mpEditVolume = true; break;
        case MP_MUDO:
            if (volumeGeral > 0) { mpVolumeAntesMudo = volumeGeral; volumeGeral = 0; mpAvisar("Mudo"); }
            else { volumeGeral = mpVolumeAntesMudo > 0 ? mpVolumeAntesMudo : 100; mpAvisar("Som ligado"); }
            salvarConfiguracaoAudio();
            break;
        case MP_TAMANHO: mpCheio = !mpCheio; break;
        case MP_MENU: mpMenuAberto = true; mpMenuSel = 0; break;
        }
    }
    return false;
}
