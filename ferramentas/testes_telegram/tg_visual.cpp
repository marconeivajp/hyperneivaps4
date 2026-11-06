// TG_VISUAL_INICIO
#define TG_MAX_ARQUIVOS 700
#define TG_MEMORIA_IMAGENS (56 * 1024 * 1024)
#define TG_MAX_GLIFOS 4096
#define TG_MAX_FONTES 5
#define TG_MAX_EMOJIS 320
#define TG_LISTA_L 640
#define TG_TOPO_H 84
#define TG_TOPO_LISTA 132
#define TG_BASE_H 76
#define TG_LINHA_LISTA 92
#define TG_BALAO_MAX 820
#define TG_FOTO_MAX 560

struct TgArquivo { int id; bool pedido, baixado, falhou, quer; int largura, altura, uso, limite; uint32_t* pixels; char caminho[220]; };
struct TgGlifo { uint32_t cp; short tam, w, h, x0, y0, avanco; unsigned char* alfa; };
struct TgEmojiIndice { char nome[72]; uint32_t inicio, tamanho; };
struct TgEmojiImagem { int indice, uso; uint32_t* pixels; };

static TgArquivo tgArquivos[TG_MAX_ARQUIVOS];
static int tgTotalArquivos = 0, tgRelogioUso = 0;
static size_t tgMemoriaImagens = 0;
static TgGlifo tgGlifos[TG_MAX_GLIFOS];
static TgEmojiIndice* tgEmojis = NULL;
static int tgTotalEmojis = 0;
static FILE* tgPacoteEmoji = NULL;
static uint32_t tgPacoteBase = 0;
static TgEmojiImagem tgEmojiImagens[TG_MAX_EMOJIS];
static int tgFusoMinutos = -180;
static stbtt_fontinfo tgFontes[TG_MAX_FONTES];
static int tgTotalFontes = 0;

// Fontes do próprio PS4 cobrem japonês, chinês, coreano, árabe e símbolos que a do app não tem.
static void tgCarregarFontes() {
    static const char* nomes[TG_MAX_FONTES] = { "SSTJpPro-Regular.otf", "DFHEI5-SONY.ttf", "SCEPS4Yoongd-Medium.otf", "SSTArabic-Roman.otf", "SST-Roman.otf" };
    for (int i = 0; i < TG_MAX_FONTES; i++) {
        char caminho[96];
        snprintf(caminho, sizeof(caminho), "/preinst/common/font/%s", nomes[i]);
        FILE* f = fopen(caminho, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long tamanho = ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char* dados = tamanho > 0 ? (unsigned char*)malloc(tamanho) : NULL;
        bool certo = dados && fread(dados, 1, tamanho, f) == (size_t)tamanho;
        fclose(f);
        int deslocamento = certo ? stbtt_GetFontOffsetForIndex(dados, 0) : -1;
        if (certo && deslocamento >= 0 && stbtt_InitFont(&tgFontes[tgTotalFontes], dados, deslocamento)) tgTotalFontes++;
        else free(dados);
    }
    pthread_mutex_lock(&tgTrava);
    // Glifos já guardados como ? podem existir agora nas fontes novas.
    for (int i = 0; i < TG_MAX_GLIFOS; i++) { free(tgGlifos[i].alfa); memset(&tgGlifos[i], 0, sizeof(tgGlifos[i])); }
    pthread_mutex_unlock(&tgTrava);
    FILE* d = fopen("/data/HyperNeivaTelegram/diagnostico.txt", "w");
    if (d) { fprintf(d, "emojis=%d fontes=%d\n", tgTotalEmojis, tgTotalFontes); fclose(d); }
    tgSujo = true;
}

// Letras "estilizadas" do Unicode são comuns em nomes e canais: viram a letra comum.
static uint32_t tgDesestilizar(uint32_t cp) {
    static const struct { uint32_t cp; char letra; } versaletes[] = {
        { 0x1D00, 'A' }, { 0x0299, 'B' }, { 0x1D04, 'C' }, { 0x1D05, 'D' }, { 0x1D07, 'E' }, { 0xA730, 'F' }, { 0x0262, 'G' }, { 0x029C, 'H' }, { 0x026A, 'I' },
        { 0x1D0A, 'J' }, { 0x1D0B, 'K' }, { 0x029F, 'L' }, { 0x1D0D, 'M' }, { 0x0274, 'N' }, { 0x1D0F, 'O' }, { 0x1D18, 'P' }, { 0x0280, 'R' }, { 0xA731, 'S' },
        { 0x1D1B, 'T' }, { 0x1D1C, 'U' }, { 0x1D20, 'V' }, { 0x1D21, 'W' }, { 0x028F, 'Y' }, { 0x1D22, 'Z' },
    };
    if (cp >= 0x1D400 && cp <= 0x1D6A3) { uint32_t i = (cp - 0x1D400) % 52; return i < 26 ? 'A' + i : 'a' + i - 26; }
    if (cp >= 0x1D7CE && cp <= 0x1D7FF) return '0' + (cp - 0x1D7CE) % 10;
    if (cp >= 0xFF01 && cp <= 0xFF5E) return cp - 0xFEE0;
    if (cp >= 0x24B6 && cp <= 0x24CF) return 'A' + cp - 0x24B6;
    if (cp >= 0x24D0 && cp <= 0x24E9) return 'a' + cp - 0x24D0;
    if (cp >= 0x1F130 && cp <= 0x1F149) return 'A' + cp - 0x1F130;
    if (cp >= 0x1F150 && cp <= 0x1F169) return 'A' + cp - 0x1F150;
    if (cp >= 0x1F170 && cp <= 0x1F189) return 'A' + cp - 0x1F170;
    if (cp >= 0x1F1E6 && cp <= 0x1F1FF) return 'A' + cp - 0x1F1E6;
    for (size_t i = 0; i < sizeof(versaletes) / sizeof(versaletes[0]); i++) if (versaletes[i].cp == cp) return (uint32_t)versaletes[i].letra;
    return 0;
}

// Marcas sem desenho próprio: mostrar ? aqui só sujaria o texto.
static bool tgInvisivel(uint32_t cp) {
    return cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x200E || cp == 0x200F || cp == 0x2060 || cp == 0xFEFF ||
        (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0xE0020 && cp <= 0xE007F);
}

// Guarda só o número do caractere que faltou, nunca o texto da mensagem.
static void tgAnotarFaltando(uint32_t cp) {
    static uint32_t vistos[200];
    static int total = 0;
    if (total >= 200) return;
    for (int i = 0; i < total; i++) if (vistos[i] == cp) return;
    vistos[total++] = cp;
    FILE* f = fopen("/data/HyperNeivaTelegram/diagnostico.txt", "a");
    if (f) { fprintf(f, "faltou %x\n", cp); fclose(f); }
}

static void tgAbrirEmojis(const char* caminho) {
    tgPacoteEmoji = fopen(caminho, "rb");
    if (!tgPacoteEmoji) return;
    uint32_t total = 0;
    if (fread(&total, 4, 1, tgPacoteEmoji) != 1 || total == 0 || total > 20000) return;
    TgEmojiIndice* lista = (TgEmojiIndice*)malloc(sizeof(TgEmojiIndice) * total);
    if (!lista) return;
    if (fread(lista, sizeof(TgEmojiIndice), total, tgPacoteEmoji) != total) { free(lista); return; }
    tgEmojis = lista;
    tgTotalEmojis = (int)total;
    tgPacoteBase = 4 + total * (uint32_t)sizeof(TgEmojiIndice);
}

static int tgBuscarEmoji(const char* nome) {
    int a = 0, b = tgTotalEmojis - 1;
    while (a <= b) {
        int m = (a + b) / 2;
        int c = strcmp(nome, tgEmojis[m].nome);
        if (c == 0) return m;
        if (c < 0) b = m - 1; else a = m + 1;
    }
    return -1;
}

static int tgLerCodigo(const char* s, uint32_t* cp) {
    unsigned char c = (unsigned char)s[0];
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t v = extra ? (c & (0x3Fu >> extra)) : c;
    int n = 1;
    for (int i = 0; i < extra && ((unsigned char)s[n] & 0xC0) == 0x80; i++) v = (v << 6) | ((unsigned char)s[n++] & 0x3F);
    *cp = v;
    return n;
}

// Acha a maior sequência de emoji que começa em s. O nome no pacote às vezes omite o FE0F.
static int tgEmojiEm(const char* s, int* bytes) {
    if (!tgEmojis) return -1;
    uint32_t cps[10];
    int fim[10], n = 0, pos = 0;
    while (n < 10 && s[pos]) {
        uint32_t cp;
        pos += tgLerCodigo(s + pos, &cp);
        cps[n] = cp;
        fim[n++] = pos;
        if (n >= 2 && cps[n - 1] != 0x200D && cps[n - 1] != 0xFE0F && cps[n - 1] != 0x20E3 && cps[n - 2] != 0x200D &&
            !(cps[n - 1] >= 0x1F3FB && cps[n - 1] <= 0x1F3FF) && !(cps[n - 1] >= 0x1F1E6 && cps[n - 1] <= 0x1F1FF) && !(cps[n - 1] >= 0xE0020 && cps[n - 1] <= 0xE007F)) { n--; break; }
    }
    if (n == 0 || (cps[0] < 0x203C && !(n >= 2 && (cps[1] == 0xFE0F || cps[1] == 0x20E3)) && cps[0] != 0xA9 && cps[0] != 0xAE)) return -1;
    for (int tam = n; tam >= 1; tam--) {
        for (int semVariacao = 0; semVariacao < 2; semVariacao++) {
            char nome[80];
            int t = 0;
            nome[0] = '\0';
            for (int i = 0; i < tam && t < 66; i++) {
                if (semVariacao && cps[i] == 0xFE0F) continue;
                t += snprintf(nome + t, sizeof(nome) - t, t ? "-%x" : "%x", cps[i]);
            }
            int achou = tgBuscarEmoji(nome);
            if (achou >= 0) { *bytes = fim[tam - 1]; return achou; }
        }
    }
    return -1;
}

static uint32_t* tgImagemEmoji(int indice) {
    static int relogio = 0;
    int livre = 0;
    relogio++;
    for (int i = 0; i < TG_MAX_EMOJIS; i++) {
        if (tgEmojiImagens[i].pixels && tgEmojiImagens[i].indice == indice) { tgEmojiImagens[i].uso = relogio; return tgEmojiImagens[i].pixels; }
        if (tgEmojiImagens[i].uso < tgEmojiImagens[livre].uso) livre = i;
    }
    uint32_t tamanho = tgEmojis[indice].tamanho;
    unsigned char* dados = (unsigned char*)malloc(tamanho);
    if (!dados) return NULL;
    uint32_t* pixels = NULL;
    fseek(tgPacoteEmoji, (long)(tgPacoteBase + tgEmojis[indice].inicio), SEEK_SET);
    if (fread(dados, 1, tamanho, tgPacoteEmoji) == tamanho) {
        int w, h, c;
        pixels = (uint32_t*)stbi_load_from_memory(dados, (int)tamanho, &w, &h, &c, 4);
        if (pixels && (w != 72 || h != 72)) { stbi_image_free(pixels); pixels = NULL; }
    }
    free(dados);
    if (!pixels) return NULL;
    if (tgEmojiImagens[livre].pixels) stbi_image_free(tgEmojiImagens[livre].pixels);
    tgEmojiImagens[livre].indice = indice;
    tgEmojiImagens[livre].pixels = pixels;
    tgEmojiImagens[livre].uso = relogio;
    return pixels;
}

static inline uint32_t tgMisturar(uint32_t fundo, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    uint32_t fr = fundo & 255, fg = (fundo >> 8) & 255, fb = (fundo >> 16) & 255, inv = 255 - a;
    return 0xFF000000u | (((b * a + fb * inv) / 255) << 16) | (((g * a + fg * inv) / 255) << 8) | ((r * a + fr * inv) / 255);
}

static void tgDesenharEmoji(uint32_t* p, int indice, int x, int y, int tam) {
    uint32_t* e = tgImagemEmoji(indice);
    if (!e) return;
    for (int j = 0; j < tam; j++) {
        int py = y + j;
        if (py < 0 || py >= 1080) continue;
        for (int i = 0; i < tam; i++) {
            int px = x + i;
            if (px < 0 || px >= 1920) continue;
            // Média de uma área da imagem de 72 pixels: reduz sem serrilhar.
            int x0 = i * 72 / tam, x1 = (i + 1) * 72 / tam, y0 = j * 72 / tam, y1 = (j + 1) * 72 / tam;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int v = y0; v < y1; v++) for (int u = x0; u < x1; u++) {
                uint32_t c = e[v * 72 + u], ca = c >> 24;
                r += (c & 255) * ca; g += ((c >> 8) & 255) * ca; b += ((c >> 16) & 255) * ca; a += ca; n++;
            }
            if (!a || !n) continue;
            p[py * 1920 + px] = tgMisturar(p[py * 1920 + px], r / a, g / a, b / a, a / n);
        }
    }
}

static TgGlifo* tgGlifo(uint32_t cp, int tam) {
    uint32_t h = (cp * 2654435761u + (uint32_t)tam * 40503u) % TG_MAX_GLIFOS;
    for (int i = 0; i < 16; i++) {
        TgGlifo* g = &tgGlifos[(h + i) % TG_MAX_GLIFOS];
        if (g->tam == tam && g->cp == cp) return g;
        if (g->tam == 0 || i == 15) {
            if (g->alfa) free(g->alfa);
            memset(g, 0, sizeof(*g));
            stbtt_fontinfo* fonte = &font;
            uint32_t usado = cp;
            if (!stbtt_FindGlyphIndex(&font, (int)cp)) {
                uint32_t simples = tgDesestilizar(cp);
                if (simples) usado = simples;
                else {
                    fonte = NULL;
                    for (int f = 0; f < tgTotalFontes && !fonte; f++) if (stbtt_FindGlyphIndex(&tgFontes[f], (int)cp)) fonte = &tgFontes[f];
                    if (!fonte) { fonte = &font; usado = '?'; tgAnotarFaltando(cp); }
                }
            }
            float s = stbtt_ScaleForPixelHeight(fonte, (float)tam);
            int avanco, lsb, x0, y0, x1, y1;
            stbtt_GetCodepointHMetrics(fonte, (int)usado, &avanco, &lsb);
            stbtt_GetCodepointBitmapBox(fonte, (int)usado, s, s, &x0, &y0, &x1, &y1);
            g->cp = cp; g->tam = (short)tam; g->w = (short)(x1 - x0); g->h = (short)(y1 - y0);
            g->x0 = (short)x0; g->y0 = (short)y0; g->avanco = (short)(avanco * s + 0.5f);
            if (g->w > 0 && g->h > 0) {
                g->alfa = (unsigned char*)malloc((size_t)g->w * g->h);
                if (g->alfa) stbtt_MakeCodepointBitmap(fonte, g->alfa, g->w, g->h, g->w, s, s, (int)usado);
            }
            return g;
        }
    }
    return NULL;
}

// Desenha ou só mede quando p é NULL. Devolve a largura em pixels.
static int tgEscrever(uint32_t* p, const char* texto, int inicioBytes, int fimBytes, int tam, int x, int y, uint32_t cor) {
    float s = stbtt_ScaleForPixelHeight(&font, (float)tam);
    int asc;
    stbtt_GetFontVMetrics(&font, &asc, 0, 0);
    asc = (int)(asc * s);
    int cx = x, i = inicioBytes;
    uint32_t cr = cor & 255, cg = (cor >> 8) & 255, cb = (cor >> 16) & 255;
    while (i < fimBytes && texto[i]) {
        int bytes = 0;
        int e = tgEmojiEm(texto + i, &bytes);
        if (e >= 0) {
            int lado = tam + 4;
            if (p) tgDesenharEmoji(p, e, cx, y + 1, lado);
            cx += lado + 2;
            i += bytes;
            continue;
        }
        uint32_t cp;
        i += tgLerCodigo(texto + i, &cp);
        if (tgInvisivel(cp)) continue;
        if (cp == '\n' || cp == '\t') cp = ' ';
        TgGlifo* g = tgGlifo(cp, tam);
        if (!g) continue;
        if (p && g->alfa) {
            for (int v = 0; v < g->h; v++) {
                int py = y + asc + g->y0 + v;
                if (py < 0 || py >= 1080) continue;
                for (int u = 0; u < g->w; u++) {
                    int px = cx + g->x0 + u;
                    uint32_t a = g->alfa[v * g->w + u];
                    if (a && px >= 0 && px < 1920) p[py * 1920 + px] = tgMisturar(p[py * 1920 + px], cr, cg, cb, a);
                }
            }
        }
        cx += g->avanco;
    }
    return cx - x;
}

static int tgLargura(const char* texto, int tam) { return tgEscrever(NULL, texto, 0, 1 << 30, tam, 0, 0, 0); }

// Corta com reticências sem partir um caractere UTF-8 no meio.
static void tgEscreverCortado(uint32_t* p, const char* texto, int tam, int x, int y, int largura, uint32_t cor) {
    if (tgLargura(texto, tam) <= largura) { tgEscrever(p, texto, 0, 1 << 30, tam, x, y, cor); return; }
    int reticencias = tgLargura("...", tam);
    int i = 0, ultimo = 0;
    while (texto[i]) {
        int bytes = 0;
        if (tgEmojiEm(texto + i, &bytes) < 0) { uint32_t cp; bytes = tgLerCodigo(texto + i, &cp); }
        if (tgEscrever(NULL, texto, 0, i + bytes, tam, 0, 0, 0) + reticencias > largura) break;
        i += bytes;
        ultimo = i;
    }
    int w = tgEscrever(p, texto, 0, ultimo, tam, x, y, cor);
    tgEscrever(p, "...", 0, 3, tam, x + w, y, cor);
}

// Próxima quebra de linha que cabe em largura, preferindo espaço. Devolve o fim da linha.
static int tgQuebrar(const char* t, int inicio, int tam, int largura, int* proximo) {
    int i = inicio, espaco = -1, w = 0;
    while (t[i] && t[i] != '\n') {
        int bytes = 0;
        if (tgEmojiEm(t + i, &bytes) < 0) { uint32_t cp; bytes = tgLerCodigo(t + i, &cp); }
        int passo = tgEscrever(NULL, t, i, i + bytes, tam, 0, 0, 0);
        if (w + passo > largura && i > inicio) {
            if (espaco > inicio) { *proximo = espaco + 1; return espaco; }
            *proximo = i;
            return i;
        }
        if (t[i] == ' ') espaco = i;
        w += passo;
        i += bytes;
    }
    *proximo = t[i] == '\n' ? i + 1 : i;
    return i;
}

static void tgRetangulo(uint32_t* p, int x, int y, int largura, int altura, uint32_t cor) {
    for (int j = y < 0 ? 0 : y; j < y + altura && j < 1080; j++)
        for (int i = x < 0 ? 0 : x; i < x + largura && i < 1920; i++) p[j * 1920 + i] = cor;
}

// Só as linhas entre topo e base são pintadas, para balões não invadirem as barras.
static void tgArredondado(uint32_t* p, int x, int y, int largura, int altura, int raio, uint32_t cor, int topo, int base) {
    uint32_t r = cor & 255, g = (cor >> 8) & 255, b = (cor >> 16) & 255;
    for (int j = 0; j < altura; j++) {
        int py = y + j;
        if (py < topo || py >= base) continue;
        int dy = j < raio ? raio - j : (j >= altura - raio ? j - (altura - raio) + 1 : 0);
        for (int i = 0; i < largura; i++) {
            int px = x + i;
            if (px < 0 || px >= 1920) continue;
            int dx = i < raio ? raio - i : (i >= largura - raio ? i - (largura - raio) + 1 : 0);
            if (!dx || !dy) { p[py * 1920 + px] = cor; continue; }
            float d = sqrtf((float)(dx * dx + dy * dy)) - (float)raio;
            if (d >= 1.0f) continue;
            if (d <= 0.0f) p[py * 1920 + px] = cor;
            else p[py * 1920 + px] = tgMisturar(p[py * 1920 + px], r, g, b, (uint32_t)((1.0f - d) * 255.0f));
        }
    }
}

static void tgDesenharPixels(uint32_t* p, const TgArquivo* a, int x, int y, int largura, int altura, int raio, int topo, int base) {
    for (int j = 0; j < altura; j++) {
        int py = y + j;
        if (py < topo || py >= base) continue;
        int sy = j * a->altura / altura;
        int dy = j < raio ? raio - j : (j >= altura - raio ? j - (altura - raio) + 1 : 0);
        for (int i = 0; i < largura; i++) {
            int px = x + i;
            if (px < 0 || px >= 1920) continue;
            int dx = i < raio ? raio - i : (i >= largura - raio ? i - (largura - raio) + 1 : 0);
            uint32_t cobertura = 255;
            if (dx && dy) {
                float d = sqrtf((float)(dx * dx + dy * dy)) - (float)raio;
                if (d >= 1.0f) continue;
                if (d > 0.0f) cobertura = (uint32_t)((1.0f - d) * 255.0f);
            }
            uint32_t c = a->pixels[sy * a->largura + i * a->largura / largura];
            uint32_t alfa = (c >> 24) * cobertura / 255;
            if (alfa) p[py * 1920 + px] = tgMisturar(p[py * 1920 + px], c & 255, (c >> 8) & 255, (c >> 16) & 255, alfa);
        }
    }
}

static TgArquivo* tgArquivo(int id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < tgTotalArquivos; i++) if (tgArquivos[i].id == id) return &tgArquivos[i];
    if (tgTotalArquivos >= TG_MAX_ARQUIVOS) return NULL;
    TgArquivo* a = &tgArquivos[tgTotalArquivos++];
    memset(a, 0, sizeof(*a));
    a->id = id;
    return a;
}

// Pede o arquivo na primeira vez que aparece na tela e devolve a imagem se já estiver pronta.
static TgArquivo* tgImagem(int id) {
    TgArquivo* a = tgArquivo(id);
    if (!a) return NULL;
    a->uso = ++tgRelogioUso;
    if (!a->pedido) { a->pedido = true; tgBaixarArquivo(id); }
    if (!a->pixels) a->quer = true;
    return a->pixels ? a : NULL;
}

static uint32_t tgCorDoId(long long id) {
    static const uint32_t cores[7] = { TG_COR(225, 110, 100), TG_COR(240, 165, 80), TG_COR(150, 120, 225), TG_COR(95, 190, 110), TG_COR(80, 185, 200), TG_COR(85, 150, 225), TG_COR(235, 110, 165) };
    return cores[(unsigned long long)(id < 0 ? -id : id) % 7];
}

static void tgAvatar(uint32_t* p, long long id, int arquivo, const char* nome, int x, int y, int lado, int topo, int base) {
    TgArquivo* a = tgImagem(arquivo);
    if (a) { tgDesenharPixels(p, a, x, y, lado, lado, lado / 2, topo, base); return; }
    tgArredondado(p, x, y, lado, lado, lado / 2, tgCorDoId(id), topo, base);
    char inicial[8] = "";
    int bytes = 1;
    if (nome && nome[0]) {
        if (tgEmojiEm(nome, &bytes) < 0) { uint32_t cp; bytes = tgLerCodigo(nome, &cp); }
        if (bytes > 7) bytes = 7;
        memcpy(inicial, nome, bytes);
        inicial[bytes] = '\0';
        if (bytes == 1 && inicial[0] >= 'a' && inicial[0] <= 'z') inicial[0] -= 32;
    }
    int tam = lado * 45 / 100;
    tgEscrever(p, inicial, 0, 8, tam, x + (lado - tgLargura(inicial, tam)) / 2, y + (lado - tam) / 2 - 2, TG_COR(255, 255, 255));
}

static void tgHora(long long data, char* saida, int tamanho) {
    long long local = data + tgFusoMinutos * 60LL;
    int minutos = (int)((local % 86400 + 86400) % 86400) / 60;
    snprintf(saida, tamanho, "%02d:%02d", minutos / 60, minutos % 60);
}

static void tgOrdenarConversas() {
    int slot = tgSlotDaAba(tgAba);
    tgVisiveis = 0;
    for (int i = 0; i < tgTotalConversas; i++) {
        if (tgConversas[i].ordens[slot] <= 0) continue;
        int j = tgVisiveis++;
        while (j > 0 && tgConversas[tgIndice[j - 1]].ordens[slot] < tgConversas[i].ordens[slot]) { tgIndice[j] = tgIndice[j - 1]; j--; }
        tgIndice[j] = i;
    }
    int linhas = (1080 - TG_TOPO_LISTA - 44) / TG_LINHA_LISTA;
    if (tgSelecao >= tgVisiveis) tgSelecao = tgVisiveis > 0 ? tgVisiveis - 1 : 0;
    if (tgSelecao < tgTopo) tgTopo = tgSelecao;
    if (tgSelecao >= tgTopo + linhas) tgTopo = tgSelecao - linhas + 1;
}

static void tgDesenharAbas(uint32_t* p) {
    int largura[TG_MAX_PASTAS + 2], fimSelecionada = 0, x = 24;
    for (int i = 0; i < tgTotalAbas(); i++) {
        largura[i] = tgLargura(tgNomeDaAba(i), 23);
        x += largura[i] + 34;
        if (i == tgAba) fimSelecionada = x;
    }
    int deslocamento = fimSelecionada > TG_LISTA_L - 10 ? fimSelecionada - (TG_LISTA_L - 10) : 0;
    x = 24 - deslocamento;
    for (int i = 0; i < tgTotalAbas(); i++) {
        if (x >= 8 && x + largura[i] < TG_LISTA_L - 8) {
            tgEscrever(p, tgNomeDaAba(i), 0, 1 << 30, 23, x, 92, i == tgAba ? TG_COR(100, 180, 240) : TG_COR(125, 139, 153));
            if (i == tgAba) tgArredondado(p, x - 4, TG_TOPO_LISTA - 5, largura[i] + 8, 4, 2, TG_COR(100, 180, 240), 0, 1080);
        }
        x += largura[i] + 34;
    }
    tgRetangulo(p, 0, TG_TOPO_LISTA - 1, TG_LISTA_L, 1, TG_COR(16, 24, 32));
}

static void tgDesenharLinha(uint32_t* p, const TgConversa* c, long long id, const char* titulo, const char* texto, long long data, int naoLidas, int y, bool marcada) {
    if (marcada) tgRetangulo(p, 0, y, TG_LISTA_L, TG_LINHA_LISTA, tgTela == TG_MENSAGENS && !tgEmBusca ? TG_COR(43, 82, 120) : TG_COR(46, 112, 165));
    tgAvatar(p, id, c ? c->foto : 0, titulo, 16, y + 10, 72, 0, 1080);
    char hora[8] = "";
    int larguraHora = 0;
    if (data) { tgHora(data, hora, sizeof(hora)); larguraHora = tgLargura(hora, 21); tgEscrever(p, hora, 0, 8, 21, TG_LISTA_L - 18 - larguraHora, y + 16, marcada ? TG_COR(225, 235, 245) : TG_COR(125, 139, 153)); }
    tgEscreverCortado(p, titulo[0] ? titulo : "(sem nome)", 27, 104, y + 12, TG_LISTA_L - 104 - 30 - larguraHora, TG_COR(255, 255, 255));
    int reserva = 0;
    if (naoLidas > 0) {
        char n[12];
        snprintf(n, sizeof(n), "%d", naoLidas);
        int w = tgLargura(n, 20) + 20;
        if (w < 34) w = 34;
        reserva = w + 10;
        tgArredondado(p, TG_LISTA_L - 18 - w, y + 50, w, 34, 17, marcada ? TG_COR(255, 255, 255) : TG_COR(64, 130, 188), 0, 1080);
        tgEscrever(p, n, 0, 12, 20, TG_LISTA_L - 18 - w + (w - tgLargura(n, 20)) / 2, y + 55, marcada ? TG_COR(43, 82, 120) : TG_COR(255, 255, 255));
    }
    tgEscreverCortado(p, texto, 23, 104, y + 52, TG_LISTA_L - 104 - 18 - reserva, marcada ? TG_COR(225, 235, 245) : TG_COR(125, 139, 153));
}

static const TgConversa* tgConversaPorId(long long id) {
    for (int i = 0; i < tgTotalConversas; i++) if (tgConversas[i].id == id) return &tgConversas[i];
    return NULL;
}

static void tgDesenharResultados(uint32_t* p, int base) {
    char titulo[160];
    snprintf(titulo, sizeof(titulo), "%s: %s", tgBuscaConversa ? "Nesta conversa" : "Pesquisa", tgBuscaTexto);
    tgEscreverCortado(p, titulo, 25, 28, 92, TG_LISTA_L - 56, TG_COR(100, 180, 240));
    tgRetangulo(p, 0, TG_TOPO_LISTA - 1, TG_LISTA_L, 1, TG_COR(16, 24, 32));
    if (tgTotalResultados == 0) {
        tgEscrever(p, tgBuscasPendentes > 0 ? "Pesquisando..." : "Nada encontrado", 0, 1 << 30, 26, 28, TG_TOPO_LISTA + 24, TG_COR(125, 139, 153));
        return;
    }
    int linhas = (base - TG_TOPO_LISTA) / TG_LINHA_LISTA;
    if (tgSelResultado < tgTopoResultado) tgTopoResultado = tgSelResultado;
    if (tgSelResultado >= tgTopoResultado + linhas) tgTopoResultado = tgSelResultado - linhas + 1;
    for (int i = 0; i < linhas && tgTopoResultado + i < tgTotalResultados; i++) {
        const TgResultado* r = &tgResultados[tgTopoResultado + i];
        const TgConversa* c = tgConversaPorId(r->conversa);
        tgDesenharLinha(p, c, r->conversa, c ? c->titulo : "", r->mensagem ? r->texto : (c ? c->ultima : ""), r->data, 0, TG_TOPO_LISTA + i * TG_LINHA_LISTA, tgTopoResultado + i == tgSelResultado);
    }
}

static void tgDesenharIconeDownloads(uint32_t* p, int x) {
    int ativos = 0;
    for (int i = 0; i < tgTotalDownloads; i++) if (tgDownloads[i].estado < TG_D_PRONTO) ativos++;
    tgArredondado(p, x, 20, 46, 46, 23, tgFilaAberta ? TG_COR(46, 112, 165) : TG_COR(36, 47, 61), 0, 1080);
    int e = tgEmojis ? tgBuscarEmoji("2b07") : -1;
    if (e >= 0) tgDesenharEmoji(p, e, x + 9, 29, 28);
    else tgEscrever(p, "v", 0, 1, 26, x + 16, 28, TG_COR(255, 255, 255));
    if (ativos > 0) {
        char n[8];
        snprintf(n, sizeof(n), "%d", ativos);
        tgArredondado(p, x + 30, 14, 26, 26, 13, TG_COR(64, 130, 188), 0, 1080);
        tgEscrever(p, n, 0, 8, 17, x + 30 + (26 - tgLargura(n, 17)) / 2, 18, TG_COR(255, 255, 255));
    }
    tgEscrever(p, "Options", 0, 1 << 30, 18, x + 62, 34, TG_COR(90, 104, 118));
}

static void tgDesenharLista(uint32_t* p) {
    tgOrdenarConversas();
    int base = 1080 - 44;
    tgRetangulo(p, 0, 0, TG_LISTA_L, 1080, TG_COR(23, 33, 43));
    tgEscrever(p, "Telegram", 0, 8, 36, 28, 22, TG_COR(255, 255, 255));
    tgDesenharIconeDownloads(p, 28 + tgLargura("Telegram", 36) + 18);
    if (tgEmBusca) tgDesenharResultados(p, base);
    else {
        tgDesenharAbas(p);
        if (tgVisiveis == 0) tgEscrever(p, tgFimDaLista[tgSlotDaAba(tgAba)] ? "Nenhuma conversa aqui" : "Carregando conversas...", 0, 1 << 30, 26, 28, TG_TOPO_LISTA + 24, TG_COR(125, 139, 153));
        int linhas = (base - TG_TOPO_LISTA) / TG_LINHA_LISTA;
        for (int i = 0; i < linhas && tgTopo + i < tgVisiveis; i++) {
            const TgConversa* c = &tgConversas[tgIndice[tgTopo + i]];
            tgDesenharLinha(p, c, c->id, c->titulo, c->ultima, c->data, c->naoLidas, TG_TOPO_LISTA + i * TG_LINHA_LISTA, tgTopo + i == tgSelecao);
        }
    }
    tgRetangulo(p, 0, base, TG_LISTA_L, 44, TG_COR(23, 33, 43));
    tgEscrever(p, "Emoji: Twemoji, CC-BY 4.0", 0, 1 << 30, 17, 28, base + 13, TG_COR(90, 104, 118));
    tgRetangulo(p, TG_LISTA_L, 0, 2, 1080, TG_COR(16, 24, 32));
}

static bool tgSoEmoji(const char* t) {
    int total = 0, i = 0;
    while (t[i]) {
        int bytes = 0;
        if (tgEmojiEm(t + i, &bytes) < 0) return false;
        i += bytes;
        total++;
    }
    return total > 0 && total <= 3;
}

// Palavra que é link sai azul; o resto da linha na cor normal.
static void tgEscreverComLinks(uint32_t* p, const char* t, int inicio, int fim, int tam, int x, int y, uint32_t cor) {
    int i = inicio;
    while (i < fim) {
        int j = i;
        while (j < fim && t[j] != ' ') j++;
        x += tgEscrever(p, t, i, j, tam, x, y, tgELink(t + i) ? TG_COR(100, 180, 240) : cor);
        if (j < fim) { x += tgEscrever(p, t, j, j + 1, tam, x, y, cor); j++; }
        i = j;
    }
}

// Altura de uma mensagem na largura dada. Com p, também desenha com a base do balão em y + altura.
static int tgBalao(uint32_t* p, TgMensagem* m, int y, bool mostrarNome, int topo, int base) {
    bool grande = tgSoEmoji(m->texto);
    const int tam = grande ? 54 : 27, linha = grande ? 68 : 37, margem = 16;
    int areaX = TG_LISTA_L + 2, areaL = 1920 - areaX;
    int maxTexto = TG_BALAO_MAX - margem * 2;
    int fotoL = 0, fotoH = 0;
    bool semBalao = m->figurinha;
    if (m->arquivo) {
        int w = m->imgL > 0 ? m->imgL : 320, h = m->imgA > 0 ? m->imgA : 240;
        int limite = semBalao ? 260 : TG_FOTO_MAX;
        fotoL = w; fotoH = h;
        if (fotoL > limite) { fotoH = fotoH * limite / fotoL; fotoL = limite; }
        if (fotoH > (semBalao ? 260 : 500)) { fotoL = fotoL * (semBalao ? 260 : 500) / fotoH; fotoH = semBalao ? 260 : 500; }
        if (fotoL < 120 && !semBalao) fotoL = 120;
    }
    char hora[8];
    tgHora(m->data, hora, sizeof(hora));
    int larguraHora = tgLargura(hora, 18);
    const char* nome = mostrarNome ? tgNomeAutor(m) : NULL;

    int largura = 0, linhas = 0, i = 0;
    bool temTexto = m->texto[0] != '\0';
    while (temTexto && m->texto[i]) {
        int proximo;
        int fim = tgQuebrar(m->texto, i, tam, maxTexto, &proximo);
        int w = tgEscrever(NULL, m->texto, i, fim, tam, 0, 0, 0);
        if (w > largura) largura = w;
        linhas++;
        i = proximo;
    }
    int interno = largura;
    if (temTexto && interno < larguraHora + 10) interno = larguraHora + 10;
    if (nome) { int w = tgLargura(nome, 22); if (w > interno) interno = w; }
    int total = margem * 2 + interno;
    if (fotoL && !semBalao && fotoL > total) total = fotoL;
    if (fotoL && !semBalao) { if (temTexto) interno = total - margem * 2; }
    if (!temTexto && !fotoL) total = margem * 2 + larguraHora;
    int altura = 0;
    if (nome) altura += 32;
    if (fotoL) altura += fotoH + (temTexto ? 8 : 0);
    if (temTexto) altura += linhas * linha + 24;
    if (!nome && !fotoL) altura += 10;
    if (temTexto) altura += 6;
    if (semBalao) { altura = fotoH + 26; total = fotoL; }
    if (!p) return altura;

    int x = m->minha ? 1920 - 66 - total : areaX + 28;
    (void)areaL;
    uint32_t fundo = m->minha ? TG_COR(43, 82, 120) : TG_COR(24, 37, 51);
    if (!semBalao) tgArredondado(p, x, y, total, altura, 16, fundo, topo, base);
    int cy = y + (nome || !fotoL ? 8 : 0);
    if (nome) { if (cy >= topo - 40 && cy < base) tgEscreverCortado(p, nome, 22, x + margem, cy, total - margem * 2, tgCorDoId(m->autor)); cy += 30; }
    if (fotoL) {
        TgArquivo* a = tgImagem(m->arquivo);
        if (a) tgDesenharPixels(p, a, x + (total - fotoL) / 2, cy, fotoL, fotoH, semBalao ? 0 : 14, topo, base);
        else if (!semBalao) tgArredondado(p, x, cy, total, fotoH, 14, TG_COR(32, 47, 63), topo, base);
        else tgArredondado(p, x, cy, fotoL, fotoH, 14, TG_COR(24, 37, 51), topo, base);
        if (m->rotulo[0]) {
            int w = tgLargura(m->rotulo, 20) + 22;
            tgArredondado(p, x + 12, cy + 12, w, 32, 12, TG_COR(16, 24, 32), topo, base);
            if (cy + 12 >= topo && cy + 44 < base) tgEscrever(p, m->rotulo, 0, 1 << 30, 20, x + 23, cy + 17, TG_COR(255, 255, 255));
        }
        cy += fotoH + (temTexto ? 8 : 0);
    }
    i = 0;
    while (temTexto && m->texto[i]) {
        int proximo;
        int fim = tgQuebrar(m->texto, i, tam, maxTexto, &proximo);
        if (cy + linha > topo && cy < base - linha) tgEscreverComLinks(p, m->texto, i, fim, tam, x + margem, cy, TG_COR(245, 245, 245));
        cy += linha;
        i = proximo;
    }
    int yHora = y + altura - 28;
    if (yHora >= topo && yHora + 24 < base) {
        if (semBalao || (fotoL && !temTexto)) {
            int w = larguraHora + 16;
            tgArredondado(p, x + total - w - 8, yHora - 4, w, 26, 10, TG_COR(16, 24, 32), topo, base);
        }
        tgEscrever(p, hora, 0, 8, 18, x + total - larguraHora - margem, yHora, m->minha ? TG_COR(125, 170, 210) : TG_COR(125, 139, 153));
    }
    return altura;
}

static float tgPosicaoNaBarra(const TgConversa* c, long long data) {
    if (!tgPrimeiraData || c->data <= tgPrimeiraData) return 1.0f;
    float f = (float)(data - tgPrimeiraData) / (float)(c->data - tgPrimeiraData);
    return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
}

static long long tgDataDaBarra(const TgConversa* c, float posicao) {
    return tgPrimeiraData + (long long)((double)(c->data - tgPrimeiraData) * posicao);
}

static void tgDataEmTexto(long long data, char* saida, int tamanho) {
    long long dias = (data + tgFusoMinutos * 60LL) / 86400 + 719468;
    long long era = dias / 146097, resto = dias - era * 146097;
    long long ano = (resto - resto / 1460 + resto / 36524 - resto / 146096) / 365;
    long long diaAno = resto - (365 * ano + ano / 4 - ano / 100);
    long long mp = (5 * diaAno + 2) / 153;
    int dia = (int)(diaAno - (153 * mp + 2) / 5 + 1), mes = (int)(mp < 10 ? mp + 3 : mp - 9);
    snprintf(saida, tamanho, "%02d/%02d/%04d", dia, mes, (int)(ano + era * 400 + (mes <= 2)));
}

// Setas e barra à direita: L1 vai à primeira mensagem, R1 à última, analógico direito arrasta.
static void tgDesenharBarra(uint32_t* p, const TgConversa* c, long long dataSelecionada, int topo, int base) {
    int x = 1920 - 44;
    int emojiCima = tgEmojis ? tgBuscarEmoji("2b06") : -1, emojiBaixo = tgEmojis ? tgBuscarEmoji("2b07") : -1;
    if (emojiCima >= 0) tgDesenharEmoji(p, emojiCima, x, topo + 10, 30);
    if (emojiBaixo >= 0) tgDesenharEmoji(p, emojiBaixo, x, base - 66, 30);
    tgEscrever(p, "L1", 0, 2, 17, x + 5, topo + 44, TG_COR(125, 139, 153));
    tgEscrever(p, "R1", 0, 2, 17, x + 5, base - 90, TG_COR(125, 139, 153));
    int y0 = topo + 72, y1 = base - 100, altura = y1 - y0;
    tgArredondado(p, x + 11, y0, 8, altura, 4, TG_COR(36, 47, 61), 0, 1080);
    float posicao = tgBarraAtiva ? tgBarraPos : tgPosicaoNaBarra(c, dataSelecionada);
    int y = y0 + (int)(posicao * (altura - 44));
    tgArredondado(p, x + 7, y, 16, 44, 8, tgBarraAtiva ? TG_COR(100, 180, 240) : TG_COR(90, 110, 130), 0, 1080);
    if (tgBarraAtiva) {
        char texto[16];
        tgDataEmTexto(tgDataDaBarra(c, tgBarraPos), texto, sizeof(texto));
        int w = tgLargura(texto, 24) + 28;
        tgArredondado(p, x - w - 12, y + 2, w, 40, 14, TG_COR(46, 112, 165), 0, 1080);
        tgEscrever(p, texto, 0, 16, 24, x - w + 2, y + 9, TG_COR(255, 255, 255));
    }
}

static void tgDesenharConversa(uint32_t* p) {
    static int alturas[TG_MAX_MENSAGENS];
    static bool nomes[TG_MAX_MENSAGENS];
    int x = TG_LISTA_L + 2, l = 1920 - x;
    int topo = TG_TOPO_H, base = 1080 - TG_BASE_H;
    tgRetangulo(p, x, 0, l, 1080, TG_COR(14, 22, 33));
    const TgConversa* c = tgConversaPorId(tgAberta);
    if (tgTela != TG_MENSAGENS || !c) {
        const char* dica = tgEmBusca ? "Escolha um resultado para abrir" : "Selecione uma conversa para abrir";
        int w = tgLargura(dica, 24) + 44;
        tgArredondado(p, x + (l - w) / 2, 510, w, 50, 25, TG_COR(30, 44, 58), 0, 1080);
        tgEscrever(p, dica, 0, 1 << 30, 24, x + (l - w) / 2 + 22, 521, TG_COR(225, 232, 240));
        tgEscrever(p, tgEmBusca ? "X  Abrir     Triângulo  Nova pesquisa     O  Fechar" : "X  Abrir     Triângulo  Pesquisar     L1 / R1  Pastas     Options  Downloads     O  Sair", 0, 1 << 30, 22, x + 40, 1034, TG_COR(125, 139, 153));
        return;
    }
    // Mede tudo primeiro: a rolagem segue a mensagem selecionada.
    bool grupo = c->id < 0;
    int visivel = base - topo - 28, selecionada = tgTotalMensagens - 1, acumulado = 0;
    if (tgSelId) for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgSelId) selecionada = i;
    for (int i = tgTotalMensagens - 1; i >= 0; i--) {
        TgMensagem* m = &tgMensagens[i];
        nomes[i] = grupo && !m->minha && m->autor != c->id && (i == 0 || tgMensagens[i - 1].autor != m->autor) && !m->figurinha;
        if (!m->alturaCache[nomes[i]]) m->alturaCache[nomes[i]] = tgBalao(NULL, m, 0, nomes[i], topo, base);
        alturas[i] = m->alturaCache[nomes[i]];
        if (i == selecionada) {
            if (acumulado < tgRolagem) tgRolagem = acumulado;
            if (acumulado + alturas[i] > tgRolagem + visivel) tgRolagem = acumulado + alturas[i] - visivel;
        }
        acumulado += alturas[i] + 8;
    }
    if (tgSelId == 0) tgRolagem = 0;
    int y = base - 14 + tgRolagem;
    for (int i = tgTotalMensagens - 1; i >= 0; i--) {
        y -= alturas[i];
        if (y < base && y + alturas[i] > topo) {
            if (i == selecionada) {
                int y0 = y - 4 < topo ? topo : y - 4, y1 = y + alturas[i] + 4 > base ? base : y + alturas[i] + 4;
                tgRetangulo(p, x, y0, l, y1 - y0, TG_COR(26, 40, 56));
            }
            tgBalao(p, &tgMensagens[i], y, nomes[i], topo, base);
        }
        y -= 8;
    }
    if (tgTotalMensagens == 0) tgEscrever(p, "Carregando mensagens...", 0, 1 << 30, 26, x + 40, 500, TG_COR(125, 139, 153));
    tgDesenharBarra(p, c, tgTotalMensagens > 0 ? tgMensagens[selecionada].data : c->data, topo, base);

    tgRetangulo(p, x, 0, l, TG_TOPO_H, TG_COR(23, 33, 43));
    tgRetangulo(p, x, TG_TOPO_H - 2, l, 2, TG_COR(16, 24, 32));
    tgAvatar(p, c->id, c->foto, c->titulo, x + 22, 14, 56, 0, 1080);
    tgEscreverCortado(p, c->titulo, 28, x + 94, 25, l - 130, TG_COR(255, 255, 255));

    const TgMensagem* m = tgTotalMensagens > 0 ? &tgMensagens[selecionada] : NULL;
    const char* acao = "X  Escrever";
    if (m && tgAudioRodando && tgAudioMensagem == m->id) acao = "X  Parar";
    else if (m && m->midia == TG_M_AUDIO) acao = "X  Tocar";
    else if (m && m->midia == TG_M_VIDEO) acao = "X  Assistir";
    else if (m && m->midia) acao = "X  Abrir";
    else if (m && m->totalLinks > 0) acao = "X  Abrir link";
    else if (m && m->grande) acao = "X  Baixar";
    char dica[140];
    snprintf(dica, sizeof(dica), "%s     Segure X  Opções     Quadrado  Escrever     Triângulo  Pesquisar     O  Voltar", acao);
    if (tgEmBusca) snprintf(dica, sizeof(dica), "X  Abrir resultado     Triângulo  Nova pesquisa     O  Fechar");
    tgRetangulo(p, x, base, l, TG_BASE_H, TG_COR(23, 33, 43));
    tgArredondado(p, x + 24, base + 14, l - 48, 48, 24, TG_COR(36, 47, 61), 0, 1080);
    tgEscrever(p, dica, 0, 1 << 30, 22, x + 50, base + 26, TG_COR(150, 165, 180));
}

static void tgDesenharMenu(uint32_t* p) {
    const TgMensagem* m = NULL;
    for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgMenuMensagem) m = &tgMensagens[i];
    if (!m) return;
    int largura = 900, altura = 70 + tgMenuTotal * 58 + 16;
    int x = TG_LISTA_L + 2 + (1920 - TG_LISTA_L - largura) / 2, y = (1080 - altura) / 2;
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(30, 44, 58), 0, 1080);
    tgEscrever(p, "Opções", 0, 1 << 30, 28, x + 28, y + 18, TG_COR(255, 255, 255));
    for (int i = 0; i < tgMenuTotal; i++) {
        int linha = y + 70 + i * 58;
        if (i == tgMenuSel) tgArredondado(p, x + 12, linha, largura - 24, 54, 12, TG_COR(46, 112, 165), 0, 1080);
        tgEscreverCortado(p, tgMenuLinks[i] == -2 ? "↩️ Responder" : tgMenuLinks[i] < 0 ? "⬇️ Baixar e salvar" : m->links[tgMenuLinks[i]], 24, x + 30, linha + 12, largura - 60, TG_COR(245, 245, 245));
    }
}

static void tgDesenharFila(uint32_t* p) {
    int largura = 1040, linhas = tgTotalDownloads > 0 ? (tgTotalDownloads < 9 ? tgTotalDownloads : 9) : 1;
    int altura = 76 + linhas * 84 + 56;
    int x = (1920 - largura) / 2, y = (1080 - altura) / 2;
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(30, 44, 58), 0, 1080);
    tgEscrever(p, "Downloads", 0, 1 << 30, 30, x + 28, y + 20, TG_COR(255, 255, 255));
    if (tgTotalDownloads == 0) tgEscrever(p, "Nenhum download", 0, 1 << 30, 24, x + 28, y + 96, TG_COR(150, 165, 180));
    int topo = tgFilaSel >= linhas ? tgFilaSel - linhas + 1 : 0;
    for (int i = 0; i < linhas && topo + i < tgTotalDownloads; i++) {
        const TgDownload* d = &tgDownloads[topo + i];
        int yl = y + 76 + i * 84;
        if (topo + i == tgFilaSel) tgArredondado(p, x + 12, yl, largura - 24, 78, 12, TG_COR(43, 82, 120), 0, 1080);
        char estado[48];
        int porcento = d->total > 0 ? (int)(d->baixado * 100 / d->total) : 0;
        if (d->estado == TG_D_BAIXANDO) snprintf(estado, sizeof(estado), "%d%%", porcento);
        else if (d->estado == TG_D_COPIAR) strcpy(estado, "Na fila");
        else if (d->estado == TG_D_COPIANDO) strcpy(estado, "Salvando...");
        else if (d->estado == TG_D_ERRO) strcpy(estado, "Erro");
        else strcpy(estado, d->pkg ? "Instalando" : "Concluído");
        int larguraEstado = tgLargura(estado, 22);
        tgEscreverCortado(p, d->nome, 24, x + 30, yl + 10, largura - 90 - larguraEstado, TG_COR(245, 245, 245));
        tgEscrever(p, estado, 0, 1 << 30, 22, x + largura - 30 - larguraEstado, yl + 12, TG_COR(190, 205, 220));
        tgArredondado(p, x + 30, yl + 52, largura - 60, 10, 5, TG_COR(23, 33, 43), 0, 1080);
        int cheio = d->estado == TG_D_BAIXANDO ? (largura - 60) * porcento / 100 : largura - 60;
        if (cheio >= 10) tgArredondado(p, x + 30, yl + 52, cheio, 10, 5, d->estado == TG_D_ERRO ? TG_COR(200, 80, 70) : TG_COR(100, 180, 240), 0, 1080);
    }
    const char* dica = "O  Fechar";
    if (tgTotalDownloads > 0) dica = tgDownloads[tgFilaSel].estado == TG_D_BAIXANDO ? "X  Cancelar     O  Fechar" : tgDownloads[tgFilaSel].estado >= TG_D_PRONTO ? "X  Tirar da lista     O  Fechar" : "O  Fechar";
    tgEscrever(p, dica, 0, 1 << 30, 22, x + 28, y + altura - 42, TG_COR(150, 165, 180));
}

static void tgDesenharVisor(uint32_t* p) {
    tgRetangulo(p, 0, 0, 1920, 1080, TG_COR(0, 0, 0));
    TgArquivo* a = tgArquivo(tgVisor);
    if (a) a->uso = ++tgRelogioUso;
    TgArquivo* mostrar = a && a->pixels ? a : tgImagem(tgVisorMini);
    if (mostrar) {
        float escala = 1920.0f / mostrar->largura;
        if (1080.0f / mostrar->altura < escala) escala = 1080.0f / mostrar->altura;
        escala *= tgVisorZoom;
        int w = (int)(mostrar->largura * escala), h = (int)(mostrar->altura * escala);
        int folgaX = w > 1920 ? (w - 1920) / 2 : 0, folgaY = h > 1080 ? (h - 1080) / 2 : 0;
        if (tgVisorX > folgaX) tgVisorX = folgaX;
        if (tgVisorX < -folgaX) tgVisorX = -folgaX;
        if (tgVisorY > folgaY) tgVisorY = folgaY;
        if (tgVisorY < -folgaY) tgVisorY = -folgaY;
        int ox = (1920 - w) / 2 + tgVisorX, oy = (1080 - h) / 2 + tgVisorY;
        int x0 = ox < 0 ? 0 : ox, x1 = ox + w > 1920 ? 1920 : ox + w, y0 = oy < 0 ? 0 : oy, y1 = oy + h > 1080 ? 1080 : oy + h;
        for (int y = y0; y < y1; y++) {
            int sy = (int)((y - oy) / escala);
            if (sy >= mostrar->altura) sy = mostrar->altura - 1;
            for (int x = x0; x < x1; x++) {
                int sx = (int)((x - ox) / escala);
                if (sx >= mostrar->largura) sx = mostrar->largura - 1;
                uint32_t c = mostrar->pixels[sy * mostrar->largura + sx];
                p[y * 1920 + x] = tgMisturar(TG_COR(0, 0, 0), c & 255, (c >> 8) & 255, (c >> 16) & 255, c >> 24);
            }
        }
    }
    const char* dica = a && a->falhou ? "Não foi possível abrir esta imagem     O  Voltar" : a && a->pixels ? "Direcional  Zoom     Analógico  Mover     O  Voltar" : "Carregando imagem...     O  Voltar";
    int w = tgLargura(dica, 22) + 40;
    tgArredondado(p, (1920 - w) / 2, 1020, w, 42, 20, TG_COR(30, 36, 44), 0, 1080);
    tgEscrever(p, dica, 0, 1 << 30, 22, (1920 - w) / 2 + 20, 1029, TG_COR(235, 240, 245));
}

static void tgDesenharEntrada(uint32_t* p) {
    tgRetangulo(p, 0, 0, 1920, 1080, TG_COR(23, 33, 43));
    tgArredondado(p, 880, 200, 160, 160, 80, TG_COR(64, 130, 188), 0, 1080);
    int e = tgEmojis ? tgBuscarEmoji("2708") : -1;
    if (e >= 0) tgDesenharEmoji(p, e, 918, 238, 84);
    const char* titulo = "Telegram";
    tgEscrever(p, titulo, 0, 8, 54, (1920 - tgLargura(titulo, 54)) / 2, 400, TG_COR(255, 255, 255));
    const char* linha = "Conectando...";
    const char* rodape = "O  Sair";
    if (tgTela == TG_TELEFONE) { linha = "Aperte X e digite seu telefone com DDI, como +5511999998888"; rodape = "X  Digitar     O  Sair"; }
    else if (tgTela == TG_CODIGO) { linha = "Aperte X e digite o código recebido no Telegram ou por SMS"; rodape = "X  Digitar     O  Sair"; }
    else if (tgTela == TG_SENHA) { linha = "Aperte X e digite sua senha de duas etapas"; rodape = "X  Digitar     O  Sair"; }
    else if (tgTela == TG_SEM_SUPORTE) linha = "Esta conta precisa de um passo que o app ainda não faz. Use outro aparelho.";
    tgEscrever(p, linha, 0, 1 << 30, 30, (1920 - tgLargura(linha, 30)) / 2, 490, TG_COR(190, 200, 212));
    if (tgAviso[0]) tgEscrever(p, tgAviso, 0, 1 << 30, 26, (1920 - tgLargura(tgAviso, 26)) / 2, 560, TG_COR(255, 170, 110));
    tgEscrever(p, rodape, 0, 1 << 30, 24, (1920 - tgLargura(rodape, 24)) / 2, 1020, TG_COR(125, 139, 153));
}

static void tgDesenhar(uint32_t* p) {
    if (tgTela != TG_CONVERSAS && tgTela != TG_MENSAGENS) { tgDesenharEntrada(p); return; }
    if (tgVisor) { tgDesenharVisor(p); return; }
    tgDesenharConversa(p);
    tgDesenharLista(p);
    if (tgMenuAberto) tgDesenharMenu(p);
    if (tgFilaAberta) tgDesenharFila(p);
    if (tgAudioRodando) {
        const char* texto = "🔊 Tocando áudio";
        int w = tgLargura(texto, 22) + 36;
        tgArredondado(p, 1920 - 28 - w, 100, w, 42, 18, TG_COR(43, 82, 120), 0, 1080);
        tgEscrever(p, texto, 0, 1 << 30, 22, 1920 - 28 - w + 18, 109, TG_COR(255, 255, 255));
    }
    if (tgAviso[0]) {
        int w = tgLargura(tgAviso, 22) + 36;
        tgArredondado(p, (1920 + TG_LISTA_L - w) / 2, 100, w, 42, 18, TG_COR(120, 50, 45), 0, 1080);
        tgEscrever(p, tgAviso, 0, 1 << 30, 22, (1920 + TG_LISTA_L - w) / 2 + 18, 109, TG_COR(255, 255, 255));
    }
}
// TG_VISUAL_FIM
