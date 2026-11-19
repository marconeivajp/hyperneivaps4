// =========================================================================
// EDITOR DE VÍDEO (motor): projeto, faixas, clipes, quadros-chave, transições, montagem do quadro,
// mixagem de áudio e exportação em MP4 (vídeo MPEG-4 + áudio AAC). Sem tela aqui: a tela fica no
// editor_video_ui.h. Roda igual no PS4 (FFmpeg 5.0 recompilado com exportação, ffmpeg_ps4/) e no
// PC (FFmpeg do MSYS2) para teste.
// Precisa, antes do include:
//   evPlatTexto(texto, px, cor, &w, &h)  -> RGBA (malloc) com o texto desenhado, ou NULL
//   evPlatLog(formato, ...)               -> registro (pode ser vazio)
// Formato das cores: 0xAARRGGBB. Buffers de imagem: RGBA, 4 bytes por pixel (R, G, B, A).
// =========================================================================
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/time.h>
}

#define EV_MAX_FAIXAS 10
#define EV_MAX_CLIPES 80          // por faixa
#define EV_MAX_CHAVES 24          // pontos de animação por clipe
#define EV_TAXA_AUDIO 48000

enum { EV_FAIXA_VISUAL = 0, EV_FAIXA_AUDIO = 1 };
enum { EV_CLIPE_VIDEO = 0, EV_CLIPE_IMAGEM, EV_CLIPE_AUDIO, EV_CLIPE_TEXTO };
enum { EV_TRANS_NENHUMA = 0, EV_TRANS_ESMAECER, EV_TRANS_DESLIZAR_ESQ, EV_TRANS_DESLIZAR_DIR, EV_TRANS_ZOOM, EV_TOTAL_TRANS };
static const char* const evNomesTransicao[EV_TOTAL_TRANS] = { "Nenhuma", "Esmaecer", "Deslizar da direita", "Deslizar da esquerda", "Zoom" };

// Ponto de animação: t em segundos desde o começo do clipe. x, y: centro em fração da tela (0..1).
// escala: 1 = ocupa a tela inteira sem cortar (vídeo/imagem) ou o tamanho natural (texto).
struct EvChave { double t; float x, y, escala, alfa, rot; };   // rot em graus

struct EvClipe {
    int tipo;
    char arquivo[512];
    double inicio;          // posição na linha do tempo (s)
    double fonteInicio;     // de onde começa dentro do arquivo (s)
    double duracao;         // quanto dura na linha do tempo (s)
    float volume;           // 0..2
    bool semAudio;          // vídeo com o áudio separado ou silenciado
    char texto[256];
    uint32_t cor;           // texto
    int tamanhoTexto;       // px numa tela de 1080 de altura
    int transicao; double transicaoDur;
    EvChave chaves[EV_MAX_CHAVES]; int totalChaves;
};

struct EvFaixa { int tipo; bool oculta, muda; EvClipe clipes[EV_MAX_CLIPES]; int total; };

struct EvProjeto {
    char nome[96];
    int largura, altura, fps;
    EvFaixa faixas[EV_MAX_FAIXAS]; int totalFaixas;
};

// ------------------------------------------------------------------ projeto
static void evProjetoNovo(EvProjeto* p, const char* nome, int largura, int altura, int fps) {
    memset(p, 0, sizeof(*p));
    snprintf(p->nome, sizeof(p->nome), "%s", nome && nome[0] ? nome : "Projeto");
    p->largura = largura & ~1; p->altura = altura & ~1; p->fps = fps > 0 ? fps : 30;
    p->totalFaixas = 2;
    p->faixas[0].tipo = EV_FAIXA_VISUAL;
    p->faixas[1].tipo = EV_FAIXA_AUDIO;
}
static int evFaixaNova(EvProjeto* p, int tipo) {
    if (p->totalFaixas >= EV_MAX_FAIXAS) return -1;
    EvFaixa* f = &p->faixas[p->totalFaixas];
    memset(f, 0, sizeof(*f)); f->tipo = tipo;
    return p->totalFaixas++;
}
static double evClipeFim(const EvClipe* c) { return c->inicio + c->duracao; }
static double evProjetoDuracao(const EvProjeto* p) {
    double fim = 0;
    for (int i = 0; i < p->totalFaixas; i++)
        for (int k = 0; k < p->faixas[i].total; k++) if (evClipeFim(&p->faixas[i].clipes[k]) > fim) fim = evClipeFim(&p->faixas[i].clipes[k]);
    return fim;
}
// Mantém os clipes da faixa em ordem de início.
static void evFaixaOrdenar(EvFaixa* f) {
    for (int i = 1; i < f->total; i++) for (int j = i; j > 0 && f->clipes[j - 1].inicio > f->clipes[j].inicio; j--) {
        EvClipe t = f->clipes[j - 1]; f->clipes[j - 1] = f->clipes[j]; f->clipes[j] = t;
    }
}
static EvClipe* evClipeNovo(EvProjeto* p, int faixa, int tipo, const char* arquivo, double inicio, double duracao) {
    if (faixa < 0 || faixa >= p->totalFaixas) return NULL;
    EvFaixa* f = &p->faixas[faixa];
    if (f->total >= EV_MAX_CLIPES) return NULL;
    EvClipe* c = &f->clipes[f->total++];
    memset(c, 0, sizeof(*c));
    c->tipo = tipo; c->inicio = inicio; c->duracao = duracao; c->volume = 1.0f;
    c->cor = 0xFFFFFFFF; c->tamanhoTexto = 72;
    if (arquivo) snprintf(c->arquivo, sizeof(c->arquivo), "%s", arquivo);
    c->chaves[0].t = 0; c->chaves[0].x = 0.5f; c->chaves[0].y = 0.5f; c->chaves[0].escala = 1.0f; c->chaves[0].alfa = 1.0f;
    c->totalChaves = 1;
    double ini = inicio;
    evFaixaOrdenar(f);
    for (int i = 0; i < f->total; i++) if (f->clipes[i].inicio == ini && f->clipes[i].tipo == tipo && !strcmp(f->clipes[i].arquivo, arquivo ? arquivo : "")) return &f->clipes[i];
    return NULL;
}
// Divide o clipe no instante t da linha do tempo: o pedaço da direita vira um clipe novo.
static bool evClipeDividir(EvProjeto* p, int faixa, int indice, double t) {
    EvFaixa* f = &p->faixas[faixa];
    if (indice < 0 || indice >= f->total || f->total >= EV_MAX_CLIPES) return false;
    EvClipe* c = &f->clipes[indice];
    double corte = t - c->inicio;
    if (corte <= 0.05 || corte >= c->duracao - 0.05) return false;
    EvClipe direita = *c;
    direita.inicio = t; direita.fonteInicio = c->fonteInicio + corte; direita.duracao = c->duracao - corte;
    direita.transicao = EV_TRANS_NENHUMA;
    // Quadros-chave: cada lado fica com os seus, reancorados.
    direita.totalChaves = 0;
    for (int k = 0; k < c->totalChaves; k++) if (c->chaves[k].t >= corte && direita.totalChaves < EV_MAX_CHAVES) { direita.chaves[direita.totalChaves] = c->chaves[k]; direita.chaves[direita.totalChaves++].t -= corte; }
    c->duracao = corte;
    int fica = 0; for (int k = 0; k < c->totalChaves; k++) if (c->chaves[k].t <= corte) c->chaves[fica++] = c->chaves[k];
    if (fica == 0) { c->chaves[0].t = 0; c->chaves[0].x = c->chaves[0].y = 0.5f; c->chaves[0].escala = c->chaves[0].alfa = 1; c->chaves[0].rot = 0; fica = 1; }
    c->totalChaves = fica;
    if (direita.totalChaves == 0) { direita.chaves[0] = c->chaves[c->totalChaves - 1]; direita.chaves[0].t = 0; direita.totalChaves = 1; }
    f->clipes[f->total++] = direita;
    evFaixaOrdenar(f);
    return true;
}
static void evClipeApagar(EvProjeto* p, int faixa, int indice) {
    EvFaixa* f = &p->faixas[faixa];
    if (indice < 0 || indice >= f->total) return;
    for (int i = indice; i < f->total - 1; i++) f->clipes[i] = f->clipes[i + 1];
    f->total--;
}
// Separa o áudio de um clipe de vídeo: o vídeo fica mudo e o som vira um clipe numa faixa de áudio.
static bool evClipeSepararAudio(EvProjeto* p, int faixa, int indice) {
    EvClipe* c = &p->faixas[faixa].clipes[indice];
    if (c->tipo != EV_CLIPE_VIDEO || c->semAudio) return false;
    int destino = -1;
    for (int i = 0; i < p->totalFaixas && destino < 0; i++) {
        if (p->faixas[i].tipo != EV_FAIXA_AUDIO) continue;
        bool livre = true;
        for (int k = 0; k < p->faixas[i].total; k++) {
            const EvClipe* o = &p->faixas[i].clipes[k];
            if (o->inicio < evClipeFim(c) && evClipeFim(o) > c->inicio) livre = false;
        }
        if (livre) destino = i;
    }
    if (destino < 0) destino = evFaixaNova(p, EV_FAIXA_AUDIO);
    if (destino < 0) return false;
    EvClipe copia = *c;
    c->semAudio = true;
    EvClipe* a = evClipeNovo(p, destino, EV_CLIPE_AUDIO, copia.arquivo, copia.inicio, copia.duracao);
    if (!a) { c->semAudio = false; return false; }
    a->fonteInicio = copia.fonteInicio; a->volume = copia.volume;
    return true;
}
// Animação: valor no instante tl (s desde o começo do clipe), linear entre os pontos.
static EvChave evClipeAnimacao(const EvClipe* c, double tl) {
    if (c->totalChaves <= 0) { EvChave k = { 0, 0.5f, 0.5f, 1, 1 }; return k; }
    if (tl <= c->chaves[0].t || c->totalChaves == 1) return c->chaves[0];
    for (int i = 1; i < c->totalChaves; i++) {
        const EvChave* a = &c->chaves[i - 1]; const EvChave* b = &c->chaves[i];
        if (tl <= b->t) {
            double u = b->t > a->t ? (tl - a->t) / (b->t - a->t) : 1;
            EvChave k; k.t = tl;
            k.x = (float)(a->x + (b->x - a->x) * u); k.y = (float)(a->y + (b->y - a->y) * u);
            k.escala = (float)(a->escala + (b->escala - a->escala) * u); k.alfa = (float)(a->alfa + (b->alfa - a->alfa) * u);
            k.rot = (float)(a->rot + (b->rot - a->rot) * u);
            return k;
        }
    }
    return c->chaves[c->totalChaves - 1];
}
// Põe (ou troca) um ponto de animação no instante tl do clipe, em ordem.
static void evClipeDefinirChave(EvClipe* c, double tl, float x, float y, float escala, float alfa, float rot = 0) {
    for (int i = 0; i < c->totalChaves; i++) if (fabs(c->chaves[i].t - tl) < 0.02) { c->chaves[i].x = x; c->chaves[i].y = y; c->chaves[i].escala = escala; c->chaves[i].alfa = alfa; c->chaves[i].rot = rot; return; }
    if (c->totalChaves >= EV_MAX_CHAVES) return;
    int i = c->totalChaves++;
    while (i > 0 && c->chaves[i - 1].t > tl) { c->chaves[i] = c->chaves[i - 1]; i--; }
    c->chaves[i].t = tl; c->chaves[i].x = x; c->chaves[i].y = y; c->chaves[i].escala = escala; c->chaves[i].alfa = alfa; c->chaves[i].rot = rot;
}
// Transformar sem animar: soma deslocamento, tamanho, giro e transparência em todos os pontos (o
// movimento animado, se houver, continua igual, só que no lugar novo).
static void evClipeTransformar(EvClipe* c, float dx, float dy, float fatorEscala, float drot, float dalfa) {
    for (int i = 0; i < c->totalChaves; i++) {
        EvChave* k = &c->chaves[i];
        k->x += dx; k->y += dy; k->escala *= fatorEscala; k->rot += drot; k->alfa += dalfa;
        if (k->escala < 0.02f) k->escala = 0.02f; if (k->escala > 8) k->escala = 8;
        if (k->alfa < 0) k->alfa = 0; if (k->alfa > 1) k->alfa = 1;
    }
}
static void evClipeApagarChave(EvClipe* c, double tl) {
    if (c->totalChaves <= 1) return;
    int melhor = 0;
    for (int i = 1; i < c->totalChaves; i++) if (fabs(c->chaves[i].t - tl) < fabs(c->chaves[melhor].t - tl)) melhor = i;
    for (int i = melhor; i < c->totalChaves - 1; i++) c->chaves[i] = c->chaves[i + 1];
    c->totalChaves--;
}

// ------------------------------------------------------------------ salvar / abrir (texto simples)
static bool evProjetoSalvar(const EvProjeto* p, const char* caminho) {
    FILE* f = fopen(caminho, "w");
    if (!f) return false;
    fprintf(f, "HNEDITOR 1\nnome\t%s\nformato\t%d\t%d\t%d\n", p->nome, p->largura, p->altura, p->fps);
    for (int i = 0; i < p->totalFaixas; i++) {
        const EvFaixa* fx = &p->faixas[i];
        fprintf(f, "faixa\t%d\t%d\t%d\n", fx->tipo, fx->oculta ? 1 : 0, fx->muda ? 1 : 0);
        for (int k = 0; k < fx->total; k++) {
            const EvClipe* c = &fx->clipes[k];
            fprintf(f, "clipe\t%d\t%.4f\t%.4f\t%.4f\t%.3f\t%d\t%08X\t%d\t%d\t%.3f\t%s\n", c->tipo, c->inicio, c->fonteInicio, c->duracao, c->volume,
                    c->semAudio ? 1 : 0, (unsigned)c->cor, c->tamanhoTexto, c->transicao, c->transicaoDur, c->arquivo);
            if (c->texto[0]) fprintf(f, "texto\t%s\n", c->texto);
            for (int j = 0; j < c->totalChaves; j++) fprintf(f, "chave\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.2f\n", c->chaves[j].t, c->chaves[j].x, c->chaves[j].y, c->chaves[j].escala, c->chaves[j].alfa, c->chaves[j].rot);
        }
    }
    fclose(f);
    return true;
}
static bool evProjetoAbrir(EvProjeto* p, const char* caminho) {
    FILE* f = fopen(caminho, "r");
    if (!f) return false;
    char linha[1400];
    if (!fgets(linha, sizeof(linha), f) || strncmp(linha, "HNEDITOR", 8)) { fclose(f); return false; }
    memset(p, 0, sizeof(*p)); p->fps = 30;
    EvClipe* atual = NULL;
    while (fgets(linha, sizeof(linha), f)) {
        size_t n = strlen(linha); while (n && (linha[n - 1] == '\n' || linha[n - 1] == '\r')) linha[--n] = 0;
        if (!strncmp(linha, "nome\t", 5)) snprintf(p->nome, sizeof(p->nome), "%s", linha + 5);
        else if (!strncmp(linha, "formato\t", 8)) sscanf(linha + 8, "%d\t%d\t%d", &p->largura, &p->altura, &p->fps);
        else if (!strncmp(linha, "faixa\t", 6) && p->totalFaixas < EV_MAX_FAIXAS) {
            EvFaixa* fx = &p->faixas[p->totalFaixas++]; memset(fx, 0, sizeof(*fx));
            int o = 0, m = 0; sscanf(linha + 6, "%d\t%d\t%d", &fx->tipo, &o, &m); fx->oculta = o; fx->muda = m;
            atual = NULL;
        }
        else if (!strncmp(linha, "clipe\t", 6) && p->totalFaixas > 0) {
            EvFaixa* fx = &p->faixas[p->totalFaixas - 1];
            if (fx->total >= EV_MAX_CLIPES) continue;
            EvClipe* c = &fx->clipes[fx->total++]; memset(c, 0, sizeof(*c));
            unsigned cor = 0xFFFFFFFF; int sem = 0, pos = 0;
            sscanf(linha + 6, "%d\t%lf\t%lf\t%lf\t%f\t%d\t%X\t%d\t%d\t%lf\t%n", &c->tipo, &c->inicio, &c->fonteInicio, &c->duracao, &c->volume, &sem, &cor, &c->tamanhoTexto, &c->transicao, &c->transicaoDur, &pos);
            c->semAudio = sem; c->cor = cor;
            if (pos > 0) snprintf(c->arquivo, sizeof(c->arquivo), "%s", linha + 6 + pos);
            atual = c;
        }
        else if (!strncmp(linha, "texto\t", 6) && atual) snprintf(atual->texto, sizeof(atual->texto), "%s", linha + 6);
        else if (!strncmp(linha, "chave\t", 6) && atual && atual->totalChaves < EV_MAX_CHAVES) {
            EvChave* k = &atual->chaves[atual->totalChaves++];
            k->rot = 0;   // projetos antigos não têm o giro
            sscanf(linha + 6, "%lf\t%f\t%f\t%f\t%f\t%f", &k->t, &k->x, &k->y, &k->escala, &k->alfa, &k->rot);
        }
    }
    fclose(f);
    if (p->largura <= 0 || p->altura <= 0) { p->largura = 1280; p->altura = 720; }
    return p->totalFaixas > 0;
}

// ------------------------------------------------------------------ leitura das fontes
// Uma fonte = um arquivo aberto para vídeo OU para áudio. Leitura em ordem (exportação) com
// busca quando o tempo pedido volta ou pula para longe (pré-visualização).
struct EvFonte {
    char arquivo[512];
    bool audio, falhou, fim;
    AVFormatContext* fmt; AVCodecContext* dec; int stream;
    AVPacket* pacote; AVFrame* quadro;
    // vídeo: último quadro convertido para RGBA, no tamanho em que cabe na tela
    struct SwsContext* sws; uint8_t* rgba; int rw, rh; double rgbaPts; double proxPts;
    // Último quadro decodificado (bruto) e o intervalo que ele cobre; rgba é a conversão dele.
    AVFrame* bruto; double brutoPts, brutoProx; bool convertido; int convW, convH;
    // áudio: amostras estéreo float a 48 kHz já decodificadas, a partir de aInicio (s da fonte)
    SwrContext* swr; float* aBuf; int aLen, aCap; double aInicio;
    int usoLru;
};

static void evFonteFechar(EvFonte* f) {
    if (f->dec) avcodec_free_context(&f->dec);
    if (f->fmt) avformat_close_input(&f->fmt);
    if (f->pacote) av_packet_free(&f->pacote);
    if (f->quadro) av_frame_free(&f->quadro);
    if (f->bruto) av_frame_free(&f->bruto);
    if (f->sws) { sws_freeContext(f->sws); f->sws = NULL; }
    if (f->swr) swr_free(&f->swr);
    free(f->rgba); free(f->aBuf);
    memset(f, 0, sizeof(*f));
}
static bool evFonteAbrir(EvFonte* f, const char* arquivo, bool audio, int threads = 2) {
    memset(f, 0, sizeof(*f));
    snprintf(f->arquivo, sizeof(f->arquivo), "%s", arquivo);
    f->audio = audio; f->rgbaPts = -1; f->proxPts = -1; f->brutoPts = -1;
    if (avformat_open_input(&f->fmt, arquivo, NULL, NULL) < 0) { f->falhou = true; return false; }
    avformat_find_stream_info(f->fmt, NULL);
    f->stream = av_find_best_stream(f->fmt, audio ? AVMEDIA_TYPE_AUDIO : AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (f->stream < 0) { f->falhou = true; return false; }
    AVCodecParameters* par = f->fmt->streams[f->stream]->codecpar;
    const AVCodec* cod = avcodec_find_decoder(par->codec_id);
    if (!cod) { f->falhou = true; return false; }
    f->dec = avcodec_alloc_context3(cod);
    avcodec_parameters_to_context(f->dec, par);
    f->dec->pkt_timebase = f->fmt->streams[f->stream]->time_base;
    if (!f->dec) { f->falhou = true; return false; }
    f->dec->thread_count = audio ? 1 : threads;
    if (avcodec_open2(f->dec, cod, NULL) < 0) { f->falhou = true; return false; }
    f->pacote = av_packet_alloc(); f->quadro = av_frame_alloc();
    if (!f->pacote || !f->quadro) { f->falhou = true; return false; }   // sem memória: a fonte fica de fora, sem cair
    return true;
}
static double evFontePts(EvFonte* f, int64_t pts) {
    AVRational tb = f->fmt->streams[f->stream]->time_base;
    double inicio = f->fmt->start_time != AV_NOPTS_VALUE ? f->fmt->start_time / (double)AV_TIME_BASE : 0;
    return pts == AV_NOPTS_VALUE ? -1 : pts * av_q2d(tb) - inicio;
}
static void evFonteBuscar(EvFonte* f, double t) {
    AVRational tb = f->fmt->streams[f->stream]->time_base;
    double inicio = f->fmt->start_time != AV_NOPTS_VALUE ? f->fmt->start_time / (double)AV_TIME_BASE : 0;
    int64_t alvo = (int64_t)((t + inicio) / av_q2d(tb));
    av_seek_frame(f->fmt, f->stream, alvo, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(f->dec);
    f->fim = false; f->rgbaPts = -1; f->proxPts = -1; f->brutoPts = -1; f->convertido = false;
    f->aLen = 0; f->aInicio = t;
    if (f->swr) swr_free(&f->swr);
}
// Próximo quadro decodificado (true) ou fim do arquivo (false).
static bool evFonteDecodificar(EvFonte* f) {
    for (;;) {
        int r = avcodec_receive_frame(f->dec, f->quadro);
        if (r == 0) return true;
        if (r == AVERROR_EOF) { f->fim = true; return false; }
        if (f->fim) return false;
        int lido;
        do {
            lido = av_read_frame(f->fmt, f->pacote);
            if (lido >= 0 && f->pacote->stream_index != f->stream) { av_packet_unref(f->pacote); continue; }
            break;
        } while (true);
        if (lido < 0) { avcodec_send_packet(f->dec, NULL); f->fim = true; continue; }
        avcodec_send_packet(f->dec, f->pacote);
        av_packet_unref(f->pacote);
    }
}
// Quadro decodificado que cobre o instante t (s da fonte), sem converter. rgbaPts guarda a posição
// do decodificador (último quadro lido) para decidir quando buscar.
// Rápido (prévia arrastando): até 8 quadros adiante sem buscar; depois de buscar, o primeiro quadro.
static AVFrame* evFonteQuadroBruto(EvFonte* f, double t, bool rapido = false) {
    if (f->falhou || f->audio) return NULL;
    if (!f->bruto) { f->bruto = av_frame_alloc(); f->brutoPts = -1; if (!f->bruto) { f->falhou = true; return NULL; } }
    if (f->brutoPts >= 0 && t >= f->brutoPts && t < f->brutoProx) return f->bruto;
    bool buscou = false;
    if (f->rgbaPts < 0 || t < f->rgbaPts - 0.001 || t > f->rgbaPts + 3.0) { evFonteBuscar(f, t); buscou = true; }
    int pulados = 0;
    while (evFonteDecodificar(f)) {
        double pts = evFontePts(f, f->quadro->best_effort_timestamp);
        double dur = 1.0 / 30;
#if LIBAVUTIL_VERSION_MAJOR >= 58
        int64_t duracaoQuadro = f->quadro->duration;
#else
        int64_t duracaoQuadro = f->quadro->pkt_duration;
#endif
        if (duracaoQuadro > 0) dur = duracaoQuadro * av_q2d(f->fmt->streams[f->stream]->time_base);
        if (pts < 0) pts = f->rgbaPts < 0 ? 0 : f->rgbaPts + dur;
        f->rgbaPts = pts;
        if (pts + dur <= t && !f->fim && !(rapido && (buscou || ++pulados > 8))) continue;   // ainda antes: pula
        av_frame_unref(f->bruto); av_frame_move_ref(f->bruto, f->quadro);
        f->brutoPts = pts; f->brutoProx = pts + dur; f->convertido = false;
        return f->bruto;
    }
    return f->brutoPts >= 0 ? f->bruto : NULL;   // fim do arquivo: fica no último quadro
}
// Quadro de vídeo no instante t (s da fonte), em RGBA no tamanho que cabe em largura x altura.
static const uint8_t* evFonteQuadro(EvFonte* f, double t, int largura, int altura, int* w, int* h, bool rapido = false) {
    AVFrame* q = evFonteQuadroBruto(f, t, rapido);
    if (!q || q->width <= 0 || q->height <= 0) return NULL;
    if (f->convertido && f->rgba && f->convW == largura && f->convH == altura) { *w = f->rw; *h = f->rh; return f->rgba; }
    int fw = q->width, fh = q->height;
    double esc = fmin((double)largura / fw, (double)altura / fh);
    int nw = (int)(fw * esc) & ~1, nh = (int)(fh * esc) & ~1;
    if (nw < 2) nw = 2; if (nh < 2) nh = 2;
    if (!f->rgba || f->rw != nw || f->rh != nh) {
        free(f->rgba); f->rgba = (uint8_t*)malloc((size_t)nw * nh * 4); f->rw = nw; f->rh = nh;
        if (!f->rgba) { f->rw = f->rh = 0; return NULL; }
    }
    f->sws = sws_getCachedContext(f->sws, fw, fh, (enum AVPixelFormat)q->format, nw, nh, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
    if (!f->sws) return NULL;
    uint8_t* dst[4] = { f->rgba, NULL, NULL, NULL }; int linha[4] = { nw * 4, 0, 0, 0 };
    sws_scale(f->sws, (const uint8_t* const*)q->data, q->linesize, 0, fh, dst, linha);
    f->convertido = true; f->convW = largura; f->convH = altura;
    *w = nw; *h = nh;
    return f->rgba;
}
// n amostras estéreo (float intercalado) a partir de t (s da fonte); silêncio onde não houver som.
static void evFonteAudio(EvFonte* f, double t, int n, float* saida) {
    memset(saida, 0, sizeof(float) * 2 * (size_t)n);
    if (f->falhou || !f->audio) return;
    double fimBuf = f->aInicio + f->aLen / (double)EV_TAXA_AUDIO;
    if (t < f->aInicio - 0.01 || t > fimBuf + 1.0) evFonteBuscar(f, t);
    // Descarta o que ficou para trás.
    int pular = (int)((t - f->aInicio) * EV_TAXA_AUDIO + 0.5);
    if (pular > 0) {
        if (pular >= f->aLen) { f->aInicio += f->aLen / (double)EV_TAXA_AUDIO; f->aLen = 0; }
        else { memmove(f->aBuf, f->aBuf + 2 * pular, sizeof(float) * 2 * (size_t)(f->aLen - pular)); f->aLen -= pular; f->aInicio += pular / (double)EV_TAXA_AUDIO; }
    }
    while (f->aLen < n && !f->fim) {
        if (!evFonteDecodificar(f)) break;
        double pts = evFontePts(f, f->quadro->best_effort_timestamp);
        if (!f->swr) {
#if LIBAVUTIL_VERSION_MAJOR >= 58
            AVChannelLayout saidaL; av_channel_layout_default(&saidaL, 2);
            swr_alloc_set_opts2(&f->swr, &saidaL, AV_SAMPLE_FMT_FLT, EV_TAXA_AUDIO, &f->dec->ch_layout, f->dec->sample_fmt, f->dec->sample_rate, 0, NULL);
#else
            uint64_t entrada = f->dec->channel_layout ? f->dec->channel_layout : av_get_default_channel_layout(f->dec->channels);
            f->swr = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, EV_TAXA_AUDIO, entrada, f->dec->sample_fmt, f->dec->sample_rate, 0, NULL);
#endif
            if (!f->swr || swr_init(f->swr) < 0) { f->falhou = true; return; }
            if (f->aLen == 0 && pts >= 0) f->aInicio = pts;
        }
        int cabe = swr_get_out_samples(f->swr, f->quadro->nb_samples) + 64;
        if (f->aLen + cabe > f->aCap) {
            int cap = (f->aLen + cabe) * 2; float* nb = (float*)realloc(f->aBuf, sizeof(float) * 2 * (size_t)cap);
            if (!nb) { f->falhou = true; return; }
            f->aBuf = nb; f->aCap = cap;
        }
        uint8_t* dst = (uint8_t*)(f->aBuf + 2 * f->aLen);
        int feitas = swr_convert(f->swr, &dst, cabe, (const uint8_t**)f->quadro->extended_data, f->quadro->nb_samples);
        if (feitas > 0) f->aLen += feitas;
        // Começo do áudio depois de t (o arquivo começa mais tarde): completa com silêncio na frente.
        if (f->aInicio > t + 0.001 && f->aLen > 0) {
            int vazio = (int)((f->aInicio - t) * EV_TAXA_AUDIO);
            if (f->aLen + vazio > f->aCap) {
                int cap = (f->aLen + vazio) * 2; float* nb = (float*)realloc(f->aBuf, sizeof(float) * 2 * (size_t)cap);
                if (!nb) { f->falhou = true; return; }
                f->aBuf = nb; f->aCap = cap;
            }
            memmove(f->aBuf + 2 * vazio, f->aBuf, sizeof(float) * 2 * (size_t)f->aLen);
            memset(f->aBuf, 0, sizeof(float) * 2 * (size_t)vazio);
            f->aLen += vazio; f->aInicio = t;
        }
    }
    int copiar = f->aLen < n ? f->aLen : n;
    if (copiar > 0) memcpy(saida, f->aBuf, sizeof(float) * 2 * (size_t)copiar);
}

// ------------------------------------------------------------------ cache de fontes e imagens
#define EV_MAX_FONTES 24
// Um EvRender por thread (pré-visualização, som da prévia, exportação): nada aqui é compartilhado.
struct EvRender {
    EvFonte fontes[EV_MAX_FONTES]; int relogio;
    bool rapido;   // prévia arrastando: quadro aproximado, sem decodificar até o instante exato
    int threadsVideo;   // 0 = padrão (2); a exportação usa 1 para gastar menos memória
    struct { char chave[600]; uint8_t* rgba; int w, h; } imagens[24];
    float* mixTemp; int mixCap;
};
static EvFonte* evRenderFonte(EvRender* r, const char* arquivo, bool audio, double fonteT) {
    EvFonte* livre = NULL; EvFonte* velho = NULL; EvFonte* melhor = NULL;
    for (int i = 0; i < EV_MAX_FONTES; i++) {
        EvFonte* f = &r->fontes[i];
        if (!f->arquivo[0]) { if (!livre) livre = f; continue; }
        if (f->audio == audio && !strcmp(f->arquivo, arquivo)) {
            // Mesma fonte usada em dois clipes ao mesmo tempo: prefere a que está mais perto de t.
            double posicao = audio ? f->aInicio : f->rgbaPts;
            if (!melhor || fabs(posicao - fonteT) < fabs((audio ? melhor->aInicio : melhor->rgbaPts) - fonteT)) melhor = f;
        }
        if (!velho || f->usoLru < velho->usoLru) velho = f;
    }
    if (melhor) { melhor->usoLru = ++r->relogio; return melhor; }
    EvFonte* f = livre ? livre : velho;
    if (f->arquivo[0]) evFonteFechar(f);
    evFonteAbrir(f, arquivo, audio, r->threadsVideo > 0 ? r->threadsVideo : 2);
    f->usoLru = ++r->relogio;
    return f;
}
static void evRenderLiberar(EvRender* r) {
    for (int i = 0; i < EV_MAX_FONTES; i++) if (r->fontes[i].arquivo[0]) evFonteFechar(&r->fontes[i]);
    for (int i = 0; i < 24; i++) { free(r->imagens[i].rgba); r->imagens[i].rgba = NULL; r->imagens[i].chave[0] = 0; }
    free(r->mixTemp); r->mixTemp = NULL; r->mixCap = 0;
}
// Imagem (arquivo) ou texto desenhado, com cache.
static uint8_t* evLerImagemArquivo(const char* caminho, int* w, int* h);   // definida por quem inclui (stb_image)
static const uint8_t* evRenderImagem(EvRender* r, const EvClipe* c, int alturaTela, int* w, int* h) {
    char chave[600];
    if (c->tipo == EV_CLIPE_TEXTO) snprintf(chave, sizeof(chave), "T%d|%08X|%d|%s", alturaTela, (unsigned)c->cor, c->tamanhoTexto, c->texto);
    else snprintf(chave, sizeof(chave), "I|%s", c->arquivo);
    int vago = -1;
    for (int i = 0; i < 24; i++) {
        if (r->imagens[i].rgba && !strcmp(r->imagens[i].chave, chave)) { *w = r->imagens[i].w; *h = r->imagens[i].h; return r->imagens[i].rgba; }
        if (!r->imagens[i].rgba && vago < 0) vago = i;
    }
    if (vago < 0) { vago = (r->relogio++) % 24; free(r->imagens[vago].rgba); r->imagens[vago].rgba = NULL; }
    uint8_t* img;
    if (c->tipo == EV_CLIPE_TEXTO) img = evPlatTexto(c->texto, c->tamanhoTexto * alturaTela / 1080, c->cor, w, h);
    else img = evLerImagemArquivo(c->arquivo, w, h);
    if (!img) return NULL;
    snprintf(r->imagens[vago].chave, sizeof(r->imagens[vago].chave), "%s", chave);
    r->imagens[vago].rgba = img; r->imagens[vago].w = *w; r->imagens[vago].h = *h;
    return img;
}

// ------------------------------------------------------------------ montagem do quadro
// Cola src (sw x sh, RGBA) centrado em (cx, cy), com tamanho dw x dh, girado rot graus e com
// opacidade alfa, em dst (RGBA w x h).
static void evColar(uint8_t* dst, int w, int h, const uint8_t* src, int sw, int sh, double cx, double cy, double dw, double dh, float alfa, float rot = 0) {
    if (alfa <= 0.003f || dw < 1 || dh < 1) return;
    double rad = rot * 3.14159265358979 / 180.0, cs = cos(rad), sn = sin(rad);
    bool girado = fabs(rot) > 0.05;
    // Caixa que contém o retângulo girado.
    double hw = (fabs(dw * cs) + fabs(dh * sn)) / 2, hh = (fabs(dw * sn) + fabs(dh * cs)) / 2;
    if (!girado) { hw = dw / 2; hh = dh / 2; }
    int x0 = (int)floor(cx - hw), y0 = (int)floor(cy - hh), x1 = (int)ceil(cx + hw), y1 = (int)ceil(cy + hh);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > w) x1 = w; if (y1 > h) y1 = h;
    double ex = sw / dw, ey = sh / dh;
    int a256 = (int)(alfa * 256);
    if (!girado) {
        // Sem giro: coluna de origem de cada x calculada uma vez; linha a linha só copia/mistura.
        if (x1 <= x0 || y1 <= y0) return;
        int largura = x1 - x0;
        int* colunas = (int*)malloc(sizeof(int) * (size_t)largura);
        if (!colunas) return;
        double esq = cx - dw / 2, topo = cy - dh / 2;
        for (int x = x0; x < x1; x++) { int sx = (int)((x + 0.5 - esq) * ex); colunas[x - x0] = sx < 0 ? 0 : sx >= sw ? sw - 1 : sx; }
        for (int y = y0; y < y1; y++) {
            double uy = y + 0.5 - topo;
            if (uy < 0 || uy >= dh) continue;
            int sy = (int)(uy * ey); if (sy >= sh) sy = sh - 1;
            const uint8_t* linhaSrc = src + (size_t)sy * sw * 4;
            uint8_t* d = dst + ((size_t)y * w + x0) * 4;
            for (int i = 0; i < largura; i++, d += 4) {
                const uint8_t* s = linhaSrc + colunas[i] * 4;
                int a = a256 >= 256 ? s[3] : s[3] * a256 >> 8;
                if (a >= 255) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; continue; }
                if (a <= 0) continue;
                d[0] = (uint8_t)((s[0] * a + d[0] * (255 - a)) / 255);
                d[1] = (uint8_t)((s[1] * a + d[1] * (255 - a)) / 255);
                d[2] = (uint8_t)((s[2] * a + d[2] * (255 - a)) / 255);
                d[3] = 255;
            }
        }
        free(colunas);
        return;
    }
    for (int y = y0; y < y1; y++) {
        uint8_t* d = dst + ((size_t)y * w + x0) * 4;
        double py = y + 0.5 - cy;
        for (int x = x0; x < x1; x++, d += 4) {
            double px = x + 0.5 - cx, ux = px, uy = py;
            if (girado) { ux = px * cs + py * sn; uy = -px * sn + py * cs; }   // volta para o sistema da imagem
            if (ux < -dw / 2 || ux >= dw / 2 || uy < -dh / 2 || uy >= dh / 2) continue;
            int sx = (int)((ux + dw / 2) * ex), sy = (int)((uy + dh / 2) * ey);
            if (sx < 0) sx = 0; if (sx >= sw) sx = sw - 1; if (sy < 0) sy = 0; if (sy >= sh) sy = sh - 1;
            const uint8_t* s = src + ((size_t)sy * sw + sx) * 4;
            int a = s[3] * a256 >> 8;
            if (a >= 255) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; continue; }
            if (a <= 0) continue;
            d[0] = (uint8_t)((s[0] * a + d[0] * (255 - a)) / 255);
            d[1] = (uint8_t)((s[1] * a + d[1] * (255 - a)) / 255);
            d[2] = (uint8_t)((s[2] * a + d[2] * (255 - a)) / 255);
            d[3] = 255;
        }
    }
}
// Desenha um clipe no instante tl (s desde o começo dele), com ajustes da transição:
// deslocX (fração da tela), fatorEscala e fatorAlfa.
static void evDesenharClipe(const EvClipe* c, double tl, EvRender* r, uint8_t* dst, int w, int h, double deslocX, double fatorEscala, float fatorAlfa) {
    int sw = 0, sh = 0;
    const uint8_t* img = NULL;
    if (c->tipo == EV_CLIPE_VIDEO) {
        EvFonte* f = evRenderFonte(r, c->arquivo, false, c->fonteInicio + tl);
        img = evFonteQuadro(f, c->fonteInicio + tl, w, h, &sw, &sh, r->rapido);
    }
    else if (c->tipo == EV_CLIPE_IMAGEM || c->tipo == EV_CLIPE_TEXTO) img = evRenderImagem(r, c, h, &sw, &sh);
    if (!img || sw <= 0 || sh <= 0) return;
    EvChave k = evClipeAnimacao(c, tl);
    // Tamanho base: vídeo/imagem cabem inteiros na tela; texto no tamanho natural.
    double base = c->tipo == EV_CLIPE_TEXTO ? 1.0 : fmin((double)w / sw, (double)h / sh);
    double dw = sw * base * k.escala * fatorEscala, dh = sh * base * k.escala * fatorEscala;
    evColar(dst, w, h, img, sw, sh, (k.x + deslocX) * w, k.y * h, dw, dh, k.alfa * fatorAlfa, k.rot);
}
// Pedaço que termina exatamente onde c começa, na mesma faixa (para a transição entre os dois).
static const EvClipe* evClipeAnterior(const EvFaixa* fx, const EvClipe* c) {
    for (int i = 0; i < fx->total; i++) {
        const EvClipe* a = &fx->clipes[i];
        if (a != c && fabs(evClipeFim(a) - c->inicio) < 0.05 && a->tipo != EV_CLIPE_AUDIO) return a;
    }
    return NULL;
}
// Monta o quadro do instante t no buffer RGBA w x h (fundo preto). Serve para a pré-visualização
// (w x h pequenos) e para a exportação (tamanho do projeto).
// Transição de um clipe: nos primeiros transicaoDur segundos dele, o pedaço anterior (se encostado)
// continua no último quadro e sai enquanto este entra: esmaecer cruzado, deslizar empurrando, zoom.
static void evMontarQuadro(const EvProjeto* p, EvRender* r, double t, uint8_t* dst, int w, int h) {
    for (size_t i = 0, n = (size_t)w * h; i < n; i++) { dst[i * 4] = dst[i * 4 + 1] = dst[i * 4 + 2] = 0; dst[i * 4 + 3] = 255; }
    for (int fi = 0; fi < p->totalFaixas; fi++) {
        const EvFaixa* fx = &p->faixas[fi];
        if (fx->tipo != EV_FAIXA_VISUAL || fx->oculta) continue;
        for (int ci = 0; ci < fx->total; ci++) {
            const EvClipe* c = &fx->clipes[ci];
            if (t < c->inicio || t >= evClipeFim(c)) continue;
            double tl = t - c->inicio;
            if (c->transicao != EV_TRANS_NENHUMA && c->transicaoDur > 0 && tl < c->transicaoDur) {
                double u = tl / c->transicaoDur;   // 0 -> 1
                const EvClipe* a = evClipeAnterior(fx, c);
                double tlA = a ? a->duracao - 0.04 : 0;   // último quadro do anterior
                if (c->transicao == EV_TRANS_ESMAECER) {
                    if (a) evDesenharClipe(a, tlA, r, dst, w, h, 0, 1, 1);
                    evDesenharClipe(c, tl, r, dst, w, h, 0, 1, (float)u);
                }
                else if (c->transicao == EV_TRANS_DESLIZAR_ESQ) {   // entra pela direita, empurra o anterior para a esquerda
                    if (a) evDesenharClipe(a, tlA, r, dst, w, h, -u, 1, 1);
                    evDesenharClipe(c, tl, r, dst, w, h, 1 - u, 1, 1);
                }
                else if (c->transicao == EV_TRANS_DESLIZAR_DIR) {
                    if (a) evDesenharClipe(a, tlA, r, dst, w, h, u, 1, 1);
                    evDesenharClipe(c, tl, r, dst, w, h, -(1 - u), 1, 1);
                }
                else {   // zoom
                    if (a) evDesenharClipe(a, tlA, r, dst, w, h, 0, 1 + u * 0.5, (float)(1 - u));
                    evDesenharClipe(c, tl, r, dst, w, h, 0, 0.3 + 0.7 * u, (float)u);
                }
                continue;
            }
            evDesenharClipe(c, tl, r, dst, w, h, 0, 1, 1);
        }
    }
}
// Mistura n amostras estéreo do instante t (áudio dos clipes de vídeo com som e dos clipes de áudio).
static void evMixar(const EvProjeto* p, EvRender* r, double t, int n, float* saida) {
    memset(saida, 0, sizeof(float) * 2 * (size_t)n);
    if (n > r->mixCap) { r->mixCap = n; r->mixTemp = (float*)realloc(r->mixTemp, sizeof(float) * 2 * (size_t)n); }
    float* temp = r->mixTemp;
    if (!temp) return;
    double fimJanela = t + n / (double)EV_TAXA_AUDIO;
    for (int fi = 0; fi < p->totalFaixas; fi++) {
        const EvFaixa* fx = &p->faixas[fi];
        if (fx->muda) continue;
        for (int ci = 0; ci < fx->total; ci++) {
            const EvClipe* c = &fx->clipes[ci];
            bool temSom = c->tipo == EV_CLIPE_AUDIO || (c->tipo == EV_CLIPE_VIDEO && !c->semAudio && fx->tipo == EV_FAIXA_VISUAL);
            if (!temSom || c->volume <= 0 || evClipeFim(c) <= t || c->inicio >= fimJanela) continue;
            // Parte da janela coberta pelo clipe.
            int ini = c->inicio > t ? (int)((c->inicio - t) * EV_TAXA_AUDIO) : 0;
            int fim = evClipeFim(c) < fimJanela ? (int)((evClipeFim(c) - t) * EV_TAXA_AUDIO) : n;
            if (fim <= ini) continue;
            double fonteT = c->fonteInicio + (t + ini / (double)EV_TAXA_AUDIO - c->inicio);
            EvFonte* f = evRenderFonte(r, c->arquivo, true, fonteT);
            evFonteAudio(f, fonteT, fim - ini, temp);
            for (int i = 0; i < (fim - ini) * 2; i++) saida[ini * 2 + i] += temp[i] * c->volume;
        }
    }
    for (int i = 0; i < n * 2; i++) { if (saida[i] > 1) saida[i] = 1; else if (saida[i] < -1) saida[i] = -1; }
}

// ------------------------------------------------------------------ exportação MP4
struct EvExportacao {
    volatile int progresso; volatile bool cancelar; volatile bool terminou; char erro[160]; double segundos;
    int threadsDecodificar;            // threads do decodificador de vídeo (a tela escolhe pela memória)
    volatile double quadrosPorSeg;     // velocidade medida
    volatile int64_t quadrosDiretos;   // quantos foram pelo caminho direto (sem montagem)
    double sEspera, sVideo, sAudio, sMontar, sMix;   // tempo esperando a montagem, codificando vídeo/áudio e montando (s)
};

// Codifica um quadro (NULL esvazia) e grava os pacotes. trava: o arquivo é compartilhado entre a
// thread do vídeo e a do áudio, só a gravação fica sob a trava.
static bool evCodificar(AVCodecContext* c, AVFrame* q, AVFormatContext* fmt, AVStream* st, AVPacket* pk, pthread_mutex_t* trava = NULL) {
    if (avcodec_send_frame(c, q) < 0) return false;
    for (;;) {
        int r = avcodec_receive_packet(c, pk);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) return true;
        if (r < 0) return false;
        av_packet_rescale_ts(pk, c->time_base, st->time_base);
        pk->stream_index = st->index;
        if (trava) pthread_mutex_lock(trava);
        int g = av_interleaved_write_frame(fmt, pk);
        if (trava) pthread_mutex_unlock(trava);
        if (g < 0) return false;
    }
}
// Áudio numa thread própria: o AAC do FFmpeg é lento (no PC, ~6x o tempo real) e, em série com o
// vídeo, dobrava o tempo da exportação. Fica até 2 s à frente do vídeo para o arquivo sair intercalado.
struct EvAudioLinha {
    const EvProjeto* p; EvRender* r; AVCodecContext* a; AVFormatContext* fmt; AVStream* st; AVFrame* qa; float* mix;
    int amostrasQuadro; int64_t totalAmostras; pthread_mutex_t* trava;
    volatile double tempoVideo; volatile bool videoAcabou, parar; bool ok; EvExportacao* exp;
};
static void* evAudioLaco(void* arg) {
    EvAudioLinha* A = (EvAudioLinha*)arg;
    AVPacket* pk = av_packet_alloc();
    A->ok = pk != NULL;
    for (int64_t amostra = 0; A->ok && amostra < A->totalAmostras && !A->parar; ) {
        if (!A->videoAcabou && amostra / (double)EV_TAXA_AUDIO > A->tempoVideo + 2.0) { av_usleep(2000); continue; }
        int64_t x0 = av_gettime_relative();
        evMixar(A->p, A->r, amostra / (double)EV_TAXA_AUDIO, A->amostrasQuadro, A->mix);
        A->exp->sMix += (av_gettime_relative() - x0) / 1e6;
        if (av_frame_make_writable(A->qa) < 0) { A->ok = false; break; }
        float* esq = (float*)A->qa->data[0]; float* dir = (float*)A->qa->data[1];
        for (int k = 0; k < A->amostrasQuadro; k++) { esq[k] = A->mix[2 * k]; dir[k] = A->mix[2 * k + 1]; }
        A->qa->pts = amostra;
        A->ok = evCodificar(A->a, A->qa, A->fmt, A->st, pk, A->trava);
        amostra += A->amostrasQuadro;
        A->exp->sAudio += (av_gettime_relative() - x0) / 1e6;
    }
    av_packet_free(&pk);
    return NULL;
}
// Caminho direto: no instante t só aparece um vídeo, inteiro, parado no centro e sem efeito. O quadro
// decodificado vai direto para o YUV do codificador, sem RGBA nem colagem (várias vezes mais rápido).
static bool evQuadroDireto(const EvProjeto* p, EvRender* r, double t, AVFrame* yuv, struct SwsContext** sws) {
    const EvClipe* unico = NULL;
    for (int fi = 0; fi < p->totalFaixas; fi++) {
        const EvFaixa* fx = &p->faixas[fi];
        if (fx->tipo != EV_FAIXA_VISUAL || fx->oculta) continue;
        for (int ci = 0; ci < fx->total; ci++) {
            const EvClipe* c = &fx->clipes[ci];
            if (c->tipo == EV_CLIPE_AUDIO || t < c->inicio || t >= evClipeFim(c)) continue;
            if (unico) return false;
            unico = c;
        }
    }
    if (!unico || unico->tipo != EV_CLIPE_VIDEO) return false;
    double tl = t - unico->inicio;
    if (unico->transicao != EV_TRANS_NENHUMA && tl < unico->transicaoDur) return false;
    EvChave k = evClipeAnimacao(unico, tl);
    if (fabs(k.x - 0.5) > 0.001 || fabs(k.y - 0.5) > 0.001 || fabs(k.escala - 1) > 0.001 || k.alfa < 0.999f || fabs(k.rot) > 0.01) return false;
    EvFonte* f = evRenderFonte(r, unico->arquivo, false, unico->fonteInicio + tl);
    AVFrame* q = evFonteQuadroBruto(f, unico->fonteInicio + tl);
    if (!q || q->width <= 0 || q->height <= 0) return false;
    // Formato de tela diferente teria faixas pretas: fica com a montagem normal.
    if (fabs((double)q->width / q->height - (double)yuv->width / yuv->height) > 0.01) return false;
    *sws = sws_getCachedContext(*sws, q->width, q->height, (enum AVPixelFormat)q->format, yuv->width, yuv->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    if (!*sws) return false;
    sws_scale(*sws, (const uint8_t* const*)q->data, q->linesize, 0, q->height, yuv->data, yuv->linesize);
    return true;
}
// Montagem dos quadros numa thread própria, adiantada até EV_FILA quadros: enquanto o codificador
// trabalha num quadro, o próximo já está sendo decodificado e montado.
#define EV_FILA 4
struct EvLinha {
    const EvProjeto* p; EvExportacao* exp; int W, H, fps; int64_t total;
    AVFrame* fila[EV_FILA];
    pthread_mutex_t m; pthread_cond_t c;
    int64_t produzidos, consumidos; volatile bool parar; bool falhou;
};
static void* evLinhaMontar(void* arg) {
    EvLinha* L = (EvLinha*)arg;
    EvRender* r = (EvRender*)calloc(1, sizeof(EvRender));
    uint8_t* rgba = (uint8_t*)malloc((size_t)L->W * L->H * 4);
    struct SwsContext* swsRgba = sws_getContext(L->W, L->H, AV_PIX_FMT_RGBA, L->W, L->H, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    struct SwsContext* swsDireto = NULL;
    bool ok = r && rgba && swsRgba;
    if (r) r->threadsVideo = L->exp->threadsDecodificar > 0 ? L->exp->threadsDecodificar : 2;
    for (int64_t i = 0; ok && i < L->total; i++) {
        pthread_mutex_lock(&L->m);
        while (!L->parar && L->produzidos - L->consumidos >= EV_FILA) pthread_cond_wait(&L->c, &L->m);
        bool parar = L->parar;
        pthread_mutex_unlock(&L->m);
        if (parar) break;
        AVFrame* q = L->fila[i % EV_FILA];
        if (av_frame_make_writable(q) < 0) { ok = false; break; }
        double t = i / (double)L->fps;
        int64_t m0 = av_gettime_relative();
        if (evQuadroDireto(L->p, r, t, q, &swsDireto)) L->exp->quadrosDiretos = L->exp->quadrosDiretos + 1;
        else {
            evMontarQuadro(L->p, r, t, rgba, L->W, L->H);
            const uint8_t* src[4] = { rgba, NULL, NULL, NULL }; int linha[4] = { L->W * 4, 0, 0, 0 };
            sws_scale(swsRgba, src, linha, 0, L->H, q->data, q->linesize);
        }
        L->exp->sMontar += (av_gettime_relative() - m0) / 1e6;
        pthread_mutex_lock(&L->m);
        L->produzidos = i + 1;
        pthread_cond_broadcast(&L->c);
        pthread_mutex_unlock(&L->m);
    }
    pthread_mutex_lock(&L->m);
    if (!ok) L->falhou = true;
    pthread_cond_broadcast(&L->c);
    pthread_mutex_unlock(&L->m);
    sws_freeContext(swsRgba); sws_freeContext(swsDireto); free(rgba);
    if (r) { evRenderLiberar(r); free(r); }
    return NULL;
}
// Exporta o projeto inteiro. Roda na thread que chamar (a tela mostra exp->progresso 0..1000).
static bool evExportar(const EvProjeto* p, const char* saida, EvExportacao* exp) {
    exp->progresso = 0; exp->erro[0] = 0; exp->quadrosPorSeg = 0; exp->quadrosDiretos = 0;
    double duracao = evProjetoDuracao(p);
    if (duracao <= 0.05) { snprintf(exp->erro, sizeof(exp->erro), "Projeto vazio"); return false; }
    int W = p->largura & ~1, H = p->altura & ~1, fps = p->fps;
    AVFormatContext* fmt = NULL;
    if (avformat_alloc_output_context2(&fmt, NULL, "mp4", saida) < 0 || !fmt) { snprintf(exp->erro, sizeof(exp->erro), "Sem saida MP4 neste FFmpeg"); return false; }
    const AVCodec* cv = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    const AVCodec* ca = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!cv || !ca) { snprintf(exp->erro, sizeof(exp->erro), "Sem codificador MPEG-4/AAC"); avformat_free_context(fmt); return false; }
    AVStream* sv = avformat_new_stream(fmt, NULL); AVStream* sa = avformat_new_stream(fmt, NULL);
    AVCodecContext* v = avcodec_alloc_context3(cv); AVCodecContext* a = avcodec_alloc_context3(ca);
    if (!sv || !sa || !v || !a) {
        snprintf(exp->erro, sizeof(exp->erro), "Sem memoria para exportar");
        avcodec_free_context(&v); avcodec_free_context(&a); avformat_free_context(fmt);
        return false;
    }
    v->width = W; v->height = H; v->time_base = AVRational{ 1, fps }; v->framerate = AVRational{ fps, 1 };
    v->pix_fmt = AV_PIX_FMT_YUV420P; v->gop_size = fps * 2; v->max_b_frames = 0;
    v->bit_rate = (int64_t)W * H * fps / 8 + 500000;   // ~3,5 Mbps em 720p30, ~8 em 1080p30
    v->thread_count = 3;
    a->sample_rate = EV_TAXA_AUDIO; a->sample_fmt = AV_SAMPLE_FMT_FLTP; a->bit_rate = 160000; a->time_base = AVRational{ 1, EV_TAXA_AUDIO };
#if LIBAVUTIL_VERSION_MAJOR >= 58
    av_channel_layout_default(&a->ch_layout, 2);
#else
    a->channel_layout = AV_CH_LAYOUT_STEREO; a->channels = 2;
#endif
    if (fmt->oformat->flags & AVFMT_GLOBALHEADER) { v->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; a->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; }
    bool ok = avcodec_open2(v, cv, NULL) >= 0 && avcodec_open2(a, ca, NULL) >= 0;
    if (ok) { avcodec_parameters_from_context(sv->codecpar, v); avcodec_parameters_from_context(sa->codecpar, a); sv->time_base = v->time_base; sa->time_base = a->time_base; }
    if (ok && !(fmt->oformat->flags & AVFMT_NOFILE)) ok = avio_open(&fmt->pb, saida, AVIO_FLAG_WRITE) >= 0;
    if (ok) ok = avformat_write_header(fmt, NULL) >= 0;
    if (!ok) {
        snprintf(exp->erro, sizeof(exp->erro), "Nao abriu a saida %s", saida);
        avcodec_free_context(&v); avcodec_free_context(&a);
        if (fmt->pb) avio_closep(&fmt->pb);
        avformat_free_context(fmt);
        return false;
    }
    // Linha de montagem (outra thread) e fila de quadros YUV.
    EvLinha* L = (EvLinha*)calloc(1, sizeof(EvLinha));
    EvRender* ra = (EvRender*)calloc(1, sizeof(EvRender));   // áudio, nesta thread
    bool filaOk = L && ra;
    if (L) {
        L->p = p; L->exp = exp; L->W = W; L->H = H; L->fps = fps; L->total = (int64_t)(duracao * fps + 0.5);
        for (int i = 0; i < EV_FILA && filaOk; i++) {
            AVFrame* q = av_frame_alloc();
            if (!q) { filaOk = false; break; }
            q->format = AV_PIX_FMT_YUV420P; q->width = W; q->height = H;
            if (av_frame_get_buffer(q, 0) < 0) { av_frame_free(&q); filaOk = false; break; }
            L->fila[i] = q;
        }
        pthread_mutex_init(&L->m, NULL); pthread_cond_init(&L->c, NULL);
    }
    int amostrasQuadro = a->frame_size > 0 ? a->frame_size : 1024;
    AVFrame* qa = av_frame_alloc();
    if (qa) {
        qa->format = AV_SAMPLE_FMT_FLTP; qa->nb_samples = amostrasQuadro; qa->sample_rate = EV_TAXA_AUDIO;
#if LIBAVUTIL_VERSION_MAJOR >= 58
        av_channel_layout_copy(&qa->ch_layout, &a->ch_layout);
#else
        qa->channel_layout = AV_CH_LAYOUT_STEREO; qa->channels = 2;
#endif
        if (av_frame_get_buffer(qa, 0) < 0) av_frame_free(&qa);
    }
    float* mix = (float*)malloc(sizeof(float) * 2 * (size_t)amostrasQuadro);
    AVPacket* pk = av_packet_alloc();
    pthread_t montador; bool montando = false;
    if (!filaOk || !qa || !mix || !pk) { ok = false; snprintf(exp->erro, sizeof(exp->erro), "Sem memoria para exportar"); }
    if (ok) {
        pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 1024 * 1024);
        montando = pthread_create(&montador, &at, evLinhaMontar, L) == 0;
        pthread_attr_destroy(&at);
        if (!montando) { ok = false; snprintf(exp->erro, sizeof(exp->erro), "Nao consegui iniciar a montagem"); }
    }
    int64_t totalQuadros = L ? L->total : 0;
    pthread_mutex_t trava; pthread_mutex_init(&trava, NULL);
    EvAudioLinha A; memset(&A, 0, sizeof(A));
    A.p = p; A.r = ra; A.a = a; A.fmt = fmt; A.st = sa; A.qa = qa; A.mix = mix; A.amostrasQuadro = amostrasQuadro;
    A.totalAmostras = (int64_t)(totalQuadros / (double)fps * EV_TAXA_AUDIO); A.trava = &trava; A.exp = exp; A.ok = true;
    pthread_t audioThread; bool audioRodando = false;
    if (ok) {
        pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 512 * 1024);
        audioRodando = pthread_create(&audioThread, &at, evAudioLaco, &A) == 0;
        pthread_attr_destroy(&at);
        if (!audioRodando) { ok = false; snprintf(exp->erro, sizeof(exp->erro), "Nao consegui iniciar o audio"); }
    }
    uint64_t inicioUs = (uint64_t)av_gettime_relative(), ultimoLog = inicioUs;
    for (int64_t i = 0; i < totalQuadros && ok && !exp->cancelar; i++) {
        int64_t e0 = av_gettime_relative();
        pthread_mutex_lock(&L->m);
        while (L->produzidos <= i && !L->falhou && !exp->cancelar) pthread_cond_wait(&L->c, &L->m);
        bool pronto = L->produzidos > i;
        pthread_mutex_unlock(&L->m);
        if (!pronto) { if (!exp->cancelar) { ok = false; snprintf(exp->erro, sizeof(exp->erro), "Falha ao montar os quadros"); } break; }
        AVFrame* qv = L->fila[i % EV_FILA];
        qv->pts = i;
        int64_t c0 = av_gettime_relative(); exp->sEspera += (c0 - e0) / 1e6;
        ok = evCodificar(v, qv, fmt, sv, pk, &trava);
        exp->sVideo += (av_gettime_relative() - c0) / 1e6;
        pthread_mutex_lock(&L->m);
        L->consumidos = i + 1;
        pthread_cond_broadcast(&L->c);
        pthread_mutex_unlock(&L->m);
        A.tempoVideo = (i + 1) / (double)fps;
        if (!A.ok) { ok = false; if (!exp->erro[0]) snprintf(exp->erro, sizeof(exp->erro), "Falha ao codificar o audio"); }
        exp->progresso = (int)((i + 1) * 1000 / totalQuadros);
        uint64_t agora = (uint64_t)av_gettime_relative();
        if (agora > inicioUs) exp->quadrosPorSeg = (i + 1) / ((agora - inicioUs) / 1e6);
        if (agora - ultimoLog > 30000000) {
            ultimoLog = agora;
            evPlatLog("EXPORTAR %lld/%lld quadros, %.1f q/s, diretos %lld | montar %.0f s, espera %.0f s, video %.0f s, audio %.0f s", (long long)(i + 1), (long long)totalQuadros, (double)exp->quadrosPorSeg, (long long)exp->quadrosDiretos, exp->sMontar, exp->sEspera, exp->sVideo, exp->sAudio);
        }
    }
    // O áudio termina o que falta (ou para, se cancelou ou falhou).
    A.videoAcabou = true;
    if (!ok || exp->cancelar) A.parar = true;
    if (audioRodando) pthread_join(audioThread, NULL);
    if (ok && !A.ok) { ok = false; snprintf(exp->erro, sizeof(exp->erro), "Falha ao codificar o audio"); }
    pthread_mutex_destroy(&trava);
    // Para a montagem antes de soltar a fila.
    if (montando) {
        pthread_mutex_lock(&L->m); L->parar = true; pthread_cond_broadcast(&L->c); pthread_mutex_unlock(&L->m);
        pthread_join(montador, NULL);
    }
    if (exp->cancelar) { ok = false; snprintf(exp->erro, sizeof(exp->erro), "Cancelado"); }   // sem esvaziar o codificador nem gravar o final
    else if (ok) { evCodificar(v, NULL, fmt, sv, pk); evCodificar(a, NULL, fmt, sa, pk); av_write_trailer(fmt); }
    else if (!exp->erro[0]) snprintf(exp->erro, sizeof(exp->erro), "Falha ao codificar");
    exp->segundos = duracao;
    evPlatLog("EXPORTAR liberando (%s) %.1f q/s, diretos %lld | montar %.1f s, espera %.1f s, video %.1f s, audio %.1f s (mistura %.1f s)", exp->erro[0] ? exp->erro : "ok", (double)exp->quadrosPorSeg, (long long)exp->quadrosDiretos, exp->sMontar, exp->sEspera, exp->sVideo, exp->sAudio, exp->sMix);
    av_packet_free(&pk); free(mix); av_frame_free(&qa);
    if (L) {
        for (int i = 0; i < EV_FILA; i++) av_frame_free(&L->fila[i]);
        pthread_mutex_destroy(&L->m); pthread_cond_destroy(&L->c);
        free(L);
    }
    avcodec_free_context(&v); avcodec_free_context(&a);
    if (fmt->pb) avio_closep(&fmt->pb);
    avformat_free_context(fmt);
    if (ra) { evRenderLiberar(ra); free(ra); }
    if (!ok || exp->cancelar) remove(saida);
    evPlatLog("EXPORTAR liberou tudo");
    return ok && !exp->cancelar;
}
