// =========================================================================
// HYPER NEIVA IPTV: perfis (M3U/M3U8 ou Xtream Codes), lista de canais por
// categoria, EPG (guia de programação) e reprodução pelo motor de vídeo (video.cpp).
// Tudo em um cabeçalho, incluído pelo network.cpp depois do Meet (o build.bat
// tem lista fixa de .cpp: código novo entra em cabeçalhos existentes).
//
// HTTPS e HLS (.m3u8): o FFmpeg deste OpenOrbis não traz TLS (sem ff_https_protocol),
// então essas listas usam um leitor próprio por SSL/sceHttp (como o rádio e o Telegram
// já fazem), que baixa os segmentos e entrega um fluxo contínuo MPEG-TS ao FFmpeg.
// HTTP puro (.ts contínuo ou .m3u8 sem TLS) vai direto pelo FFmpeg (mais simples e robusto).
// =========================================================================
#pragma once
#include <orbis/Http.h>
extern "C" {
#include <libavformat/avformat.h>
}

extern int httpCtxId;
extern int skipSslCallback(int libsslId, unsigned int verifyErr, void* const sslCert[], int certNum, void* userArg);

#define IPTV_RAIZ "/data/HyperNeiva/iptv"
#define IPTV_PERFIS_TXT REMOTO_RAIZ "/configuracao/iptv_perfis.txt"
#define IPTV_MAX_PERFIS 20
#define IPTV_MAX_CANAIS 8000
#define IPTV_MAX_CATEGORIAS 300
#define IPTV_UA_PADRAO "HyperNeiva/1.0 (PlayStation 4)"

// ---------------------------------------------------------------- dados
struct IptvPerfil {
    char nome[40];
    int tipo;                    // 0 = M3U/M3U8, 1 = Xtream Codes
    char m3uUrl[500];
    char m3uArquivo[300];
    char xtreamServidor[200], xtreamUsuario[64], xtreamSenha[64];
    char epgUrl[500];
    char userAgent[128];
    int cacheMb;                 // 0 (desligado=2MB reais), 10, 20, 50, 100, 500
    bool timeoutRede;            // true = 15s (padrão), false = praticamente sem limite
    char proxy[128];             // host:porta, só vale para streams http simples
    int codecPref;               // 0 HEVC/H.265, 1 AVC/H.264, 2 AV1
};
static IptvPerfil iptvPerfis[IPTV_MAX_PERFIS];
static int iptvTotalPerfis = 0;

struct IptvCanal { char nome[96], logo[260], grupo[64], tvgId[64]; char url[700]; };
static IptvCanal* iptvCanais = NULL;          // malloc (IPTV_MAX_CANAIS)
static int iptvTotalCanais = 0;
static char iptvCategorias[IPTV_MAX_CATEGORIAS][64];
static int iptvTotalCategorias = 0;
static char iptvErro[200] = "";
static char iptvAviso[200] = "";
// Canal/perfil que está (ou acabou de ser pedido para) tocar — o leitor https/HLS usa isso
// para saber o User-Agent/tipo a aplicar quando video.cpp chamar iptvAbrirFluxo().
static int iptvCanalAbertoIdx = -1;
static IptvPerfil* iptvPerfilAbertoP = NULL;
static char iptvNomeCanalAberto[96] = "";

// ---------------------------------------------------------------- tela (estado)
// 0 lista de perfis, 1 editor de perfil, 2 categorias, 3 lista de canais
static int iptvTela = 0;
static int iptvSelPerfil = 0, iptvSelCategoria = 0, iptvSelCanal = 0, iptvOffCanal = 0;
static int iptvEditando = -1;                // -1 = perfil novo, senão índice em iptvPerfis
static IptvPerfil iptvEdit;
static int iptvSelCampo = 0;
static int iptvImeModo = 0;                   // 0 nenhum; 1..N = índice do campo de texto em edição
static bool iptvCarregando = false;
static volatile bool iptvCarregandoRodando = false;
static char iptvFiltro[40] = "";
static char iptvEpgTexto[260] = "";           // "agora: ..." do canal em destaque
static uint64_t iptvEpgPedidoMs = 0;
static int iptvEpgPedidoCanal = -1;

// ---------------------------------------------------------------- utilidades
static void iptvSlug(const char* in, char* out, int tam) {
    int k = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p && k < tam - 1; p++) {
        unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[k++] = (char)c;
        else if (c >= 'A' && c <= 'Z') out[k++] = (char)(c + 32);
        else if (k > 0 && out[k - 1] != '_') out[k++] = '_';
    }
    while (k > 0 && out[k - 1] == '_') k--;
    out[k] = 0;
    if (!out[0]) snprintf(out, tam, "perfil");
}
static void iptvTrim(char* s) {
    int n = (int)strlen(s); while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) s[--n] = 0;
    int i = 0; while (s[i] == ' ') i++; if (i) memmove(s, s + i, n - i + 1);
}
static bool iptvContemSemCase(const char* texto, const char* alvo) {
    if (!texto || !alvo || !*alvo) return false;
    size_t n = strlen(alvo);
    for (const char* p = texto; *p; p++) { size_t i = 0; while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)alvo[i])) i++; if (i == n) return true; }
    return false;
}

// ---------------------------------------------------------------- base64 (decode: Xtream manda título/descrição em base64)
static int iptvBase64Dec(const char* in, uint8_t* out, int cap) {
    static const int8_t T[256] = {}; // preenchido na 1a chamada
    static bool pronto = false;
    static int8_t tab[256];
    if (!pronto) {
        memset(tab, -1, sizeof(tab));
        const char* al = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) tab[(unsigned char)al[i]] = (int8_t)i;
        pronto = true;
    }
    (void)T;
    int val = 0, bits = 0, k = 0;
    for (const char* p = in; *p && *p != '='; p++) {
        int8_t d = tab[(unsigned char)*p]; if (d < 0) continue;
        val = (val << 6) | d; bits += 6;
        if (bits >= 8) { bits -= 8; if (k < cap - 1) out[k++] = (uint8_t)((val >> bits) & 0xFF); }
    }
    out[k] = 0;
    return k;
}

// ---------------------------------------------------------------- JSON tolerante (reaproveita a ideia do Meet)
static const char* iptvJsonValor(const char* t, const char* chave, const char* limite) {
    char k[48]; snprintf(k, sizeof(k), "\"%s\"", chave);
    for (const char* p = strstr(t, k); p && (!limite || p < limite); p = strstr(p + 1, k)) {
        const char* v = p + strlen(k);
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        if (*v != ':') continue;
        v++; while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        return v;
    }
    return NULL;
}
static void iptvJsonTexto(const char* t, const char* chave, char* out, int tam, const char* limite = NULL) {
    out[0] = 0;
    const char* v = iptvJsonValor(t, chave, limite);
    if (!v) return;
    if (*v == 'n') return;   // null
    bool aspas = *v == '"'; if (aspas) v++;
    int k = 0;
    for (; *v && k < tam - 1; v++) {
        if (aspas && *v == '"') break;
        if (!aspas && (*v == ',' || *v == '}' || *v == ' ')) break;
        if (*v == '\\' && v[1]) {
            v++;
            if (*v == 'u' && v[1] && v[2] && v[3] && v[4]) { v += 4; out[k++] = '?'; continue; }
            out[k++] = (*v == 'n' || *v == 'r' || *v == 't') ? ' ' : *v;
            continue;
        }
        out[k++] = *v;
    }
    out[k] = 0;
}
static long iptvJsonNum(const char* t, const char* chave, const char* limite = NULL) {
    const char* v = iptvJsonValor(t, chave, limite);
    if (!v) return -1;
    if (*v == '"') v++;
    return atol(v);
}
// Acha o fim do objeto JSON (chave abre em 'ini') para não vazar valores de objetos vizinhos.
static const char* iptvFimObjeto(const char* ini) {
    int prof = 0; bool aspas = false;
    for (const char* p = ini; *p; p++) {
        if (aspas) { if (*p == '\\' && p[1]) p++; else if (*p == '"') aspas = false; continue; }
        if (*p == '"') aspas = true;
        else if (*p == '{' || *p == '[') prof++;
        else if (*p == '}' || *p == ']') { prof--; if (prof <= 0) return p + 1; }
    }
    return ini + strlen(ini);
}
// Acha o '{' que abre o objeto que contém 'p' (ex.: achou "stream_id" no meio do objeto, mas o
// campo "name" vem antes dele no JSON real da Xtream — sem isto, a busca por "name" a partir
// de 'p' não acharia nada, ou pegaria o do objeto vizinho).
static const char* iptvInicioObjeto(const char* comeco, const char* p) {
    int prof = 0;
    for (const char* q = p; q >= comeco; q--) {
        if (*q == '}') prof++;
        else if (*q == '{') { if (prof == 0) return q; prof--; }
    }
    return p;
}

// ---------------------------------------------------------------- HTTP: GET bufferizado (sem o teto de 8MB do iaHttpGet)
static int iptvHttpGetBuf(const char* url, const char* userAgent, char** resp, size_t* tamResp, size_t tetoBytes) {
    *resp = NULL; *tamResp = 0;
    int tpl = sceHttpCreateTemplate(httpCtxId, userAgent && userAgent[0] ? userAgent : IPTV_UA_PADRAO, ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return tpl;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    typedef int (*DefinirRedirecionamento)(int, int);
    ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(tpl, 1);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
    int status = req < 0 ? (conn < 0 ? conn : req) : -1;
    if (req >= 0) {
        ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(req, 30u * 1000000u);
        sceHttpSetConnectTimeOut(req, 15u * 1000000u);
        int rc = sceHttpSendRequest(req, NULL, 0);
        if (rc >= 0) {
            sceHttpGetStatusCode(req, &status);
            size_t cap = 65536, n = 0; char* buf = (char*)malloc(cap);
            char bloco[32768]; int lidos;
            while (buf && (lidos = sceHttpReadData(req, bloco, sizeof(bloco))) > 0) {
                if (n + lidos + 1 > cap) { size_t nova = cap * 2; char* nb = (char*)realloc(buf, nova); if (!nb) break; buf = nb; cap = nova; }
                memcpy(buf + n, bloco, lidos); n += lidos;
                if (tetoBytes && n > tetoBytes) break;
            }
            if (buf) { buf[n] = 0; *resp = buf; *tamResp = n; }
        }
        else status = rc;
        sceHttpDeleteRequest(req);
    }
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return status;
}

// ---------------------------------------------------------------- categorias
static void iptvCategoriaAdicionar(const char* nome) {
    const char* n = (nome && nome[0]) ? nome : "Sem categoria";
    for (int i = 0; i < iptvTotalCategorias; i++) if (!strcmp(iptvCategorias[i], n)) return;
    if (iptvTotalCategorias < IPTV_MAX_CATEGORIAS) snprintf(iptvCategorias[iptvTotalCategorias++], 64, "%.63s", n);
}
static void iptvOrdenarCategorias() {
    for (int i = 1; i < iptvTotalCategorias; i++) for (int j = i; j > 0 && strcasecmp(iptvCategorias[j - 1], iptvCategorias[j]) > 0; j--) {
        char t[64]; memcpy(t, iptvCategorias[j - 1], 64); memcpy(iptvCategorias[j - 1], iptvCategorias[j], 64); memcpy(iptvCategorias[j], t, 64);
    }
}

// ---------------------------------------------------------------- parser M3U / M3U8
static void iptvAdicionarCanal(const char* nome, const char* grupo, const char* tvgId, const char* logo, const char* url) {
    if (iptvTotalCanais >= IPTV_MAX_CANAIS || !url || !url[0]) return;
    IptvCanal* c = &iptvCanais[iptvTotalCanais++];
    snprintf(c->nome, sizeof(c->nome), "%.95s", nome && nome[0] ? nome : "Canal");
    snprintf(c->grupo, sizeof(c->grupo), "%.63s", grupo && grupo[0] ? grupo : "Sem categoria");
    snprintf(c->tvgId, sizeof(c->tvgId), "%.63s", tvgId ? tvgId : "");
    snprintf(c->logo, sizeof(c->logo), "%.259s", logo ? logo : "");
    snprintf(c->url, sizeof(c->url), "%.699s", url);
    iptvCategoriaAdicionar(c->grupo);
}
static void iptvAtributoM3U(const char* linha, const char* chave, char* out, int tam) {
    out[0] = 0;
    char k[32]; snprintf(k, sizeof(k), "%s=\"", chave);
    const char* p = strstr(linha, k); if (!p) return;
    p += strlen(k);
    int n = 0; while (p[n] && p[n] != '"' && n < tam - 1) n++;
    snprintf(out, tam, "%.*s", n, p);
}
static void iptvParseM3U(char* texto) {
    char tvgName[96] = "", grupo[64] = "", tvgId[64] = "", logo[260] = "", display[96] = "";
    bool temExtinf = false;
    char* linha = texto;
    while (linha && *linha) {
        char* fim = strchr(linha, '\n');
        if (fim) *fim = 0;
        iptvTrim(linha);
        if (!strncmp(linha, "#EXTINF", 7)) {
            iptvAtributoM3U(linha, "tvg-name", tvgName, sizeof(tvgName));
            iptvAtributoM3U(linha, "group-title", grupo, sizeof(grupo));
            iptvAtributoM3U(linha, "tvg-id", tvgId, sizeof(tvgId));
            iptvAtributoM3U(linha, "tvg-logo", logo, sizeof(logo));
            const char* virgula = strrchr(linha, ',');
            if (virgula) snprintf(display, sizeof(display), "%.95s", virgula + 1);
            temExtinf = true;
        }
        else if (linha[0] && linha[0] != '#') {
            if (temExtinf) {
                char url[700]; snprintf(url, sizeof(url), "%.699s", linha);
                iptvAdicionarCanal(tvgName[0] ? tvgName : display, grupo, tvgId, logo, url);
            }
            tvgName[0] = grupo[0] = tvgId[0] = logo[0] = display[0] = 0; temExtinf = false;
        }
        linha = fim ? fim + 1 : NULL;
    }
}

// ---------------------------------------------------------------- Xtream Codes
static void iptvXtreamUrlBase(const IptvPerfil* p, char* out, int tam) {
    char serv[200]; snprintf(serv, sizeof(serv), "%s", p->xtreamServidor);
    int n = (int)strlen(serv); while (n > 0 && serv[n - 1] == '/') serv[--n] = 0;
    snprintf(out, tam, "%s", serv);
}
static bool iptvXtreamListar(const IptvPerfil* p) {
    char base[220]; iptvXtreamUrlBase(p, base, sizeof(base));
    char urlCat[400]; snprintf(urlCat, sizeof(urlCat), "%s/player_api.php?username=%s&password=%s&action=get_live_categories", base, p->xtreamUsuario, p->xtreamSenha);
    char* rc; size_t nc;
    if (iptvHttpGetBuf(urlCat, p->userAgent, &rc, &nc, 4 * 1024 * 1024) < 0 || !rc) { snprintf(iptvErro, sizeof(iptvErro), "Sem resposta do servidor Xtream (categorias)."); return false; }
    char categorias[800][48]; int idsCat[800]; int nCat = 0;
    for (const char* p2 = rc; (p2 = strstr(p2, "\"category_id\"")) && nCat < 800; ) {
        const char* objIni = iptvInicioObjeto(rc, p2);
        const char* fimObj = iptvFimObjeto(objIni);
        char nome[64] = ""; iptvJsonTexto(objIni, "category_name", nome, sizeof(nome), fimObj);
        long id = iptvJsonNum(objIni, "category_id", fimObj);
        snprintf(categorias[nCat], 48, "%.47s", nome); idsCat[nCat] = (int)id; nCat++;
        p2 = fimObj;
    }
    free(rc);
    char urlStr[400]; snprintf(urlStr, sizeof(urlStr), "%s/player_api.php?username=%s&password=%s&action=get_live_streams", base, p->xtreamUsuario, p->xtreamSenha);
    char* rs; size_t ns;
    if (iptvHttpGetBuf(urlStr, p->userAgent, &rs, &ns, 48 * 1024 * 1024) < 0 || !rs) { snprintf(iptvErro, sizeof(iptvErro), "Sem resposta do servidor Xtream (canais)."); return false; }
    int total = 0;
    for (const char* p2 = rs; (p2 = strstr(p2, "\"stream_id\"")) && total < IPTV_MAX_CANAIS; total++) {
        const char* objIni = iptvInicioObjeto(rs, p2);
        const char* fimObj = iptvFimObjeto(objIni);
        char nome[96] = "", logo[260] = "", epgId[64] = "";
        iptvJsonTexto(objIni, "name", nome, sizeof(nome), fimObj);
        iptvJsonTexto(objIni, "stream_icon", logo, sizeof(logo), fimObj);
        iptvJsonTexto(objIni, "epg_channel_id", epgId, sizeof(epgId), fimObj);
        long sid = iptvJsonNum(objIni, "stream_id", fimObj);
        long catId = iptvJsonNum(objIni, "category_id", fimObj);
        char grupo[48] = "Sem categoria";
        for (int i = 0; i < nCat; i++) if (idsCat[i] == (int)catId) { snprintf(grupo, sizeof(grupo), "%s", categorias[i]); break; }
        char url[700]; snprintf(url, sizeof(url), "%s/live/%s/%s/%ld.ts", base, p->xtreamUsuario, p->xtreamSenha, sid);
        // guarda o stream_id em tvgId prefixado, para o EPG curto (get_short_epg) saber qual pedir
        char tvgId[64]; snprintf(tvgId, sizeof(tvgId), "xt:%ld:%s", sid, epgId);
        iptvAdicionarCanal(nome, grupo, tvgId, logo, url);
        p2 = fimObj;
    }
    free(rs);
    return iptvTotalCanais > 0;
}
// Programa atual (EPG curto da Xtream): decodifica o título em base64.
static bool iptvXtreamEpgAgora(const IptvPerfil* p, long streamId, char* out, int tam) {
    char base[220]; iptvXtreamUrlBase(p, base, sizeof(base));
    char url[400]; snprintf(url, sizeof(url), "%s/player_api.php?username=%s&password=%s&action=get_short_epg&stream_id=%ld&limit=1", base, p->xtreamUsuario, p->xtreamSenha, streamId);
    char* r; size_t n;
    if (iptvHttpGetBuf(url, p->userAgent, &r, &n, 1024 * 1024) < 0 || !r) return false;
    const char* tit = strstr(r, "\"title\"");
    bool ok = false;
    if (tit) {
        char b64[300] = ""; iptvJsonTexto(tit, "title", b64, sizeof(b64));
        uint8_t dec[260]; int k = iptvBase64Dec(b64, dec, sizeof(dec));
        if (k > 0) { snprintf(out, tam, "%.*s", k, (char*)dec); ok = true; }
    }
    free(r);
    return ok;
}

// ---------------------------------------------------------------- EPG XMLTV (M3U): varredura leve, sob demanda
static void iptvXmlTexto(const char* bloco, const char* tag, char* out, int tam) {
    out[0] = 0;
    char a[24]; snprintf(a, sizeof(a), "<%s", tag);
    const char* p = strstr(bloco, a); if (!p) return;
    p = strchr(p, '>'); if (!p) return; p++;
    const char* fim = strstr(p, "</");
    int n = fim ? (int)(fim - p) : 0; if (n > tam - 1) n = tam - 1;
    snprintf(out, tam, "%.*s", n, p);
}
// Varre (em blocos, sem carregar tudo na RAM) o XMLTV procurando o programa "agora" do canal.
static bool iptvEpgXmltvAgora(const char* url, const char* userAgent, const char* tvgId, char* out, int tam) {
    if (!tvgId || !tvgId[0]) return false;
    int tpl = sceHttpCreateTemplate(httpCtxId, userAgent && userAgent[0] ? userAgent : IPTV_UA_PADRAO, ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    typedef int (*DefinirRedirecionamento)(int, int);
    ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(tpl, 1);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
    bool achou = false;
    if (req >= 0) {
        sceHttpSetConnectTimeOut(req, 15u * 1000000u);
        ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(req, 20u * 1000000u);
        if (sceHttpSendRequest(req, NULL, 0) >= 0) {
            char marca[80]; snprintf(marca, sizeof(marca), "channel=\"%.60s\"", tvgId);
            char janela[9000]; int usado = 0;
            size_t totalLido = 0; const size_t TETO = 20 * 1024 * 1024;
            time_t agora = time(NULL);
            int n;
            char bloco[8192];
            while (!achou && totalLido < TETO && (n = sceHttpReadData(req, bloco, sizeof(bloco))) > 0) {
                totalLido += n;
                int copiar = n; if (usado + copiar > (int)sizeof(janela) - 1) copiar = (int)sizeof(janela) - 1 - usado;
                if (copiar > 0) { memcpy(janela + usado, bloco, copiar); usado += copiar; }
                janela[usado] = 0;
                for (char* p = janela; (p = strstr(p, marca)); p++) {
                    char* inicioProg = p; while (inicioProg > janela && !(inicioProg[0] == '<' && !strncmp(inicioProg, "<programme", 10))) inicioProg--;
                    if (strncmp(inicioProg, "<programme", 10)) continue;
                    char* fimProg = strstr(inicioProg, "</programme>"); if (!fimProg) continue;
                    char ini[32] = "", fimA[32] = "";
                    { const char* a2 = strstr(inicioProg, "start=\""); if (a2 && a2 < fimProg) { a2 += 7; int k = 0; while (a2[k] && a2[k] != '"' && k < 31) k++; snprintf(ini, sizeof(ini), "%.*s", k, a2); } }
                    { const char* a2 = strstr(inicioProg, "stop=\""); if (a2 && a2 < fimProg) { a2 += 6; int k = 0; while (a2[k] && a2[k] != '"' && k < 31) k++; snprintf(fimA, sizeof(fimA), "%.*s", k, a2); } }
                    struct tm tmI = {}, tmF = {};
                    if (ini[0] && fimA[0] && sscanf(ini, "%4d%2d%2d%2d%2d%2d", &tmI.tm_year, &tmI.tm_mon, &tmI.tm_mday, &tmI.tm_hour, &tmI.tm_min, &tmI.tm_sec) == 6 &&
                        sscanf(fimA, "%4d%2d%2d%2d%2d%2d", &tmF.tm_year, &tmF.tm_mon, &tmF.tm_mday, &tmF.tm_hour, &tmF.tm_min, &tmF.tm_sec) == 6) {
                        tmI.tm_year -= 1900; tmI.tm_mon -= 1; tmF.tm_year -= 1900; tmF.tm_mon -= 1;
                        time_t ti = mktime(&tmI), tf = mktime(&tmF);
                        if (agora >= ti && agora < tf) {
                            char titulo[260] = ""; char buf2[8300]; snprintf(buf2, sizeof(buf2), "%.*s", (int)(fimProg - inicioProg), inicioProg);
                            iptvXmlTexto(buf2, "title", titulo, sizeof(titulo));
                            if (titulo[0]) { snprintf(out, tam, "%s", titulo); achou = true; break; }
                        }
                    }
                }
                // mantém só a cauda da janela (um programme pode cruzar o limite do bloco lido)
                if (usado > 4096) { memmove(janela, janela + usado - 4096, 4096); usado = 4096; }
            }
        }
        sceHttpDeleteRequest(req);
    }
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return achou;
}

// ---------------------------------------------------------------- leitor https / HLS (.m3u8)
// O FFmpeg deste OpenOrbis não tem TLS, então canais https (ou listas .m3u8, mesmo http, para
// não depender do demuxer nativo de HLS refazendo conexão a cada segmento) baixam por sceHttp
// e entregam um fluxo contínuo MPEG-TS ao FFmpeg (o truque comum para HLS "simples", sem DRM).
#define IPTV_MAX_SEG 48
struct IptvFluxo {
    bool hls, emUso;
    char url[700];                  // canal (.ts contínuo) ou playlist (.m3u8)
    char userAgent[160];
    char segmentos[IPTV_MAX_SEG][700]; int totalSeg, segAtual;
    char urlVariante[700];          // playlist resolvida (após master -> variante)
    long ultimaSeqVista;
    uint64_t proximaAtualizacaoMs;
    int tpl, conn, req;             // conexão sceHttp ativa (segmento atual ou .ts contínuo)
    volatile bool parar;
    int falhasSeguidas;
};
// Um só canal IPTV toca por vez (como o Telegram faz com tgFluxo): nada de malloc por abertura.
static IptvFluxo iptvFluxoAtual;
static AVIOContext* iptvAvioAtual = NULL;
static void iptvFecharConexao(IptvFluxo* f) {
    if (f->req >= 0) sceHttpDeleteRequest(f->req);
    if (f->conn >= 0) sceHttpDeleteConnection(f->conn);
    if (f->tpl >= 0) sceHttpDeleteTemplate(f->tpl);
    f->req = f->conn = f->tpl = -1;
}
static bool iptvAbrirConexao(IptvFluxo* f, const char* url, bool manterViva) {
    iptvFecharConexao(f);
    f->tpl = sceHttpCreateTemplate(httpCtxId, f->userAgent[0] ? f->userAgent : IPTV_UA_PADRAO, ORBIS_HTTP_VERSION_1_1, 1);
    if (f->tpl < 0) return false;
    sceHttpsSetSslCallback(f->tpl, skipSslCallback, NULL);
    typedef int (*DefinirRedirecionamento)(int, int);
    ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(f->tpl, 1);
    f->conn = sceHttpCreateConnectionWithURL(f->tpl, url, 0);
    if (f->conn < 0) { iptvFecharConexao(f); return false; }
    f->req = sceHttpCreateRequestWithURL(f->conn, ORBIS_METHOD_GET, url, 0);
    if (f->req < 0) { iptvFecharConexao(f); return false; }
    sceHttpAddRequestHeader(f->req, "Connection", manterViva ? "keep-alive" : "close", 0);
    sceHttpSetConnectTimeOut(f->req, 15u * 1000000u);
    ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(f->req, 20u * 1000000u);
    int rc = sceHttpSendRequest(f->req, NULL, 0);
    if (rc < 0) { iptvFecharConexao(f); return false; }
    int status = 0; sceHttpGetStatusCode(f->req, &status);
    if (status >= 300) { iptvFecharConexao(f); return false; }
    return true;
}
// Resolve uma URL de segmento/variante relativa à playlist que a listou.
static void iptvResolverUrl(const char* base, const char* alvo, char* out, int tam) {
    if (!strncmp(alvo, "http://", 7) || !strncmp(alvo, "https://", 8)) { snprintf(out, tam, "%s", alvo); return; }
    const char* esquema = strstr(base, "://");
    if (alvo[0] == '/' && esquema) {
        const char* host = esquema + 3; const char* barra = strchr(host, '/');
        int n = barra ? (int)(barra - base) : (int)strlen(base);
        snprintf(out, tam, "%.*s%s", n, base, alvo);
        return;
    }
    const char* ultimaBarra = strrchr(base, '/');
    int n = ultimaBarra ? (int)(ultimaBarra - base) + 1 : (int)strlen(base);
    snprintf(out, tam, "%.*s%s", n, base, alvo);
}
// Baixa e interpreta a playlist (resolve master->variante pela preferência de codec; coleta
// os segmentos novos da variante, por número de sequência, para listas em direto).
static bool iptvAtualizarPlaylist(IptvFluxo* f, int codecPref) {
    char* texto; size_t n;
    if (iptvHttpGetBuf(f->urlVariante[0] ? f->urlVariante : f->url, f->userAgent, &texto, &n, 2 * 1024 * 1024) < 0 || !texto) return false;
    if (strstr(texto, "#EXT-X-KEY") && strstr(texto, "METHOD=AES")) { snprintf(iptvErro, sizeof(iptvErro), "Lista criptografada (DRM): sem suporte."); free(texto); f->parar = true; return false; }
    if (!f->urlVariante[0] && strstr(texto, "#EXT-X-STREAM-INF")) {
        // master playlist: escolhe a variante pelo codec preferido, senão a de maior BANDWIDTH.
        // Percorre linha a linha (como o parser M3U): a linha de STREAM-INF só marca o que vem,
        // a URL chega na iteração seguinte — nunca "espia" e corta à frente, isso quebrava o laço.
        // Entre as variantes do codec preferido, fica com a de maior BANDWIDTH; só cai para a
        // maior BANDWIDTH geral se nenhuma bater com o codec (duas melhores "correndo" juntas).
        const char* alvo = codecPref == 1 ? "avc1" : codecPref == 2 ? "av01" : "hvc1";
        char melhorUrlAlvo[700] = "", melhorUrlGeral[700] = ""; long melhorBwAlvo = -1, melhorBwGeral = -1;
        bool esperandoUrl = false, pendenteAlvo = false; long pendenteBw = 0;
        char* linha = texto;
        while (linha && *linha) {
            char* fim = strchr(linha, '\n'); if (fim) *fim = 0;
            if (!strncmp(linha, "#EXT-X-STREAM-INF", 17)) {
                const char* bw = strstr(linha, "BANDWIDTH="); pendenteBw = bw ? atol(bw + 10) : 0;
                const char* codecsAttr = strstr(linha, "CODECS=\"");
                pendenteAlvo = codecsAttr && (iptvContemSemCase(codecsAttr, alvo) || (codecPref == 0 && iptvContemSemCase(codecsAttr, "hev1")));
                esperandoUrl = true;
            }
            else if (linha[0] && linha[0] != '#') {
                if (esperandoUrl) {
                    if (pendenteBw > melhorBwGeral) { iptvResolverUrl(f->url, linha, melhorUrlGeral, sizeof(melhorUrlGeral)); melhorBwGeral = pendenteBw; }
                    if (pendenteAlvo && pendenteBw > melhorBwAlvo) { iptvResolverUrl(f->url, linha, melhorUrlAlvo, sizeof(melhorUrlAlvo)); melhorBwAlvo = pendenteBw; }
                }
                esperandoUrl = false;
            }
            linha = fim ? fim + 1 : NULL;
        }
        free(texto);
        const char* melhorEscolhido = melhorUrlAlvo[0] ? melhorUrlAlvo : melhorUrlGeral;
        char melhorUrl[700]; snprintf(melhorUrl, sizeof(melhorUrl), "%s", melhorEscolhido);
        if (!melhorUrl[0]) { f->parar = true; return false; }
        snprintf(f->urlVariante, sizeof(f->urlVariante), "%s", melhorUrl);
        return iptvAtualizarPlaylist(f, codecPref);
    }
    long seq = 0; { const char* s = strstr(texto, "#EXT-X-MEDIA-SEQUENCE:"); if (s) seq = atol(s + 22); }
    int novos = 0;
    char* linha = texto; long i = seq;
    while (linha && *linha && f->totalSeg < IPTV_MAX_SEG) {
        char* fim = strchr(linha, '\n'); if (fim) *fim = 0;
        iptvTrim(linha);
        if (linha[0] && linha[0] != '#') {
            if (i > f->ultimaSeqVista) {
                iptvResolverUrl(f->urlVariante[0] ? f->urlVariante : f->url, linha, f->segmentos[f->totalSeg], sizeof(f->segmentos[0]));
                f->totalSeg++; f->ultimaSeqVista = i; novos++;
            }
            i++;
        }
        linha = fim ? fim + 1 : NULL;
    }
    free(texto);
    return novos > 0;
}
// Lê bytes para o FFmpeg: percorre segmentos (HLS) ou mantém a conexão contínua (.ts).
static int iptvFluxoLer(void* opaque, uint8_t* buf, int tam) {
    IptvFluxo* f = (IptvFluxo*)opaque;
    if (f->parar) return AVERROR_EOF;
    if (!f->hls) {
        if (f->req < 0 && !iptvAbrirConexao(f, f->url, true)) { if (++f->falhasSeguidas > 20) return AVERROR_EOF; remotoDormir(300); return 0 < tam ? AVERROR(EAGAIN) : 0; }
        int n = sceHttpReadData(f->req, buf, tam);
        if (n > 0) { f->falhasSeguidas = 0; return n; }
        iptvFecharConexao(f);
        if (++f->falhasSeguidas > 20) return AVERROR_EOF;
        return AVERROR(EAGAIN);
    }
    // HLS
    for (int tentativas = 0; tentativas < 400; tentativas++) {
        if (f->parar) return AVERROR_EOF;
        if (f->req >= 0) {
            int n = sceHttpReadData(f->req, buf, tam);
            if (n > 0) { f->falhasSeguidas = 0; return n; }
            iptvFecharConexao(f);
            f->segAtual++;
            continue;
        }
        if (f->segAtual < f->totalSeg) {
            if (iptvAbrirConexao(f, f->segmentos[f->segAtual], false)) continue;
            f->segAtual++;   // segmento falhou: pula
            continue;
        }
        // fila vazia: atualiza a playlist (sem martelar o servidor)
        uint64_t agora = remotoAgoraMs();
        if (agora < f->proximaAtualizacaoMs) { remotoDormir(200); continue; }
        f->proximaAtualizacaoMs = agora + 3000;
        if (f->totalSeg >= IPTV_MAX_SEG) { f->totalSeg = 0; f->segAtual = 0; }   // fila cheia: reaproveita o espaço
        extern int videoCodecPreferidoPendenteLeitura();
        bool achou = iptvAtualizarPlaylist(f, iptvPerfilAbertoP ? iptvPerfilAbertoP->codecPref : 0);
        if (f->parar) return AVERROR_EOF;
        if (!achou && ++f->falhasSeguidas > 30) return AVERROR_EOF;
        if (!achou) remotoDormir(500);
    }
    return AVERROR(EAGAIN);
}
// video.cpp chama isto ANTES de fechar o vídeo: solta uma leitura de rede que esteja bloqueada
// (como o Telegram faz com telegramPararFluxo), senão fechar esperaria até o timeout de rede.
void iptvPararFluxo() {
    iptvFluxoAtual.parar = true;
    if (iptvFluxoAtual.req >= 0) sceHttpAbortRequest(iptvFluxoAtual.req);
}
// Chamado depois, já com a leitura solta: libera a conexão e o próprio AVIOContext de verdade.
void iptvFecharFluxo() {
    if (!iptvFluxoAtual.emUso) return;
    iptvFecharConexao(&iptvFluxoAtual);
    iptvFluxoAtual.emUso = false;
    if (iptvAvioAtual) { av_freep(&iptvAvioAtual->buffer); avio_context_free(&iptvAvioAtual); }
}
// Chamado por video.cpp ao abrir um vídeo: devolve um AVIOContext próprio se a URL for https
// ou .m3u8 (TLS/HLS não dá pelo FFmpeg nativo aqui); NULL deixa o FFmpeg abrir direto (http puro).
AVIOContext* iptvAbrirFluxo(const char* caminho) {
    if (!caminho) return NULL;
    bool https = !strncmp(caminho, "https://", 8);
    bool hls = iptvContemSemCase(caminho, ".m3u8");
    if (!https && !hls) return NULL;
    memset(&iptvFluxoAtual, 0, sizeof(iptvFluxoAtual));
    IptvFluxo* f = &iptvFluxoAtual;
    snprintf(f->url, sizeof(f->url), "%s", caminho);
    const char* ua = (iptvPerfilAbertoP && iptvCanalAbertoIdx >= 0) ? iptvPerfilAbertoP->userAgent : IPTV_UA_PADRAO;
    snprintf(f->userAgent, sizeof(f->userAgent), "%s", ua[0] ? ua : IPTV_UA_PADRAO);
    f->hls = hls; f->emUso = true; f->req = f->conn = f->tpl = -1; f->ultimaSeqVista = -1;
    uint8_t* memoria = (uint8_t*)av_malloc(65536);
    if (!memoria) { f->emUso = false; return NULL; }
    AVIOContext* avio = avio_alloc_context(memoria, 65536, 0, f, iptvFluxoLer, NULL, NULL);
    if (!avio) { av_free(memoria); f->emUso = false; return NULL; }
    iptvAvioAtual = avio;
    return avio;
}

// ---------------------------------------------------------------- persistência dos perfis
static void iptvSalvarPerfis() {
    FILE* f = fopen(IPTV_PERFIS_TXT, "w"); if (!f) return;
    for (int i = 0; i < iptvTotalPerfis; i++) {
        IptvPerfil* p = &iptvPerfis[i];
        fprintf(f, "%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%s\t%d\n",
            p->nome, p->tipo, p->m3uUrl, p->m3uArquivo, p->xtreamServidor, p->xtreamUsuario, p->xtreamSenha,
            p->epgUrl, p->userAgent[0] ? p->userAgent : IPTV_UA_PADRAO, p->cacheMb, p->timeoutRede ? 1 : 0,
            p->proxy[0] ? p->proxy : "-", p->codecPref);
    }
    fclose(f);
}
// Separa por \t (campo vazio vira string vazia; sscanf com %[^\t] não sabe representar isso).
static int iptvDividirTabs(char* linha, char* campos[], int maxCampos) {
    int n = 0;
    char* p = linha;
    while (n < maxCampos) {
        campos[n++] = p;
        char* t = strchr(p, '\t');
        if (!t) break;
        *t = 0; p = t + 1;
    }
    return n;
}
static void iptvCarregarPerfis() {
    iptvTotalPerfis = 0;
    FILE* f = fopen(IPTV_PERFIS_TXT, "r"); if (!f) return;
    char l[1600];
    while (fgets(l, sizeof(l), f) && iptvTotalPerfis < IPTV_MAX_PERFIS) {
        l[strcspn(l, "\r\n")] = 0;
        char* c[13]; int n = iptvDividirTabs(l, c, 13);
        if (n < 13 || !c[0][0]) continue;
        IptvPerfil p; memset(&p, 0, sizeof(p));
        snprintf(p.nome, sizeof(p.nome), "%s", c[0]);
        p.tipo = atoi(c[1]);
        snprintf(p.m3uUrl, sizeof(p.m3uUrl), "%s", c[2]);
        snprintf(p.m3uArquivo, sizeof(p.m3uArquivo), "%s", c[3]);
        snprintf(p.xtreamServidor, sizeof(p.xtreamServidor), "%s", c[4]);
        snprintf(p.xtreamUsuario, sizeof(p.xtreamUsuario), "%s", c[5]);
        snprintf(p.xtreamSenha, sizeof(p.xtreamSenha), "%s", c[6]);
        snprintf(p.epgUrl, sizeof(p.epgUrl), "%s", c[7]);
        snprintf(p.userAgent, sizeof(p.userAgent), "%s", c[8][0] ? c[8] : IPTV_UA_PADRAO);
        p.cacheMb = atoi(c[9]);
        p.timeoutRede = atoi(c[10]) != 0;
        if (strcmp(c[11], "-")) snprintf(p.proxy, sizeof(p.proxy), "%s", c[11]);
        p.codecPref = atoi(c[12]);
        iptvPerfis[iptvTotalPerfis++] = p;
    }
    fclose(f);
}
static void iptvCanalCachePath(const char* nomePerfil, char* out, int tam) { char slug[48]; iptvSlug(nomePerfil, slug, sizeof(slug)); snprintf(out, tam, "%s/%s.txt", IPTV_RAIZ, slug); }
static void iptvSalvarCacheCanais(const char* nomePerfil) {
    char caminho[300]; iptvCanalCachePath(nomePerfil, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "w"); if (!f) return;
    for (int i = 0; i < iptvTotalCanais; i++) { IptvCanal* c = &iptvCanais[i]; fprintf(f, "%s\t%s\t%s\t%s\t%s\n", c->nome, c->grupo, c->tvgId, c->logo, c->url); }
    fclose(f);
}
static bool iptvCarregarCacheCanais(const char* nomePerfil) {
    char caminho[300]; iptvCanalCachePath(nomePerfil, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "r"); if (!f) return false;
    iptvTotalCanais = 0; iptvTotalCategorias = 0;
    char l[1300];
    while (fgets(l, sizeof(l), f) && iptvTotalCanais < IPTV_MAX_CANAIS) {
        l[strcspn(l, "\r\n")] = 0;
        char* c[5]; if (iptvDividirTabs(l, c, 5) >= 5 && c[0][0]) iptvAdicionarCanal(c[0], c[1], c[2], c[3], c[4]);
    }
    fclose(f);
    iptvOrdenarCategorias();
    return iptvTotalCanais > 0;
}

// ---------------------------------------------------------------- busca da lista (thread)
static void iptvBuscarAgora(IptvPerfil* p) {
    iptvErro[0] = 0;
    iptvTotalCanais = 0; iptvTotalCategorias = 0;
    bool ok = false;
    if (p->tipo == 1) {
        ok = iptvXtreamListar(p);
    }
    else if (p->m3uArquivo[0]) {
        FILE* f = fopen(p->m3uArquivo, "rb");
        if (!f) snprintf(iptvErro, sizeof(iptvErro), "Nao achei o arquivo: %s", p->m3uArquivo);
        else {
            fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
            char* buf = (char*)malloc(n + 1);
            if (buf) { fread(buf, 1, n, f); buf[n] = 0; iptvParseM3U(buf); free(buf); ok = iptvTotalCanais > 0; }
            fclose(f);
        }
    }
    else if (p->m3uUrl[0]) {
        char* r; size_t n;
        int st = iptvHttpGetBuf(p->m3uUrl, p->userAgent, &r, &n, 64 * 1024 * 1024);
        if (st < 0 || !r) snprintf(iptvErro, sizeof(iptvErro), "Nao consegui baixar a playlist (rede).");
        else if (st >= 300) { snprintf(iptvErro, sizeof(iptvErro), "Servidor respondeu HTTP %d.", st); free(r); }
        else { iptvParseM3U(r); free(r); ok = iptvTotalCanais > 0; }
    }
    else snprintf(iptvErro, sizeof(iptvErro), "Preencha a URL da playlist, o arquivo, ou os dados do Xtream Codes.");
    if (ok) { iptvOrdenarCategorias(); iptvSalvarCacheCanais(p->nome); snprintf(iptvAviso, sizeof(iptvAviso), "%d canais em %d categorias.", iptvTotalCanais, iptvTotalCategorias); }
    else if (!iptvErro[0]) snprintf(iptvErro, sizeof(iptvErro), "A lista voltou vazia.");
    iptvCarregandoRodando = false;
}
static void* iptvThreadBuscar(void* arg) { iptvBuscarAgora((IptvPerfil*)arg); return NULL; }
static void iptvAtualizarLista() {
    if (iptvCarregandoRodando || iptvEditando < 0 || iptvEditando >= iptvTotalPerfis) return;
    iptvCarregandoRodando = true; iptvCarregando = true;
    if (!remotoCriarThread(iptvThreadBuscar, &iptvPerfis[iptvEditando])) iptvCarregandoRodando = false;
}

// ---------------------------------------------------------------- reprodução
void iptvPararEpg() { iptvEpgPedidoCanal = -1; iptvEpgTexto[0] = 0; }
static void iptvAbrirCanal(int idx) {
    if (idx < 0 || idx >= iptvTotalCanais || iptvEditando < 0) return;
    IptvCanal* c = &iptvCanais[idx];
    IptvPerfil* p = &iptvPerfis[iptvEditando];
    extern void videoDefinirCacheRedeIptv(size_t);
    extern void videoDefinirCodecPreferidoIptv(int);
    extern void videoDefinirOpcoesHttpIptv(const char* userAgent, bool timeoutOn, const char* proxy);
    extern void iniciarVideoMP4(const char* caminho);
    size_t bytes = p->cacheMb <= 0 ? (2u * 1024 * 1024) : (size_t)p->cacheMb * 1024 * 1024;
    videoDefinirCacheRedeIptv(bytes);
    videoDefinirCodecPreferidoIptv(p->codecPref);
    videoDefinirOpcoesHttpIptv(p->userAgent[0] ? p->userAgent : IPTV_UA_PADRAO, p->timeoutRede, p->proxy);
    iptvCanalAbertoIdx = idx; iptvPerfilAbertoP = p;
    snprintf(iptvNomeCanalAberto, sizeof(iptvNomeCanalAberto), "%s", c->nome);
    iniciarVideoMP4(c->url);
    snprintf(msgStatus, sizeof(msgStatus), "IPTV: %s", c->nome); msgTimer = 180;
    conquistaGanhar("iptv");
}

// ---------------------------------------------------------------- tela: perfis
static void iptvNovoPerfil() {
    memset(&iptvEdit, 0, sizeof(iptvEdit));
    snprintf(iptvEdit.userAgent, sizeof(iptvEdit.userAgent), "%s", IPTV_UA_PADRAO);
    iptvEdit.cacheMb = 100; iptvEdit.timeoutRede = true; iptvEdit.codecPref = 0;
    iptvEditando = -1; iptvSelCampo = 0; iptvTela = 1;
}
static void iptvEditarPerfil(int i) { iptvEdit = iptvPerfis[i]; iptvEditando = i; iptvSelCampo = 0; iptvTela = 1; }
static void iptvAbrirPerfil(int i) {
    iptvEditando = i; iptvSelCategoria = -1; iptvSelCanal = 0; iptvOffCanal = 0; iptvFiltro[0] = 0;
    if (!iptvCanais) iptvCanais = (IptvCanal*)malloc(sizeof(IptvCanal) * IPTV_MAX_CANAIS);
    if (!iptvCarregarCacheCanais(iptvPerfis[i].nome)) iptvAtualizarLista();
    iptvTela = 2;
}
// Campos do editor: índice -> o que faz. Monta dinamicamente conforme o tipo.
#define IPTV_CAMPO_NOME 0
#define IPTV_CAMPO_TIPO 1
static int iptvTotalCampos() { return iptvEdit.tipo == 1 ? 11 : 10; }
void iptvAbrirTela() {
    extern MenuLevel menuAtual;
    if (!iptvTotalPerfis && !iptvCanais) iptvCarregarPerfis();
    menuAtual = MENU_IPTV; iptvTela = 0; iptvSelPerfil = 0; iptvErro[0] = iptvAviso[0] = 0;
}
bool iptvControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    uint32_t n = botoes & ~antigos;
    if (iptvImeModo) {
        char t[520]; int r = iaTecladoResultado(t, sizeof(t));
        if (r < 0) return false;
        int campo = iptvImeModo; iptvImeModo = 0;
        if (r == 1) {
            iptvTrim(t);
            switch (campo) {
                case 1: snprintf(iptvEdit.nome, sizeof(iptvEdit.nome), "%s", t); break;
                case 3: if (iptvEdit.tipo == 0) snprintf(iptvEdit.m3uUrl, sizeof(iptvEdit.m3uUrl), "%s", t); else snprintf(iptvEdit.xtreamServidor, sizeof(iptvEdit.xtreamServidor), "%s", t); break;
                case 4: if (iptvEdit.tipo == 0) snprintf(iptvEdit.m3uArquivo, sizeof(iptvEdit.m3uArquivo), "%s", t); else snprintf(iptvEdit.xtreamUsuario, sizeof(iptvEdit.xtreamUsuario), "%s", t); break;
                case 5: snprintf(iptvEdit.xtreamSenha, sizeof(iptvEdit.xtreamSenha), "%s", t); break;
                case 6: snprintf(iptvEdit.epgUrl, sizeof(iptvEdit.epgUrl), "%s", t); break;
                case 7: snprintf(iptvEdit.userAgent, sizeof(iptvEdit.userAgent), "%s", t[0] ? t : IPTV_UA_PADRAO); break;
                case 10: snprintf(iptvEdit.proxy, sizeof(iptvEdit.proxy), "%s", t); break;
                case 99: snprintf(iptvFiltro, sizeof(iptvFiltro), "%s", t); iptvSelCanal = 0; iptvOffCanal = 0; break;
            }
        }
        return false;
    }
    // ---- 0: lista de perfis
    if (iptvTela == 0) {
        int total = iptvTotalPerfis + 1;   // +1 = "Novo perfil"
        if (n & ORBIS_PAD_BUTTON_UP) iptvSelPerfil = (iptvSelPerfil + total - 1) % total;
        if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelPerfil = (iptvSelPerfil + 1) % total;
        if (n & ORBIS_PAD_BUTTON_CIRCLE) return true;
        if (n & ORBIS_PAD_BUTTON_CROSS) { if (iptvSelPerfil == iptvTotalPerfis) iptvNovoPerfil(); else iptvAbrirPerfil(iptvSelPerfil); }
        if ((n & ORBIS_PAD_BUTTON_TRIANGLE) && iptvSelPerfil < iptvTotalPerfis) iptvEditarPerfil(iptvSelPerfil);
        if ((n & ORBIS_PAD_BUTTON_SQUARE) && iptvSelPerfil < iptvTotalPerfis) {
            for (int i = iptvSelPerfil; i < iptvTotalPerfis - 1; i++) iptvPerfis[i] = iptvPerfis[i + 1];
            iptvTotalPerfis--; iptvSalvarPerfis(); if (iptvSelPerfil >= iptvTotalPerfis && iptvSelPerfil > 0) iptvSelPerfil--;
        }
        return false;
    }
    // ---- 1: editor de perfil
    if (iptvTela == 1) {
        int total = iptvTotalCampos();
        if (n & ORBIS_PAD_BUTTON_UP) iptvSelCampo = (iptvSelCampo + total - 1) % total;
        if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelCampo = (iptvSelCampo + 1) % total;
        if (n & ORBIS_PAD_BUTTON_CIRCLE) { iptvTela = 0; return false; }
        bool xtream = iptvEdit.tipo == 1;
        if (n & ORBIS_PAD_BUTTON_CROSS) {
            if (iptvSelCampo == 0) { if (iaAbrirTecladoBase(usuario, "Nome da playlist", 24, iptvEdit.nome)) iptvImeModo = 1; }
            else if (iptvSelCampo == 1) iptvEdit.tipo = iptvEdit.tipo ? 0 : 1;
            else if (iptvSelCampo == 2) { if (iaAbrirTecladoBase(usuario, xtream ? "Servidor (http://host:porta)" : "URL da playlist (m3u/m3u8)", 90, xtream ? iptvEdit.xtreamServidor : iptvEdit.m3uUrl)) iptvImeModo = 3; }
            else if (iptvSelCampo == 3) { if (iaAbrirTecladoBase(usuario, xtream ? "Usuario" : "Ou um arquivo local (opcional)", 60, xtream ? iptvEdit.xtreamUsuario : iptvEdit.m3uArquivo)) iptvImeModo = 4; }
            else if (iptvSelCampo == 4 && xtream) { if (iaAbrirTecladoBase(usuario, "Senha", 60, iptvEdit.xtreamSenha)) iptvImeModo = 5; }
            else if (iptvSelCampo == (xtream ? 5 : 4)) { if (iaAbrirTecladoBase(usuario, "URL do EPG (XMLTV, opcional)", 90, iptvEdit.epgUrl)) iptvImeModo = 6; }
            else if (iptvSelCampo == (xtream ? 6 : 5)) { if (iaAbrirTecladoBase(usuario, "User-Agent", 60, iptvEdit.userAgent)) iptvImeModo = 7; }
            else if (iptvSelCampo == (xtream ? 7 : 6)) { static const int op[] = { 0, 10, 20, 50, 100, 500 }; int i = 0; while (i < 5 && op[i] != iptvEdit.cacheMb) i++; iptvEdit.cacheMb = op[(i + 1) % 6]; }
            else if (iptvSelCampo == (xtream ? 8 : 7)) iptvEdit.timeoutRede = !iptvEdit.timeoutRede;
            else if (iptvSelCampo == (xtream ? 9 : 8)) { if (iaAbrirTecladoBase(usuario, "Proxy HTTP (host:porta, opcional)", 60, iptvEdit.proxy)) iptvImeModo = 10; }
            else if (iptvSelCampo == (xtream ? 10 : 9)) iptvEdit.codecPref = (iptvEdit.codecPref + 1) % 3;
        }
        if (n & ORBIS_PAD_BUTTON_TRIANGLE) {
            if (!iptvEdit.nome[0]) snprintf(iptvErro, sizeof(iptvErro), "De um nome a playlist.");
            else {
                int idx = iptvEditando;
                if (idx < 0) { if (iptvTotalPerfis >= IPTV_MAX_PERFIS) { snprintf(iptvErro, sizeof(iptvErro), "Limite de %d perfis.", IPTV_MAX_PERFIS); return false; } idx = iptvTotalPerfis++; }
                iptvPerfis[idx] = iptvEdit;
                iptvSalvarPerfis();
                iptvAbrirPerfil(idx);
            }
        }
        return false;
    }
    // ---- 2/3: categorias e canais (tela única: -1 = mostra categorias; senão lista os canais dela)
    if (iptvTela == 2) {
        if (iptvSelCategoria < 0) {
            if (n & ORBIS_PAD_BUTTON_CIRCLE) { iptvTela = 0; return false; }
            int total = iptvTotalCategorias + 1;   // 0 = "Todos os canais"
            if (n & ORBIS_PAD_BUTTON_UP) iptvSelCanal = (iptvSelCanal + total - 1) % total;
            if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelCanal = (iptvSelCanal + 1) % total;
            if (n & ORBIS_PAD_BUTTON_CROSS) { iptvSelCategoria = iptvSelCanal; iptvSelCanal = 0; iptvOffCanal = 0; }
            if (n & ORBIS_PAD_BUTTON_TRIANGLE) iptvAtualizarLista();
            return false;
        }
        // dentro de uma categoria: O volta para a lista de categorias (não direto para os perfis)
        if (n & ORBIS_PAD_BUTTON_CIRCLE) { iptvSelCategoria = -1; iptvSelCanal = 0; iptvFiltro[0] = 0; return false; }
        // monta a lista filtrada (categoria + texto) sob demanda
        int ids[IPTV_MAX_CANAIS]; int total = 0;
        const char* catNome = iptvSelCategoria == 0 ? NULL : iptvCategorias[iptvSelCategoria - 1];
        for (int i = 0; i < iptvTotalCanais && total < IPTV_MAX_CANAIS; i++) {
            if (catNome && strcmp(iptvCanais[i].grupo, catNome)) continue;
            if (iptvFiltro[0] && !iptvContemSemCase(iptvCanais[i].nome, iptvFiltro)) continue;
            ids[total++] = i;
        }
        if (total > 0) {
            if (n & ORBIS_PAD_BUTTON_UP) iptvSelCanal = (iptvSelCanal + total - 1) % total;
            if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelCanal = (iptvSelCanal + 1) % total;
            if (n & ORBIS_PAD_BUTTON_L1) iptvSelCanal = iptvSelCanal >= 10 ? iptvSelCanal - 10 : 0;
            if (n & ORBIS_PAD_BUTTON_R1) iptvSelCanal = iptvSelCanal + 10 < total ? iptvSelCanal + 10 : total - 1;
            if (n & ORBIS_PAD_BUTTON_CROSS) iptvAbrirCanal(ids[iptvSelCanal]);
        }
        if (n & ORBIS_PAD_BUTTON_SQUARE) { if (iaAbrirTecladoBase(usuario, "Buscar canal pelo nome", 24, iptvFiltro)) iptvImeModo = 99; }
        return false;
    }
    return false;
}

static const char* iptvNomeCache(const char* texto, bool vazio) { return (!texto || !texto[0]) ? (vazio ? "(nenhum)" : "") : texto; }
void iptvRenderizar(uint32_t* p) {
    telaFundo(p, 0xFF0F1216);
    desenharTexto(p, "Hyper Neiva IPTV", 40, 50, 50, 0xFFFFFFFF);
    if (iptvTela == 0) {
        desenharTexto(p, "Suas listas (M3U/M3U8 ou Xtream Codes).", 22, 50, 88, 0xFF8A96A3);
        int y = 150;
        for (int i = 0; i < iptvTotalPerfis; i++, y += 56) {
            bool sel = i == iptvSelPerfil;
            if (sel) telaRet(p, 40, y - 8, 1100, 48, 0xFF24456B);
            char l[90]; snprintf(l, sizeof(l), "%s  -  %s", iptvPerfis[i].nome, iptvPerfis[i].tipo ? "Xtream Codes" : "M3U/M3U8");
            desenharTexto(p, l, 26, 60, y, sel ? 0xFFFFFFFF : 0xFFC0CCD8);
        }
        bool selNovo = iptvSelPerfil == iptvTotalPerfis;
        if (selNovo) telaRet(p, 40, y - 8, 1100, 48, 0xFF24456B);
        desenharTexto(p, "+ Nova playlist", 26, 60, y, selNovo ? 0xFFFFFFFF : 0xFF8FC8FF);
        desenharTexto(p, "X abre   Triangulo edita   Quadrado apaga   O volta", 20, 50, 1040, 0xFF8A96A3);
        return;
    }
    if (iptvTela == 1) {
        bool xtream = iptvEdit.tipo == 1;
        const char* itens[11];
        char tipoStr[40], cacheStr[40], toStr[40], codecStr[40];
        snprintf(tipoStr, sizeof(tipoStr), "Tipo: %s", xtream ? "Xtream Codes" : "M3U / M3U8");
        if (iptvEdit.cacheMb <= 0) snprintf(cacheStr, sizeof(cacheStr), "Cache: Desligado");
        else snprintf(cacheStr, sizeof(cacheStr), "Cache: %d MB", iptvEdit.cacheMb);
        snprintf(toStr, sizeof(toStr), "Timeout de rede: %s", iptvEdit.timeoutRede ? "Ligado" : "Desligado");
        snprintf(codecStr, sizeof(codecStr), "Codec preferencial: %s", iptvEdit.codecPref == 0 ? "HEVC / H.265" : iptvEdit.codecPref == 1 ? "AVC / H.264" : "AV1");
        char nomeL[80], urlL[110], arqL[320], epgL[110], uaL[80], proxyL[80], senhaL[40];
        snprintf(nomeL, sizeof(nomeL), "Nome: %s", iptvEdit.nome[0] ? iptvEdit.nome : "(toque para digitar)");
        snprintf(urlL, sizeof(urlL), "%s: %s", xtream ? "Servidor" : "URL da playlist", iptvNomeCache(xtream ? iptvEdit.xtreamServidor : iptvEdit.m3uUrl, true));
        snprintf(arqL, sizeof(arqL), "%s: %s", xtream ? "Usuario" : "Arquivo local", iptvNomeCache(xtream ? iptvEdit.xtreamUsuario : iptvEdit.m3uArquivo, true));
        snprintf(senhaL, sizeof(senhaL), "Senha: %s", iptvEdit.xtreamSenha[0] ? "********" : "(nenhuma)");
        snprintf(epgL, sizeof(epgL), "URL do EPG (XMLTV): %s", iptvNomeCache(iptvEdit.epgUrl, true));
        snprintf(uaL, sizeof(uaL), "User-Agent: %s", iptvEdit.userAgent[0] ? iptvEdit.userAgent : IPTV_UA_PADRAO);
        snprintf(proxyL, sizeof(proxyL), "Proxy HTTP: %s", iptvNomeCache(iptvEdit.proxy, true));
        int k = 0;
        itens[k++] = nomeL; itens[k++] = tipoStr; itens[k++] = urlL; itens[k++] = arqL;
        if (xtream) itens[k++] = senhaL;
        itens[k++] = epgL; itens[k++] = uaL; itens[k++] = cacheStr; itens[k++] = toStr; itens[k++] = proxyL; itens[k++] = codecStr;
        int y = 140;
        for (int i = 0; i < k; i++, y += 56) {
            if (i == iptvSelCampo) telaRet(p, 40, y - 8, 1200, 48, 0xFF24456B);
            desenharTexto(p, itens[i], 24, 60, y, i == iptvSelCampo ? 0xFFFFFFFF : 0xFFC0CCD8);
        }
        if (iptvErro[0]) desenharTexto(p, iptvErro, 22, 50, 960, 0xFFFF8A80);
        desenharTexto(p, "X muda/edita o campo   Triangulo salva e busca a lista   O volta sem salvar", 20, 50, 1040, 0xFF8A96A3);
        return;
    }
    // ---- categorias / canais
    if (iptvCarregandoRodando) { desenharTexto(p, "Buscando a lista de canais...", 24, 50, 100, 0xFFFFE08A); desenharTexto(p, "O volta", 20, 50, 1040, 0xFF8A96A3); return; }
    if (iptvSelCategoria < 0) {
        desenharTexto(p, iptvPerfis[iptvEditando].nome, 24, 50, 88, 0xFF8FA6BF);
        if (iptvErro[0]) desenharTexto(p, iptvErro, 22, 50, 120, 0xFFFF8A80);
        else if (iptvAviso[0]) desenharTexto(p, iptvAviso, 22, 50, 120, 0xFF8FC8FF);
        int y = 170;
        bool selTudo = iptvSelCanal == 0;
        if (selTudo) telaRet(p, 40, y - 8, 1100, 48, 0xFF24456B);
        char l0[60]; snprintf(l0, sizeof(l0), "Todos os canais (%d)", iptvTotalCanais);
        desenharTexto(p, l0, 26, 60, y, selTudo ? 0xFFFFFFFF : 0xFF8FC8FF); y += 56;
        for (int i = 0; i < iptvTotalCategorias; i++, y += 56) {
            bool sel = iptvSelCanal == i + 1;
            if (sel) telaRet(p, 40, y - 8, 1100, 48, 0xFF24456B);
            int qtd = 0; for (int k = 0; k < iptvTotalCanais; k++) if (!strcmp(iptvCanais[k].grupo, iptvCategorias[i])) qtd++;
            char l[90]; snprintf(l, sizeof(l), "%s (%d)", iptvCategorias[i], qtd);
            desenharTexto(p, l, 26, 60, y, sel ? 0xFFFFFFFF : 0xFFC0CCD8);
        }
        desenharTexto(p, "X entra   Triangulo atualiza a lista   O volta", 20, 50, 1040, 0xFF8A96A3);
        return;
    }
    int ids[IPTV_MAX_CANAIS]; int total = 0;
    const char* catNome = iptvSelCategoria == 0 ? NULL : iptvCategorias[iptvSelCategoria - 1];
    for (int i = 0; i < iptvTotalCanais && total < IPTV_MAX_CANAIS; i++) {
        if (catNome && strcmp(iptvCanais[i].grupo, catNome)) continue;
        if (iptvFiltro[0] && !iptvContemSemCase(iptvCanais[i].nome, iptvFiltro)) continue;
        ids[total++] = i;
    }
    char cab[120]; snprintf(cab, sizeof(cab), "%s%s%s  -  %d canal(is)", catNome ? catNome : "Todos", iptvFiltro[0] ? "  -  busca: " : "", iptvFiltro, total);
    desenharTexto(p, cab, 22, 50, 88, 0xFF8FA6BF);
    if (iptvSelCanal >= total && total > 0) iptvSelCanal = total - 1;
    int visiveis = 15;
    if (iptvSelCanal < iptvOffCanal) iptvOffCanal = iptvSelCanal;
    if (iptvSelCanal >= iptvOffCanal + visiveis) iptvOffCanal = iptvSelCanal - visiveis + 1;
    int y = 130;
    for (int i = iptvOffCanal; i < total && i < iptvOffCanal + visiveis; i++, y += 56) {
        bool sel = i == iptvSelCanal;
        if (sel) telaRet(p, 40, y - 8, 1260, 48, 0xFF24456B);
        char l[110]; snprintf(l, sizeof(l), "%s", iptvCanais[ids[i]].nome);
        desenharTexto(p, l, 25, 60, y, sel ? 0xFFFFFFFF : 0xFFC0CCD8);
    }
    if (!total) desenharTexto(p, "Nenhum canal encontrado.", 24, 60, 170, 0xFF8A96A3);
    desenharTexto(p, "X toca   Quadrado busca por nome   L1/R1 pula 10   O volta as categorias", 20, 50, 1040, 0xFF8A96A3);
}
