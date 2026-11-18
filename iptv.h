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

// tipo: 0 ao vivo, 1 filme (VOD), 2 série. Série: 'url' fica vazia e 'xtreamId' guarda o
// series_id (busca os episódios com get_series_info antes de poder tocar algo).
struct IptvCanal { char nome[96], logo[260], grupo[64], tvgId[64]; char url[700]; int tipo; long xtreamId; };
static IptvCanal* iptvCanais = NULL;          // malloc (IPTV_MAX_CANAIS)
static int iptvTotalCanais = 0;
static int iptvTipoAtual = 0;                 // aba atual: 0 ao vivo, 1 filmes, 2 séries
#define IPTV_MAX_EPISODIOS 500
struct IptvEpisodio { char nome[120]; char url[700]; };
static IptvEpisodio iptvEpisodios[IPTV_MAX_EPISODIOS];
static int iptvTotalEpisodios = 0;
static char iptvSerieNome[96] = "";
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
static int iptvSelPerfil = 0, iptvSelCategoria = 0, iptvSelCanal = 0, iptvOffCanal = 0, iptvOffCategoria = 0;
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
// Favoritos: lista de URLs por perfil (Triangulo marca/desmarca na lista de canais). Pais/idioma
// e qualidade (HD/SD) já vêm no nome e no tvg-id da maioria das listas — a busca por texto
// (Quadrado) já casa com os dois; "quando canal.tvgId tem .br/.pt etc." documenta isso embaixo.
#define IPTV_MAX_FAVORITOS 300
static char iptvFavoritos[IPTV_MAX_FAVORITOS][700];
static int iptvTotalFavoritos = 0;
static bool iptvSoFavoritos = false;

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
// Redirecionamento seguido à mão (iptvHttpGetBuf): o automático do sceHttp recebia o salto do
// jmp2.uk (Pluto TV) para um endereço de ~3.200 letras e o app caía (CE-34878-0) ao abrir o canal.
// Endereço de destino maior que isto não é pedido: a resposta 3xx volta para quem chamou.
#define IPTV_REDIR_MAX 2048
// Passo a passo da abertura no video_debug_log.txt (só as primeiras linhas de cada canal): o app
// caía (CE-34878-0) ao abrir Band/Pluto sem deixar rastro; isto mostra em que chamada parou.
extern void logVideoInfo(const char* formato, ...);
static int iptvPassosRestantes = 0;
#define IPTV_PASSO(...) do { if (iptvPassosRestantes > 0) { iptvPassosRestantes--; logVideoInfo(__VA_ARGS__); } } while (0)
static int iptvRedirLongo = 0;   // tamanho do último Location recusado por ser longo (0 = nenhum)
static int iptvHttpGetUma(const char* url, const char* userAgent, char** resp, size_t* tamResp, size_t tetoBytes, char* destino, size_t tamDestino) {
    *resp = NULL; *tamResp = 0; destino[0] = 0;
    IPTV_PASSO("IPTV http: pedindo %.150s", url);
    int tpl = sceHttpCreateTemplate(httpCtxId, userAgent && userAgent[0] ? userAgent : IPTV_UA_PADRAO, ORBIS_HTTP_VERSION_1_1, 1);
    IPTV_PASSO("IPTV http: template %d", tpl);
    if (tpl < 0) return tpl;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    typedef int (*DefinirRedirecionamento)(int, int);
    ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(tpl, 0);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
    int status = req < 0 ? (conn < 0 ? conn : req) : -1;
    IPTV_PASSO("IPTV http: conexao %d pedido %d", conn, req);
    if (req >= 0) {
        ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(req, 15u * 1000000u);
        sceHttpSetConnectTimeOut(req, 8u * 1000000u);
        int rc = sceHttpSendRequest(req, NULL, 0);
        IPTV_PASSO("IPTV http: enviado 0x%08X", (unsigned)rc);
        if (rc >= 0) {
            sceHttpGetStatusCode(req, &status);
            IPTV_PASSO("IPTV http: status %d", status);
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                char* cab = NULL; size_t tc = 0;
                if (sceHttpGetAllResponseHeaders(req, &cab, &tc) >= 0 && cab) {
                    for (size_t i = 0; i + 9 < tc; i++) {
                        if ((i == 0 || cab[i - 1] == '\n') && strncasecmp(cab + i, "location:", 9) == 0) {
                            size_t j = i + 9; while (j < tc && cab[j] == ' ') j++;
                            size_t total = 0; while (j + total < tc && cab[j + total] != '\r' && cab[j + total] != '\n') total++;
                            if (total < tamDestino) { memcpy(destino, cab + j, total); destino[total] = 0; }
                            else iptvRedirLongo = (int)total;
                            IPTV_PASSO("IPTV http: location com %u letras", (unsigned)total);
                            break;
                        }
                    }
                }
            }
            size_t cap = 65536, n = 0; char* buf = (char*)malloc(cap);
            char bloco[32768]; int lidos;
            while (buf && (lidos = sceHttpReadData(req, bloco, sizeof(bloco))) > 0) {
                if (n + lidos + 1 > cap) { size_t nova = cap * 2; char* nb = (char*)realloc(buf, nova); if (!nb) break; buf = nb; cap = nova; }
                memcpy(buf + n, bloco, lidos); n += lidos;
                if (tetoBytes && n > tetoBytes) break;
            }
            if (buf) { buf[n] = 0; *resp = buf; *tamResp = n; }
            IPTV_PASSO("IPTV http: corpo %u bytes", (unsigned)n);
        }
        else status = rc;
        sceHttpDeleteRequest(req);
    }
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    IPTV_PASSO("IPTV http: liberado, devolvendo %d", status);
    return status;
}
static int iptvHttpGetBuf(const char* url, const char* userAgent, char** resp, size_t* tamResp, size_t tetoBytes) {
    static char atual[IPTV_REDIR_MAX], destino[IPTV_REDIR_MAX], montado[IPTV_REDIR_MAX];
    iptvRedirLongo = 0;
    if (strlen(url) >= sizeof(atual)) { *resp = NULL; *tamResp = 0; return -1; }
    snprintf(atual, sizeof(atual), "%s", url);
    int status = -1;
    for (int salto = 0; salto < 6; salto++) {
        status = iptvHttpGetUma(atual, userAgent, resp, tamResp, tetoBytes, destino, sizeof(destino));
        if (!destino[0] || salto == 5) break;               // não é salto (ou saltos demais): devolve como veio
        if (*resp) { free(*resp); *resp = NULL; *tamResp = 0; }
        char* frag = strchr(destino, '#'); if (frag) *frag = 0;
        if (!strncmp(destino, "http://", 7) || !strncmp(destino, "https://", 8)) snprintf(montado, sizeof(montado), "%s", destino);
        else {
            const char* esquema = strstr(atual, "://");
            const char* host = esquema ? esquema + 3 : atual;
            const char* corte = destino[0] == '/' ? strchr(host, '/') : strrchr(host, '/');
            int n = corte ? (int)(corte - atual) + (destino[0] == '/' ? 0 : 1) : (int)strlen(atual);
            if (!corte && destino[0] != '/') { montado[0] = 0; snprintf(montado, sizeof(montado), "%s/", atual); n = (int)strlen(montado); }
            else snprintf(montado, sizeof(montado), "%.*s", n, atual);
            if (strlen(montado) + strlen(destino) >= sizeof(montado)) { iptvRedirLongo = (int)(strlen(montado) + strlen(destino)); break; }
            strncat(montado, destino, sizeof(montado) - strlen(montado) - 1);
        }
        snprintf(atual, sizeof(atual), "%s", montado);
    }
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
// M3U não separa "ao vivo/filme/série" por protocolo (tudo é só uma URL): classifica pelo
// nome da categoria, do jeito que a maioria das listas BR já nomeia.
static int iptvClassificarTipoM3U(const char* grupo) {
    static const char* filme[] = { "filme", "movie", "vod", NULL };
    static const char* serie[] = { "serie", "série", "series", "séries", "anime", "novela", "dorama", NULL };
    for (int i = 0; filme[i]; i++) if (iptvContemSemCase(grupo, filme[i])) return 1;
    for (int i = 0; serie[i]; i++) if (iptvContemSemCase(grupo, serie[i])) return 2;
    return 0;
}
// tipo: 0 ao vivo, 1 filme, 2 série (série de verdade do Xtream: url vazia, xtreamId = series_id)
static void iptvAdicionarCanalTipo(const char* nome, const char* grupo, const char* tvgId, const char* logo, const char* url, int tipo, long xtreamId) {
    if (iptvTotalCanais >= IPTV_MAX_CANAIS || (!url[0] && tipo != 2)) return;
    IptvCanal* c = &iptvCanais[iptvTotalCanais++];
    snprintf(c->nome, sizeof(c->nome), "%.95s", nome && nome[0] ? nome : "Canal");
    snprintf(c->grupo, sizeof(c->grupo), "%.63s", grupo && grupo[0] ? grupo : "Sem categoria");
    snprintf(c->tvgId, sizeof(c->tvgId), "%.63s", tvgId ? tvgId : "");
    snprintf(c->logo, sizeof(c->logo), "%.259s", logo ? logo : "");
    snprintf(c->url, sizeof(c->url), "%.699s", url ? url : "");
    c->tipo = tipo; c->xtreamId = xtreamId;
    // categorias são recalculadas por aba (iptvRecalcularCategorias), não aqui: uma lista M3U
    // mistura ao vivo/filme/série no mesmo array, e cada aba só mostra as categorias dela.
}
// Reconstrói iptvCategorias só com as categorias dos canais do tipo da aba atual.
static void iptvRecalcularCategorias() {
    iptvTotalCategorias = 0;
    for (int i = 0; i < iptvTotalCanais; i++) if (iptvCanais[i].tipo == iptvTipoAtual) iptvCategoriaAdicionar(iptvCanais[i].grupo);
    iptvOrdenarCategorias();
}
static void iptvAdicionarCanal(const char* nome, const char* grupo, const char* tvgId, const char* logo, const char* url) {
    iptvAdicionarCanalTipo(nome, grupo, tvgId, logo, url, iptvClassificarTipoM3U(grupo), -1);
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
            { char* ponto = strchr(grupo, ';'); if (ponto) *ponto = 0; }   // "Esportes;Familia" -> só a 1a (categoria principal)
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
// Busca categorias + itens de UM tipo (0 ao vivo, 1 filmes, 2 séries) na API Xtream Codes.
// Ao vivo e filmes têm URL direta; série não tem stream_id (só dá para tocar depois de
// escolher um episódio, via iptvXtreamEpisodios).
static bool iptvXtreamListarTipo(const IptvPerfil* p, int tipo) {
    static const char* acaoCat[3] = { "get_live_categories", "get_vod_categories", "get_series_categories" };
    static const char* acaoItens[3] = { "get_live_streams", "get_vod_streams", "get_series" };
    static const char* chaveId[3] = { "\"stream_id\"", "\"stream_id\"", "\"series_id\"" };
    char base[220]; iptvXtreamUrlBase(p, base, sizeof(base));
    char urlCat[400]; snprintf(urlCat, sizeof(urlCat), "%s/player_api.php?username=%s&password=%s&action=%s", base, p->xtreamUsuario, p->xtreamSenha, acaoCat[tipo]);
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
    char urlStr[400]; snprintf(urlStr, sizeof(urlStr), "%s/player_api.php?username=%s&password=%s&action=%s", base, p->xtreamUsuario, p->xtreamSenha, acaoItens[tipo]);
    char* rs; size_t ns;
    if (iptvHttpGetBuf(urlStr, p->userAgent, &rs, &ns, 48 * 1024 * 1024) < 0 || !rs) { snprintf(iptvErro, sizeof(iptvErro), "Sem resposta do servidor Xtream (lista)."); return false; }
    int total = 0;
    for (const char* p2 = rs; (p2 = strstr(p2, chaveId[tipo])) && total < IPTV_MAX_CANAIS; total++) {
        const char* objIni = iptvInicioObjeto(rs, p2);
        const char* fimObj = iptvFimObjeto(objIni);
        char nome[96] = "", logo[260] = "", epgId[64] = "", ext[16] = "mp4";
        iptvJsonTexto(objIni, "name", nome, sizeof(nome), fimObj);
        iptvJsonTexto(objIni, tipo == 2 ? "cover" : "stream_icon", logo, sizeof(logo), fimObj);
        if (tipo == 0) iptvJsonTexto(objIni, "epg_channel_id", epgId, sizeof(epgId), fimObj);
        if (tipo == 1) iptvJsonTexto(objIni, "container_extension", ext, sizeof(ext));
        long sid = iptvJsonNum(objIni, tipo == 2 ? "series_id" : "stream_id", fimObj);
        long catId = iptvJsonNum(objIni, "category_id", fimObj);
        char grupo[48] = "Sem categoria";
        for (int i = 0; i < nCat; i++) if (idsCat[i] == (int)catId) { snprintf(grupo, sizeof(grupo), "%s", categorias[i]); break; }
        char tvgId[64] = "";
        char url[700] = "";
        if (tipo == 0) { snprintf(url, sizeof(url), "%s/live/%s/%s/%ld.ts", base, p->xtreamUsuario, p->xtreamSenha, sid); snprintf(tvgId, sizeof(tvgId), "xt:%ld:%s", sid, epgId); }
        else if (tipo == 1) snprintf(url, sizeof(url), "%s/movie/%s/%s/%ld.%s", base, p->xtreamUsuario, p->xtreamSenha, sid, ext);
        // tipo == 2 (série): sem URL ainda, só o series_id em xtreamId
        iptvAdicionarCanalTipo(nome, grupo, tvgId, logo, url, tipo, sid);
        p2 = fimObj;
    }
    free(rs);
    return iptvTotalCanais > 0;
}
// Episódios de uma série (get_series_info): junta todas as temporadas num só histórico,
// com "1x03 - Nome" para não precisar de outra tela de temporadas.
static bool iptvXtreamEpisodios(const IptvPerfil* p, long seriesId) {
    iptvTotalEpisodios = 0;
    char base[220]; iptvXtreamUrlBase(p, base, sizeof(base));
    char url[400]; snprintf(url, sizeof(url), "%s/player_api.php?username=%s&password=%s&action=get_series_info&series_id=%ld", base, p->xtreamUsuario, p->xtreamSenha, seriesId);
    char* r; size_t n;
    if (iptvHttpGetBuf(url, p->userAgent, &r, &n, 8 * 1024 * 1024) < 0 || !r) { snprintf(iptvErro, sizeof(iptvErro), "Sem resposta do servidor (episodios)."); return false; }
    const char* eps = strstr(r, "\"episodes\"");
    const char* fimEps = eps ? iptvFimObjeto(strchr(eps, '{') ? strchr(eps, '{') : eps) : NULL;
    if (eps && fimEps) {
        for (const char* p2 = eps; (p2 = strstr(p2, "\"id\"")) && p2 < fimEps && iptvTotalEpisodios < IPTV_MAX_EPISODIOS; ) {
            const char* objIni = iptvInicioObjeto(eps, p2);
            const char* fimObj = iptvFimObjeto(objIni);
            long epId = iptvJsonNum(objIni, "id", fimObj);
            long season = iptvJsonNum(objIni, "season", fimObj);
            long numEp = iptvJsonNum(objIni, "episode_num", fimObj);
            char titulo[80] = "", ext[16] = "mp4";
            iptvJsonTexto(objIni, "title", titulo, sizeof(titulo), fimObj);
            iptvJsonTexto(objIni, "container_extension", ext, sizeof(ext), fimObj);
            if (epId > 0) {
                IptvEpisodio* e = &iptvEpisodios[iptvTotalEpisodios++];
                snprintf(e->nome, sizeof(e->nome), "%ldx%02ld - %s", season > 0 ? season : 1, numEp, titulo[0] ? titulo : "Episodio");
                snprintf(e->url, sizeof(e->url), "%s/series/%s/%s/%ld.%s", base, p->xtreamUsuario, p->xtreamSenha, epId, ext);
            }
            p2 = fimObj;
        }
    }
    free(r);
    return iptvTotalEpisodios > 0;
}
static long iptvSerieIdPendente = -1;
static void* iptvThreadEpisodios(void*) {
    if (iptvEditando >= 0) iptvXtreamEpisodios(&iptvPerfis[iptvEditando], iptvSerieIdPendente);
    iptvCarregandoRodando = false;
    return NULL;
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
    // Curto de propósito: isto roda no laço de abertura do vídeo, que trava a tela até devolver
    // algo. Um canal fora do ar precisa falhar em segundos, não em minutos.
    sceHttpSetConnectTimeOut(f->req, 6u * 1000000u);
    ((int (*)(int, unsigned int))sceHttpSetRecvTimeOut)(f->req, 8u * 1000000u);
    IPTV_PASSO("IPTV conexao: enviando %.150s", url);
    int rc = sceHttpSendRequest(f->req, NULL, 0);
    IPTV_PASSO("IPTV conexao: enviado 0x%08X", (unsigned)rc);
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
    int status = iptvHttpGetBuf(f->urlVariante[0] ? f->urlVariante : f->url, f->userAgent, &texto, &n, 2 * 1024 * 1024);
    IPTV_PASSO("IPTV lista: status %d, %u bytes, %s", status, (unsigned)n, texto && strstr(texto, "#EXTM3U") ? "com #EXTM3U" : "sem #EXTM3U");
    if (status < 0) { if (texto) free(texto); return false; }
    if (!texto) { texto = (char*)malloc(1); if (!texto) return false; texto[0] = 0; }
    // Resposta que não é lista (redirecionamento não seguido, 404, página HTML): antes cada linha
    // do HTML ("<html>", "<center>...") virava um "segmento" e o leitor tentava abrir lixo.
    // Na abertura (nenhum segmento visto ainda) desiste na hora com o motivo; com o canal já
    // tocando, conta só como uma falha da atualização (pode ser um soluço do servidor).
    if (status < 200 || status >= 300 || !strstr(texto, "#EXTM3U")) {
        free(texto);
        if (f->ultimaSeqVista >= 0) return false;
        if (status >= 300 && status < 400 && iptvRedirLongo) snprintf(iptvErro, sizeof(iptvErro), "O canal redireciona para um endereco longo demais (%d letras, ex.: Pluto TV): sem suporte.", iptvRedirLongo);
        else if (status >= 300 && status < 400) snprintf(iptvErro, sizeof(iptvErro), "O canal redireciona para um endereco que o PS4 nao seguiu (%d).", status);
        else if (status < 200 || status >= 300) snprintf(iptvErro, sizeof(iptvErro), "O servidor do canal respondeu %d (fora do ar ou bloqueado).", status);
        else snprintf(iptvErro, sizeof(iptvErro), "O endereco nao devolveu uma lista de video (HLS).");
        f->parar = true; return false;
    }
    if (strstr(texto, "#EXT-X-KEY") && strstr(texto, "METHOD=AES")) { snprintf(iptvErro, sizeof(iptvErro), "Lista criptografada (DRM): sem suporte."); free(texto); f->parar = true; return false; }
    // Áudio em lista separada (ex.: Pluto TV): o leitor só segue uma lista, tocaria sem som.
    if (!f->urlVariante[0] && strstr(texto, "#EXT-X-STREAM-INF")) {
        for (const char* m = strstr(texto, "#EXT-X-MEDIA:"); m; m = strstr(m + 1, "#EXT-X-MEDIA:")) {
            const char* fimLinha = strchr(m, '\n'); size_t tl = fimLinha ? (size_t)(fimLinha - m) : strlen(m);
            const char* tipo = strstr(m, "TYPE=AUDIO"); const char* uri = strstr(m, "URI=");
            if (tipo && uri && (size_t)(tipo - m) < tl && (size_t)(uri - m) < tl) {
                snprintf(iptvErro, sizeof(iptvErro), "Canal com audio em lista separada (ex.: Pluto TV): sem suporte.");
                free(texto); f->parar = true; return false;
            }
        }
    }
    // URL maior que o espaço guardado sairia cortada (o Pluto usa linhas de 2.500 letras).
    for (const char* l = texto; l && *l; ) {
        const char* fimLinha = strchr(l, '\n'); size_t tl = fimLinha ? (size_t)(fimLinha - l) : strlen(l);
        if (tl >= sizeof(f->urlVariante) - 1 && l[0] != '#') {
            snprintf(iptvErro, sizeof(iptvErro), "Enderecos do canal longos demais (%u letras): sem suporte.", (unsigned)tl);
            free(texto); f->parar = true; return false;
        }
        l = fimLinha ? fimLinha + 1 : NULL;
    }
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
// Esta função roda no laço de abertura do vídeo, que bloqueia a tela toda (nenhum botão
// responde) até ela devolver algo — por isso desiste rápido (poucos segundos) quando o canal
// está fora do ar, em vez de martelar o servidor por minutos com a tela travada.
#define IPTV_FALHAS_MAX 4
extern bool videoAberturaEstourou();   // video.cpp: prazo de abertura de canal (20 s) estourou
static int iptvFluxoLerDentro(IptvFluxo* f, uint8_t* buf, int tam);
static int iptvFluxoLer(void* opaque, uint8_t* buf, int tam) {
    IptvFluxo* f = (IptvFluxo*)opaque;
    int r = iptvFluxoLerDentro(f, buf, tam);
    if (r <= 0) IPTV_PASSO("IPTV ler: pedido de %d bytes devolveu %d%s", tam, r, f->parar ? " (parado)" : "");
    return r;
}
static int iptvFluxoLerDentro(IptvFluxo* f, uint8_t* buf, int tam) {
    if (f->parar || videoAberturaEstourou()) return AVERROR_EOF;
    if (!f->hls) {
        if (f->req < 0 && !iptvAbrirConexao(f, f->url, true)) { if (++f->falhasSeguidas > IPTV_FALHAS_MAX) return AVERROR_EOF; remotoDormir(250); return 0 < tam ? AVERROR(EAGAIN) : 0; }
        int n = sceHttpReadData(f->req, buf, tam);
        if (n > 0) { f->falhasSeguidas = 0; return n; }
        iptvFecharConexao(f);
        if (++f->falhasSeguidas > IPTV_FALHAS_MAX) return AVERROR_EOF;
        return AVERROR(EAGAIN);
    }
    // HLS
    for (int tentativas = 0; tentativas < 200; tentativas++) {
        if (f->parar || videoAberturaEstourou()) return AVERROR_EOF;
        if (f->req >= 0) {
            int n = sceHttpReadData(f->req, buf, tam);
            if (n > 0) { f->falhasSeguidas = 0; return n; }
            iptvFecharConexao(f);
            f->segAtual++;
            continue;
        }
        if (f->segAtual < f->totalSeg) {
            if (iptvAbrirConexao(f, f->segmentos[f->segAtual], false)) continue;
            f->segAtual++;   // segmento falhou: pula, mas conta para o limite de desistência
            if (++f->falhasSeguidas > IPTV_FALHAS_MAX) return AVERROR_EOF;
            continue;
        }
        // fila vazia: atualiza a playlist (sem martelar o servidor)
        uint64_t agora = remotoAgoraMs();
        if (agora < f->proximaAtualizacaoMs) { remotoDormir(200); continue; }
        f->proximaAtualizacaoMs = agora + 3000;
        if (f->totalSeg >= IPTV_MAX_SEG) { f->totalSeg = 0; f->segAtual = 0; }   // fila cheia: reaproveita o espaço
        bool achou = iptvAtualizarPlaylist(f, iptvPerfilAbertoP ? iptvPerfilAbertoP->codecPref : 0);
        if (f->parar) return AVERROR_EOF;
        if (!achou && ++f->falhasSeguidas > IPTV_FALHAS_MAX) return AVERROR_EOF;
        if (achou) f->falhasSeguidas = 0;
        else remotoDormir(400);
    }
    return AVERROR_EOF;
}
// video.cpp chama ANTES do avformat_open_input: só entrega o canal ao FFmpeg se já há dados
// chegando (lista lida e conexão aberta num segmento, ou o .ts contínuo conectado). Canal que
// recusa (403 do Band, 302 do Pluto) ou não responde (Agro Brasil) fazia o leitor devolver "fim"
// sem nenhum byte; o FFmpeg seguia sondando um fluxo vazio (pedidos de até 1 MB) e o app caía
// (CE-34878-0) antes de voltar. Assim o erro sai daqui, sem o FFmpeg.
bool iptvFluxoPronto() {
    IptvFluxo* f = &iptvFluxoAtual;
    if (!f->emUso) return false;
    bool pronto = false;
    for (int t = 0; t < 200; t++) {
        if (f->parar || videoAberturaEstourou()) break;
        if (f->req >= 0) { pronto = true; break; }
        if (!f->hls) {
            if (iptvAbrirConexao(f, f->url, true)) { pronto = true; break; }
            if (++f->falhasSeguidas > IPTV_FALHAS_MAX) break;
            remotoDormir(250); continue;
        }
        if (f->segAtual < f->totalSeg) {
            if (iptvAbrirConexao(f, f->segmentos[f->segAtual], false)) { pronto = true; break; }
            f->segAtual++;
            if (++f->falhasSeguidas > IPTV_FALHAS_MAX) break;
            continue;
        }
        uint64_t agora = remotoAgoraMs();
        if (agora < f->proximaAtualizacaoMs) { remotoDormir(200); continue; }
        f->proximaAtualizacaoMs = agora + 3000;
        bool achou = iptvAtualizarPlaylist(f, iptvPerfilAbertoP ? iptvPerfilAbertoP->codecPref : 0);
        if (f->parar) break;
        if (achou) f->falhasSeguidas = 0;
        else { if (++f->falhasSeguidas > IPTV_FALHAS_MAX) break; remotoDormir(400); }
    }
    IPTV_PASSO("IPTV pronto: %s", pronto ? "sim" : "nao");
    if (pronto) f->falhasSeguidas = 0;
    else if (!iptvErro[0]) snprintf(iptvErro, sizeof(iptvErro), "%s", videoAberturaEstourou() ? "O canal nao respondeu em 20 s (fora do ar ou lento)." : "O servidor do canal nao respondeu (fora do ar ou bloqueado).");
    return pronto;
}
// video.cpp chama isto ANTES de fechar o vídeo: solta uma leitura de rede que esteja bloqueada
// (como o Telegram faz com telegramPararFluxo), senão fechar esperaria até o timeout de rede.
void iptvPararFluxo() {
    iptvFluxoAtual.parar = true;
    if (iptvFluxoAtual.req >= 0) sceHttpAbortRequest(iptvFluxoAtual.req);
}
// Motivo da última falha do leitor (para o log de vídeo).
const char* iptvUltimoErro() { return iptvErro; }
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
    iptvPassosRestantes = 80;
    IPTV_PASSO("IPTV fluxo criado (%s)", hls ? "HLS" : "continuo");
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
// ---------------------------------------------------------------- favoritos (por perfil)
static void iptvFavoritosPath(const char* nomePerfil, char* out, int tam) {
    char slug[48]; iptvSlug(nomePerfil, slug, sizeof(slug));
    snprintf(out, tam, "%s/%s_favoritos.txt", IPTV_RAIZ, slug);
}
static void iptvSalvarFavoritos(const char* nomePerfil) {
    char caminho[300]; iptvFavoritosPath(nomePerfil, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "w"); if (!f) return;
    for (int i = 0; i < iptvTotalFavoritos; i++) fprintf(f, "%s\n", iptvFavoritos[i]);
    fclose(f);
}
static void iptvCarregarFavoritos(const char* nomePerfil) {
    iptvTotalFavoritos = 0;
    char caminho[300]; iptvFavoritosPath(nomePerfil, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "r"); if (!f) return;
    char l[700];
    while (fgets(l, sizeof(l), f) && iptvTotalFavoritos < IPTV_MAX_FAVORITOS) {
        l[strcspn(l, "\r\n")] = 0;
        if (l[0]) snprintf(iptvFavoritos[iptvTotalFavoritos++], sizeof(iptvFavoritos[0]), "%s", l);
    }
    fclose(f);
}
// Série não tem URL própria (só depois de escolher o episódio): usa uma chave sintética.
static void iptvChaveFavorito(const IptvCanal* c, char* out, int tam) {
    if (c->url[0]) snprintf(out, tam, "%s", c->url);
    else snprintf(out, tam, "serie:%ld:%s", c->xtreamId, c->nome);
}
static bool iptvEhFavorito(const char* url) {
    for (int i = 0; i < iptvTotalFavoritos; i++) if (!strcmp(iptvFavoritos[i], url)) return true;
    return false;
}
// Marca/desmarca; salva na hora (são poucos, não pesa).
static void iptvAlternarFavorito(const char* url) {
    for (int i = 0; i < iptvTotalFavoritos; i++) if (!strcmp(iptvFavoritos[i], url)) {
        for (int k = i; k < iptvTotalFavoritos - 1; k++) snprintf(iptvFavoritos[k], sizeof(iptvFavoritos[0]), "%s", iptvFavoritos[k + 1]);
        iptvTotalFavoritos--;
        iptvSalvarFavoritos(iptvPerfis[iptvEditando].nome);
        return;
    }
    if (iptvTotalFavoritos < IPTV_MAX_FAVORITOS) { snprintf(iptvFavoritos[iptvTotalFavoritos++], sizeof(iptvFavoritos[0]), "%s", url); iptvSalvarFavoritos(iptvPerfis[iptvEditando].nome); }
}

// M3U guarda os 3 tipos juntos num só cache (uma só busca); Xtream busca cada tipo com uma
// chamada de API diferente, então cada aba tem seu próprio arquivo de cache.
static void iptvCanalCachePath(const char* nomePerfil, int tipoXtream, char* out, int tam) {
    char slug[48]; iptvSlug(nomePerfil, slug, sizeof(slug));
    static const char* suf[3] = { "_live", "_vod", "_series" };
    if (tipoXtream < 0) snprintf(out, tam, "%s/%s.txt", IPTV_RAIZ, slug);
    else snprintf(out, tam, "%s/%s%s.txt", IPTV_RAIZ, slug, suf[tipoXtream]);
}
static void iptvSalvarCacheCanais(const char* nomePerfil, int tipoXtream) {
    char caminho[300]; iptvCanalCachePath(nomePerfil, tipoXtream, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "w"); if (!f) return;
    for (int i = 0; i < iptvTotalCanais; i++) { IptvCanal* c = &iptvCanais[i]; fprintf(f, "%s\t%s\t%s\t%s\t%s\t%d\t%ld\n", c->nome, c->grupo, c->tvgId, c->logo, c->url, c->tipo, c->xtreamId); }
    fclose(f);
}
static bool iptvCarregarCacheCanais(const char* nomePerfil, int tipoXtream) {
    char caminho[300]; iptvCanalCachePath(nomePerfil, tipoXtream, caminho, sizeof(caminho));
    FILE* f = fopen(caminho, "r"); if (!f) return false;
    iptvTotalCanais = 0;
    char l[1300];
    while (fgets(l, sizeof(l), f) && iptvTotalCanais < IPTV_MAX_CANAIS) {
        l[strcspn(l, "\r\n")] = 0;
        char* c[7]; int n = iptvDividirTabs(l, c, 7);
        if (n >= 5 && c[0][0]) iptvAdicionarCanalTipo(c[0], c[1], c[2], c[3], c[4], n >= 6 ? atoi(c[5]) : 0, n >= 7 ? atol(c[6]) : -1);
    }
    fclose(f);
    iptvRecalcularCategorias();
    return iptvTotalCanais > 0;
}

// ---------------------------------------------------------------- busca da lista (thread)
static void iptvBuscarAgora(IptvPerfil* p) {
    iptvErro[0] = 0;
    iptvTotalCanais = 0; iptvTotalCategorias = 0;
    bool ok = false;
    if (p->tipo == 1) {
        ok = iptvXtreamListarTipo(p, iptvTipoAtual);
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
    if (ok) {
        iptvRecalcularCategorias();
        iptvSalvarCacheCanais(p->nome, p->tipo == 1 ? iptvTipoAtual : -1);
        int nTipo = 0; for (int i = 0; i < iptvTotalCanais; i++) if (iptvCanais[i].tipo == iptvTipoAtual) nTipo++;
        snprintf(iptvAviso, sizeof(iptvAviso), "%d itens em %d categorias.", nTipo, iptvTotalCategorias);
    }
    else if (!iptvErro[0]) snprintf(iptvErro, sizeof(iptvErro), "A lista voltou vazia.");
    iptvCarregandoRodando = false;
}
static void* iptvThreadBuscar(void* arg) { iptvBuscarAgora((IptvPerfil*)arg); return NULL; }
static void iptvAtualizarLista() {
    if (iptvCarregandoRodando || iptvEditando < 0 || iptvEditando >= iptvTotalPerfis) return;
    iptvCarregandoRodando = true; iptvCarregando = true;
    if (!remotoCriarThread(iptvThreadBuscar, &iptvPerfis[iptvEditando])) iptvCarregandoRodando = false;
}

// ---------------------------------------------------------------- filtros (país e qualidade)
// País: o código no tvg-id ("Globo.br@HD" -> "br", "AELatinAmerica.us@Brazil" -> "us"). As listas
// públicas (iptv-org) não trazem idioma, mas trazem isso. Qualidade: "(720p)" no nome ou @HD/@SD.
static char iptvFiltroPais[8] = "";           // "" = todos
static int iptvFiltroQualidade = 0;           // 0 todas, 1 só HD (720p ou mais), 2 só SD
static void iptvPaisDoCanal(const IptvCanal* c, char* out, int tam) {
    out[0] = 0;
    const char* arroba = strchr(c->tvgId, '@');
    const char* fim = arroba ? arroba : c->tvgId + strlen(c->tvgId);
    const char* p = fim; while (p > c->tvgId && p[-1] != '.') p--;
    if (p == c->tvgId) return;                 // sem ponto: não tem país
    int n = (int)(fim - p); if (n < 2 || n > 3 || n >= tam) return;
    for (int i = 0; i < n; i++) { char ch = p[i]; if (!isalpha((unsigned char)ch)) { out[0] = 0; return; } out[i] = (char)tolower((unsigned char)ch); }
    out[n] = 0;
}
// 1 = HD, 0 = SD, -1 = não diz.
static int iptvEhHD(const IptvCanal* c) {
    for (const char* p = strchr(c->nome, '('); p; p = strchr(p + 1, '(')) {
        int linhas = atoi(p + 1);
        if (linhas > 0) { const char* q = p + 1; while (isdigit((unsigned char)*q)) q++; if (*q == 'p' || *q == 'i') return linhas >= 720 ? 1 : 0; }
    }
    const char* a = strchr(c->tvgId, '@');
    if (a) { if (!strcasecmp(a, "@SD")) return 0; if (!strcasecmp(a, "@HD") || !strcasecmp(a, "@FHD") || !strcasecmp(a, "@UHD")) return 1; }
    if (iptvContemSemCase(c->nome, " FHD") || iptvContemSemCase(c->nome, " HD") || iptvContemSemCase(c->nome, " 4K") || iptvContemSemCase(c->nome, "UHD")) return 1;
    if (iptvContemSemCase(c->nome, " SD")) return 0;
    return -1;
}
static bool iptvPassaFiltros(int i) {
    const IptvCanal* c = &iptvCanais[i];
    if (iptvFiltroPais[0]) { char pais[8]; iptvPaisDoCanal(c, pais, sizeof(pais)); if (strcmp(pais, iptvFiltroPais)) return false; }
    if (iptvFiltroQualidade == 1 && iptvEhHD(c) != 1) return false;
    if (iptvFiltroQualidade == 2 && iptvEhHD(c) == 1) return false;
    return true;
}
// Próximo país da aba atual (em ordem alfabética; depois do último volta para "todos").
static void iptvProximoPais() {
    char melhor[8] = "";
    for (int i = 0; i < iptvTotalCanais; i++) {
        if (iptvCanais[i].tipo != iptvTipoAtual) continue;
        char pais[8]; iptvPaisDoCanal(&iptvCanais[i], pais, sizeof(pais));
        if (!pais[0] || strcmp(pais, iptvFiltroPais) <= 0) continue;
        if (!melhor[0] || strcmp(pais, melhor) < 0) snprintf(melhor, sizeof(melhor), "%s", pais);
    }
    snprintf(iptvFiltroPais, sizeof(iptvFiltroPais), "%s", melhor);
}
static void iptvDescreverFiltros(char* out, int tam) {
    static const char* q[3] = { "", "   [so HD]", "   [so SD]" };
    char pais[24] = ""; if (iptvFiltroPais[0]) snprintf(pais, sizeof(pais), "   [pais: %s]", iptvFiltroPais);
    snprintf(out, tam, "%s%s", pais, q[iptvFiltroQualidade]);
}

// ---------------------------------------------------------------- reprodução
void iptvPararEpg() { iptvEpgPedidoCanal = -1; iptvEpgTexto[0] = 0; }
// O motor de vídeo não devolve erro: se depois de abrir ele não está rodando, o canal falhou
// (fora do ar, bloqueado no país, formato sem suporte ou demorou mais de 20 s). Antes a tela
// continuava dizendo "IPTV: nome" como se estivesse tocando.
static void iptvAvisarSeNaoAbriu(const char* nome) {
    extern bool videoRodando;
    if (videoRodando) { snprintf(msgStatus, sizeof(msgStatus), "IPTV: %.100s", nome); msgTimer = 180; return; }
    // O leitor (iptvAtualizarPlaylist) pode ter deixado o motivo exato: mantém.
    if (iptvErro[0]) { char motivo[200]; snprintf(motivo, sizeof(motivo), "%s", iptvErro); snprintf(iptvErro, sizeof(iptvErro), "Nao abriu \"%.50s\": %.140s", nome, motivo); }
    else snprintf(iptvErro, sizeof(iptvErro), "Nao abriu \"%.60s\": fora do ar, bloqueado ou formato sem suporte. Tente outro.", nome);
    snprintf(msgStatus, sizeof(msgStatus), "IPTV: CANAL NAO ABRIU (%.80s)", nome); msgTimer = 360;
}
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
    iptvErro[0] = 0;
    iniciarVideoMP4(c->url);
    iptvAvisarSeNaoAbriu(c->nome);
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
// Troca Ao vivo/Filmes/Séries (L2/R2 nas telas de categoria e canais). No M3U já tem tudo
// carregado numa busca só (só refiltra); no Xtream cada aba é uma busca/cache separada.
static void iptvSelecionarTipo(int tipo) {
    if (tipo < 0) tipo = 2; if (tipo > 2) tipo = 0;
    iptvTipoAtual = tipo;
    iptvSelCategoria = -1; iptvSelCanal = 0; iptvOffCanal = 0; iptvOffCategoria = 0; iptvFiltro[0] = 0;
    if (iptvEditando < 0) return;
    IptvPerfil* p = &iptvPerfis[iptvEditando];
    if (p->tipo == 1) { if (!iptvCarregarCacheCanais(p->nome, tipo)) iptvAtualizarLista(); }
    else iptvRecalcularCategorias();
}
static void iptvAbrirPerfil(int i) {
    iptvEditando = i; iptvTipoAtual = 0; iptvSelCategoria = -1; iptvSelCanal = 0; iptvOffCanal = 0; iptvOffCategoria = 0; iptvFiltro[0] = 0; iptvSoFavoritos = false;
    iptvCarregarFavoritos(iptvPerfis[i].nome);
    if (!iptvCanais) iptvCanais = (IptvCanal*)malloc(sizeof(IptvCanal) * IPTV_MAX_CANAIS);
    bool tevCache = iptvPerfis[i].tipo == 1 ? iptvCarregarCacheCanais(iptvPerfis[i].nome, 0) : iptvCarregarCacheCanais(iptvPerfis[i].nome, -1);
    if (!tevCache) iptvAtualizarLista();
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
        if (n & ORBIS_PAD_BUTTON_L2) iptvSelecionarTipo(iptvTipoAtual - 1);
        if (n & ORBIS_PAD_BUTTON_R2) iptvSelecionarTipo(iptvTipoAtual + 1);
        if (iptvSelCategoria < 0) {
            if (n & ORBIS_PAD_BUTTON_CIRCLE) { iptvTela = 0; return false; }
            // Quadrado: só favoritos desta aba (de qualquer categoria) - um filtro rápido, além
            // de categoria e busca por texto (que já casa com país/idioma no tvg-id e qualidade
            // tipo "720p"/"HD" no nome, porque é onde as listas já trazem isso).
            if (n & ORBIS_PAD_BUTTON_SQUARE) { iptvSoFavoritos = !iptvSoFavoritos; iptvSelCanal = 0; iptvOffCategoria = 0; }
            if (n & ORBIS_PAD_BUTTON_R1) { iptvProximoPais(); iptvSelCanal = 0; iptvOffCategoria = 0; }
            if (n & ORBIS_PAD_BUTTON_L1) { iptvFiltroQualidade = (iptvFiltroQualidade + 1) % 3; iptvSelCanal = 0; iptvOffCategoria = 0; }
            int total = iptvTotalCategorias + 1;   // 0 = "Todos"
            if (n & ORBIS_PAD_BUTTON_UP) iptvSelCanal = (iptvSelCanal + total - 1) % total;
            if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelCanal = (iptvSelCanal + 1) % total;
            if (n & ORBIS_PAD_BUTTON_CROSS) { iptvSelCategoria = iptvSelCanal; iptvSelCanal = 0; iptvOffCanal = 0; }
            if (n & ORBIS_PAD_BUTTON_TRIANGLE) iptvAtualizarLista();
            return false;
        }
        // dentro de uma categoria: O volta para a lista de categorias (não direto para os perfis)
        if (n & ORBIS_PAD_BUTTON_CIRCLE) { iptvSelCategoria = -1; iptvSelCanal = 0; iptvOffCategoria = 0; iptvFiltro[0] = 0; return false; }
        // monta a lista filtrada (categoria + texto + aba atual + favoritos) sob demanda
        int ids[IPTV_MAX_CANAIS]; int total = 0;
        const char* catNome = iptvSelCategoria == 0 ? NULL : iptvCategorias[iptvSelCategoria - 1];
        for (int i = 0; i < iptvTotalCanais && total < IPTV_MAX_CANAIS; i++) {
            if (iptvCanais[i].tipo != iptvTipoAtual) continue;
            if (catNome && strcmp(iptvCanais[i].grupo, catNome)) continue;
            if (iptvFiltro[0] && !iptvContemSemCase(iptvCanais[i].nome, iptvFiltro) && !iptvContemSemCase(iptvCanais[i].tvgId, iptvFiltro)) continue;
            if (iptvSoFavoritos) { char chave[700]; iptvChaveFavorito(&iptvCanais[i], chave, sizeof(chave)); if (!iptvEhFavorito(chave)) continue; }
            if (!iptvPassaFiltros(i)) continue;
            ids[total++] = i;
        }
        if (total > 0) {
            if (n & ORBIS_PAD_BUTTON_UP) iptvSelCanal = (iptvSelCanal + total - 1) % total;
            if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelCanal = (iptvSelCanal + 1) % total;
            if (n & ORBIS_PAD_BUTTON_L1) iptvSelCanal = iptvSelCanal >= 10 ? iptvSelCanal - 10 : 0;
            if (n & ORBIS_PAD_BUTTON_R1) iptvSelCanal = iptvSelCanal + 10 < total ? iptvSelCanal + 10 : total - 1;
            if (n & ORBIS_PAD_BUTTON_CROSS) {
                int idx = ids[iptvSelCanal];
                if (iptvCanais[idx].tipo == 2) {   // série: busca os episódios antes de poder tocar
                    snprintf(iptvSerieNome, sizeof(iptvSerieNome), "%s", iptvCanais[idx].nome);
                    iptvSerieIdPendente = iptvCanais[idx].xtreamId;
                    iptvCarregandoRodando = true;
                    if (!remotoCriarThread(iptvThreadEpisodios, NULL)) iptvCarregandoRodando = false;
                    iptvTela = 5; iptvSelCanal = 0; iptvOffCanal = 0;
                }
                else iptvAbrirCanal(idx);
            }
            if (n & ORBIS_PAD_BUTTON_TRIANGLE) { char chave[700]; iptvChaveFavorito(&iptvCanais[ids[iptvSelCanal]], chave, sizeof(chave)); iptvAlternarFavorito(chave); }
        }
        if (n & ORBIS_PAD_BUTTON_SQUARE) { if (iaAbrirTecladoBase(usuario, "Buscar por nome ou pais/idioma (ex.: br, HD)", 24, iptvFiltro)) iptvImeModo = 99; }
        return false;
    }
    // ---- 5: episódios de uma série
    if (iptvTela == 5) {
        if (n & ORBIS_PAD_BUTTON_CIRCLE) { iptvTela = 2; return false; }
        if (iptvCarregandoRodando) return false;
        if (iptvTotalEpisodios > 0) {
            if (n & ORBIS_PAD_BUTTON_UP) iptvSelCanal = (iptvSelCanal + iptvTotalEpisodios - 1) % iptvTotalEpisodios;
            if (n & ORBIS_PAD_BUTTON_DOWN) iptvSelCanal = (iptvSelCanal + 1) % iptvTotalEpisodios;
            if (n & ORBIS_PAD_BUTTON_CROSS) {
                IptvPerfil* p = &iptvPerfis[iptvEditando];
                extern void videoDefinirCacheRedeIptv(size_t); extern void videoDefinirCodecPreferidoIptv(int);
                extern void videoDefinirOpcoesHttpIptv(const char*, bool, const char*); extern void iniciarVideoMP4(const char*);
                size_t bytes = p->cacheMb <= 0 ? (2u * 1024 * 1024) : (size_t)p->cacheMb * 1024 * 1024;
                videoDefinirCacheRedeIptv(bytes); videoDefinirCodecPreferidoIptv(p->codecPref);
                videoDefinirOpcoesHttpIptv(p->userAgent[0] ? p->userAgent : IPTV_UA_PADRAO, p->timeoutRede, p->proxy);
                iptvErro[0] = 0;
                iniciarVideoMP4(iptvEpisodios[iptvSelCanal].url);
                iptvAvisarSeNaoAbriu(iptvEpisodios[iptvSelCanal].nome);
                conquistaGanhar("iptv");
            }
        }
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
    if (iptvTela == 5) {
        char cabEp[150]; snprintf(cabEp, sizeof(cabEp), "Episodios de %s", iptvSerieNome);
        desenharTexto(p, cabEp, 24, 50, 88, 0xFF8FA6BF);
        if (iptvCarregandoRodando) { desenharTexto(p, "Buscando episodios...", 24, 50, 130, 0xFFFFE08A); desenharTexto(p, "O volta", 20, 50, 1040, 0xFF8A96A3); return; }
        if (!iptvTotalEpisodios) { desenharTexto(p, "Nao achei episodios para esta serie.", 22, 50, 130, 0xFFFF8A80); desenharTexto(p, "O volta", 20, 50, 1040, 0xFF8A96A3); return; }
        int visEp = 15;
        int offEp = iptvSelCanal >= visEp ? iptvSelCanal - visEp + 1 : 0;
        int y = 140;
        for (int i = offEp; i < iptvTotalEpisodios && i < offEp + visEp; i++, y += 50) {
            bool sel = i == iptvSelCanal;
            if (sel) telaRet(p, 40, y - 8, 1100, 44, 0xFF24456B);
            desenharTexto(p, iptvEpisodios[i].nome, 24, 60, y, sel ? 0xFFFFFFFF : 0xFFC0CCD8);
        }
        desenharTexto(p, "X toca o episodio   O volta", 20, 50, 1040, 0xFF8A96A3);
        return;
    }
    // ---- categorias / canais
    static const char* nomeTipo[3] = { "Ao vivo", "Filmes", "Series" };
    if (iptvCarregandoRodando) { desenharTexto(p, "Buscando...", 24, 50, 100, 0xFFFFE08A); desenharTexto(p, "O volta", 20, 50, 1040, 0xFF8A96A3); return; }
    char filtros[60]; iptvDescreverFiltros(filtros, sizeof(filtros));
    char aba[220]; snprintf(aba, sizeof(aba), "%s   [ %s ]%s%s   L2/R2 troca", iptvPerfis[iptvEditando].nome, nomeTipo[iptvTipoAtual], iptvSoFavoritos ? "   [so favoritos]" : "", filtros);
    if (iptvSelCategoria < 0) {
        desenharTexto(p, aba, 24, 50, 88, 0xFF8FA6BF);
        if (iptvErro[0]) desenharTexto(p, iptvErro, 22, 50, 120, 0xFFFF8A80);
        else if (iptvAviso[0]) desenharTexto(p, iptvAviso, 22, 50, 120, 0xFF8FC8FF);
        auto contaFiltrado = [](int i) -> bool {
            if (iptvCanais[i].tipo != iptvTipoAtual) return false;
            if (!iptvPassaFiltros(i)) return false;
            if (!iptvSoFavoritos) return true;
            char chave[700]; iptvChaveFavorito(&iptvCanais[i], chave, sizeof(chave)); return iptvEhFavorito(chave);
        };
        int totalDaAba = 0; for (int i = 0; i < iptvTotalCanais; i++) if (contaFiltrado(i)) totalDaAba++;
        // "Todos" conta como o item 0, cada categoria é item i+1: a janela rola para acompanhar
        // iptvSelCanal (sem isso, descer passava da tela e não tinha como ver o item selecionado).
        int totalItensCat = iptvTotalCategorias + 1;
        int visCat = 15;
        if (iptvSelCanal < iptvOffCategoria) iptvOffCategoria = iptvSelCanal;
        if (iptvSelCanal >= iptvOffCategoria + visCat) iptvOffCategoria = iptvSelCanal - visCat + 1;
        int y = 170;
        for (int item = iptvOffCategoria; item < totalItensCat && item < iptvOffCategoria + visCat; item++, y += 56) {
            bool sel = iptvSelCanal == item;
            if (sel) telaRet(p, 40, y - 8, 1100, 48, 0xFF24456B);
            if (item == 0) {
                char l0[60]; snprintf(l0, sizeof(l0), "Todos (%d)", totalDaAba);
                desenharTexto(p, l0, 26, 60, y, sel ? 0xFFFFFFFF : 0xFF8FC8FF);
            }
            else {
                int i = item - 1;
                int qtd = 0; for (int k = 0; k < iptvTotalCanais; k++) if (contaFiltrado(k) && !strcmp(iptvCanais[k].grupo, iptvCategorias[i])) qtd++;
                char l[90]; snprintf(l, sizeof(l), "%s (%d)", iptvCategorias[i], qtd);
                desenharTexto(p, l, 26, 60, y, sel ? 0xFFFFFFFF : 0xFFC0CCD8);
            }
        }
        if (!totalDaAba && !iptvTotalCategorias) desenharTexto(p, iptvSoFavoritos ? "(nenhum favorito nesta aba)" : "(nada aqui ainda nesta aba)", 22, 60, 170, 0xFF8A96A3);
        desenharTexto(p, "X entra   Triangulo atualiza   Quadrado so favoritos   R1 pais   L1 HD/SD   L2/R2 troca aba   O volta", 20, 50, 1040, 0xFF8A96A3);
        return;
    }
    int ids[IPTV_MAX_CANAIS]; int total = 0;
    const char* catNome = iptvSelCategoria == 0 ? NULL : iptvCategorias[iptvSelCategoria - 1];
    for (int i = 0; i < iptvTotalCanais && total < IPTV_MAX_CANAIS; i++) {
        if (iptvCanais[i].tipo != iptvTipoAtual) continue;
        if (catNome && strcmp(iptvCanais[i].grupo, catNome)) continue;
        if (iptvFiltro[0] && !iptvContemSemCase(iptvCanais[i].nome, iptvFiltro) && !iptvContemSemCase(iptvCanais[i].tvgId, iptvFiltro)) continue;
        if (iptvSoFavoritos) { char chave[700]; iptvChaveFavorito(&iptvCanais[i], chave, sizeof(chave)); if (!iptvEhFavorito(chave)) continue; }
        if (!iptvPassaFiltros(i)) continue;
        ids[total++] = i;
    }
    char cab[220]; snprintf(cab, sizeof(cab), "%s  -  %s%s%s  -  %d item(ns)%s", nomeTipo[iptvTipoAtual], catNome ? catNome : "Todos", iptvFiltro[0] ? "  -  busca: " : "", iptvFiltro, total, filtros);
    desenharTexto(p, cab, 22, 50, 88, 0xFF8FA6BF);
    if (iptvErro[0]) desenharTexto(p, iptvErro, 22, 50, 1000, 0xFFFF8A80);
    if (iptvSelCanal >= total && total > 0) iptvSelCanal = total - 1;
    int visiveis = 15;
    if (iptvSelCanal < iptvOffCanal) iptvOffCanal = iptvSelCanal;
    if (iptvSelCanal >= iptvOffCanal + visiveis) iptvOffCanal = iptvSelCanal - visiveis + 1;
    int y = 130;
    for (int i = iptvOffCanal; i < total && i < iptvOffCanal + visiveis; i++, y += 56) {
        bool sel = i == iptvSelCanal;
        if (sel) telaRet(p, 40, y - 8, 1260, 48, 0xFF24456B);
        char chave[700]; iptvChaveFavorito(&iptvCanais[ids[i]], chave, sizeof(chave));
        char l[115]; snprintf(l, sizeof(l), "%s%s", iptvEhFavorito(chave) ? "* " : "", iptvCanais[ids[i]].nome);
        desenharTexto(p, l, 25, 60, y, sel ? 0xFFFFFFFF : (iptvEhFavorito(chave) ? 0xFFFFD54F : 0xFFC0CCD8));
    }
    if (!total) desenharTexto(p, "Nada encontrado.", 24, 60, 170, 0xFF8A96A3);
    desenharTexto(p, iptvTipoAtual == 2 ? "X abre os episodios   Triangulo favorito   Quadrado busca   L1/R1 pula 10   O volta" : "X toca   Triangulo favorito   Quadrado busca (nome/pais)   L1/R1 pula 10   O volta",
        20, 50, 1040, 0xFF8A96A3);
}
