#include "radio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <orbis/Http.h>
#include <orbis/libkernel.h>
#include "audio.h" 
#include "menu_grafico.h"
#include "network.h"

extern char linksAtuais[3000][1024];
extern char nomes[3000][256];
extern int totalItens;
extern int sel;
extern int off;
extern MenuLevel menuAtual;
extern bool showOpcoes;
extern int selOpcao;
extern int mapOpcoes[150];
extern const char* listaOpcoes[150];
extern int totalOpcoes;
extern char msgStatus[128];
extern int msgTimer;
void executarAcaoMenuRadio(int id);

char radioBrowserQuery[128] = "";
char radioFavPath[] = "/data/HyperNeiva/configuracao/radio_favoritos.json";

char debugHttpInfo[256] = "";

// ---- Lista própria das rádios (o 'nomes' do menu muda quando se sai da tela) ----
struct RadioEst { char nome[128]; char url[512]; char icone[512]; };
static RadioEst radioLista[100];
static int radioTotal = 0, radioAtual = -1;
static RadioEst radioUltima;            // a que está tocando (para o player)
static volatile unsigned radioVersao = 1;
static volatile int radioCapasGeracao = 0;
#define RADIO_FAV_TXT "/data/HyperNeiva/configuracao/radio_favoritos.txt"
#define RADIO_CAPAS "/data/HyperNeiva/cache/radio"

unsigned radioVersaoCapas() { return radioVersao; }

static const char* radioSemEsquema(const char* u) {
    if (!strncmp(u, "https://", 8)) return u + 8;
    if (!strncmp(u, "http://", 7)) return u + 7;
    return u;
}
static void radioArquivoCapa(const char* icone, char* saida, int tam) {
    unsigned h = 2166136261u;
    for (const char* c = icone; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
    snprintf(saida, tam, RADIO_CAPAS "/%08x.img", h);
}
static bool radioCapaDe(const RadioEst* e, char* saida, int tam) {
    if (!e->icone[0]) return false;
    radioArquivoCapa(e->icone, saida, tam);
    struct stat st;
    return stat(saida, &st) == 0 && st.st_size > 0;
}
// Capa de uma rádio da lista do menu (tema da lista).
bool radioCapaCaminho(int indice, char* saida, int tam) {
    if (indice < 0 || indice >= radioTotal) return false;
    return radioCapaDe(&radioLista[indice], saida, tam);
}

int baixarArquivoRadio(const char* url, const char* localPath);
static void* radioThreadCapas(void* arg) {
    int geracao = (int)(intptr_t)arg;
    mkdir("/data/HyperNeiva/cache", 0777);
    mkdir(RADIO_CAPAS, 0777);
    for (int i = 0; i < radioTotal && geracao == radioCapasGeracao; i++) {
        char icone[512]; snprintf(icone, sizeof(icone), "%s", radioLista[i].icone);
        if (!icone[0]) continue;
        char destino[256]; radioArquivoCapa(icone, destino, sizeof(destino));
        struct stat st;
        if (stat(destino, &st) == 0) continue;            // já baixada (ou já falhou: arquivo vazio)
        char tmp[300]; snprintf(tmp, sizeof(tmp), "%s.tmp", destino);
        if (baixarArquivoRadio(icone, tmp) == 0) { remove(destino); rename(tmp, destino); radioVersao++; }
        else { remove(tmp); FILE* f = fopen(destino, "wb"); if (f) fclose(f); }
    }
    return NULL;
}
static void radioListaPronta() {
    radioVersao++;
    if (radioAtual >= radioTotal) radioAtual = -1;
    pthread_t t;
    int g = ++radioCapasGeracao;
    if (pthread_create(&t, NULL, radioThreadCapas, (void*)(intptr_t)g) == 0) pthread_detach(t);
}

// Favoritos: "nome<TAB>url<TAB>icone" por linha.
static bool radioEhFavoritoUrl(const char* url) {
    FILE* f = fopen(RADIO_FAV_TXT, "r");
    if (!f) return false;
    char l[1300]; bool achou = false;
    while (!achou && fgets(l, sizeof(l), f)) {
        char* a = strchr(l, '\t'); if (!a) continue;
        char* b = strchr(a + 1, '\t'); if (b) *b = 0;
        a[1 + strcspn(a + 1, "\r\n")] = 0;
        achou = !strcmp(radioSemEsquema(a + 1), radioSemEsquema(url));
    }
    fclose(f);
    return achou;
}
// Liga ou desliga o favorito. Devolve true se ficou favorito.
static bool radioFavoritoAlternar(const RadioEst* e) {
    if (!e->url[0]) return false;
    bool era = radioEhFavoritoUrl(e->url);
    char* resto = NULL; size_t n = 0;
    FILE* f = fopen(RADIO_FAV_TXT, "r");
    if (f) {
        resto = (char*)calloc(1, 200000);
        char l[1300];
        while (resto && fgets(l, sizeof(l), f)) {
            char c[1300]; snprintf(c, sizeof(c), "%s", l);
            char* a = strchr(c, '\t'); if (!a) continue;
            char* b = strchr(a + 1, '\t'); if (b) *b = 0;
            a[1 + strcspn(a + 1, "\r\n")] = 0;
            if (era && !strcmp(radioSemEsquema(a + 1), radioSemEsquema(e->url))) continue;
            size_t k = strlen(l);
            if (n + k + 2 < 200000) { memcpy(resto + n, l, k); n += k; if (l[k - 1] != '\n') resto[n++] = '\n'; }
        }
        fclose(f);
    }
    mkdir("/data/HyperNeiva/configuracao", 0777);
    f = fopen(RADIO_FAV_TXT, "w");
    if (f) {
        if (resto) fwrite(resto, 1, n, f);
        if (!era) fprintf(f, "%s\t%s\t%s\n", e->nome, e->url, e->icone);
        fclose(f);
    }
    free(resto);
    return !era;
}
static void radioCarregarFavoritos() {
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    radioTotal = 0;
    FILE* f = fopen(RADIO_FAV_TXT, "r");
    if (f) {
        char l[1300];
        while (radioTotal < 100 && fgets(l, sizeof(l), f)) {
            l[strcspn(l, "\r\n")] = 0;
            char* a = strchr(l, '\t'); if (!a) continue; *a = 0;
            char* b = strchr(a + 1, '\t'); if (b) *b = 0;
            RadioEst* e = &radioLista[radioTotal];
            snprintf(e->nome, sizeof(e->nome), "%s", l);
            snprintf(e->url, sizeof(e->url), "%s", a + 1);
            snprintf(e->icone, sizeof(e->icone), "%s", b ? b + 1 : "");
            snprintf(nomes[radioTotal], 256, "%s", e->nome);
            snprintf(linksAtuais[radioTotal], 1024, "%s", e->url);
            radioTotal++;
        }
        fclose(f);
    }
    totalItens = radioTotal;
    if (!totalItens) { strcpy(nomes[0], "Nenhum favorito: Triangulo numa radio > tres pontos > Favoritar"); totalItens = 1; }
    sel = 0; off = 0;
    radioListaPronta();
}

// =========================================================================
// PODCASTS: Top 50 do Brasil e busca (API pública da Apple, sem chave) e os
// episódios lidos do feed RSS de cada podcast.
// =========================================================================
static int radioModo = 0;              // 0 rádios, 1 lista de podcasts, 2 episódios
static RadioEst podShows[100];
static int podTotal = 0, podSel = 0;
static bool radioUltimaPodcast = false;
bool radioPesquisaPodcast = false;     // o teclado 10 pesquisa podcast em vez de rádio
bool radioEhPodcast() { return radioUltimaPodcast; }

static void radioListaParaMenu(const char* vazio) {
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    for (int i = 0; i < radioTotal; i++) {
        snprintf(nomes[i], 256, "%s", radioLista[i].nome);
        snprintf(linksAtuais[i], 1024, "%s", radioLista[i].url);
    }
    totalItens = radioTotal;
    if (!totalItens) { snprintf(nomes[0], 256, "%s", vazio); totalItens = 1; }
}
static char* radioLerArquivo(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024 * 1024) { fclose(f); return NULL; }
    char* b = (char*)malloc(n + 1);
    if (b) { b[fread(b, 1, n, f)] = 0; }
    fclose(f);
    return b;
}
static void radioCodificarUrl(const char* in, char* out, int tam) {
    static const char* hex = "0123456789ABCDEF";
    int k = 0;
    for (const unsigned char* c = (const unsigned char*)in; *c && k < tam - 4; c++) {
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_' || *c == '.') out[k++] = *c;
        else if (*c == ' ') out[k++] = '+';
        else { out[k++] = '%'; out[k++] = hex[*c >> 4]; out[k++] = hex[*c & 15]; }
    }
    out[k] = 0;
}
// "chave":"valor" do JSON (só texto), entre ini e fim, com \uXXXX e \/ resolvidos.
static bool podJson(const char* ini, const char* fim, const char* chave, char* out, int tam) {
    out[0] = 0;
    char k[64]; snprintf(k, sizeof(k), "\"%s\":\"", chave);
    const char* p = strstr(ini, k);
    if (!p || (fim && p >= fim)) return false;
    p += strlen(k);
    int n = 0;
    while (*p && *p != '"' && n < tam - 4) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'u') {
                char hx[5] = { p[1], p[2], p[3], p[4], 0 }; unsigned v = (unsigned)strtol(hx, NULL, 16);
                p += 5;
                if (v < 0x80) out[n++] = (char)v;
                else if (v < 0x800) { out[n++] = (char)(0xC0 | (v >> 6)); out[n++] = (char)(0x80 | (v & 63)); }
                else { out[n++] = (char)(0xE0 | (v >> 12)); out[n++] = (char)(0x80 | ((v >> 6) & 63)); out[n++] = (char)(0x80 | (v & 63)); }
                continue;
            }
            out[n++] = *p == 'n' ? ' ' : *p == 't' ? ' ' : *p;
            p++;
            continue;
        }
        out[n++] = *p++;
    }
    out[n] = 0;
    return true;
}
// Resultados do iTunes (lookup ou search): nome, feed e capa de cada podcast.
static void podLerItunes(const char* j) {
    podTotal = 0;
    const char* p = strstr(j, "\"results\"");
    while (p && podTotal < 100) {
        const char* ini = strstr(p, "\"wrapperType\"");
        if (!ini) break;
        const char* fim = strstr(ini + 13, "\"wrapperType\"");
        RadioEst* e = &podShows[podTotal];
        podJson(ini, fim, "collectionName", e->nome, sizeof(e->nome));
        podJson(ini, fim, "feedUrl", e->url, sizeof(e->url));
        if (!podJson(ini, fim, "artworkUrl600", e->icone, sizeof(e->icone))) podJson(ini, fim, "artworkUrl100", e->icone, sizeof(e->icone));
        if (e->nome[0] && e->url[0]) podTotal++;
        p = fim;
    }
}
static void podMostrarShows() {
    radioModo = 1;
    radioTotal = podTotal;
    memcpy(radioLista, podShows, sizeof(RadioEst) * podTotal);
    radioListaParaMenu("Nenhum podcast encontrado.");
    radioListaPronta();
}
static void* podThreadTop(void* arg) {
    char* termo = (char*)arg;
    const char* arq = "/data/HyperNeiva/podcast_api.json";
    bool ok = false;
    if (termo) {
        char q[300]; radioCodificarUrl(termo, q, sizeof(q));
        char url[512]; snprintf(url, sizeof(url), "https://itunes.apple.com/search?media=podcast&entity=podcast&country=BR&limit=50&term=%s", q);
        if (baixarArquivoRadio(url, arq) == 0) { char* j = radioLerArquivo(arq); if (j) { podLerItunes(j); free(j); ok = true; } }
        free(termo);
    }
    else {
        // Ranking da Apple Podcasts no Brasil e, com os ids, os feeds.
        snprintf(nomes[0], 256, "Buscando o Top 50 do Brasil...");
        if (baixarArquivoRadio("https://rss.marketingtools.apple.com/api/v2/br/podcasts/top/50/podcasts.json", arq) == 0) {
            char* j = radioLerArquivo(arq);
            char ids[1200] = "";
            for (const char* p = j ? strstr(j, "\"results\"") : NULL; p; ) {
                p = strstr(p, "\"id\":\"");
                if (!p) break;
                p += 6;
                int n = 0; while (p[n] >= '0' && p[n] <= '9') n++;
                if (n > 0 && p[n] == '"' && strlen(ids) + n + 2 < sizeof(ids)) { if (ids[0]) strcat(ids, ","); strncat(ids, p, n); }
            }
            free(j);
            if (ids[0]) {
                snprintf(nomes[0], 256, "Lendo os podcasts...");
                char url[1400]; snprintf(url, sizeof(url), "https://itunes.apple.com/lookup?country=BR&entity=podcast&id=%s", ids);
                if (baixarArquivoRadio(url, arq) == 0) { char* l = radioLerArquivo(arq); if (l) { podLerItunes(l); free(l); ok = true; } }
            }
        }
    }
    if (!ok) { radioTotal = 0; podTotal = 0; radioModo = 1; radioListaParaMenu("Erro de rede: nao consegui buscar os podcasts."); return NULL; }
    podMostrarShows();
    return NULL;
}
static void podDecodificar(char* s) {
    char* o = s;
    for (char* p = s; *p; ) {
        if (*p == '&') {
            struct { const char* e; char c; } t[] = { { "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' }, { "&quot;", '"' }, { "&apos;", '\'' }, { "&#39;", '\'' } };
            bool feito = false;
            for (int i = 0; i < 6 && !feito; i++) { size_t n = strlen(t[i].e); if (!strncmp(p, t[i].e, n)) { *o++ = t[i].c; p += n; feito = true; } }
            if (feito) continue;
        }
        *o++ = *p++;
    }
    *o = 0;
}
static void* podThreadFeed(void* arg) {
    (void)arg;
    RadioEst show = podShows[podSel];
    const char* arq = "/data/HyperNeiva/podcast_feed.xml";
    char* x = baixarArquivoRadio(show.url, arq) == 0 ? radioLerArquivo(arq) : NULL;
    int total = 0;
    for (const char* it = x ? strstr(x, "<item") : NULL; it && total < 100; ) {
        const char* fim = strstr(it + 5, "</item>");
        if (!fim) break;
        RadioEst* e = &radioLista[total];
        e->nome[0] = 0; e->url[0] = 0;
        snprintf(e->icone, sizeof(e->icone), "%s", show.icone);
        const char* t = strstr(it, "<title");
        if (t && t < fim) {
            t = strchr(t, '>');
            const char* tf = t ? strstr(t, "</title>") : NULL;
            if (t && tf && tf < fim) {
                t++;
                if (!strncmp(t, "<![CDATA[", 9)) { t += 9; const char* c = strstr(t, "]]>"); if (c && c < tf) tf = c; }
                int n = (int)(tf - t); if (n > (int)sizeof(e->nome) - 1) n = sizeof(e->nome) - 1;
                memcpy(e->nome, t, n); e->nome[n] = 0;
                podDecodificar(e->nome);
            }
        }
        const char* en = strstr(it, "<enclosure");
        if (en && en < fim) {
            const char* u = strstr(en, "url=");
            if (u && u < fim) {
                char aspas = u[4]; u += 5;
                const char* uf = strchr(u, aspas);
                int n = uf ? (int)(uf - u) : 0;
                if (n > 0 && n < (int)sizeof(e->url)) { memcpy(e->url, u, n); e->url[n] = 0; podDecodificar(e->url); }
            }
        }
        if (e->url[0]) { if (!e->nome[0]) snprintf(e->nome, sizeof(e->nome), "Episodio %d", total + 1); total++; }
        it = strstr(fim, "<item");
    }
    bool lido = x != NULL;
    free(x);
    if (radioModo != 2) return NULL;     // o usuário já voltou
    radioTotal = total;
    radioListaParaMenu(lido ? "Nenhum episodio com audio neste podcast." : "Erro de rede: nao consegui ler o feed.");
    sel = 0; off = 0;
    radioListaPronta();
    return NULL;
}
static void podIniciar(char* termo) {
    radioTotal = 0; radioModo = 1; radioVersao++;
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    strcpy(nomes[0], termo ? "Pesquisando podcasts..." : "Buscando o Top 50 do Brasil...");
    totalItens = 1; sel = 0; off = 0;
    menuAtual = MENU_RADIO_LISTA;
    pthread_t t;
    if (pthread_create(&t, NULL, podThreadTop, termo) == 0) pthread_detach(t);
    else free(termo);
}
static void podAbrirShow(int i) {
    if (i < 0 || i >= podTotal) return;
    podSel = i;
    radioModo = 2; radioTotal = 0; radioVersao++;
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    snprintf(nomes[0], 256, "Carregando episodios de %.200s...", podShows[i].nome);
    totalItens = 1; sel = 0; off = 0;
    pthread_t t;
    if (pthread_create(&t, NULL, podThreadFeed, NULL) == 0) pthread_detach(t);
}
// Bolinha nos episódios volta para a lista de podcasts (controle.cpp).
bool radioVoltar() {
    if (menuAtual != MENU_RADIO_LISTA || radioModo != 2) return false;
    podMostrarShows();
    sel = podSel < totalItens ? podSel : 0; off = 0;
    return true;
}

static void radioTocarIndice(int i) {
    if (i < 0 || i >= radioTotal || strlen(radioLista[i].url) < 6) return;
    radioAtual = i;
    radioUltima = radioLista[i];
    radioUltimaPodcast = radioModo == 2;
    // Preserve HTTPS: o cliente OpenOrbis faz o transporte TLS no PS4.
    tocarMusicaNova(radioLista[i].url);
    if ((menuAtual == MENU_RADIO_LISTA || menuAtual == MENU_RADIO_FAVORITOS) && i < totalItens) sel = i;
}

// Próxima / anterior na lista das rádios (player e menu antigo).
void radioTrocarEstacao(int passo) {
    if (radioTotal <= 0 || radioModo == 1) return;
    int i = radioAtual < 0 ? (menuAtual == MENU_RADIO_LISTA || menuAtual == MENU_RADIO_FAVORITOS ? sel : 0) : radioAtual;
    for (int k = 0; k < radioTotal; k++) {
        i = (i + passo + radioTotal) % radioTotal;
        if (strlen(radioLista[i].url) > 5) { radioTocarIndice(i); return; }
    }
}

// Para o player: nome e capa da rádio que está tocando.
bool radioInfo(const char* url, char* nome, int tamNome, char* capa, int tamCapa) {
    nome[0] = 0; capa[0] = 0;
    const RadioEst* e = NULL;
    if (radioUltima.url[0] && !strcmp(radioSemEsquema(radioUltima.url), radioSemEsquema(url))) e = &radioUltima;
    for (int i = 0; !e && i < radioTotal; i++) if (!strcmp(radioSemEsquema(radioLista[i].url), radioSemEsquema(url))) e = &radioLista[i];
    if (!e) return false;
    snprintf(nome, tamNome, "%s", e->nome);
    radioCapaDe(e, capa, tamCapa);
    return true;
}
bool radioFavoritaAtual() { return radioUltima.url[0] && radioEhFavoritoUrl(radioUltima.url); }
void radioFavoritarAtual() {
    if (!radioUltima.url[0]) { sprintf(msgStatus, "NENHUMA RADIO TOCANDO"); msgTimer = 90; return; }
    bool fav = radioFavoritoAlternar(&radioUltima);
    sprintf(msgStatus, fav ? "ADICIONADA AOS FAVORITOS" : "REMOVIDA DOS FAVORITOS"); msgTimer = 90;
}

void preencherMenuRadioCategorias() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Radios Populares (Top 50)");
    strcpy(nomes[1], "Radios de Sao Paulo (89 FM, Kiss, etc)");
    strcpy(nomes[2], "Podcasts (Tags)");
    strcpy(nomes[3], "Pesquisar (Regiao ou Estilo)");
    strcpy(nomes[4], "Meus Favoritos");
    strcpy(nomes[5], "Podcasts brasileiros (Top 50)");
    strcpy(nomes[6], "Pesquisar podcasts (Brasil)");
    totalItens = 7; sel = 0; off = 0; menuAtual = MENU_RADIO_CATEGORIA;
}

int baixarArquivoRadio(const char* url, const char* localPath) {
    extern int httpCtxId;
    int ret = 0;

    int tmpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", 1, 1);
    bool ownTmpl = true;
    if (tmpl >= 0) {
        sceHttpsSetSslCallback(tmpl, skipSslCallback, NULL);
        typedef int (*DefinirRedirecionamento)(int, int);
        ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(tmpl, 1);
    }
    if (tmpl < 0) {
        tmpl = httpCtxId;
        ownTmpl = false;
    }

    int conn = sceHttpCreateConnectionWithURL(tmpl, url, 0);
    if (conn < 0) {
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return conn;
    }

    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    if (req < 0) {
        sceHttpDeleteConnection(conn);
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return req;
    }

    sceHttpAddRequestHeader(req, "User-Agent", "HyperNeiva/1.0 (PS4)", 0);

    ret = sceHttpSendRequest(req, NULL, 0);
    if (ret < 0) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return ret;
    }

    int statusCode = 0;
    sceHttpGetStatusCode(req, &statusCode);
    if (statusCode != 200) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return -2;
    }

    FILE* f = fopen(localPath, "wb");
    if (f) {
        unsigned char buf[32768];
        int n;
        while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
            fwrite(buf, 1, n, f);
        }
        fclose(f);
        if (n < 0) {
            ret = n;
        }
        else {
            ret = 0;
        }
    }
    else {
        ret = -3;
    }

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    if (ownTmpl) sceHttpDeleteTemplate(tmpl);

    return ret;
}

void* threadBuscarRadio(void* arg) {
    char url[512];
    const char* jsonPath = "/data/HyperNeiva/radio_api.json";

    // Servidor global 'all' como prim�rio. Se falhar, usa o 'de1'.
    const char* servidores[] = {
        "all.api.radio-browser.info",
        "de1.api.radio-browser.info"
    };

    int downloadStatus = -1;

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    totalItens = 1;

    // Tenta descarregar o JSON
    for (int i = 0; i < 2; i++) {
        sprintf(nomes[0], "Conectando ao servidor (%d/2)...", i + 1);

        if (strcmp(radioBrowserQuery, "topclick") == 0) {
            sprintf(url, "http://%s/json/stations/topclick/50", servidores[i]);
        }
        else if (strcmp(radioBrowserQuery, "saopaulo") == 0) {
            sprintf(url, "http://%s/json/stations/search?state=Sao%%20Paulo&limit=50&order=clickcount&reverse=true", servidores[i]);
        }
        else if (strlen(radioBrowserQuery) > 0) {
            sprintf(url, "http://%s/json/stations/search?name=%s&limit=50", servidores[i], radioBrowserQuery);
        }
        else {
            sprintf(url, "http://%s/json/stations/topclick/50", servidores[i]);
        }

        downloadStatus = baixarArquivoRadio(url, jsonPath);

        if (downloadStatus == 0) {
            break; // Sucesso!
        }
        else {
            sceKernelUsleep(1000000);
        }
    }

    // Leitura e Preenchimento da Lista
    if (downloadStatus == 0) {
        strcpy(nomes[0], "Lendo estacoes...");

        FILE* f = fopen(jsonPath, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* buffer = (char*)malloc(fsize + 1);
            if (buffer) {
                fread(buffer, 1, fsize, f);
                buffer[fsize] = '\0';

                memset(nomes, 0, sizeof(nomes));
                memset(linksAtuais, 0, sizeof(linksAtuais));
                totalItens = 0;

                char* ptr = buffer;

                // O Parser Blindado (Imune a erros de JSON da API)
                while (totalItens < 50 && ptr != NULL) {
                    char* namePtr = strstr(ptr, "\"name\":\"");
                    if (!namePtr) break;

                    char* streamPtr = strstr(namePtr, "\"url_resolved\":\"");
                    if (!streamPtr) break;

                    namePtr += 8;
                    char* nameEnd = strchr(namePtr, '\"');
                    if (!nameEnd) break;

                    streamPtr += 16;
                    char* streamEnd = strchr(streamPtr, '\"');
                    if (!streamEnd) break;

                    int nameLen = nameEnd - namePtr;
                    if (nameLen > 255) nameLen = 255;
                    strncpy(nomes[totalItens], namePtr, nameLen);
                    nomes[totalItens][nameLen] = '\0';

                    int streamLen = streamEnd - streamPtr;
                    if (streamLen > 1023) streamLen = 1023;
                    strncpy(linksAtuais[totalItens], streamPtr, streamLen);
                    linksAtuais[totalItens][streamLen] = '\0';

                    // Ícone da estação (antes do próximo "name").
                    {
                        RadioEst* e = &radioLista[totalItens];
                        snprintf(e->nome, sizeof(e->nome), "%s", nomes[totalItens]);
                        snprintf(e->url, sizeof(e->url), "%s", linksAtuais[totalItens]);
                        e->icone[0] = 0;
                        char* proximo = strstr(streamEnd, "\"name\":\"");
                        char* fav = strstr(streamEnd, "\"favicon\":\"");
                        if (fav && (!proximo || fav < proximo)) {
                            fav += 11;
                            char* fim = strchr(fav, '\"');
                            int n = fim ? (int)(fim - fav) : 0;
                            if (n > 0 && n < (int)sizeof(e->icone)) { memcpy(e->icone, fav, n); e->icone[n] = 0; }
                        }
                    }
                    totalItens++;
                    ptr = streamEnd; // Avan�a o leitor no texto
                }
                free(buffer);
            }
            fclose(f);
        }
    }
    else {
        strcpy(nomes[0], "Erro de Rede: Nao foi possivel baixar a lista.");
        totalItens = 1;
    }

    radioTotal = (downloadStatus == 0) ? totalItens : 0;
    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhuma estacao encontrada.");
        totalItens = 1;
    }
    radioListaPronta();

    return NULL;
}

void buscarEstacoesRadio(const char* query, bool isPodcast) {
    if (radioPesquisaPodcast) { radioPesquisaPodcast = false; podIniciar(strdup(query)); return; }
    radioModo = 0;
    strcpy(radioBrowserQuery, query);
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    strcpy(nomes[0], "Preparando Rede...");
    radioTotal = 0; radioVersao++;
    totalItens = 1; sel = 0; off = 0;
    menuAtual = MENU_RADIO_LISTA;

    pthread_t t;
    pthread_create(&t, NULL, threadBuscarRadio, (void*)(uintptr_t)isPodcast);
    pthread_detach(t);
}

void acaoCross_Radio() {
    if (showOpcoes) { executarAcaoMenuRadio(mapOpcoes[selOpcao]); return; }
    if (menuAtual == MENU_RADIO_CATEGORIA) {
        if (sel == 0) buscarEstacoesRadio("topclick", false);
        else if (sel == 1) buscarEstacoesRadio("saopaulo", false);
        else if (sel == 2) buscarEstacoesRadio("podcast", true);
        else if (sel == 3) {
            extern void exibirTecladoVirtual(int tipo);
            exibirTecladoVirtual(10);
        }
        else if (sel == 4) { menuAtual = MENU_RADIO_FAVORITOS; radioModo = 0; radioCarregarFavoritos(); }
        else if (sel == 5) podIniciar(NULL);
        else if (sel == 6) { radioPesquisaPodcast = true; extern void exibirTecladoVirtual(int tipo); exibirTecladoVirtual(10); }
    }
    else if (menuAtual == MENU_RADIO_LISTA || menuAtual == MENU_RADIO_FAVORITOS) {
        if (menuAtual == MENU_RADIO_LISTA && radioModo == 1) { if (sel < podTotal) podAbrirShow(sel); }
        else if (sel < radioTotal && strlen(radioLista[sel].url) > 5) { radioTocarIndice(sel); extern void conquistaGanhar(const char*); conquistaGanhar("radio_player"); }
        else if (strlen(linksAtuais[sel]) > 5) {
            // Preserve HTTPS: o cliente OpenOrbis faz o transporte TLS no PS4.
            tocarMusicaNova(linksAtuais[sel]);
        }
    }
}

void preencherMenuOpcoesRadio() {
    listaOpcoes[0] = "Pesquisar"; mapOpcoes[0] = 700;
    listaOpcoes[1] = menuAtual == MENU_RADIO_FAVORITOS ? "Remover dos Favoritos" : "Adicionar aos Favoritos"; mapOpcoes[1] = 701;
    listaOpcoes[2] = "PLAY / PAUSE"; mapOpcoes[2] = 702;
    listaOpcoes[3] = "PARAR"; mapOpcoes[3] = 703;
    listaOpcoes[4] = "PROXIMA ESTACAO"; mapOpcoes[4] = 706;
    listaOpcoes[5] = "ESTACAO ANTERIOR"; mapOpcoes[5] = 707;
    listaOpcoes[6] = "VOLUME +"; mapOpcoes[6] = 704;
    listaOpcoes[7] = "VOLUME -"; mapOpcoes[7] = 705;
    listaOpcoes[8] = "VOLTAR"; mapOpcoes[8] = 708;
    for (int i = 9; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
    totalOpcoes = 9; showOpcoes = true; selOpcao = 0;
}

void executarAcaoMenuRadio(int id) {
    if (id == 700) { extern void exibirTecladoVirtual(int tipo); exibirTecladoVirtual(10); showOpcoes = false; }
    else if (id == 701) {
        showOpcoes = false;
        if (menuAtual == MENU_RADIO_LISTA && radioModo == 1) { sprintf(msgStatus, "ABRA O PODCAST E FAVORITE UM EPISODIO"); msgTimer = 120; }
        else if ((menuAtual == MENU_RADIO_LISTA || menuAtual == MENU_RADIO_FAVORITOS) && sel < radioTotal) {
            bool fav = radioFavoritoAlternar(&radioLista[sel]);
            sprintf(msgStatus, fav ? "ADICIONADA AOS FAVORITOS" : "REMOVIDA DOS FAVORITOS"); msgTimer = 90;
            if (menuAtual == MENU_RADIO_FAVORITOS) { int volta = sel; radioCarregarFavoritos(); sel = volta < totalItens ? volta : totalItens - 1; }
        }
        else radioFavoritarAtual();
    }
    else if (id == 702) {
        if (strcmp(musicaAtual, "PARADO") == 0) { if (strlen(ultimaMusicaTocada) > 0) { tocarMusicaNova(ultimaMusicaTocada); sprintf(msgStatus, "REPRODUZINDO"); msgTimer = 90; } else { sprintf(msgStatus, "NENHUMA ESTACAO SELECIONADA"); msgTimer = 90; } }
        else { comandoPausar = !comandoPausar; sprintf(msgStatus, comandoPausar ? "PAUSADO" : "REPRODUZINDO"); msgTimer = 90; }
    }
    else if (id == 703) { tocarMusicaNova("PARADO"); sprintf(msgStatus, "PARADO"); msgTimer = 90; }
    else if (id == 704) { aumentarVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; }
    else if (id == 705) { diminuirVolume(); sprintf(msgStatus, "VOLUME: %d%%", volumeGeral); msgTimer = 60; }
    else if (id == 706) {
        if (radioTotal > 0) { radioTrocarEstacao(1); sprintf(msgStatus, "PROXIMA ESTACAO"); msgTimer = 90; }
        else if (totalItens > 0) { sel = (sel + 1) % totalItens; if (strlen(linksAtuais[sel]) > 5) { char lc[1024]; if (strncmp(linksAtuais[sel], "https://", 8) == 0) sprintf(lc, "http://%s", linksAtuais[sel] + 8); else strcpy(lc, linksAtuais[sel]); tocarMusicaNova(lc); sprintf(msgStatus, "PROXIMA ESTACAO"); msgTimer = 90; } }
    }
    else if (id == 707) {
        if (radioTotal > 0) { radioTrocarEstacao(-1); sprintf(msgStatus, "ESTACAO ANTERIOR"); msgTimer = 90; }
        else if (totalItens > 0) { sel = (sel - 1 + totalItens) % totalItens; if (strlen(linksAtuais[sel]) > 5) { char lc[1024]; if (strncmp(linksAtuais[sel], "https://", 8) == 0) sprintf(lc, "http://%s", linksAtuais[sel] + 8); else strcpy(lc, linksAtuais[sel]); tocarMusicaNova(lc); sprintf(msgStatus, "ESTACAO ANTERIOR"); msgTimer = 90; } }
    }
    else if (id == 708) { showOpcoes = false; }
}

// Triângulo: nas listas abre o player (como o de músicas); nas categorias, as opções antigas.
// As opções antigas continuam no player: três pontinhos > Opções antigas da rádio.
void acaoTriangle_Radio() {
    if (showOpcoes) { showOpcoes = false; return; }
    if ((menuAtual == MENU_RADIO_LISTA && radioModo != 1) || menuAtual == MENU_RADIO_FAVORITOS) {
        if (!radioUltima.url[0] && sel < radioTotal) radioUltima = radioLista[sel];
        extern void musicaPlayerAbrir(bool cheio);
        musicaPlayerAbrir(false);
        return;
    }
    preencherMenuOpcoesRadio();
}

void acaoCircle_Radio() {
    // Agora o controle central (controle.cpp) cuida de fechar Overlays
    // e da navega��o de volta (voltarNavegacao).
    // Esta fun��o pode ser usada para disparar efeitos colaterais espec�ficos se necess�rio.
}



