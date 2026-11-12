// =========================================================================
// EDITOR DE IMAGEM: núcleo (documento, camadas, desfazer, ajustes, filtros,
// desenho, formas, texto, seleção e arquivos).
// Incluído pelo network.cpp (PS4) e pelo teste do PC (ferramentas/editor_teste).
// Pixels guardados como 0xAARRGGBB, o mesmo formato da tela.
// Precisa, antes do include: stb_image, stb_image_write, stb_truetype (só declarações),
// miniz.h e as funções edAgoraMs().
// =========================================================================
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ED_MAX_CAMADAS 12
#define ED_MAX_LADO 2048
#define ED_UNDO_BYTES ((size_t)96 * 1024 * 1024)
#define ED_UNDO_MAX 40

static inline int edLim(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static inline float edLimF(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline uint8_t edB(float v) { return v <= 0 ? 0 : v >= 255 ? 255 : (uint8_t)(v + 0.5f); }
#define ED_A(c) ((uint32_t)(c) >> 24)
#define ED_R(c) (((uint32_t)(c) >> 16) & 255)
#define ED_G(c) (((uint32_t)(c) >> 8) & 255)
#define ED_BL(c) ((uint32_t)(c) & 255)
static inline uint32_t edCor(int r, int g, int b, int a) { return ((uint32_t)edLim(a, 0, 255) << 24) | ((uint32_t)edLim(r, 0, 255) << 16) | ((uint32_t)edLim(g, 0, 255) << 8) | (uint32_t)edLim(b, 0, 255); }

// ---------------------------------------------------------------- documento
enum { ED_MIS_NORMAL, ED_MIS_MULTIPLICAR, ED_MIS_TELA, ED_MIS_SOBREPOR, ED_MIS_CLAREAR, ED_MIS_ESCURECER, ED_MIS_ADICIONAR, ED_MIS_DIFERENCA, ED_MIS_TOTAL };
static const char* const edNomesMistura[ED_MIS_TOTAL] = { "Normal", "Multiplicar", "Tela", "Sobrepor", "Clarear", "Escurecer", "Adicionar", "Diferenca" };

struct EdCamada { uint32_t* px; bool visivel; int opacidade; int mistura; char nome[40]; };
struct EdDoc {
    int w, h, n, ativa;
    EdCamada cam[ED_MAX_CAMADAS];
    uint32_t* comp;          // tudo misturado, para mostrar e salvar
    bool compSujo;
    uint8_t* sel;            // seleção (0..255); NULL = sem seleção
    uint8_t* traco;          // máscara de trabalho (pincel, formas, balde)
    uint32_t* base;          // a camada como estava no começo do traço
    bool mudou;
    unsigned versao;         // muda a cada alteração (prévias e miniaturas)
    char caminho[512];
};
static EdDoc ed;

static void edLiberarDoc() {
    for (int i = 0; i < ed.n; i++) free(ed.cam[i].px);
    free(ed.comp); free(ed.sel); free(ed.traco); free(ed.base);
    memset(&ed, 0, sizeof(ed));
}
static bool edNovoDoc(int w, int h, uint32_t fundo) {
    edLiberarDoc();
    w = edLim(w, 1, ED_MAX_LADO); h = edLim(h, 1, ED_MAX_LADO);
    size_t n = (size_t)w * h;
    ed.w = w; ed.h = h;
    ed.comp = (uint32_t*)malloc(n * 4);
    ed.traco = (uint8_t*)calloc(n, 1);
    ed.base = (uint32_t*)malloc(n * 4);
    ed.cam[0].px = (uint32_t*)malloc(n * 4);
    if (!ed.comp || !ed.traco || !ed.base || !ed.cam[0].px) { edLiberarDoc(); return false; }
    for (size_t i = 0; i < n; i++) ed.cam[0].px[i] = fundo;
    ed.cam[0].visivel = true; ed.cam[0].opacidade = 100; ed.cam[0].mistura = ED_MIS_NORMAL;
    snprintf(ed.cam[0].nome, sizeof(ed.cam[0].nome), "Fundo");
    ed.n = 1; ed.ativa = 0; ed.compSujo = true; ed.versao = 1;
    return true;
}
static uint32_t* edAtiva() { return ed.cam[ed.ativa].px; }
static void edSujou() { ed.compSujo = true; ed.mudou = true; ed.versao++; }

// Mistura de uma camada sobre o que já está embaixo (cores sem pré-multiplicar).
static inline float edMisturaCanal(int modo, float b, float s) {
    switch (modo) {
    case ED_MIS_MULTIPLICAR: return b * s;
    case ED_MIS_TELA: return 1 - (1 - b) * (1 - s);
    case ED_MIS_SOBREPOR: return b < 0.5f ? 2 * b * s : 1 - 2 * (1 - b) * (1 - s);
    case ED_MIS_CLAREAR: return b > s ? b : s;
    case ED_MIS_ESCURECER: return b < s ? b : s;
    case ED_MIS_ADICIONAR: return b + s > 1 ? 1 : b + s;
    case ED_MIS_DIFERENCA: return fabsf(b - s);
    default: return s;
    }
}
static inline uint32_t edSobre(uint32_t d, uint32_t s, int opacidade, int modo) {
    uint32_t sa = ED_A(s) * (uint32_t)opacidade / 100;
    if (sa == 0) return d;
    uint32_t da = ED_A(d);
    if (modo == ED_MIS_NORMAL) {
        if (sa == 255 || da == 0) return (s & 0x00FFFFFF) | (sa << 24);
        float as = sa / 255.0f, ab = da / 255.0f, ao = as + ab * (1 - as);
        float r = (ED_R(s) * as + ED_R(d) * ab * (1 - as)) / ao;
        float g = (ED_G(s) * as + ED_G(d) * ab * (1 - as)) / ao;
        float b = (ED_BL(s) * as + ED_BL(d) * ab * (1 - as)) / ao;
        return edCor((int)(r + .5f), (int)(g + .5f), (int)(b + .5f), (int)(ao * 255 + .5f));
    }
    float as = sa / 255.0f, ab = da / 255.0f, ao = as + ab * (1 - as);
    float cs[3] = { ED_R(s) / 255.0f, ED_G(s) / 255.0f, ED_BL(s) / 255.0f };
    float cb[3] = { ED_R(d) / 255.0f, ED_G(d) / 255.0f, ED_BL(d) / 255.0f };
    int o[3];
    for (int k = 0; k < 3; k++) {
        float c = (1 - ab) * cs[k] + ab * edMisturaCanal(modo, cb[k], cs[k]);
        o[k] = (int)((c * as + cb[k] * ab * (1 - as)) / ao * 255 + .5f);
    }
    return edCor(o[0], o[1], o[2], (int)(ao * 255 + .5f));
}
static void edComporRet(int x0, int y0, int x1, int y1) {
    x0 = edLim(x0, 0, ed.w); x1 = edLim(x1, 0, ed.w); y0 = edLim(y0, 0, ed.h); y1 = edLim(y1, 0, ed.h);
    if (x0 >= x1 || y0 >= y1) return;
    bool simples = ed.n == 1 && ed.cam[0].visivel && ed.cam[0].opacidade == 100;
    for (int y = y0; y < y1; y++) {
        uint32_t* o = ed.comp + (size_t)y * ed.w;
        if (simples) { memcpy(o + x0, ed.cam[0].px + (size_t)y * ed.w + x0, (size_t)(x1 - x0) * 4); continue; }
        for (int x = x0; x < x1; x++) o[x] = 0;
        for (int c = 0; c < ed.n; c++) {
            const EdCamada* L = &ed.cam[c];
            if (!L->visivel || L->opacidade <= 0) continue;
            const uint32_t* s = L->px + (size_t)y * ed.w;
            for (int x = x0; x < x1; x++) o[x] = edSobre(o[x], s[x], L->opacidade, L->mistura);
        }
    }
}
static void edCompor() { if (!ed.compSujo) return; edComporRet(0, 0, ed.w, ed.h); ed.compSujo = false; }

// ---------------------------------------------------------------- desfazer / refazer
struct EdPasso { int tipo; int camada; int w, h, n, ativa; EdCamada meta[ED_MAX_CAMADAS]; uint32_t* px[ED_MAX_CAMADAS]; size_t bytes; };
static EdPasso edDesfazer[ED_UNDO_MAX], edRefazer[ED_UNDO_MAX];
static int edTotalDesfazer = 0, edTotalRefazer = 0;

static void edPassoLiberar(EdPasso* p) { for (int i = 0; i < ED_MAX_CAMADAS; i++) { free(p->px[i]); p->px[i] = NULL; } p->bytes = 0; }
// tipo 0: só a camada ativa; tipo 1: o documento todo (cortar, girar, camadas).
static bool edPassoTirar(EdPasso* p, int tipo) {
    memset(p, 0, sizeof(*p));
    p->tipo = tipo; p->camada = ed.ativa; p->w = ed.w; p->h = ed.h; p->n = ed.n; p->ativa = ed.ativa;
    size_t n = (size_t)ed.w * ed.h;
    for (int i = 0; i < ed.n; i++) {
        p->meta[i] = ed.cam[i]; p->meta[i].px = NULL;
        if (tipo == 0 && i != ed.ativa) continue;
        p->px[i] = (uint32_t*)malloc(n * 4);
        if (!p->px[i]) { edPassoLiberar(p); return false; }
        memcpy(p->px[i], ed.cam[i].px, n * 4);
        p->bytes += n * 4;
    }
    return true;
}
static void edPassoAplicar(EdPasso* p) {
    if (p->tipo == 0) {
        if (p->camada < ed.n && p->px[p->camada] && p->w == ed.w && p->h == ed.h) {
            memcpy(ed.cam[p->camada].px, p->px[p->camada], (size_t)ed.w * ed.h * 4);
            ed.cam[p->camada].visivel = p->meta[p->camada].visivel;
            ed.cam[p->camada].opacidade = p->meta[p->camada].opacidade;
            ed.cam[p->camada].mistura = p->meta[p->camada].mistura;
        }
    }
    else {
        for (int i = 0; i < ed.n; i++) free(ed.cam[i].px);
        bool tamanho = p->w != ed.w || p->h != ed.h;
        ed.w = p->w; ed.h = p->h; ed.n = p->n; ed.ativa = p->ativa;
        for (int i = 0; i < ed.n; i++) { ed.cam[i] = p->meta[i]; ed.cam[i].px = p->px[i]; p->px[i] = NULL; }
        if (tamanho) {
            size_t n = (size_t)ed.w * ed.h;
            free(ed.comp); free(ed.traco); free(ed.base); free(ed.sel); ed.sel = NULL;
            ed.comp = (uint32_t*)malloc(n * 4); ed.traco = (uint8_t*)calloc(n, 1); ed.base = (uint32_t*)malloc(n * 4);
        }
    }
    ed.ativa = edLim(ed.ativa, 0, ed.n - 1);
    edSujou();
}
static size_t edBytesDesfazer() { size_t t = 0; for (int i = 0; i < edTotalDesfazer; i++) t += edDesfazer[i].bytes; for (int i = 0; i < edTotalRefazer; i++) t += edRefazer[i].bytes; return t; }
static void edEmpurrar(EdPasso* pilha, int* total, EdPasso* p) {
    if (*total == ED_UNDO_MAX) { edPassoLiberar(&pilha[0]); memmove(pilha, pilha + 1, sizeof(EdPasso) * (ED_UNDO_MAX - 1)); (*total)--; }
    pilha[(*total)++] = *p;
}
// Chame ANTES de mudar a imagem.
static void edGuardarPasso(int tipo) {
    for (int i = 0; i < edTotalRefazer; i++) edPassoLiberar(&edRefazer[i]);
    edTotalRefazer = 0;
    EdPasso p;
    if (!edPassoTirar(&p, tipo)) return;
    edEmpurrar(edDesfazer, &edTotalDesfazer, &p);
    while (edBytesDesfazer() > ED_UNDO_BYTES && edTotalDesfazer > 1) {
        edPassoLiberar(&edDesfazer[0]);
        memmove(edDesfazer, edDesfazer + 1, sizeof(EdPasso) * (edTotalDesfazer - 1));
        edTotalDesfazer--;
    }
}
static bool edDesfazerUm() {
    if (!edTotalDesfazer) return false;
    EdPasso p = edDesfazer[--edTotalDesfazer];
    EdPasso agora;
    int ativaAntes = ed.ativa;
    if (p.tipo == 0) ed.ativa = p.camada;
    if (edPassoTirar(&agora, p.tipo)) edEmpurrar(edRefazer, &edTotalRefazer, &agora);
    ed.ativa = ativaAntes;
    edPassoAplicar(&p);
    edPassoLiberar(&p);
    return true;
}
static bool edRefazerUm() {
    if (!edTotalRefazer) return false;
    EdPasso p = edRefazer[--edTotalRefazer];
    EdPasso agora;
    int ativaAntes = ed.ativa;
    if (p.tipo == 0) ed.ativa = p.camada;
    if (edPassoTirar(&agora, p.tipo)) edEmpurrar(edDesfazer, &edTotalDesfazer, &agora);
    ed.ativa = ativaAntes;
    edPassoAplicar(&p);
    edPassoLiberar(&p);
    return true;
}
static void edLimparHistorico() {
    for (int i = 0; i < edTotalDesfazer; i++) edPassoLiberar(&edDesfazer[i]);
    for (int i = 0; i < edTotalRefazer; i++) edPassoLiberar(&edRefazer[i]);
    edTotalDesfazer = edTotalRefazer = 0;
}

// ---------------------------------------------------------------- camadas
static bool edNovaCamada(const char* nome, uint32_t preencher) {
    if (ed.n >= ED_MAX_CAMADAS) return false;
    uint32_t* px = (uint32_t*)malloc((size_t)ed.w * ed.h * 4);
    if (!px) return false;
    for (size_t i = 0, n = (size_t)ed.w * ed.h; i < n; i++) px[i] = preencher;
    int pos = ed.ativa + 1;
    memmove(&ed.cam[pos + 1], &ed.cam[pos], sizeof(EdCamada) * (ed.n - pos));
    ed.cam[pos].px = px; ed.cam[pos].visivel = true; ed.cam[pos].opacidade = 100; ed.cam[pos].mistura = ED_MIS_NORMAL;
    snprintf(ed.cam[pos].nome, sizeof(ed.cam[pos].nome), "%s", nome);
    ed.n++; ed.ativa = pos;
    edSujou();
    return true;
}
static bool edDuplicarCamada() {
    if (ed.n >= ED_MAX_CAMADAS) return false;
    EdCamada c = ed.cam[ed.ativa];
    char nome[40]; snprintf(nome, sizeof(nome), "%.30s copia", c.nome);
    if (!edNovaCamada(nome, 0)) return false;
    memcpy(edAtiva(), c.px, (size_t)ed.w * ed.h * 4);
    ed.cam[ed.ativa].opacidade = c.opacidade; ed.cam[ed.ativa].mistura = c.mistura;
    return true;
}
static void edApagarCamada() {
    if (ed.n <= 1) return;
    free(ed.cam[ed.ativa].px);
    memmove(&ed.cam[ed.ativa], &ed.cam[ed.ativa + 1], sizeof(EdCamada) * (ed.n - ed.ativa - 1));
    ed.n--; if (ed.ativa >= ed.n) ed.ativa = ed.n - 1;
    edSujou();
}
static void edMoverCamada(int passo) {
    int d = ed.ativa + passo;
    if (d < 0 || d >= ed.n) return;
    EdCamada t = ed.cam[d]; ed.cam[d] = ed.cam[ed.ativa]; ed.cam[ed.ativa] = t;
    ed.ativa = d;
    edSujou();
}
static void edMesclarAbaixo() {
    if (ed.ativa == 0) return;
    EdCamada* s = &ed.cam[ed.ativa]; EdCamada* b = &ed.cam[ed.ativa - 1];
    size_t n = (size_t)ed.w * ed.h;
    if (s->visivel) for (size_t i = 0; i < n; i++) b->px[i] = edSobre(b->px[i], s->px[i], s->opacidade, s->mistura);
    edApagarCamada();
    ed.ativa = ed.ativa > 0 ? ed.ativa : 0;
}
static void edAchatar() {
    edCompor();
    for (int i = 1; i < ed.n; i++) free(ed.cam[i].px);
    memcpy(ed.cam[0].px, ed.comp, (size_t)ed.w * ed.h * 4);
    ed.cam[0].visivel = true; ed.cam[0].opacidade = 100; ed.cam[0].mistura = ED_MIS_NORMAL;
    ed.n = 1; ed.ativa = 0;
    edSujou();
}

// ---------------------------------------------------------------- arquivos
static uint32_t* edRgbaParaInterno(const unsigned char* rgba, int w, int h) {
    uint32_t* o = (uint32_t*)malloc((size_t)w * h * 4);
    if (!o) return NULL;
    for (size_t i = 0, n = (size_t)w * h; i < n; i++) o[i] = edCor(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]);
    return o;
}
// Reduz (média de área) para caber em lado x lado.
static uint32_t* edReduzir(const uint32_t* src, int w, int h, int nw, int nh) {
    uint32_t* o = (uint32_t*)malloc((size_t)nw * nh * 4);
    if (!o) return NULL;
    for (int y = 0; y < nh; y++) {
        int y0 = (int)((long long)y * h / nh), y1 = (int)((long long)(y + 1) * h / nh); if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < nw; x++) {
            int x0 = (int)((long long)x * w / nw), x1 = (int)((long long)(x + 1) * w / nw); if (x1 <= x0) x1 = x0 + 1;
            uint32_t sr = 0, sg = 0, sb = 0, sa = 0, cnt = 0;
            int pa = (y1 - y0) * (x1 - x0) > 64 ? 2 : 1;   // amostra 1 de cada 4 em áreas grandes
            for (int yy = y0; yy < y1; yy += pa) for (int xx = x0; xx < x1; xx += pa) {
                uint32_t c = src[(size_t)yy * w + xx], a = ED_A(c);
                sr += ED_R(c) * a; sg += ED_G(c) * a; sb += ED_BL(c) * a; sa += a; cnt++;
            }
            o[(size_t)y * nw + x] = sa ? edCor(sr / sa, sg / sa, sb / sa, sa / cnt) : 0;
        }
    }
    return o;
}
static bool edAbrirImagem(const char* caminho) {
    int w = 0, h = 0, c = 0;
    unsigned char* d = stbi_load(caminho, &w, &h, &c, 4);
    if (!d) return false;
    uint32_t* px = edRgbaParaInterno(d, w, h);
    stbi_image_free(d);
    if (!px) return false;
    if (w > ED_MAX_LADO || h > ED_MAX_LADO) {
        float f = fminf((float)ED_MAX_LADO / w, (float)ED_MAX_LADO / h);
        int nw = (int)(w * f), nh = (int)(h * f);
        uint32_t* r = edReduzir(px, w, h, nw, nh);
        free(px);
        if (!r) return false;
        px = r; w = nw; h = nh;
    }
    if (!edNovoDoc(w, h, 0)) { free(px); return false; }
    free(ed.cam[0].px);
    ed.cam[0].px = px;
    snprintf(ed.cam[0].nome, sizeof(ed.cam[0].nome), "Imagem");
    snprintf(ed.caminho, sizeof(ed.caminho), "%s", caminho);
    ed.mudou = false;
    edLimparHistorico();
    return true;
}
static bool edSalvarPng(const char* caminho, const uint32_t* px, int w, int h) {
    unsigned char* o = (unsigned char*)malloc((size_t)w * h * 4);
    if (!o) return false;
    for (size_t i = 0, n = (size_t)w * h; i < n; i++) { uint32_t c = px[i]; o[i * 4] = ED_R(c); o[i * 4 + 1] = ED_G(c); o[i * 4 + 2] = ED_BL(c); o[i * 4 + 3] = ED_A(c); }
    int ok = stbi_write_png(caminho, w, h, 4, o, w * 4);
    free(o);
    return ok != 0;
}
static bool edSalvarJpg(const char* caminho, const uint32_t* px, int w, int h, int qualidade) {
    unsigned char* o = (unsigned char*)malloc((size_t)w * h * 3);
    if (!o) return false;
    for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
        uint32_t c = px[i], a = ED_A(c);   // transparente vira branco
        o[i * 3] = (uint8_t)((ED_R(c) * a + 255 * (255 - a)) / 255);
        o[i * 3 + 1] = (uint8_t)((ED_G(c) * a + 255 * (255 - a)) / 255);
        o[i * 3 + 2] = (uint8_t)((ED_BL(c) * a + 255 * (255 - a)) / 255);
    }
    int ok = stbi_write_jpg(caminho, w, h, 3, o, qualidade);
    free(o);
    return ok != 0;
}
// Projeto .hnimg: "HNIMG1", w, h, n, ativa e, por camada, metadados e pixels comprimidos.
static bool edSalvarProjeto(const char* caminho) {
    FILE* f = fopen(caminho, "wb");
    if (!f) return false;
    int32_t cab[4] = { ed.w, ed.h, ed.n, ed.ativa };
    fwrite("HNIMG1", 1, 6, f); fwrite(cab, 4, 4, f);
    size_t n = (size_t)ed.w * ed.h * 4;
    mz_ulong cap = mz_compressBound((mz_ulong)n);
    unsigned char* z = (unsigned char*)malloc(cap);
    bool ok = z != NULL;
    for (int i = 0; ok && i < ed.n; i++) {
        EdCamada m = ed.cam[i];
        int32_t meta[3] = { m.visivel, m.opacidade, m.mistura };
        fwrite(m.nome, 1, 40, f); fwrite(meta, 4, 3, f);
        mz_ulong zt = cap;
        ok = mz_compress2(z, &zt, (const unsigned char*)m.px, (mz_ulong)n, 6) == MZ_OK;
        uint32_t t = (uint32_t)zt;
        if (ok) { fwrite(&t, 4, 1, f); fwrite(z, 1, zt, f); }
    }
    free(z);
    if (fclose(f) != 0) ok = false;
    if (!ok) remove(caminho);
    return ok;
}
static bool edAbrirProjeto(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return false;
    char mag[6]; int32_t cab[4];
    bool ok = fread(mag, 1, 6, f) == 6 && !memcmp(mag, "HNIMG1", 6) && fread(cab, 4, 4, f) == 4 &&
        cab[0] > 0 && cab[1] > 0 && cab[0] <= ED_MAX_LADO && cab[1] <= ED_MAX_LADO && cab[2] >= 1 && cab[2] <= ED_MAX_CAMADAS;
    if (ok) ok = edNovoDoc(cab[0], cab[1], 0);
    size_t n = (size_t)cab[0] * cab[1] * 4;
    for (int i = 0; ok && i < cab[2]; i++) {
        if (i > 0) ok = edNovaCamada("", 0);
        if (!ok) break;
        EdCamada* m = &ed.cam[i];
        int32_t meta[3]; uint32_t t = 0;
        ok = fread(m->nome, 1, 40, f) == 40 && fread(meta, 4, 3, f) == 3 && fread(&t, 4, 1, f) == 1 && t > 0 && t < 0x40000000u;
        m->nome[39] = 0;
        if (!ok) break;
        m->visivel = meta[0] != 0; m->opacidade = edLim(meta[1], 0, 100); m->mistura = edLim(meta[2], 0, ED_MIS_TOTAL - 1);
        unsigned char* z = (unsigned char*)malloc(t);
        ok = z && fread(z, 1, t, f) == t;
        mz_ulong dn = (mz_ulong)n;
        if (ok) ok = mz_uncompress((unsigned char*)m->px, &dn, z, t) == MZ_OK && dn == n;
        free(z);
    }
    fclose(f);
    if (!ok) { edLiberarDoc(); return false; }
    ed.ativa = edLim(cab[3], 0, ed.n - 1);
    snprintf(ed.caminho, sizeof(ed.caminho), "%s", caminho);
    ed.mudou = false; edSujou(); ed.mudou = false;
    edLimparHistorico();
    return true;
}

// ---------------------------------------------------------------- cor: HSL / HSV
static void edRgbHsl(float r, float g, float b, float* h, float* s, float* l) {
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
    *l = (mx + mn) / 2;
    if (mx == mn) { *h = 0; *s = 0; return; }
    float d = mx - mn;
    *s = *l > 0.5f ? d / (2 - mx - mn) : d / (mx + mn);
    if (mx == r) *h = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) *h = (b - r) / d + 2;
    else *h = (r - g) / d + 4;
    *h *= 60;
}
static float edHue2(float p, float q, float t) {
    if (t < 0) t += 1; if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}
static void edHslRgb(float h, float s, float l, float* r, float* g, float* b) {
    if (s <= 0) { *r = *g = *b = l; return; }
    h = fmodf(h, 360); if (h < 0) h += 360; h /= 360;
    float q = l < 0.5f ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
    *r = edHue2(p, q, h + 1.0f / 3); *g = edHue2(p, q, h); *b = edHue2(p, q, h - 1.0f / 3);
}
static uint32_t edHsvCor(float h, float s, float v, int a) {
    h = fmodf(h, 360); if (h < 0) h += 360;
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = v - c, r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; } else if (h < 120) { r = x; g = c; } else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; } else if (h < 300) { r = x; b = c; } else { r = c; b = x; }
    return edCor((int)((r + m) * 255 + .5f), (int)((g + m) * 255 + .5f), (int)((b + m) * 255 + .5f), a);
}
static void edCorHsv(uint32_t c, int* h, int* s, int* v) {
    float r = ED_R(c) / 255.0f, g = ED_G(c) / 255.0f, b = ED_BL(c) / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn, hh = 0;
    if (d > 0) { if (mx == r) hh = fmodf((g - b) / d, 6); else if (mx == g) hh = (b - r) / d + 2; else hh = (r - g) / d + 4; hh *= 60; if (hh < 0) hh += 360; }
    *h = (int)(hh + .5f) % 360; *s = mx > 0 ? (int)(d / mx * 100 + .5f) : 0; *v = (int)(mx * 100 + .5f);
}

// ---------------------------------------------------------------- ajustes e filtros
enum { ED_ESP_NENHUM, ED_ESP_SEPIA, ED_ESP_PB, ED_ESP_INVERTER, ED_ESP_NEON, ED_ESP_ANIME, ED_ESP_BRILHO_SUAVE, ED_ESP_DESENHO };
struct EdAjustes {
    int brilho, contraste, exposicao, realces, sombras, claros, escuros;
    int saturacao, vibracao, calor, tom, colorir, colorirMatiz;
    int curva[4][5];          // geral, R, G, B; pontos 0/25/50/75/100%; deslocamento -100..100
    int hsl[8][3];            // vermelho, laranja, amarelo, verde, ciano, azul, roxo, magenta
    int clareza, nitidez, desfoque, ruido, removerRuido;
    int vinheta, posterizar, dispersao, pixelizar;
    int especial, especialForca;
};
static bool edAjNeutro(const EdAjustes* a) {
    static const EdAjustes zero = {};
    EdAjustes t = *a; t.colorirMatiz = 0;
    if (!t.especial || !t.especialForca) { t.especial = 0; t.especialForca = 0; }
    return !memcmp(&t, &zero, sizeof(t));
}

// Desfoque de caixa separável (2 passadas ~ gaussiano), alfa pré-multiplicado, somas corridas.
// Buffers de trabalho reaproveitados: alocar 30 MB novos a cada desfoque custava mais que a conta.
static int32_t* edTrabA = NULL; static int32_t* edTrabB = NULL; static size_t edTrabCap = 0;
static bool edTrabalho(size_t n) {
    if (n <= edTrabCap) return true;
    free(edTrabA); free(edTrabB);
    edTrabA = (int32_t*)malloc(n * 4 * sizeof(int32_t)); edTrabB = (int32_t*)malloc(n * 4 * sizeof(int32_t));
    if (!edTrabA || !edTrabB) { free(edTrabA); free(edTrabB); edTrabA = edTrabB = NULL; edTrabCap = 0; return false; }
    edTrabCap = n;
    return true;
}
static void edLiberarTrabalho() { free(edTrabA); free(edTrabB); edTrabA = edTrabB = NULL; edTrabCap = 0; }
static void edDesfocarCaixa(uint32_t* px, int w, int h, int r) {
    if (r < 1 || w < 1 || h < 1) return;
    if (r > w && r > h) r = w > h ? w : h;
    size_t n = (size_t)w * h;
    if (!edTrabalho(n)) return;
    int32_t* q = edTrabA; int32_t* outro = edTrabB;
    int m = w > h ? w : h;
    int32_t* tmp = (int32_t*)malloc((size_t)m * 4 * sizeof(int32_t));
    int32_t* som = (int32_t*)malloc((size_t)w * 4 * sizeof(int32_t));
    if (!tmp || !som) { free(tmp); free(som); return; }
    for (size_t i = 0; i < n; i++) {
        uint32_t c = px[i]; int a = ED_A(c);
        q[i * 4] = ED_R(c) * a; q[i * 4 + 1] = ED_G(c) * a; q[i * 4 + 2] = ED_BL(c) * a; q[i * 4 + 3] = a * 255;
    }
    int janela = 2 * r + 1;
    int64_t inv = ((int64_t)1 << 24) / janela;
    for (int passada = 0; passada < 2; passada++) {
        // horizontal
        for (int y = 0; y < h; y++) {
            int32_t* L = q + (size_t)y * w * 4;
            int32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
            for (int x = -r; x <= r; x++) { int xx = x < 0 ? 0 : x >= w ? w - 1 : x; s0 += L[xx * 4]; s1 += L[xx * 4 + 1]; s2 += L[xx * 4 + 2]; s3 += L[xx * 4 + 3]; }
            for (int x = 0; x < w; x++) {
                tmp[x * 4] = (int32_t)((s0 * inv) >> 24); tmp[x * 4 + 1] = (int32_t)((s1 * inv) >> 24); tmp[x * 4 + 2] = (int32_t)((s2 * inv) >> 24); tmp[x * 4 + 3] = (int32_t)((s3 * inv) >> 24);
                int xs = x - r < 0 ? 0 : x - r, xe = x + r + 1 >= w ? w - 1 : x + r + 1;
                s0 += L[xe * 4] - L[xs * 4]; s1 += L[xe * 4 + 1] - L[xs * 4 + 1]; s2 += L[xe * 4 + 2] - L[xs * 4 + 2]; s3 += L[xe * 4 + 3] - L[xs * 4 + 3];
            }
            memcpy(L, tmp, (size_t)w * 4 * sizeof(int32_t));
        }
        // vertical: soma de todas as colunas, linha a linha (bom para o cache)
        memset(som, 0, (size_t)w * 4 * sizeof(int32_t));
        for (int y = -r; y <= r; y++) { const int32_t* L = q + (size_t)(y < 0 ? 0 : y >= h ? h - 1 : y) * w * 4; for (int k = 0; k < w * 4; k++) som[k] += L[k]; }
        int32_t* saida = outro;
        for (int y = 0; y < h; y++) {
            int32_t* o = saida + (size_t)y * w * 4;
            for (int k = 0; k < w * 4; k++) o[k] = (int32_t)((som[k] * inv) >> 24);
            const int32_t* sai = q + (size_t)(y - r < 0 ? 0 : y - r) * w * 4;
            const int32_t* entra = q + (size_t)(y + r + 1 >= h ? h - 1 : y + r + 1) * w * 4;
            for (int k = 0; k < w * 4; k++) som[k] += entra[k] - sai[k];
        }
        outro = q; q = saida;
    }
    for (size_t i = 0; i < n; i++) {
        int a = q[i * 4 + 3];
        if (a <= 0) { px[i] = 0; continue; }
        // q = cor * alfa; a = alfa * 255
        float inv = 255.0f / a;   // divisão inteira por pixel era o mais lento do desfoque
        int rr = (int)(q[i * 4] * inv + .5f), gg = (int)(q[i * 4 + 1] * inv + .5f), bb = (int)(q[i * 4 + 2] * inv + .5f), aa = (int)(a * (1.0f / 255) + .5f);
        px[i] = ((uint32_t)(aa > 255 ? 255 : aa) << 24) | ((uint32_t)(rr > 255 ? 255 : rr) << 16) | ((uint32_t)(gg > 255 ? 255 : gg) << 8) | (uint32_t)(bb > 255 ? 255 : bb);
    }
    free(tmp); free(som);
}

static float edSuave(float a, float b, float x) { float t = edLimF((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
// Curva por 5 pontos (0, 64, 128, 192, 255) com deslocamentos: interpolação suave (Catmull-Rom).
static void edCurvaLut(const int* p, float* lut) {
    float y[5];
    for (int i = 0; i < 5; i++) y[i] = edLimF(i / 4.0f + p[i] / 100.0f * 0.25f, 0, 1);
    for (int i = 0; i < 256; i++) {
        float x = i / 255.0f * 4; int k = (int)x; if (k > 3) k = 3;
        float t = x - k;
        float p0 = y[k > 0 ? k - 1 : 0], p1 = y[k], p2 = y[k + 1], p3 = y[k + 2 <= 4 ? k + 2 : 4];
        float v = 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t);
        lut[i] = edLimF(v, 0, 1);
    }
}
static unsigned edRand = 12345;
static inline int edAleatorio() { edRand = edRand * 1103515245u + 12345u; return (int)((edRand >> 16) & 0x7FFF); }

static void edEspecial(uint32_t* px, int w, int h, int tipo, float escala);
// Aplica todos os ajustes de src em dst (mesmo tamanho). escala: tamanho da prévia / imagem
// inteira, para os raios (desfoque, clareza...) baterem com o resultado final.
static void edProcessar(const uint32_t* src, uint32_t* dst, int w, int h, const EdAjustes* a, float escala) {
    size_t n = (size_t)w * h;
    memcpy(dst, src, n * 4);
    // 1. remover ruído: média dos vizinhos parecidos (preserva bordas)
    if (a->removerRuido > 0) {
        uint32_t* t = (uint32_t*)malloc(n * 4);
        if (t) {
            memcpy(t, dst, n * 4);
            int lim = 8 + a->removerRuido * 60 / 100, r = a->removerRuido > 60 ? 2 : 1;
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                uint32_t c = t[(size_t)y * w + x];
                int cr = ED_R(c), cg = ED_G(c), cb = ED_BL(c), sr = 0, sg = 0, sb = 0, k = 0;
                bool miolo = x >= r && y >= r && x < w - r && y < h - r;
                for (int dy = -r; dy <= r; dy++) {
                    int yy = miolo ? y + dy : edLim(y + dy, 0, h - 1);
                    const uint32_t* tl = t + (size_t)yy * w;
                    for (int dx = -r; dx <= r; dx++) {
                        uint32_t v = tl[miolo ? x + dx : edLim(x + dx, 0, w - 1)];
                        int d = abs((int)ED_R(v) - cr) + abs((int)ED_G(v) - cg) + abs((int)ED_BL(v) - cb);
                        if (d <= lim) { sr += ED_R(v); sg += ED_G(v); sb += ED_BL(v); k++; }
                    }
                }
                dst[(size_t)y * w + x] = edCor(sr / k, sg / k, sb / k, ED_A(c));
            }
            free(t);
        }
    }
    // 2. luz e cor, pixel a pixel, com tabelas
    bool cor = a->brilho || a->contraste || a->exposicao || a->realces || a->sombras || a->claros || a->escuros || a->saturacao || a->vibracao ||
        a->calor || a->tom || a->colorir || a->especial == ED_ESP_SEPIA || a->especial == ED_ESP_PB || a->especial == ED_ESP_INVERTER;
    bool curvas = false, hsl = false;
    for (int c = 0; c < 4; c++) for (int i = 0; i < 5; i++) if (a->curva[c][i]) curvas = true;
    for (int c = 0; c < 8; c++) for (int i = 0; i < 3; i++) if (a->hsl[c][i]) hsl = true;
    if (cor || curvas || hsl) {
        float tom[256], cg[256], cc[3][256];
        float ex = powf(2.0f, a->exposicao / 50.0f), co = a->contraste >= 0 ? 1 + a->contraste / 50.0f : 1 + a->contraste / 100.0f;
        edCurvaLut(a->curva[0], cg);
        for (int k = 0; k < 3; k++) edCurvaLut(a->curva[k + 1], cc[k]);
        for (int i = 0; i < 256; i++) {
            float v = i / 255.0f * ex + a->brilho / 200.0f;
            v = (v - 0.5f) * co + 0.5f;
            v += a->sombras / 100.0f * 0.45f * (1 - v) * (1 - v) * (v > 0 ? 1 : 0);
            v += a->realces / 100.0f * 0.45f * v * v;
            v += a->claros / 100.0f * 0.25f * edSuave(0.5f, 1.0f, v);
            v += a->escuros / 100.0f * 0.25f * (1 - edSuave(0.0f, 0.5f, v));
            v = edLimF(v, 0, 1);
            tom[i] = cg[(int)(v * 255 + .5f)];
        }
        unsigned char lut[3][256];
        for (int k = 0; k < 3; k++) for (int i = 0; i < 256; i++) lut[k][i] = edB(cc[k][(int)(tom[i] * 255 + .5f)] * 255);
        float sat = 1 + a->saturacao / 100.0f, vib = a->vibracao / 100.0f, calor = a->calor * 0.6f, tint = a->tom * 0.5f;
        static const float centros[8] = { 0, 30, 60, 120, 180, 240, 280, 320 };
        // Com HSL ou colorir, a cor vira uma tabela 3D e cada pixel só interpola (HSL por pixel é caro).
        // Sem eles, a conta direta é mais rápida.
        bool usarTabela = hsl || a->colorir;
        static uint8_t tab3d[33 * 33 * 33 * 3];
        size_t totalCores = usarTabela ? 33 * 33 * 33 : n;
        for (size_t qi = 0; qi < totalCores; qi++) {
            int vr, vg, vb;
            if (usarTabela) { int ir = (int)(qi / (33 * 33)), ig = (int)((qi / 33) % 33), ib = (int)(qi % 33); vr = ir * 255 / 32; vg = ig * 255 / 32; vb = ib * 255 / 32; }
            else { uint32_t c0 = dst[qi]; vr = ED_R(c0); vg = ED_G(c0); vb = ED_BL(c0); }
            float r = lut[0][vr], g = lut[1][vg], b = lut[2][vb];
            r += calor; b -= calor; g -= tint;
            if (sat != 1 || vib != 0) {
                float L = 0.299f * r + 0.587f * g + 0.114f * b;
                float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
                float s0 = mx > 0 ? (mx - mn) / fmaxf(mx, 1) : 0;
                float f = sat * (1 + vib * (1 - s0) * (vib > 0 ? 1.4f : 1));
                r = L + (r - L) * f; g = L + (g - L) * f; b = L + (b - L) * f;
            }
            if (hsl || a->colorir) {
                float hh, ss, ll;
                edRgbHsl(edLimF(r, 0, 255) / 255, edLimF(g, 0, 255) / 255, edLimF(b, 0, 255) / 255, &hh, &ss, &ll);
                if (hsl && ss > 0.02f) {
                    float dh = 0, fs = 1, dl = 0;
                    for (int k = 0; k < 8; k++) {
                        float d = fabsf(hh - centros[k]); if (d > 180) d = 360 - d;
                        float largura = k == 0 || k == 1 || k == 2 ? 30 : 45;
                        float pw = 1 - d / largura; if (pw <= 0) continue;
                        dh += pw * a->hsl[k][0] * 0.3f; fs *= 1 + pw * a->hsl[k][1] / 100.0f; dl += pw * a->hsl[k][2] / 100.0f * 0.35f * ss;
                    }
                    hh += dh; ss = edLimF(ss * fs, 0, 1); ll = edLimF(ll + dl, 0, 1);
                }
                if (a->colorir) {
                    float cr, cg2, cb2; edHslRgb((float)a->colorirMatiz, 0.65f, ll, &cr, &cg2, &cb2);
                    float hr, hg, hb; edHslRgb(hh, ss, ll, &hr, &hg, &hb);
                    float t = a->colorir / 100.0f;
                    r = (hr + (cr - hr) * t) * 255; g = (hg + (cg2 - hg) * t) * 255; b = (hb + (cb2 - hb) * t) * 255;
                }
                else { float hr, hg, hb; edHslRgb(hh, ss, ll, &hr, &hg, &hb); r = hr * 255; g = hg * 255; b = hb * 255; }
            }
            if (a->especial == ED_ESP_SEPIA || a->especial == ED_ESP_PB || a->especial == ED_ESP_INVERTER) {
                float f = a->especialForca / 100.0f, nr, ng, nb;
                if (a->especial == ED_ESP_SEPIA) { nr = r * .393f + g * .769f + b * .189f; ng = r * .349f + g * .686f + b * .168f; nb = r * .272f + g * .534f + b * .131f; }
                else if (a->especial == ED_ESP_PB) { nr = ng = nb = 0.299f * r + 0.587f * g + 0.114f * b; }
                else { nr = 255 - r; ng = 255 - g; nb = 255 - b; }
                r += (nr - r) * f; g += (ng - g) * f; b += (nb - b) * f;
            }
            if (!usarTabela) { dst[qi] = (dst[qi] & 0xFF000000u) | ((uint32_t)edB(r) << 16) | ((uint32_t)edB(g) << 8) | (uint32_t)edB(b); continue; }
            tab3d[qi * 3] = edB(r); tab3d[qi * 3 + 1] = edB(g); tab3d[qi * 3 + 2] = edB(b);
        }
        for (size_t i = 0; usarTabela && i < n; i++) {
            uint32_t c = dst[i];
            int R = ED_R(c) * 32, G = ED_G(c) * 32, B = ED_BL(c) * 32;
            int r0 = R / 255, g0 = G / 255, b0 = B / 255;
            int fr = (R - r0 * 255) * 256 / 255, fg = (G - g0 * 255) * 256 / 255, fb = (B - b0 * 255) * 256 / 255;
            int r1 = r0 < 32 ? r0 + 1 : 32, g1 = g0 < 32 ? g0 + 1 : 32, b1 = b0 < 32 ? b0 + 1 : 32;
            int o[3];
            for (int k = 0; k < 3; k++) {
#define ED_T3(a, b2, c2) (int)tab3d[(((a) * 33 + (b2)) * 33 + (c2)) * 3 + k]
                int c00 = ED_T3(r0, g0, b0) * (256 - fb) + ED_T3(r0, g0, b1) * fb, c01 = ED_T3(r0, g1, b0) * (256 - fb) + ED_T3(r0, g1, b1) * fb;
                int c10 = ED_T3(r1, g0, b0) * (256 - fb) + ED_T3(r1, g0, b1) * fb, c11 = ED_T3(r1, g1, b0) * (256 - fb) + ED_T3(r1, g1, b1) * fb;
#undef ED_T3
                int c0 = (c00 >> 8) * (256 - fg) + (c01 >> 8) * fg, c1 = (c10 >> 8) * (256 - fg) + (c11 >> 8) * fg;
                o[k] = ((c0 >> 8) * (256 - fr) + (c1 >> 8) * fr) >> 8;
            }
            dst[i] = (c & 0xFF000000u) | ((uint32_t)o[0] << 16) | ((uint32_t)o[1] << 8) | (uint32_t)o[2];
        }
    }
    // 3. clareza (contraste local) e nitidez (máscara de desfoque)
    for (int etapa = 0; etapa < 2; etapa++) {
        int quanto = etapa == 0 ? a->clareza : a->nitidez;
        if (!quanto) continue;
        int raio = etapa == 0 ? (int)(18 * escala + 1) : (int)(1.5f * escala + 1);
        uint32_t* bl = (uint32_t*)malloc(n * 4);
        if (!bl) continue;
        memcpy(bl, dst, n * 4);
        edDesfocarCaixa(bl, w, h, raio);
        float f = etapa == 0 ? quanto / 100.0f * 0.8f : quanto / 100.0f * 1.6f;
        for (size_t i = 0; i < n; i++) {
            uint32_t c = dst[i], d = bl[i];
            float peso = 1;
            if (etapa == 0) { float L = (ED_R(c) + ED_G(c) + ED_BL(c)) / 765.0f; peso = 1 - fabsf(L - 0.5f) * 1.2f; if (peso < 0.1f) peso = 0.1f; }
            dst[i] = edCor((int)(ED_R(c) + (ED_R(c) - (float)ED_R(d)) * f * peso), (int)(ED_G(c) + (ED_G(c) - (float)ED_G(d)) * f * peso),
                (int)(ED_BL(c) + (ED_BL(c) - (float)ED_BL(d)) * f * peso), ED_A(c));
        }
        free(bl);
    }
    // 4. desfoque
    if (a->desfoque > 0) edDesfocarCaixa(dst, w, h, (int)(a->desfoque * 0.3f * escala + 0.5f) > 0 ? (int)(a->desfoque * 0.3f * escala + 0.5f) : 1);
    // 5. efeitos especiais de vizinhança
    if (a->especial >= ED_ESP_NEON && a->especialForca > 0) {
        uint32_t* t = (uint32_t*)malloc(n * 4);
        if (t) {
            memcpy(t, dst, n * 4);
            edEspecial(t, w, h, a->especial, escala);
            float f = a->especialForca / 100.0f;
            for (size_t i = 0; i < n; i++) {
                uint32_t c = dst[i], d = t[i];
                dst[i] = edCor((int)(ED_R(c) + ((float)ED_R(d) - ED_R(c)) * f), (int)(ED_G(c) + ((float)ED_G(d) - ED_G(c)) * f), (int)(ED_BL(c) + ((float)ED_BL(d) - ED_BL(c)) * f), ED_A(c));
            }
            free(t);
        }
    }
    // 6. dispersão (vermelho para um lado, azul para o outro)
    if (a->dispersao > 0) {
        int d = (int)(a->dispersao * escala + 0.5f); if (d < 1) d = 1;
        uint32_t* t = (uint32_t*)malloc(n * 4);
        if (t) {
            memcpy(t, dst, n * 4);
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                uint32_t c = t[(size_t)y * w + x];
                uint32_t cr = t[(size_t)y * w + edLim(x - d, 0, w - 1)], cb = t[(size_t)y * w + edLim(x + d, 0, w - 1)];
                dst[(size_t)y * w + x] = edCor(ED_R(cr), ED_G(c), ED_BL(cb), ED_A(c));
            }
            free(t);
        }
    }
    // 7. pixelizar
    if (a->pixelizar > 1) {
        int b = (int)(a->pixelizar * escala + 0.5f); if (b < 2) b = 2;
        for (int by = 0; by < h; by += b) for (int bx = 0; bx < w; bx += b) {
            int sr = 0, sg = 0, sb = 0, sa = 0, k = 0;
            for (int y = by; y < by + b && y < h; y++) for (int x = bx; x < bx + b && x < w; x++) { uint32_t c = dst[(size_t)y * w + x]; sr += ED_R(c); sg += ED_G(c); sb += ED_BL(c); sa += ED_A(c); k++; }
            uint32_t m = edCor(sr / k, sg / k, sb / k, sa / k);
            for (int y = by; y < by + b && y < h; y++) for (int x = bx; x < bx + b && x < w; x++) dst[(size_t)y * w + x] = m;
        }
    }
    // 8. ruído, vinheta e posterizar
    if (a->ruido > 0 || a->vinheta || a->posterizar > 1) {
        float amp = a->ruido * 0.6f, cx = w / 2.0f, cy = h / 2.0f, maxd = cx * cx + cy * cy, vin = a->vinheta / 100.0f;
        int niv = a->posterizar;
        edRand = 777;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            uint32_t c = dst[(size_t)y * w + x];
            float r = ED_R(c), g = ED_G(c), b = ED_BL(c);
            if (amp > 0) { float z = ((edAleatorio() + edAleatorio() + edAleatorio()) / 98301.0f - 0.5f) * 2 * amp; r += z; g += z; b += z; }
            if (vin) {
                float d = ((x - cx) * (x - cx) + (y - cy) * (y - cy)) / maxd;
                float k = edSuave(0.25f, 1.0f, d) * vin;
                if (k > 0) { r *= 1 - k * 0.85f; g *= 1 - k * 0.85f; b *= 1 - k * 0.85f; }
                else { r += (255 - r) * -k * 0.7f; g += (255 - g) * -k * 0.7f; b += (255 - b) * -k * 0.7f; }
            }
            if (niv > 1) {
                float q = 255.0f / (niv - 1);
                r = floorf(edLimF(r, 0, 255) / q + 0.5f) * q; g = floorf(edLimF(g, 0, 255) / q + 0.5f) * q; b = floorf(edLimF(b, 0, 255) / q + 0.5f) * q;
            }
            dst[(size_t)y * w + x] = edCor((int)(r + .5f), (int)(g + .5f), (int)(b + .5f), ED_A(c));
        }
    }
}
// Bordas (Sobel) na luminância.
static void edBordas(const uint32_t* px, int w, int h, float* m) {
    size_t n = (size_t)w * h;
    float* L = (float*)malloc(n * sizeof(float));
    if (!L) { memset(m, 0, n * sizeof(float)); return; }
    for (size_t i = 0; i < n; i++) { uint32_t c = px[i]; L[i] = ED_R(c) * 0.299f + ED_G(c) * 0.587f + ED_BL(c) * 0.114f; }
    for (int y = 0; y < h; y++) {
        const float* a = L + (size_t)(y > 0 ? y - 1 : 0) * w, *b = L + (size_t)y * w, *c = L + (size_t)(y < h - 1 ? y + 1 : y) * w;
        for (int x = 0; x < w; x++) {
            int x0 = x > 0 ? x - 1 : 0, x2 = x < w - 1 ? x + 1 : x;
            float gx = -a[x0] - 2 * b[x0] - c[x0] + a[x2] + 2 * b[x2] + c[x2], gy = -a[x0] - 2 * a[x] - a[x2] + c[x0] + 2 * c[x] + c[x2];
            float v = sqrtf(gx * gx + gy * gy) / 360.0f;
            m[(size_t)y * w + x] = v > 1 ? 1 : v;
        }
    }
    free(L);
}
static void edEspecial(uint32_t* px, int w, int h, int tipo, float escala) {
    size_t n = (size_t)w * h;
    if (tipo == ED_ESP_BRILHO_SUAVE) {
        uint32_t* b = (uint32_t*)malloc(n * 4); if (!b) return;
        memcpy(b, px, n * 4); edDesfocarCaixa(b, w, h, (int)(12 * escala) + 1);
        for (size_t i = 0; i < n; i++) {
            uint32_t c = px[i], d = b[i];
            px[i] = edCor(255 - (255 - ED_R(c)) * (255 - ED_R(d)) / 255, 255 - (255 - ED_G(c)) * (255 - ED_G(d)) / 255, 255 - (255 - ED_BL(c)) * (255 - ED_BL(d)) / 255, ED_A(c));
        }
        free(b); return;
    }
    float* m = (float*)malloc(n * sizeof(float)); if (!m) return;
    if (tipo == ED_ESP_ANIME) {
        // cores chapadas (suaviza e posteriza) com contorno escuro
        uint32_t* s = (uint32_t*)malloc(n * 4); if (!s) { free(m); return; }
        memcpy(s, px, n * 4); edDesfocarCaixa(s, w, h, (int)(2 * escala) + 1);
        edBordas(px, w, h, m);
        for (size_t i = 0; i < n; i++) {
            uint32_t c = s[i]; float q = 255.0f / 5;
            float r = floorf(ED_R(c) / q + .5f) * q, g = floorf(ED_G(c) / q + .5f) * q, b = floorf(ED_BL(c) / q + .5f) * q;
            float e = m[i] > 0.25f ? 1 - edLimF((m[i] - 0.25f) * 3, 0, 0.9f) : 1;
            float Lm = 0.299f * r + 0.587f * g + 0.114f * b;
            r = Lm + (r - Lm) * 1.3f; g = Lm + (g - Lm) * 1.3f; b = Lm + (b - Lm) * 1.3f;
            px[i] = edCor((int)(r * e), (int)(g * e), (int)(b * e), ED_A(px[i]));
        }
        free(s); free(m); return;
    }
    edBordas(px, w, h, m);
    for (size_t i = 0; i < n; i++) {
        uint32_t c = px[i]; float e = m[i];
        if (tipo == ED_ESP_NEON) {
            // cor bem saturada da própria imagem, acesa só nas bordas
            float r = ED_R(c), g = ED_G(c), b = ED_BL(c), mn = fminf(r, fminf(g, b));
            r -= mn; g -= mn; b -= mn;
            float mx = fmaxf(r, fmaxf(g, b)); if (mx < 1) { r = g = b = 255; mx = 255; }
            float k = edLimF(e * 2.2f, 0, 1) * 255 / mx;
            px[i] = edCor((int)(r * k), (int)(g * k), (int)(b * k), ED_A(c));
        }
        else {   // desenho a lápis
            int v = (int)(255 - edLimF(e * 2.5f, 0, 1) * 230);
            px[i] = edCor(v, v, v, ED_A(c));
        }
    }
    free(m);
}

// Filtros prontos: ajustes + efeito, com intensidade.
struct EdFiltro { const char* nome; EdAjustes a; };
static EdFiltro edFiltros[20];
static int edTotalFiltros = 0;
static void edMontarFiltros() {
    if (edTotalFiltros) return;
    EdAjustes z = {};
    EdFiltro* f = edFiltros;
#define ED_F(nome_) (f->nome = nome_, f->a = z, &(f++)->a)
    EdAjustes* a;
    a = ED_F("Nenhum");
    a = ED_F("Vintage"); a->saturacao = -25; a->calor = 22; a->contraste = -10; a->vinheta = 35; a->ruido = 12; a->curva[0][0] = 40; a->curva[0][4] = -15;
    a = ED_F("Sepia"); a->especial = ED_ESP_SEPIA; a->especialForca = 100; a->contraste = 8;
    a = ED_F("Preto e branco"); a->especial = ED_ESP_PB; a->especialForca = 100; a->contraste = 15;
    a = ED_F("Noir"); a->especial = ED_ESP_PB; a->especialForca = 100; a->contraste = 45; a->vinheta = 45; a->escuros = -25;
    a = ED_F("Cinema"); a->contraste = 15; a->saturacao = -10; a->curva[1][3] = 15; a->curva[1][4] = 10; a->curva[3][0] = 30; a->curva[3][1] = 15; a->curva[3][4] = -15;
    a = ED_F("Frio"); a->calor = -35; a->tom = -5; a->contraste = 5;
    a = ED_F("Quente"); a->calor = 35; a->vibracao = 15;
    a = ED_F("Drama"); a->clareza = 60; a->contraste = 25; a->saturacao = -15; a->vinheta = 30;
    a = ED_F("Vivido"); a->vibracao = 50; a->saturacao = 15; a->contraste = 10;
    a = ED_F("Polaroid"); a->calor = 15; a->curva[0][0] = 45; a->curva[0][4] = -30; a->saturacao = -10; a->vinheta = 15;
    a = ED_F("Desbotado"); a->contraste = -30; a->curva[0][0] = 55; a->saturacao = -20;
    a = ED_F("HDR"); a->clareza = 80; a->sombras = 40; a->realces = -40; a->vibracao = 25;
    a = ED_F("Sonho"); a->especial = ED_ESP_BRILHO_SUAVE; a->especialForca = 70; a->calor = 10; a->contraste = -10;
    a = ED_F("Neon"); a->especial = ED_ESP_NEON; a->especialForca = 100;
    a = ED_F("Anime"); a->especial = ED_ESP_ANIME; a->especialForca = 100;
    a = ED_F("Desenho a lapis"); a->especial = ED_ESP_DESENHO; a->especialForca = 100;
    a = ED_F("Retro 8 bits"); a->pixelizar = 8; a->posterizar = 6; a->saturacao = 20;
    a = ED_F("Inverter"); a->especial = ED_ESP_INVERTER; a->especialForca = 100;
#undef ED_F
    edTotalFiltros = (int)(f - edFiltros);
}
static EdAjustes edFiltroEm(int i, int intensidade) {
    edMontarFiltros();
    EdAjustes a = edFiltros[edLim(i, 0, edTotalFiltros - 1)].a;
    float f = intensidade / 100.0f;
    int* v = (int*)&a; int total = (int)(sizeof(a) / sizeof(int));
    int iEsp = (int)(&a.especial - (int*)&a);
    for (int k = 0; k < total; k++) if (k != iEsp && k != iEsp + 1 && k != (int)(&a.colorirMatiz - (int*)&a) && k != (int)(&a.posterizar - (int*)&a) && k != (int)(&a.pixelizar - (int*)&a)) v[k] = (int)(v[k] * f);
    a.especialForca = (int)(a.especialForca * f);
    if (f < 0.5f) { a.posterizar = 0; a.pixelizar = 0; }
    return a;
}

// Aplica um processamento à camada ativa respeitando a seleção.
static void edAplicarNaCamada(const EdAjustes* a) {
    size_t n = (size_t)ed.w * ed.h;
    uint32_t* o = (uint32_t*)malloc(n * 4);
    if (!o) return;
    edProcessar(edAtiva(), o, ed.w, ed.h, a, 1.0f);
    uint32_t* L = edAtiva();
    if (ed.sel) {
        for (size_t i = 0; i < n; i++) {
            int k = ed.sel[i]; if (!k) continue;
            uint32_t c = L[i], d = o[i];
            L[i] = k == 255 ? d : edCor((ED_R(c) * (255 - k) + ED_R(d) * k) / 255, (ED_G(c) * (255 - k) + ED_G(d) * k) / 255, (ED_BL(c) * (255 - k) + ED_BL(d) * k) / 255, (ED_A(c) * (255 - k) + ED_A(d) * k) / 255);
        }
        free(o);
    }
    else { free(ed.cam[ed.ativa].px); ed.cam[ed.ativa].px = o; }
    edSujou();
}

// ---------------------------------------------------------------- máscara de traço e desenho
static int edTx0, edTy0, edTx1, edTy1;     // área suja da máscara
static void edTracoComecar() {
    memcpy(ed.base, edAtiva(), (size_t)ed.w * ed.h * 4);
    memset(ed.traco, 0, (size_t)ed.w * ed.h);
    edTx0 = ed.w; edTy0 = ed.h; edTx1 = 0; edTy1 = 0;
}
static void edTracoArea(int x0, int y0, int x1, int y1) {
    if (x0 < edTx0) edTx0 = x0; if (y0 < edTy0) edTy0 = y0; if (x1 > edTx1) edTx1 = x1; if (y1 > edTy1) edTy1 = y1;
    edTx0 = edLim(edTx0, 0, ed.w); edTy0 = edLim(edTy0, 0, ed.h); edTx1 = edLim(edTx1, 0, ed.w); edTy1 = edLim(edTy1, 0, ed.h);
}
// Disco suave na máscara (fica o maior valor: o traço não escurece onde passa duas vezes).
static void edMascaraDisco(float cx, float cy, float raio, int dureza, int forca) {
    if (raio < 0.5f) raio = 0.5f;
    int x0 = (int)floorf(cx - raio - 1), x1 = (int)ceilf(cx + raio + 1), y0 = (int)floorf(cy - raio - 1), y1 = (int)ceilf(cy + raio + 1);
    x0 = edLim(x0, 0, ed.w); x1 = edLim(x1, 0, ed.w); y0 = edLim(y0, 0, ed.h); y1 = edLim(y1, 0, ed.h);
    float interno = raio * edLimF(dureza / 100.0f, 0, 0.98f);
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        float d = sqrtf((x + .5f - cx) * (x + .5f - cx) + (y + .5f - cy) * (y + .5f - cy));
        float v;
        if (d >= raio + 0.5f) continue;
        if (d <= interno) v = 1; else if (raio - interno < 1) v = edLimF(raio + 0.5f - d, 0, 1); else v = 1 - edSuave(interno, raio + 0.5f, d);
        int m = (int)(v * forca + .5f);
        uint8_t* t = &ed.traco[(size_t)y * ed.w + x];
        if (m > *t) *t = (uint8_t)m;
    }
    edTracoArea(x0, y0, x1, y1);
}
static void edMascaraLinha(float ax, float ay, float bx, float by, float raio, int dureza, int forca) {
    float d = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    float passo = raio * 0.25f; if (passo < 0.5f) passo = 0.5f;
    int n = (int)(d / passo) + 1;
    for (int i = 0; i <= n; i++) { float t = (float)i / n; edMascaraDisco(ax + (bx - ax) * t, ay + (by - ay) * t, raio, dureza, forca); }
}
// Polígono preenchido na máscara, com borda suave (4 subamostras por linha).
struct EdPt { float x, y; };
static void edMascaraPoligono(const EdPt* p, int n, int forca) {
    if (n < 3) return;
    float mnx = p[0].x, mxx = p[0].x, mny = p[0].y, mxy = p[0].y;
    for (int i = 1; i < n; i++) { mnx = fminf(mnx, p[i].x); mxx = fmaxf(mxx, p[i].x); mny = fminf(mny, p[i].y); mxy = fmaxf(mxy, p[i].y); }
    int x0 = edLim((int)floorf(mnx), 0, ed.w), x1 = edLim((int)ceilf(mxx) + 1, 0, ed.w), y0 = edLim((int)floorf(mny), 0, ed.h), y1 = edLim((int)ceilf(mxy) + 1, 0, ed.h);
    if (x0 >= x1 || y0 >= y1) return;
    float* cob = (float*)calloc((size_t)(x1 - x0 + 2), sizeof(float));
    float* xs = (float*)malloc(sizeof(float) * (n + 2));
    if (!cob || !xs) { free(cob); free(xs); return; }
    for (int y = y0; y < y1; y++) {
        memset(cob, 0, sizeof(float) * (x1 - x0 + 2));
        for (int s = 0; s < 4; s++) {
            float sy = y + (s + 0.5f) / 4;
            int k = 0;
            for (int i = 0; i < n; i++) {
                EdPt a = p[i], b = p[(i + 1) % n];
                if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs[k++] = a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x);
            }
            for (int i = 1; i < k; i++) { float v = xs[i]; int j = i - 1; while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; } xs[j + 1] = v; }
            for (int i = 0; i + 1 < k; i += 2) {
                float a = edLimF(xs[i], (float)x0, (float)x1), b = edLimF(xs[i + 1], (float)x0, (float)x1);
                if (b <= a) continue;
                int ia = (int)floorf(a), ib = (int)floorf(b);
                if (ia == ib) { cob[ia - x0] += (b - a) / 4; continue; }
                cob[ia - x0] += (ia + 1 - a) / 4;
                for (int x = ia + 1; x < ib; x++) cob[x - x0] += 0.25f;
                if (ib < x1) cob[ib - x0] += (b - ib) / 4;
            }
        }
        for (int x = x0; x < x1; x++) {
            int m = (int)(edLimF(cob[x - x0], 0, 1) * forca + .5f);
            uint8_t* t = &ed.traco[(size_t)y * ed.w + x];
            if (m > *t) *t = (uint8_t)m;
        }
    }
    free(cob); free(xs);
    edTracoArea(x0, y0, x1, y1);
}
// Contorno de um polígono (fechado ou não) com espessura.
static void edMascaraContorno(const EdPt* p, int n, bool fechado, float esp, int forca) {
    int segmentos = fechado ? n : n - 1;
    for (int i = 0; i < segmentos; i++) { EdPt a = p[i], b = p[(i + 1) % n]; edMascaraLinha(a.x, a.y, b.x, b.y, esp / 2, 100, forca); }
}
// Pinta a cor (ou apaga) usando a máscara sobre a camada como estava no começo (ed.base).
// modo: 0 pintar, 1 apagar, 2 marcador (no máximo metade da força)
static void edTracoAplicar(uint32_t cor, int opacidade, int modo) {
    uint32_t* L = edAtiva();
    for (int y = edTy0; y < edTy1; y++) for (int x = edTx0; x < edTx1; x++) {
        size_t i = (size_t)y * ed.w + x;
        int m = ed.traco[i] * opacidade / 100;
        if (ed.sel) m = m * ed.sel[i] / 255;
        uint32_t b = ed.base[i];
        if (!m) { L[i] = b; continue; }
        if (modo == 1) { L[i] = (b & 0x00FFFFFF) | ((uint32_t)(ED_A(b) * (255 - m) / 255) << 24); continue; }
        if (modo == 2) m = m / 2;
        uint32_t s = (cor & 0x00FFFFFF) | ((uint32_t)(ED_A(cor) * m / 255) << 24);
        L[i] = edSobre(b, s, 100, ED_MIS_NORMAL);
    }
    ed.compSujo = true; ed.mudou = true;
}
static void edTracoComporArea() { edComporRet(edTx0, edTy0, edTx1, edTy1); }

// Formas: devolve os pontos do contorno dentro do retângulo (x0,y0)-(x1,y1).
enum { ED_F_RET, ED_F_RET_REDONDO, ED_F_ELIPSE, ED_F_TRIANGULO, ED_F_ESTRELA, ED_F_SETA, ED_F_HEXAGONO, ED_F_CORACAO, ED_F_BALAO, ED_F_LINHA, ED_F_TOTAL };
static const char* const edNomesForma[ED_F_TOTAL] = { "Retangulo", "Ret. arredondado", "Elipse", "Triangulo", "Estrela", "Seta", "Hexagono", "Coracao", "Balao de fala", "Linha" };
static int edFormaPontos(int forma, float x0, float y0, float x1, float y1, EdPt* p, int max) {
    if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }
    float w = x1 - x0, h = y1 - y0, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int n = 0;
#define ED_P(X, Y) do { if (n < max) { p[n].x = (X); p[n].y = (Y); n++; } } while (0)
    switch (forma) {
    case ED_F_RET: ED_P(x0, y0); ED_P(x1, y0); ED_P(x1, y1); ED_P(x0, y1); break;
    case ED_F_RET_REDONDO: {
        float r = fminf(w, h) * 0.18f;
        float cs[4][2] = { { x1 - r, y0 + r }, { x1 - r, y1 - r }, { x0 + r, y1 - r }, { x0 + r, y0 + r } };
        for (int c = 0; c < 4; c++) for (int i = 0; i <= 8; i++) { float ang = (-90 + c * 90 + i * 90 / 8.0f) * 3.14159265f / 180; ED_P(cs[c][0] + cosf(ang) * r, cs[c][1] + sinf(ang) * r); }
        break;
    }
    case ED_F_ELIPSE: for (int i = 0; i < 72; i++) { float ang = i * 3.14159265f * 2 / 72; ED_P(cx + cosf(ang) * w / 2, cy + sinf(ang) * h / 2); } break;
    case ED_F_TRIANGULO: ED_P(cx, y0); ED_P(x1, y1); ED_P(x0, y1); break;
    case ED_F_ESTRELA: for (int i = 0; i < 10; i++) { float ang = (-90 + i * 36) * 3.14159265f / 180, r = i % 2 ? 0.4f : 1.0f; ED_P(cx + cosf(ang) * w / 2 * r, cy + 0.06f * h + sinf(ang) * h / 2 * r); } break;
    case ED_F_SETA: ED_P(x0, cy - h * 0.18f); ED_P(x1 - w * 0.35f, cy - h * 0.18f); ED_P(x1 - w * 0.35f, y0); ED_P(x1, cy); ED_P(x1 - w * 0.35f, y1); ED_P(x1 - w * 0.35f, cy + h * 0.18f); ED_P(x0, cy + h * 0.18f); break;
    case ED_F_HEXAGONO: for (int i = 0; i < 6; i++) { float ang = i * 60 * 3.14159265f / 180; ED_P(cx + cosf(ang) * w / 2, cy + sinf(ang) * h / 2); } break;
    case ED_F_CORACAO: for (int i = 0; i < 80; i++) {
        float t = i * 3.14159265f * 2 / 80;
        float hx = 16 * powf(sinf(t), 3), hy = -(13 * cosf(t) - 5 * cosf(2 * t) - 2 * cosf(3 * t) - cosf(4 * t));
        ED_P(cx + hx / 34 * w, cy + (hy + 2) / 32 * h);
    } break;
    case ED_F_BALAO: {
        float bh = h * 0.78f, r = fminf(w, bh) * 0.2f;
        float cs[4][2] = { { x1 - r, y0 + r }, { x1 - r, y0 + bh - r }, { x0 + r, y0 + bh - r }, { x0 + r, y0 + r } };
        for (int c = 0; c < 4; c++) {
            for (int i = 0; i <= 6; i++) { float ang = (-90 + c * 90 + i * 15) * 3.14159265f / 180; ED_P(cs[c][0] + cosf(ang) * r, cs[c][1] + sinf(ang) * r); }
            if (c == 1) { ED_P(x0 + w * 0.42f, y0 + bh); ED_P(x0 + w * 0.22f, y1); ED_P(x0 + w * 0.28f, y0 + bh); }
        }
        break;
    }
    default: ED_P(x0, y0); ED_P(x1, y1); break;
    }
#undef ED_P
    return n;
}

// Balde: pinta a área parecida com o ponto (contígua ou em toda a camada).
static void edBalde(int sx, int sy, int tolerancia, bool global, uint32_t cor, int opacidade) {
    if (sx < 0 || sy < 0 || sx >= ed.w || sy >= ed.h) return;
    edTracoComecar();
    uint32_t* L = edAtiva();
    uint32_t alvo = L[(size_t)sy * ed.w + sx];
    int tol = tolerancia * 1020 / 100;
#define ED_PARECIDO(c) (abs((int)ED_R(c) - (int)ED_R(alvo)) + abs((int)ED_G(c) - (int)ED_G(alvo)) + abs((int)ED_BL(c) - (int)ED_BL(alvo)) + abs((int)ED_A(c) - (int)ED_A(alvo)) <= tol)
    if (global) {
        for (size_t i = 0, n = (size_t)ed.w * ed.h; i < n; i++) if (ED_PARECIDO(L[i])) ed.traco[i] = 255;
    }
    else {
        int* fila = (int*)malloc(sizeof(int) * (size_t)ed.w * ed.h);
        if (!fila) return;
        int ini = 0, fim = 0;
        fila[fim++] = sy * ed.w + sx; ed.traco[sy * ed.w + sx] = 255;
        while (ini < fim) {
            int i = fila[ini++], x = i % ed.w, y = i / ed.w;
            int viz[4] = { x > 0 ? i - 1 : -1, x < ed.w - 1 ? i + 1 : -1, y > 0 ? i - ed.w : -1, y < ed.h - 1 ? i + ed.w : -1 };
            for (int k = 0; k < 4; k++) { int j = viz[k]; if (j >= 0 && !ed.traco[j] && ED_PARECIDO(L[j])) { ed.traco[j] = 255; fila[fim++] = j; } }
        }
        free(fila);
    }
#undef ED_PARECIDO
    edTracoArea(0, 0, ed.w, ed.h);
    edTracoAplicar(cor, opacidade, 0);
}
// Degradê linear ou radial da cor 1 para a cor 2.
static void edDegrade(float ax, float ay, float bx, float by, bool radial, uint32_t c1, uint32_t c2, int opacidade) {
    uint32_t* L = edAtiva();
    float dx = bx - ax, dy = by - ay, d2 = dx * dx + dy * dy; if (d2 < 1) d2 = 1;
    float raio = sqrtf(d2);
    for (int y = 0; y < ed.h; y++) for (int x = 0; x < ed.w; x++) {
        size_t i = (size_t)y * ed.w + x;
        float t = radial ? sqrtf((x - ax) * (x - ax) + (y - ay) * (y - ay)) / raio : ((x - ax) * dx + (y - ay) * dy) / d2;
        t = edLimF(t, 0, 1);
        uint32_t c = edCor((int)(ED_R(c1) + ((float)ED_R(c2) - ED_R(c1)) * t), (int)(ED_G(c1) + ((float)ED_G(c2) - ED_G(c1)) * t),
            (int)(ED_BL(c1) + ((float)ED_BL(c2) - ED_BL(c1)) * t), (int)(ED_A(c1) + ((float)ED_A(c2) - ED_A(c1)) * t));
        int op = opacidade; if (ed.sel) op = op * ed.sel[i] / 255;
        L[i] = edSobre(L[i], c, op, ED_MIS_NORMAL);
    }
    edSujou();
}

// ---------------------------------------------------------------- seleção
enum { ED_SEL_NOVA, ED_SEL_SOMAR, ED_SEL_TIRAR };
static bool edSelGarantir() { if (!ed.sel) ed.sel = (uint8_t*)calloc((size_t)ed.w * ed.h, 1); return ed.sel != NULL; }
// Junta a máscara de traço (ed.traco) à seleção.
static void edSelDaMascara(int modo, int suavizar) {
    if (!edSelGarantir()) return;
    size_t n = (size_t)ed.w * ed.h;
    if (suavizar > 0) {
        // suaviza a borda: desfoque da máscara
        uint32_t* t = (uint32_t*)malloc(n * 4);
        if (t) {
            for (size_t i = 0; i < n; i++) t[i] = 0xFF000000u | ed.traco[i];
            edDesfocarCaixa(t, ed.w, ed.h, suavizar);
            for (size_t i = 0; i < n; i++) ed.traco[i] = (uint8_t)(t[i] & 255);
            free(t);
        }
    }
    for (size_t i = 0; i < n; i++) {
        int m = ed.traco[i], s = ed.sel[i];
        ed.sel[i] = (uint8_t)(modo == ED_SEL_NOVA ? m : modo == ED_SEL_SOMAR ? (s > m ? s : m) : s * (255 - m) / 255);
    }
    if (suavizar > 0) edLiberarTrabalho();
    bool algo = false; for (size_t i = 0; i < n && !algo; i++) algo = ed.sel[i] != 0;
    if (!algo) { free(ed.sel); ed.sel = NULL; }
    ed.versao++;
}
static void edSelTudo() { if (edSelGarantir()) memset(ed.sel, 255, (size_t)ed.w * ed.h); ed.versao++; }
static void edSelNenhuma() { free(ed.sel); ed.sel = NULL; ed.versao++; }
static void edSelInverter() {
    if (!ed.sel) { edSelTudo(); return; }
    for (size_t i = 0, n = (size_t)ed.w * ed.h; i < n; i++) ed.sel[i] = 255 - ed.sel[i];
    ed.versao++;
}
// Varinha: marca em ed.traco a região parecida com o ponto (na imagem toda, misturada).
static void edVarinha(int sx, int sy, int tolerancia) {
    edCompor();
    memset(ed.traco, 0, (size_t)ed.w * ed.h);
    if (sx < 0 || sy < 0 || sx >= ed.w || sy >= ed.h) return;
    uint32_t alvo = ed.comp[(size_t)sy * ed.w + sx];
    int tol = tolerancia * 765 / 100;
    int* fila = (int*)malloc(sizeof(int) * (size_t)ed.w * ed.h);
    if (!fila) return;
    int ini = 0, fim = 0;
    fila[fim++] = sy * ed.w + sx; ed.traco[sy * ed.w + sx] = 255;
    while (ini < fim) {
        int i = fila[ini++], x = i % ed.w, y = i / ed.w;
        int viz[4] = { x > 0 ? i - 1 : -1, x < ed.w - 1 ? i + 1 : -1, y > 0 ? i - ed.w : -1, y < ed.h - 1 ? i + ed.w : -1 };
        for (int k = 0; k < 4; k++) {
            int j = viz[k]; if (j < 0 || ed.traco[j]) continue;
            uint32_t c = ed.comp[j];
            if (abs((int)ED_R(c) - (int)ED_R(alvo)) + abs((int)ED_G(c) - (int)ED_G(alvo)) + abs((int)ED_BL(c) - (int)ED_BL(alvo)) <= tol) { ed.traco[j] = 255; fila[fim++] = j; }
        }
    }
    free(fila);
}
// Remover fundo. modo 0: a partir das bordas (fundo liso); 1: cor-chave (fundo verde/azul).
static int edRemoverFundo(int modo, uint32_t chave, int tolerancia, int suavizar) {
    edCompor();
    size_t n = (size_t)ed.w * ed.h;
    memset(ed.traco, 0, n);
    if (modo == 1) {
        int tol = tolerancia * 765 / 100;
        for (size_t i = 0; i < n; i++) {
            uint32_t c = ed.comp[i];
            int d = abs((int)ED_R(c) - (int)ED_R(chave)) + abs((int)ED_G(c) - (int)ED_G(chave)) + abs((int)ED_BL(c) - (int)ED_BL(chave));
            if (d <= tol) ed.traco[i] = 255;
            else if (d <= tol + 60) ed.traco[i] = (uint8_t)(255 - (d - tol) * 255 / 60);
        }
    }
    else {
        // a cor mais comum da borda e tudo que se liga a ela pela borda
        int hist[4096] = {};
        for (int x = 0; x < ed.w; x++) { uint32_t a = ed.comp[x], b = ed.comp[(size_t)(ed.h - 1) * ed.w + x]; hist[((ED_R(a) >> 4) << 8) | ((ED_G(a) >> 4) << 4) | (ED_BL(a) >> 4)]++; hist[((ED_R(b) >> 4) << 8) | ((ED_G(b) >> 4) << 4) | (ED_BL(b) >> 4)]++; }
        for (int y = 0; y < ed.h; y++) { uint32_t a = ed.comp[(size_t)y * ed.w], b = ed.comp[(size_t)y * ed.w + ed.w - 1]; hist[((ED_R(a) >> 4) << 8) | ((ED_G(a) >> 4) << 4) | (ED_BL(a) >> 4)]++; hist[((ED_R(b) >> 4) << 8) | ((ED_G(b) >> 4) << 4) | (ED_BL(b) >> 4)]++; }
        int melhor = 0; for (int i = 1; i < 4096; i++) if (hist[i] > hist[melhor]) melhor = i;
        int fr = ((melhor >> 8) << 4) + 8, fg = (((melhor >> 4) & 15) << 4) + 8, fb = ((melhor & 15) << 4) + 8;
        int tol = 30 + tolerancia * 400 / 100;
        int* fila = (int*)malloc(sizeof(int) * n);
        if (!fila) return 0;
        int ini = 0, fim = 0;
#define ED_FUNDO(c) (abs((int)ED_R(c) - fr) + abs((int)ED_G(c) - fg) + abs((int)ED_BL(c) - fb) <= tol)
        for (int x = 0; x < ed.w; x++) for (int k = 0; k < 2; k++) { int i = (k ? ed.h - 1 : 0) * ed.w + x; if (!ed.traco[i] && ED_FUNDO(ed.comp[i])) { ed.traco[i] = 255; fila[fim++] = i; } }
        for (int y = 0; y < ed.h; y++) for (int k = 0; k < 2; k++) { int i = y * ed.w + (k ? ed.w - 1 : 0); if (!ed.traco[i] && ED_FUNDO(ed.comp[i])) { ed.traco[i] = 255; fila[fim++] = i; } }
        while (ini < fim) {
            int i = fila[ini++], x = i % ed.w, y = i / ed.w;
            int viz[4] = { x > 0 ? i - 1 : -1, x < ed.w - 1 ? i + 1 : -1, y > 0 ? i - ed.w : -1, y < ed.h - 1 ? i + ed.w : -1 };
            for (int k = 0; k < 4; k++) { int j = viz[k]; if (j >= 0 && !ed.traco[j] && ED_FUNDO(ed.comp[j])) { ed.traco[j] = 255; fila[fim++] = j; } }
        }
#undef ED_FUNDO
        free(fila);
    }
    if (suavizar > 0) {
        uint32_t* t = (uint32_t*)malloc(n * 4);
        if (t) {
            for (size_t i = 0; i < n; i++) t[i] = 0xFF000000u | ed.traco[i];
            edDesfocarCaixa(t, ed.w, ed.h, suavizar);
            for (size_t i = 0; i < n; i++) ed.traco[i] = (uint8_t)(t[i] & 255);
            free(t);
        }
    }
    int removidos = 0;
    uint32_t* L = edAtiva();
    for (size_t i = 0; i < n; i++) {
        int m = ed.traco[i]; if (!m) continue;
        if (ed.sel) m = m * ed.sel[i] / 255;
        uint32_t c = L[i];
        L[i] = (c & 0x00FFFFFF) | ((uint32_t)(ED_A(c) * (255 - m) / 255) << 24);
        if (m > 128) removidos++;
    }
    edSujou();
    return removidos;
}

// ---------------------------------------------------------------- transformar
static bool edTrocarTamanhoDoc(int nw, int nh) {
    size_t n = (size_t)nw * nh;
    uint32_t* c = (uint32_t*)malloc(n * 4); uint8_t* t = (uint8_t*)calloc(n, 1); uint32_t* b = (uint32_t*)malloc(n * 4);
    if (!c || !t || !b) { free(c); free(t); free(b); return false; }
    free(ed.comp); free(ed.traco); free(ed.base); free(ed.sel);
    ed.comp = c; ed.traco = t; ed.base = b; ed.sel = NULL;
    ed.w = nw; ed.h = nh;
    return true;
}
// Gira 90 (1), 180 (2) ou 270 (3) graus; espelha (4 horizontal, 5 vertical).
static bool edGirarEspelhar(int op) {
    int nw = (op == 1 || op == 3) ? ed.h : ed.w, nh = (op == 1 || op == 3) ? ed.w : ed.h;
    int w = ed.w, h = ed.h;
    uint32_t* novos[ED_MAX_CAMADAS] = {};
    for (int c = 0; c < ed.n; c++) {
        novos[c] = (uint32_t*)malloc((size_t)nw * nh * 4);
        if (!novos[c]) { for (int k = 0; k <= c; k++) free(novos[k]); return false; }
        const uint32_t* s = ed.cam[c].px;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int nx, ny;
            switch (op) {
            case 1: nx = h - 1 - y; ny = x; break;
            case 2: nx = w - 1 - x; ny = h - 1 - y; break;
            case 3: nx = y; ny = w - 1 - x; break;
            case 4: nx = w - 1 - x; ny = y; break;
            default: nx = x; ny = h - 1 - y; break;
            }
            novos[c][(size_t)ny * nw + nx] = s[(size_t)y * w + x];
        }
    }
    if ((nw != w || nh != h) && !edTrocarTamanhoDoc(nw, nh)) { for (int k = 0; k < ed.n; k++) free(novos[k]); return false; }
    for (int c = 0; c < ed.n; c++) { free(ed.cam[c].px); ed.cam[c].px = novos[c]; }
    if (ed.sel && (op == 4 || op == 5 || op == 2)) { free(ed.sel); ed.sel = NULL; }
    edSujou();
    return true;
}
static inline uint32_t edAmostrar(const uint32_t* px, int w, int h, float x, float y) {
    x -= 0.5f; y -= 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = x - x0, fy = y - y0;
    float acc[4] = {}, pesoA = 0;
    for (int k = 0; k < 4; k++) {
        int xx = x0 + (k & 1), yy = y0 + (k >> 1);
        float p = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
        if (xx < 0 || yy < 0 || xx >= w || yy >= h || p <= 0) continue;
        uint32_t c = px[(size_t)yy * w + xx];
        float a = ED_A(c) * p;
        acc[0] += ED_R(c) * a; acc[1] += ED_G(c) * a; acc[2] += ED_BL(c) * a; pesoA += a;
    }
    if (pesoA <= 0) return 0;
    return edCor((int)(acc[0] / pesoA + .5f), (int)(acc[1] / pesoA + .5f), (int)(acc[2] / pesoA + .5f), (int)(pesoA + .5f));
}
// Gira em ângulo livre (mantém o tamanho; os cantos ficam transparentes) e/ou amplia para cobrir.
static bool edGirarLivre(float graus, bool cobrir) {
    float a = graus * 3.14159265f / 180, cs = cosf(a), sn = sinf(a);
    float esc = 1;
    if (cobrir) { float ww = ed.w, hh = ed.h; esc = (ww * fabsf(cs) + hh * fabsf(sn)) / ww; float e2 = (ww * fabsf(sn) + hh * fabsf(cs)) / hh; if (e2 > esc) esc = e2; }
    size_t n = (size_t)ed.w * ed.h;
    for (int c = 0; c < ed.n; c++) {
        uint32_t* o = (uint32_t*)malloc(n * 4);
        if (!o) return false;
        const uint32_t* s = ed.cam[c].px;
        float cx = ed.w / 2.0f, cy = ed.h / 2.0f;
        for (int y = 0; y < ed.h; y++) for (int x = 0; x < ed.w; x++) {
            float dx = (x + .5f - cx) / esc, dy = (y + .5f - cy) / esc;
            float sx = dx * cs + dy * sn + cx, sy = -dx * sn + dy * cs + cy;
            o[(size_t)y * ed.w + x] = edAmostrar(s, ed.w, ed.h, sx, sy);
        }
        free(ed.cam[c].px); ed.cam[c].px = o;
    }
    edSujou();
    return true;
}
static bool edRedimensionar(int nw, int nh) {
    nw = edLim(nw, 1, ED_MAX_LADO); nh = edLim(nh, 1, ED_MAX_LADO);
    uint32_t* novos[ED_MAX_CAMADAS] = {};
    for (int c = 0; c < ed.n; c++) {
        if (nw <= ed.w && nh <= ed.h) novos[c] = edReduzir(ed.cam[c].px, ed.w, ed.h, nw, nh);
        else {
            novos[c] = (uint32_t*)malloc((size_t)nw * nh * 4);
            if (novos[c]) for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) novos[c][(size_t)y * nw + x] = edAmostrar(ed.cam[c].px, ed.w, ed.h, (x + .5f) * ed.w / nw, (y + .5f) * ed.h / nh);
        }
        if (!novos[c]) { for (int k = 0; k <= c; k++) free(novos[k]); return false; }
    }
    if (!edTrocarTamanhoDoc(nw, nh)) { for (int k = 0; k < ed.n; k++) free(novos[k]); return false; }
    for (int c = 0; c < ed.n; c++) { free(ed.cam[c].px); ed.cam[c].px = novos[c]; }
    edSujou();
    return true;
}
static bool edCortar(int x0, int y0, int x1, int y1) {
    x0 = edLim(x0, 0, ed.w - 1); y0 = edLim(y0, 0, ed.h - 1); x1 = edLim(x1, x0 + 1, ed.w); y1 = edLim(y1, y0 + 1, ed.h);
    int nw = x1 - x0, nh = y1 - y0;
    uint32_t* novos[ED_MAX_CAMADAS] = {};
    for (int c = 0; c < ed.n; c++) {
        novos[c] = (uint32_t*)malloc((size_t)nw * nh * 4);
        if (!novos[c]) { for (int k = 0; k <= c; k++) free(novos[k]); return false; }
        for (int y = 0; y < nh; y++) memcpy(novos[c] + (size_t)y * nw, ed.cam[c].px + (size_t)(y + y0) * ed.w + x0, (size_t)nw * 4);
    }
    if (!edTrocarTamanhoDoc(nw, nh)) { for (int k = 0; k < ed.n; k++) free(novos[k]); return false; }
    for (int c = 0; c < ed.n; c++) { free(ed.cam[c].px); ed.cam[c].px = novos[c]; }
    edSujou();
    return true;
}
// Perspectiva: os 4 cantos da imagem vão para q[0..3] (sup-esq, sup-dir, inf-dir, inf-esq).
static bool edHomografia(const EdPt* de, const EdPt* para, double* H) {
    double A[8][9];
    for (int i = 0; i < 4; i++) {
        double x = de[i].x, y = de[i].y, u = para[i].x, v = para[i].y;
        double l1[9] = { x, y, 1, 0, 0, 0, -u * x, -u * y, u }, l2[9] = { 0, 0, 0, x, y, 1, -v * x, -v * y, v };
        memcpy(A[i * 2], l1, sizeof(l1)); memcpy(A[i * 2 + 1], l2, sizeof(l2));
    }
    for (int c = 0; c < 8; c++) {
        int p = c; for (int r = c + 1; r < 8; r++) if (fabs(A[r][c]) > fabs(A[p][c])) p = r;
        if (fabs(A[p][c]) < 1e-12) return false;
        if (p != c) for (int k = 0; k < 9; k++) { double t = A[c][k]; A[c][k] = A[p][k]; A[p][k] = t; }
        for (int r = 0; r < 8; r++) if (r != c) { double f = A[r][c] / A[c][c]; for (int k = c; k < 9; k++) A[r][k] -= f * A[c][k]; }
    }
    for (int i = 0; i < 8; i++) H[i] = A[i][8] / A[i][i];
    H[8] = 1;
    return true;
}
static bool edPerspectiva(const EdPt* cantos) {
    EdPt orig[4] = { { 0, 0 }, { (float)ed.w, 0 }, { (float)ed.w, (float)ed.h }, { 0, (float)ed.h } };
    double H[9];
    if (!edHomografia(cantos, orig, H)) return false;   // da posição nova para a original (amostragem inversa)
    size_t n = (size_t)ed.w * ed.h;
    for (int c = 0; c < ed.n; c++) {
        uint32_t* o = (uint32_t*)malloc(n * 4);
        if (!o) return false;
        for (int y = 0; y < ed.h; y++) for (int x = 0; x < ed.w; x++) {
            double X = x + .5, Y = y + .5, d = H[6] * X + H[7] * Y + H[8];
            float sx = (float)((H[0] * X + H[1] * Y + H[2]) / d), sy = (float)((H[3] * X + H[4] * Y + H[5]) / d);
            o[(size_t)y * ed.w + x] = (sx < 0 || sy < 0 || sx > ed.w || sy > ed.h) ? 0 : edAmostrar(ed.cam[c].px, ed.w, ed.h, sx, sy);
        }
        free(ed.cam[c].px); ed.cam[c].px = o;
    }
    edSujou();
    return true;
}
// Desloca o conteúdo da camada ativa (ferramenta Mover).
static void edDeslocarCamada(int dx, int dy) {
    if (!dx && !dy) return;
    size_t n = (size_t)ed.w * ed.h;
    uint32_t* o = (uint32_t*)calloc(n, 4);
    if (!o) return;
    const uint32_t* s = edAtiva();
    for (int y = 0; y < ed.h; y++) {
        int sy = y - dy; if (sy < 0 || sy >= ed.h) continue;
        for (int x = 0; x < ed.w; x++) { int sx = x - dx; if (sx >= 0 && sx < ed.w) o[(size_t)y * ed.w + x] = s[(size_t)sy * ed.w + sx]; }
    }
    free(ed.cam[ed.ativa].px); ed.cam[ed.ativa].px = o;
    edSujou();
}

// ---------------------------------------------------------------- objeto flutuante (texto, adesivo, imagem)
struct EdFlut { bool ativo; uint32_t* px; int w, h; float cx, cy, escala, angulo; int opacidade; int tipo; char nome[40]; };
static EdFlut edObj;
static void edObjLimpar() { free(edObj.px); memset(&edObj, 0, sizeof(edObj)); }
// Amostra do objeto no ponto (x, y) da imagem.
static inline uint32_t edObjEm(float x, float y) {
    float a = -edObj.angulo * 3.14159265f / 180, cs = cosf(a), sn = sinf(a);
    float dx = (x - edObj.cx) / edObj.escala, dy = (y - edObj.cy) / edObj.escala;
    float sx = dx * cs - dy * sn + edObj.w / 2.0f, sy = dx * sn + dy * cs + edObj.h / 2.0f;
    if (sx < -1 || sy < -1 || sx > edObj.w + 1 || sy > edObj.h + 1) return 0;
    return edAmostrar(edObj.px, edObj.w, edObj.h, sx, sy);
}
static void edObjCaixa(float* x0, float* y0, float* x1, float* y1) {
    float a = edObj.angulo * 3.14159265f / 180, cs = fabsf(cosf(a)), sn = fabsf(sinf(a));
    float hw = (edObj.w * cs + edObj.h * sn) / 2 * edObj.escala, hh = (edObj.w * sn + edObj.h * cs) / 2 * edObj.escala;
    *x0 = edObj.cx - hw; *x1 = edObj.cx + hw; *y0 = edObj.cy - hh; *y1 = edObj.cy + hh;
}
// Grava o objeto numa camada nova.
static bool edObjFixar() {
    if (!edObj.ativo || !edObj.px) return false;
    edGuardarPasso(1);
    if (!edNovaCamada(edObj.nome[0] ? edObj.nome : "Objeto", 0)) {
        // sem espaço para camada: grava na ativa
    }
    float fx0, fy0, fx1, fy1; edObjCaixa(&fx0, &fy0, &fx1, &fy1);
    int x0 = edLim((int)floorf(fx0), 0, ed.w), x1 = edLim((int)ceilf(fx1) + 1, 0, ed.w), y0 = edLim((int)floorf(fy0), 0, ed.h), y1 = edLim((int)ceilf(fy1) + 1, 0, ed.h);
    uint32_t* L = edAtiva();
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        uint32_t c = edObjEm(x + .5f, y + .5f);
        if (ED_A(c)) L[(size_t)y * ed.w + x] = edSobre(L[(size_t)y * ed.w + x], c, edObj.opacidade, ED_MIS_NORMAL);
    }
    edObjLimpar();
    edSujou();
    return true;
}
static bool edObjDeImagem(const uint32_t* px, int w, int h, const char* nome, int tipo) {
    edObjLimpar();
    edObj.px = (uint32_t*)malloc((size_t)w * h * 4);
    if (!edObj.px) return false;
    memcpy(edObj.px, px, (size_t)w * h * 4);
    edObj.w = w; edObj.h = h; edObj.ativo = true; edObj.escala = 1; edObj.opacidade = 100; edObj.tipo = tipo;
    edObj.cx = ed.w / 2.0f; edObj.cy = ed.h / 2.0f;
    snprintf(edObj.nome, sizeof(edObj.nome), "%s", nome);
    return true;
}

// ---------------------------------------------------------------- texto (stb_truetype)
#define ED_MAX_FONTES 80
struct EdFonteArq { char nome[64]; char caminho[300]; };
static EdFonteArq edFontes[ED_MAX_FONTES];
static int edTotalFontes = 0, edFonteCarregada = -1;
static unsigned char* edFonteDados = NULL;
static stbtt_fontinfo edFonteInfo;
static void edFonteAdicionar(const char* caminho, const char* nome) {
    if (edTotalFontes >= ED_MAX_FONTES) return;
    FILE* f = fopen(caminho, "rb"); if (!f) return; fclose(f);
    for (int i = 0; i < edTotalFontes; i++) if (!strcmp(edFontes[i].caminho, caminho)) return;
    snprintf(edFontes[edTotalFontes].caminho, sizeof(edFontes[0].caminho), "%s", caminho);
    snprintf(edFontes[edTotalFontes].nome, sizeof(edFontes[0].nome), "%s", nome);
    char* p = strrchr(edFontes[edTotalFontes].nome, '.'); if (p) *p = 0;
    edTotalFontes++;
}
static bool edFonteUsar(int i) {
    if (i == edFonteCarregada && edFonteDados) return true;
    if (i < 0 || i >= edTotalFontes) return false;
    FILE* f = fopen(edFontes[i].caminho, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* d = n > 0 && n < 40 * 1024 * 1024 ? (unsigned char*)malloc(n) : NULL;
    bool ok = d && fread(d, 1, n, f) == (size_t)n;
    fclose(f);
    int off = ok ? stbtt_GetFontOffsetForIndex(d, 0) : -1;
    if (!ok || off < 0 || !stbtt_InitFont(&edFonteInfo, d, off)) { free(d); return false; }
    free(edFonteDados); edFonteDados = d; edFonteCarregada = i;
    return true;
}
static int edUtf8(const char* s, uint32_t* cp) {
    unsigned char c = (unsigned char)s[0];
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t v = extra ? (c & (0x3Fu >> extra)) : c;
    int n = 1;
    for (int i = 0; i < extra && ((unsigned char)s[n] & 0xC0) == 0x80; i++) v = (v << 6) | ((unsigned char)s[n++] & 0x3F);
    *cp = v; return n;
}
struct EdTexto {
    char texto[600];
    int fonte, tamanho, contorno, sombra, fundo, alinhamento, espacoLetras, entrelinhas, curva, opacidade;
    uint32_t cor, corContorno, corFundo;
};
// Gera o texto como imagem (para o objeto flutuante). "|" quebra a linha.
static bool edTextoGerar(const EdTexto* t) {
    if (!edFonteUsar(t->fonte) && !edFonteUsar(0)) return false;
    float esc = stbtt_ScaleForPixelHeight(&edFonteInfo, (float)t->tamanho);
    int asc, desc, gap; stbtt_GetFontVMetrics(&edFonteInfo, &asc, &desc, &gap);
    float alturaLinha = (asc - desc + gap) * esc * (1 + t->entrelinhas / 100.0f);
    // linhas e larguras
    char linhas[16][300]; int nl = 0;
    { const char* p = t->texto; while (*p && nl < 16) { const char* q = strchr(p, '|'); int k = q ? (int)(q - p) : (int)strlen(p); if (k > 299) k = 299; memcpy(linhas[nl], p, k); linhas[nl][k] = 0; nl++; if (!q) break; p = q + 1; } }
    if (!nl) return false;
    float larg[16]; float maxL = 1;
    float espaco = t->espacoLetras / 100.0f * t->tamanho * 0.3f;
    for (int l = 0; l < nl; l++) {
        float x = 0; uint32_t ant = 0;
        for (const char* s = linhas[l]; *s; ) { uint32_t cp; s += edUtf8(s, &cp); int av, lsb; stbtt_GetCodepointHMetrics(&edFonteInfo, cp, &av, &lsb); if (ant) x += stbtt_GetCodepointKernAdvance(&edFonteInfo, ant, cp) * esc; x += av * esc + espaco; ant = cp; }
        larg[l] = x > 1 ? x : 1; if (larg[l] > maxL) maxL = larg[l];
    }
    int pad = t->contorno + (t->sombra ? t->tamanho / 12 + 4 : 0) + (t->fundo ? t->tamanho / 3 : 0) + 4;
    float curva = t->curva / 100.0f;      // 1 = meio círculo
    int W, H;
    float raio = 0;
    if (curva != 0) {
        raio = maxL / (fabsf(curva) * 3.14159265f);
        float meioAng = fabsf(curva) * 3.14159265f / 2;
        float flecha = raio * (1 - cosf(meioAng)) + (curva < 0 ? (nl - 1) * alturaLinha * (1 - cosf(meioAng)) : 0);
        float corda = meioAng < 3.14159265f / 2 ? 2 * raio * sinf(meioAng) : 2 * raio;
        if (curva < 0) corda += 2 * (nl - 1) * alturaLinha;
        W = (int)(corda + 2 * pad + 2 * t->tamanho); H = (int)(alturaLinha * nl + flecha + 2 * pad + t->tamanho);
    }
    else { W = (int)(maxL + 2 * pad + 2); H = (int)(alturaLinha * nl + 2 * pad + 2); }
    if (W > 4096 || H > 4096 || W < 1 || H < 1) return false;
    float* A = (float*)calloc((size_t)W * H, sizeof(float));
    if (!A) return false;
    for (int l = 0; l < nl; l++) {
        float x = t->alinhamento == 1 ? (maxL - larg[l]) / 2 : t->alinhamento == 2 ? maxL - larg[l] : 0;
        float base = pad + asc * esc + l * alturaLinha;
        uint32_t ant = 0;
        for (const char* s = linhas[l]; *s; ) {
            uint32_t cp; s += edUtf8(s, &cp);
            int av, lsb; stbtt_GetCodepointHMetrics(&edFonteInfo, cp, &av, &lsb);
            if (ant) x += stbtt_GetCodepointKernAdvance(&edFonteInfo, ant, cp) * esc;
            int gw, gh, gx, gy;
            unsigned char* bm = stbtt_GetCodepointBitmap(&edFonteInfo, esc, esc, cp, &gw, &gh, &gx, &gy);
            float avanco = av * esc + espaco;
            if (bm) {
                if (curva == 0) {
                    int ox = (int)(pad + x + gx + 0.5f), oy = (int)(base + gy + 0.5f);
                    for (int j = 0; j < gh; j++) for (int i = 0; i < gw; i++) {
                        int px = ox + i, py = oy + j; if (px < 0 || py < 0 || px >= W || py >= H) continue;
                        float v = bm[j * gw + i] / 255.0f; if (v > A[(size_t)py * W + px]) A[(size_t)py * W + px] = v;
                    }
                }
                else {
                    // letra girada sobre o arco: centro do arco embaixo (curva > 0) ou em cima
                    // Curva > 0: arco-íris (centro embaixo); < 0: sorriso (centro em cima).
                    float meio = x + avanco / 2 - maxL / 2;      // distância ao longo do arco
                    float r = curva > 0 ? raio - l * alturaLinha : raio + l * alturaLinha;
                    if (r < t->tamanho) r = (float)t->tamanho;
                    float ang = meio / r;
                    float cxA = W / 2.0f;
                    float cyA = curva > 0 ? pad + asc * esc + raio : H - pad + desc * esc - raio - (nl - 1) * alturaLinha;
                    float px0 = cxA + sinf(ang) * r, py0 = curva > 0 ? cyA - cosf(ang) * r : cyA + cosf(ang) * r;
                    float giro = curva > 0 ? ang : -ang;
                    float cs = cosf(giro), sn = sinf(giro);
                    // pixel da letra (i,j) relativo à linha de base no centro do avanço
                    float lx0 = gx - avanco / 2 + espaco / 2, ly0 = gy;
                    int R2 = (int)(sqrtf((float)(gw * gw + gh * gh)) + fabsf(lx0) + fabsf(ly0) + 2);
                    for (int yy = (int)py0 - R2; yy <= (int)py0 + R2; yy++) for (int xx = (int)px0 - R2; xx <= (int)px0 + R2; xx++) {
                        if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                        float dx = xx + .5f - px0, dy = yy + .5f - py0;
                        float u = dx * cs + dy * sn - lx0, v = -dx * sn + dy * cs - ly0;
                        int iu = (int)floorf(u), iv = (int)floorf(v);
                        if (iu < 0 || iv < 0 || iu >= gw || iv >= gh) continue;
                        float a = bm[iv * gw + iu] / 255.0f; if (a > A[(size_t)yy * W + xx]) A[(size_t)yy * W + xx] = a;
                    }
                }
                stbtt_FreeBitmap(bm, NULL);
            }
            x += avanco; ant = cp;
        }
    }
    // contorno (dilatação em disco)
    float* O = NULL;
    if (t->contorno > 0) {
        int r = t->contorno;
        O = (float*)calloc((size_t)W * H, sizeof(float));
        if (O) {
            float* tmp = (float*)calloc((size_t)W * H, sizeof(float));
            if (tmp) {
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float m = 0; for (int d = -r; d <= r; d++) { int xx = x + d; if (xx >= 0 && xx < W && A[(size_t)y * W + xx] > m) m = A[(size_t)y * W + xx]; } tmp[(size_t)y * W + x] = m; }
                // vertical com o perfil do disco
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                    float m = 0;
                    for (int d = -r; d <= r; d++) { int yy = y + d; if (yy < 0 || yy >= H) continue; float v = tmp[(size_t)yy * W + x]; if (v > m) m = v; }
                    O[(size_t)y * W + x] = m;
                }
                free(tmp);
            }
        }
    }
    uint32_t* px = (uint32_t*)calloc((size_t)W * H, 4);
    if (!px) { free(A); free(O); return false; }
    if (t->fundo) {
        // caixa arredondada atrás do texto
        float x0 = 2, y0 = 2, x1 = W - 2.0f, y1 = H - 2.0f, rr = t->tamanho * 0.25f;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            float qx = fmaxf(fmaxf(x0 + rr - x, x - (x1 - rr)), 0), qy = fmaxf(fmaxf(y0 + rr - y, y - (y1 - rr)), 0);
            if (qx * qx + qy * qy <= rr * rr) px[(size_t)y * W + x] = t->corFundo;
        }
    }
    const float* forma = O ? O : A;
    if (t->sombra) {
        int d = t->tamanho / 14 + 2;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            int sx = x - d, sy = y - d; if (sx < 0 || sy < 0) continue;
            float v = forma[(size_t)sy * W + sx] * 0.6f; if (v <= 0) continue;
            px[(size_t)y * W + x] = edSobre(px[(size_t)y * W + x], edCor(0, 0, 0, (int)(v * 255)), 100, ED_MIS_NORMAL);
        }
    }
    for (size_t i = 0, n = (size_t)W * H; i < n; i++) {
        if (O && O[i] > 0) px[i] = edSobre(px[i], (t->corContorno & 0x00FFFFFF) | ((uint32_t)(ED_A(t->corContorno) * O[i]) << 24), 100, ED_MIS_NORMAL);
        if (A[i] > 0) px[i] = edSobre(px[i], (t->cor & 0x00FFFFFF) | ((uint32_t)(ED_A(t->cor) * A[i]) << 24), 100, ED_MIS_NORMAL);
    }
    free(A); free(O);
    float cx = edObj.ativo ? edObj.cx : ed.w / 2.0f, cy = edObj.ativo ? edObj.cy : ed.h / 2.0f, esc2 = edObj.ativo && edObj.tipo == 1 ? edObj.escala : 1, ang = edObj.ativo && edObj.tipo == 1 ? edObj.angulo : 0;
    edObjLimpar();
    edObj.px = px; edObj.w = W; edObj.h = H; edObj.ativo = true; edObj.cx = cx; edObj.cy = cy; edObj.escala = esc2; edObj.angulo = ang; edObj.opacidade = t->opacidade; edObj.tipo = 1;
    snprintf(edObj.nome, sizeof(edObj.nome), "Texto");
    return true;
}

// Carimbo: copia da origem (deslocada) para a camada, pela máscara do traço.
static void edCarimboAplicar(int dx, int dy, int opacidade) {
    uint32_t* L = edAtiva();
    for (int y = edTy0; y < edTy1; y++) for (int x = edTx0; x < edTx1; x++) {
        size_t i = (size_t)y * ed.w + x;
        int m = ed.traco[i] * opacidade / 100; if (ed.sel) m = m * ed.sel[i] / 255;
        uint32_t b = ed.base[i];
        if (!m) { L[i] = b; continue; }
        int sx = x + dx, sy = y + dy;
        if (sx < 0 || sy < 0 || sx >= ed.w || sy >= ed.h) { L[i] = b; continue; }
        uint32_t s = ed.base[(size_t)sy * ed.w + sx];
        L[i] = edCor((ED_R(b) * (255 - m) + ED_R(s) * m) / 255, (ED_G(b) * (255 - m) + ED_G(s) * m) / 255, (ED_BL(b) * (255 - m) + ED_BL(s) * m) / 255, (ED_A(b) * (255 - m) + ED_A(s) * m) / 255);
    }
    ed.compSujo = true; ed.mudou = true;
}
// Retoque com pincel: 0 olhos vermelhos, 1 suavizar (corretivo), 2 pixelizar, 3 clarear, 4 escurecer, 5 desfocar, 6 saturar
static void edRetoqueAplicar(int modo, int forca) {
    int x0 = edTx0, y0 = edTy0, x1 = edTx1, y1 = edTy1;
    if (x0 >= x1 || y0 >= y1) return;
    int w = x1 - x0, h = y1 - y0;
    uint32_t* reg = NULL;
    if (modo == 1 || modo == 2 || modo == 5) {
        reg = (uint32_t*)malloc((size_t)w * h * 4);
        if (!reg) return;
        for (int y = 0; y < h; y++) memcpy(reg + (size_t)y * w, ed.base + (size_t)(y + y0) * ed.w + x0, (size_t)w * 4);
        if (modo == 2) { EdAjustes a = {}; a.pixelizar = 6 + forca / 6; uint32_t* t = (uint32_t*)malloc((size_t)w * h * 4); if (t) { edProcessar(reg, t, w, h, &a, 1); free(reg); reg = t; } }
        else edDesfocarCaixa(reg, w, h, modo == 1 ? 6 : 3);
    }
    uint32_t* L = edAtiva();
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        size_t i = (size_t)y * ed.w + x;
        int m = ed.traco[i] * forca / 100; if (ed.sel) m = m * ed.sel[i] / 255;
        uint32_t b = ed.base[i];
        if (!m) { L[i] = b; continue; }
        uint32_t s = b;
        int r = ED_R(b), g = ED_G(b), bl = ED_BL(b);
        switch (modo) {
        case 0: if (r > g + 20 && r > bl + 20) { int v = (g + bl) / 2; s = edCor(v, g, bl, ED_A(b)); } break;
        case 3: s = edCor(r + (255 - r) / 3, g + (255 - g) / 3, bl + (255 - bl) / 3, ED_A(b)); break;
        case 4: s = edCor(r * 2 / 3, g * 2 / 3, bl * 2 / 3, ED_A(b)); break;
        case 6: { float L2 = 0.299f * r + 0.587f * g + 0.114f * bl; s = edCor((int)(L2 + (r - L2) * 1.5f), (int)(L2 + (g - L2) * 1.5f), (int)(L2 + (bl - L2) * 1.5f), ED_A(b)); break; }
        default: s = reg[(size_t)(y - y0) * w + (x - x0)]; break;
        }
        L[i] = edCor((ED_R(b) * (255 - m) + ED_R(s) * m) / 255, (ED_G(b) * (255 - m) + ED_G(s) * m) / 255, (ED_BL(b) * (255 - m) + ED_BL(s) * m) / 255, (ED_A(b) * (255 - m) + ED_A(s) * m) / 255);
    }
    free(reg);
    ed.compSujo = true; ed.mudou = true;
}
