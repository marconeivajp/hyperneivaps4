// =========================================================================
// STREMIO: addons (protocolo aberto do Stremio) dentro de Mídia. Triângulo na raiz de Mídia troca
// entre o conteúdo local e o Stremio. Catálogo -> título -> (episódio) -> fonte -> player.
// Fonte com link direto toca como IPTV; fonte de torrent baixa em ordem (libhntorrent, modo
// "assistir") e o player lê enquanto baixa (torrent://<hash>/<arquivo>).
// Incluído no fim do network.cpp, depois do iptv.h (usa iptvHttpGetBuf e companhia).
// Funcional primeiro: a lista usa a mesma tela de nomes da Mídia.
// =========================================================================
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ------------------------------------------------------------------ JSON (árvore simples)
// Os textos ficam num bloco só (pool) e os nós guardam deslocamentos: realloc não quebra nada.
struct SjNo { char tipo; int chave, texto, filho, prox; double num; };   // tipo: n b d s a o
struct SjDoc { SjNo* nos; int n, cap; char* pool; int pn, pcap; bool erro; };

static void sjLiberar(SjDoc* d) { free(d->nos); free(d->pool); memset(d, 0, sizeof(*d)); }
static int sjNovo(SjDoc* d, char tipo) {
    if (d->n == d->cap) {
        int nova = d->cap ? d->cap * 2 : 1024;
        SjNo* nn = (SjNo*)realloc(d->nos, sizeof(SjNo) * (size_t)nova);
        if (!nn) { d->erro = true; return -1; }
        d->nos = nn; d->cap = nova;
    }
    SjNo* no = &d->nos[d->n];
    memset(no, 0, sizeof(*no)); no->tipo = tipo; no->chave = no->texto = no->filho = no->prox = -1;
    return d->n++;
}
static int sjPoolGarantir(SjDoc* d, int mais) {
    if (d->pn + mais + 1 <= d->pcap) return 1;
    int nova = d->pcap ? d->pcap : 4096;
    while (nova < d->pn + mais + 1) nova *= 2;
    char* np = (char*)realloc(d->pool, (size_t)nova);
    if (!np) { d->erro = true; return 0; }
    d->pool = np; d->pcap = nova;
    return 1;
}
static int sjPoolUtf8(SjDoc* d, unsigned cp) {
    if (!sjPoolGarantir(d, 4)) return 0;
    char* s = d->pool + d->pn;
    if (cp < 0x80) { s[0] = (char)cp; d->pn += 1; }
    else if (cp < 0x800) { s[0] = (char)(0xC0 | (cp >> 6)); s[1] = (char)(0x80 | (cp & 63)); d->pn += 2; }
    else if (cp < 0x10000) { s[0] = (char)(0xE0 | (cp >> 12)); s[1] = (char)(0x80 | ((cp >> 6) & 63)); s[2] = (char)(0x80 | (cp & 63)); d->pn += 3; }
    else { s[0] = (char)(0xF0 | (cp >> 18)); s[1] = (char)(0x80 | ((cp >> 12) & 63)); s[2] = (char)(0x80 | ((cp >> 6) & 63)); s[3] = (char)(0x80 | (cp & 63)); d->pn += 4; }
    return 1;
}
static const char* sjEspacos(const char* p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; return p; }
static unsigned sjHex4(const char* p) {
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i]; v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0xFFFFFFFFu;
    }
    return v;
}
// p aponta para as aspas de abertura. Devolve o deslocamento do texto no pool.
static const char* sjTextoLer(SjDoc* d, const char* p, int* saida) {
    p++;
    *saida = d->pn;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            char c = *p;
            if (c == 'u') {
                unsigned cp = sjHex4(p + 1);
                if (cp == 0xFFFFFFFFu) { d->erro = true; return p; }
                p += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF && p[1] == '\\' && p[2] == 'u') {
                    unsigned baixo = sjHex4(p + 3);
                    if (baixo >= 0xDC00 && baixo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (baixo - 0xDC00); p += 6; }
                }
                if (!sjPoolUtf8(d, cp)) return p;
            }
            else {
                char r = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c == 'b' ? '\b' : c == 'f' ? '\f' : c;
                if (!sjPoolGarantir(d, 1)) return p;
                d->pool[d->pn++] = r;
            }
            if (*p) p++;
            continue;
        }
        if (!sjPoolGarantir(d, 1)) return p;
        d->pool[d->pn++] = *p++;
    }
    if (!sjPoolGarantir(d, 1)) return p;
    d->pool[d->pn++] = 0;
    if (*p == '"') p++; else d->erro = true;
    return p;
}
static const char* sjValor(SjDoc* d, const char* p, int* indice, int profundidade) {
    *indice = -1;
    p = sjEspacos(p);
    if (profundidade > 64) { d->erro = true; return p; }
    if (*p == '{' || *p == '[') {
        bool objeto = *p == '{';
        int eu = sjNovo(d, objeto ? 'o' : 'a');
        if (eu < 0) return p;
        p = sjEspacos(p + 1);
        int ultimo = -1;
        while (*p && *p != (objeto ? '}' : ']') && !d->erro) {
            int chave = -1;
            if (objeto) {
                if (*p != '"') { d->erro = true; return p; }
                p = sjTextoLer(d, p, &chave);
                p = sjEspacos(p);
                if (*p != ':') { d->erro = true; return p; }
                p++;
            }
            int filho;
            p = sjValor(d, p, &filho, profundidade + 1);
            if (filho < 0) { d->erro = true; return p; }
            d->nos[filho].chave = chave;
            if (ultimo < 0) d->nos[eu].filho = filho; else d->nos[ultimo].prox = filho;
            ultimo = filho;
            p = sjEspacos(p);
            if (*p == ',') p = sjEspacos(p + 1);
        }
        if (*p) p++;
        *indice = eu;
        return p;
    }
    if (*p == '"') {
        int eu = sjNovo(d, 's');
        if (eu < 0) return p;
        int t; p = sjTextoLer(d, p, &t);
        d->nos[eu].texto = t;
        *indice = eu;
        return p;
    }
    if (!strncmp(p, "true", 4) || !strncmp(p, "false", 5)) {
        int eu = sjNovo(d, 'b');
        if (eu >= 0) d->nos[eu].num = p[0] == 't' ? 1 : 0;
        *indice = eu;
        return p + (p[0] == 't' ? 4 : 5);
    }
    if (!strncmp(p, "null", 4)) { *indice = sjNovo(d, 'n'); return p + 4; }
    char* fim = NULL;
    double v = strtod(p, &fim);
    if (fim == p) { d->erro = true; return p; }
    int eu = sjNovo(d, 'd');
    if (eu >= 0) d->nos[eu].num = v;
    *indice = eu;
    return fim;
}
static int sjAnalisar(SjDoc* d, const char* texto) {
    memset(d, 0, sizeof(*d));
    int raiz;
    sjValor(d, texto, &raiz, 0);
    if (d->erro) return -1;
    return raiz;
}
static const char* sjPoolTexto(const SjDoc* d, int off) { return off >= 0 ? d->pool + off : ""; }
static int sjCampo(const SjDoc* d, int obj, const char* chave) {
    if (obj < 0 || d->nos[obj].tipo != 'o') return -1;
    for (int c = d->nos[obj].filho; c >= 0; c = d->nos[c].prox) if (!strcmp(sjPoolTexto(d, d->nos[c].chave), chave)) return c;
    return -1;
}
static const char* sjTexto(const SjDoc* d, int no) { return (no >= 0 && d->nos[no].tipo == 's') ? sjPoolTexto(d, d->nos[no].texto) : ""; }
static const char* sjCampoTexto(const SjDoc* d, int obj, const char* chave) { return sjTexto(d, sjCampo(d, obj, chave)); }
static double sjCampoNum(const SjDoc* d, int obj, const char* chave, double padrao) {
    int c = sjCampo(d, obj, chave);
    if (c < 0) return padrao;
    if (d->nos[c].tipo == 'd' || d->nos[c].tipo == 'b') return d->nos[c].num;
    if (d->nos[c].tipo == 's') return atof(sjPoolTexto(d, d->nos[c].texto));
    return padrao;
}
static int sjPrimeiro(const SjDoc* d, int no) { return (no >= 0 && (d->nos[no].tipo == 'a' || d->nos[no].tipo == 'o')) ? d->nos[no].filho : -1; }
static int sjProximo(const SjDoc* d, int no) { return no >= 0 ? d->nos[no].prox : -1; }

#ifndef STREMIO_SO_JSON
// ------------------------------------------------------------------ addons
#define ST_ADDONS_TXT "/data/HyperNeiva/configuracao/stremio_addons.txt"
#define ST_PASTA_ASSISTIR "/data/HyperNeiva/baixado/torrent/assistindo"
#define ST_MAX_ADDONS 16
#define ST_MAX_CATALOGOS 32
#define ST_MAX_METAS 400
#define ST_MAX_EPISODIOS 800
#define ST_MAX_FONTES 120
// Cinemeta é o addon oficial de catálogo do Stremio (filmes e séries, sem vídeos). FenixFlix e
// FrostStream (pedidos do usuário, achados no stremio-addons.net) trazem as fontes de vídeo.
static const char* const stAddonsPadrao[] = {
    "https://v3-cinemeta.strem.io", "https://fenixflix.fenixhub.online", "https://froststream.cloutteam.com" };
#define ST_PADROES_MARCA "# padroes v2"
// Alguns addons (BestCine) recusam pedido que não pareça de navegador: 403 "Acesso negado".
#define ST_UA "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124 Safari/537.36"

struct StCatalogo { char tipo[24], id[96], nome[96]; bool skip; };
struct StAddon {
    char base[480], nome[96], erro[96];
    bool lido, temStream, temMeta;
    char tipos[200], prefixos[300];         // separados por espaço (" movie series ")
    StCatalogo cat[ST_MAX_CATALOGOS]; int totalCat;
};
struct StMeta { char id[120], tipo[24], nome[140]; int addon; };
struct StEpisodio { char id[160], nome[200]; int temporada, episodio; };
struct StFonte { char nome[240], url[1200], hash[48], ua[200]; int arquivo, addon; };

static StAddon stAddons[ST_MAX_ADDONS]; static int stTotalAddons = 0; static bool stAddonsCarregados = false;
static StMeta stMetas[ST_MAX_METAS]; static int stTotalMetas = 0;
static StEpisodio stEpisodios[ST_MAX_EPISODIOS]; static int stTotalEpisodios = 0;
static StFonte stFontes[ST_MAX_FONTES]; static int stTotalFontes = 0;

// Itens da lista da tela: cada linha de nomes[] aponta para uma ação.
enum { ST_I_ADICIONAR, ST_I_CATALOGO, ST_I_REMOVER, ST_I_META, ST_I_MAIS, ST_I_EPISODIO, ST_I_FONTE, ST_I_NADA,
       ST_I_SITE, ST_I_SITE_BUSCAR, ST_I_SITE_CATEGORIA, ST_I_SITE_ADDON, ST_I_SITE_PROXIMA };

// ------------------------------------------------------------------ stremio-addons.net (lista pública de addons)
// O site é Next.js: a lista vem em HTML com links /addons/<nome>; a página de cada addon traz o
// endereço do manifest.json. robots.txt do site libera /addons.
#define ST_SITE "https://stremio-addons.net"
#define ST_MAX_SITE 200
struct StSiteAddon { char slug[80], nome[100], descricao[160]; };
static StSiteAddon stSite[ST_MAX_SITE]; static int stTotalSite = 0;
static char stSiteProxima[300] = "";       // endereço da próxima página (rel="next"), vazio se acabou
static char stSiteTitulo[120] = "";
static const char* const stSiteCategorias[][2] = {
    { "Populares", "/addons?sort=popular" }, { "Novos", "/addons?sort=new" },
    { "Torrents", "/addons?categories=torrents&sort=popular" },
    { "Filmes e series", "/addons?categories=movies&categories=tv+shows&sort=popular" },
    { "Links diretos (HTTP)", "/addons?categories=http+streams&sort=popular" },
    { "TV ao vivo", "/addons?categories=live+tv&sort=popular" },
    { "Anime", "/addons?categories=anime&sort=popular" },
    { "Catalogos (metadata)", "/addons?categories=metadata&sort=popular" } };
static const int stTotalSiteCategorias = (int)(sizeof(stSiteCategorias) / sizeof(stSiteCategorias[0]));
static int stImeModo = 0;                   // 1 endereço do addon, 2 busca no site
static void stLog(const char* formato, ...);

// &amp; &quot; &#x27; ... e tira espaços repetidos.
static void stHtmlTexto(const char* ini, const char* fim, char* saida, int tam) {
    int n = 0; bool espaco = true;
    for (const char* p = ini; p < fim && n < tam - 1; ) {
        if (*p == '&') {
            const char* q = (const char*)memchr(p, ';', (size_t)(fim - p));
            if (q && q - p < 10) {
                char ent[12]; int k = (int)(q - p - 1); memcpy(ent, p + 1, (size_t)k); ent[k] = 0;
                unsigned cp = 0;
                if (!strcmp(ent, "amp")) cp = '&'; else if (!strcmp(ent, "quot")) cp = '"'; else if (!strcmp(ent, "lt")) cp = '<';
                else if (!strcmp(ent, "gt")) cp = '>'; else if (!strcmp(ent, "nbsp")) cp = ' '; else if (!strcmp(ent, "apos")) cp = '\'';
                else if (ent[0] == '#') cp = (unsigned)strtoul(ent + (ent[1] == 'x' ? 2 : 1), NULL, ent[1] == 'x' ? 16 : 10);
                if (cp) {
                    char u[5]; int b = 0;
                    if (cp < 0x80) u[b++] = (char)cp;
                    else if (cp < 0x800) { u[b++] = (char)(0xC0 | (cp >> 6)); u[b++] = (char)(0x80 | (cp & 63)); }
                    else { u[b++] = (char)(0xE0 | (cp >> 12)); u[b++] = (char)(0x80 | ((cp >> 6) & 63)); u[b++] = (char)(0x80 | (cp & 63)); }
                    for (int i = 0; i < b && n < tam - 1; i++) saida[n++] = u[i];
                    espaco = cp == ' ';
                    p = q + 1; continue;
                }
            }
        }
        char c = *p++;
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ' && espaco) continue;
        espaco = c == ' ';
        saida[n++] = c;
    }
    while (n > 0 && saida[n - 1] == ' ') n--;
    saida[n] = 0;
}
// Lê uma página de lista do site: cada cartão é um <a href="/addons/<nome>"> com o nome e a descrição dentro.
static void stSiteLerLista(const char* caminho, bool continuar) {
    if (!continuar) stTotalSite = 0;
    stSiteProxima[0] = 0;
    char url[400];
    if (!strncmp(caminho, "http", 4)) snprintf(url, sizeof(url), "%s", caminho);
    else snprintf(url, sizeof(url), "%s%s", ST_SITE, caminho);
    snprintf(msgStatus, sizeof(msgStatus), "STREMIO: LENDO A LISTA DE ADDONS..."); msgTimer = 120;
    char* h = NULL; size_t n = 0;
    int status = iptvHttpGetBuf(url, ST_UA, &h, &n, 8 * 1024 * 1024);
    if (status < 200 || status >= 300 || !h) { stLog("SITE HTTP %d %s", status, url); if (h) free(h); return; }
    int novos = 0;
    const char* fimHtml = h + n;
    for (const char* p = strstr(h, "href=\"/addons/"); p && stTotalSite < ST_MAX_SITE; ) {
        const char* s = p + 14;
        const char* aspas = strchr(s, '"');
        const char* prox = strstr(s, "href=\"/addons/");
        if (!aspas) break;
        char slug[80]; int k = (int)(aspas - s);
        bool valido = k > 0 && k < (int)sizeof(slug);
        if (valido) { memcpy(slug, s, (size_t)k); slug[k] = 0; for (int i = 0; i < k; i++) if (!strchr("abcdefghijklmnopqrstuvwxyz0123456789._+-", slug[i])) valido = false; }
        for (int i = 0; valido && i < stTotalSite; i++) if (!strcmp(stSite[i].slug, slug)) valido = false;
        if (valido) {
            // Texto do cartão: pedaços entre as tags, até o próximo cartão (ou 6 KB adiante).
            const char* fimCartao = prox ? prox : fimHtml;
            if (fimCartao - aspas > 6000) fimCartao = aspas + 6000;
            char partes[2][200] = { "", "" }; int achadas = 0;
            const char* q = strchr(aspas, '>');
            while (q && q < fimCartao && achadas < 2) {
                const char* abre = (const char*)memchr(q + 1, '<', (size_t)(fimCartao - q - 1));
                if (!abre) break;
                char t[200]; stHtmlTexto(q + 1, abre, t, sizeof(t));
                if (t[0] && strcmp(t, "|")) snprintf(partes[achadas++], 200, "%s", t);
                q = strchr(abre, '>');
            }
            if (achadas > 0) {
                StSiteAddon* a = &stSite[stTotalSite++];
                snprintf(a->slug, sizeof(a->slug), "%s", slug);
                snprintf(a->nome, sizeof(a->nome), "%.98s", partes[0]);
                snprintf(a->descricao, sizeof(a->descricao), "%.158s", partes[1]);
                novos++;
            }
        }
        p = prox;
    }
    const char* proxima = strstr(h, "rel=\"next\" href=\"");
    if (proxima) {
        proxima += 17;
        const char* f = strchr(proxima, '"');
        if (f) { char t[300]; stHtmlTexto(proxima, f, t, sizeof(t)); snprintf(stSiteProxima, sizeof(stSiteProxima), "%s", t); }
    }
    free(h);
    stLog("SITE %s: +%d (proxima: %s)", url, novos, stSiteProxima[0] ? stSiteProxima : "-");
}
// Página do addon no site -> endereço do manifest.json (https:// ou stremio://).
static bool stSiteManifesto(const char* slug, char* saida, int tam) {
    char url[300]; snprintf(url, sizeof(url), "%s/addons/%s", ST_SITE, slug);
    char* h = NULL; size_t n = 0;
    int status = iptvHttpGetBuf(url, ST_UA, &h, &n, 8 * 1024 * 1024);
    saida[0] = 0;
    if (status >= 200 && status < 300 && h) {
        for (const char* p = h; (p = strstr(p, "manifest.json")) != NULL; p++) {
            const char* ini = p;
            while (ini > h && !strchr("\"' <>\\(", ini[-1])) ini--;
            if (strncmp(ini, "https://", 8) && strncmp(ini, "http://", 7) && strncmp(ini, "stremio://", 10)) continue;
            if (strstr(ini, "stremio-addons.net") == ini + (ini[4] == 's' ? 8 : 7)) continue;   // o manifest do próprio site
            int k = (int)(p + 13 - ini);
            if (k >= tam) continue;
            memcpy(saida, ini, (size_t)k); saida[k] = 0;
            break;
        }
    }
    if (h) free(h);
    stLog("SITE manifest %s: %s", slug, saida[0] ? saida : "nao achou");
    return saida[0] != 0;
}
static int stAdicionarAddon(const char* endereco);   // mais abaixo
struct StItem { int tipo, a, b; };
static StItem stItens[3000];
static bool stAtivo = false;
static int stNivel = 0;                 // 0 raiz, 1 catálogo, 2 episódios, 3 fontes
static int stCatAddon = -1, stCatIndice = -1, stCatSkip = 0;
static int stMetaAtual = -1, stSelNivel[6], stOffNivel[6];   // 4 e 5: stremio-addons.net
static char stTitulo[200] = "";
static bool stImeAberto = false;
static int stRemoverConfirma = -1; static uint64_t stRemoverQuando = 0;

static void stLog(const char* formato, ...) {
    FILE* f = fopen("/data/HyperNeiva/stremio_log.txt", "a");
    if (!f) return;
    va_list a; va_start(a, formato); vfprintf(f, formato, a); va_end(a);
    fputc('\n', f); fclose(f);
}

// "stremio://x/manifest.json" vira https; tira o /manifest.json do fim para montar os pedidos.
static void stBaseDoManifest(const char* url, char* base, int tam) {
    char u[480];
    if (!strncmp(url, "stremio://", 10)) snprintf(u, sizeof(u), "https://%s", url + 10);
    else snprintf(u, sizeof(u), "%s", url);
    int n = (int)strlen(u);
    while (n > 0 && (u[n - 1] == ' ' || u[n - 1] == '\r' || u[n - 1] == '\n')) u[--n] = 0;
    const char* m = strstr(u, "/manifest.json");
    if (m) u[m - u] = 0;
    n = (int)strlen(u);
    while (n > 0 && u[n - 1] == '/') u[--n] = 0;
    snprintf(base, tam, "%s", u);
}
// Codifica um pedaço do caminho (id com espaço, acento etc.); ':' fica, como o Stremio manda.
static void stCodificar(const char* s, char* saida, int tam) {
    int n = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p && n + 4 < tam; p++) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || strchr("-_.~:", *p)) saida[n++] = (char)*p;
        else n += snprintf(saida + n, (size_t)(tam - n), "%%%02X", *p);
    }
    saida[n] = 0;
}
static bool stListaTem(const char* lista, const char* palavra) {
    if (!lista[0]) return false;
    char alvo[120]; snprintf(alvo, sizeof(alvo), " %s ", palavra);
    return strstr(lista, alvo) != NULL;
}
static void stListaAdicionar(char* lista, int tam, const char* palavra) {
    if (!palavra[0] || stListaTem(lista, palavra)) return;
    int n = (int)strlen(lista);
    snprintf(lista + n, (size_t)(tam - n), "%s%s ", n ? "" : " ", palavra);
}

static int stBaixarJson(const char* url, SjDoc* doc) {
    char* texto = NULL; size_t n = 0;
    int status = iptvHttpGetBuf(url, ST_UA, &texto, &n, 16 * 1024 * 1024);
    if (status < 200 || status >= 300 || !texto) { stLog("HTTP %d %s", status, url); if (texto) free(texto); return -1; }
    int raiz = sjAnalisar(doc, texto);
    free(texto);
    if (raiz < 0) { stLog("JSON invalido %s", url); sjLiberar(doc); }
    return raiz;
}

static void stLerManifesto(StAddon* a) {
    a->lido = true; a->erro[0] = 0; a->totalCat = 0; a->tipos[0] = a->prefixos[0] = 0;
    char url[600]; snprintf(url, sizeof(url), "%s/manifest.json", a->base);
    SjDoc d; int raiz = stBaixarJson(url, &d);
    if (raiz < 0) { snprintf(a->erro, sizeof(a->erro), "manifest nao abriu"); if (!a->nome[0]) snprintf(a->nome, sizeof(a->nome), "%.90s", a->base); return; }
    snprintf(a->nome, sizeof(a->nome), "%.90s", sjCampoTexto(&d, raiz, "name")[0] ? sjCampoTexto(&d, raiz, "name") : a->base);
    for (int t = sjPrimeiro(&d, sjCampo(&d, raiz, "types")); t >= 0; t = sjProximo(&d, t)) stListaAdicionar(a->tipos, sizeof(a->tipos), sjTexto(&d, t));
    for (int t = sjPrimeiro(&d, sjCampo(&d, raiz, "idPrefixes")); t >= 0; t = sjProximo(&d, t)) stListaAdicionar(a->prefixos, sizeof(a->prefixos), sjTexto(&d, t));
    for (int r = sjPrimeiro(&d, sjCampo(&d, raiz, "resources")); r >= 0; r = sjProximo(&d, r)) {
        const char* nome = d.nos[r].tipo == 's' ? sjTexto(&d, r) : sjCampoTexto(&d, r, "name");
        if (!strcmp(nome, "stream")) a->temStream = true;
        if (!strcmp(nome, "meta")) a->temMeta = true;
        if (d.nos[r].tipo == 'o') {
            for (int t = sjPrimeiro(&d, sjCampo(&d, r, "types")); t >= 0; t = sjProximo(&d, t)) stListaAdicionar(a->tipos, sizeof(a->tipos), sjTexto(&d, t));
            for (int t = sjPrimeiro(&d, sjCampo(&d, r, "idPrefixes")); t >= 0; t = sjProximo(&d, t)) stListaAdicionar(a->prefixos, sizeof(a->prefixos), sjTexto(&d, t));
        }
    }
    for (int c = sjPrimeiro(&d, sjCampo(&d, raiz, "catalogs")); c >= 0 && a->totalCat < ST_MAX_CATALOGOS; c = sjProximo(&d, c)) {
        // Catálogo que exige busca ou gênero obrigatório não abre sem pergunta: fica de fora por enquanto.
        bool obrigatorio = false, skip = false;
        for (int e = sjPrimeiro(&d, sjCampo(&d, c, "extra")); e >= 0; e = sjProximo(&d, e)) {
            if (sjCampoNum(&d, e, "isRequired", 0)) obrigatorio = true;
            if (!strcmp(sjCampoTexto(&d, e, "name"), "skip")) skip = true;
        }
        for (int e = sjPrimeiro(&d, sjCampo(&d, c, "extraRequired")); e >= 0; e = sjProximo(&d, e)) obrigatorio = true;
        for (int e = sjPrimeiro(&d, sjCampo(&d, c, "extraSupported")); e >= 0; e = sjProximo(&d, e)) if (!strcmp(sjTexto(&d, e), "skip")) skip = true;
        if (obrigatorio) continue;
        StCatalogo* k = &a->cat[a->totalCat++];
        snprintf(k->tipo, sizeof(k->tipo), "%s", sjCampoTexto(&d, c, "type"));
        snprintf(k->id, sizeof(k->id), "%s", sjCampoTexto(&d, c, "id"));
        snprintf(k->nome, sizeof(k->nome), "%s", sjCampoTexto(&d, c, "name")[0] ? sjCampoTexto(&d, c, "name") : k->id);
        k->skip = skip;
    }
    // BestCine e outros: o manifest público é só um aviso, as fontes vêm depois de configurar no site.
    int dicas = sjCampo(&d, raiz, "behaviorHints");
    if (sjCampoNum(&d, dicas, "configurationRequired", 0) && !a->temStream && a->totalCat == 0)
        snprintf(a->erro, sizeof(a->erro), "precisa configurar no site do addon e colar o endereco configurado");
    stLog("ADDON %s: %d catalogos, stream=%d meta=%d tipos=[%s] prefixos=[%s]%s", a->nome, a->totalCat, a->temStream, a->temMeta, a->tipos, a->prefixos, a->erro[0] ? " (precisa configurar)" : "");
    sjLiberar(&d);
}

static void stSalvarAddons() {
    FILE* f = fopen(ST_ADDONS_TXT, "w");
    if (!f) return;
    fprintf(f, "# Addons do Stremio: um endereco de manifest.json por linha (o app le na abertura do Stremio)\n");
    fprintf(f, "%s\n", ST_PADROES_MARCA);
    for (int i = 0; i < stTotalAddons; i++) fprintf(f, "%s/manifest.json\n", stAddons[i].base);
    fclose(f);
}
static void stCarregarAddons() {
    if (stAddonsCarregados) return;
    stAddonsCarregados = true;
    stTotalAddons = 0;
    FILE* f = fopen(ST_ADDONS_TXT, "r");
    bool temPadroes = false;
    if (f) {
        char linha[600];
        while (fgets(linha, sizeof(linha), f) && stTotalAddons < ST_MAX_ADDONS) {
            if (!strncmp(linha, ST_PADROES_MARCA, strlen(ST_PADROES_MARCA))) temPadroes = true;
            if (linha[0] == '#' || strlen(linha) < 8) continue;
            StAddon* a = &stAddons[stTotalAddons];
            memset(a, 0, sizeof(*a));
            stBaseDoManifest(linha, a->base, sizeof(a->base));
            if (a->base[0]) stTotalAddons++;
        }
        fclose(f);
    }
    // Primeira vez (ou arquivo da 4.05/4.06, só com o Cinemeta): entram os padrões que faltam, uma vez só.
    if (!temPadroes) {
        for (unsigned k = 0; k < sizeof(stAddonsPadrao) / sizeof(stAddonsPadrao[0]) && stTotalAddons < ST_MAX_ADDONS; k++) {
            bool existe = false;
            for (int i = 0; i < stTotalAddons; i++) if (!strcmp(stAddons[i].base, stAddonsPadrao[k])) existe = true;
            if (existe) continue;
            memset(&stAddons[stTotalAddons], 0, sizeof(StAddon));
            snprintf(stAddons[stTotalAddons].base, sizeof(stAddons[0].base), "%s", stAddonsPadrao[k]);
            stTotalAddons++;
        }
        stSalvarAddons();
    }
    for (int i = 0; i < stTotalAddons; i++) stLerManifesto(&stAddons[i]);
}

// ------------------------------------------------------------------ montar a lista na tela
static bool stAddonAtende(const StAddon* a, const char* tipo, const char* id);
// Catálogo só aparece se algum addon instalado tem vídeo para o tipo e para os ids dele
// (ex.: os do Cinemeta, "tt...", aparecem quando o FenixFlix ou o FrostStream estão instalados).
static bool stCatalogoTemFonte(int addon, int cat) {
    const StAddon* a = &stAddons[addon];
    const char* tipo = a->cat[cat].tipo;
    for (int s = 0; s < stTotalAddons; s++) {
        const StAddon* f = &stAddons[s];
        if (!f->temStream || f->erro[0]) continue;
        if (s == addon && (!f->tipos[0] || stListaTem(f->tipos, tipo))) return true;
        if (!a->prefixos[0]) { if (stAddonAtende(f, tipo, "tt")) return true; continue; }
        const char* p = a->prefixos;
        while (*p) {
            while (*p == ' ') p++;
            const char* fim = strchr(p, ' ');
            int n = fim ? (int)(fim - p) : (int)strlen(p);
            char prefixo[64]; if (n <= 0 || n >= (int)sizeof(prefixo)) { p += n; continue; }
            memcpy(prefixo, p, (size_t)n); prefixo[n] = 0;
            if (stAddonAtende(f, tipo, prefixo)) return true;
            p += n;
        }
    }
    return false;
}
static int stTotalItens = 0;   // nomes, totalItens, sel e off vêm do menu.h
static void stItem(int tipo, int a, int b, const char* texto) {
    if (stTotalItens >= 3000) return;
    stItens[stTotalItens] = { tipo, a, b };
    snprintf(nomes[stTotalItens], 256, "%s", texto);
    stTotalItens++;
}
static void stMostrar() {
    stTotalItens = 0;
    char linha[300];
    if (stNivel == 4) {
        stItem(ST_I_SITE_BUSCAR, 0, 0, "Buscar addon pelo nome");
        for (int i = 0; i < stTotalSiteCategorias; i++) { snprintf(linha, sizeof(linha), "Categoria: %s", stSiteCategorias[i][0]); stItem(ST_I_SITE_CATEGORIA, i, 0, linha); }
        snprintf(stTitulo, sizeof(stTitulo), "STREMIO > Procurar addons (stremio-addons.net)");
    }
    else if (stNivel == 5) {
        for (int i = 0; i < stTotalSite; i++) {
            bool instalado = false;
            for (int k = 0; k < stTotalAddons; k++) if (!strcasecmp(stAddons[k].nome, stSite[i].nome)) instalado = true;
            snprintf(linha, sizeof(linha), "%s%s - %s", instalado ? "[instalado] " : "", stSite[i].nome, stSite[i].descricao);
            stItem(ST_I_SITE_ADDON, i, 0, linha);
        }
        if (stSiteProxima[0]) stItem(ST_I_SITE_PROXIMA, 0, 0, "Proxima pagina...");
        if (stTotalSite == 0) stItem(ST_I_NADA, 0, 0, "Nada encontrado (ou o site nao respondeu)");
        snprintf(stTitulo, sizeof(stTitulo), "STREMIO > %.80s (%d) - X instala", stSiteTitulo, stTotalSite);
    }
    else if (stNivel == 0) {
        stItem(ST_I_ADICIONAR, 0, 0, "+ Adicionar addon (endereco do manifest.json)");
        stItem(ST_I_SITE, 0, 0, "+ Procurar addons (stremio-addons.net)");
        for (int i = 0; i < stTotalAddons; i++) {
            StAddon* a = &stAddons[i];
            if (a->erro[0]) { snprintf(linha, sizeof(linha), "[%s] erro: %s", a->nome, a->erro); stItem(ST_I_NADA, i, 0, linha); }
            for (int c = 0; c < a->totalCat; c++) {
                if (!stCatalogoTemFonte(i, c)) continue;   // nenhum addon instalado tem vídeo para ele
                const char* tipo = !strcmp(a->cat[c].tipo, "movie") ? "Filmes" : !strcmp(a->cat[c].tipo, "series") ? "Series" : a->cat[c].tipo;
                snprintf(linha, sizeof(linha), "%s - %s (%s)", a->nome, a->cat[c].nome, tipo);
                stItem(ST_I_CATALOGO, i, c, linha);
            }
        }
        for (int i = 0; i < stTotalAddons; i++) {
            snprintf(linha, sizeof(linha), stRemoverConfirma == i ? "- Aperte X de novo para remover: %s" : "- Remover addon: %s", stAddons[i].nome);
            stItem(ST_I_REMOVER, i, 0, linha);
        }
        snprintf(stTitulo, sizeof(stTitulo), "STREMIO (Triangulo: voltar a Midia local)");
    }
    else if (stNivel == 1) {
        for (int i = 0; i < stTotalMetas; i++) stItem(ST_I_META, i, 0, stMetas[i].nome);
        if (stTotalMetas > 0 && stTotalMetas < ST_MAX_METAS && stCatAddon >= 0 && stAddons[stCatAddon].cat[stCatIndice].skip) stItem(ST_I_MAIS, 0, 0, "Mais...");
        if (stTotalMetas == 0) stItem(ST_I_NADA, 0, 0, "Catalogo vazio ou fora do ar");
    }
    else if (stNivel == 2) {
        for (int i = 0; i < stTotalEpisodios; i++) stItem(ST_I_EPISODIO, i, 0, stEpisodios[i].nome);
        if (stTotalEpisodios == 0) stItem(ST_I_NADA, 0, 0, "Sem episodios");
    }
    else if (stNivel == 3) {
        for (int i = 0; i < stTotalFontes; i++) stItem(ST_I_FONTE, i, 0, stFontes[i].nome);
        if (stTotalFontes == 0) stItem(ST_I_NADA, 0, 0, "Nenhuma fonte: adicione um addon que tenha videos");
    }
    totalItens = stTotalItens;
    if (sel >= totalItens) sel = totalItens > 0 ? totalItens - 1 : 0;
    if (off > sel) off = sel;
    snprintf(msgStatus, sizeof(msgStatus), "%.120s", stTitulo); msgTimer = 400;
}

// ------------------------------------------------------------------ pedidos aos addons
static void stAbrirCatalogo(int addon, int cat, bool mais) {
    StAddon* a = &stAddons[addon];
    StCatalogo* k = &a->cat[cat];
    if (!mais) { stTotalMetas = 0; stCatSkip = 0; }
    char id[200], url[900];
    stCodificar(k->id, id, sizeof(id));
    if (stCatSkip > 0) snprintf(url, sizeof(url), "%s/catalog/%s/%s/skip=%d.json", a->base, k->tipo, id, stCatSkip);
    else snprintf(url, sizeof(url), "%s/catalog/%s/%s.json", a->base, k->tipo, id);
    snprintf(msgStatus, sizeof(msgStatus), "STREMIO: CARREGANDO %s...", k->nome); msgTimer = 120;
    SjDoc d; int raiz = stBaixarJson(url, &d);
    int novos = 0;
    if (raiz >= 0) {
        for (int m = sjPrimeiro(&d, sjCampo(&d, raiz, "metas")); m >= 0 && stTotalMetas < ST_MAX_METAS; m = sjProximo(&d, m)) {
            StMeta* x = &stMetas[stTotalMetas];
            snprintf(x->id, sizeof(x->id), "%s", sjCampoTexto(&d, m, "id"));
            if (!x->id[0]) continue;
            snprintf(x->tipo, sizeof(x->tipo), "%s", sjCampoTexto(&d, m, "type")[0] ? sjCampoTexto(&d, m, "type") : k->tipo);
            const char* ano = sjCampoTexto(&d, m, "releaseInfo");
            if (ano[0]) snprintf(x->nome, sizeof(x->nome), "%.120s (%.12s)", sjCampoTexto(&d, m, "name"), ano);
            else snprintf(x->nome, sizeof(x->nome), "%.130s", sjCampoTexto(&d, m, "name"));
            x->addon = addon;
            stTotalMetas++; novos++;
        }
        sjLiberar(&d);
    }
    stCatSkip += novos;
    stCatAddon = addon; stCatIndice = cat;
    snprintf(stTitulo, sizeof(stTitulo), "STREMIO > %.60s > %.60s (%d)", a->nome, k->nome, stTotalMetas);
    stLog("CATALOGO %s %s: +%d", a->nome, k->nome, novos);
}

static int stCompararEpisodio(const void* x, const void* y) {
    const StEpisodio* a = (const StEpisodio*)x; const StEpisodio* b = (const StEpisodio*)y;
    // Temporada 0 (especiais) vai para o fim.
    int ta = a->temporada == 0 ? 100000 : a->temporada, tb = b->temporada == 0 ? 100000 : b->temporada;
    if (ta != tb) return ta - tb;
    return a->episodio - b->episodio;
}
// Série: pede a lista de episódios ao addon do catálogo (ou a qualquer um que tenha "meta").
static bool stAbrirSerie(int meta) {
    StMeta* m = &stMetas[meta];
    stTotalEpisodios = 0;
    int ordem[ST_MAX_ADDONS + 1], n = 0;
    ordem[n++] = m->addon;
    for (int i = 0; i < stTotalAddons; i++) if (i != m->addon && stAddons[i].temMeta) ordem[n++] = i;
    char id[240]; stCodificar(m->id, id, sizeof(id));
    for (int k = 0; k < n && stTotalEpisodios == 0; k++) {
        StAddon* a = &stAddons[ordem[k]];
        char url[900]; snprintf(url, sizeof(url), "%s/meta/%s/%s.json", a->base, m->tipo, id);
        SjDoc d; int raiz = stBaixarJson(url, &d);
        if (raiz < 0) continue;
        int meta2 = sjCampo(&d, raiz, "meta");
        for (int v = sjPrimeiro(&d, sjCampo(&d, meta2, "videos")); v >= 0 && stTotalEpisodios < ST_MAX_EPISODIOS; v = sjProximo(&d, v)) {
            StEpisodio* e = &stEpisodios[stTotalEpisodios];
            snprintf(e->id, sizeof(e->id), "%s", sjCampoTexto(&d, v, "id"));
            if (!e->id[0]) continue;
            e->temporada = (int)sjCampoNum(&d, v, "season", 0);
            e->episodio = (int)sjCampoNum(&d, v, "episode", sjCampoNum(&d, v, "number", 0));
            const char* t = sjCampoTexto(&d, v, "name")[0] ? sjCampoTexto(&d, v, "name") : sjCampoTexto(&d, v, "title");
            if (e->temporada || e->episodio) snprintf(e->nome, sizeof(e->nome), "T%d E%02d - %.150s", e->temporada, e->episodio, t);
            else snprintf(e->nome, sizeof(e->nome), "%.190s", t);
            stTotalEpisodios++;
        }
        sjLiberar(&d);
    }
    qsort(stEpisodios, (size_t)stTotalEpisodios, sizeof(StEpisodio), stCompararEpisodio);
    snprintf(stTitulo, sizeof(stTitulo), "STREMIO > %.100s (%d episodios)", m->nome, stTotalEpisodios);
    return stTotalEpisodios > 0;
}

static bool stAddonAtende(const StAddon* a, const char* tipo, const char* id) {
    if (!a->temStream) return false;
    if (a->tipos[0] && !stListaTem(a->tipos, tipo)) return false;
    if (!a->prefixos[0]) return true;
    // idPrefixes: o id tem que começar por um deles ("tt", "kitsu:" ...)
    const char* p = a->prefixos;
    while (*p) {
        while (*p == ' ') p++;
        const char* fim = strchr(p, ' ');
        int n = fim ? (int)(fim - p) : (int)strlen(p);
        if (n > 0 && !strncmp(id, p, (size_t)n)) return true;
        p += n;
    }
    return false;
}
static void stPrimeiraLinha(const char* s, char* saida, int tam) {
    int n = 0;
    for (; *s && n < tam - 1; s++) saida[n++] = (*s == '\n' || *s == '\r') ? ' ' : *s;
    saida[n] = 0;
}
static void stBuscarFontes(const char* tipo, const char* id, const char* titulo) {
    stTotalFontes = 0;
    char idc[240]; stCodificar(id, idc, sizeof(idc));
    for (int i = 0; i < stTotalAddons; i++) {
        StAddon* a = &stAddons[i];
        if (a->erro[0] || !stAddonAtende(a, tipo, id)) continue;
        snprintf(msgStatus, sizeof(msgStatus), "STREMIO: BUSCANDO FONTES EM %s...", a->nome); msgTimer = 120;
        char url[900]; snprintf(url, sizeof(url), "%s/stream/%s/%s.json", a->base, tipo, idc);
        SjDoc d; int raiz = stBaixarJson(url, &d);
        if (raiz < 0) continue;
        int achados = 0;
        for (int s = sjPrimeiro(&d, sjCampo(&d, raiz, "streams")); s >= 0 && stTotalFontes < ST_MAX_FONTES; s = sjProximo(&d, s)) {
            StFonte* f = &stFontes[stTotalFontes];
            memset(f, 0, sizeof(*f));
            snprintf(f->url, sizeof(f->url), "%s", sjCampoTexto(&d, s, "url"));
            snprintf(f->hash, sizeof(f->hash), "%s", sjCampoTexto(&d, s, "infoHash"));
            f->arquivo = (int)sjCampoNum(&d, s, "fileIdx", -1);
            if (!f->url[0] && strlen(f->hash) != 40) continue;   // externalUrl, ytId etc.: ainda sem suporte
            // Link que pede User-Agent próprio (behaviorHints.proxyHeaders.request).
            int pedido = sjCampo(&d, sjCampo(&d, sjCampo(&d, s, "behaviorHints"), "proxyHeaders"), "request");
            snprintf(f->ua, sizeof(f->ua), "%s", sjCampoTexto(&d, pedido, "User-Agent")[0] ? sjCampoTexto(&d, pedido, "User-Agent") : sjCampoTexto(&d, pedido, "user-agent"));
            char nome[120], desc[200];
            stPrimeiraLinha(sjCampoTexto(&d, s, "name"), nome, sizeof(nome));
            stPrimeiraLinha(sjCampoTexto(&d, s, "title")[0] ? sjCampoTexto(&d, s, "title") : sjCampoTexto(&d, s, "description"), desc, sizeof(desc));
            snprintf(f->nome, sizeof(f->nome), "%s%.40s | %.60s | %.120s", f->url[0] ? "[link] " : "[torrent] ", a->nome, nome, desc);
            f->addon = i;
            stTotalFontes++; achados++;
        }
        sjLiberar(&d);
        stLog("FONTES %s %s: %d", a->nome, id, achados);
    }
    // 4K (2160p/UHD) não decodifica no PS4: vai para o fim da lista, com aviso. A ordem do resto fica.
    static StFonte ordenadas[ST_MAX_FONTES];
    int n = 0;
    for (int passo = 0; passo < 2; passo++)
        for (int i = 0; i < stTotalFontes; i++) {
            char minusculo[240]; int k = 0;
            for (const char* c = stFontes[i].nome; *c && k < 239; c++) minusculo[k++] = (char)tolower((unsigned char)*c);
            minusculo[k] = 0;
            bool quatroK = strstr(minusculo, "4k") || strstr(minusculo, "2160") || strstr(minusculo, "uhd");
            if (quatroK != (passo == 1)) continue;
            ordenadas[n] = stFontes[i];
            if (quatroK) { char t[240]; snprintf(t, sizeof(t), "[4K: o PS4 nao toca] %s", stFontes[i].nome); snprintf(ordenadas[n].nome, sizeof(ordenadas[n].nome), "%s", t); }
            n++;
        }
    memcpy(stFontes, ordenadas, sizeof(StFonte) * (size_t)n);
    snprintf(stTitulo, sizeof(stTitulo), "STREMIO > %.100s (%d fontes)", titulo, stTotalFontes);
}

// ------------------------------------------------------------------ torrent: esperar o começo e tocar
extern "C" {
    int hn_torrent_iniciar(const char* dados, const char* destino);
    typedef struct HnTorrentFluxoInfo { int temMetadados, arquivo; long long tamanho, baixado, continuo; int fontes, taxa; char nome[256], erro[128]; } HnTorrentFluxoInfo;
    int hn_torrent_fluxo_abrir(const char* origem, int arquivo, const char* pasta, char* erroTexto, int tamanhoErro);
    int hn_torrent_fluxo_info(HnTorrentFluxoInfo* saida);
    int hn_torrent_fluxo_ler(long long posicao, void* destino, int tamanho, int esperaMs);
    void hn_torrent_fluxo_fechar(int apagar);
}
static bool stEsperandoTorrent = false;
static uint64_t stEsperaInicioMs = 0, stEsperaUltimoMs = 0;
static char stTorrentCaminho[120] = "";
static AVIOContext* stAvio = NULL;
static int64_t stPos = 0, stTamanho = 0;
static volatile bool stPararLeitura = false;
static uint64_t stAbrindoDesdeMs = 0;

// ------------------------------------------------------------------ link direto: http/https com Range
// O FFmpeg do PS4 não tem TLS: os links /r/ do FenixFlix redirecionam (301, depois 302) para um MP4
// em https (Cloudflare Workers) e falhavam na hora. Este leitor usa o sceHttp (que tem TLS): segue
// os saltos, pede "Range: bytes=pos-" e reabre a conexão quando o player pula (o MP4 precisa).
static char stLinkUrl[2048] = "", stLinkFinal[2048] = "", stLinkUa[200] = "";
static int stLinkTpl = -1, stLinkConn = -1, stLinkReq = -1;
static int64_t stLinkPos = 0, stLinkPosReq = -1, stLinkTam = -1;
static volatile bool stLinkParar = false;
static AVIOContext* stLinkAvio = NULL;

static void stLinkFecharConexao() {
    if (stLinkReq >= 0) sceHttpDeleteRequest(stLinkReq);
    if (stLinkConn >= 0) sceHttpDeleteConnection(stLinkConn);
    if (stLinkTpl >= 0) sceHttpDeleteTemplate(stLinkTpl);
    stLinkReq = stLinkConn = stLinkTpl = -1;
    stLinkPosReq = -1;
}
static bool stLinkCabecalho(int req, const char* nome, char* saida, int tam) {
    char* cab = NULL; size_t tc = 0;
    saida[0] = 0;
    if (sceHttpGetAllResponseHeaders(req, &cab, &tc) < 0 || !cab) return false;
    size_t k = strlen(nome);
    for (size_t i = 0; i + k < tc; i++) {
        if ((i == 0 || cab[i - 1] == '\n') && strncasecmp(cab + i, nome, k) == 0 && cab[i + k] == ':') {
            size_t j = i + k + 1; while (j < tc && cab[j] == ' ') j++;
            int n = 0; while (j < tc && cab[j] != '\r' && cab[j] != '\n' && n < tam - 1) saida[n++] = cab[j++];
            saida[n] = 0;
            return true;
        }
    }
    return false;
}
// Abre a conexão na posição stLinkPos. Segue até 6 saltos (endereço relativo ou completo).
static bool stLinkAbrirConexao() {
    stLinkFecharConexao();
    static char atual[2048];
    snprintf(atual, sizeof(atual), "%s", stLinkFinal[0] ? stLinkFinal : stLinkUrl);
    for (int salto = 0; salto < 6 && !stLinkParar; salto++) {
        stLinkTpl = sceHttpCreateTemplate(httpCtxId, stLinkUa[0] ? stLinkUa : ST_UA, ORBIS_HTTP_VERSION_1_1, 1);
        if (stLinkTpl < 0) { stLinkTpl = -1; return false; }
        sceHttpsSetSslCallback(stLinkTpl, skipSslCallback, NULL);
        typedef int (*DefinirRedirecionamento)(int, int);
        ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(stLinkTpl, 0);
        stLinkConn = sceHttpCreateConnectionWithURL(stLinkTpl, atual, 1);
        stLinkReq = stLinkConn >= 0 ? sceHttpCreateRequestWithURL(stLinkConn, ORBIS_METHOD_GET, atual, 0) : -1;
        if (stLinkReq < 0) { stLinkFecharConexao(); return false; }
        char faixa[48]; snprintf(faixa, sizeof(faixa), "bytes=%lld-", (long long)stLinkPos);
        sceHttpAddRequestHeader(stLinkReq, "Range", faixa, 0);
        sceHttpSetConnectTimeOut(stLinkReq, 8u * 1000000u);
        ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(stLinkReq, 20u * 1000000u);
        int rc = sceHttpSendRequest(stLinkReq, NULL, 0);
        int status = 0;
        if (rc >= 0) sceHttpGetStatusCode(stLinkReq, &status);
        if (rc < 0 || status <= 0) { stLog("LINK falhou 0x%08X %.120s", (unsigned)rc, atual); stLinkFecharConexao(); return false; }
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            char destino[2048];
            if (!stLinkCabecalho(stLinkReq, "location", destino, sizeof(destino)) || !destino[0]) { stLinkFecharConexao(); return false; }
            stLinkFecharConexao();
            if (!strncmp(destino, "http://", 7) || !strncmp(destino, "https://", 8)) snprintf(atual, sizeof(atual), "%s", destino);
            else {
                char base[2048]; snprintf(base, sizeof(base), "%s", atual);
                char* host = strstr(base, "://"); char* barra = host ? strchr(host + 3, '/') : NULL;
                if (destino[0] == '/') { if (barra) *barra = 0; snprintf(atual, sizeof(atual), "%s%s", base, destino); }
                else { char* ult = strrchr(base, '/'); if (ult && ult > host + 2) ult[1] = 0; snprintf(atual, sizeof(atual), "%s%s", base, destino); }
            }
            continue;
        }
        if (status != 200 && status != 206) { stLog("LINK status %d %.120s", status, atual); stLinkFecharConexao(); return false; }
        char valor[160];
        if (status == 206 && stLinkCabecalho(stLinkReq, "content-range", valor, sizeof(valor))) {
            const char* barra = strrchr(valor, '/');
            if (barra && barra[1] != '*') stLinkTam = atoll(barra + 1);
        }
        else if (status == 200 && stLinkCabecalho(stLinkReq, "content-length", valor, sizeof(valor))) stLinkTam = atoll(valor);
        // Servidor sem Range (200 a partir do zero): descarta até a posição pedida.
        if (status == 200 && stLinkPos > 0) {
            static uint8_t lixo[65536];
            int64_t faltam = stLinkPos;
            while (faltam > 0 && !stLinkParar) {
                int n = sceHttpReadData(stLinkReq, lixo, (int)(faltam < (int64_t)sizeof(lixo) ? faltam : (int64_t)sizeof(lixo)));
                if (n <= 0) { stLinkFecharConexao(); return false; }
                faltam -= n;
            }
        }
        snprintf(stLinkFinal, sizeof(stLinkFinal), "%s", atual);
        stLinkPosReq = stLinkPos;
        return true;
    }
    stLinkFecharConexao();
    return false;
}
static int stLinkLer(void*, uint8_t* destino, int tamanho) {
    for (int tentativa = 0; tentativa < 4; tentativa++) {
        if (stLinkParar) return AVERROR_EOF;
        if (stLinkTam >= 0 && stLinkPos >= stLinkTam) return AVERROR_EOF;
        if (stLinkReq < 0 || stLinkPosReq != stLinkPos) {
            if (!stLinkAbrirConexao()) { sceKernelUsleep(300000); continue; }
        }
        int n = sceHttpReadData(stLinkReq, destino, tamanho);
        if (n > 0) { stLinkPos += n; stLinkPosReq += n; return n; }
        stLinkFecharConexao();   // conexão caiu (ou o servidor fechou): reabre na mesma posição
        if (n == 0 && stLinkTam < 0) return AVERROR_EOF;
    }
    return AVERROR(EIO);
}
static int64_t stLinkSaltar(void*, int64_t posicao, int origem) {
    if (origem & AVSEEK_SIZE) return stLinkTam;
    origem &= ~AVSEEK_FORCE;
    if (origem == SEEK_SET) stLinkPos = posicao;
    else if (origem == SEEK_CUR) stLinkPos += posicao;
    else if (origem == SEEK_END && stLinkTam >= 0) stLinkPos = stLinkTam + posicao;
    else return -1;
    if (stLinkPos < 0) stLinkPos = 0;
    return stLinkPos;
}
static AVIOContext* stLinkAbrirFluxo() {
    if (stLinkAvio) { av_freep(&stLinkAvio->buffer); avio_context_free(&stLinkAvio); }
    stLinkFecharConexao();
    stLinkPos = 0; stLinkTam = -1; stLinkParar = false; stLinkFinal[0] = 0;
    if (!stLinkAbrirConexao()) { stLog("LINK nao abriu %.150s", stLinkUrl); return NULL; }
    unsigned char* memoria = (unsigned char*)av_malloc(262144);
    if (!memoria) { stLinkFecharConexao(); return NULL; }
    stLinkAvio = avio_alloc_context(memoria, 262144, 0, NULL, stLinkLer, NULL, stLinkSaltar);
    if (!stLinkAvio) { av_free(memoria); stLinkFecharConexao(); return NULL; }
    stLog("LINK aberto (%lld bytes): %.150s", (long long)stLinkTam, stLinkFinal);
    return stLinkAvio;
}

static void stTocarFonte(int i) {
    StFonte* f = &stFontes[i];
    extern void iniciarVideoMP4(const char* caminho);
    extern bool videoRodando;
    iptvAntesDoVideo();
    if (f->url[0]) {
        snprintf(iptvUaForcado, sizeof(iptvUaForcado), "%s", f->ua[0] ? f->ua : ST_UA);
        iptvCanalAbertoIdx = -1;
        iptvErro[0] = 0;
        // Lista HLS (canais de TV) continua no leitor do IPTV; arquivo (MP4/MKV) vai pelo leitor com Range.
        bool hls = iptvContemSemCase(f->url, ".m3u8");
        if (!hls) {
            snprintf(stLinkUrl, sizeof(stLinkUrl), "%s", f->url);
            snprintf(stLinkUa, sizeof(stLinkUa), "%s", f->ua[0] ? f->ua : ST_UA);
            stLog("TOCAR link %.150s", f->url);
        }
        iniciarVideoMP4(hls ? f->url : "stlink://video");
        extern char videoMotivoRecusa[160];
        if (!videoRodando && videoMotivoRecusa[0]) { snprintf(msgStatus, sizeof(msgStatus), "%s", videoMotivoRecusa); msgTimer = 500; }
        else if (!videoRodando) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: O LINK NAO ABRIU (%.80s). Tente outra fonte.", iptvErro[0] ? iptvErro : "fora do ar ou formato sem suporte"); msgTimer = 400; }
        return;
    }
    if (hn_torrent_iniciar("/data/HyperNeiva/torrent", "/data/HyperNeiva/baixado/torrent") != 0) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: TORRENT NAO INICIOU (veja torrent_log.txt)"); msgTimer = 400; return; }
    char erro[128] = "";
    if (hn_torrent_fluxo_abrir(f->hash, f->arquivo, ST_PASTA_ASSISTIR, erro, sizeof(erro)) != 0) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: TORRENT RECUSADO (%.80s)", erro); msgTimer = 400; return; }
    snprintf(stTorrentCaminho, sizeof(stTorrentCaminho), "torrent://%s/%d", f->hash, f->arquivo);
    stEsperandoTorrent = true;
    stEsperaInicioMs = stEsperaUltimoMs = remotoAgoraMs();
    snprintf(msgStatus, sizeof(msgStatus), "STREMIO: BUSCANDO FONTES DO TORRENT... (Bolinha cancela)"); msgTimer = 400;
    stLog("TORRENT %s arquivo %d", f->hash, f->arquivo);
}

// Instala um addon pelo endereço do manifest (teclado ou stremio-addons.net). 1 = ok.
static int stAdicionarAddon(const char* endereco) {
    char base[480]; stBaseDoManifest(endereco, base, sizeof(base));
    for (int i = 0; i < stTotalAddons; i++) if (!strcmp(stAddons[i].base, base)) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: %.80s JA ESTA INSTALADO", stAddons[i].nome); msgTimer = 300; return 0; }
    if (stTotalAddons >= ST_MAX_ADDONS) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: LIMITE DE %d ADDONS. REMOVA UM ANTES.", ST_MAX_ADDONS); msgTimer = 300; return 0; }
    StAddon* a = &stAddons[stTotalAddons];
    memset(a, 0, sizeof(*a));
    snprintf(a->base, sizeof(a->base), "%s", base);
    stLerManifesto(a);
    stTotalAddons++;
    stSalvarAddons();
    snprintf(msgStatus, sizeof(msgStatus), a->erro[0] ? "STREMIO: ADDON SALVO, MAS O MANIFEST NAO ABRIU" : "STREMIO: ADDON %.80s INSTALADO", a->nome); msgTimer = 300;
    return a->erro[0] ? 0 : 1;
}

// Chamado a cada quadro (processarControles): acompanha o torrent até ter o começo do vídeo.
void stremioTick() {
    if (stImeAberto) {
        char t[1200]; int r = iaTecladoResultado(t, sizeof(t));
        if (r >= 0) {
            stImeAberto = false;
            if (r == 1 && stImeModo == 1 && strlen(t) > 8) { stAdicionarAddon(t); if (stAtivo && stNivel == 0) stMostrar(); }
            else if (r == 1 && stImeModo == 2 && t[0]) {
                for (char* c = t; *c; c++) if (*c == ':') *c = ' ';
                char q[200], caminho[260]; stCodificar(t, q, sizeof(q));
                snprintf(caminho, sizeof(caminho), "/addons?query=%s", q);
                snprintf(stSiteTitulo, sizeof(stSiteTitulo), "Busca: %.60s", t);
                stSiteLerLista(caminho, false);
                stSelNivel[4] = sel; stOffNivel[4] = off;
                stNivel = 5; sel = 0; off = 0; stMostrar();
            }
        }
    }
    if (!stEsperandoTorrent) return;
    uint64_t agora = remotoAgoraMs();
    if (agora - stEsperaUltimoMs < 400) return;
    stEsperaUltimoMs = agora;
    HnTorrentFluxoInfo info;
    if (hn_torrent_fluxo_info(&info) != 0) { stEsperandoTorrent = false; return; }
    long long minimo = info.tamanho > 0 && info.tamanho < 4LL * 1024 * 1024 ? info.tamanho : 4LL * 1024 * 1024;
    int segundos = (int)((agora - stEsperaInicioMs) / 1000);
    if (info.erro[0]) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: ERRO NO TORRENT (%.80s)", info.erro); msgTimer = 400; hn_torrent_fluxo_fechar(1); stEsperandoTorrent = false; return; }
    if (!info.temMetadados || info.arquivo < 0) {
        snprintf(msgStatus, sizeof(msgStatus), "STREMIO: BUSCANDO O TORRENT... %d s, %d fontes (Bolinha cancela)", segundos, info.fontes); msgTimer = 400;
        if (segundos > 120) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: TORRENT SEM FONTES (ninguem compartilhando agora). Tente outra fonte."); msgTimer = 500; hn_torrent_fluxo_fechar(1); stEsperandoTorrent = false; }
        return;
    }
    if (info.continuo < minimo) {
        snprintf(msgStatus, sizeof(msgStatus), "STREMIO: PREPARANDO %.40s %lld%% (%.1f MB/s, %d fontes)", info.nome, info.continuo * 100 / (minimo > 0 ? minimo : 1), info.taxa / 1048576.0, info.fontes); msgTimer = 400;
        if (segundos > 300 && info.taxa < 20000) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: TORRENT LENTO DEMAIS. Tente outra fonte."); msgTimer = 500; hn_torrent_fluxo_fechar(1); stEsperandoTorrent = false; }
        return;
    }
    stEsperandoTorrent = false;
    stLog("TORRENT pronto: %s %lld bytes, %d s", info.nome, info.tamanho, segundos);
    extern void iniciarVideoMP4(const char* caminho);
    extern bool videoRodando;
    iniciarVideoMP4(stTorrentCaminho);
    if (!videoRodando) { snprintf(msgStatus, sizeof(msgStatus), "STREMIO: O VIDEO DO TORRENT NAO ABRIU (formato?)"); msgTimer = 400; hn_torrent_fluxo_fechar(1); }
}

// Player: leitor do arquivo do torrent (chamado pelo remotoAbrirFluxo do network.cpp).
static int stAvioLer(void*, uint8_t* destino, int tamanho) {
    extern bool videoRodando;
    for (;;) {
        if (stPararLeitura) return AVERROR_EOF;
        int r = hn_torrent_fluxo_ler(stPos, destino, tamanho, 400);
        if (r > 0) { stPos += r; return r; }
        if (r == 0) return AVERROR_EOF;
        if (r == -1) return AVERROR(EIO);
        // Ainda chegando. Na abertura a tela fica parada: desiste em 60 s em vez de travar de vez.
        if (!videoRodando && remotoAgoraMs() - stAbrindoDesdeMs > 60000) return AVERROR_EOF;
    }
}
static int64_t stAvioSaltar(void*, int64_t posicao, int origem) {
    if (origem & AVSEEK_SIZE) return stTamanho;
    origem &= ~AVSEEK_FORCE;
    if (origem == SEEK_SET) stPos = posicao;
    else if (origem == SEEK_CUR) stPos += posicao;
    else if (origem == SEEK_END) stPos = stTamanho + posicao;
    else return -1;
    if (stPos < 0) stPos = 0;
    return stPos;
}
AVIOContext* stremioAbrirFluxo(const char* caminho) {
    if (!strncmp(caminho, "stlink://", 9)) return stLinkUrl[0] ? stLinkAbrirFluxo() : NULL;
    if (strncmp(caminho, "torrent://", 10) != 0) return NULL;
    HnTorrentFluxoInfo info;
    if (hn_torrent_fluxo_info(&info) != 0 || info.tamanho <= 0) return NULL;
    if (stAvio) { av_freep(&stAvio->buffer); avio_context_free(&stAvio); }
    unsigned char* memoria = (unsigned char*)av_malloc(262144);
    if (!memoria) return NULL;
    stAvio = avio_alloc_context(memoria, 262144, 0, NULL, stAvioLer, NULL, stAvioSaltar);
    if (!stAvio) { av_free(memoria); return NULL; }
    stPos = 0; stTamanho = info.tamanho; stPararLeitura = false; stAbrindoDesdeMs = remotoAgoraMs();
    stLog("PLAYER abriu %s (%lld bytes)", caminho, (long long)stTamanho);
    return stAvio;
}
void stremioPararFluxo() {
    if (stAvio) stPararLeitura = true;
    if (stLinkAvio) { stLinkParar = true; if (stLinkReq >= 0) sceHttpAbortRequest(stLinkReq); }
}
void stremioFecharFluxo() {
    if (stLinkAvio) {
        av_freep(&stLinkAvio->buffer);
        avio_context_free(&stLinkAvio);
        stLinkFecharConexao();
        stLog("PLAYER fechou o link");
    }
    if (!stAvio) return;
    av_freep(&stAvio->buffer);
    avio_context_free(&stAvio);
    hn_torrent_fluxo_fechar(1);   // o que foi baixado para assistir não fica ocupando o disco
    stLog("PLAYER fechou o torrent");
}

// ------------------------------------------------------------------ ligação com a tela de Mídia
bool stremioAtivo() { return stAtivo; }
void stremioDesligar() { stAtivo = false; }
void stremioEntrar() {
    stAtivo = true;
    stCarregarAddons();
    stNivel = 0; sel = 0; off = 0;
    stMostrar();
}
void stremioSair() {
    stAtivo = false;
    extern char caminhoMidiaAtual[512]; extern void abrirPastaMidia(const char* caminho);
    char pasta[512]; snprintf(pasta, sizeof(pasta), "%s", caminhoMidiaAtual);
    abrirPastaMidia(pasta);
}
void stremioRepreencher() { if (stAtivo) stMostrar(); }
// Bolinha: volta um nível; na raiz, volta para a Mídia local. Cancela o torrent que está esperando.
bool stremioBolinha() {
    if (!stAtivo) return false;
    if (stEsperandoTorrent) { stEsperandoTorrent = false; hn_torrent_fluxo_fechar(1); snprintf(msgStatus, sizeof(msgStatus), "STREMIO: CANCELADO"); msgTimer = 120; return true; }
    if (stNivel == 0) { stremioSair(); return true; }
    if (stNivel == 4 || stNivel == 5) {
        stNivel = stNivel == 5 ? 4 : 0;
        sel = stSelNivel[stNivel]; off = stOffNivel[stNivel];
        stMostrar();
        return true;
    }
    if (stNivel == 3 && stTotalEpisodios > 0 && stMetaAtual >= 0 && !strcmp(stMetas[stMetaAtual].tipo, "series")) stNivel = 2;
    else if (stNivel >= 2) stNivel = 1;
    else stNivel = 0;
    sel = stSelNivel[stNivel]; off = stOffNivel[stNivel];
    if (stNivel == 1 && stCatAddon >= 0) snprintf(stTitulo, sizeof(stTitulo), "STREMIO > %.60s > %.60s (%d)", stAddons[stCatAddon].nome, stAddons[stCatAddon].cat[stCatIndice].nome, stTotalMetas);
    stMostrar();
    return true;
}
void stremioX() {
    if (!stAtivo || sel < 0 || sel >= stTotalItens) return;
    if (stEsperandoTorrent) return;
    StItem it = stItens[sel];
    stSelNivel[stNivel] = sel; stOffNivel[stNivel] = off;
    switch (it.tipo) {
    case ST_I_ADICIONAR: {
        extern int32_t global_uId;
        if (iaAbrirTecladoBase(global_uId, "Endereco do manifest.json do addon", 1000, "https://")) { stImeAberto = true; stImeModo = 1; }
        break;
    }
    case ST_I_SITE:
        stNivel = 4; sel = 0; off = 0; stMostrar();
        break;
    case ST_I_SITE_BUSCAR: {
        extern int32_t global_uId;
        if (iaAbrirTecladoBase(global_uId, "Nome do addon (ex.: fenixflix)", 60, "")) { stImeAberto = true; stImeModo = 2; }
        break;
    }
    case ST_I_SITE_CATEGORIA:
        snprintf(stSiteTitulo, sizeof(stSiteTitulo), "%s", stSiteCategorias[it.a][0]);
        stSiteLerLista(stSiteCategorias[it.a][1], false);
        stNivel = 5; sel = 0; off = 0; stMostrar();
        break;
    case ST_I_SITE_PROXIMA: {
        int antes = sel;
        char proxima[300]; snprintf(proxima, sizeof(proxima), "%s", stSiteProxima);
        stSiteLerLista(proxima, true);
        stMostrar(); sel = antes; if (sel >= totalItens) sel = totalItens - 1;
        break;
    }
    case ST_I_SITE_ADDON: {
        char manifesto[480];
        snprintf(msgStatus, sizeof(msgStatus), "STREMIO: INSTALANDO %.80s...", stSite[it.a].nome); msgTimer = 120;
        if (!stSiteManifesto(stSite[it.a].slug, manifesto, sizeof(manifesto))) {
            snprintf(msgStatus, sizeof(msgStatus), "STREMIO: %.60s PEDE CONFIGURACAO NO SITE (sem manifest direto)", stSite[it.a].nome); msgTimer = 400;
            break;
        }
        stAdicionarAddon(manifesto);
        char aviso[128]; snprintf(aviso, sizeof(aviso), "%s", msgStatus);
        int antes = sel; stMostrar(); sel = antes;
        snprintf(msgStatus, sizeof(msgStatus), "%s", aviso); msgTimer = 300;
        break;
    }
    case ST_I_REMOVER: {
        uint64_t agora = remotoAgoraMs();
        if (stRemoverConfirma == it.a && agora - stRemoverQuando < 5000) {
            for (int i = it.a; i < stTotalAddons - 1; i++) stAddons[i] = stAddons[i + 1];
            stTotalAddons--;
            stSalvarAddons();
            stRemoverConfirma = -1;
        }
        else { stRemoverConfirma = it.a; stRemoverQuando = agora; }
        stMostrar();
        break;
    }
    case ST_I_CATALOGO:
        stAbrirCatalogo(it.a, it.b, false);
        stNivel = 1; sel = 0; off = 0; stMostrar();
        break;
    case ST_I_MAIS: {
        int antes = sel;
        stAbrirCatalogo(stCatAddon, stCatIndice, true);
        stMostrar(); sel = antes; if (sel >= totalItens) sel = totalItens - 1;
        break;
    }
    case ST_I_META: {
        stMetaAtual = it.a;
        StMeta* m = &stMetas[it.a];
        if (!strcmp(m->tipo, "series") && stAbrirSerie(it.a)) { stNivel = 2; sel = 0; off = 0; stMostrar(); break; }
        stTotalEpisodios = 0;
        char tituloAntes[200]; snprintf(tituloAntes, sizeof(tituloAntes), "%s", stTitulo);
        stBuscarFontes(m->tipo, m->id, m->nome);
        // Sem fonte: avisa e fica na lista, em vez de abrir uma tela vazia.
        if (stTotalFontes == 0) { snprintf(stTitulo, sizeof(stTitulo), "%s", tituloAntes); snprintf(msgStatus, sizeof(msgStatus), "SEM FONTES PARA %.60s NOS ADDONS INSTALADOS", m->nome); msgTimer = 300; break; }
        stNivel = 3; sel = 0; off = 0; stMostrar();
        break;
    }
    case ST_I_EPISODIO: {
        StMeta* m = &stMetas[stMetaAtual];
        char tituloAntes[200]; snprintf(tituloAntes, sizeof(tituloAntes), "%s", stTitulo);
        stBuscarFontes(m->tipo, stEpisodios[it.a].id, stEpisodios[it.a].nome);
        if (stTotalFontes == 0) { snprintf(stTitulo, sizeof(stTitulo), "%s", tituloAntes); snprintf(msgStatus, sizeof(msgStatus), "SEM FONTES PARA %.60s", stEpisodios[it.a].nome); msgTimer = 300; break; }
        stNivel = 3; sel = 0; off = 0; stMostrar();
        break;
    }
    case ST_I_FONTE:
        stTocarFonte(it.a);
        break;
    default: break;
    }
}
#endif   // STREMIO_SO_JSON
