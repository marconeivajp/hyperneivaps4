#define TELEGRAM_PASTA "/data/HyperNeivaTelegram"
#define TG_MAX_CONVERSAS 600
#define TG_MAX_PASTAS 14
#define TG_MAX_RESULTADOS 60
#define TG_MAX_MENSAGENS 1500
#define TG_MAX_DOWNLOADS 24
#define TG_MAX_USUARIOS 800
#define TG_COR(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

enum TgTela { TG_INICIANDO, TG_TELEFONE, TG_CODIGO, TG_SENHA, TG_SEM_SUPORTE, TG_CONVERSAS, TG_MENSAGENS };
enum TgModoIme { TG_IME_TELEFONE, TG_IME_CODIGO, TG_IME_SENHA, TG_IME_MENSAGEM, TG_IME_BUSCA };
enum TgMidia { TG_M_NADA, TG_M_FOTO, TG_M_VIDEO, TG_M_AUDIO };

// ordens: 0 lista principal, 1 arquivadas, 2 em diante as pastas.
struct TgConversa { long long id, data, lidoAte; long long ordens[TG_MAX_PASTAS + 2]; int naoLidas, foto; char titulo[160]; char ultima[260]; };
struct TgMensagem { long long id, autor, data; bool minha, figurinha; int arquivo, imgL, imgA, grande, midia, totalLinks, alturaCache[2]; char rotulo[32]; char resumo[48]; char arquivoNome[100]; char links[4][200]; char texto[1024]; };
struct TgPasta { int id; char nome[64]; };
enum TgEstadoDownload { TG_D_BAIXANDO, TG_D_COPIAR, TG_D_COPIANDO, TG_D_PRONTO, TG_D_ERRO };
struct TgDownload { int arquivo, estado; bool salvar, pkg; long long total, baixado, mensagem; char nome[120]; };
struct TgResultado { long long conversa, mensagem, data; char texto[200]; };
struct TgUsuario { long long id; char nome[64]; };

// TG_JSON_INICIO
// O TDLib sempre escreve os campos na mesma ordem, e aspas dentro de texto vêm escapadas: procurar
// a chave completa é seguro.
static const char* tgAcha(const char* inicio, const char* fim, const char* agulha) {
    const char* p = strstr(inicio, agulha);
    return (p && (!fim || p < fim)) ? p : NULL;
}

static bool tgNumero(const char* inicio, const char* fim, const char* chave, long long* saida) {
    const char* p = tgAcha(inicio, fim, chave);
    if (!p) return false;
    p += strlen(chave);
    if (*p == '"') p++;
    *saida = strtoll(p, NULL, 10);
    return true;
}

static int tgGravarUtf8(unsigned int cp, char* s) {
    if (cp < 0x80) { s[0] = (char)cp; return 1; }
    if (cp < 0x800) { s[0] = (char)(0xC0 | (cp >> 6)); s[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { s[0] = (char)(0xE0 | (cp >> 12)); s[1] = (char)(0x80 | ((cp >> 6) & 63)); s[2] = (char)(0x80 | (cp & 63)); return 3; }
    s[0] = (char)(0xF0 | (cp >> 18)); s[1] = (char)(0x80 | ((cp >> 12) & 63)); s[2] = (char)(0x80 | ((cp >> 6) & 63)); s[3] = (char)(0x80 | (cp & 63));
    return 4;
}

static unsigned int tgHex4(const char** p) {
    char hex[5] = { 0, 0, 0, 0, 0 };
    for (int i = 0; i < 4 && **p; i++) hex[i] = *(*p)++;
    return (unsigned int)strtoul(hex, NULL, 16);
}

static bool tgTexto(const char* inicio, const char* fim, const char* chave, char* saida, int tamanho) {
    saida[0] = '\0';
    const char* p = tgAcha(inicio, fim, chave);
    if (!p) return false;
    p += strlen(chave);
    int n = 0;
    while (*p && *p != '"') {
        char pedaco[4];
        int bytes = 1;
        if (*p == '\\') {
            p++;
            char e = *p;
            if (!e) break;
            p++;
            if (e == 'u') {
                unsigned int cp = tgHex4(&p);
                if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                    p += 2;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (tgHex4(&p) - 0xDC00);
                }
                bytes = tgGravarUtf8(cp, pedaco);
            }
            else pedaco[0] = e == 'n' ? '\n' : (e == 'r' || e == 't' || e == 'b' || e == 'f') ? ' ' : e;
        }
        else {
            unsigned char c = (unsigned char)*p;
            bytes = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            for (int i = 0; i < bytes; i++) { if (!*p) { bytes = i; break; } pedaco[i] = *p++; }
        }
        // Sem espaço para o caractere inteiro: para aqui e não deixa UTF-8 partido.
        if (n + bytes > tamanho - 1) break;
        memcpy(saida + n, pedaco, bytes);
        n += bytes;
    }
    saida[n] = '\0';
    return true;
}

// O teclado do PS4 entrega UTF-16: \uXXXX mantém acento e emoji sem converter para UTF-8.
static void tgEscapar(const uint16_t* texto, char* saida, int tamanho) {
    int n = 0;
    for (int i = 0; texto[i] && n < tamanho - 8; i++) {
        uint16_t c = texto[i];
        if (c == '"' || c == '\\') { saida[n++] = '\\'; saida[n++] = (char)c; }
        else if (c < 0x20 || c >= 0x7F) n += snprintf(saida + n, tamanho - n, "\\u%04x", c);
        else saida[n++] = (char)c;
    }
    saida[n] = '\0';
}

static bool tgTipo(const char* r, const char* tipo) {
    char chave[80];
    snprintf(chave, sizeof(chave), "{\"@type\":\"%s\"", tipo);
    return strncmp(r, chave, strlen(chave)) == 0;
}

static bool tgConteudoE(const char* conteudo, const char* tipo) {
    size_t n = strlen(tipo);
    return strncmp(conteudo, tipo, n) == 0 && conteudo[n] == '"';
}

// Das versões da foto, a maior que cabe em 800 pixels economiza download e memória.
static void tgLerFoto(const char* conteudo, const char* fim, TgMensagem* m) {
    const char* chave = "{\"@type\":\"photoSize\",\"type\":\"";
    long long melhor = 0, maior = 0;
    for (const char* p = tgAcha(conteudo, fim, chave); p; ) {
        const char* proximo = tgAcha(p + 1, fim, chave);
        long long id = 0, largura = 0, altura = 0;
        tgNumero(p, proximo, "\"photo\":{\"@type\":\"file\",\"id\":", &id);
        tgNumero(p, proximo, "\"width\":", &largura);
        tgNumero(p, proximo, "\"height\":", &altura);
        bool cabe = largura <= 800;
        if (id && (melhor == 0 || (cabe && largura > melhor))) {
            melhor = largura;
            m->arquivo = (int)id; m->imgL = (int)largura; m->imgA = (int)altura;
        }
        // A versão maior fica para o visor em tela cheia.
        if (id && largura >= maior) { maior = largura; m->grande = (int)id; m->midia = TG_M_FOTO; }
        p = proximo;
    }
}

static void tgLerAbrir(const char* conteudo, const char* fim, const char* chave, int midia, TgMensagem* m) {
    long long id = 0;
    if (tgNumero(conteudo, fim, chave, &id)) { m->grande = (int)id; m->midia = midia; }
}

static void tgLerMiniatura(const char* conteudo, const char* fim, TgMensagem* m) {
    const char* t = tgAcha(conteudo, fim, "\"thumbnail\":{\"@type\":\"thumbnail\"");
    if (!t) return;
    long long id = 0, largura = 0, altura = 0;
    const char* arquivo = tgAcha(t, fim, "\"file\":{\"@type\":\"file\",\"id\":");
    if (!arquivo || !tgNumero(arquivo, fim, "\"file\":{\"@type\":\"file\",\"id\":", &id)) return;
    tgNumero(t, arquivo, "\"width\":", &largura);
    tgNumero(t, arquivo, "\"height\":", &altura);
    m->arquivo = (int)id; m->imgL = (int)largura; m->imgA = (int)altura;
}

static void tgDuracao(const char* conteudo, const char* fim, char* saida, int tamanho) {
    long long d = 0;
    tgNumero(conteudo, fim, "\"duration\":", &d);
    snprintf(saida, tamanho, "%lld:%02lld", d / 60, d % 60);
}

static void tgLerConteudo(const char* c, const char* fim, TgMensagem* m) {
    char tempo[16], nome[200];
    const char* legenda = "\"caption\":{\"@type\":\"formattedText\",\"text\":\"";
    tgTexto(c, fim, "\"file_name\":\"", m->arquivoNome, sizeof(m->arquivoNome));
    if (tgConteudoE(c, "messageText")) {
        tgTexto(c, fim, "\"text\":{\"@type\":\"formattedText\",\"text\":\"", m->texto, sizeof(m->texto));
    }
    else if (tgConteudoE(c, "messagePhoto")) {
        tgLerFoto(c, fim, m);
        tgTexto(c, fim, legenda, m->texto, sizeof(m->texto));
        strcpy(m->resumo, "🖼 Foto");
    }
    else if (tgConteudoE(c, "messageVideo") || tgConteudoE(c, "messageVideoNote")) {
        tgLerMiniatura(c, fim, m);
        tgLerAbrir(c, fim, "\"video\":{\"@type\":\"file\",\"id\":", TG_M_VIDEO, m);
        tgDuracao(c, fim, tempo, sizeof(tempo));
        snprintf(m->rotulo, sizeof(m->rotulo), "Vídeo %s", tempo);
        tgTexto(c, fim, legenda, m->texto, sizeof(m->texto));
        strcpy(m->resumo, "🎬 Vídeo");
    }
    else if (tgConteudoE(c, "messageAnimation")) {
        tgLerMiniatura(c, fim, m);
        tgLerAbrir(c, fim, "\"animation\":{\"@type\":\"file\",\"id\":", TG_M_VIDEO, m);
        strcpy(m->rotulo, "GIF");
        tgTexto(c, fim, legenda, m->texto, sizeof(m->texto));
        strcpy(m->resumo, "GIF");
    }
    else if (tgConteudoE(c, "messageSticker")) {
        char emoji[32];
        tgTexto(c, fim, "\"emoji\":\"", emoji, sizeof(emoji));
        snprintf(m->resumo, sizeof(m->resumo), "%s Figurinha", emoji);
        long long id = 0, largura = 0, altura = 0;
        tgNumero(c, fim, "\"width\":", &largura);
        tgNumero(c, fim, "\"height\":", &altura);
        if (tgAcha(c, fim, "\"format\":{\"@type\":\"stickerFormatWebp\"}") && tgNumero(c, fim, "\"sticker\":{\"@type\":\"file\",\"id\":", &id)) {
            m->arquivo = (int)id; m->imgL = (int)largura; m->imgA = (int)altura;
        }
        else tgLerMiniatura(c, fim, m);
        if (m->arquivo) { m->figurinha = true; m->imgL = (int)largura; m->imgA = (int)altura; }
        else snprintf(m->texto, sizeof(m->texto), "%s", emoji);
    }
    else if (tgConteudoE(c, "messageVoiceNote")) {
        tgDuracao(c, fim, tempo, sizeof(tempo));
        tgLerAbrir(c, fim, "\"voice\":{\"@type\":\"file\",\"id\":", TG_M_AUDIO, m);
        snprintf(m->texto, sizeof(m->texto), "🎤 Áudio %s", tempo);
        strcpy(m->resumo, "🎤 Áudio");
    }
    else if (tgConteudoE(c, "messageAudio")) {
        tgTexto(c, fim, "\"title\":\"", nome, sizeof(nome));
        tgLerAbrir(c, fim, "\"audio\":{\"@type\":\"file\",\"id\":", TG_M_AUDIO, m);
        snprintf(m->texto, sizeof(m->texto), "🎵 %s", nome[0] ? nome : "Música");
        strcpy(m->resumo, "🎵 Música");
    }
    else if (tgConteudoE(c, "messageDocument")) {
        tgTexto(c, fim, "\"file_name\":\"", nome, sizeof(nome));
        int midia = tgAcha(c, fim, "\"mime_type\":\"image/") ? TG_M_FOTO : tgAcha(c, fim, "\"mime_type\":\"video/") ? TG_M_VIDEO : tgAcha(c, fim, "\"mime_type\":\"audio/") ? TG_M_AUDIO : TG_M_NADA;
        tgLerAbrir(c, fim, "\"document\":{\"@type\":\"file\",\"id\":", midia, m);
        snprintf(m->texto, sizeof(m->texto), "📄 %s", nome[0] ? nome : "Arquivo");
        strcpy(m->resumo, "📄 Arquivo");
    }
    else if (tgConteudoE(c, "messageLocation")) strcpy(m->texto, "📍 Localização");
    else if (tgConteudoE(c, "messageContact")) strcpy(m->texto, "👤 Contato");
    else if (tgConteudoE(c, "messagePoll")) strcpy(m->texto, "📊 Enquete");
    else if (tgConteudoE(c, "messageAnimatedEmoji") || tgConteudoE(c, "messageDice")) tgTexto(c, fim, "\"emoji\":\"", m->texto, sizeof(m->texto));
    else if (tgConteudoE(c, "messageCall")) strcpy(m->texto, "📞 Chamada");
    else if (tgConteudoE(c, "messageChatAddMembers") || tgConteudoE(c, "messageChatJoinByLink") || tgConteudoE(c, "messageChatJoinByRequest")) strcpy(m->texto, "👋 Entrou no grupo");
    else if (tgConteudoE(c, "messageChatDeleteMember")) strcpy(m->texto, "Saiu do grupo");
    else if (tgConteudoE(c, "messagePinMessage")) strcpy(m->texto, "📌 Fixou uma mensagem");
    else if (tgConteudoE(c, "messageChatChangeTitle")) strcpy(m->texto, "Mudou o nome do grupo");
    else if (tgConteudoE(c, "messageChatChangePhoto")) strcpy(m->texto, "Mudou a foto do grupo");
    else if (tgConteudoE(c, "messageBasicGroupChatCreate") || tgConteudoE(c, "messageSupergroupChatCreate")) strcpy(m->texto, "Grupo criado");
    else if (tgConteudoE(c, "messageVenue")) strcpy(m->texto, "📍 Localização");
    else if (tgConteudoE(c, "messageStory")) strcpy(m->texto, "Story");
    else if (tgConteudoE(c, "messageGift") || tgConteudoE(c, "messageGiftedPremium")) strcpy(m->texto, "🎁 Presente");
    else strcpy(m->texto, "Mensagem não suportada");
}

static bool tgELink(const char* t) {
    static const char* inicios[] = { "http://", "https://", "t.me/", "telegram.me/", "tg://" };
    for (size_t i = 0; i < sizeof(inicios) / sizeof(inicios[0]); i++) if (strncmp(t, inicios[i], strlen(inicios[i])) == 0) return true;
    if (t[0] != '@') return false;
    int n = 1;
    while ((t[n] >= 'a' && t[n] <= 'z') || (t[n] >= 'A' && t[n] <= 'Z') || (t[n] >= '0' && t[n] <= '9') || t[n] == '_') n++;
    return n >= 5;
}

static void tgAdicionarLink(TgMensagem* m, const char* url) {
    if (!url[0] || m->totalLinks >= 4) return;
    for (int i = 0; i < m->totalLinks; i++) if (strcmp(m->links[i], url) == 0) return;
    snprintf(m->links[m->totalLinks++], sizeof(m->links[0]), "%s", url);
}

static void tgLerLinksMarcados(const char* inicio, const char* fim, const char* chave, TgMensagem* m) {
    char url[200];
    for (const char* p = inicio; (p = tgAcha(p, fim, chave)) != NULL; p += strlen(chave)) {
        tgTexto(p, fim, chave, url, sizeof(url));
        tgAdicionarLink(m, url);
    }
}

// Junta links do texto, links escondidos em palavras e botões. @nome e t.me viram endereço completo.
static void tgLerLinks(const char* conteudo, const char* fim, TgMensagem* m) {
    tgLerLinksMarcados(conteudo, fim, "\"@type\":\"textEntityTypeTextUrl\",\"url\":\"", m);
    const char* t = m->texto;
    for (int i = 0; t[i]; i++) {
        bool inicioDePalavra = i == 0 || strchr(" \n\t([<\"'", t[i - 1]);
        if (!inicioDePalavra || !tgELink(t + i)) continue;
        char url[200];
        int n = 0;
        while (t[i + n] && !strchr(" \n\t", t[i + n]) && n < 170) n++;
        while (n > 0 && strchr(".,;:!?)]}>\"'", t[i + n - 1])) n--;
        if (t[i] == '@') snprintf(url, sizeof(url), "https://t.me/%.*s", n - 1, t + i + 1);
        else if (t[i] == 't' && t[i + 1] != 'g') snprintf(url, sizeof(url), "https://%.*s", n, t + i);
        else snprintf(url, sizeof(url), "%.*s", n, t + i);
        tgAdicionarLink(m, url);
        i += n > 0 ? n - 1 : 0;
    }
    tgLerLinksMarcados(conteudo, fim, "\"@type\":\"inlineKeyboardButtonTypeUrl\",\"url\":\"", m);
}

static bool tgLerMensagem(const char* inicio, const char* fim, TgMensagem* m, long long* conversa) {
    memset(m, 0, sizeof(*m));
    *conversa = 0;
    if (!tgNumero(inicio, fim, "\"id\":", &m->id)) return false;
    m->minha = tgAcha(inicio, fim, "\"is_outgoing\":true") != NULL;

    // Um canal é remetente com chat_id próprio: o da mensagem vem depois do bloco sender_id.
    const char* depois = inicio;
    const char* remetente = tgAcha(inicio, fim, "\"sender_id\":{");
    if (remetente) {
        const char* fecha = strchr(remetente, '}');
        if (!tgNumero(remetente, fecha, "\"user_id\":", &m->autor)) tgNumero(remetente, fecha, "\"chat_id\":", &m->autor);
        if (fecha) depois = fecha;
    }
    tgNumero(depois, fim, "\"chat_id\":", conversa);
    tgNumero(depois, fim, "\"date\":", &m->data);

    // Resposta a outro chat também traz content; o da própria mensagem é sempre o último.
    const char* chave = "\"content\":{\"@type\":\"";
    const char* conteudo = NULL;
    for (const char* q = depois; (q = tgAcha(q, fim, chave)) != NULL; q++) conteudo = q;
    if (conteudo) {
        tgLerConteudo(conteudo + strlen(chave), fim, m);
        tgLerLinks(conteudo, fim, m);
    }
    return true;
}

// Texto curto para a lista: legenda ou mensagem, se houver, ou o nome do tipo de mídia.
static void tgResumir(const TgMensagem* m, char* saida, int tamanho) {
    snprintf(saida, tamanho, "%s%s", m->minha ? "Você: " : "", m->texto[0] ? m->texto : m->resumo);
    for (char* p = saida; *p; p++) if (*p == '\n') *p = ' ';
}

// Avança para a próxima posição de lista. tipo: 0 principal, 1 arquivadas, 2 pasta.
static const char* tgProximaPosicao(const char* p, const char* fim, int* tipo, int* pasta, long long* ordem) {
    const char* chave = "\"@type\":\"chatPosition\",\"list\":{\"@type\":\"";
    const char* q = tgAcha(p, fim, chave);
    if (!q) return NULL;
    q += strlen(chave);
    long long id = 0;
    *pasta = 0;
    *ordem = 0;
    if (strncmp(q, "chatListMain", 12) == 0) *tipo = 0;
    else if (strncmp(q, "chatListArchive", 15) == 0) *tipo = 1;
    else { *tipo = 2; tgNumero(q, fim, "\"chat_folder_id\":", &id); *pasta = (int)id; }
    tgNumero(q, fim, "\"order\":", ordem);
    return q;
}

static int tgLerPastas(const char* r, TgPasta* lista, int maximo) {
    const char* chave = "{\"@type\":\"chatFolderInfo\",\"id\":";
    int total = 0;
    for (const char* p = strstr(r, chave); p && total < maximo; ) {
        const char* proxima = strstr(p + 1, chave);
        long long id = 0;
        tgNumero(p, proxima, chave, &id);
        lista[total].id = (int)id;
        tgTexto(p, proxima, "\"name\":{\"@type\":\"chatFolderName\",\"text\":{\"@type\":\"formattedText\",\"text\":\"", lista[total].nome, sizeof(lista[total].nome));
        total++;
        p = proxima;
    }
    return total;
}

// Lista de números de um campo como "chat_ids":[1,-2,3].
static int tgLerIds(const char* r, const char* chave, long long* lista, int maximo) {
    const char* p = strstr(r, chave);
    if (!p) return 0;
    p += strlen(chave);
    int total = 0;
    while (*p && *p != ']' && total < maximo) {
        char* depois;
        long long v = strtoll(p, &depois, 10);
        if (depois == p) break;
        lista[total++] = v;
        p = depois;
        if (*p == ',') p++;
    }
    return total;
}
// TG_JSON_FIM

static pthread_mutex_t tgTrava;
static volatile bool tgSujo = true;
static int tgCliente = 0;
static TgTela tgTela = TG_INICIANDO;
static char tgAviso[160] = "";
static char tgApiHash[40] = "";
static int tgApiId = 0;

static TgConversa tgConversas[TG_MAX_CONVERSAS];
static int tgTotalConversas = 0;
static int tgIndice[TG_MAX_CONVERSAS];
static int tgVisiveis = 0;
static int tgSelecao = 0, tgTopo = 0;
static bool tgCarregando = false, tgFimDaLista[TG_MAX_PASTAS + 2];
static TgPasta tgPastas[TG_MAX_PASTAS];
static int tgTotalPastas = 0, tgAba = 0, tgSlotCarregando = 0;

static bool tgEmBusca = false;
static TgResultado tgResultados[TG_MAX_RESULTADOS];
static int tgTotalResultados = 0, tgSelResultado = 0, tgTopoResultado = 0, tgBuscasPendentes = 0;
static long long tgBuscaConversa = 0;
static char tgBuscaTexto[128] = "";

static int tgVisor = 0, tgVisorMini = 0, tgVisorX = 0, tgVisorY = 0;
static float tgVisorZoom = 1.0f;
static int tgPendente = 0, tgPendenteTipo = TG_M_NADA;
static bool tgAbrirAgora = false;
static char tgPendenteCaminho[220];
static volatile bool tgAudioRodando = false, tgAudioParar = false;
static TgDownload tgDownloads[TG_MAX_DOWNLOADS];
static int tgTotalDownloads = 0, tgFilaSel = 0;
static bool tgFilaAberta = false, tgFaltamNovas = false;
static bool tgPedindoNovas = false, tgIrPrimeira = false, tgBarraAtiva = false;
static float tgBarraPos = 0.0f;
static long long tgPrimeiraData = 0;
static struct { long long conversa, mensagem; } tgPosicoes[TG_MAX_CONVERSAS];
static int tgTotalPosicoes = 0;
static struct {
    volatile bool ativo, parar, completo;
    int arquivo;
    FILE* f;
    long long total, pos, inicio, prefixo, pedido;
    char caminho[220];
    AVIOContext* avio;
} tgFluxo;
static bool tgMenuAberto = false;
static int tgMenuSel = 0, tgMenuTotal = 0;
static int tgMenuLinks[6];   // índice do link, -1 baixa o arquivo, -2 responde
static long long tgResponderA = 0;
static long long tgMenuMensagem = 0;
static char tgLinkAtual[200] = "";
static bool tgAbrirExterno = false;
static long long tgAudioMensagem = 0;

static TgUsuario tgUsuarios[TG_MAX_USUARIOS];
static int tgTotalUsuarios = 0;

static TgMensagem tgMensagens[TG_MAX_MENSAGENS];
static int tgTotalMensagens = 0;
static long long tgAberta = 0;
static int tgRolagem = 0, tgPedidosHistorico = 0;
static long long tgSelId = 0, tgAlvo = 0;
static bool tgPedindoHistorico = false, tgFimDoHistorico = false;

static volatile bool tgImeAberto = false;
static TgModoIme tgModoIme = TG_IME_MENSAGEM;
static wchar_t tgTextoIme[520];
static uint32_t* tgQuadro = NULL;

static void tgEnviar(const char* formato, ...) {
    static char pedido[4096];
    va_list args;
    va_start(args, formato);
    vsnprintf(pedido, sizeof(pedido), formato, args);
    va_end(args);
    td_send(tgCliente, pedido);
}

static void tgBaixarArquivo(int id) {
    tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":8,\"offset\":0,\"limit\":0,\"synchronous\":false}", id);
}

static const char* tgNomeAutor(const TgMensagem* m) {
    if (m->minha) return "Você";
    for (int i = 0; i < tgTotalUsuarios; i++) if (tgUsuarios[i].id == m->autor) return tgUsuarios[i].nome;
    for (int i = 0; i < tgTotalConversas; i++) if (tgConversas[i].id == m->autor) return tgConversas[i].titulo;
    return "Telegram";
}

static int tgSlotDaAba(int aba) { return aba == 0 ? 0 : aba <= tgTotalPastas ? aba + 1 : 1; }
static int tgTotalAbas() { return tgTotalPastas + 2; }
static const char* tgNomeDaAba(int aba) { return aba == 0 ? "Todas" : aba <= tgTotalPastas ? tgPastas[aba - 1].nome : "Arquivadas"; }

//@@VISUAL@@

static TgConversa* tgConversa(long long id) {
    for (int i = 0; i < tgTotalConversas; i++) if (tgConversas[i].id == id) return &tgConversas[i];
    if (tgTotalConversas >= TG_MAX_CONVERSAS) return NULL;
    TgConversa* c = &tgConversas[tgTotalConversas++];
    memset(c, 0, sizeof(*c));
    c->id = id;
    return c;
}

static void tgGuardarMensagem(const TgMensagem* m) {
    int i = 0;
    while (i < tgTotalMensagens && tgMensagens[i].id < m->id) i++;
    if (i < tgTotalMensagens && tgMensagens[i].id == m->id) { tgMensagens[i] = *m; return; }
    if (tgTotalMensagens == TG_MAX_MENSAGENS) {
        // Subindo além do limite da memória, as mais novas saem; ao voltar ao fim a conversa recarrega.
        if (i == 0) { tgTotalMensagens--; tgFaltamNovas = true; }
        else {
            memmove(&tgMensagens[0], &tgMensagens[1], sizeof(TgMensagem) * (TG_MAX_MENSAGENS - 1));
            tgTotalMensagens--;
            i--;
        }
    }
    memmove(&tgMensagens[i + 1], &tgMensagens[i], sizeof(TgMensagem) * (tgTotalMensagens - i));
    tgMensagens[i] = *m;
    tgTotalMensagens++;
}

static long long tgPosicaoDe(long long conversa) {
    for (int i = 0; i < tgTotalPosicoes; i++) if (tgPosicoes[i].conversa == conversa) return tgPosicoes[i].mensagem;
    return 0;
}

static void tgGuardarPosicao(long long conversa, long long mensagem) {
    int i = 0;
    while (i < tgTotalPosicoes && tgPosicoes[i].conversa != conversa) i++;
    if (i == tgTotalPosicoes) {
        if (!mensagem || i >= TG_MAX_CONVERSAS) return;
        tgTotalPosicoes++;
    }
    tgPosicoes[i].conversa = conversa;
    tgPosicoes[i].mensagem = mensagem;
    FILE* f = fopen(TELEGRAM_PASTA "/posicoes.txt", "w");
    if (!f) return;
    for (int j = 0; j < tgTotalPosicoes; j++) if (tgPosicoes[j].mensagem) fprintf(f, "%lld %lld\n", tgPosicoes[j].conversa, tgPosicoes[j].mensagem);
    fclose(f);
}

static void tgLerPosicoes() {
    FILE* f = fopen(TELEGRAM_PASTA "/posicoes.txt", "r");
    if (!f) return;
    while (tgTotalPosicoes < TG_MAX_CONVERSAS && fscanf(f, "%lld %lld", &tgPosicoes[tgTotalPosicoes].conversa, &tgPosicoes[tgTotalPosicoes].mensagem) == 2) tgTotalPosicoes++;
    fclose(f);
}

static void tgPedirHistorico(long long aPartirDe) {
    tgPedidosHistorico++;
    tgPedindoHistorico = true;
    tgEnviar("{\"@type\":\"getChatHistory\",\"chat_id\":%lld,\"from_message_id\":%lld,\"offset\":0,\"limit\":50,\"only_local\":false,\"@extra\":\"historico\"}", tgAberta, aPartirDe);
}

static void tgMarcarLida(long long mensagem) {
    tgEnviar("{\"@type\":\"viewMessages\",\"chat_id\":%lld,\"message_ids\":[%lld],\"force_read\":true}", tgAberta, mensagem);
}

static void tgCarregarConversas() {
    int slot = tgSlotDaAba(tgAba);
    if (tgCarregando || tgFimDaLista[slot]) return;
    tgCarregando = true;
    tgSlotCarregando = slot;
    if (slot == 0) tgEnviar("{\"@type\":\"loadChats\",\"chat_list\":{\"@type\":\"chatListMain\"},\"limit\":50,\"@extra\":\"conversas\"}");
    else if (slot == 1) tgEnviar("{\"@type\":\"loadChats\",\"chat_list\":{\"@type\":\"chatListArchive\"},\"limit\":50,\"@extra\":\"conversas\"}");
    else tgEnviar("{\"@type\":\"loadChats\",\"chat_list\":{\"@type\":\"chatListFolder\",\"chat_folder_id\":%d},\"limit\":50,\"@extra\":\"conversas\"}", tgPastas[slot - 2].id);
}

static void tgAplicarPosicoes(const char* r, TgConversa* c) {
    int tipo, pasta;
    long long ordem;
    for (const char* p = r; (p = tgProximaPosicao(p, NULL, &tipo, &pasta, &ordem)) != NULL; ) {
        if (tipo < 2) c->ordens[tipo] = ordem;
        else for (int i = 0; i < tgTotalPastas; i++) if (tgPastas[i].id == pasta) c->ordens[i + 2] = ordem;
    }
}

static void tgAdicionarResultado(long long conversa, long long mensagem, long long data, const char* texto) {
    for (int i = 0; i < tgTotalResultados; i++) if (tgResultados[i].conversa == conversa && tgResultados[i].mensagem == mensagem) return;
    if (tgTotalResultados >= TG_MAX_RESULTADOS) return;
    TgResultado* r = &tgResultados[tgTotalResultados++];
    r->conversa = conversa; r->mensagem = mensagem; r->data = data;
    snprintf(r->texto, sizeof(r->texto), "%s", texto);
}

static void tgProcessarBusca(const char* r) {
    static long long ids[TG_MAX_RESULTADOS];
    if (tgBuscasPendentes > 0) tgBuscasPendentes--;
    if (tgTipo(r, "chats")) {
        int n = tgLerIds(r, "\"chat_ids\":[", ids, TG_MAX_RESULTADOS);
        for (int i = 0; i < n; i++) tgAdicionarResultado(ids[i], 0, 0, "");
    }
    else if (tgTipo(r, "foundMessages") || tgTipo(r, "foundChatMessages")) {
        const char* chave = "{\"@type\":\"message\",\"id\":";
        static TgMensagem m;
        for (const char* p = strstr(r, chave); p; ) {
            const char* proxima = strstr(p + 1, chave);
            long long conversa;
            char resumo[200];
            if (tgLerMensagem(p, proxima, &m, &conversa)) { tgResumir(&m, resumo, sizeof(resumo)); tgAdicionarResultado(conversa, m.id, m.data, resumo); }
            p = proxima;
        }
    }
}

static void tgIniciarBusca(const char* escapado) {
    char consulta[3300];
    snprintf(consulta, sizeof(consulta), "\"q\":\"%s\"", escapado);
    tgTexto(consulta, NULL, "\"q\":\"", tgBuscaTexto, sizeof(tgBuscaTexto));
    tgEmBusca = true;
    tgTotalResultados = 0; tgSelResultado = 0; tgTopoResultado = 0;
    tgBuscaConversa = tgTela == TG_MENSAGENS ? tgAberta : 0;
    if (tgBuscaConversa) {
        tgBuscasPendentes = 1;
        tgEnviar("{\"@type\":\"searchChatMessages\",\"chat_id\":%lld,\"query\":\"%s\",\"from_message_id\":0,\"offset\":0,\"limit\":50,\"@extra\":\"busca\"}", tgBuscaConversa, escapado);
        return;
    }
    tgBuscasPendentes = 4;
    tgEnviar("{\"@type\":\"searchChats\",\"query\":\"%s\",\"limit\":20,\"@extra\":\"busca\"}", escapado);
    tgEnviar("{\"@type\":\"searchChatsOnServer\",\"query\":\"%s\",\"limit\":20,\"@extra\":\"busca\"}", escapado);
    tgEnviar("{\"@type\":\"searchPublicChats\",\"query\":\"%s\",\"@extra\":\"busca\"}", escapado);
    tgEnviar("{\"@type\":\"searchMessages\",\"query\":\"%s\",\"offset\":\"\",\"limit\":30,\"@extra\":\"busca\"}", escapado);
}

static void tgProcessarEstado(const char* r) {
    tgAviso[0] = '\0';
    if (strstr(r, "authorizationStateWaitTdlibParameters")) {
        tgEnviar("{\"@type\":\"setTdlibParameters\",\"use_test_dc\":false,\"database_directory\":\"" TELEGRAM_PASTA "/conta\",\"files_directory\":\"" TELEGRAM_PASTA "/files\","
            "\"database_encryption_key\":\"\",\"use_file_database\":false,\"use_chat_info_database\":false,\"use_message_database\":false,\"use_secret_chats\":false,"
            "\"api_id\":%d,\"api_hash\":\"%s\",\"system_language_code\":\"pt\",\"device_model\":\"PlayStation 4\",\"system_version\":\"GoldHEN\",\"application_version\":\"0.1\"}", tgApiId, tgApiHash);
    }
    else if (strstr(r, "authorizationStateWaitPhoneNumber")) tgTela = TG_TELEFONE;
    else if (strstr(r, "authorizationStateWaitCode")) tgTela = TG_CODIGO;
    else if (strstr(r, "authorizationStateWaitPassword")) tgTela = TG_SENHA;
    else if (strstr(r, "authorizationStateReady")) { tgTela = TG_CONVERSAS; tgCarregarConversas(); }
    else if (strstr(r, "authorizationStateLoggingOut") || strstr(r, "authorizationStateClosing") || strstr(r, "authorizationStateClosed")) {
        tgTela = TG_INICIANDO;
        strcpy(tgAviso, "Sessão encerrada. Reabra o app.");
    }
    else tgTela = TG_SEM_SUPORTE;
}

static void tgProcessarMensagens(const char* r, bool novas) {
    const char* chave = "{\"@type\":\"message\",\"id\":";
    int lidas = 0;
    long long maisNovaAntes = tgTotalMensagens > 0 ? tgMensagens[tgTotalMensagens - 1].id : 0;
    if (novas) tgPedindoNovas = false;
    else tgPedindoHistorico = false;
    for (const char* p = strstr(r, chave); p; ) {
        const char* proxima = strstr(p + 1, chave);
        TgMensagem m;
        long long conversa;
        if (tgLerMensagem(p, proxima, &m, &conversa) && conversa == tgAberta) { tgGuardarMensagem(&m); lidas++; }
        p = proxima;
    }
    if (!tgAberta || tgTotalMensagens == 0) return;
    if (novas) {
        // Nada mais novo que a última carregada: a conversa chegou ao fim de verdade.
        if (tgMensagens[tgTotalMensagens - 1].id == maisNovaAntes) tgFaltamNovas = false;
        return;
    }
    if (tgIrPrimeira) {
        tgIrPrimeira = false;
        tgSelId = tgMensagens[0].id;
        tgFimDoHistorico = true;
        tgFaltamNovas = true;
        return;
    }
    if (lidas == 0) tgFimDoHistorico = true;
    if (tgAlvo) {
        for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgAlvo) tgSelId = tgAlvo;
        tgAlvo = 0;
        tgFaltamNovas = true;   // o salto carrega só a vizinhança da mensagem
        return;
    }
    // A primeira resposta costuma trazer poucas mensagens, então pede mais até encher a tela.
    if (lidas > 0 && tgTotalMensagens < 40 && tgPedidosHistorico < 6) tgPedirHistorico(tgMensagens[0].id);
    else if (tgSelId == 0) tgMarcarLida(tgMensagens[tgTotalMensagens - 1].id);
}

static TgDownload* tgDownload(int arquivo) {
    for (int i = 0; i < tgTotalDownloads; i++) if (tgDownloads[i].arquivo == arquivo) return &tgDownloads[i];
    return NULL;
}

static void tgRemoverDownload(int indice) {
    memmove(&tgDownloads[indice], &tgDownloads[indice + 1], sizeof(TgDownload) * (tgTotalDownloads - indice - 1));
    tgTotalDownloads--;
    if (tgFilaSel >= tgTotalDownloads) tgFilaSel = tgTotalDownloads > 0 ? tgTotalDownloads - 1 : 0;
}

static TgDownload* tgNovoDownload(int arquivo, const char* nome, long long mensagem) {
    TgDownload* d = tgDownload(arquivo);
    if (d) return d;
    if (tgTotalDownloads == TG_MAX_DOWNLOADS) {
        int livre = -1;
        for (int i = 0; i < tgTotalDownloads && livre < 0; i++) if (tgDownloads[i].estado >= TG_D_PRONTO) livre = i;
        if (livre < 0) return NULL;
        tgRemoverDownload(livre);
    }
    d = &tgDownloads[tgTotalDownloads++];
    memset(d, 0, sizeof(*d));
    d->arquivo = arquivo;
    d->mensagem = mensagem;
    snprintf(d->nome, sizeof(d->nome), "%s", nome);
    return d;
}

static void tgProcessarArquivo(const char* r) {
    long long id = 0;
    if (!tgNumero(r, NULL, "{\"@type\":\"file\",\"id\":", &id)) return;
    bool completo = strstr(r, "\"is_downloading_completed\":true") != NULL;
    if (id == tgFluxo.arquivo) {
        long long total = 0;
        tgNumero(r, NULL, "\"size\":", &total);
        if (total <= 0) tgNumero(r, NULL, "\"expected_size\":", &total);
        if (total > 0) tgFluxo.total = total;
        tgNumero(r, NULL, "\"download_offset\":", &tgFluxo.inicio);
        tgNumero(r, NULL, "\"downloaded_prefix_size\":", &tgFluxo.prefixo);
        if (!tgFluxo.ativo) tgTexto(r, NULL, "\"local\":{\"@type\":\"localFile\",\"path\":\"", tgFluxo.caminho, sizeof(tgFluxo.caminho));
        if (completo) tgFluxo.completo = true;
        // Com o começo do arquivo no disco, o player já consegue abrir.
        if (!tgFluxo.ativo && !completo && id == tgPendente && tgFluxo.caminho[0] && tgFluxo.inicio == 0 && tgFluxo.prefixo >= 512 * 1024) tgAbrirAgora = true;
    }
    TgDownload* d = tgDownload((int)id);
    if (d && d->estado == TG_D_BAIXANDO) {
        tgNumero(r, NULL, "\"size\":", &d->total);
        if (d->total <= 0) tgNumero(r, NULL, "\"expected_size\":", &d->total);
        tgNumero(r, NULL, "\"downloaded_size\":", &d->baixado);
        tgSujo = true;
    }
    if (id == tgPendente && !completo && d && id != tgFluxo.arquivo) {
        snprintf(tgAviso, sizeof(tgAviso), "Baixando %lld%%   Options: downloads", d->total > 0 ? d->baixado * 100 / d->total : 0);
    }
    if (!completo) return;
    TgArquivo* a = tgArquivo((int)id);
    if (!a) return;
    if (!a->baixado) {
        tgTexto(r, NULL, "\"local\":{\"@type\":\"localFile\",\"path\":\"", a->caminho, sizeof(a->caminho));
        a->baixado = a->caminho[0] != '\0';
    }
    if (!a->baixado) return;
    if (d && d->estado == TG_D_BAIXANDO) d->estado = d->salvar ? TG_D_COPIAR : TG_D_PRONTO;
    if (id == tgPendente) tgAbrirAgora = true;
}

static void tgAbrirConversa(long long id, long long mensagem);
static void tgCarregarEm(long long mensagem);

static void tgProcessarLink(const char* r) {
    char valor[220];
    long long id = 0, mensagem = 0;
    tgAviso[0] = '\0';
    if (tgTipo(r, "internalLinkTypePublicChat")) {
        tgTexto(r, NULL, "\"chat_username\":\"", valor, sizeof(valor));
        tgEnviar("{\"@type\":\"searchPublicChat\",\"username\":\"%s\",\"@extra\":\"link\"}", valor);
        strcpy(tgAviso, "Abrindo...");
    }
    else if (tgTipo(r, "internalLinkTypeMessage")) {
        tgTexto(r, NULL, "\"url\":\"", valor, sizeof(valor));
        tgEnviar("{\"@type\":\"getMessageLinkInfo\",\"url\":\"%s\",\"@extra\":\"link\"}", valor);
        strcpy(tgAviso, "Abrindo...");
    }
    else if (tgTipo(r, "internalLinkTypeChatInvite")) {
        tgTexto(r, NULL, "\"invite_link\":\"", valor, sizeof(valor));
        tgEnviar("{\"@type\":\"checkChatInviteLink\",\"invite_link\":\"%s\",\"@extra\":\"link\"}", valor);
        strcpy(tgAviso, "Abrindo...");
    }
    else if (tgTipo(r, "chat")) { if (tgNumero(r, NULL, "{\"@type\":\"chat\",\"id\":", &id)) tgAbrirConversa(id, 0); }
    else if (tgTipo(r, "messageLinkInfo")) {
        tgNumero(r, NULL, "\"chat_id\":", &id);
        tgNumero(r, NULL, "\"message\":{\"@type\":\"message\",\"id\":", &mensagem);
        if (id) tgAbrirConversa(id, mensagem);
        else strcpy(tgAviso, "Você não tem acesso a esta mensagem.");
    }
    else if (tgTipo(r, "chatInviteLinkInfo")) {
        tgNumero(r, NULL, "\"chat_id\":", &id);
        if (id) tgAbrirConversa(id, 0);
        else strcpy(tgAviso, "Convite de grupo: entre pelo celular.");
    }
    else if (tgTipo(r, "error")) {
        long long codigo = 0;
        tgNumero(r, NULL, "\"code\":", &codigo);
        // 404 no primeiro passo quer dizer que o link não é do Telegram.
        if (codigo == 404 && tgLinkAtual[0]) tgAbrirExterno = true;
        else tgTexto(r, NULL, "\"message\":\"", tgAviso, sizeof(tgAviso));
    }
    else strcpy(tgAviso, "Este tipo de link ainda não abre aqui.");
    tgLinkAtual[0] = tgAbrirExterno ? tgLinkAtual[0] : '\0';
}

static void tgProcessar(const char* r) {
    long long id = 0;
    if (tgTipo(r, "updateFile") || tgTipo(r, "file")) { tgProcessarArquivo(r); return; }
    if (strstr(r, "\"@extra\":\"data\"")) {
        if (tgTipo(r, "message") && tgNumero(r, NULL, "\"id\":", &id)) tgCarregarEm(id);
        tgSujo = true;
        return;
    }
    if (strstr(r, "\"@extra\":\"link\"")) {
        tgProcessarLink(r);
        tgSujo = true;
        return;
    }
    if (strstr(r, "\"@extra\":\"busca\"")) {
        tgProcessarBusca(r);
        tgSujo = true;
        return;
    }
    if (tgTipo(r, "updateChatFolders")) {
        tgTotalPastas = tgLerPastas(r, tgPastas, TG_MAX_PASTAS);
        if (tgAba >= tgTotalAbas()) tgAba = 0;
        tgSujo = true;
        return;
    }
    if (tgTipo(r, "updateAuthorizationState")) tgProcessarEstado(r);
    else if (tgTipo(r, "updateUser")) {
        if (tgNumero(r, NULL, "\"user\":{\"@type\":\"user\",\"id\":", &id)) {
            TgUsuario* u = NULL;
            for (int i = 0; i < tgTotalUsuarios; i++) if (tgUsuarios[i].id == id) u = &tgUsuarios[i];
            if (!u && tgTotalUsuarios < TG_MAX_USUARIOS) u = &tgUsuarios[tgTotalUsuarios++];
            if (u) { u->id = id; tgTexto(r, NULL, "\"first_name\":\"", u->nome, sizeof(u->nome)); }
        }
        return;
    }
    else if (tgTipo(r, "updateNewChat")) {
        if (!tgNumero(r, NULL, "\"chat\":{\"@type\":\"chat\",\"id\":", &id)) return;
        TgConversa* c = tgConversa(id);
        if (!c) return;
        tgTexto(r, NULL, "\"title\":\"", c->titulo, sizeof(c->titulo));
        long long n = 0;
        if (tgNumero(r, NULL, "\"unread_count\":", &n)) c->naoLidas = (int)n;
        if (tgNumero(r, NULL, "\"small\":{\"@type\":\"file\",\"id\":", &n)) c->foto = (int)n;
        tgNumero(r, NULL, "\"last_read_inbox_message_id\":", &c->lidoAte);
        tgAplicarPosicoes(r, c);
    }
    else if (tgTipo(r, "updateChatTitle")) {
        if (!tgNumero(r, NULL, "\"chat_id\":", &id)) return;
        TgConversa* c = tgConversa(id);
        if (c) tgTexto(r, NULL, "\"title\":\"", c->titulo, sizeof(c->titulo));
    }
    else if (tgTipo(r, "updateChatPhoto")) {
        long long n = 0;
        if (!tgNumero(r, NULL, "\"chat_id\":", &id)) return;
        TgConversa* c = tgConversa(id);
        if (c) c->foto = tgNumero(r, NULL, "\"small\":{\"@type\":\"file\",\"id\":", &n) ? (int)n : 0;
    }
    else if (tgTipo(r, "updateChatPosition")) {
        if (!tgNumero(r, NULL, "\"chat_id\":", &id)) return;
        TgConversa* c = tgConversa(id);
        if (c) tgAplicarPosicoes(r, c);
    }
    else if (tgTipo(r, "updateChatReadInbox")) {
        long long n = 0;
        if (!tgNumero(r, NULL, "\"chat_id\":", &id) || !tgNumero(r, NULL, "\"unread_count\":", &n)) return;
        TgConversa* c = tgConversa(id);
        if (c) { c->naoLidas = (int)n; tgNumero(r, NULL, "\"last_read_inbox_message_id\":", &c->lidoAte); }
    }
    else if (tgTipo(r, "updateChatLastMessage")) {
        if (!tgNumero(r, NULL, "\"chat_id\":", &id)) return;
        TgConversa* c = tgConversa(id);
        if (!c) return;
        const char* posicoes = strstr(r, "\"positions\":");
        const char* ultima = tgAcha(r, posicoes, "\"last_message\":{");
        if (ultima) {
            static TgMensagem m;
            long long conversa;
            if (tgLerMensagem(ultima, posicoes, &m, &conversa)) { tgResumir(&m, c->ultima, sizeof(c->ultima)); c->data = m.data; }
        }
        if (posicoes) tgAplicarPosicoes(posicoes, c);
    }
    else if (tgTipo(r, "updateNewMessage")) {
        static TgMensagem m;
        long long conversa;
        if (!tgLerMensagem(r, NULL, &m, &conversa) || conversa != tgAberta || tgFaltamNovas) return;
        tgGuardarMensagem(&m);
        if (tgSelId == 0 && !m.minha) tgMarcarLida(m.id);
    }
    else if (tgTipo(r, "updateMessageSendSucceeded")) {
        long long antigo = 0;
        if (!tgNumero(r, NULL, "\"old_message_id\":", &antigo)) return;
        static TgMensagem m;
        long long conversa;
        if (!tgLerMensagem(r, NULL, &m, &conversa) || conversa != tgAberta) return;
        for (int i = 0; i < tgTotalMensagens; i++) {
            if (tgMensagens[i].id != antigo) continue;
            memmove(&tgMensagens[i], &tgMensagens[i + 1], sizeof(TgMensagem) * (tgTotalMensagens - i - 1));
            tgTotalMensagens--;
            break;
        }
        tgGuardarMensagem(&m);
    }
    else if (tgTipo(r, "updateMessageSendFailed")) strcpy(tgAviso, "Mensagem não enviada.");
    else if (tgTipo(r, "messages")) {
        if (strstr(r, "\"@extra\":\"historico\"")) tgProcessarMensagens(r, false);
        else if (strstr(r, "\"@extra\":\"novas\"")) tgProcessarMensagens(r, true);
        else if (strstr(r, "\"@extra\":\"primeira\"")) {
            long long data = 0, menor = 0;
            for (const char* p = r; (p = strstr(p, "\"date\":")) != NULL; p += 7) {
                tgNumero(p, NULL, "\"date\":", &data);
                if (data > 0 && (!menor || data < menor)) menor = data;
            }
            tgPrimeiraData = menor;
        }
    }
    else if (tgTipo(r, "ok")) {
        if (strstr(r, "\"@extra\":\"conversas\"")) tgCarregando = false;
        return;
    }
    else if (tgTipo(r, "error")) {
        long long codigo = 0;
        tgNumero(r, NULL, "\"code\":", &codigo);
        if (strstr(r, "\"@extra\":\"conversas\"")) {
            tgCarregando = false;
            if (codigo == 404) { tgFimDaLista[tgSlotCarregando] = true; return; }
        }
        tgTexto(r, NULL, "\"message\":\"", tgAviso, sizeof(tgAviso));
    }
    else return;
    tgSujo = true;
}

static void* threadTelegram(void*) {
    signal(SIGPIPE, SIG_IGN);
    // Acima do nível 1 o TDLib grava telefone e mensagens no arquivo.
    td_execute("{\"@type\":\"setLogStream\",\"log_stream\":{\"@type\":\"logStreamFile\",\"path\":\"" TELEGRAM_PASTA "/td.log\",\"max_file_size\":10485760}}");
    td_execute("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":1}");
    tgCliente = td_create_client_id();
    td_send(tgCliente, "{\"@type\":\"getOption\",\"name\":\"version\"}");
    for (;;) {
        const char* r = td_receive(1.0);
        if (!r) continue;
        pthread_mutex_lock(&tgTrava);
        tgProcessar(r);
        pthread_mutex_unlock(&tgTrava);
    }
    return NULL;
}

// Figurinhas são WebP, que o stb não lê; o FFmpeg do player de vídeo decodifica.
static uint32_t* tgLerWebp(const unsigned char* dados, int tamanho, int* largura, int* altura) {
    uint32_t* saida = NULL;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_WEBP);
    AVCodecContext* contexto = codec ? avcodec_alloc_context3(codec) : NULL;
    AVPacket* pacote = av_packet_alloc();
    AVFrame* quadro = av_frame_alloc();
    unsigned char* copia = (unsigned char*)calloc(1, (size_t)tamanho + AV_INPUT_BUFFER_PADDING_SIZE);
    if (contexto && pacote && quadro && copia && avcodec_open2(contexto, codec, NULL) >= 0) {
        memcpy(copia, dados, tamanho);
        pacote->data = copia;
        pacote->size = tamanho;
        if (avcodec_send_packet(contexto, pacote) >= 0 && avcodec_receive_frame(contexto, quadro) >= 0) {
            struct SwsContext* conversor = sws_getContext(quadro->width, quadro->height, (AVPixelFormat)quadro->format, quadro->width, quadro->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
            saida = (uint32_t*)malloc((size_t)quadro->width * quadro->height * 4);
            if (conversor && saida) {
                uint8_t* planos[4] = { (uint8_t*)saida, NULL, NULL, NULL };
                int passos[4] = { quadro->width * 4, 0, 0, 0 };
                sws_scale(conversor, quadro->data, quadro->linesize, 0, quadro->height, planos, passos);
                *largura = quadro->width;
                *altura = quadro->height;
            }
            else { free(saida); saida = NULL; }
            if (conversor) sws_freeContext(conversor);
        }
    }
    pacote->data = NULL;
    pacote->size = 0;
    free(copia);
    av_frame_free(&quadro);
    av_packet_free(&pacote);
    avcodec_free_context(&contexto);
    return saida;
}

// Lista usa até 640 pixels e o visor até 1920: quem chega maior só gastaria memória.
static uint32_t* tgReduzir(uint32_t* pixels, int* largura, int* altura, int limite) {
    int w = *largura, h = *altura;
    if (w <= limite && h <= limite) return pixels;
    int nw = w >= h ? limite : w * limite / h, nh = w >= h ? h * limite / w : limite;
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    uint32_t* menor = (uint32_t*)malloc((size_t)nw * nh * 4);
    if (menor) {
        for (int y = 0; y < nh; y++) {
            int y0 = y * h / nh, y1 = (y + 1) * h / nh;
            if (y1 <= y0) y1 = y0 + 1;
            for (int x = 0; x < nw; x++) {
                int x0 = x * w / nw, x1 = (x + 1) * w / nw;
                if (x1 <= x0) x1 = x0 + 1;
                uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int v = y0; v < y1; v++) for (int u = x0; u < x1; u++) {
                    uint32_t c = pixels[v * w + u];
                    r += c & 255; g += (c >> 8) & 255; b += (c >> 16) & 255; a += c >> 24; n++;
                }
                menor[y * nw + x] = ((a / n) << 24) | ((b / n) << 16) | ((g / n) << 8) | (r / n);
            }
        }
        *largura = nw;
        *altura = nh;
    }
    free(pixels);
    return menor;
}

static uint32_t* tgLerImagem(const char* caminho, int* largura, int* altura, int limite) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long tamanho = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t* pixels = NULL;
    unsigned char* dados = (tamanho > 12 && tamanho < 24 * 1024 * 1024) ? (unsigned char*)malloc(tamanho) : NULL;
    if (dados && fread(dados, 1, tamanho, f) == (size_t)tamanho) {
        if (memcmp(dados, "RIFF", 4) == 0 && memcmp(dados + 8, "WEBP", 4) == 0) pixels = tgLerWebp(dados, (int)tamanho, largura, altura);
        else {
            int canais;
            unsigned char* lido = stbi_load_from_memory(dados, (int)tamanho, largura, altura, &canais, 4);
            if (lido) {
                size_t bytes = (size_t)*largura * *altura * 4;
                pixels = (uint32_t*)malloc(bytes);
                if (pixels) memcpy(pixels, lido, bytes);
                stbi_image_free(lido);
            }
        }
    }
    free(dados);
    fclose(f);
    return pixels ? tgReduzir(pixels, largura, altura, limite) : NULL;
}

// A decodificação fica fora da thread da tela para o controle não engasgar.
static void* threadImagens(void*) {
    static char caminho[220];
    tgCarregarFontes();
    for (;;) {
        int id = 0, melhorUso = -1, limite = 640;
        pthread_mutex_lock(&tgTrava);
        for (int i = 0; i < tgTotalArquivos; i++) {
            TgArquivo* a = &tgArquivos[i];
            if (a->quer && a->baixado && !a->pixels && !a->falhou && a->uso > melhorUso) { melhorUso = a->uso; id = a->id; limite = a->limite ? a->limite : 640; strcpy(caminho, a->caminho); }
        }
        pthread_mutex_unlock(&tgTrava);
        if (!id) { sceKernelUsleep(60000); continue; }

        int largura = 0, altura = 0;
        uint32_t* pixels = tgLerImagem(caminho, &largura, &altura, limite);

        pthread_mutex_lock(&tgTrava);
        TgArquivo* a = tgArquivo(id);
        if (a) {
            a->quer = false;
            a->falhou = pixels == NULL;
            if (pixels) {
                size_t bytes = (size_t)largura * altura * 4;
                while (tgMemoriaImagens + bytes > TG_MEMORIA_IMAGENS) {
                    TgArquivo* velho = NULL;
                    for (int i = 0; i < tgTotalArquivos; i++) if (tgArquivos[i].pixels && (!velho || tgArquivos[i].uso < velho->uso)) velho = &tgArquivos[i];
                    if (!velho) break;
                    tgMemoriaImagens -= (size_t)velho->largura * velho->altura * 4;
                    free(velho->pixels);
                    velho->pixels = NULL;
                }
                a->pixels = pixels;
                a->largura = largura;
                a->altura = altura;
                tgMemoriaImagens += bytes;
                pixels = NULL;
            }
            tgSujo = true;
        }
        pthread_mutex_unlock(&tgTrava);
        free(pixels);
    }
    return NULL;
}

// O player do app exige faixa de vídeo: áudio de voz e música têm caminho próprio até a saída de som.
static void* threadAudio(void* argumento) {
    const char* caminho = (const char*)argumento;
    extern int32_t global_uId;
    AVFormatContext* formato = NULL;
    AVCodecContext* contexto = NULL;
    SwrContext* conversor = NULL;
    AVPacket* pacote = av_packet_alloc();
    AVFrame* quadro = av_frame_alloc();
    static int16_t fila[(16384 + 1024) * 2];
    int porta = -1, naFila = 0, faixa = -1;
    const int passo = 1024;

    if (pacote && quadro && avformat_open_input(&formato, caminho, NULL, NULL) == 0 && avformat_find_stream_info(formato, NULL) >= 0) {
        for (unsigned int i = 0; i < formato->nb_streams; i++) if (formato->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) { faixa = (int)i; break; }
    }
    const AVCodec* codec = faixa >= 0 ? avcodec_find_decoder(formato->streams[faixa]->codecpar->codec_id) : NULL;
    if (codec) contexto = avcodec_alloc_context3(codec);
    if (contexto) {
        avcodec_parameters_to_context(contexto, formato->streams[faixa]->codecpar);
        if (avcodec_open2(contexto, codec, NULL) >= 0) {
            uint64_t layout = contexto->channel_layout ? contexto->channel_layout : (uint64_t)av_get_default_channel_layout(contexto->channels);
            conversor = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 48000, layout, contexto->sample_fmt, contexto->sample_rate, 0, NULL);
            if (conversor && swr_init(conversor) < 0) swr_free(&conversor);
        }
    }
    if (conversor) porta = sceAudioOutOpen(global_uId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, passo, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (porta >= 0) {
        while (!tgAudioParar && av_read_frame(formato, pacote) >= 0) {
            if (pacote->stream_index == faixa && avcodec_send_packet(contexto, pacote) >= 0) {
                while (!tgAudioParar && avcodec_receive_frame(contexto, quadro) >= 0) {
                    uint8_t* saida[1] = { (uint8_t*)(fila + naFila * 2) };
                    int feitos = swr_convert(conversor, saida, 16384, (const uint8_t**)quadro->extended_data, quadro->nb_samples);
                    if (feitos > 0) naFila += feitos;
                    int lidos = 0;
                    while (naFila - lidos >= passo && !tgAudioParar) { sceAudioOutOutput(porta, fila + lidos * 2); lidos += passo; }
                    if (lidos) { memmove(fila, fila + lidos * 2, (size_t)(naFila - lidos) * 4); naFila -= lidos; }
                }
            }
            av_packet_unref(pacote);
        }
        if (naFila > 0 && !tgAudioParar) {
            memset(fila + naFila * 2, 0, (size_t)(passo - naFila) * 4);
            sceAudioOutOutput(porta, fila);
        }
        sceAudioOutOutput(porta, NULL);
        sceAudioOutClose(porta);
    }
    else {
        pthread_mutex_lock(&tgTrava);
        strcpy(tgAviso, "Não foi possível tocar este áudio.");
        pthread_mutex_unlock(&tgTrava);
    }
    if (conversor) swr_free(&conversor);
    if (contexto) avcodec_free_context(&contexto);
    if (formato) avformat_close_input(&formato);
    av_frame_free(&quadro);
    av_packet_free(&pacote);
    tgAudioMensagem = 0;
    tgAudioRodando = false;
    tgSujo = true;
    return NULL;
}

static void tgPararAudio() {
    tgAudioParar = true;
    for (int i = 0; i < 100 && tgAudioRodando; i++) sceKernelUsleep(10000);
}

static void tgTocarAudio(const char* caminho, long long mensagem) {
    static char arquivo[220];
    tgPararAudio();
    if (tgAudioRodando) return;
    snprintf(arquivo, sizeof(arquivo), "%s", caminho);
    tgAudioParar = false;
    tgAudioRodando = true;
    tgAudioMensagem = mensagem;
    pthread_t t;
    if (pthread_create(&t, NULL, threadAudio, arquivo) != 0) { tgAudioRodando = false; tgAudioMensagem = 0; return; }
    pthread_detach(t);
}

static long long tgPendenteMensagem = 0;
static char tgCopiaOrigem[220], tgCopiaDestino[320], tgInstalar[320] = "";
static int tgCopiaArquivo = 0;
static bool tgCopiaPkg = false;
static volatile bool tgCopiando = false;

// Copia em vez de mover: o TDLib continua dono do arquivo que baixou.
static void* threadSalvar(void*) {
    FILE* a = fopen(tgCopiaOrigem, "rb");
    FILE* b = a ? fopen(tgCopiaDestino, "wb") : NULL;
    bool certo = a && b;
    static char bloco[1 << 18];
    size_t n;
    while (certo && (n = fread(bloco, 1, sizeof(bloco), a)) > 0) certo = fwrite(bloco, 1, n, b) == n;
    if (a) fclose(a);
    if (b) fclose(b);
    if (!certo) remove(tgCopiaDestino);
    pthread_mutex_lock(&tgTrava);
    TgDownload* d = tgDownload(tgCopiaArquivo);
    if (d) d->estado = certo ? TG_D_PRONTO : TG_D_ERRO;
    if (certo && tgCopiaPkg) {
        // PKG é grande: a cópia em /data/pkg basta, a do TDLib só gastaria espaço.
        TgArquivo* arquivo = tgArquivo(tgCopiaArquivo);
        if (arquivo) { arquivo->baixado = false; arquivo->pedido = false; }
        tgEnviar("{\"@type\":\"deleteFile\",\"file_id\":%d}", tgCopiaArquivo);
        snprintf(tgInstalar, sizeof(tgInstalar), "%s", tgCopiaDestino);
    }
    else snprintf(tgAviso, sizeof(tgAviso), certo ? "Salvo em baixado/telegram" : "Não foi possível salvar o arquivo.");
    pthread_mutex_unlock(&tgTrava);
    tgSujo = true;
    tgCopiando = false;
    return NULL;
}

// Chamada com a trava pega: um arquivo por vez, para não disputar o disco.
static void tgIniciarCopia() {
    if (tgCopiando) return;
    for (int i = 0; i < tgTotalDownloads; i++) {
        TgDownload* d = &tgDownloads[i];
        if (d->estado != TG_D_COPIAR) continue;
        TgArquivo* a = tgArquivo(d->arquivo);
        if (!a) { d->estado = TG_D_ERRO; continue; }
        const char* pasta = d->pkg ? "/data/pkg" : "/data/HyperNeiva/baixado/telegram";
        sceKernelMkdir(pasta, 0777);
        snprintf(tgCopiaOrigem, sizeof(tgCopiaOrigem), "%s", a->caminho);
        snprintf(tgCopiaDestino, sizeof(tgCopiaDestino), "%s/%s", pasta, d->nome);
        FILE* existe = fopen(tgCopiaDestino, "rb");
        if (existe) {
            fclose(existe);
            snprintf(tgCopiaDestino, sizeof(tgCopiaDestino), "%s/%lld_%s", pasta, d->mensagem, d->nome);
        }
        tgCopiaArquivo = d->arquivo;
        tgCopiaPkg = d->pkg;
        d->estado = TG_D_COPIANDO;
        tgCopiando = true;
        pthread_t t;
        if (pthread_create(&t, NULL, threadSalvar, NULL) == 0) pthread_detach(t);
        else { tgCopiando = false; d->estado = TG_D_ERRO; }
        return;
    }
}

void telegramFecharFluxo();

static void tgAbrirNoNavegador(const char* url) {
    extern void sceSystemServiceLaunchWebBrowser();
    int resultado = ((int (*)(const char*, void*))sceSystemServiceLaunchWebBrowser)(url, NULL);
    if (resultado < 0) {
        pthread_mutex_lock(&tgTrava);
        snprintf(tgAviso, sizeof(tgAviso), "Abra no navegador: %.130s", url);
        pthread_mutex_unlock(&tgTrava);
        tgSujo = true;
    }
}

// O player lê daqui enquanto o TDLib baixa. Trecho que ainda não chegou espera, e um salto
// no vídeo pede o download a partir do ponto novo.
static int tgFluxoLer(void*, uint8_t* destino, int tamanho) {
    for (int espera = 0; espera < 3000; espera++) {
        if (tgFluxo.parar) return AVERROR_EOF;
        pthread_mutex_lock(&tgTrava);
        long long disponivel = 0;
        if (tgFluxo.completo) disponivel = tgFluxo.total - tgFluxo.pos;
        else if (tgFluxo.pos >= tgFluxo.inicio && tgFluxo.pos < tgFluxo.inicio + tgFluxo.prefixo) disponivel = tgFluxo.inicio + tgFluxo.prefixo - tgFluxo.pos;
        else if (tgFluxo.pedido != tgFluxo.pos) {
            tgFluxo.pedido = tgFluxo.pos;
            tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":32,\"offset\":%lld,\"limit\":0,\"synchronous\":false}", tgFluxo.arquivo, tgFluxo.pos);
        }
        pthread_mutex_unlock(&tgTrava);
        if (tgFluxo.completo && disponivel <= 0) return AVERROR_EOF;
        if (disponivel > 0) {
            int n = disponivel < tamanho ? (int)disponivel : tamanho;
            fseek(tgFluxo.f, (long)tgFluxo.pos, SEEK_SET);
            int lidos = (int)fread(destino, 1, n, tgFluxo.f);
            if (lidos <= 0) return AVERROR_EOF;
            tgFluxo.pos += lidos;
            return lidos;
        }
        sceKernelUsleep(20000);
    }
    return AVERROR_EOF;
}

static int64_t tgFluxoSaltar(void*, int64_t posicao, int origem) {
    origem &= ~AVSEEK_FORCE;
    if (origem == AVSEEK_SIZE) return tgFluxo.total;
    long long nova = origem == SEEK_SET ? posicao : origem == SEEK_CUR ? tgFluxo.pos + posicao : tgFluxo.total + posicao;
    if (nova < 0) return -1;
    tgFluxo.pos = nova;
    return nova;
}

// Devolve a leitura própria quando o player vai abrir o vídeo em andamento; nos demais, NULL.
AVIOContext* telegramAbrirFluxo(const char* caminho) {
    if (!tgFluxo.ativo || strcmp(caminho, tgFluxo.caminho) != 0) return NULL;
    tgFluxo.f = fopen(caminho, "rb");
    unsigned char* memoria = tgFluxo.f ? (unsigned char*)av_malloc(65536) : NULL;
    if (memoria) tgFluxo.avio = avio_alloc_context(memoria, 65536, 0, NULL, tgFluxoLer, NULL, tgFluxoSaltar);
    if (!tgFluxo.avio) {
        if (memoria) av_free(memoria);
        if (tgFluxo.f) { fclose(tgFluxo.f); tgFluxo.f = NULL; }
    }
    return tgFluxo.avio;
}

void telegramPararFluxo() { if (tgFluxo.ativo) tgFluxo.parar = true; }

void telegramFecharFluxo() {
    if (!tgFluxo.ativo) return;
    tgFluxo.parar = true;
    if (tgFluxo.avio) {
        av_freep(&tgFluxo.avio->buffer);
        avio_context_free(&tgFluxo.avio);
    }
    if (tgFluxo.f) { fclose(tgFluxo.f); tgFluxo.f = NULL; }
    pthread_mutex_lock(&tgTrava);
    // Parar de assistir não pode derrubar um download que está na fila.
    if (!tgFluxo.completo && !tgDownload(tgFluxo.arquivo)) {
        tgEnviar("{\"@type\":\"cancelDownloadFile\",\"file_id\":%d,\"only_if_pending\":false}", tgFluxo.arquivo);
        tgEnviar("{\"@type\":\"deleteFile\",\"file_id\":%d}", tgFluxo.arquivo);
    }
    tgFluxo.arquivo = 0;
    tgFluxo.ativo = false;
    tgFluxo.completo = false;
    tgFluxo.caminho[0] = '\0';
    pthread_mutex_unlock(&tgTrava);
}

// Fora da trava: o player leva tempo para abrir e usa a thread principal.
static void tgAbrirPendente() {
    extern void iniciarVideoMP4(const char* caminho);
    extern void pararVideo();
    extern bool videoRodando, bloqueio_fechar_video;
    // Fecha o vídeo em miniatura antes: o player só pode ter uma leitura de cada vez.
    if (tgAbrirAgora && videoRodando && tgPendenteTipo == TG_M_VIDEO) {
        bloqueio_fechar_video = false;
        pararVideo();
    }
    pthread_mutex_lock(&tgTrava);
    tgIniciarCopia();
    static char instalar[320];
    snprintf(instalar, sizeof(instalar), "%s", tgInstalar);
    tgInstalar[0] = '\0';
    static char externo[200];
    bool navegador = tgAbrirExterno;
    if (navegador) {
        snprintf(externo, sizeof(externo), "%s", tgLinkAtual);
        tgAbrirExterno = false;
        tgLinkAtual[0] = '\0';
    }
    bool abrir = tgAbrirAgora;
    int tipo = tgPendenteTipo;
    long long mensagem = tgPendenteMensagem;
    if (abrir) {
        TgArquivo* a = tgArquivo(tgPendente);
        snprintf(tgPendenteCaminho, sizeof(tgPendenteCaminho), "%s", a ? a->caminho : "");
        if (a && !a->baixado && tgFluxo.arquivo == tgPendente) {
            tgFluxo.ativo = true;
            tgFluxo.parar = false;
            tgFluxo.pos = 0;
            tgFluxo.pedido = 0;
            snprintf(tgPendenteCaminho, sizeof(tgPendenteCaminho), "%s", tgFluxo.caminho);
        }
        else if (tgFluxo.arquivo == tgPendente) tgFluxo.arquivo = 0;
        tgAbrirAgora = false;
        tgPendente = 0;
        tgAviso[0] = '\0';
    }
    pthread_mutex_unlock(&tgTrava);
    if (navegador) tgAbrirNoNavegador(externo);
    if (instalar[0]) {
        extern void instalarPkgLocal(const char* caminhoAbsoluto);
        instalarPkgLocal(instalar);
        pthread_mutex_lock(&tgTrava);
        snprintf(tgAviso, sizeof(tgAviso), "PKG em /data/pkg: %.100s", msgStatus);
        pthread_mutex_unlock(&tgTrava);
        tgSujo = true;
    }
    if (!abrir) return;
    if (tipo == TG_M_AUDIO) tgTocarAudio(tgPendenteCaminho, mensagem);
    else {
        tgPararAudio();
        iniciarVideoMP4(tgPendenteCaminho);
        if (!videoRodando) {
            telegramFecharFluxo();
            pthread_mutex_lock(&tgTrava);
            strcpy(tgAviso, "Não foi possível abrir este vídeo.");
            pthread_mutex_unlock(&tgTrava);
        }
    }
    tgSujo = true;
}

static void tgAbrirMidia(const TgMensagem* m) {
    TgArquivo* a = tgArquivo(m->grande);
    if (!a) { strcpy(tgAviso, "Muitos arquivos abertos. Reabra o app."); return; }
    if (m->midia == TG_M_FOTO) {
        tgVisor = m->grande;
        tgVisorMini = m->arquivo;
        tgVisorZoom = 1.0f; tgVisorX = 0; tgVisorY = 0;
        a->limite = 1920;
        a->uso = ++tgRelogioUso;
        a->quer = true;
        a->falhou = false;
        if (!a->pedido) { a->pedido = true; tgBaixarArquivo(a->id); }
        return;
    }
    tgPendente = a->id;
    tgPendenteTipo = m->midia;
    tgPendenteMensagem = m->id;
    a->pedido = true;
    if (a->baixado) { tgAbrirAgora = true; return; }
    if (m->midia == TG_M_VIDEO) {
        if (tgFluxo.ativo) { strcpy(tgAviso, "Feche o vídeo atual primeiro."); tgPendente = 0; return; }
        tgFluxo.arquivo = a->id;
        tgFluxo.caminho[0] = '\0';
        tgFluxo.prefixo = 0; tgFluxo.inicio = 0; tgFluxo.total = 0;
        tgFluxo.completo = false;
        strcpy(tgAviso, "Carregando vídeo...");
        tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":32,\"offset\":0,\"limit\":0,\"synchronous\":false}", a->id);
        return;
    }
    tgNovoDownload(a->id, m->arquivoNome[0] ? m->arquivoNome : "Áudio", m->id);
    strcpy(tgAviso, "Baixando 0%   Options: downloads");
    tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":32,\"offset\":0,\"limit\":0,\"synchronous\":false}", a->id);
}

static void tgBaixarParaPasta(const TgMensagem* m) {
    TgArquivo* a = tgArquivo(m->grande);
    if (!a) { strcpy(tgAviso, "Muitos arquivos abertos. Reabra o app."); return; }
    char nome[120];
    const char* extensao = m->midia == TG_M_FOTO ? "jpg" : m->midia == TG_M_VIDEO ? "mp4" : m->midia == TG_M_AUDIO ? "ogg" : "bin";
    if (m->arquivoNome[0]) snprintf(nome, sizeof(nome), "%s", m->arquivoNome);
    else snprintf(nome, sizeof(nome), "telegram_%lld.%s", m->id, extensao);
    for (char* p = nome; *p; p++) if (strchr("/\\:*?\"<>|", *p)) *p = '_';
    TgDownload* d = tgNovoDownload(a->id, nome, m->id);
    if (!d) { strcpy(tgAviso, "Fila de downloads cheia."); return; }
    if (d->estado >= TG_D_COPIAR && d->estado != TG_D_PRONTO && d->estado != TG_D_ERRO) return;
    snprintf(d->nome, sizeof(d->nome), "%s", nome);
    const char* ponto = strrchr(nome, '.');
    d->pkg = ponto && strcasecmp(ponto, ".pkg") == 0;
    d->salvar = true;
    a->pedido = true;
    strcpy(tgAviso, d->pkg ? "PKG na fila: instala ao terminar" : "Adicionado aos downloads");
    if (a->baixado) { d->estado = TG_D_COPIAR; return; }
    d->estado = TG_D_BAIXANDO;
    tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":16,\"offset\":0,\"limit\":0,\"synchronous\":false}", a->id);
}

static void tgAbrirLink(const char* url) {
    if (strpbrk(url, "\"\\")) { strcpy(tgAviso, "Link inválido."); return; }
    snprintf(tgLinkAtual, sizeof(tgLinkAtual), "%s", url);
    strcpy(tgAviso, "Abrindo link...");
    tgEnviar("{\"@type\":\"getInternalLinkType\",\"link\":\"%s\",\"@extra\":\"link\"}", url);
}

static void tgAbrirMenu(const TgMensagem* m) {
    tgMenuTotal = 0;
    tgMenuLinks[tgMenuTotal++] = -2;
    for (int i = 0; i < m->totalLinks; i++) tgMenuLinks[tgMenuTotal++] = i;
    if (m->grande) tgMenuLinks[tgMenuTotal++] = -1;
    tgMenuSel = 0;
    tgMenuMensagem = m->id;
    tgMenuAberto = tgMenuTotal > 0;
}

static void tgAbrirTeclado(int32_t usuario, TgModoIme modo, const wchar_t* titulo, int limite) {
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(tgTextoIme, 0, sizeof(tgTextoIme));
    param.userId = usuario;
    param.maxTextLength = limite;
    param.inputTextBuffer = tgTextoIme;
    param.title = titulo;
    param.type = (OrbisImeType)0;
    if (sceImeDialogInit(&param, NULL) >= 0) { tgImeAberto = true; tgModoIme = modo; }
}

static void tgAtualizarTeclado() {
    if (!tgImeAberto) return;
    int status = (int)sceImeDialogGetStatus();
    if (status == 0 || status == 1) return;
    OrbisDialogResult resultado;
    memset(&resultado, 0, sizeof(resultado));
    sceImeDialogGetResult(&resultado);
    int32_t botao = *(int32_t*)&resultado;
    static char texto[3200];
    tgEscapar((const uint16_t*)tgTextoIme, texto, sizeof(texto));
    sceImeDialogTerm();
    tgImeAberto = false;

    if (botao == 0 && texto[0]) {
        pthread_mutex_lock(&tgTrava);
        if (tgModoIme == TG_IME_MENSAGEM) {
            if (tgResponderA) tgEnviar("{\"@type\":\"sendMessage\",\"chat_id\":%lld,\"reply_to\":{\"@type\":\"inputMessageReplyToMessage\",\"message_id\":%lld},\"input_message_content\":{\"@type\":\"inputMessageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\"}}}", tgAberta, tgResponderA, texto);
            else tgEnviar("{\"@type\":\"sendMessage\",\"chat_id\":%lld,\"input_message_content\":{\"@type\":\"inputMessageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\"}}}", tgAberta, texto);
            tgSelId = 0;
        }
        else if (tgModoIme == TG_IME_BUSCA) tgIniciarBusca(texto);
        else {
            if (tgModoIme == TG_IME_TELEFONE) tgEnviar("{\"@type\":\"setAuthenticationPhoneNumber\",\"phone_number\":\"%s\"}", texto);
            else if (tgModoIme == TG_IME_CODIGO) tgEnviar("{\"@type\":\"checkAuthenticationCode\",\"code\":\"%s\"}", texto);
            else tgEnviar("{\"@type\":\"checkAuthenticationPassword\",\"password\":\"%s\"}", texto);
            strcpy(tgAviso, "Enviando...");
        }
        pthread_mutex_unlock(&tgTrava);
    }
    tgResponderA = 0;
    memset(texto, 0, sizeof(texto));
    memset(tgTextoIme, 0, sizeof(tgTextoIme));
    tgSujo = true;
}

// Saiu antes do vídeo abrir: para o download, salvo se também estiver na fila.
static void telegramFecharFluxoTravado() {
    if (!tgDownload(tgFluxo.arquivo)) tgEnviar("{\"@type\":\"cancelDownloadFile\",\"file_id\":%d,\"only_if_pending\":false}", tgFluxo.arquivo);
    tgFluxo.arquivo = 0;
    tgFluxo.caminho[0] = '\0';
}

// Recarrega a janela de mensagens: em volta de uma mensagem, ou a partir da mais nova.
static void tgLimparJanela() {
    tgTotalMensagens = 0;
    tgRolagem = 0;
    tgSelId = 0;
    tgAlvo = 0;
    tgFaltamNovas = false;
    tgIrPrimeira = false;
    tgPedidosHistorico = 0;
    tgFimDoHistorico = false;
    tgPedindoNovas = false;
}

static void tgCarregarEm(long long mensagem) {
    tgLimparJanela();
    tgAlvo = mensagem;
    if (mensagem) {
        tgPedidosHistorico = 1;
        tgPedindoHistorico = true;
        tgEnviar("{\"@type\":\"getChatHistory\",\"chat_id\":%lld,\"from_message_id\":%lld,\"offset\":-25,\"limit\":50,\"only_local\":false,\"@extra\":\"historico\"}", tgAberta, mensagem);
    }
    else tgPedirHistorico(0);
}

// 1 << 20 é a primeira mensagem possível no servidor.
static void tgCarregarInicio() {
    tgLimparJanela();
    tgIrPrimeira = true;
    tgPedindoHistorico = true;
    tgEnviar("{\"@type\":\"getChatHistory\",\"chat_id\":%lld,\"from_message_id\":1048576,\"offset\":-49,\"limit\":50,\"only_local\":false,\"@extra\":\"historico\"}", tgAberta);
}

static void tgAbrirConversa(long long id, long long mensagem) {
    tgEmBusca = false;
    tgMenuAberto = false;
    if (tgAberta) {
        tgGuardarPosicao(tgAberta, tgSelId);
        tgEnviar("{\"@type\":\"closeChat\",\"chat_id\":%lld}", tgAberta);
    }
    tgAberta = id;
    tgPrimeiraData = 0;
    tgBarraAtiva = false;
    tgTela = TG_MENSAGENS;
    tgAviso[0] = '\0';
    // Com não lidas continua da última lida; sem elas, volta ao ponto onde saiu.
    if (!mensagem) {
        TgConversa* c = tgConversa(id);
        mensagem = c && c->naoLidas > 0 && c->lidoAte ? c->lidoAte : tgPosicaoDe(id);
    }
    tgEnviar("{\"@type\":\"openChat\",\"chat_id\":%lld}", id);
    tgEnviar("{\"@type\":\"getChatHistory\",\"chat_id\":%lld,\"from_message_id\":1048576,\"offset\":-2,\"limit\":3,\"only_local\":false,\"@extra\":\"primeira\"}", id);
    tgCarregarEm(mensagem);
}

static void tgFecharConversa() {
    tgGuardarPosicao(tgAberta, tgSelId);
    tgEnviar("{\"@type\":\"closeChat\",\"chat_id\":%lld}", tgAberta);
    tgAberta = 0;
    tgTela = TG_CONVERSAS;
    tgAviso[0] = '\0';
    // O download segue na fila, mas não abre sozinho depois de sair da conversa.
    if (tgPendente && tgPendente == tgFluxo.arquivo && !tgFluxo.ativo) telegramFecharFluxoTravado();
    tgPendente = 0;
    tgAbrirAgora = false;
}

static int tgIndiceSelecionado() {
    if (tgSelId) for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgSelId) return i;
    return tgTotalMensagens - 1;
}

static void tgMoverSelecao(int passo) {
    if (tgTotalMensagens == 0) return;
    int i = tgIndiceSelecionado() + passo;
    if (i < 0) i = 0;
    if (i >= tgTotalMensagens - 1 && !tgFaltamNovas) {
        tgSelId = 0;
        tgMarcarLida(tgMensagens[tgTotalMensagens - 1].id);
        return;
    }
    if (i > tgTotalMensagens - 1) i = tgTotalMensagens - 1;
    tgSelId = tgMensagens[i].id;
    if (passo > 0) tgMarcarLida(tgSelId);
    if (i < 6 && !tgPedindoHistorico && !tgFimDoHistorico) tgPedirHistorico(tgMensagens[0].id);
    if (i > tgTotalMensagens - 7 && tgFaltamNovas && !tgPedindoNovas) {
        tgPedindoNovas = true;
        tgEnviar("{\"@type\":\"getChatHistory\",\"chat_id\":%lld,\"from_message_id\":%lld,\"offset\":-49,\"limit\":50,\"only_local\":false,\"@extra\":\"novas\"}", tgAberta, tgMensagens[tgTotalMensagens - 1].id);
    }
}

static void tgControlesBusca(int passo, bool xis, bool bolinha, bool triangulo, int32_t usuario) {
    if (bolinha) { tgEmBusca = false; return; }
    if (triangulo) { tgAbrirTeclado(usuario, TG_IME_BUSCA, L"Pesquisar", 100); return; }
    if (passo && tgTotalResultados > 0) tgSelResultado = (tgSelResultado + passo + tgTotalResultados) % tgTotalResultados;
    if (xis && tgTotalResultados > 0) {
        const TgResultado* r = &tgResultados[tgSelResultado];
        tgEmBusca = false;
        tgAbrirConversa(r->conversa, r->mensagem);
    }
}

// Devolve true quando o usuário sai do Telegram.
bool telegramControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    static int segurado = 0;
    tgAbrirPendente();
    if (tgImeAberto) return false;
    uint32_t novos = botoes & ~antigos;
    uint32_t direcoes = ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN;
    int passo = 0;
    if (botoes & direcoes) {
        segurado++;
        if ((novos & direcoes) || (segurado > 20 && segurado % 4 == 0)) passo = (botoes & ORBIS_PAD_BUTTON_UP) ? -1 : 1;
    }
    else segurado = 0;
    bool xis = novos & ORBIS_PAD_BUTTON_CROSS, bolinha = novos & ORBIS_PAD_BUTTON_CIRCLE;
    bool triangulo = novos & ORBIS_PAD_BUTTON_TRIANGLE, quadrado = novos & ORBIS_PAD_BUTTON_SQUARE;
    int aba = (novos & ORBIS_PAD_BUTTON_R1) ? 1 : (novos & ORBIS_PAD_BUTTON_L1) ? -1 : 0;

    // Na conversa o X decide ao soltar: toque faz a ação da mensagem, segurar abre as opções.
    static int xTempo = 0;
    static bool xNaConversa = false;
    bool xCurto = false, xLongo = false;
    if (xis && tgTela == TG_MENSAGENS && !tgVisor && !tgMenuAberto && !tgEmBusca && !tgFilaAberta) { xNaConversa = true; xTempo = 0; }
    if (botoes & ORBIS_PAD_BUTTON_CROSS) {
        if (xNaConversa && ++xTempo == 30) xLongo = true;
    }
    else {
        if (xNaConversa && xTempo < 30) xCurto = true;
        xNaConversa = false;
        xTempo = 0;
    }
    bool opcoes = novos & ORBIS_PAD_BUTTON_OPTIONS;

    // Analógico direito arrasta a barra; ao soltar, a conversa salta para aquela data.
    extern uint8_t gEmuAnaLX, gEmuAnaLY, gEmuAnaRX, gEmuAnaRY;
    bool semLeitura = !gEmuAnaLX && !gEmuAnaLY && !gEmuAnaRX && !gEmuAnaRY;
    int analogico = semLeitura ? 0 : (int)gEmuAnaRY - 128;
    bool emConversa = tgTela == TG_MENSAGENS && !tgVisor && !tgMenuAberto && !tgFilaAberta && !tgEmBusca;
    if (emConversa && (analogico > 30 || analogico < -30) && tgPrimeiraData) {
        pthread_mutex_lock(&tgTrava);
        if (!tgBarraAtiva) {
            const TgConversa* c = tgConversaPorId(tgAberta);
            tgBarraAtiva = true;
            tgBarraPos = c && tgTotalMensagens > 0 ? tgPosicaoNaBarra(c, tgMensagens[tgIndiceSelecionado()].data) : 1.0f;
        }
        tgBarraPos += (float)analogico / 12000.0f;
        if (tgBarraPos < 0.0f) tgBarraPos = 0.0f;
        if (tgBarraPos > 1.0f) tgBarraPos = 1.0f;
        pthread_mutex_unlock(&tgTrava);
        tgSujo = true;
    }
    else if (tgBarraAtiva) {
        pthread_mutex_lock(&tgTrava);
        const TgConversa* c = tgConversaPorId(tgAberta);
        tgBarraAtiva = false;
        if (c) tgEnviar("{\"@type\":\"getChatMessageByDate\",\"chat_id\":%lld,\"date\":%lld,\"@extra\":\"data\"}", tgAberta, tgDataDaBarra(c, tgBarraPos));
        pthread_mutex_unlock(&tgTrava);
        tgSujo = true;
    }
    if (!passo && !xis && !bolinha && !triangulo && !quadrado && !aba && !xCurto && !xLongo && !opcoes) return false;

    bool sair = false;
    pthread_mutex_lock(&tgTrava);
    if (tgFilaAberta) {
        if (bolinha || opcoes || aba) tgFilaAberta = false;
        else if (passo && tgTotalDownloads > 0) tgFilaSel = (tgFilaSel + passo + tgTotalDownloads) % tgTotalDownloads;
        else if (xis && tgTotalDownloads > 0) {
            TgDownload* d = &tgDownloads[tgFilaSel];
            if (d->estado == TG_D_BAIXANDO) {
                tgEnviar("{\"@type\":\"cancelDownloadFile\",\"file_id\":%d,\"only_if_pending\":false}", d->arquivo);
                if (tgPendente == d->arquivo) { tgPendente = 0; tgAviso[0] = '\0'; }
                TgArquivo* a = tgArquivo(d->arquivo);
                if (a) a->pedido = false;
                tgRemoverDownload(tgFilaSel);
            }
            else if (d->estado >= TG_D_PRONTO) tgRemoverDownload(tgFilaSel);
        }
    }
    else if (opcoes && !tgVisor && (tgTela == TG_CONVERSAS || tgTela == TG_MENSAGENS)) { tgFilaAberta = true; tgMenuAberto = false; tgFilaSel = 0; }
    else if (tgVisor) {
        if (bolinha) tgVisor = 0;
        else if (passo) {
            tgVisorZoom *= passo < 0 ? 1.25f : 0.8f;
            if (tgVisorZoom < 1.0f) { tgVisorZoom = 1.0f; tgVisorX = 0; tgVisorY = 0; }
            if (tgVisorZoom > 6.0f) tgVisorZoom = 6.0f;
        }
    }
    else if (tgMenuAberto) {
        const TgMensagem* m = NULL;
        for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgMenuMensagem) m = &tgMensagens[i];
        if (bolinha || !m) tgMenuAberto = false;
        else if (passo) tgMenuSel = (tgMenuSel + passo + tgMenuTotal) % tgMenuTotal;
        else if (xis) {
            int opcao = tgMenuLinks[tgMenuSel];
            tgMenuAberto = false;
            if (opcao == -2) { tgResponderA = m->id; tgAbrirTeclado(usuario, TG_IME_MENSAGEM, L"Responder", 500); }
            else if (opcao < 0) tgBaixarParaPasta(m);
            else tgAbrirLink(m->links[opcao]);
        }
    }
    else if (tgEmBusca) tgControlesBusca(passo, xis, bolinha, triangulo, usuario);
    else if (tgTela == TG_MENSAGENS) {
        if (bolinha) tgFecharConversa();
        else if (quadrado) tgAbrirTeclado(usuario, TG_IME_MENSAGEM, L"Mensagem", 500);
        else if (triangulo) tgAbrirTeclado(usuario, TG_IME_BUSCA, L"Pesquisar nesta conversa", 100);
        else if (passo) tgMoverSelecao(passo);
        else if (aba < 0) tgCarregarInicio();
        else if (aba > 0) tgCarregarEm(0);
        else if (xLongo && tgTotalMensagens > 0) tgAbrirMenu(&tgMensagens[tgIndiceSelecionado()]);
        else if (xCurto && tgTotalMensagens > 0) {
            const TgMensagem* m = &tgMensagens[tgIndiceSelecionado()];
            if (tgAudioRodando && tgAudioMensagem == m->id) tgAudioParar = true;
            else if (m->midia) tgAbrirMidia(m);
            else if (m->totalLinks == 1 && !m->grande) tgAbrirLink(m->links[0]);
            else if (m->totalLinks > 0) tgAbrirMenu(m);
            else if (m->grande) tgBaixarParaPasta(m);
            else tgAbrirTeclado(usuario, TG_IME_MENSAGEM, L"Mensagem", 500);
        }
    }
    else if (tgTela == TG_CONVERSAS) {
        tgOrdenarConversas();
        if (bolinha) sair = true;
        else if (triangulo) tgAbrirTeclado(usuario, TG_IME_BUSCA, L"Pesquisar", 100);
        else if (aba) {
            // Depois da primeira e da última aba fica a fila de downloads.
            int nova = tgAba + aba;
            if (nova < 0 || nova >= tgTotalAbas()) { tgFilaAberta = true; tgFilaSel = 0; }
            else {
                tgAba = nova;
                tgSelecao = 0; tgTopo = 0;
                tgCarregarConversas();
            }
        }
        else if (xis) { if (tgVisiveis > 0) tgAbrirConversa(tgConversas[tgIndice[tgSelecao]].id, 0); }
        else if (passo && tgVisiveis > 0) {
            tgSelecao = (tgSelecao + passo + tgVisiveis) % tgVisiveis;
            tgOrdenarConversas();
            if (tgSelecao >= tgVisiveis - 5) tgCarregarConversas();
        }
    }
    else if (bolinha) sair = true;
    else if (xis && tgTela == TG_TELEFONE) tgAbrirTeclado(usuario, TG_IME_TELEFONE, L"Telefone com DDI, como +5511999998888", 24);
    else if (xis && tgTela == TG_CODIGO) tgAbrirTeclado(usuario, TG_IME_CODIGO, L"Codigo do Telegram", 16);
    else if (xis && tgTela == TG_SENHA) tgAbrirTeclado(usuario, TG_IME_SENHA, L"Senha de duas etapas", 128);
    pthread_mutex_unlock(&tgTrava);
    tgSujo = true;
    return sair;
}

// O redesenho completo é o custo da tela: limita a 12 por segundo e copia o quadro nos demais.
void renderizarTelegram(uint32_t* p) {
    static int espera = 0;
    tgAtualizarTeclado();
    // O analógico esquerdo move a foto ampliada no visor.
    if (tgVisor && tgVisorZoom > 1.0f) {
        extern uint8_t gEmuAnaLX, gEmuAnaLY;
        int lx = (int)gEmuAnaLX - 128, ly = (int)gEmuAnaLY - 128;
        if (lx > 30 || lx < -30 || ly > 30 || ly < -30) {
            tgVisorX -= (lx > 30 || lx < -30) ? lx / 4 : 0;
            tgVisorY -= (ly > 30 || ly < -30) ? ly / 4 : 0;
            tgSujo = true;
        }
    }
    if (!tgQuadro) tgQuadro = (uint32_t*)malloc(1920 * 1080 * 4);
    if (!tgQuadro) return;
    if (espera > 0) espera--;
    if (tgSujo && espera == 0) {
        tgSujo = false;
        espera = 5;
        pthread_mutex_lock(&tgTrava);
        tgDesenhar(tgQuadro);
        pthread_mutex_unlock(&tgTrava);
        // Desenha na ordem RGBA das imagens; a tela do PS4 espera vermelho e azul trocados.
        for (int i = 0; i < 1920 * 1080; i++) {
            uint32_t c = tgQuadro[i];
            tgQuadro[i] = (c & 0xFF00FF00u) | ((c & 255) << 16) | ((c >> 16) & 255);
        }
    }
    memcpy(p, tgQuadro, 1920 * 1080 * 4);
}

static bool tgLerChaves() {
    FILE* f = fopen(TELEGRAM_PASTA "/api.txt", "r");
    if (!f) return false;
    char id[32] = "", hash[64] = "";
    bool leu = fgets(id, sizeof(id), f) && fgets(hash, sizeof(hash), f);
    fclose(f);
    if (!leu) return false;
    hash[strcspn(hash, "\r\n ")] = '\0';
    tgApiId = atoi(id);
    if (tgApiId <= 0 || strlen(hash) != 32 || strspn(hash, "0123456789abcdefABCDEF") != 32) return false;
    strcpy(tgApiHash, hash);
    return true;
}

static void iniciarTelegram() {
    static bool iniciado = false;
    if (!iniciado) {
        if (!tgLerChaves()) { strcpy(msgStatus, "TELEGRAM: FALTA O ARQUIVO api.txt"); msgTimer = 300; return; }
        sceKernelMkdir(TELEGRAM_PASTA "/conta", 0777);
        sceKernelMkdir(TELEGRAM_PASTA "/files", 0777);
        int fuso = 0, verao = 0;
        if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE, &fuso) == 0 && fuso >= -720 && fuso <= 840) tgFusoMinutos = fuso;
        if (sceSystemServiceParamGetInt((OrbisSystemServiceParamId)5, &verao) == 0 && verao == 1) tgFusoMinutos += 60;
        tgAbrirEmojis("/app0/assets/emoji.pack");
        tgLerPosicoes();
        pthread_mutex_init(&tgTrava, NULL);
        pthread_t t, imagens;
        if (pthread_create(&t, NULL, threadTelegram, NULL) != 0 || pthread_create(&imagens, NULL, threadImagens, NULL) != 0) { strcpy(msgStatus, "TELEGRAM: FALHA AO INICIAR"); msgTimer = 300; return; }
        pthread_detach(t);
        pthread_detach(imagens);
        iniciado = true;
    }
    tgSujo = true;
    menuAtual = MENU_TELEGRAM;
}

