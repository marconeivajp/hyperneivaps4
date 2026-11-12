// =========================================================================
// EDITOR DE IMAGEM: tela, ferramentas, painéis e controles.
// Precisa, antes do include (network.cpp no PS4, teste no PC):
//   edPlatTexto, edPlatLargura, edPlatAgoraMs, edPlatTecladoAbrir, edPlatTecladoResultado,
//   edPlatToque, edPlatAnalogico, edPlatEmoji, edPlatUsarImagem, edPlatConquista, edPlatCriarPasta
// Botões: os mesmos ORBIS_PAD_BUTTON_*.
// =========================================================================
#pragma once
#include "editor_imagem.h"
#include <dirent.h>
#include <sys/stat.h>

#define ED_VX 236
#define ED_VY 78
#define ED_VW 1268
#define ED_VH 924
#define ED_PX 1512
#define ED_PW 400

bool editorImagemAberto = false;

enum {
    ED_T_MOVER, ED_T_PINCEL, ED_T_BORRACHA, ED_T_LINHA, ED_T_FORMA, ED_T_TEXTO, ED_T_ADESIVO, ED_T_BALDE, ED_T_DEGRADE, ED_T_CONTAGOTAS,
    ED_T_COR, ED_T_SELECAO, ED_T_CARIMBO, ED_T_RETOQUE, ED_T_CORTAR, ED_T_TRANSFORMAR, ED_T_LUZ, ED_T_CORES, ED_T_CURVAS, ED_T_HSL,
    ED_T_DETALHES, ED_T_EFEITOS, ED_T_FILTROS, ED_T_FUNDO, ED_T_CAMADAS, ED_T_TOTAL
};
static const char* const edNomesFerramenta[ED_T_TOTAL] = {
    "Mover", "Pincel", "Borracha", "Linha", "Formas", "Texto", "Adesivos", "Balde", "Degrade", "Conta-gotas",
    "Cor", "Selecao", "Carimbo", "Retoque", "Cortar", "Girar / espelhar", "Luz", "Cor da foto", "Curvas", "HSL",
    "Detalhes", "Efeitos", "Filtros", "Remover fundo", "Camadas"
};
static bool edFerramentaAjuste(int t) { return t >= ED_T_LUZ && t <= ED_T_EFEITOS; }

// ---------------------------------------------------------------- estado da tela
static int edFerramenta = ED_T_PINCEL;
static int edLinhaSel[ED_T_TOTAL];
static float edZoom = 1, edCx = 0, edCy = 0;
static float edCurX = ED_VX + ED_VW / 2, edCurY = ED_VY + ED_VH / 2;
static bool edApertado = false, edArrastando = false;
static float edIniX, edIniY, edUltX, edUltY;          // em coordenadas da imagem
static uint64_t edUltimoMs = 0;
static char edAvisoTxt[200] = "";
static uint64_t edAvisoAte = 0;
static bool edGrade = false;
static uint32_t* edOriginal = NULL; static int edOrigW = 0, edOrigH = 0;
static bool edMostrarOriginal = false;
static int edImeModo = 0;                             // 1 texto, 2 cor HEX, 3 nome da camada
static int edOpPendente = 0; static bool edOpMostrada = false; static char edOpTexto[80] = "";
static int edMenu = 0, edMenuSel = 0;                 // 1 menu Options, 2 novo, 3 seletor, 4 sair, 5 usar como
static bool edPainelFoco = false;
static uint32_t edBotoesAntes = 0;
static uint64_t edRepeteEm = 0; static int edRepeteN = 0;

// Cores
static uint32_t edCor1 = 0xFFE53935, edCor2 = 0xFFFFFFFF;
static int edCorEditando = 0, edCorH = 0, edCorS = 0, edCorV = 0, edCorA = 255, edPaleta = 0, edPaletaItem = 0;
// Pincel / borracha / linha / formas
static int edPTam = 20, edPDureza = 70, edPOpac = 100, edPTipo = 0, edSimetria = 0;
static int edBTam = 40, edBDureza = 50, edBOpac = 100;
static int edLEsp = 8, edLTravar = 0, edLSeta = 0;
static int edFTipo = 0, edFPreencher = 0, edFContorno = 0, edFOpac = 100, edFQuadrado = 0;
// Texto
static EdTexto edTx = { "", 0, 96, 6, 1, 0, 1, 0, 0, 0, 100, 0xFFFFFFFF, 0xFF000000, 0xB4000000 };
// Adesivos
static int edAdTipo = 0, edAdItem = 0, edAdTam = 256;
// Balde, degradê, conta-gotas
static int edBalTol = 25, edBalGlobal = 0, edBalOpac = 100;
static int edDgTipo = 0, edDgOpac = 100, edDgTransp = 0;
static int edCgDestino = 0;
// Seleção
static int edSelTipo = 0, edSelModo = 0, edSelTol = 30, edSelSuave = 0;
static EdPt* edLaco = NULL; static int edLacoN = 0;
// Carimbo e retoque
static int edCaTam = 30, edCaDureza = 50, edCaOpac = 100; static float edCaOx = -1, edCaOy = -1, edCaDx = 0, edCaDy = 0; static bool edCaDefinido = false;
static int edReModo = 0, edReTam = 30, edReForca = 80;
// Cortar
static int edCtProp = 0; static float edCt[4] = { 0, 0, 0, 0 }; static bool edCtAtivo = false;
// Transformar
static int edTrAngulo = 0, edTrEscala = 100, edTrCanto = 0; static EdPt edTrCantos[4]; static bool edTrPerspAtiva = false;
// Ajustes e filtros
static EdAjustes edAj = {};
static int edCurvaCanal = 0, edHslCor = 0;
static int edFiltro = 0, edFiltroForca = 100;
// Fundo
static int edFuModo = 0, edFuTol = 25, edFuSuave = 1;
// Camadas
static int edCamOpac = 100;

// Prévia (imagem reduzida com os ajustes)
static uint32_t* edPrevBase = NULL; static uint32_t* edPrev = NULL;
static int edPrevW = 0, edPrevH = 0; static unsigned edPrevBaseVer = 0, edPrevVer = 0, edParamVer = 1; static uint64_t edParamMs = 0;

static void edAvisar(const char* t) { snprintf(edAvisoTxt, sizeof(edAvisoTxt), "%s", t); edAvisoAte = edPlatAgoraMs() + 2600; }
static void edParametroMudou() { edParamVer++; edParamMs = edPlatAgoraMs(); }

// ---------------------------------------------------------------- vista
static void edAjustarVista() {
    float zx = (float)ED_VW / ed.w, zy = (float)ED_VH / ed.h;
    edZoom = (zx < zy ? zx : zy) * 0.94f;
    edCx = ed.w / 2.0f; edCy = ed.h / 2.0f;
}
static inline float edImgX(float sx) { return edCx + (sx - (ED_VX + ED_VW / 2.0f)) / edZoom; }
static inline float edImgY(float sy) { return edCy + (sy - (ED_VY + ED_VH / 2.0f)) / edZoom; }
static inline float edTelaX(float ix) { return ED_VX + ED_VW / 2.0f + (ix - edCx) * edZoom; }
static inline float edTelaY(float iy) { return ED_VY + ED_VH / 2.0f + (iy - edCy) * edZoom; }

// ---------------------------------------------------------------- desenho na tela
static void edRet(uint32_t* p, int x, int y, int w, int h, uint32_t c) {
    int x0 = edLim(x, 0, 1920), x1 = edLim(x + w, 0, 1920), y0 = edLim(y, 0, 1080), y1 = edLim(y + h, 0, 1080);
    for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) p[yy * 1920 + xx] = c;
}
static void edRetBorda(uint32_t* p, int x, int y, int w, int h, uint32_t c, int e) { edRet(p, x, y, w, e, c); edRet(p, x, y + h - e, w, e, c); edRet(p, x, y, e, h, c); edRet(p, x + w - e, y, e, h, c); }
static void edPonto(uint32_t* p, int x, int y, uint32_t c) {
    if (x < ED_VX || y < ED_VY || x >= ED_VX + ED_VW || y >= ED_VY + ED_VH) return;
    p[y * 1920 + x] = c;
}
static void edTelaLinha(uint32_t* p, float ax, float ay, float bx, float by, uint32_t c, bool tracejada) {
    float d = fmaxf(fabsf(bx - ax), fabsf(by - ay));
    int n = (int)d + 1; if (n > 6000) n = 6000;
    for (int i = 0; i <= n; i++) {
        if (tracejada && (i / 6) % 2) continue;
        float t = (float)i / n;
        int x = (int)(ax + (bx - ax) * t), y = (int)(ay + (by - ay) * t);
        edPonto(p, x, y, c); edPonto(p, x + 1, y, (i / 6) % 2 ? c : 0xFF000000);
    }
}
static void edTelaCirculo(uint32_t* p, float cx, float cy, float r, uint32_t c) {
    int n = (int)(r * 6.3f) + 8; if (n > 2000) n = 2000;
    for (int i = 0; i < n; i++) { float a = i * 6.2831853f / n; edPonto(p, (int)(cx + cosf(a) * r), (int)(cy + sinf(a) * r), (i / 4) % 2 ? c : 0xFF000000); }
}
static void edTexto(uint32_t* p, const char* t, int tam, int x, int y, uint32_t c) { edPlatTexto(p, t, tam, x, y, c); }
static void edTextoCortado(uint32_t* p, const char* t, int tam, int x, int y, int largura, uint32_t c) {
    char b[200]; snprintf(b, sizeof(b), "%s", t);
    if (edPlatLargura(b, tam) > largura) {
        size_t n = strlen(b);
        while (n > 1) {
            n--; while (n > 0 && ((unsigned char)b[n] & 0xC0) == 0x80) n--;   // não corta no meio de uma letra
            b[n] = 0;
            char c2[204]; snprintf(c2, sizeof(c2), "%s..", b);
            if (edPlatLargura(c2, tam) <= largura) { snprintf(b, sizeof(b), "%s", c2); break; }
        }
    }
    edTexto(p, b, tam, x, y, c);
}

// ---------------------------------------------------------------- painel: linhas
enum { ED_L_SLIDER, ED_L_OPCAO, ED_L_ACAO, ED_L_INFO };
struct EdLinha { char rotulo[56]; int tipo; int* v; int mn, mx, passo; const char* const* ops; int nops; int acao; };
static EdLinha edLinhas[48];
static int edNL = 0;
static EdLinha* edL(const char* r, int tipo) { EdLinha* l = &edLinhas[edNL < 47 ? edNL++ : 47]; memset(l, 0, sizeof(*l)); snprintf(l->rotulo, sizeof(l->rotulo), "%s", r); l->tipo = tipo; return l; }
static void edSl(const char* r, int* v, int mn, int mx, int passo) { EdLinha* l = edL(r, ED_L_SLIDER); l->v = v; l->mn = mn; l->mx = mx; l->passo = passo; }
static void edOp(const char* r, int* v, const char* const* ops, int n) { EdLinha* l = edL(r, ED_L_OPCAO); l->v = v; l->ops = ops; l->nops = n; }
static void edAc(const char* r, int acao) { EdLinha* l = edL(r, ED_L_ACAO); l->acao = acao; }
static void edIn(const char* r) { edL(r, ED_L_INFO); }

static const char* const edNaoSim[] = { "Nao", "Sim" };
static const char* const edTiposPincel[] = { "Pincel", "Lapis", "Marcador", "Spray" };
static const char* const edSimetrias[] = { "Nenhuma", "Espelho horizontal", "Espelho vertical", "Nos dois eixos" };
static const char* const edPreenchimentos[] = { "Cor 1, contorno cor 2", "So contorno (cor 1)" };
static const char* const edAlinhamentos[] = { "Esquerda", "Centro", "Direita" };
static const char* const edTiposAdesivo[] = { "Emojis", "Simbolos", "Meus adesivos" };
static const char* const edTiposDegrade[] = { "Linear", "Radial" };
static const char* const edDestinosCg[] = { "Cor 1", "Cor 2" };
static const char* const edTiposSel[] = { "Retangulo", "Elipse", "Laco (livre)", "Varinha magica" };
static const char* const edModosSel[] = { "Nova", "Somar", "Tirar" };
static const char* const edModosRetoque[] = { "Olhos vermelhos", "Corretivo (suaviza)", "Pixelizar area", "Clarear", "Escurecer", "Desfocar", "Saturar" };
static const char* const edProporcoes[] = { "Livre", "Original", "1:1", "4:3", "3:4", "16:9", "9:16", "3:2", "Avatar PS4 (440)", "Fundo PS4 (1920x1080)" };
static const char* const edCantos[] = { "Nenhum", "Superior esquerdo", "Superior direito", "Inferior direito", "Inferior esquerdo" };
static const char* const edCanaisCurva[] = { "Geral", "Vermelho", "Verde", "Azul" };
static const char* const edCoresHsl[] = { "Vermelho", "Laranja", "Amarelo", "Verde", "Ciano", "Azul", "Roxo", "Magenta" };
static const char* const edModosFundo[] = { "Pelas bordas (fundo liso)", "Cor-chave (cor 1)" };
static const char* const edEditandoCor[] = { "Cor 1", "Cor 2" };
static const char* const edPaletas[] = { "Basica", "Pastel", "Neon", "PlayStation", "Pele", "Salvas" };
static const char* const edSetas[] = { "Sem ponta", "Seta no fim", "Setas nas pontas" };

static uint32_t edPaletaCores[6][16] = {
    { 0xFF000000, 0xFFFFFFFF, 0xFF808080, 0xFFE53935, 0xFFFF9800, 0xFFFFEB3B, 0xFF4CAF50, 0xFF00BCD4, 0xFF2196F3, 0xFF3F51B5, 0xFF9C27B0, 0xFFE91E63, 0xFF795548, 0xFF607D8B, 0xFFC0C0C0, 0xFF1B5E20 },
    { 0xFFFFD1DC, 0xFFFFE4B5, 0xFFFFFACD, 0xFFC1F0C1, 0xFFB5EAEA, 0xFFAEC6CF, 0xFFCBAACB, 0xFFF5DEB3, 0xFFFFB3BA, 0xFFFFDFBA, 0xFFFFFFBA, 0xFFBAFFC9, 0xFFBAE1FF, 0xFFE0BBE4, 0xFFFEC8D8, 0xFFD5AAFF },
    { 0xFFFF00FF, 0xFF00FFFF, 0xFF39FF14, 0xFFFFFF00, 0xFFFF3131, 0xFFFF6EC7, 0xFF7DF9FF, 0xFFBC13FE, 0xFF0FF0FC, 0xFFFE019A, 0xFFCCFF00, 0xFFFF5F1F, 0xFF04D9FF, 0xFFFFF01F, 0xFF8F00FF, 0xFF00FF7F },
    { 0xFF003791, 0xFF0070D1, 0xFF7FA8E0, 0xFFFF6B6B, 0xFFE58BC6, 0xFF3DD6A6, 0xFF1F1F1F, 0xFFFFFFFF, 0xFF00439C, 0xFF2D2D2D, 0xFFB0B0B0, 0xFF5C5C5C, 0xFF0B1E3F, 0xFF4F9DFF, 0xFFD9E7FF, 0xFF00296B },
    { 0xFFFFDBAC, 0xFFF1C27D, 0xFFE0AC69, 0xFFC68642, 0xFF8D5524, 0xFF5C3A1E, 0xFFFFE0BD, 0xFFFFCD94, 0xFFEAC086, 0xFFFFAD60, 0xFFE8B98A, 0xFFD2996C, 0xFFA67358, 0xFF7C4A35, 0xFF4A2C1D, 0xFF2E1A12 },
    { 0 }
};
#define ED_PALETA_ARQ "/data/HyperNeiva/editor/paleta.txt"
static void edPaletaCarregar() {
    FILE* f = fopen(ED_PALETA_ARQ, "r");
    if (!f) return;
    for (int i = 0; i < 16; i++) { unsigned v; if (fscanf(f, "%x", &v) != 1) break; edPaletaCores[5][i] = v; }
    fclose(f);
}
static void edPaletaSalvar() {
    FILE* f = fopen(ED_PALETA_ARQ, "w");
    if (!f) return;
    for (int i = 0; i < 16; i++) fprintf(f, "%08X\n", edPaletaCores[5][i]);
    fclose(f);
}
static void edCorParaSliders() { uint32_t c = edCorEditando ? edCor2 : edCor1; edCorHsv(c, &edCorH, &edCorS, &edCorV); edCorA = ED_A(c); }
static void edSlidersParaCor() { uint32_t c = edHsvCor((float)edCorH, edCorS / 100.0f, edCorV / 100.0f, edCorA); if (edCorEditando) edCor2 = c; else edCor1 = c; }

// ---------------------------------------------------------------- adesivos
static const char* const edEmojis[] = {
    "1f600", "1f602", "1f923", "1f60d", "1f618", "1f60e", "1f914", "1f644", "1f62d", "1f621", "1f631", "1f973", "1f929", "1f607", "1f60a", "1f609",
    "1f61c", "1f92a", "1f97a", "1f634", "1f480", "1f47b", "1f47d", "1f916", "1f4a9", "1f525", "2b50", "2728", "1f4a5", "1f4af", "2764", "1f9e1",
    "1f49b", "1f49a", "1f499", "1f49c", "1f5a4", "1f494", "1f495", "1f44d", "1f44e", "1f44f", "1f64f", "1f4aa", "270c", "1f918", "1f44c", "1f440",
    "1f3ae", "1f579", "1f3c6", "1f947", "1f389", "1f381", "1f382", "1f3b5", "1f3a7", "1f4f7", "1f4f1", "1f4bb", "1f680", "26a1", "2600", "1f319",
    "1f308", "2601", "2744", "1f30a", "1f334", "1f338", "1f339", "1f340", "1f355", "1f354", "1f37f", "1f37a", "2615", "1f436", "1f431", "1f981",
    "1f43c", "1f984", "1f98b", "1f40d", "1f1e7-1f1f7", "26bd", "1f3c0", "1f697", "2708", "1f3e0", "1f4a1", "1f4b0", "1f48e", "1f451", "1f4cc", "1f4ac",
};
#define ED_TOTAL_EMOJIS ((int)(sizeof(edEmojis) / sizeof(edEmojis[0])))
static const char* const edSimbolosEmoji[] = { "2705", "274c", "26a0", "2753", "2757", "27a1", "2b05", "2b06", "2b07", "1f51d", "1f195", "1f4af", "2b50", "2764", "1f6ab", "267b" };
#define ED_TOTAL_SIMBOLOS (4 + (int)(sizeof(edSimbolosEmoji) / sizeof(edSimbolosEmoji[0])))
static char edAdesivosArq[64][128]; static int edTotalAdesivos = 0;
static void edCarregarAdesivos() {
    edTotalAdesivos = 0;
    DIR* d = opendir("/data/HyperNeiva/editor/adesivos");
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && edTotalAdesivos < 64) {
        const char* x = strrchr(e->d_name, '.');
        if (x && (!strcasecmp(x, ".png") || !strcasecmp(x, ".jpg") || !strcasecmp(x, ".jpeg") || !strcasecmp(x, ".gif") || !strcasecmp(x, ".bmp"))) snprintf(edAdesivosArq[edTotalAdesivos++], 128, "%s", e->d_name);
    }
    closedir(d);
}
static int edTotalItensAdesivo() { return edAdTipo == 0 ? ED_TOTAL_EMOJIS : edAdTipo == 1 ? ED_TOTAL_SIMBOLOS : edTotalAdesivos; }
// Botão do PlayStation desenhado (0 X, 1 bola, 2 quadrado, 3 triângulo).
static uint32_t* edSimboloPs(int qual, int lado) {
    uint32_t* px = (uint32_t*)calloc((size_t)lado * lado, 4);
    if (!px) return NULL;
    static const uint32_t cores[4] = { 0xFF7FA8E0, 0xFFFF6B6B, 0xFFE58BC6, 0xFF3DD6A6 };
    float c = lado / 2.0f, R = lado * 0.48f, e = lado * 0.055f, s = lado * 0.24f;
    for (int y = 0; y < lado; y++) for (int x = 0; x < lado; x++) {
        float fx = x + .5f - c, fy = y + .5f - c;
        float dc = sqrtf(fx * fx + fy * fy);
        float fundo = edLimF(R - dc + 0.5f, 0, 1);
        if (fundo <= 0) continue;
        float d;
        if (qual == 0) { float d1 = fabsf(fx - fy) / 1.41421f, d2 = fabsf(fx + fy) / 1.41421f; d = (fabsf(fx) <= s && fabsf(fy) <= s) ? fminf(d1, d2) : 1e9f; }
        else if (qual == 1) d = fabsf(dc - s);
        else if (qual == 2) { float ax = fabsf(fx), ay = fabsf(fy); float k = s * 0.85f; d = (ax <= k + e && ay <= k + e) ? fminf(fabsf(ax - k) + (ay > k ? ay - k : 0), fabsf(ay - k) + (ax > k ? ax - k : 0)) : 1e9f; if (ax <= k && ay <= k) d = fminf(k - ax, k - ay); }
        else {
            EdPt t[3] = { { 0, -s * 1.05f }, { s * 1.05f, s * 0.75f }, { -s * 1.05f, s * 0.75f } };
            d = 1e9f;
            for (int i = 0; i < 3; i++) {
                EdPt a = t[i], b = t[(i + 1) % 3]; float vx = b.x - a.x, vy = b.y - a.y, wx = fx - a.x, wy = fy - a.y;
                float tt = edLimF((wx * vx + wy * vy) / (vx * vx + vy * vy), 0, 1); float qx = a.x + vx * tt - fx, qy = a.y + vy * tt - fy;
                d = fminf(d, sqrtf(qx * qx + qy * qy));
            }
        }
        float sim = edLimF(e - d + 0.5f, 0, 1);
        uint32_t base = edCor(30, 30, 36, (int)(fundo * 255));
        uint32_t cc = cores[qual];
        px[(size_t)y * lado + x] = sim > 0 ? edSobre(base, (cc & 0x00FFFFFF) | ((uint32_t)(sim * 255) << 24), 100, ED_MIS_NORMAL) : base;
    }
    return px;
}
// Imagem do adesivo escolhido (o chamador libera). w e h de saída.
static uint32_t* edAdesivoImagem(int* w, int* h) {
    if (edAdTipo == 0 || (edAdTipo == 1 && edAdItem >= 4)) {
        const char* nome = edAdTipo == 0 ? edEmojis[edLim(edAdItem, 0, ED_TOTAL_EMOJIS - 1)] : edSimbolosEmoji[edLim(edAdItem - 4, 0, ED_TOTAL_SIMBOLOS - 5)];
        uint32_t rgba[72 * 72];
        if (!edPlatEmoji(nome, rgba)) return NULL;
        uint32_t* o = (uint32_t*)malloc(72 * 72 * 4);
        if (!o) return NULL;
        for (int i = 0; i < 72 * 72; i++) { const unsigned char* b = (const unsigned char*)&rgba[i]; o[i] = edCor(b[0], b[1], b[2], b[3]); }
        // amplia para ficar bom em tamanho grande
        int L = 288; uint32_t* g = (uint32_t*)malloc((size_t)L * L * 4);
        if (g) { for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) g[(size_t)y * L + x] = edAmostrar(o, 72, 72, (x + .5f) * 72 / L, (y + .5f) * 72 / L); free(o); *w = *h = L; return g; }
        *w = *h = 72; return o;
    }
    if (edAdTipo == 1) { *w = *h = 288; return edSimboloPs(edLim(edAdItem, 0, 3), 288); }
    if (edAdItem < 0 || edAdItem >= edTotalAdesivos) return NULL;
    char c[300]; snprintf(c, sizeof(c), "/data/HyperNeiva/editor/adesivos/%s", edAdesivosArq[edAdItem]);
    int iw, ih, ic; unsigned char* d = stbi_load(c, &iw, &ih, &ic, 4);
    if (!d) return NULL;
    uint32_t* o = edRgbaParaInterno(d, iw, ih); stbi_image_free(d);
    *w = iw; *h = ih; return o;
}
// Miniatura do adesivo para o painel (cache).
static uint32_t* edAdMini = NULL; static int edAdMiniW = 0, edAdMiniH = 0, edAdMiniDe = -1;
static void edAdMiniAtualizar() {
    int chave = edAdTipo * 1000 + edAdItem;
    if (chave == edAdMiniDe) return;
    free(edAdMini); edAdMini = edAdesivoImagem(&edAdMiniW, &edAdMiniH); edAdMiniDe = chave;
}

// ---------------------------------------------------------------- arquivos e pastas
static void edCriarPastas() { edPlatCriarPasta("/data/HyperNeiva/editor"); edPlatCriarPasta("/data/HyperNeiva/editor/projetos"); edPlatCriarPasta("/data/HyperNeiva/editor/adesivos"); edPlatCriarPasta("/data/HyperNeiva/fontes"); edPlatCriarPasta("/data/HyperNeiva/midia"); edPlatCriarPasta("/data/HyperNeiva/midia/imagens"); }
static void edCarregarFontes() {
    if (edTotalFontes) return;
    edFonteAdicionar("/app0/assets/fonts/font.ttf", "Hyper Neiva");
    const char* pastas[2] = { "/data/HyperNeiva/fontes", "/preinst/common/font" };
    for (int k = 0; k < 2; k++) {
        DIR* d = opendir(pastas[k]);
        if (!d) continue;
        struct dirent* e;
        while ((e = readdir(d))) {
            const char* x = strrchr(e->d_name, '.');
            if (!x || (strcasecmp(x, ".ttf") && strcasecmp(x, ".otf"))) continue;
            char c[400]; snprintf(c, sizeof(c), "%s/%s", pastas[k], e->d_name);
            edFonteAdicionar(c, e->d_name);
        }
        closedir(d);
    }
}
static void edCaminhoSaida(char* saida, int tam, const char* ext) {
    char base[512];
    const char* fonte = ed.caminho[0] ? ed.caminho : "";
    const char* xx = strrchr(fonte, '.');
    if (fonte[0] && xx && strcasecmp(xx, ".hnimg") != 0) {
        snprintf(base, sizeof(base), "%.*s", (int)(xx - fonte), fonte);
        size_t n = strlen(base);
        if (n > 8 && !strcmp(base + n - 8, "_editado")) base[n - 8] = 0;
        snprintf(saida, tam, "%s_editado.%s", base, ext);
        return;
    }
    for (int i = 1; i < 10000; i++) {
        snprintf(saida, tam, "/data/HyperNeiva/editor/imagem_%03d.%s", i, ext);
        struct stat st; if (stat(saida, &st) != 0) return;
    }
}
static bool edSalvar(int formato, char* caminhoSaida, int tam) {
    edCompor();
    char c[512];
    edCaminhoSaida(c, sizeof(c), formato == 1 ? "jpg" : "png");
    bool ok = formato == 1 ? edSalvarJpg(c, ed.comp, ed.w, ed.h, 92) : edSalvarPng(c, ed.comp, ed.w, ed.h);
    if (ok) { ed.mudou = false; if (caminhoSaida) snprintf(caminhoSaida, tam, "%s", c); }
    char m[300]; snprintf(m, sizeof(m), ok ? "Salvo em %.240s" : "Nao consegui salvar em %.220s", c);
    edAvisar(m);
    return ok;
}

// Seletor de arquivos
struct EdItemArq { char nome[200]; bool pasta; };
static EdItemArq* edArqs = NULL; static int edTotalArqs = 0, edArqSel = 0, edArqTopo = 0;
static char edArqPasta[512] = ""; static int edArqFinalidade = 0;   // 0 abrir, 1 inserir como camada, 2 colagem
static int edColagemN = 0, edColagemCelula = 0;
static bool edEhImagem(const char* n) {
    const char* x = strrchr(n, '.'); if (!x) return false;
    return !strcasecmp(x, ".png") || !strcasecmp(x, ".jpg") || !strcasecmp(x, ".jpeg") || !strcasecmp(x, ".bmp") || !strcasecmp(x, ".gif") || !strcasecmp(x, ".tga") || !strcasecmp(x, ".psd") || !strcasecmp(x, ".hnimg");
}
static int edCmpArq(const void* a, const void* b) { const EdItemArq* x = (const EdItemArq*)a; const EdItemArq* y = (const EdItemArq*)b; if (x->pasta != y->pasta) return x->pasta ? -1 : 1; return strcasecmp(x->nome, y->nome); }
static void edListarPasta(const char* pasta) {
    if (!edArqs) edArqs = (EdItemArq*)malloc(sizeof(EdItemArq) * 2000);
    if (!edArqs) return;
    snprintf(edArqPasta, sizeof(edArqPasta), "%s", pasta);
    edTotalArqs = 0; edArqSel = 0; edArqTopo = 0;
    if (!pasta[0]) {
        const char* raizes[] = { "/data/HyperNeiva/midia/imagens", "/data/HyperNeiva/editor", "/data/HyperNeiva/baixado", "/data/HyperNeiva", "/mnt/usb0", "/mnt/usb1", "/data" };
        for (int i = 0; i < 7; i++) { snprintf(edArqs[edTotalArqs].nome, 200, "%s", raizes[i]); edArqs[edTotalArqs++].pasta = true; }
        return;
    }
    DIR* d = opendir(pasta);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && edTotalArqs < 2000) {
        if (e->d_name[0] == '.') continue;
        char c[800]; snprintf(c, sizeof(c), "%s/%s", pasta, e->d_name);
        struct stat st; bool dir = stat(c, &st) == 0 && S_ISDIR(st.st_mode);
        if (!dir && !edEhImagem(e->d_name)) continue;
        snprintf(edArqs[edTotalArqs].nome, 200, "%s", e->d_name); edArqs[edTotalArqs++].pasta = dir;
    }
    closedir(d);
    qsort(edArqs, edTotalArqs, sizeof(EdItemArq), edCmpArq);
}
static void edAbrirSeletor(int finalidade) {
    edArqFinalidade = finalidade;
    char pasta[512] = "/data/HyperNeiva/midia/imagens";
    if (ed.caminho[0] && finalidade != 2) { snprintf(pasta, sizeof(pasta), "%s", ed.caminho); char* b = strrchr(pasta, '/'); if (b && b != pasta) *b = 0; }
    edListarPasta(pasta);
    if (!edTotalArqs) edListarPasta("");
    edMenu = 3;
}

// ---------------------------------------------------------------- abrir / fechar
static void edGuardarOriginal() {
    edCompor();
    free(edOriginal); edOriginal = (uint32_t*)malloc((size_t)ed.w * ed.h * 4);
    if (edOriginal) { memcpy(edOriginal, ed.comp, (size_t)ed.w * ed.h * 4); edOrigW = ed.w; edOrigH = ed.h; }
}
static void edDocPronto() {
    edCompor(); edGuardarOriginal(); edAjustarVista();
    memset(&edAj, 0, sizeof(edAj)); edFiltro = 0; edObjLimpar(); edCtAtivo = false; edTrPerspAtiva = false; edCaDefinido = false;
    edParametroMudou(); edPrevBaseVer = 0;
}
static bool edNovoCanvas(int w, int h, uint32_t fundo) {
    if (!edNovoDoc(w, h, fundo)) { edAvisar("Sem memoria para esse tamanho."); return false; }
    ed.caminho[0] = 0; ed.mudou = false; edLimparHistorico(); edDocPronto();
    return true;
}
// Abre o editor com uma imagem (ou tela nova, se caminho for NULL).
bool editorImagemAbrir(const char* caminho) {
    edCriarPastas(); edCarregarFontes(); edPaletaCarregar(); edMontarFiltros(); edCarregarAdesivos();
    bool ok;
    if (caminho && caminho[0]) {
        const char* x = strrchr(caminho, '.');
        ok = (x && !strcasecmp(x, ".hnimg")) ? edAbrirProjeto(caminho) : edAbrirImagem(caminho);
        if (ok) edDocPronto();
    }
    else ok = edNovoCanvas(1920, 1080, 0xFFFFFFFF);
    if (!ok) { edAvisar("Nao consegui abrir a imagem."); return false; }
    editorImagemAberto = true;
    edMenu = caminho && caminho[0] ? 0 : 2; edMenuSel = 0;
    edUltimoMs = edPlatAgoraMs();
    edCorParaSliders();
    edPlatConquista("editor");
    return true;
}
static void edFechar() {
    editorImagemAberto = false;
    edObjLimpar(); edLimparHistorico(); edLiberarDoc();
    free(edOriginal); edOriginal = NULL; free(edPrevBase); edPrevBase = NULL; free(edPrev); edPrev = NULL; edPrevW = edPrevH = 0;
    free(edLaco); edLaco = NULL; edLacoN = 0;
    edLiberarTrabalho(); edObjLimpar();
    free(edAdMini); edAdMini = NULL; edAdMiniDe = -1;
}

// ---------------------------------------------------------------- aplicar pendentes
static bool edAjustePendente() { return !edAjNeutro(&edAj); }
static void edAplicarAjustes() {
    if (!edAjustePendente()) return;
    edGuardarPasso(0); edAplicarNaCamada(&edAj); memset(&edAj, 0, sizeof(edAj)); edParametroMudou();
    edAvisar("Ajustes aplicados (Quadrado desfaz)");
}
static void edAplicarFiltro() {
    if (!edFiltro) return;
    EdAjustes a = edFiltroEm(edFiltro, edFiltroForca);
    edGuardarPasso(0); edAplicarNaCamada(&a); edFiltro = 0; edParametroMudou();
    edAvisar("Filtro aplicado (Quadrado desfaz)");
}
// Operações pesadas: mostram "Processando..." num quadro e rodam no seguinte.
enum { ED_OP_AJUSTES = 1, ED_OP_FILTRO, ED_OP_FUNDO, ED_OP_GIRAR_LIVRE, ED_OP_ESCALA, ED_OP_PERSPECTIVA, ED_OP_SALVAR_PNG, ED_OP_SALVAR_JPG, ED_OP_PROJETO, ED_OP_USAR, ED_OP_CORTAR, ED_OP_SAIR_SALVANDO };
static void edAgendar(int op, const char* texto) { edOpPendente = op; edOpMostrada = false; snprintf(edOpTexto, sizeof(edOpTexto), "%s", texto); }

static void edTrocarFerramenta(int nova) {
    if (nova == edFerramenta) return;
    if (edFerramentaAjuste(edFerramenta) && !edFerramentaAjuste(nova) && edAjustePendente()) edAgendar(ED_OP_AJUSTES, "Aplicando ajustes...");
    if (edFerramenta == ED_T_FILTROS && edFiltro) edAgendar(ED_OP_FILTRO, "Aplicando filtro...");
    if (edObj.ativo && edObj.tipo == 1 && nova != ED_T_TEXTO) { edObjFixar(); edAvisar("Texto fixado numa camada nova"); }
    if (edObj.ativo && edObj.tipo != 1 && nova != ED_T_ADESIVO && nova != ED_T_MOVER) { edObjFixar(); edAvisar("Objeto fixado numa camada nova"); }
    edFerramenta = nova;
    edArrastando = false;
    if (nova == ED_T_CORTAR && !edCtAtivo) { edCt[0] = 0; edCt[1] = 0; edCt[2] = (float)ed.w; edCt[3] = (float)ed.h; edCtAtivo = true; }
    if (nova == ED_T_TRANSFORMAR && !edTrPerspAtiva) { edTrCantos[0] = { 0, 0 }; edTrCantos[1] = { (float)ed.w, 0 }; edTrCantos[2] = { (float)ed.w, (float)ed.h }; edTrCantos[3] = { 0, (float)ed.h }; }
    if (nova == ED_T_COR) edCorParaSliders();
    if (nova == ED_T_ADESIVO) edAdMiniDe = -1;
    edParametroMudou();
}

// ---------------------------------------------------------------- monta o painel da ferramenta
enum {
    ED_A_TROCAR_CORES = 1, ED_A_HEX, ED_A_PALETA_USAR, ED_A_PALETA_SALVAR, ED_A_TEXTO_ESCREVER, ED_A_TEXTO_FIXAR, ED_A_SEL_TUDO, ED_A_SEL_INVERTER, ED_A_SEL_NENHUMA,
    ED_A_SEL_APAGAR, ED_A_SEL_COPIAR, ED_A_SEL_PREENCHER, ED_A_CORTAR, ED_A_CORTAR_TUDO, ED_A_G90E, ED_A_G90D, ED_A_G180, ED_A_ESPH, ED_A_ESPV, ED_A_GLIVRE, ED_A_ESCALA,
    ED_A_PERSP, ED_A_PERSP_ZERAR, ED_A_AJ_APLICAR, ED_A_AJ_ZERAR, ED_A_FILTRO_APLICAR, ED_A_FUNDO, ED_A_CAM_NOVA, ED_A_CAM_DUP, ED_A_CAM_APAGAR, ED_A_CAM_SUBIR,
    ED_A_CAM_DESCER, ED_A_CAM_MESCLAR, ED_A_CAM_ACHATAR, ED_A_CAM_NOME, ED_A_CAM_VISIVEL, ED_A_ADESIVO_POR, ED_A_OBJ_FIXAR, ED_A_OBJ_CANCELAR, ED_A_CAM_DESLOCAR_ZERO,
    ED_A_CARIMBO_INFO
};
static int edCamVisivel = 1, edCamMistura = 0;
static const char* edNomesCamadas[ED_MAX_CAMADAS];
static void edMontarPainel() {
    edNL = 0;
    char t[80];
    switch (edFerramenta) {
    case ED_T_MOVER:
        edIn("X arrasta a camada ativa");
        edIn("Com objeto solto: analogico move,");
        edIn("L2/R2 tamanho, analogico dir. gira");
        if (edObj.ativo) { edAc("Fixar o objeto", ED_A_OBJ_FIXAR); edAc("Descartar o objeto", ED_A_OBJ_CANCELAR); }
        break;
    case ED_T_PINCEL:
        edOp("Tipo", &edPTipo, edTiposPincel, 4);
        edSl("Tamanho", &edPTam, 1, 400, 1); edSl("Dureza", &edPDureza, 0, 100, 5); edSl("Opacidade", &edPOpac, 1, 100, 5);
        edOp("Simetria", &edSimetria, edSimetrias, 4);
        edIn("Touchpad desenha direto na tela");
        break;
    case ED_T_BORRACHA:
        edSl("Tamanho", &edBTam, 1, 400, 1); edSl("Dureza", &edBDureza, 0, 100, 5); edSl("Forca", &edBOpac, 1, 100, 5); edOp("Simetria", &edSimetria, edSimetrias, 4);
        break;
    case ED_T_LINHA:
        edSl("Espessura", &edLEsp, 1, 200, 1); edSl("Opacidade", &edPOpac, 1, 100, 5); edOp("Travar 45 graus", &edLTravar, edNaoSim, 2); edOp("Pontas", &edLSeta, edSetas, 3);
        edIn("Segure X e arraste");
        break;
    case ED_T_FORMA:
        edOp("Forma", &edFTipo, edNomesForma, ED_F_TOTAL - 1);
        edOp("Preenchimento", &edFPreencher, edPreenchimentos, 2);
        edSl("Contorno", &edFContorno, 0, 80, 1); edSl("Opacidade", &edFOpac, 1, 100, 5); edOp("Proporcao fixa", &edFQuadrado, edNaoSim, 2);
        edIn("Segure X e arraste");
        break;
    case ED_T_TEXTO:
        edAc(edObj.ativo && edObj.tipo == 1 ? "Mudar o texto" : "Escrever texto", ED_A_TEXTO_ESCREVER);
        { EdLinha* l = edL("Fonte", ED_L_OPCAO); l->v = &edTx.fonte; l->nops = edTotalFontes; l->ops = NULL; }
        edSl("Tamanho", &edTx.tamanho, 8, 600, 2); edSl("Contorno (cor 2)", &edTx.contorno, 0, 40, 1);
        edOp("Sombra", &edTx.sombra, edNaoSim, 2); edOp("Fundo (cor 2)", &edTx.fundo, edNaoSim, 2); edOp("Alinhamento", &edTx.alinhamento, edAlinhamentos, 3);
        edSl("Espaco entre letras", &edTx.espacoLetras, -50, 200, 5); edSl("Entrelinhas", &edTx.entrelinhas, -50, 200, 5);
        edSl("Curva", &edTx.curva, -100, 100, 5); edSl("Opacidade", &edTx.opacidade, 5, 100, 5);
        if (edObj.ativo && edObj.tipo == 1) edAc("Fixar o texto", ED_A_TEXTO_FIXAR);
        edIn("Cor do texto = cor 1. | quebra linha");
        break;
    case ED_T_ADESIVO: {
        edOp("Tipo", &edAdTipo, edTiposAdesivo, 3);
        int total = edTotalItensAdesivo();
        snprintf(t, sizeof(t), "Item (%d)", total);
        edSl(t, &edAdItem, 0, total > 0 ? total - 1 : 0, 1);
        edSl("Tamanho", &edAdTam, 16, 1400, 8);
        edAc(edObj.ativo ? "Fixar" : "Colocar no centro", edObj.ativo ? ED_A_OBJ_FIXAR : ED_A_ADESIVO_POR);
        if (edAdTipo == 2) edIn("PNGs em /data/HyperNeiva/editor/adesivos");
        break;
    }
    case ED_T_BALDE:
        edSl("Tolerancia", &edBalTol, 0, 100, 1); edOp("Pintar toda cor igual", &edBalGlobal, edNaoSim, 2); edSl("Opacidade", &edBalOpac, 1, 100, 5);
        break;
    case ED_T_DEGRADE:
        edOp("Tipo", &edDgTipo, edTiposDegrade, 2); edSl("Opacidade", &edDgOpac, 1, 100, 5); edOp("Cor 2 transparente", &edDgTransp, edNaoSim, 2);
        edIn("Segure X e arraste (cor 1 > cor 2)");
        break;
    case ED_T_CONTAGOTAS:
        edOp("Pegar para", &edCgDestino, edDestinosCg, 2); edIn("X pega a cor da imagem");
        break;
    case ED_T_COR:
        edOp("Editando", &edCorEditando, edEditandoCor, 2);
        edSl("Matiz", &edCorH, 0, 359, 2); edSl("Saturacao", &edCorS, 0, 100, 2); edSl("Brilho", &edCorV, 0, 100, 2); edSl("Transparencia", &edCorA, 0, 255, 5);
        edAc("Trocar cor 1 e cor 2", ED_A_TROCAR_CORES); edAc("Digitar codigo HEX", ED_A_HEX);
        edOp("Paleta", &edPaleta, edPaletas, 6); edSl("Cor da paleta", &edPaletaItem, 0, 15, 1);
        edAc("Usar a cor da paleta", ED_A_PALETA_USAR); edAc("Guardar em Salvas", ED_A_PALETA_SALVAR);
        break;
    case ED_T_SELECAO:
        edOp("Tipo", &edSelTipo, edTiposSel, 4); edOp("Modo", &edSelModo, edModosSel, 3); edSl("Tolerancia (varinha)", &edSelTol, 0, 100, 1); edSl("Suavizar borda", &edSelSuave, 0, 30, 1);
        edAc("Selecionar tudo", ED_A_SEL_TUDO); edAc("Inverter", ED_A_SEL_INVERTER); edAc("Tirar selecao", ED_A_SEL_NENHUMA);
        edAc("Apagar o conteudo", ED_A_SEL_APAGAR); edAc("Copiar para camada nova", ED_A_SEL_COPIAR); edAc("Preencher com a cor 1", ED_A_SEL_PREENCHER);
        break;
    case ED_T_CARIMBO:
        edSl("Tamanho", &edCaTam, 1, 300, 1); edSl("Dureza", &edCaDureza, 0, 100, 5); edSl("Opacidade", &edCaOpac, 1, 100, 5);
        edIn(edCaDefinido ? "Origem definida (R3 troca)" : "R3 define a origem da copia");
        break;
    case ED_T_RETOQUE:
        edOp("Modo", &edReModo, edModosRetoque, 7); edSl("Tamanho", &edReTam, 2, 300, 1); edSl("Forca", &edReForca, 5, 100, 5);
        break;
    case ED_T_CORTAR:
        edOp("Proporcao", &edCtProp, edProporcoes, 10); edAc("Cortar", ED_A_CORTAR); edAc("Imagem inteira", ED_A_CORTAR_TUDO);
        edIn("Segure X e marque a area");
        break;
    case ED_T_TRANSFORMAR:
        edAc("Girar 90 graus a esquerda", ED_A_G90E); edAc("Girar 90 graus a direita", ED_A_G90D); edAc("Girar 180 graus", ED_A_G180);
        edAc("Espelhar na horizontal", ED_A_ESPH); edAc("Espelhar na vertical", ED_A_ESPV);
        edSl("Angulo livre", &edTrAngulo, -45, 45, 1); edAc("Endireitar (aplicar angulo)", ED_A_GLIVRE);
        edSl("Tamanho (%)", &edTrEscala, 10, 300, 5); snprintf(t, sizeof(t), "Redimensionar para %dx%d", ed.w * edTrEscala / 100, ed.h * edTrEscala / 100); edAc(t, ED_A_ESCALA);
        edOp("Perspectiva: canto", &edTrCanto, edCantos, 5); edAc("Aplicar perspectiva", ED_A_PERSP); edAc("Zerar perspectiva", ED_A_PERSP_ZERAR);
        break;
    case ED_T_LUZ:
        edSl("Exposicao", &edAj.exposicao, -100, 100, 2); edSl("Brilho", &edAj.brilho, -100, 100, 2); edSl("Contraste", &edAj.contraste, -100, 100, 2);
        edSl("Realces", &edAj.realces, -100, 100, 2); edSl("Sombras", &edAj.sombras, -100, 100, 2); edSl("Faixa clara", &edAj.claros, -100, 100, 2); edSl("Faixa escura", &edAj.escuros, -100, 100, 2);
        edAc("Aplicar", ED_A_AJ_APLICAR); edAc("Zerar", ED_A_AJ_ZERAR);
        break;
    case ED_T_CORES:
        edSl("Saturacao", &edAj.saturacao, -100, 100, 2); edSl("Vibracao", &edAj.vibracao, -100, 100, 2); edSl("Calor", &edAj.calor, -100, 100, 2); edSl("Tom (verde/magenta)", &edAj.tom, -100, 100, 2);
        edSl("Colorir", &edAj.colorir, 0, 100, 2); edSl("Cor do colorir", &edAj.colorirMatiz, 0, 359, 3);
        edAc("Aplicar", ED_A_AJ_APLICAR); edAc("Zerar", ED_A_AJ_ZERAR);
        break;
    case ED_T_CURVAS: {
        edOp("Canal", &edCurvaCanal, edCanaisCurva, 4);
        static const char* const pts[5] = { "Pretos (0%)", "Sombras (25%)", "Meios (50%)", "Luzes (75%)", "Brancos (100%)" };
        for (int i = 0; i < 5; i++) edSl(pts[i], &edAj.curva[edCurvaCanal][i], -100, 100, 2);
        edAc("Aplicar", ED_A_AJ_APLICAR); edAc("Zerar", ED_A_AJ_ZERAR);
        break;
    }
    case ED_T_HSL:
        edOp("Cor", &edHslCor, edCoresHsl, 8);
        edSl("Matiz", &edAj.hsl[edHslCor][0], -100, 100, 2); edSl("Saturacao", &edAj.hsl[edHslCor][1], -100, 100, 2); edSl("Luminosidade", &edAj.hsl[edHslCor][2], -100, 100, 2);
        edAc("Aplicar", ED_A_AJ_APLICAR); edAc("Zerar", ED_A_AJ_ZERAR);
        break;
    case ED_T_DETALHES:
        edSl("Clareza", &edAj.clareza, -100, 100, 2); edSl("Nitidez", &edAj.nitidez, 0, 100, 2); edSl("Desfoque", &edAj.desfoque, 0, 100, 2);
        edSl("Remover ruido", &edAj.removerRuido, 0, 100, 5); edSl("Adicionar ruido", &edAj.ruido, 0, 100, 2);
        edAc("Aplicar", ED_A_AJ_APLICAR); edAc("Zerar", ED_A_AJ_ZERAR);
        break;
    case ED_T_EFEITOS:
        edSl("Vinheta", &edAj.vinheta, -100, 100, 2); edSl("Posterizar (niveis)", &edAj.posterizar, 0, 16, 1); edSl("Dispersao", &edAj.dispersao, 0, 40, 1); edSl("Pixelizar", &edAj.pixelizar, 0, 64, 1);
        edAc("Aplicar", ED_A_AJ_APLICAR); edAc("Zerar", ED_A_AJ_ZERAR);
        break;
    case ED_T_FILTROS: {
        static const char* nomesF[24]; for (int i = 0; i < edTotalFiltros; i++) nomesF[i] = edFiltros[i].nome;
        edOp("Filtro", &edFiltro, nomesF, edTotalFiltros); edSl("Intensidade", &edFiltroForca, 0, 100, 5); edAc("Aplicar filtro", ED_A_FILTRO_APLICAR);
        break;
    }
    case ED_T_FUNDO:
        edOp("Modo", &edFuModo, edModosFundo, 2); edSl("Tolerancia", &edFuTol, 0, 100, 1); edSl("Suavizar borda", &edFuSuave, 0, 12, 1);
        edAc("Remover o fundo", ED_A_FUNDO);
        edIn("Fundo complicado: use Selecao >");
        edIn("Varinha e Apagar o conteudo");
        break;
    case ED_T_CAMADAS:
        for (int i = 0; i < ed.n; i++) edNomesCamadas[i] = ed.cam[i].nome;
        { EdLinha* l = edL("Camada ativa", ED_L_OPCAO); l->v = &ed.ativa; l->ops = edNomesCamadas; l->nops = ed.n; }
        edCamVisivel = ed.cam[ed.ativa].visivel; edCamOpac = ed.cam[ed.ativa].opacidade; edCamMistura = ed.cam[ed.ativa].mistura;
        edOp("Visivel", &edCamVisivel, edNaoSim, 2); edSl("Opacidade", &edCamOpac, 0, 100, 5); edOp("Mistura", &edCamMistura, edNomesMistura, ED_MIS_TOTAL);
        edAc("Nova camada", ED_A_CAM_NOVA); edAc("Duplicar", ED_A_CAM_DUP); edAc("Apagar", ED_A_CAM_APAGAR); edAc("Subir", ED_A_CAM_SUBIR); edAc("Descer", ED_A_CAM_DESCER);
        edAc("Mesclar com a de baixo", ED_A_CAM_MESCLAR); edAc("Juntar todas", ED_A_CAM_ACHATAR); edAc("Renomear", ED_A_CAM_NOME);
        break;
    }
}

// ---------------------------------------------------------------- ações do painel
static void edExecutarAcao(int a, int32_t usuario) {
    switch (a) {
    case ED_A_TROCAR_CORES: { uint32_t t = edCor1; edCor1 = edCor2; edCor2 = t; edCorParaSliders(); break; }
    case ED_A_HEX: { char ini[16]; snprintf(ini, sizeof(ini), "%06X", (edCorEditando ? edCor2 : edCor1) & 0xFFFFFF); if (edPlatTecladoAbrir(usuario, "Cor HEX (ex.: FF8800)", 8, ini)) edImeModo = 2; break; }
    case ED_A_PALETA_USAR: { uint32_t c = edPaletaCores[edPaleta][edPaletaItem]; if (edCorEditando) edCor2 = c; else edCor1 = c; edCorParaSliders(); break; }
    case ED_A_PALETA_SALVAR: memmove(&edPaletaCores[5][1], &edPaletaCores[5][0], 15 * 4); edPaletaCores[5][0] = edCorEditando ? edCor2 : edCor1; edPaletaSalvar(); edAvisar("Cor guardada na paleta Salvas"); break;
    case ED_A_TEXTO_ESCREVER: if (edPlatTecladoAbrir(usuario, "Texto (| quebra a linha)", 200, edTx.texto)) edImeModo = 1; break;
    case ED_A_TEXTO_FIXAR: case ED_A_OBJ_FIXAR: if (edObjFixar()) edAvisar("Fixado numa camada nova"); break;
    case ED_A_OBJ_CANCELAR: edObjLimpar(); break;
    case ED_A_ADESIVO_POR: {
        int w, h; uint32_t* px = edAdesivoImagem(&w, &h);
        if (!px) { edAvisar("Adesivo indisponivel"); break; }
        edObjDeImagem(px, w, h, "Adesivo", 2); free(px);
        edObj.escala = (float)edAdTam / (w > h ? w : h);
        edObj.cx = edLimF(edImgX(edCurX), 0, (float)ed.w); edObj.cy = edLimF(edImgY(edCurY), 0, (float)ed.h);
        break;
    }
    case ED_A_SEL_TUDO: edSelTudo(); break;
    case ED_A_SEL_INVERTER: edSelInverter(); break;
    case ED_A_SEL_NENHUMA: edSelNenhuma(); break;
    case ED_A_SEL_APAGAR: {
        if (!ed.sel) { edAvisar("Nada selecionado"); break; }
        edGuardarPasso(0);
        uint32_t* L = edAtiva();
        for (size_t i = 0, n = (size_t)ed.w * ed.h; i < n; i++) if (ed.sel[i]) L[i] = (L[i] & 0x00FFFFFF) | ((uint32_t)(ED_A(L[i]) * (255 - ed.sel[i]) / 255) << 24);
        edSujou(); break;
    }
    case ED_A_SEL_COPIAR: {
        if (!ed.sel) { edAvisar("Nada selecionado"); break; }
        edGuardarPasso(1);
        uint32_t* orig = edAtiva();
        if (!edNovaCamada("Recorte", 0)) { edAvisar("Limite de camadas"); break; }
        uint32_t* L = edAtiva();
        for (size_t i = 0, n = (size_t)ed.w * ed.h; i < n; i++) if (ed.sel[i]) L[i] = (orig[i] & 0x00FFFFFF) | ((uint32_t)(ED_A(orig[i]) * ed.sel[i] / 255) << 24);
        edSujou(); break;
    }
    case ED_A_SEL_PREENCHER: {
        edGuardarPasso(0);
        uint32_t* L = edAtiva();
        for (size_t i = 0, n = (size_t)ed.w * ed.h; i < n; i++) { int m = ed.sel ? ed.sel[i] : 255; if (m) L[i] = edSobre(L[i], edCor1, m * 100 / 255, ED_MIS_NORMAL); }
        edSujou(); break;
    }
    case ED_A_CORTAR: edAgendar(ED_OP_CORTAR, "Cortando..."); break;
    case ED_A_CORTAR_TUDO: edCt[0] = 0; edCt[1] = 0; edCt[2] = (float)ed.w; edCt[3] = (float)ed.h; break;
    case ED_A_G90E: edGuardarPasso(1); edGirarEspelhar(3); edAjustarVista(); break;
    case ED_A_G90D: edGuardarPasso(1); edGirarEspelhar(1); edAjustarVista(); break;
    case ED_A_G180: edGuardarPasso(1); edGirarEspelhar(2); break;
    case ED_A_ESPH: edGuardarPasso(1); edGirarEspelhar(4); break;
    case ED_A_ESPV: edGuardarPasso(1); edGirarEspelhar(5); break;
    case ED_A_GLIVRE: if (edTrAngulo) edAgendar(ED_OP_GIRAR_LIVRE, "Girando..."); break;
    case ED_A_ESCALA: if (edTrEscala != 100) edAgendar(ED_OP_ESCALA, "Redimensionando..."); break;
    case ED_A_PERSP: if (edTrPerspAtiva) edAgendar(ED_OP_PERSPECTIVA, "Aplicando perspectiva..."); else edAvisar("Escolha um canto e arraste com X"); break;
    case ED_A_PERSP_ZERAR: edTrPerspAtiva = false; edTrCantos[0] = { 0, 0 }; edTrCantos[1] = { (float)ed.w, 0 }; edTrCantos[2] = { (float)ed.w, (float)ed.h }; edTrCantos[3] = { 0, (float)ed.h }; break;
    case ED_A_AJ_APLICAR: edAgendar(ED_OP_AJUSTES, "Aplicando ajustes..."); break;
    case ED_A_AJ_ZERAR: memset(&edAj, 0, sizeof(edAj)); edParametroMudou(); break;
    case ED_A_FILTRO_APLICAR: edAgendar(ED_OP_FILTRO, "Aplicando filtro..."); break;
    case ED_A_FUNDO: edAgendar(ED_OP_FUNDO, "Removendo o fundo..."); break;
    case ED_A_CAM_NOVA: edGuardarPasso(1); if (!edNovaCamada("Camada", 0)) edAvisar("Limite de 12 camadas (ou sem memoria)"); break;
    case ED_A_CAM_DUP: edGuardarPasso(1); if (!edDuplicarCamada()) edAvisar("Limite de 12 camadas (ou sem memoria)"); break;
    case ED_A_CAM_APAGAR: if (ed.n > 1) { edGuardarPasso(1); edApagarCamada(); } else edAvisar("A imagem precisa de uma camada"); break;
    case ED_A_CAM_SUBIR: edGuardarPasso(1); edMoverCamada(1); break;
    case ED_A_CAM_DESCER: edGuardarPasso(1); edMoverCamada(-1); break;
    case ED_A_CAM_MESCLAR: if (ed.ativa > 0) { edGuardarPasso(1); edMesclarAbaixo(); } break;
    case ED_A_CAM_ACHATAR: if (ed.n > 1) { edGuardarPasso(1); edAchatar(); } break;
    case ED_A_CAM_NOME: if (edPlatTecladoAbrir(usuario, "Nome da camada", 30, ed.cam[ed.ativa].nome)) edImeModo = 3; break;
    }
    edParametroMudou();
}
// Valor mudou no painel: efeitos colaterais.
static void edValorMudou(EdLinha* l) {
    if (edFerramenta == ED_T_COR) { if (l->v == &edCorEditando) edCorParaSliders(); else if (l->v != &edPaleta && l->v != &edPaletaItem) edSlidersParaCor(); }
    if (edFerramenta == ED_T_CAMADAS) {
        if (l->v == &edCamVisivel || l->v == &edCamOpac || l->v == &edCamMistura) {
            ed.cam[ed.ativa].visivel = edCamVisivel != 0; ed.cam[ed.ativa].opacidade = edCamOpac; ed.cam[ed.ativa].mistura = edCamMistura; edSujou();
        }
    }
    if (edFerramenta == ED_T_TEXTO && edObj.ativo && edObj.tipo == 1) { edTx.cor = edCor1; edTx.corContorno = edCor2; edTx.corFundo = (edCor2 & 0x00FFFFFF) | 0xB4000000; edTextoGerar(&edTx); }
    if (edFerramenta == ED_T_ADESIVO) {
        if (l->v == &edAdTipo) edAdItem = 0;
        edAdItem = edLim(edAdItem, 0, edTotalItensAdesivo() > 0 ? edTotalItensAdesivo() - 1 : 0);
        if (edObj.ativo && edObj.tipo == 2 && l->v == &edAdTam) edObj.escala = (float)edAdTam / (edObj.w > edObj.h ? edObj.w : edObj.h);
        if (edObj.ativo && edObj.tipo == 2 && (l->v == &edAdItem || l->v == &edAdTipo)) { float cx = edObj.cx, cy = edObj.cy; edExecutarAcao(ED_A_ADESIVO_POR, 0); edObj.cx = cx; edObj.cy = cy; }
    }
    if (edFerramenta == ED_T_CORTAR && l->v == &edCtProp) {
        // encaixa a proporção no corte atual
        float pr[10] = { 0, (float)ed.w / ed.h, 1, 4.0f / 3, 3.0f / 4, 16.0f / 9, 9.0f / 16, 1.5f, 1, 16.0f / 9 };
        float r = pr[edCtProp];
        if (r > 0) {
            float w = edCt[2] - edCt[0], h = edCt[3] - edCt[1], cx = (edCt[0] + edCt[2]) / 2, cy = (edCt[1] + edCt[3]) / 2;
            if (w / h > r) w = h * r; else h = w / r;
            if (w > ed.w) { w = (float)ed.w; h = w / r; } if (h > ed.h) { h = (float)ed.h; w = h * r; }
            edCt[0] = edLimF(cx - w / 2, 0, ed.w - w); edCt[1] = edLimF(cy - h / 2, 0, ed.h - h); edCt[2] = edCt[0] + w; edCt[3] = edCt[1] + h;
        }
    }
    edParametroMudou();
}

// ---------------------------------------------------------------- ferramentas no canvas
static void edPintarSegmento(float ax, float ay, float bx, float by) {
    bool borracha = edFerramenta == ED_T_BORRACHA;
    int tam = borracha ? edBTam : edPTam, dureza = borracha ? edBDureza : (edPTipo == 1 ? 100 : edPDureza);
    if (edPTipo == 1 && !borracha) { if (tam > 6) tam = 6; }
    float r = tam / 2.0f;
    float pts[4][4]; int np = 1;
    pts[0][0] = ax; pts[0][1] = ay; pts[0][2] = bx; pts[0][3] = by;
    if (edSimetria == 1 || edSimetria == 3) { pts[np][0] = ed.w - ax; pts[np][1] = ay; pts[np][2] = ed.w - bx; pts[np][3] = by; np++; }
    if (edSimetria == 2 || edSimetria == 3) { pts[np][0] = ax; pts[np][1] = ed.h - ay; pts[np][2] = bx; pts[np][3] = ed.h - by; np++; }
    if (edSimetria == 3) { pts[np][0] = ed.w - ax; pts[np][1] = ed.h - ay; pts[np][2] = ed.w - bx; pts[np][3] = ed.h - by; np++; }
    int x0 = edTx0, y0 = edTy0, x1 = edTx1, y1 = edTy1;
    for (int k = 0; k < np; k++) {
        if (edPTipo == 3 && !borracha) {
            // spray: pontinhos aleatórios
            float d = sqrtf((pts[k][2] - pts[k][0]) * (pts[k][2] - pts[k][0]) + (pts[k][3] - pts[k][1]) * (pts[k][3] - pts[k][1]));
            int n = (int)(r * 1.2f + d * 0.6f) + 4;
            for (int i = 0; i < n; i++) {
                float t = (edAleatorio() % 1000) / 1000.0f, a = (edAleatorio() % 6283) / 1000.0f, rr = r * sqrtf((edAleatorio() % 1000) / 1000.0f);
                edMascaraDisco(pts[k][0] + (pts[k][2] - pts[k][0]) * t + cosf(a) * rr, pts[k][1] + (pts[k][3] - pts[k][1]) * t + sinf(a) * rr, 1.0f + tam / 60.0f, 80, 255);
            }
        }
        else edMascaraLinha(pts[k][0], pts[k][1], pts[k][2], pts[k][3], r, dureza, 255);
    }
    (void)x0; (void)y0; (void)x1; (void)y1;
    if (edFerramenta == ED_T_CARIMBO) { edCarimboAplicar((int)edCaDx, (int)edCaDy, edCaOpac); }
    else if (edFerramenta == ED_T_RETOQUE) edRetoqueAplicar(edReModo, edReForca);
    else edTracoAplicar(edCor1, borracha ? edBOpac : edPOpac, borracha ? 1 : edPTipo == 2 ? 2 : 0);
    edTracoComporArea();
}
// Forma (ou linha) do arrasto, gravada na camada ativa.
static void edFixarForma(float ax, float ay, float bx, float by) {
    edGuardarPasso(0); edTracoComecar();
    if (edFerramenta == ED_T_LINHA) {
        edMascaraLinha(ax, ay, bx, by, edLEsp / 2.0f, 100, 255);
        // pontas de seta
        for (int k = 0; k < edLSeta; k++) {
            float px = k == 0 ? bx : ax, py = k == 0 ? by : ay, qx = k == 0 ? ax : bx, qy = k == 0 ? ay : by;
            float ang = atan2f(py - qy, px - qx), tam = edLEsp * 3.0f + 12;
            EdPt t[3] = { { px + cosf(ang) * tam * 0.4f, py + sinf(ang) * tam * 0.4f }, { px + cosf(ang + 2.5f) * tam, py + sinf(ang + 2.5f) * tam }, { px + cosf(ang - 2.5f) * tam, py + sinf(ang - 2.5f) * tam } };
            edMascaraPoligono(t, 3, 255);
        }
        edTracoAplicar(edCor1, edPOpac, 0);
    }
    else {
        EdPt p[200]; int n = edFormaPontos(edFTipo, ax, ay, bx, by, p, 200);
        if (edFPreencher == 0) { edMascaraPoligono(p, n, 255); edTracoAplicar(edCor1, edFOpac, 0); }
        if (edFContorno > 0 || edFPreencher == 1) {
            memcpy(ed.base, edAtiva(), (size_t)ed.w * ed.h * 4); memset(ed.traco, 0, (size_t)ed.w * ed.h);
            edMascaraContorno(p, n, true, (float)(edFContorno > 0 ? edFContorno : 6), 255);
            edTracoAplicar(edFPreencher == 0 ? edCor2 : edCor1, edFOpac, 0);
        }
    }
    edSujou();
}
// Ajusta o ponto final do arrasto (proporção e 45 graus).
static void edRestringir(float ax, float ay, float* bx, float* by) {
    float dx = *bx - ax, dy = *by - ay;
    if (edFerramenta == ED_T_LINHA && edLTravar) {
        float ang = atan2f(dy, dx), passo = 3.14159265f / 4, d = sqrtf(dx * dx + dy * dy);
        ang = roundf(ang / passo) * passo; *bx = ax + cosf(ang) * d; *by = ay + sinf(ang) * d;
    }
    if (edFerramenta == ED_T_FORMA && edFQuadrado) { float m = fmaxf(fabsf(dx), fabsf(dy)); *bx = ax + (dx < 0 ? -m : m); *by = ay + (dy < 0 ? -m : m); }
    if (edFerramenta == ED_T_CORTAR && edCtProp > 0) {
        float pr[10] = { 0, (float)ed.w / ed.h, 1, 4.0f / 3, 3.0f / 4, 16.0f / 9, 9.0f / 16, 1.5f, 1, 16.0f / 9 };
        float r = pr[edCtProp], w = fabsf(dx), h = fabsf(dy);
        if (w / fmaxf(h, 1) > r) h = w / r; else w = h * r;
        *bx = ax + (dx < 0 ? -w : w); *by = ay + (dy < 0 ? -h : h);
    }
}
static void edAoApertar(float ix, float iy) {
    edIniX = edUltX = ix; edIniY = edUltY = iy; edArrastando = true;
    switch (edFerramenta) {
    case ED_T_MOVER:
        if (!edObj.ativo) { edGuardarPasso(0); memcpy(ed.base, edAtiva(), (size_t)ed.w * ed.h * 4); }
        break;
    case ED_T_PINCEL: case ED_T_BORRACHA: case ED_T_CARIMBO: case ED_T_RETOQUE:
        if (edFerramenta == ED_T_CARIMBO && !edCaDefinido) { edAvisar("Aperte R3 na origem primeiro"); edArrastando = false; return; }
        edGuardarPasso(0); edTracoComecar();
        if (edFerramenta == ED_T_CARIMBO) { edCaDx = edCaOx - ix; edCaDy = edCaOy - iy; }
        if (edFerramenta == ED_T_CARIMBO || edFerramenta == ED_T_RETOQUE) { int t = edFerramenta == ED_T_CARIMBO ? edCaTam : edReTam, d = edFerramenta == ED_T_CARIMBO ? edCaDureza : 40; edMascaraDisco(ix, iy, t / 2.0f, d, 255); if (edFerramenta == ED_T_CARIMBO) edCarimboAplicar((int)edCaDx, (int)edCaDy, edCaOpac); else edRetoqueAplicar(edReModo, edReForca); edTracoComporArea(); }
        else edPintarSegmento(ix, iy, ix, iy);
        break;
    case ED_T_BALDE: edGuardarPasso(0); edBalde((int)ix, (int)iy, edBalTol, edBalGlobal != 0, edCor1, edBalOpac); edSujou(); edArrastando = false; break;
    case ED_T_CONTAGOTAS: {
        edCompor(); int x = (int)ix, y = (int)iy;
        if (x >= 0 && y >= 0 && x < ed.w && y < ed.h) { uint32_t c = ed.comp[(size_t)y * ed.w + x] | 0xFF000000; if (edCgDestino) edCor2 = c; else edCor1 = c; char m[64]; snprintf(m, sizeof(m), "Cor %d: #%06X", edCgDestino + 1, c & 0xFFFFFF); edAvisar(m); }
        edArrastando = false; break;
    }
    case ED_T_TEXTO:
        if (edObj.ativo && edObj.tipo == 1) { edObj.cx = ix; edObj.cy = iy; edArrastando = false; }
        else { edArrastando = false; edObj.cx = ix; edObj.cy = iy; }
        break;
    case ED_T_ADESIVO:
        if (!edObj.ativo) { edExecutarAcao(ED_A_ADESIVO_POR, 0); edObj.cx = ix; edObj.cy = iy; }
        else { edObj.cx = ix; edObj.cy = iy; }
        break;
    case ED_T_SELECAO:
        if (edSelTipo == 3) { edVarinha((int)ix, (int)iy, edSelTol); edTracoArea(0, 0, ed.w, ed.h); edSelDaMascara(edSelModo, edSelSuave); edArrastando = false; }
        else if (edSelTipo == 2) { if (!edLaco) edLaco = (EdPt*)malloc(sizeof(EdPt) * 4000); edLacoN = 0; if (edLaco) { edLaco[edLacoN].x = ix; edLaco[edLacoN].y = iy; edLacoN++; } }
        break;
    case ED_T_TRANSFORMAR:
        if (edTrCanto > 0) { edTrCantos[edTrCanto - 1].x = ix; edTrCantos[edTrCanto - 1].y = iy; edTrPerspAtiva = true; }
        else edArrastando = false;
        break;
    default: break;
    }
}
static void edAoSegurar(float ix, float iy) {
    if (!edArrastando) return;
    switch (edFerramenta) {
    case ED_T_MOVER:
        if (edObj.ativo) { edObj.cx += ix - edUltX; edObj.cy += iy - edUltY; }
        else {
            int dx = (int)roundf(ix - edIniX), dy = (int)roundf(iy - edIniY);
            uint32_t* L = edAtiva();
            memset(L, 0, (size_t)ed.w * ed.h * 4);
            for (int y = 0; y < ed.h; y++) { int sy = y - dy; if (sy < 0 || sy >= ed.h) continue; int x0 = edLim(dx, 0, ed.w), x1 = edLim(ed.w + dx, 0, ed.w); if (x1 > x0) memcpy(L + (size_t)y * ed.w + x0, ed.base + (size_t)sy * ed.w + x0 - dx, (size_t)(x1 - x0) * 4); }
            ed.compSujo = true;
        }
        break;
    case ED_T_PINCEL: case ED_T_BORRACHA: case ED_T_CARIMBO: case ED_T_RETOQUE:
        if (fabsf(ix - edUltX) + fabsf(iy - edUltY) < 0.4f) return;
        if (edFerramenta == ED_T_CARIMBO || edFerramenta == ED_T_RETOQUE) {
            int t = edFerramenta == ED_T_CARIMBO ? edCaTam : edReTam, d = edFerramenta == ED_T_CARIMBO ? edCaDureza : 40;
            edMascaraLinha(edUltX, edUltY, ix, iy, t / 2.0f, d, 255);
            if (edFerramenta == ED_T_CARIMBO) edCarimboAplicar((int)edCaDx, (int)edCaDy, edCaOpac); else edRetoqueAplicar(edReModo, edReForca);
            edTracoComporArea();
        }
        else edPintarSegmento(edUltX, edUltY, ix, iy);
        break;
    case ED_T_TEXTO: case ED_T_ADESIVO: if (edObj.ativo) { edObj.cx += ix - edUltX; edObj.cy += iy - edUltY; } break;
    case ED_T_SELECAO: if (edSelTipo == 2 && edLaco && edLacoN < 4000 && fabsf(ix - edUltX) + fabsf(iy - edUltY) > 2) { edLaco[edLacoN].x = ix; edLaco[edLacoN].y = iy; edLacoN++; } else if (edSelTipo == 2) return; break;
    case ED_T_TRANSFORMAR: if (edTrCanto > 0) { edTrCantos[edTrCanto - 1].x = ix; edTrCantos[edTrCanto - 1].y = iy; } break;
    default: break;
    }
    edUltX = ix; edUltY = iy;
}
static void edAoSoltar(float ix, float iy) {
    if (!edArrastando) return;
    edArrastando = false;
    float bx = ix, by = iy; edRestringir(edIniX, edIniY, &bx, &by);
    switch (edFerramenta) {
    case ED_T_MOVER: if (!edObj.ativo) edSujou(); break;
    case ED_T_PINCEL: case ED_T_BORRACHA: case ED_T_CARIMBO: case ED_T_RETOQUE: edSujou(); break;
    case ED_T_LINHA: case ED_T_FORMA: if (fabsf(bx - edIniX) + fabsf(by - edIniY) > 2) edFixarForma(edIniX, edIniY, bx, by); break;
    case ED_T_DEGRADE: if (fabsf(bx - edIniX) + fabsf(by - edIniY) > 2) { edGuardarPasso(0); edDegrade(edIniX, edIniY, bx, by, edDgTipo == 1, edCor1, edDgTransp ? (edCor2 & 0x00FFFFFF) : edCor2, edDgOpac); } break;
    case ED_T_CORTAR:
        if (fabsf(bx - edIniX) > 4 && fabsf(by - edIniY) > 4) {
            edCt[0] = edLimF(fminf(edIniX, bx), 0, (float)ed.w); edCt[1] = edLimF(fminf(edIniY, by), 0, (float)ed.h);
            edCt[2] = edLimF(fmaxf(edIniX, bx), 0, (float)ed.w); edCt[3] = edLimF(fmaxf(edIniY, by), 0, (float)ed.h);
        }
        break;
    case ED_T_SELECAO:
        if (edSelTipo == 0 || edSelTipo == 1) {
            if (fabsf(bx - edIniX) < 2 || fabsf(by - edIniY) < 2) { if (edSelModo == ED_SEL_NOVA) edSelNenhuma(); break; }
            memset(ed.traco, 0, (size_t)ed.w * ed.h);
            EdPt p[100]; int n = edFormaPontos(edSelTipo == 0 ? ED_F_RET : ED_F_ELIPSE, edIniX, edIniY, bx, by, p, 100);
            edMascaraPoligono(p, n, 255); edSelDaMascara(edSelModo, edSelSuave);
        }
        else if (edSelTipo == 2 && edLaco && edLacoN > 2) { memset(ed.traco, 0, (size_t)ed.w * ed.h); edMascaraPoligono(edLaco, edLacoN, 255); edSelDaMascara(edSelModo, edSelSuave); }
        edLacoN = 0;
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- operações pesadas
static void edExecutarPendente() {
    int op = edOpPendente; edOpPendente = 0;
    char saida[512] = "";
    switch (op) {
    case ED_OP_AJUSTES: edAplicarAjustes(); break;
    case ED_OP_FILTRO: edAplicarFiltro(); break;
    case ED_OP_FUNDO: {
        edGuardarPasso(0);
        int n = edRemoverFundo(edFuModo, edCor1, edFuTol, edFuSuave);
        char m[96]; snprintf(m, sizeof(m), n ? "Fundo removido (%d pixels)" : "Nao achei fundo: suba a tolerancia", n); edAvisar(m);
        break;
    }
    case ED_OP_GIRAR_LIVRE: edGuardarPasso(1); edGirarLivre((float)edTrAngulo, true); edTrAngulo = 0; break;
    case ED_OP_ESCALA: edGuardarPasso(1); if (!edRedimensionar(ed.w * edTrEscala / 100, ed.h * edTrEscala / 100)) edAvisar("Sem memoria"); edTrEscala = 100; edAjustarVista(); break;
    case ED_OP_PERSPECTIVA: edGuardarPasso(1); edPerspectiva(edTrCantos); edExecutarAcao(ED_A_PERSP_ZERAR, 0); edTrCanto = 0; break;
    case ED_OP_CORTAR: {
        int x0 = (int)edCt[0], y0 = (int)edCt[1], x1 = (int)edCt[2], y1 = (int)edCt[3];
        if (x1 - x0 < 2 || y1 - y0 < 2 || (x0 == 0 && y0 == 0 && x1 == ed.w && y1 == ed.h && edCtProp < 8)) { edAvisar("Marque a area com X antes"); break; }
        edGuardarPasso(1); edCortar(x0, y0, x1, y1);
        if (edCtProp == 8) edRedimensionar(440, 440); else if (edCtProp == 9) edRedimensionar(1920, 1080);
        edCt[0] = 0; edCt[1] = 0; edCt[2] = (float)ed.w; edCt[3] = (float)ed.h; edAjustarVista();
        break;
    }
    case ED_OP_SALVAR_PNG: edSalvar(0, NULL, 0); break;
    case ED_OP_SALVAR_JPG: edSalvar(1, NULL, 0); break;
    case ED_OP_PROJETO: {
        char c[512], nome[200] = "projeto";
        if (ed.caminho[0]) { const char* b = strrchr(ed.caminho, '/'); snprintf(nome, sizeof(nome), "%s", b ? b + 1 : ed.caminho); char* x = strrchr(nome, '.'); if (x) *x = 0; }
        snprintf(c, sizeof(c), "/data/HyperNeiva/editor/projetos/%s.hnimg", nome);
        bool ok = edSalvarProjeto(c);
        char m[300]; snprintf(m, sizeof(m), ok ? "Projeto salvo: %.240s" : "Nao consegui salvar o projeto %.200s", c); edAvisar(m);
        if (ok) { ed.mudou = false; snprintf(ed.caminho, sizeof(ed.caminho), "%s", c); }
        break;
    }
    case ED_OP_USAR: if (edSalvar(0, saida, sizeof(saida))) { edFechar(); edPlatUsarImagem(saida); } break;
    case ED_OP_SAIR_SALVANDO: if (edSalvar(0, NULL, 0)) edFechar(); break;
    }
    edLiberarTrabalho();   // os buffers de resolução cheia não ficam presos
    edParametroMudou();
}

// ---------------------------------------------------------------- menus sobrepostos
static const char* const edItensMenu[] = { "Salvar (PNG)", "Salvar como JPG", "Salvar e usar como avatar / fundo / capa", "Salvar projeto (.hnimg, com camadas)",
    "Abrir imagem ou projeto", "Inserir imagem como camada", "Novo / modelos", "Desfazer", "Refazer", "Grade: ligar/desligar", "Ajustar a tela", "Sair do editor" };
#define ED_TOTAL_MENU 12
static const char* const edItensNovo[] = { "Tela branca 1920x1080 (fundo PS4)", "Avatar PS4 440x440", "Quadrado 1080x1080", "Thumbnail YouTube 1280x720 (degrade)",
    "Story 1080x1920", "Transparente 1920x1080", "Colagem de 2 fotos", "Colagem de 4 fotos", "Meme (texto na imagem atual)", "Moldura na imagem atual", "Continuar na imagem atual" };
#define ED_TOTAL_NOVO 11
static void edColagemRet(int i, int* x, int* y, int* w, int* h) {
    int m = 12;
    if (edColagemN == 2) { *w = (ed.w - 3 * m) / 2; *h = ed.h - 2 * m; *x = m + i * (*w + m); *y = m; }
    else { *w = (ed.w - 3 * m) / 2; *h = (ed.h - 3 * m) / 2; *x = m + (i % 2) * (*w + m); *y = m + (i / 2) * (*h + m); }
}
static void edColagemColocar(const uint32_t* px, int w, int h) {
    int cx, cy, cw, ch; edColagemRet(edColagemCelula, &cx, &cy, &cw, &ch);
    edGuardarPasso(0);
    float esc = fmaxf((float)cw / w, (float)ch / h);    // cobre a célula
    uint32_t* L = edAtiva();
    for (int y = 0; y < ch; y++) for (int x = 0; x < cw; x++) {
        float sx = (x + .5f - cw / 2.0f) / esc + w / 2.0f, sy = (y + .5f - ch / 2.0f) / esc + h / 2.0f;
        L[(size_t)(cy + y) * ed.w + cx + x] = edAmostrar(px, w, h, sx, sy) | 0xFF000000;
    }
    edSujou();
    edColagemCelula++;
}
static void edNovoModelo(int i) {
    switch (i) {
    case 0: edNovoCanvas(1920, 1080, 0xFFFFFFFF); break;
    case 1: edNovoCanvas(440, 440, 0xFFFFFFFF); break;
    case 2: edNovoCanvas(1080, 1080, 0xFFFFFFFF); break;
    case 3: if (edNovoCanvas(1280, 720, 0xFFFFFFFF)) { edDegrade(0, 0, 1280, 720, false, edCor1, edCor2, 100); ed.mudou = false; edGuardarOriginal(); edTx.tamanho = 130; edTx.contorno = 10; edFerramenta = ED_T_TEXTO; edAvisar("Escreva o titulo: Texto > X"); } break;
    case 4: edNovoCanvas(1080, 1920, 0xFFFFFFFF); break;
    case 5: edNovoCanvas(1920, 1080, 0); break;
    case 6: case 7:
        if (edNovoCanvas(1920, 1080, 0xFFFFFFFF)) { edColagemN = i == 6 ? 2 : 4; edColagemCelula = 0; edAbrirSeletor(2); edAvisar("Escolha a foto 1"); return; }
        break;
    case 8: edTx.cor = 0xFFFFFFFF; edCor1 = 0xFFFFFFFF; edCor2 = 0xFF000000; edTx.contorno = 8; edTx.tamanho = ed.h / 8 > 24 ? ed.h / 8 : 24; edTx.alinhamento = 1; edTx.sombra = 0; edTx.fundo = 0;
        edFerramenta = ED_T_TEXTO; edCurX = edTelaX(ed.w / 2.0f); edCurY = edTelaY(ed.h * 0.1f); edAvisar("Meme: X escreve o texto de cima; depois mova e repita embaixo"); break;
    case 9: {
        edGuardarPasso(1);
        if (edNovaCamada("Moldura", 0)) {
            int b = (ed.w < ed.h ? ed.w : ed.h) / 25 + 4;
            uint32_t* L = edAtiva();
            for (int y = 0; y < ed.h; y++) for (int x = 0; x < ed.w; x++) if (x < b || y < b || x >= ed.w - b || y >= ed.h - b) L[(size_t)y * ed.w + x] = edCor1;
            edSujou(); edAvisar("Moldura com a cor 1 (camada Moldura)");
        }
        break;
    }
    default: break;
    }
    edMenu = 0;
}
static void edSeletorEscolheu(const char* caminho) {
    if (edArqFinalidade == 0) {
        const char* x = strrchr(caminho, '.');
        bool ok = (x && !strcasecmp(x, ".hnimg")) ? edAbrirProjeto(caminho) : edAbrirImagem(caminho);
        if (ok) { edDocPronto(); edMenu = 0; } else edAvisar("Nao consegui abrir");
        return;
    }
    int w, h, c; unsigned char* d = stbi_load(caminho, &w, &h, &c, 4);
    if (!d) { edAvisar("Nao consegui abrir essa imagem"); return; }
    uint32_t* px = edRgbaParaInterno(d, w, h); stbi_image_free(d);
    if (!px) return;
    if (edArqFinalidade == 2) {
        edColagemColocar(px, w, h); free(px);
        if (edColagemCelula >= edColagemN) { edMenu = 0; edAvisar("Colagem pronta"); }
        else { char m[48]; snprintf(m, sizeof(m), "Escolha a foto %d", edColagemCelula + 1); edAvisar(m); }
        return;
    }
    edObjDeImagem(px, w, h, "Imagem", 3); free(px);
    float esc = fminf(ed.w * 0.6f / w, ed.h * 0.6f / h); edObj.escala = esc > 1 ? 1 : esc;
    edMenu = 0; edFerramenta = ED_T_MOVER;
    edAvisar("Mova com o analogico, L2/R2 tamanho, X fixa");
}

// ---------------------------------------------------------------- controles
static bool edApertou(uint32_t n, uint32_t b) { return (n & b) != 0; }
// Devolve true quando o editor fecha.
bool editorImagemControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    (void)antigos;
    uint64_t agora = edPlatAgoraMs();
    float dt = (agora - edUltimoMs) / 1000.0f; if (dt > 0.1f) dt = 0.1f; if (dt < 0) dt = 0;
    edUltimoMs = agora;
    uint32_t n = botoes & ~edBotoesAntes;
    edBotoesAntes = botoes;
    if (edOpPendente) { if (edOpMostrada) edExecutarPendente(); return !editorImagemAberto; }
    if (edImeModo) {
        char t[600]; int r = edPlatTecladoResultado(t, sizeof(t));
        if (r < 0) return false;
        int modo = edImeModo; edImeModo = 0;
        if (r == 1) {
            if (modo == 1) {
                snprintf(edTx.texto, sizeof(edTx.texto), "%s", t);
                edTx.cor = edCor1; edTx.corContorno = edCor2; edTx.corFundo = (edCor2 & 0x00FFFFFF) | 0xB4000000;
                bool novo = !(edObj.ativo && edObj.tipo == 1);
                if (novo) { edObj.ativo = false; }
                float cx = edObj.cx, cy = edObj.cy;
                if (edTextoGerar(&edTx)) { if (novo) { edObj.cx = cx ? cx : ed.w / 2.0f; edObj.cy = cy ? cy : ed.h / 2.0f; } edAvisar("Analogico move, L2/R2 tamanho, X fixa"); }
                else edAvisar("Nao consegui usar essa fonte");
            }
            else if (modo == 2) { unsigned v = 0; const char* s = t[0] == '#' ? t + 1 : t; if (sscanf(s, "%x", &v) == 1) { uint32_t c = 0xFF000000u | (v & 0xFFFFFF); if (edCorEditando) edCor2 = c; else edCor1 = c; edCorParaSliders(); } }
            else if (modo == 3) { snprintf(ed.cam[ed.ativa].nome, sizeof(ed.cam[0].nome), "%.38s", t); }
        }
        return false;
    }
    // ---- menus sobrepostos
    if (edMenu) {
        int total = edMenu == 1 ? ED_TOTAL_MENU : edMenu == 2 ? ED_TOTAL_NOVO : edMenu == 3 ? edTotalArqs : edMenu == 4 ? 3 : 0;
        if (edApertou(n, ORBIS_PAD_BUTTON_UP) && total) edMenuSel = (edMenuSel + total - 1) % total;
        if (edApertou(n, ORBIS_PAD_BUTTON_DOWN) && total) edMenuSel = (edMenuSel + 1) % total;
        if (edMenu == 3) {
            if (edApertou(n, ORBIS_PAD_BUTTON_L1)) edMenuSel = edLim(edMenuSel - 15, 0, total ? total - 1 : 0);
            if (edApertou(n, ORBIS_PAD_BUTTON_R1)) edMenuSel = edLim(edMenuSel + 15, 0, total ? total - 1 : 0);
            if (edApertou(n, ORBIS_PAD_BUTTON_CIRCLE)) {
                if (!edArqPasta[0]) { edMenu = edArqFinalidade == 2 ? 0 : 1; edMenuSel = 0; return false; }
                char p[512]; snprintf(p, sizeof(p), "%s", edArqPasta); char* b = strrchr(p, '/');
                if (b && b != p) { *b = 0; edListarPasta(p); } else edListarPasta("");
                edMenuSel = 0; return false;
            }
            if (edApertou(n, ORBIS_PAD_BUTTON_CROSS) && total) {
                EdItemArq* it = &edArqs[edMenuSel];
                char c[800];
                if (!edArqPasta[0]) snprintf(c, sizeof(c), "%s", it->nome); else snprintf(c, sizeof(c), "%s/%s", edArqPasta, it->nome);
                if (it->pasta) { edListarPasta(c); edMenuSel = 0; }
                else edSeletorEscolheu(c);
            }
            return false;
        }
        if (edApertou(n, ORBIS_PAD_BUTTON_CIRCLE)) { edMenu = 0; return false; }
        if (edApertou(n, ORBIS_PAD_BUTTON_CROSS)) {
            if (edMenu == 1) {
                int i = edMenuSel; edMenu = 0;
                switch (i) {
                case 0: edAgendar(ED_OP_SALVAR_PNG, "Salvando..."); break;
                case 1: edAgendar(ED_OP_SALVAR_JPG, "Salvando..."); break;
                case 2: edAgendar(ED_OP_USAR, "Salvando..."); break;
                case 3: edAgendar(ED_OP_PROJETO, "Salvando o projeto..."); break;
                case 4: edAbrirSeletor(0); edMenuSel = 0; break;
                case 5: edAbrirSeletor(1); edMenuSel = 0; break;
                case 6: edMenu = 2; edMenuSel = 0; break;
                case 7: if (!edDesfazerUm()) edAvisar("Nada para desfazer"); break;
                case 8: if (!edRefazerUm()) edAvisar("Nada para refazer"); break;
                case 9: edGrade = !edGrade; break;
                case 10: edAjustarVista(); break;
                case 11: if (ed.mudou) { edMenu = 4; edMenuSel = 0; } else { edFechar(); return true; } break;
                }
            }
            else if (edMenu == 2) edNovoModelo(edMenuSel);
            else if (edMenu == 4) {
                if (edMenuSel == 0) { edAgendar(ED_OP_SAIR_SALVANDO, "Salvando..."); edMenu = 0; }
                else if (edMenuSel == 1) { edFechar(); return true; }
                else edMenu = 0;
            }
        }
        return false;
    }
    // ---- objeto solto (texto, adesivo, imagem)
    int lx, ly, rx, ry; edPlatAnalogico(&lx, &ly, &rx, &ry);
    lx -= 128; ly -= 128; rx -= 128; ry -= 128;
    bool objetoSolto = edObj.ativo && (edFerramenta == ED_T_TEXTO || edFerramenta == ED_T_ADESIVO || edFerramenta == ED_T_MOVER);
    // ---- trocar ferramenta e mexer no painel
    if (edApertou(n, ORBIS_PAD_BUTTON_L1)) edTrocarFerramenta((edFerramenta + ED_T_TOTAL - 1) % ED_T_TOTAL);
    if (edApertou(n, ORBIS_PAD_BUTTON_R1)) edTrocarFerramenta((edFerramenta + 1) % ED_T_TOTAL);
    edMontarPainel();
    int& ls = edLinhaSel[edFerramenta];
    if (ls >= edNL) ls = edNL ? edNL - 1 : 0;
    if (edApertou(n, ORBIS_PAD_BUTTON_UP) && edNL) { int g = 0; do { ls = (ls + edNL - 1) % edNL; } while (edLinhas[ls].tipo == ED_L_INFO && ++g < edNL); }
    if (edApertou(n, ORBIS_PAD_BUTTON_DOWN) && edNL) { int g = 0; do { ls = (ls + 1) % edNL; } while (edLinhas[ls].tipo == ED_L_INFO && ++g < edNL); }
    // esquerda/direita com repetição ao segurar
    int dir = 0;
    uint32_t lr = botoes & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT);
    if (n & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) { dir = (n & ORBIS_PAD_BUTTON_RIGHT) ? 1 : -1; edRepeteEm = agora + 320; edRepeteN = 0; }
    else if (lr && agora >= edRepeteEm) { dir = (lr & ORBIS_PAD_BUTTON_RIGHT) ? 1 : -1; edRepeteEm = agora + 45; edRepeteN++; }
    if (dir && edNL && ls < edNL) {
        EdLinha* l = &edLinhas[ls];
        if (l->tipo == ED_L_SLIDER && l->v) {
            int passo = l->passo * (edRepeteN > 20 ? 5 : 1);
            int novo = edLim(*l->v + dir * passo, l->mn, l->mx);
            if (novo != *l->v) { *l->v = novo; edValorMudou(l); }
        }
        else if (l->tipo == ED_L_OPCAO && l->v && l->nops > 0 && (n & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT))) { *l->v = (*l->v + dir + l->nops) % l->nops; edValorMudou(l); }
        else if (l->tipo == ED_L_ACAO && dir > 0 && (n & ORBIS_PAD_BUTTON_RIGHT)) edExecutarAcao(l->acao, usuario);
    }
    if (edApertou(n, ORBIS_PAD_BUTTON_TRIANGLE)) {
        if (edFerramenta == ED_T_TEXTO && edObj.ativo && edObj.tipo == 1) edExecutarAcao(ED_A_TEXTO_ESCREVER, usuario);
        else if (edNL && ls < edNL && edLinhas[ls].tipo == ED_L_ACAO) edExecutarAcao(edLinhas[ls].acao, usuario);
        else if (edFerramentaAjuste(edFerramenta)) edExecutarAcao(ED_A_AJ_APLICAR, usuario);
        else if (edFerramenta == ED_T_FILTROS) edExecutarAcao(ED_A_FILTRO_APLICAR, usuario);
        else if (edFerramenta == ED_T_CORTAR) edExecutarAcao(ED_A_CORTAR, usuario);
        else if (edFerramenta == ED_T_TEXTO) edExecutarAcao(ED_A_TEXTO_ESCREVER, usuario);
    }
    if (edApertou(n, ORBIS_PAD_BUTTON_SQUARE)) { if (edArrastando) {} else if (edDesfazerUm()) edAvisar("Desfeito"); else edAvisar("Nada para desfazer"); }
    if (edApertou(n, ORBIS_PAD_BUTTON_TOUCH_PAD)) { if (edRefazerUm()) edAvisar("Refeito"); }
    if (edApertou(n, ORBIS_PAD_BUTTON_OPTIONS)) { edMenu = 1; edMenuSel = 0; return false; }
    if (edApertou(n, ORBIS_PAD_BUTTON_R3) && edFerramenta == ED_T_CARIMBO) { edCaOx = edImgX(edCurX); edCaOy = edImgY(edCurY); edCaDefinido = true; edAvisar("Origem do carimbo definida"); }
    edMostrarOriginal = (botoes & ORBIS_PAD_BUTTON_L3) != 0;
    if (edApertou(n, ORBIS_PAD_BUTTON_CIRCLE)) {
        if (edArrastando) { edArrastando = false; if (edFerramenta == ED_T_PINCEL || edFerramenta == ED_T_BORRACHA || edFerramenta == ED_T_MOVER) edDesfazerUm(); }
        else if (edObj.ativo) { edObjLimpar(); edAvisar("Objeto descartado"); }
        else if (ed.mudou) { edMenu = 4; edMenuSel = 1; }
        else { edFechar(); return true; }
        return false;
    }
    // ---- zoom e movimento da vista
    bool l2 = (botoes & ORBIS_PAD_BUTTON_L2) != 0, r2 = (botoes & ORBIS_PAD_BUTTON_R2) != 0;
    if (objetoSolto) {
        if (r2) edObj.escala *= 1 + 1.2f * dt;
        if (l2) edObj.escala /= 1 + 1.2f * dt;
        edObj.escala = edLimF(edObj.escala, 0.02f, 20);
        if (abs(rx) > 24) edObj.angulo += rx / 128.0f * 120 * dt;
        if (abs(ry) > 24) { edCx += 0; }
    }
    else {
        if (r2 || l2) {
            float ix = edImgX(edCurX), iy = edImgY(edCurY);
            edZoom *= r2 ? 1 + 1.6f * dt : 1 / (1 + 1.6f * dt);
            edZoom = edLimF(edZoom, 0.05f, 32);
            edCx = ix - (edCurX - (ED_VX + ED_VW / 2.0f)) / edZoom; edCy = iy - (edCurY - (ED_VY + ED_VH / 2.0f)) / edZoom;
        }
        if (abs(rx) > 24) edCx += rx / 128.0f * 900 * dt / edZoom;
        if (abs(ry) > 24) edCy += ry / 128.0f * 900 * dt / edZoom;
    }
    // ---- cursor: analógico esquerdo (fino perto do centro) ou touchpad (posição direta)
    float ixAntes = edImgX(edCurX), iyAntes = edImgY(edCurY);
    float vel = 1100;
    if (abs(lx) > 14) edCurX += (lx > 0 ? 1 : -1) * (lx * lx) / 16129.0f * vel * dt;
    if (abs(ly) > 14) edCurY += (ly > 0 ? 1 : -1) * (ly * ly) / 16129.0f * vel * dt;
    int tx, ty; bool toque = edPlatToque(&tx, &ty) > 0;
    if (toque) { edCurX = ED_VX + tx * (float)ED_VW / 1920; edCurY = ED_VY + ty * (float)ED_VH / 942; }
    edCurX = edLimF(edCurX, ED_VX, ED_VX + ED_VW - 1); edCurY = edLimF(edCurY, ED_VY, ED_VY + ED_VH - 1);
    float ix = edImgX(edCurX), iy = edImgY(edCurY);
    // ---- X (ou dedo no touchpad) usa a ferramenta
    bool aperta = (botoes & ORBIS_PAD_BUTTON_CROSS) || toque;
    if (objetoSolto && (n & ORBIS_PAD_BUTTON_CROSS) && edFerramenta != ED_T_ADESIVO) { if (edObjFixar()) edAvisar("Fixado numa camada nova"); edApertado = true; return false; }
    if (objetoSolto && (n & ORBIS_PAD_BUTTON_CROSS) && edFerramenta == ED_T_ADESIVO) { if (edObjFixar()) edAvisar("Adesivo fixado"); edApertado = true; return false; }
    if (objetoSolto && !aperta && (abs(lx) > 14 || abs(ly) > 14)) {
        // o analógico arrasta o objeto junto com o cursor
        edObj.cx += ix - ixAntes; edObj.cy += iy - iyAntes;
    }
    if (edFerramenta == ED_T_TEXTO && !edObj.ativo && (n & ORBIS_PAD_BUTTON_CROSS)) {
        edObj.cx = ix; edObj.cy = iy;
        if (edPlatTecladoAbrir(usuario, "Texto (| quebra a linha)", 200, "")) edImeModo = 1;
        edTx.texto[0] = 0; edApertado = true;
        return false;
    }
    if (aperta && !edApertado) edAoApertar(ix, iy);
    else if (aperta && edApertado) edAoSegurar(ix, iy);
    else if (!aperta && edApertado) edAoSoltar(ix, iy);
    edApertado = aperta;
    return false;
}

// ---------------------------------------------------------------- prévia
static bool edPreviaAtiva() { return (edFerramentaAjuste(edFerramenta) && edAjustePendente()) || (edFerramenta == ED_T_FILTROS && edFiltro); }
static void edAtualizarPrevia() {
    edCompor();
    if (!edPrevBase || edPrevBaseVer != ed.versao) {
        float f = fminf(1.0f, fminf(900.0f / ed.w, 900.0f / ed.h));
        int pw = (int)(ed.w * f), ph = (int)(ed.h * f); if (pw < 1) pw = 1; if (ph < 1) ph = 1;
        free(edPrevBase); free(edPrev);
        edPrevBase = f < 1 ? edReduzir(ed.comp, ed.w, ed.h, pw, ph) : (uint32_t*)malloc((size_t)pw * ph * 4);
        if (edPrevBase && f >= 1) memcpy(edPrevBase, ed.comp, (size_t)pw * ph * 4);
        edPrev = (uint32_t*)malloc((size_t)pw * ph * 4);
        edPrevW = pw; edPrevH = ph; edPrevBaseVer = ed.versao; edPrevVer = 0;
    }
    if (!edPrevBase || !edPrev) return;
    if (edPrevVer == edParamVer) return;
    if (edPlatAgoraMs() - edParamMs < 60 && edPrevVer) return;   // espera parar de mexer
    EdAjustes a = edFerramenta == ED_T_FILTROS ? edFiltroEm(edFiltro, edFiltroForca) : edAj;
    edProcessar(edPrevBase, edPrev, edPrevW, edPrevH, &a, (float)edPrevW / ed.w);
    edPrevVer = edParamVer;
}

// ---------------------------------------------------------------- desenho da tela
static void edDesenharCanvas(uint32_t* p) {
    const uint32_t* src = ed.comp; int sw = ed.w, sh = ed.h;
    if (edMostrarOriginal && edOriginal) { src = edOriginal; sw = edOrigW; sh = edOrigH; }
    else if (edPreviaAtiva()) { edAtualizarPrevia(); if (edPrev && edPrevVer) { src = edPrev; sw = edPrevW; sh = edPrevH; } }
    float sx2 = (float)sw / ed.w, sy2 = (float)sh / ed.h;
    if (edMostrarOriginal && edOriginal) { sx2 = (float)sw / edOrigW; sy2 = (float)sh / edOrigH; }
    static int mapaX[ED_VW], mapaImgX[ED_VW];
    for (int x = 0; x < ED_VW; x++) {
        float ixf = edImgX(ED_VX + x + 0.5f);
        int ixi = (int)floorf(ixf);
        mapaImgX[x] = (ixi >= 0 && ixi < ed.w) ? ixi : -1;
        mapaX[x] = mapaImgX[x] >= 0 ? edLim((int)(ixf * sx2), 0, sw - 1) : -1;
    }
    // objeto solto: caixa na tela
    float ox0 = 0, oy0 = 0, ox1 = 0, oy1 = 0; float ocs = 1, osn = 0;
    if (edObj.ativo) { edObjCaixa(&ox0, &oy0, &ox1, &oy1); float a = -edObj.angulo * 3.14159265f / 180; ocs = cosf(a); osn = sinf(a); }
    for (int y = 0; y < ED_VH; y++) {
        uint32_t* d = p + (size_t)(ED_VY + y) * 1920 + ED_VX;
        float iyf = edImgY(ED_VY + y + 0.5f);
        int iyi = (int)floorf(iyf);
        if (iyi < 0 || iyi >= ed.h) { for (int x = 0; x < ED_VW; x++) d[x] = 0xFF25272D; continue; }
        const uint32_t* linha = src + (size_t)edLim((int)(iyf * sy2), 0, sh - 1) * sw;
        const uint8_t* selLinha = ed.sel ? ed.sel + (size_t)iyi * ed.w : NULL;
        bool linhaObj = edObj.ativo && iyf >= oy0 && iyf <= oy1;
        for (int x = 0; x < ED_VW; x++) {
            if (mapaX[x] < 0) { d[x] = 0xFF25272D; continue; }
            uint32_t c = linha[mapaX[x]];
            if (linhaObj) {
                float ixf = edImgX(ED_VX + x + 0.5f);
                if (ixf >= ox0 && ixf <= ox1) {
                    float dx = (ixf - edObj.cx) / edObj.escala, dy = (iyf - edObj.cy) / edObj.escala;
                    float sxo = dx * ocs - dy * osn + edObj.w / 2.0f, syo = dx * osn + dy * ocs + edObj.h / 2.0f;
                    int xi = (int)sxo, yi = (int)syo;
                    if (sxo >= 0 && syo >= 0 && xi < edObj.w && yi < edObj.h) { uint32_t o = edObj.px[(size_t)yi * edObj.w + xi]; if (ED_A(o)) c = edSobre(c, o, edObj.opacidade, ED_MIS_NORMAL); }
                }
            }
            uint32_t a = ED_A(c);
            if (a < 255) {
                uint32_t xadrez = (((ED_VX + x) >> 4) + ((ED_VY + y) >> 4)) & 1 ? 0xFFC8C8C8 : 0xFFF2F2F2;
                c = a == 0 ? xadrez : edCor((ED_R(c) * a + ED_R(xadrez) * (255 - a)) / 255, (ED_G(c) * a + ED_G(xadrez) * (255 - a)) / 255, (ED_BL(c) * a + ED_BL(xadrez) * (255 - a)) / 255, 255);
            }
            if (selLinha && !selLinha[mapaImgX[x]]) c = ((c >> 1) & 0x7F7F7F) | 0xFF000000;
            d[x] = c;
        }
    }
    // grade (terços e linhas a cada 10%)
    if (edGrade) for (int i = 1; i < 10; i++) {
        uint32_t c = i % 3 == 0 ? 0xFFFFFFFF : 0xFF7A7A7A;
        float gx = edTelaX(ed.w * i / 10.0f), gy = edTelaY(ed.h * i / 10.0f);
        edTelaLinha(p, gx, edTelaY(0), gx, edTelaY((float)ed.h), c, true); edTelaLinha(p, edTelaX(0), gy, edTelaX((float)ed.w), gy, c, true);
    }
    // contorno da imagem
    edTelaLinha(p, edTelaX(0), edTelaY(0), edTelaX((float)ed.w), edTelaY(0), 0xFF000000, false);
    edTelaLinha(p, edTelaX(0), edTelaY((float)ed.h), edTelaX((float)ed.w), edTelaY((float)ed.h), 0xFF000000, false);
    // caixa do objeto
    if (edObj.ativo) {
        float c = cosf(edObj.angulo * 3.14159265f / 180), s = sinf(edObj.angulo * 3.14159265f / 180), hw = edObj.w * edObj.escala / 2, hh = edObj.h * edObj.escala / 2;
        float px[4], py[4]; float cxs[4] = { -hw, hw, hw, -hw }, cys[4] = { -hh, -hh, hh, hh };
        for (int i = 0; i < 4; i++) { px[i] = edTelaX(edObj.cx + cxs[i] * c - cys[i] * s); py[i] = edTelaY(edObj.cy + cxs[i] * s + cys[i] * c); }
        for (int i = 0; i < 4; i++) edTelaLinha(p, px[i], py[i], px[(i + 1) % 4], py[(i + 1) % 4], 0xFF29B6F6, true);
        // guia de centro
        if (fabsf(edObj.cx - ed.w / 2.0f) < ed.w * 0.01f) edTelaLinha(p, edTelaX(ed.w / 2.0f), edTelaY(0), edTelaX(ed.w / 2.0f), edTelaY((float)ed.h), 0xFFFF4081, false);
        if (fabsf(edObj.cy - ed.h / 2.0f) < ed.h * 0.01f) edTelaLinha(p, edTelaX(0), edTelaY(ed.h / 2.0f), edTelaX((float)ed.w), edTelaY(ed.h / 2.0f), 0xFFFF4081, false);
    }
    float cx = edImgX(edCurX), cy = edImgY(edCurY);
    // prévia do arrasto
    if (edArrastando && (edFerramenta == ED_T_LINHA || edFerramenta == ED_T_FORMA || edFerramenta == ED_T_DEGRADE || edFerramenta == ED_T_CORTAR || (edFerramenta == ED_T_SELECAO && edSelTipo < 2))) {
        float bx = cx, by = cy; edRestringir(edIniX, edIniY, &bx, &by);
        if (edFerramenta == ED_T_LINHA || edFerramenta == ED_T_DEGRADE) edTelaLinha(p, edTelaX(edIniX), edTelaY(edIniY), edTelaX(bx), edTelaY(by), 0xFFFFFFFF, edFerramenta == ED_T_DEGRADE);
        else {
            int forma = edFerramenta == ED_T_FORMA ? edFTipo : (edFerramenta == ED_T_SELECAO && edSelTipo == 1) ? ED_F_ELIPSE : ED_F_RET;
            EdPt q[200]; int nq = edFormaPontos(forma, edIniX, edIniY, bx, by, q, 200);
            for (int i = 0; i < nq; i++) edTelaLinha(p, edTelaX(q[i].x), edTelaY(q[i].y), edTelaX(q[(i + 1) % nq].x), edTelaY(q[(i + 1) % nq].y), 0xFFFFFFFF, true);
        }
    }
    if (edArrastando && edFerramenta == ED_T_SELECAO && edSelTipo == 2 && edLaco) for (int i = 0; i + 1 < edLacoN; i++) edTelaLinha(p, edTelaX(edLaco[i].x), edTelaY(edLaco[i].y), edTelaX(edLaco[i + 1].x), edTelaY(edLaco[i + 1].y), 0xFFFFFFFF, true);
    // retângulo do corte
    if (edFerramenta == ED_T_CORTAR && edCtAtivo && !edArrastando) {
        float x0 = edTelaX(edCt[0]), y0 = edTelaY(edCt[1]), x1 = edTelaX(edCt[2]), y1 = edTelaY(edCt[3]);
        for (int y = ED_VY; y < ED_VY + ED_VH; y++) for (int x = ED_VX; x < ED_VX + ED_VW; x++) if (x < x0 || x > x1 || y < y0 || y > y1) { uint32_t c = p[y * 1920 + x]; p[y * 1920 + x] = ((c >> 1) & 0x7F7F7F) | 0xFF000000; }
        edTelaLinha(p, x0, y0, x1, y0, 0xFFFFFFFF, false); edTelaLinha(p, x1, y0, x1, y1, 0xFFFFFFFF, false); edTelaLinha(p, x1, y1, x0, y1, 0xFFFFFFFF, false); edTelaLinha(p, x0, y1, x0, y0, 0xFFFFFFFF, false);
        for (int i = 1; i < 3; i++) { edTelaLinha(p, x0 + (x1 - x0) * i / 3, y0, x0 + (x1 - x0) * i / 3, y1, 0xFF9E9E9E, true); edTelaLinha(p, x0, y0 + (y1 - y0) * i / 3, x1, y0 + (y1 - y0) * i / 3, 0xFF9E9E9E, true); }
    }
    // perspectiva
    if (edFerramenta == ED_T_TRANSFORMAR && edTrPerspAtiva) for (int i = 0; i < 4; i++) {
        edTelaLinha(p, edTelaX(edTrCantos[i].x), edTelaY(edTrCantos[i].y), edTelaX(edTrCantos[(i + 1) % 4].x), edTelaY(edTrCantos[(i + 1) % 4].y), 0xFF29B6F6, false);
        edRet(p, (int)edTelaX(edTrCantos[i].x) - 6, (int)edTelaY(edTrCantos[i].y) - 6, 12, 12, i + 1 == edTrCanto ? 0xFFFF4081 : 0xFFFFFFFF);
    }
    // origem do carimbo
    if (edFerramenta == ED_T_CARIMBO && edCaDefinido) {
        float ox = edApertado ? cx + edCaDx : edCaOx, oy = edApertado ? cy + edCaDy : edCaOy;
        edTelaCirculo(p, edTelaX(ox), edTelaY(oy), edCaTam / 2.0f * edZoom, 0xFF4CAF50);
    }
    // cursor
    int tam = edFerramenta == ED_T_PINCEL ? edPTam : edFerramenta == ED_T_BORRACHA ? edBTam : edFerramenta == ED_T_CARIMBO ? edCaTam : edFerramenta == ED_T_RETOQUE ? edReTam : 0;
    if (tam) edTelaCirculo(p, edCurX, edCurY, tam / 2.0f * edZoom, 0xFFFFFFFF);
    for (int k = -14; k <= 14; k++) { if (abs(k) < 4) continue; edPonto(p, (int)edCurX + k, (int)edCurY, 0xFFFFFFFF); edPonto(p, (int)edCurX, (int)edCurY + k, 0xFFFFFFFF); edPonto(p, (int)edCurX + k, (int)edCurY + 1, 0xFF000000); edPonto(p, (int)edCurX + 1, (int)edCurY + k, 0xFF000000); }
}
static void edDesenharPainel(uint32_t* p) {
    edRet(p, ED_PX, ED_VY, ED_PW, ED_VH, 0xFF1F2228);
    edTexto(p, edNomesFerramenta[edFerramenta], 30, ED_PX + 18, ED_VY + 12, 0xFFFFFFFF);
    edMontarPainel();
    int& ls = edLinhaSel[edFerramenta];
    if (ls >= edNL) ls = edNL ? edNL - 1 : 0;
    if (edNL && edLinhas[ls].tipo == ED_L_INFO) { for (int k = 0; k < edNL; k++) if (edLinhas[k].tipo != ED_L_INFO) { ls = k; break; } }
    const int y0 = ED_VY + 60, passo = 42;
    int extra = (edFerramenta == ED_T_COR || edFerramenta == ED_T_CURVAS || edFerramenta == ED_T_ADESIVO || edFerramenta == ED_T_CAMADAS) ? 260 : 0;
    int vis = (ED_VH - 70 - extra) / passo;
    int topo = ls >= vis ? ls - vis + 1 : 0;
    for (int k = 0; k < vis && topo + k < edNL; k++) {
        EdLinha* l = &edLinhas[topo + k];
        int y = y0 + k * passo;
        bool sel = topo + k == ls;
        if (sel) edRet(p, ED_PX + 8, y - 4, ED_PW - 16, passo - 4, 0xFF2E3A4B);
        uint32_t cr = l->tipo == ED_L_INFO ? 0xFF8A93A0 : l->tipo == ED_L_ACAO ? 0xFF7FD3FF : 0xFFE6E9EE;
        char rot[80]; snprintf(rot, sizeof(rot), "%s%s", l->tipo == ED_L_ACAO ? "> " : "", l->rotulo);
        edTextoCortado(p, rot, 21, ED_PX + 18, y + 4, l->tipo == ED_L_SLIDER || l->tipo == ED_L_OPCAO ? 200 : ED_PW - 40, cr);
        char v[80] = "";
        if (l->tipo == ED_L_SLIDER && l->v) {
            snprintf(v, sizeof(v), "%d", *l->v);
            int bx = ED_PX + 220, bw = 120; float f = (float)(*l->v - l->mn) / (l->mx - l->mn > 0 ? l->mx - l->mn : 1);
            edRet(p, bx, y + 26, bw, 4, 0xFF4A5260); edRet(p, bx, y + 26, (int)(bw * f), 4, 0xFF29B6F6);
            if (l->mn < 0) edRet(p, bx + bw * (-l->mn) / (l->mx - l->mn), y + 22, 2, 12, 0xFFFFFFFF);
            edTexto(p, v, 20, ED_PX + ED_PW - 18 - edPlatLargura(v, 20), y + 2, 0xFFFFFFFF);
        }
        else if (l->tipo == ED_L_OPCAO && l->v) {
            const char* o = l->ops ? (*l->v >= 0 && *l->v < l->nops ? l->ops[*l->v] : "") : (edFerramenta == ED_T_TEXTO && *l->v < edTotalFontes ? edFontes[*l->v].nome : "");
            snprintf(v, sizeof(v), "< %s >", o);
            edTextoCortado(p, v, 20, ED_PX + 210, y + 4, ED_PW - 225, sel ? 0xFF7FD3FF : 0xFFFFFFFF);
        }
    }
    if (topo > 0) edTexto(p, "...", 20, ED_PX + ED_PW - 40, y0 - 24, 0xFF8A93A0);
    // extras visuais
    int ey = ED_VY + ED_VH - extra;
    if (edFerramenta == ED_T_COR) {
        // quadrado saturação x brilho e barra de matiz
        for (int y = 0; y < 180; y++) for (int x = 0; x < 180; x++) p[(ey + 20 + y) * 1920 + ED_PX + 20 + x] = edHsvCor((float)edCorH, x / 179.0f, 1 - y / 179.0f, 255);
        edRetBorda(p, ED_PX + 20 + edCorS * 179 / 100 - 5, ey + 20 + (100 - edCorV) * 179 / 100 - 5, 10, 10, 0xFFFFFFFF, 2);
        for (int y = 0; y < 180; y++) edRet(p, ED_PX + 212, ey + 20 + y, 26, 1, edHsvCor(y * 2.0f, 1, 1, 255));
        edRet(p, ED_PX + 208, ey + 20 + edCorH * 179 / 359 - 1, 34, 3, 0xFFFFFFFF);
        for (int i = 0; i < 16; i++) { int x = ED_PX + 256 + (i % 4) * 34, y = ey + 20 + (i / 4) * 34; edRet(p, x, y, 30, 30, edPaletaCores[edPaleta][i] | 0xFF000000); if (i == edPaletaItem) edRetBorda(p, x - 2, y - 2, 34, 34, 0xFFFFFFFF, 2); }
        char hex[40]; snprintf(hex, sizeof(hex), "#%06X", (edCorEditando ? edCor2 : edCor1) & 0xFFFFFF); edTexto(p, hex, 22, ED_PX + 20, ey + 214, 0xFFFFFFFF);
    }
    if (edFerramenta == ED_T_CURVAS) {
        int gx = ED_PX + 30, gy = ey + 10, gw = 340, gh = 220;
        edRet(p, gx, gy, gw, gh, 0xFF15171B);
        for (int i = 1; i < 4; i++) { edRet(p, gx + gw * i / 4, gy, 1, gh, 0xFF2E3138); edRet(p, gx, gy + gh * i / 4, gw, 1, 0xFF2E3138); }
        static const uint32_t cc[4] = { 0xFFFFFFFF, 0xFFFF5252, 0xFF69F0AE, 0xFF448AFF };
        for (int c = 0; c < 4; c++) {
            float lut[256]; edCurvaLut(edAj.curva[c], lut);
            for (int x = 0; x < gw; x++) { int i = x * 255 / (gw - 1); int y = gy + gh - 1 - (int)(lut[i] * (gh - 1)); edRet(p, gx + x, y, 1, c == edCurvaCanal ? 3 : 1, c == edCurvaCanal ? cc[c] : (cc[c] & 0x00FFFFFF) | 0xFF000000); }
        }
    }
    if (edFerramenta == ED_T_ADESIVO) {
        edAdMiniAtualizar();
        edRet(p, ED_PX + 100, ey + 10, 200, 200, 0xFF2E3138);
        if (edAdMini) {
            float f = fminf(190.0f / edAdMiniW, 190.0f / edAdMiniH);
            int w = (int)(edAdMiniW * f), h = (int)(edAdMiniH * f), x0 = ED_PX + 200 - w / 2, y0 = ey + 110 - h / 2;
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { uint32_t c = edAdMini[(size_t)(y * edAdMiniH / h) * edAdMiniW + x * edAdMiniW / w]; if (ED_A(c)) p[(y0 + y) * 1920 + x0 + x] = edSobre(0xFF2E3138, c, 100, ED_MIS_NORMAL); }
        }
        else edTexto(p, "sem imagem", 22, ED_PX + 140, ey + 100, 0xFF8A93A0);
    }
    if (edFerramenta == ED_T_CAMADAS) {
        // miniaturas, de cima para baixo
        int n = ed.n < 4 ? ed.n : 4;
        for (int k = 0; k < n; k++) {
            int i = ed.n - 1 - k, y = ey + k * 62, x = ED_PX + 20;
            edRet(p, x - 4, y - 2, ED_PW - 32, 58, i == ed.ativa ? 0xFF2E3A4B : 0xFF262A31);
            for (int yy = 0; yy < 52; yy++) for (int xx = 0; xx < 80; xx++) {
                uint32_t c = ed.cam[i].px[(size_t)(yy * ed.h / 52) * ed.w + xx * ed.w / 80];
                uint32_t xadrez = ((xx >> 3) + (yy >> 3)) & 1 ? 0xFF9E9E9E : 0xFFDDDDDD;
                p[(y + yy) * 1920 + x + xx] = edSobre(xadrez, c, 100, ED_MIS_NORMAL);
            }
            char t[80]; snprintf(t, sizeof(t), "%s%s  %d%%", ed.cam[i].visivel ? "" : "(oculta) ", ed.cam[i].nome, ed.cam[i].opacidade);
            edTextoCortado(p, t, 20, x + 92, y + 14, ED_PW - 130, 0xFFFFFFFF);
        }
    }
}
static void edDesenharMenuLista(uint32_t* p, const char* titulo, const char* const* itens, int total, const char* dica) {
    int w = 820, h = 90 + total * 46 + 50, x = (1920 - w) / 2, y = (1080 - h) / 2;
    edRet(p, x, y, w, h, 0xFF1B1E24); edRetBorda(p, x, y, w, h, 0xFF29B6F6, 2);
    edTexto(p, titulo, 32, x + 30, y + 22, 0xFFFFFFFF);
    for (int i = 0; i < total; i++) {
        int yy = y + 80 + i * 46;
        if (i == edMenuSel) edRet(p, x + 14, yy - 4, w - 28, 42, 0xFF2E3A4B);
        edTexto(p, itens[i], 24, x + 34, yy + 4, i == edMenuSel ? 0xFF7FD3FF : 0xFFE6E9EE);
    }
    edTexto(p, dica, 20, x + 30, y + h - 38, 0xFF8A93A0);
}
static void edDesenharSeletor(uint32_t* p) {
    int w = 1100, h = 860, x = (1920 - w) / 2, y = (1080 - h) / 2;
    edRet(p, x, y, w, h, 0xFF1B1E24); edRetBorda(p, x, y, w, h, 0xFF29B6F6, 2);
    const char* tit = edArqFinalidade == 2 ? "Escolha a foto da colagem" : edArqFinalidade == 1 ? "Inserir imagem como camada" : "Abrir imagem ou projeto";
    edTexto(p, tit, 32, x + 30, y + 20, 0xFFFFFFFF);
    edTextoCortado(p, edArqPasta[0] ? edArqPasta : "Lugares", 22, x + 30, y + 66, w - 60, 0xFF7FD3FF);
    const int vis = 16, passo = 42;
    if (edMenuSel < edArqTopo) edArqTopo = edMenuSel;
    if (edMenuSel >= edArqTopo + vis) edArqTopo = edMenuSel - vis + 1;
    if (!edTotalArqs) edTexto(p, "Nenhuma imagem aqui", 24, x + 30, y + 130, 0xFF8A93A0);
    for (int k = 0; k < vis && edArqTopo + k < edTotalArqs; k++) {
        int i = edArqTopo + k, yy = y + 110 + k * passo;
        if (i == edMenuSel) edRet(p, x + 14, yy - 4, w - 28, passo - 4, 0xFF2E3A4B);
        char t[260]; snprintf(t, sizeof(t), "%s%s", edArqs[i].pasta ? "[pasta] " : "", edArqs[i].nome);
        edTextoCortado(p, t, 23, x + 34, yy + 4, w - 70, edArqs[i].pasta ? 0xFFFFD54F : 0xFFE6E9EE);
    }
    edTexto(p, "X abre   O volta uma pasta   L1/R1 pagina", 20, x + 30, y + h - 40, 0xFF8A93A0);
}
void editorImagemRenderizar(uint32_t* p) {
    static int vistaW = 0, vistaH = 0;
    if (ed.w != vistaW || ed.h != vistaH) { vistaW = ed.w; vistaH = ed.h; edAjustarVista(); }
    edRet(p, 0, 0, 1920, 1080, 0xFF15171B);
    edCompor();
    edDesenharCanvas(p);
    // topo
    char t[300];
    const char* nome = ed.caminho[0] ? (strrchr(ed.caminho, '/') ? strrchr(ed.caminho, '/') + 1 : ed.caminho) : "Nova imagem";
    snprintf(t, sizeof(t), "Editor de imagem  -  %.120s%s   %dx%d   %d%%   camada %d/%d%s", nome, ed.mudou ? " *" : "", ed.w, ed.h, (int)(edZoom * 100 + .5f), ed.ativa + 1, ed.n, ed.sel ? "   (selecao)" : "");
    edTextoCortado(p, t, 24, 20, 22, 1500, 0xFFFFFFFF);
    edTexto(p, "Cor 1", 18, 1560, 6, 0xFF8A93A0); edTexto(p, "Cor 2", 18, 1660, 6, 0xFF8A93A0);
    edRet(p, 1560, 30, 80, 36, edCor1 | 0xFF000000); edRetBorda(p, 1560, 30, 80, 36, 0xFFFFFFFF, 2);
    edRet(p, 1660, 30, 80, 36, edCor2 | 0xFF000000); edRetBorda(p, 1660, 30, 80, 36, 0xFF8A93A0, 2);
    if (edMostrarOriginal) edTexto(p, "ORIGINAL", 26, 1770, 34, 0xFFFFD54F);
    // ferramentas
    edRet(p, 0, ED_VY, ED_VX - 6, ED_VH, 0xFF1F2228);
    for (int i = 0; i < ED_T_TOTAL; i++) {
        int y = ED_VY + 8 + i * 36;
        if (i == edFerramenta) edRet(p, 6, y - 3, ED_VX - 18, 34, 0xFF29B6F6);
        edTexto(p, edNomesFerramenta[i], 21, 18, y + 3, i == edFerramenta ? 0xFF0B1A26 : 0xFFD0D4DA);
    }
    edDesenharPainel(p);
    // rodapé
    edRet(p, 0, ED_VY + ED_VH + 4, 1920, 1080 - ED_VY - ED_VH - 4, 0xFF15171B);
    const char* dica = "L1/R1 ferramenta   Cima/Baixo opcao   Esq/Dir valor   Triangulo executa   X usa   Quadrado desfaz   Touchpad (clique) refaz   L2/R2 zoom   L3 original   Options menu   O sai";
    if ((edFerramenta == ED_T_TEXTO || edFerramenta == ED_T_ADESIVO || edFerramenta == ED_T_MOVER) && edObj.ativo) dica = "Analogico move   L2/R2 tamanho   Analogico direito gira   X fixa numa camada nova   O descarta   Triangulo muda o texto";
    edTexto(p, dica, 19, 20, 1046, 0xFF9AA3AF);
    if (edAvisoAte > edPlatAgoraMs()) {
        int w = edPlatLargura(edAvisoTxt, 24) + 40;
        edRet(p, ED_VX + (ED_VW - w) / 2, ED_VY + 16, w, 46, 0xE0101215 | 0xFF000000); edRetBorda(p, ED_VX + (ED_VW - w) / 2, ED_VY + 16, w, 46, 0xFF29B6F6, 2);
        edTexto(p, edAvisoTxt, 24, ED_VX + (ED_VW - w) / 2 + 20, ED_VY + 26, 0xFFFFFFFF);
    }
    if (edMenu == 1) edDesenharMenuLista(p, "Menu", edItensMenu, ED_TOTAL_MENU, "X escolhe   O fecha");
    else if (edMenu == 2) edDesenharMenuLista(p, "Novo ou modelo", edItensNovo, ED_TOTAL_NOVO, "X escolhe   O fecha");
    else if (edMenu == 3) edDesenharSeletor(p);
    else if (edMenu == 4) { static const char* const s[] = { "Salvar (PNG) e sair", "Sair sem salvar", "Continuar editando" }; edDesenharMenuLista(p, "A imagem mudou", s, 3, "X escolhe"); }
    if (edOpPendente) {
        int w = 520, h = 120, x = (1920 - w) / 2, y = (1080 - h) / 2;
        edRet(p, x, y, w, h, 0xFF1B1E24); edRetBorda(p, x, y, w, h, 0xFF29B6F6, 3);
        edTexto(p, edOpTexto, 30, x + 40, y + 44, 0xFFFFFFFF);
        edOpMostrada = true;
    }
}
