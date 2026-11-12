#include "extra.h"
#include "menu.h"
#include "graphics.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/AudioIn.h>
#include <orbis/libkernel.h>
#include <pthread.h>

extern char msgStatus[128];
extern int msgTimer;

extern int sel;
extern MenuLevel menuAtual;
extern int totalItens;
extern char nomes[3000][256];

// PONTE: Lê o comando já aberto pelo main.cpp
extern int globalPadHandle;

// SFX EXTERNOS (Elementos Sonoros)
extern int16_t* sfxUpData; extern size_t sfxUpLen;
extern int16_t* sfxDownData; extern size_t sfxDownLen;
extern int16_t* sfxCrossData; extern size_t sfxCrossLen;
extern int16_t* sfxCircleData; extern size_t sfxCircleLen;

// Matrizes para guardar o rastro (gráfico) contínuo
static bool trilhaL[256][256];
static bool trilhaR[256][256];

// AGORA TEMOS DUAS MATRIZES DE RASTRO (Uma para cada dedo)
static bool trilhaTouch1[576][282];
static bool trilhaTouch2[576][282];

// Guarda a posição anterior para desenhar linhas perfeitas (Bresenham)
static int prevLx = 128, prevLy = 128;
static int prevRx = 128, prevRy = 128;

// Posições anteriores independentes para os dois dedos
static int prevT1x = -1, prevT1y = -1;
static int prevT2x = -1, prevT2y = -1;

// CONTROLE DE HARDWARE (Som e Luz)
static int padAudioPort = -1;
static int padMicPort = -1;
static int micLevelDisplay = 0;
static uint32_t prevButtonsTest = 0;

struct ColorPreset { const char* name; uint8_t r, g, b; uint32_t btn; };
static ColorPreset colors[] = {
    {"BRANCO (CIMA)", 255, 255, 255, ORBIS_PAD_BUTTON_UP},
    {"AMARELO (BAIXO)", 255, 255, 0, ORBIS_PAD_BUTTON_DOWN},
    {"CYAN (ESQUERDA)", 0, 255, 255, ORBIS_PAD_BUTTON_LEFT},
    {"LIMA (DIREITA)", 128, 255, 0, ORBIS_PAD_BUTTON_RIGHT},
    {"VERDE (TRIAN.)", 0, 255, 0, ORBIS_PAD_BUTTON_TRIANGLE},
    {"VERMELHO (BOLA)", 255, 0, 0, ORBIS_PAD_BUTTON_CIRCLE},
    {"AZUL (CRUZ)", 0, 0, 255, ORBIS_PAD_BUTTON_CROSS},
    {"MAGENTA (QUADRADO)", 255, 0, 255, ORBIS_PAD_BUTTON_SQUARE}
};

void preencherMenuExtra() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Teste de Controle (Analise de Drift e Botoes)");
    strcpy(nomes[1], "Piano Virtual (Instrumento)");
    strcpy(nomes[2], "Telegram");
    strcpy(nomes[3], "Ventoinha e temperatura");
    strcpy(nomes[4], "Inteligencia Artificial (Gemini)");
    strcpy(nomes[5], "Chaves de API");
    strcpy(nomes[6], "Previsao do tempo");
    totalItens = 7;
    menuAtual = MENU_EXTRA;
    sel = 0;
}

void finalizarControleTeste() {
    if (padAudioPort >= 0) {
        sceAudioOutClose(padAudioPort);
        padAudioPort = -1;
    }
    if (padMicPort >= 0) {
        // No OpenOrbis sceAudioInClose as vezes nao recebe handle ou usa global
        ((void(*)())sceAudioInClose)(); 
        padMicPort = -1;
    }
    // Reseta a luz para o padrão (Desligado ou cor neutra)
    OrbisPadColor color = { 0, 0, 0 };
    scePadSetLightBar(globalPadHandle, &color);
    prevButtonsTest = 0;
}

// =======================================================
// TESTE DO TDLIB: confere no console se a biblioteca do Telegram inicia e alcança a rede.
// A chave abaixo é falsa de propósito: uma resposta de API_ID inválido já prova rede e criptografia.
// =======================================================
extern "C" {
int td_create_client_id();
void td_send(int client_id, const char* request);
const char* td_receive(double timeout);
const char* td_execute(const char* request);
}

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include "ImeDialog.h"
#include "stb_image.h"
#include <math.h>
#include <orbis/SystemService.h>
#include <dirent.h>
#include <strings.h>
#include <sys/types.h>
extern "C" {
typedef struct OpusEncoder OpusEncoder;
OpusEncoder* opus_encoder_create(int32_t fs, int canais, int aplicacao, int* erro);
int32_t opus_encode(OpusEncoder* e, const int16_t* pcm, int amostras, unsigned char* saida, int32_t maximo);
int opus_encoder_ctl(OpusEncoder* e, int pedido, ...);
void opus_encoder_destroy(OpusEncoder* e);
}
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include "remoto_ime.h"   // teclado do PS4 recebe texto do celular
}

extern "C" int sysctl(const int* nome, unsigned int tamanhoNome, void* saida, size_t* tamanhoSaida, const void* novo, size_t tamanhoNovo);

// O OpenSSL do SDK procura esta função, que o PS4 não exporta. Sem fonte segura ela falha de
// propósito: chave do Telegram gerada com aleatório fraco é pior do que não abrir.
extern "C" int getentropy(void* destino, size_t tamanho) {
    unsigned char* p = (unsigned char*)destino;
    size_t feito = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        while (feito < tamanho) {
            ssize_t n = read(fd, p + feito, tamanho - feito);
            if (n <= 0) break;
            feito += (size_t)n;
        }
        close(fd);
    }
    static const int kernArnd[2] = { 1, 37 };   // CTL_KERN, KERN_ARND
    while (feito < tamanho) {
        size_t n = tamanho - feito > 256 ? 256 : tamanho - feito;
        if (sysctl(kernArnd, 2, p + feito, &n, NULL, 0) != 0 || n == 0) return -1;
        feito += n;
    }
    return 0;
}

#define TELEGRAM_PASTA "/data/HyperNeivaTelegram"
#define TG_MAX_CONVERSAS 600
#define TG_MAX_PASTAS 14
#define TG_MAX_RESULTADOS 60
#define TG_MAX_MENSAGENS 1500
#define TG_MAX_DOWNLOADS 24
#define TG_MAX_USUARIOS 800
#define TG_COR(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

enum TgTela { TG_INICIANDO, TG_TELEFONE, TG_CODIGO, TG_SENHA, TG_SEM_SUPORTE, TG_CONVERSAS, TG_MENSAGENS };
enum TgModoIme { TG_IME_TELEFONE, TG_IME_CODIGO, TG_IME_SENHA, TG_IME_MENSAGEM, TG_IME_BUSCA, TG_IME_PASTA, TG_IME_GRUPO, TG_IME_CANAL };
enum TgMidia { TG_M_NADA, TG_M_FOTO, TG_M_VIDEO, TG_M_AUDIO };

// ordens: 0 lista principal, 1 arquivadas, 2 em diante as pastas.
struct TgConversa { long long id, data, lidoAte; long long ordens[TG_MAX_PASTAS + 2]; int naoLidas, foto; char titulo[160]; char ultima[260]; int mudo, tipo; long long grupoId; };   // tipo: 0 pessoa, 1 grupo, 2 canal
struct TgMensagem { long long id, autor, data; bool minha, figurinha; int arquivo, imgL, imgA, grande, midia, totalLinks, alturaCache[2]; char rotulo[32]; char resumo[48]; char arquivoNome[100]; char links[12][200]; char texto[1024]; };
struct TgPasta { int id; char nome[64]; };
enum TgEstadoDownload { TG_D_BAIXANDO, TG_D_COPIAR, TG_D_COPIANDO, TG_D_PRONTO, TG_D_ERRO };
struct TgDownload { int arquivo, estado; bool salvar, pkg, iniciado; long long total, baixado, mensagem, conversa, proxima; int tentativas; char nome[120]; long long inicioMs, marcaMs, marcaBytes; double velocidade; };
struct TgResultado { long long conversa, mensagem, data; char texto[200]; };
struct TgUsuario { long long id; char nome[64]; };

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
        char artista[120] = "";
        tgTexto(c, fim, "\"title\":\"", nome, sizeof(nome));
        tgTexto(c, fim, "\"performer\":\"", artista, sizeof(artista));
        tgLerAbrir(c, fim, "\"audio\":{\"@type\":\"file\",\"id\":", TG_M_AUDIO, m);
        // Título e artista quando houver, e sempre o nome do arquivo (com a extensão).
        if (nome[0]) snprintf(m->texto, sizeof(m->texto), "🎵 %s%s%s%s%s%s", artista, artista[0] ? " - " : "", nome,
            m->arquivoNome[0] ? "  (" : "", m->arquivoNome, m->arquivoNome[0] ? ")" : "");
        else snprintf(m->texto, sizeof(m->texto), "🎵 %s", m->arquivoNome[0] ? m->arquivoNome : "Música");
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

static bool tgColetando = false;   // o menu do X está juntando todos os links
static void tgMenuGuardarUrl(const char* url);
static void tgAdicionarLink(TgMensagem* m, const char* url) {
    if (tgColetando) { if (url[0]) tgMenuGuardarUrl(url); return; }
    if (!url[0] || m->totalLinks >= 12) return;
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
static void tgLerLinksTexto(const char* conteudo, const char* fim, const char* t, TgMensagem* m) {
    tgLerLinksMarcados(conteudo, fim, "\"@type\":\"textEntityTypeTextUrl\",\"url\":\"", m);
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
static void tgLerLinks(const char* conteudo, const char* fim, TgMensagem* m) { tgLerLinksTexto(conteudo, fim, m->texto, m); }

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
// Itens do menu do X: índice em tgMenuUrls, -1 baixa, -2 responde, -3 encaminha, -4 seleciona, -5 copia.
// Sem limite: o menu busca a mensagem inteira e lista todos os links dela.
static int* tgMenuLinks = NULL;
static int tgMenuCap = 0;
static char (*tgMenuUrls)[200] = NULL;
static int tgMenuTotalUrls = 0, tgMenuCapUrls = 0;
static void tgMenuItem(int valor);
static void tgMenuGuardarUrl(const char* url);
// Selecionar várias mensagens para encaminhar, apagar ou copiar.
static bool tgSelecionando = false;
static long long tgMarcadas[100];
static int tgTotalMarcadas = 0;
static bool tgSelMenuAberto = false, tgConfirmarApagar = false;
static int tgSelMenuSel = 0;
// Texto copiado: o Quadrado oferece colar no teclado.
static char tgCopiado[1800] = "";
// Quadrado na conversa: escrever, enviar arquivo ou gravar áudio.
static bool tgAcoesAberto = false;
static int tgAcoesSel = 0;
// Filtro da conversa (Quadrado > Filtro), menu do Quadrado na lista de conversas e confirmação
// com dois X para sair de grupo ou apagar pasta.
static int tgFiltro = 0;
static long long tgMaisAntiga = 0;
static bool tgListaMenuAberto = false;
static int tgListaMenuSel = 0;
static bool tgConfirmar = false;
static long long tgChatDaPasta = 0;
static char tgConviteLink[220] = "";
static volatile bool tgAudioFluxo = false;
static bool tgAcoesSub = false;          // Quadrado na conversa: submenu "Opções do grupo/canal"
static int tgListaModo = 0;              // Quadrado na lista: 0 principal, 1 escolher pasta, 2 opções da conversa, 3 marcar conversas da pasta nova
static char tgNovaPastaNome[64] = "";
static long long tgNovaPastaIds[100];
static int tgNovaPastaTotal = 0;
static long long tgEditarMsg = 0;        // mensagem sendo editada (segure X > Editar)
// Membro ou não de cada grupo/canal (updateSupergroup / updateBasicGroup).
struct TgGrupoStatus { long long id; bool membro; };
static TgGrupoStatus tgGrupos[4000];
static int tgTotalGrupos = 0;
static void tgGuardarStatusGrupo(long long id, bool membro) {
    for (int i = 0; i < tgTotalGrupos; i++) if (tgGrupos[i].id == id) { tgGrupos[i].membro = membro; return; }
    if (tgTotalGrupos < 4000) { tgGrupos[tgTotalGrupos].id = id; tgGrupos[tgTotalGrupos].membro = membro; tgTotalGrupos++; }
}
#define TG_TOTAL_FILTROS 7
static const char* tgNomesFiltro[TG_TOTAL_FILTROS] = { "Tudo", "Só mensagens", "Só arquivos", "Só vídeos", "Só áudios", "Só fotos", "Só links" };
static const char* tgFiltroTdlib[TG_TOTAL_FILTROS] = { "", "", "searchMessagesFilterDocument", "searchMessagesFilterVideo", "searchMessagesFilterAudio", "searchMessagesFilterPhoto", "searchMessagesFilterUrl" };
// Encaminhar: a mensagem escolhida vai para a conversa marcada na lista da esquerda.
static long long tgEncaminharMsg = 0, tgEncaminharDe = 0;
// Escolha de arquivo para enviar.
struct TgEntrada { char nome[100]; bool pasta; };
static bool tgSeletorAberto = false;
static char tgSeletorPasta[300] = "/data";
static TgEntrada tgSeletorItens[300];
static int tgSeletorTotal = 0, tgSeletorSel = 0;
// Gravação de áudio pelo microfone do fone ligado no controle.
static volatile bool tgGravando = false, tgGravacaoParar = false, tgGravacaoCancelar = false;
static volatile int tgGravacaoSegundos = 0;
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
static wchar_t tgUltimaBusca[128];
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
    tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":24,\"offset\":0,\"limit\":0,\"synchronous\":false}", id);
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
static int tgPacoteFd = -1;
static char tgEmojiMotivo[96] = "nao aberto";
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
    if (d) { fprintf(d, "emojis=%d (%s) fontes=%d\n", tgTotalEmojis, tgEmojiMotivo, tgTotalFontes); fclose(d); }
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

// Os ícones e emojis vêm deste pacote: lido aos pedaços, com o motivo anotado se falhar.
static bool tgLerEm(int fd, void* destino, size_t tamanho, off_t posicao) {
    size_t feito = 0;
    while (feito < tamanho) {
        // pread da libc dava EBADF no /app0: usa a leitura do próprio kernel do PS4.
        ssize_t n = (ssize_t)sceKernelPread(fd, (char*)destino + feito, tamanho - feito, posicao + (off_t)feito);
        if (n <= 0 || (size_t)n > tamanho - feito) return false;
        feito += (size_t)n;
    }
    return true;
}
static void tgAbrirEmojis(const char* caminho) {
    (void)tgPacoteEmoji;
    int fd = sceKernelOpen(caminho, 0 /* leitura */, 0);
    if (fd < 0) { snprintf(tgEmojiMotivo, sizeof(tgEmojiMotivo), "open %s erro=%x", caminho, (unsigned)fd); return; }
    uint32_t total = 0;
    if (!tgLerEm(fd, &total, 4, 0) || total == 0 || total > 20000) {
        snprintf(tgEmojiMotivo, sizeof(tgEmojiMotivo), "cabecalho total=%u", total);
        sceKernelClose(fd);
        return;
    }
    TgEmojiIndice* lista = (TgEmojiIndice*)malloc(sizeof(TgEmojiIndice) * total);
    if (!lista) { snprintf(tgEmojiMotivo, sizeof(tgEmojiMotivo), "sem memoria"); sceKernelClose(fd); return; }
    if (!tgLerEm(fd, lista, sizeof(TgEmojiIndice) * total, 4)) {
        snprintf(tgEmojiMotivo, sizeof(tgEmojiMotivo), "indice");
        free(lista);
        sceKernelClose(fd);
        return;
    }
    tgPacoteFd = fd;
    tgPacoteBase = 4 + total * (uint32_t)sizeof(TgEmojiIndice);
    tgTotalEmojis = (int)total;
    tgEmojis = lista;
    snprintf(tgEmojiMotivo, sizeof(tgEmojiMotivo), "ok");
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
    if (tgLerEm(tgPacoteFd, dados, tamanho, (off_t)(tgPacoteBase + tgEmojis[indice].inicio))) {
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
                    if (!fonte) {
                        bool emoji = cp >= 0x1F000 || (cp >= 0x2190 && cp <= 0x2BFF) || cp == 0x3030 || cp == 0x303D;
                        fonte = &font; usado = emoji ? ' ' : '?'; tgAnotarFaltando(cp);
                    }
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
        tgEscrever(p, tgEmBusca ? "X  Abrir     Triângulo  Nova pesquisa     O  Fechar" : "X  Abrir     Quadrado  Pastas e grupos     Triângulo  Pesquisar     L1 / R1  Pastas     Options  Downloads     O  Sair", 0, 1 << 30, 22, x + 40, 1034, TG_COR(125, 139, 153));
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
            bool marcada = false;
            for (int k = 0; k < tgTotalMarcadas && !marcada; k++) marcada = tgMarcadas[k] == tgMensagens[i].id;
            if (i == selecionada || marcada) {
                int y0 = y - 4 < topo ? topo : y - 4, y1 = y + alturas[i] + 4 > base ? base : y + alturas[i] + 4;
                tgRetangulo(p, x, y0, l, y1 - y0, marcada ? (i == selecionada ? TG_COR(45, 90, 135) : TG_COR(33, 68, 104)) : TG_COR(26, 40, 56));
                if (marcada) tgRetangulo(p, x, y0, 8, y1 - y0, TG_COR(100, 180, 240));
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
    tgEscreverCortado(p, c->titulo, 28, x + 94, 25, l - 130 - (tgFiltro ? 330 : 0), TG_COR(255, 255, 255));
    if (tgFiltro) {
        char f[64]; snprintf(f, sizeof(f), "Filtro: %s", tgNomesFiltro[tgFiltro]);
        int w = tgLargura(f, 22) + 32;
        tgArredondado(p, x + l - w - 24, 22, w, 40, 20, TG_COR(46, 112, 165), 0, 1080);
        tgEscrever(p, f, 0, 1 << 30, 22, x + l - w - 8, 30, TG_COR(255, 255, 255));
    }

    const TgMensagem* m = tgTotalMensagens > 0 ? &tgMensagens[selecionada] : NULL;
    const char* acao = "X  Escrever";
    if (m && tgAudioRodando && tgAudioMensagem == m->id) acao = "X  Parar";
    else if (m && m->midia == TG_M_AUDIO) acao = "X  Tocar";
    else if (m && m->midia == TG_M_VIDEO) acao = "X  Assistir";
    else if (m && m->midia) acao = "X  Abrir";
    else if (m && m->totalLinks > 0) acao = "X  Abrir link";
    else if (m && m->grande) acao = "X  Baixar";
    char dica[140];
    snprintf(dica, sizeof(dica), "%s     Segure X  Opções     Quadrado  Enviar e opções do grupo     Triângulo  Pesquisar     O  Voltar", acao);
    if (tgEmBusca) snprintf(dica, sizeof(dica), "X  Abrir resultado     Triângulo  Nova pesquisa     O  Fechar");
    if (tgSelecionando) snprintf(dica, sizeof(dica), "%d selecionada(s)     X  Marcar / desmarcar     Quadrado  Encaminhar, apagar, copiar     O  Sair da seleção", tgTotalMarcadas);
    tgRetangulo(p, x, base, l, TG_BASE_H, TG_COR(23, 33, 43));
    tgArredondado(p, x + 24, base + 14, l - 48, 48, 24, TG_COR(36, 47, 61), 0, 1080);
    tgEscrever(p, dica, 0, 1 << 30, 22, x + 50, base + 26, TG_COR(150, 165, 180));
}

static const char* tgIcone(const char* comEmoji, const char* semEmoji);
static void tgDesenharMenu(uint32_t* p) {
    const TgMensagem* m = NULL;
    for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgMenuMensagem) m = &tgMensagens[i];
    if (!m) return;
    const int maximo = 13;
    int linhas = tgMenuTotal < maximo ? tgMenuTotal : maximo;
    int topo = tgMenuSel >= linhas ? tgMenuSel - linhas + 1 : 0;
    int largura = 1000, altura = 70 + linhas * 58 + (tgMenuTotal > linhas ? 50 : 16);
    int x = TG_LISTA_L + 2 + (1920 - TG_LISTA_L - largura) / 2, y = (1080 - altura) / 2;
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(30, 44, 58), 0, 1080);
    tgEscrever(p, "Opções", 0, 1 << 30, 28, x + 28, y + 18, TG_COR(255, 255, 255));
    for (int j = 0; j < linhas; j++) {
        int i = topo + j;
        int linha = y + 70 + j * 58;
        if (i == tgMenuSel) tgArredondado(p, x + 12, linha, largura - 24, 54, 12, TG_COR(46, 112, 165), 0, 1080);
        int op = tgMenuLinks[i];
        const char* nome = op >= 0 ? (op < tgMenuTotalUrls ? tgMenuUrls[op] : "") :
            op == -2 ? tgIcone("↩️ Responder", "Responder") : op == -3 ? tgIcone("➡️ Encaminhar", "Encaminhar") :
            op == -4 ? tgIcone("☑️ Selecionar", "Selecionar") : op == -5 ? tgIcone("📋 Copiar texto", "Copiar texto") :
            op == -6 ? tgIcone("✏️ Editar", "Editar") :
            tgIcone("⬇️ Baixar e salvar", "Baixar e salvar");
        tgEscreverCortado(p, nome, 24, x + 30, linha + 12, largura - 60, TG_COR(245, 245, 245));
    }
    if (tgMenuTotal > linhas) {
        char mais[48];
        snprintf(mais, sizeof(mais), "%d de %d", tgMenuSel + 1, tgMenuTotal);
        tgEscrever(p, mais, 0, 1 << 30, 20, x + largura - 30 - tgLargura(mais, 20), y + altura - 38, TG_COR(150, 165, 180));
    }
}

static uint64_t tgAgoraMs() { return sceKernelGetProcessTime() / 1000; }
static void tgFormatarTempo(long long s, char* o, int t) {
    if (s < 0) s = 0;
    if (s >= 3600) snprintf(o, t, "%lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60);
    else snprintf(o, t, "%lld:%02lld", s / 60, s % 60);
}
// "45%  2.3 MB/s  1:12  faltam 0:35"
static void tgInfoDownload(const TgDownload* d, char* o, int t) {
    int porcento = d->total > 0 ? (int)(d->baixado * 100 / d->total) : 0;
    if (!d->inicioMs || d->velocidade <= 1.0) { snprintf(o, t, "%d%%", porcento); return; }
    char vel[24], dec[16], resta[16];
    if (d->velocidade >= 1048576.0) snprintf(vel, sizeof(vel), "%.1f MB/s", d->velocidade / 1048576.0);
    else snprintf(vel, sizeof(vel), "%.0f KB/s", d->velocidade / 1024.0);
    tgFormatarTempo((long long)((tgAgoraMs() - d->inicioMs) / 1000), dec, sizeof(dec));
    if (d->total > 0) { tgFormatarTempo((long long)((d->total - d->baixado) / d->velocidade), resta, sizeof(resta)); snprintf(o, t, "%d%%  %s  %s  faltam %s", porcento, vel, dec, resta); }
    else snprintf(o, t, "%d%%  %s  %s", porcento, vel, dec);
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
        char estado[96];
        int porcento = d->total > 0 ? (int)(d->baixado * 100 / d->total) : 0;
        if (d->estado == TG_D_BAIXANDO && !d->iniciado) strcpy(estado, "Na fila");
        else if (d->estado == TG_D_BAIXANDO) tgInfoDownload(d, estado, sizeof(estado));
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

// Monta a faixa com a mensagem do vídeo: nome do arquivo e legenda. Devolve a altura usada.
static int tgMontarLegenda(const TgMensagem* m, uint32_t* p, int alturaMaxima) {
    const int tam = 28, linha = 38, largura = 1760;
    static char texto[1200];
    snprintf(texto, sizeof(texto), "%s%s%s", m->texto, m->texto[0] && m->arquivoNome[0] ? "\n" : "", m->arquivoNome);
    if (!texto[0]) return 0;
    int linhas = 0, i = 0;
    while (texto[i]) { int proximo; tgQuebrar(texto, i, tam, largura, &proximo); linhas++; i = proximo; }
    int altura = linhas * linha + 32;
    if (altura > alturaMaxima) altura = alturaMaxima;
    for (int k = 0; k < 1920 * altura; k++) p[k] = TG_COR(18, 24, 32);
    int y = 16;
    i = 0;
    while (texto[i] && y + linha <= altura) {
        int proximo;
        int fim = tgQuebrar(texto, i, tam, largura, &proximo);
        tgEscreverComLinks(p, texto, i, fim, tam, 80, y, TG_COR(245, 245, 245));
        y += linha;
        i = proximo;
    }
    return altura;
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

static const char* tgIcone(const char* comEmoji, const char* semEmoji) { return tgEmojis ? comEmoji : semEmoji; }

// Itens do Quadrado na conversa: enviar e as opções do grupo/canal.
enum { TG_A_ESCREVER, TG_A_ARQUIVO, TG_A_GRAVAR, TG_A_COLAR, TG_A_FILTRO, TG_A_MUDO, TG_A_LIDO, TG_A_ARQUIVAR, TG_A_ENTRAR, TG_A_SAIR, TG_A_OPCOES };
// 1 membro, 0 fora, -1 ainda não sabe.
static int tgMembro(const TgConversa* c) {
    if (!c || !c->tipo || !c->grupoId) return -1;
    for (int i = 0; i < tgTotalGrupos; i++) if (tgGrupos[i].id == c->grupoId) return tgGrupos[i].membro ? 1 : 0;
    return -1;
}
static int tgAcoesItens(int* itens) {
    int n = 0;
    const TgConversa* c = tgConversaPorId(tgAberta);
    if (!tgAcoesSub) {
        itens[n++] = TG_A_ESCREVER; itens[n++] = TG_A_ARQUIVO; itens[n++] = TG_A_GRAVAR;
        if (tgCopiado[0]) itens[n++] = TG_A_COLAR;
        itens[n++] = TG_A_OPCOES;
        return n;
    }
    itens[n++] = TG_A_FILTRO; itens[n++] = TG_A_MUDO; itens[n++] = TG_A_LIDO; itens[n++] = TG_A_ARQUIVAR;
    int membro = tgMembro(c);
    if (c && c->tipo && membro != 1) itens[n++] = TG_A_ENTRAR;
    if (c && c->tipo && membro != 0) itens[n++] = TG_A_SAIR;
    return n;
}
static void tgRotuloConversa(int item, const TgConversa* c, char* s, int t) {
    const char* tipo = c && c->tipo == 2 ? "canal" : "grupo";
    switch (item) {
    case TG_A_ESCREVER: snprintf(s, t, "%s", tgIcone("✏️ Escrever mensagem", "Escrever mensagem")); break;
    case TG_A_ARQUIVO: snprintf(s, t, "%s", tgIcone("📎 Enviar arquivo", "Enviar arquivo")); break;
    case TG_A_GRAVAR: snprintf(s, t, "%s", tgIcone("🎤 Gravar áudio", "Gravar áudio")); break;
    case TG_A_COLAR: snprintf(s, t, "%s", tgIcone("📋 Colar texto copiado", "Colar texto copiado")); break;
    case TG_A_FILTRO: snprintf(s, t, "%s: %s   (X troca)", tgIcone("🔎 Mostrar", "Mostrar"), tgNomesFiltro[tgFiltro]); break;
    case TG_A_MUDO: snprintf(s, t, "%s", c && c->mudo ? tgIcone("🔔 Ativar notificações", "Ativar notificações") : tgIcone("🔕 Silenciar", "Silenciar")); break;
    case TG_A_LIDO: snprintf(s, t, "%s", tgIcone("✅ Marcar tudo como lido", "Marcar tudo como lido")); break;
    case TG_A_ARQUIVAR: snprintf(s, t, "%s", c && c->ordens[1] ? tgIcone("📤 Desarquivar", "Desarquivar") : tgIcone("🗄 Arquivar", "Arquivar")); break;
    case TG_A_ENTRAR: snprintf(s, t, "%s %s", tgIcone("➕ Entrar no", "Entrar no"), tipo); break;
    case TG_A_SAIR: if (tgConfirmar) snprintf(s, t, "Aperte X de novo para sair do %s", tipo); else snprintf(s, t, "%s %s", tgIcone("🚪 Sair do", "Sair do"), tipo); break;
    case TG_A_OPCOES: snprintf(s, t, "%s %s  ›", tgIcone("⚙️ Opções", "Opções"), c && c->tipo == 2 ? "do canal" : c && c->tipo == 1 ? "do grupo" : "da conversa"); break;
    }
}
static void tgDesenharMenuGenerico(uint32_t* p, const char* titulo, const char (*rotulos)[160], int total, int sel) {
    int largura = 820, visiveis = total < 13 ? total : 13;
    int altura = 70 + visiveis * 58 + 60;
    int x = TG_LISTA_L + 2 + (1920 - TG_LISTA_L - largura) / 2, y = (1080 - altura) / 2;
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(30, 44, 58), 0, 1080);
    tgEscrever(p, titulo, 0, 1 << 30, 28, x + 28, y + 18, TG_COR(255, 255, 255));
    int topo = sel >= visiveis ? sel - visiveis + 1 : 0;
    for (int k = 0; k < visiveis && topo + k < total; k++) {
        int i = topo + k, linha = y + 70 + k * 58;
        if (i == sel) tgArredondado(p, x + 12, linha, largura - 24, 54, 12, TG_COR(46, 112, 165), 0, 1080);
        tgEscreverCortado(p, rotulos[i], 24, x + 30, linha + 12, largura - 60, TG_COR(245, 245, 245));
    }
    tgEscrever(p, "X  Escolher     O  Fechar", 0, 1 << 30, 22, x + 28, y + altura - 42, TG_COR(150, 165, 180));
}
static void tgDesenharAcoes(uint32_t* p) {
    int itens[16], total = tgAcoesItens(itens);
    const TgConversa* c = tgConversaPorId(tgAberta);
    static char rotulos[16][160];
    for (int i = 0; i < total; i++) tgRotuloConversa(itens[i], c, rotulos[i], sizeof(rotulos[i]));
    if (tgAcoesSel >= total) tgAcoesSel = total - 1;
    const char* titulo = !tgAcoesSub ? "Enviar" : c && c->tipo == 2 ? "Opções do canal" : c && c->tipo == 1 ? "Opções do grupo" : "Opções da conversa";
    tgDesenharMenuGenerico(p, titulo, rotulos, total, tgAcoesSel);
}

// Itens do Quadrado na lista de conversas. Modos: 0 principal, 1 escolher a pasta, 2 opções da
// conversa marcada, 3 marcar as conversas da pasta nova.
enum { TG_L_PASTA = 0, TG_L_NOVA_PASTA = 1000, TG_L_ADD_PASTA, TG_L_OPCOES, TG_L_APAGAR_PASTA, TG_L_GRUPO, TG_L_CANAL, TG_L_MUDO, TG_L_ARQUIVAR, TG_L_SAIR, TG_L_CRIAR_OK, TG_L_MARCAR = 2000 };
#define TG_L_MAX_ITENS 330
static TgConversa* tgConversaMarcadaNaLista() { return tgVisiveis > 0 ? &tgConversas[tgIndice[tgSelecao]] : NULL; }
static bool tgNaNovaPasta(long long id) { for (int i = 0; i < tgNovaPastaTotal; i++) if (tgNovaPastaIds[i] == id) return true; return false; }
static int tgListaItens(int* itens) {
    int n = 0;
    TgConversa* c = tgConversaMarcadaNaLista();
    if (tgListaModo == 1) { for (int i = 0; i < tgTotalPastas; i++) itens[n++] = TG_L_PASTA + i; return n; }
    if (tgListaModo == 2) {
        if (c) { itens[n++] = TG_L_MUDO; itens[n++] = TG_L_ARQUIVAR; if (c->tipo && tgMembro(c) != 0) itens[n++] = TG_L_SAIR; }
        return n;
    }
    if (tgListaModo == 3) {
        itens[n++] = TG_L_CRIAR_OK;
        for (int j = 0; j < tgVisiveis && n < TG_L_MAX_ITENS; j++) itens[n++] = TG_L_MARCAR + j;
        return n;
    }
    itens[n++] = TG_L_NOVA_PASTA;
    if (c && tgTotalPastas > 0) itens[n++] = TG_L_ADD_PASTA;
    if (tgAba >= 1 && tgAba <= tgTotalPastas) itens[n++] = TG_L_APAGAR_PASTA;
    itens[n++] = TG_L_GRUPO; itens[n++] = TG_L_CANAL;
    if (c) itens[n++] = TG_L_OPCOES;
    return n;
}
static void tgRotuloLista(int item, const TgConversa* c, char* s, int t) {
    const char* titulo = c ? c->titulo : "";
    if (item >= TG_L_MARCAR) {
        const TgConversa* k = &tgConversas[tgIndice[item - TG_L_MARCAR]];
        snprintf(s, t, "%s  %s", tgNaNovaPasta(k->id) ? "[x]" : "[  ]", k->titulo);
        return;
    }
    if (item >= TG_L_PASTA && item < TG_L_NOVA_PASTA) { snprintf(s, t, "%s %s", tgIcone("📁", ">"), tgPastas[item].nome); return; }
    switch (item) {
    case TG_L_NOVA_PASTA: snprintf(s, t, "%s", tgIcone("📁 Criar pasta", "Criar pasta")); break;
    case TG_L_ADD_PASTA: snprintf(s, t, "%s \"%.40s\" a uma pasta  ›", tgIcone("➕ Adicionar", "Adicionar"), titulo); break;
    case TG_L_OPCOES: snprintf(s, t, "%s \"%.40s\"  ›", tgIcone("⚙️ Opções de", "Opções de"), titulo); break;
    case TG_L_APAGAR_PASTA: if (tgConfirmar) snprintf(s, t, "Aperte X de novo para apagar a pasta %s", tgNomeDaAba(tgAba)); else snprintf(s, t, "%s %s", tgIcone("🗑 Apagar a pasta", "Apagar a pasta"), tgNomeDaAba(tgAba)); break;
    case TG_L_GRUPO: snprintf(s, t, "%s", tgIcone("👥 Criar grupo", "Criar grupo")); break;
    case TG_L_CANAL: snprintf(s, t, "%s", tgIcone("📢 Criar canal", "Criar canal")); break;
    case TG_L_MUDO: snprintf(s, t, "%s", c && c->mudo ? tgIcone("🔔 Ativar notificações", "Ativar notificações") : tgIcone("🔕 Silenciar", "Silenciar")); break;
    case TG_L_ARQUIVAR: snprintf(s, t, "%s", c && c->ordens[1] ? tgIcone("📤 Desarquivar", "Desarquivar") : tgIcone("🗄 Arquivar", "Arquivar")); break;
    case TG_L_SAIR: if (tgConfirmar) snprintf(s, t, "Aperte X de novo para sair de \"%.40s\"", titulo); else snprintf(s, t, "%s \"%.40s\"", tgIcone("🚪 Sair de", "Sair de"), titulo); break;
    case TG_L_CRIAR_OK: snprintf(s, t, "%s \"%s\" com %d conversa(s)", tgIcone("✅ Criar a pasta", "Criar a pasta"), tgNovaPastaNome, tgNovaPastaTotal); break;
    }
}
static void tgDesenharListaMenu(uint32_t* p) {
    static int itens[TG_L_MAX_ITENS];
    int total = tgListaItens(itens);
    const TgConversa* c = tgConversaMarcadaNaLista();
    static char rotulos[TG_L_MAX_ITENS][160];
    for (int i = 0; i < total; i++) tgRotuloLista(itens[i], c, rotulos[i], sizeof(rotulos[i]));
    if (tgListaMenuSel >= total) tgListaMenuSel = total > 0 ? total - 1 : 0;
    char titulo[160];
    if (tgListaModo == 1) snprintf(titulo, sizeof(titulo), "Adicionar \"%.40s\" à pasta", c ? c->titulo : "");
    else if (tgListaModo == 2) snprintf(titulo, sizeof(titulo), "Opções de \"%.40s\"", c ? c->titulo : "");
    else if (tgListaModo == 3) snprintf(titulo, sizeof(titulo), "Pasta \"%s\": X marca as conversas", tgNovaPastaNome);
    else snprintf(titulo, sizeof(titulo), "Conversas e pastas");
    tgDesenharMenuGenerico(p, titulo, rotulos, total, tgListaMenuSel);
}


static void tgDesenharSelMenu(uint32_t* p) {
    char apagar[64];
    if (tgConfirmarApagar) snprintf(apagar, sizeof(apagar), "Confirmar: apagar %d para todos", tgTotalMarcadas);
    else snprintf(apagar, sizeof(apagar), "%s", tgIcone("🗑️ Apagar", "Apagar"));
    const char* nomes[3] = { tgIcone("➡️ Encaminhar", "Encaminhar"), apagar, tgIcone("📋 Copiar texto", "Copiar texto") };
    int largura = 760, altura = 70 + 3 * 58 + 60;
    int x = TG_LISTA_L + 2 + (1920 - TG_LISTA_L - largura) / 2, y = (1080 - altura) / 2;
    char titulo[48];
    snprintf(titulo, sizeof(titulo), "%d selecionada(s)", tgTotalMarcadas);
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(30, 44, 58), 0, 1080);
    tgEscrever(p, titulo, 0, 1 << 30, 28, x + 28, y + 18, TG_COR(255, 255, 255));
    for (int i = 0; i < 3; i++) {
        int linha = y + 70 + i * 58;
        if (i == tgSelMenuSel) tgArredondado(p, x + 12, linha, largura - 24, 54, 12, i == 1 && tgConfirmarApagar ? TG_COR(160, 60, 55) : TG_COR(46, 112, 165), 0, 1080);
        tgEscrever(p, nomes[i], 0, 1 << 30, 24, x + 30, linha + 12, TG_COR(245, 245, 245));
    }
    tgEscrever(p, "X  Escolher     O  Voltar", 0, 1 << 30, 22, x + 28, y + altura - 42, TG_COR(150, 165, 180));
}

static void tgDesenharSeletor(uint32_t* p) {
    const int linhas = 11;
    int largura = 1100, altura = 80 + linhas * 58 + 60;
    int x = (1920 - largura) / 2, y = (1080 - altura) / 2;
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(30, 44, 58), 0, 1080);
    tgEscreverCortado(p, tgSeletorPasta, 26, x + 28, y + 20, largura - 56, TG_COR(255, 255, 255));
    if (tgSeletorTotal == 0) tgEscrever(p, "Pasta vazia", 0, 1 << 30, 24, x + 28, y + 90, TG_COR(150, 165, 180));
    int topo = tgSeletorSel >= linhas ? tgSeletorSel - linhas + 1 : 0;
    for (int i = 0; i < linhas && topo + i < tgSeletorTotal; i++) {
        const TgEntrada* e = &tgSeletorItens[topo + i];
        int yl = y + 76 + i * 58;
        if (topo + i == tgSeletorSel) tgArredondado(p, x + 12, yl, largura - 24, 54, 12, TG_COR(46, 112, 165), 0, 1080);
        char linha[140];
        snprintf(linha, sizeof(linha), "%s%s", e->pasta ? tgIcone("📁 ", "[pasta] ") : tgIcone("📄 ", ""), e->nome);
        tgEscreverCortado(p, linha, 24, x + 30, yl + 12, largura - 60, TG_COR(245, 245, 245));
    }
    tgEscrever(p, "X  Abrir pasta / Enviar arquivo     O  Voltar     Triângulo  Fechar", 0, 1 << 30, 22, x + 28, y + altura - 42, TG_COR(150, 165, 180));
}

static void tgDesenharGravacao(uint32_t* p) {
    char texto[160];
    int s = tgGravacaoSegundos;
    snprintf(texto, sizeof(texto), "%sGravando áudio  %d:%02d", tgIcone("🎤 ", ""), s / 60, s % 60);
    const char* dica = "X  Enviar     O  Cancelar";
    int largura = 760, altura = 150;
    int x = TG_LISTA_L + 2 + (1920 - TG_LISTA_L - largura) / 2, y = (1080 - altura) / 2;
    tgArredondado(p, x, y, largura, altura, 18, TG_COR(120, 40, 40), 0, 1080);
    tgEscrever(p, texto, 0, 1 << 30, 30, x + (largura - tgLargura(texto, 30)) / 2, y + 26, TG_COR(255, 255, 255));
    tgEscrever(p, dica, 0, 1 << 30, 22, x + (largura - tgLargura(dica, 22)) / 2, y + 96, TG_COR(240, 220, 220));
}

static void tgDesenharEncaminhar(uint32_t* p) {
    const char* texto = "Encaminhar: escolha a conversa na lista e aperte X     O  Cancelar";
    int w = tgLargura(texto, 22) + 36;
    tgArredondado(p, (1920 + TG_LISTA_L - w) / 2, 160, w, 42, 18, TG_COR(43, 82, 120), 0, 1080);
    tgEscrever(p, texto, 0, 1 << 30, 22, (1920 + TG_LISTA_L - w) / 2 + 18, 169, TG_COR(255, 255, 255));
}

// Entre um vídeo e outro a conversa não aparece: fica como o player esperando o próximo.
static bool tgTrocandoVideo = false;
static int tgTrocaEspera = 0;

static void tgDesenhar(uint32_t* p) {
    if (tgTrocandoVideo) {
        const char* texto = "Carregando próximo vídeo...   O  Cancelar";
        tgRetangulo(p, 0, 0, 1920, 1080, TG_COR(0, 0, 0));
        tgEscrever(p, texto, 0, 1 << 30, 30, (1920 - tgLargura(texto, 30)) / 2, 520, TG_COR(255, 255, 255));
        return;
    }
    if (tgTela != TG_CONVERSAS && tgTela != TG_MENSAGENS) { tgDesenharEntrada(p); return; }
    if (tgVisor) { tgDesenharVisor(p); return; }
    tgDesenharConversa(p);
    tgDesenharLista(p);
    if (tgMenuAberto) tgDesenharMenu(p);
    if (tgFilaAberta) tgDesenharFila(p);
    if (tgAcoesAberto) tgDesenharAcoes(p);
    if (tgListaMenuAberto) tgDesenharListaMenu(p);
    if (tgSelMenuAberto) tgDesenharSelMenu(p);
    if (tgSeletorAberto) tgDesenharSeletor(p);
    if (tgEncaminharMsg) tgDesenharEncaminhar(p);
    if (tgGravando) tgDesenharGravacao(p);
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

// Mensagens apagadas (aqui ou em outro aparelho) saem da tela.
static void tgRemoverMensagens(const long long* ids, int n) {
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < tgTotalMensagens; i++) {
            if (tgMensagens[i].id != ids[k]) continue;
            if (tgSelId == ids[k]) tgSelId = i + 1 < tgTotalMensagens ? tgMensagens[i + 1].id : (i > 0 ? tgMensagens[i - 1].id : 0);
            memmove(&tgMensagens[i], &tgMensagens[i + 1], sizeof(TgMensagem) * (tgTotalMensagens - i - 1));
            tgTotalMensagens--;
            break;
        }
    }
    tgSujo = true;
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
    if (tgFiltro >= 2) {
        tgEnviar("{\"@type\":\"searchChatMessages\",\"chat_id\":%lld,\"query\":\"\",\"from_message_id\":%lld,\"offset\":0,\"limit\":50,\"filter\":{\"@type\":\"%s\"},\"@extra\":\"historico\"}", tgAberta, aPartirDe, tgFiltroTdlib[tgFiltro]);
        return;
    }
    // Só mensagens: o filtro é local, então a página seguinte parte da mais antiga lida (não da mostrada).
    if (tgFiltro == 1 && tgMaisAntiga && (aPartirDe == 0 || aPartirDe > tgMaisAntiga) && tgTotalMensagens > 0) aPartirDe = tgMaisAntiga;
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
    else if (strstr(r, "authorizationStateReady")) { extern void conquistaGanhar(const char* chave); conquistaGanhar("tg_login"); tgTela = TG_CONVERSAS; tgCarregarConversas(); }
    else if (strstr(r, "authorizationStateLoggingOut") || strstr(r, "authorizationStateClosing") || strstr(r, "authorizationStateClosed")) {
        tgTela = TG_INICIANDO;
        strcpy(tgAviso, "Sessão encerrada. Reabra o app.");
    }
    else tgTela = TG_SEM_SUPORTE;
}

static bool tgPassaFiltro(const TgMensagem* m) {
    if (tgFiltro == 1) return !m->midia && !m->grande && !m->figurinha && m->texto[0];
    return true;
}

static void tgProcessarMensagens(const char* r, bool novas) {
    const char* chave = "{\"@type\":\"message\",\"id\":";
    int lidas = 0, lidasTodas = 0;
    long long maisNovaAntes = tgTotalMensagens > 0 ? tgMensagens[tgTotalMensagens - 1].id : 0;
    if (novas) tgPedindoNovas = false;
    else tgPedindoHistorico = false;
    for (const char* p = strstr(r, chave); p; ) {
        const char* proxima = strstr(p + 1, chave);
        TgMensagem m;
        long long conversa;
        if (tgLerMensagem(p, proxima, &m, &conversa) && conversa == tgAberta) {
            lidasTodas++;
            if (!tgMaisAntiga || m.id < tgMaisAntiga) tgMaisAntiga = m.id;
            if (tgPassaFiltro(&m)) { tgGuardarMensagem(&m); lidas++; }
        }
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
    if (lidasTodas == 0) tgFimDoHistorico = true;
    if (tgAlvo) {
        for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgAlvo) tgSelId = tgAlvo;
        tgAlvo = 0;
        tgFaltamNovas = true;   // o salto carrega só a vizinhança da mensagem
        return;
    }
    // A primeira resposta costuma trazer poucas mensagens, então pede mais até encher a tela.
    if (lidasTodas > 0 && tgTotalMensagens < 40 && tgPedidosHistorico < (tgFiltro == 1 ? 12 : 6)) tgPedirHistorico(tgMensagens[0].id);
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

// A fila baixa um arquivo por vez, na ordem em que entrou, e fica guardada para a próxima abertura.
#define TG_FILA_ARQUIVO TELEGRAM_PASTA "/downloads.txt"
static int tgFilaProvisorio = 0;

static void tgCarregarFila() {
    FILE* f = fopen(TG_FILA_ARQUIVO, "r");
    if (!f) return;
    char linha[200];
    while (fgets(linha, sizeof(linha), f) && tgTotalDownloads < TG_MAX_DOWNLOADS) {
        long long conversa = 0, mensagem = 0;
        int pkg = 0;
        char nome[120] = "";
        if (sscanf(linha, "%lld %lld %d %119[^\n]", &conversa, &mensagem, &pkg, nome) != 4 || !conversa || !mensagem) continue;
        TgDownload* d = &tgDownloads[tgTotalDownloads++];
        memset(d, 0, sizeof(*d));
        d->arquivo = -(++tgFilaProvisorio);   // o número do arquivo muda a cada abertura: pede a mensagem de novo
        d->estado = TG_D_BAIXANDO;
        d->salvar = true;
        d->pkg = pkg != 0;
        d->conversa = conversa;
        d->mensagem = mensagem;
        snprintf(d->nome, sizeof(d->nome), "%s", nome);
    }
    fclose(f);
}

static void tgSalvarFila() {
    static unsigned long long ultimo = ~0ULL;
    unsigned long long h = 1469598103934665603ULL;
    for (int i = 0; i < tgTotalDownloads; i++) {
        const TgDownload* d = &tgDownloads[i];
        if (!d->salvar || d->estado >= TG_D_PRONTO || !d->conversa) continue;
        h = (h ^ (unsigned long long)d->mensagem) * 1099511628211ULL;
        h = (h ^ (unsigned long long)d->conversa) * 1099511628211ULL;
    }
    if (h == ultimo) return;
    ultimo = h;
    FILE* f = fopen(TG_FILA_ARQUIVO, "w");
    if (!f) return;
    for (int i = 0; i < tgTotalDownloads; i++) {
        const TgDownload* d = &tgDownloads[i];
        if (!d->salvar || d->estado >= TG_D_PRONTO || !d->conversa) continue;
        char nome[120];
        snprintf(nome, sizeof(nome), "%s", d->nome);
        for (char* p = nome; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
        fprintf(f, "%lld %lld %d %s\n", d->conversa, d->mensagem, d->pkg ? 1 : 0, nome);
    }
    fclose(f);
}

// Chamada com a trava pega, pela thread do TDLib, a cada resposta ou segundo.
static void tgAndarFila() {
    static bool carregou = false;
    if (!carregou) { carregou = true; tgCarregarFila(); }
    bool pronto = tgTela == TG_CONVERSAS || tgTela == TG_MENSAGENS;
    uint64_t agora = sceKernelGetProcessTime();
    for (int i = 0; i < tgTotalDownloads; i++) {
        TgDownload* d = &tgDownloads[i];
        if (d->arquivo > 0 || d->estado != TG_D_BAIXANDO || !pronto || (uint64_t)d->proxima > agora) continue;
        if (d->tentativas >= 6) { d->estado = TG_D_ERRO; tgSujo = true; continue; }
        d->tentativas++;
        d->proxima = (long long)(agora + 10000000ULL);
        tgEnviar("{\"@type\":\"getMessage\",\"chat_id\":%lld,\"message_id\":%lld,\"@extra\":\"fila:%lld\"}", d->conversa, d->mensagem, d->mensagem);
    }
    // Só um download da fila por vez; o próximo é sempre o mais antigo que ainda não começou.
    bool ativo = false;
    for (int i = 0; i < tgTotalDownloads; i++) if (tgDownloads[i].estado == TG_D_BAIXANDO && tgDownloads[i].iniciado) ativo = true;
    for (int i = 0; i < tgTotalDownloads && !ativo; i++) {
        TgDownload* d = &tgDownloads[i];
        if (d->estado != TG_D_BAIXANDO || d->iniciado) continue;
        if (d->arquivo <= 0) break;   // espera a mensagem dele voltar, para não furar a ordem
        d->iniciado = true;
        // Prioridade mínima: um arquivo grande da fila não pode segurar as fotos da conversa.
        tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":1,\"offset\":0,\"limit\":0,\"synchronous\":false}", d->arquivo);
        tgSujo = true;
        break;
    }
    tgSalvarFila();
}

// Resposta do getMessage de um item guardado: troca o número provisório pelo arquivo de verdade.
static void tgFilaRestaurada(const char* r, long long mensagem) {
    int indice = -1;
    for (int i = 0; i < tgTotalDownloads; i++) if (tgDownloads[i].arquivo <= 0 && tgDownloads[i].mensagem == mensagem) indice = i;
    if (indice < 0) return;
    TgDownload* d = &tgDownloads[indice];
    static TgMensagem m;
    long long conversa = 0;
    if (!tgLerMensagem(r, r + strlen(r), &m, &conversa) || !m.grande) { d->estado = TG_D_ERRO; tgSujo = true; return; }
    if (tgDownload(m.grande)) { tgRemoverDownload(indice); return; }
    TgArquivo* a = tgArquivo(m.grande);
    if (!a) { d->estado = TG_D_ERRO; tgSujo = true; return; }
    d->arquivo = a->id;
    d->iniciado = false;
    a->pedido = true;
    if (a->baixado) d->estado = TG_D_COPIAR;
    tgSujo = true;
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
        // O player só abre com folga de dados: com 512 KB, um vídeo 1080p começava sem nada
        // para decodificar e o app fechava (9 de 49 vídeos 1080p no log; nenhum 720p).
        long long folga = tgFluxo.total > 0 && tgFluxo.total < 16LL * 1024 * 1024 ? tgFluxo.total / 2 : 8LL * 1024 * 1024;
        // Áudio toca com bem menos: uns 384 KB já são vários segundos de MP3.
        bool audio = tgPendenteTipo == TG_M_AUDIO;
        if (audio) folga = tgFluxo.total > 0 && tgFluxo.total < 512LL * 1024 ? tgFluxo.total : 384LL * 1024;
        if (!tgFluxo.ativo && !completo && id == tgPendente && tgFluxo.caminho[0] && tgFluxo.inicio == 0) {
            if (tgFluxo.prefixo >= folga) tgAbrirAgora = true;
            else snprintf(tgAviso, sizeof(tgAviso), audio ? "Carregando áudio... %lld%%" : "Carregando vídeo... %lld%%", tgFluxo.prefixo * 100 / (folga > 0 ? folga : 1));
        }
    }
    TgDownload* d = tgDownload((int)id);
    if (d && d->estado == TG_D_BAIXANDO) {
        tgNumero(r, NULL, "\"size\":", &d->total);
        if (d->total <= 0) tgNumero(r, NULL, "\"expected_size\":", &d->total);
        tgNumero(r, NULL, "\"downloaded_size\":", &d->baixado);
        // Velocidade suavizada, medida a cada segundo.
        uint64_t agora = tgAgoraMs();
        if (!d->inicioMs) { d->inicioMs = agora; d->marcaMs = agora; d->marcaBytes = d->baixado; }
        else if (agora - d->marcaMs >= 1000) {
            double v = (double)(d->baixado - d->marcaBytes) * 1000.0 / (double)(agora - d->marcaMs);
            d->velocidade = d->velocidade > 0 ? d->velocidade * 0.6 + v * 0.4 : v;
            d->marcaMs = agora; d->marcaBytes = d->baixado;
        }
        tgSujo = true;
    }
    if (id == tgPendente && !completo && d && id != tgFluxo.arquivo) {
        char info[96]; tgInfoDownload(d, info, sizeof(info));
        snprintf(tgAviso, sizeof(tgAviso), "Baixando %s   Options: downloads", info);
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
        snprintf(tgConviteLink, sizeof(tgConviteLink), "%s", valor);
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
        else if (tgConviteLink[0]) {
            // Ainda não é membro: entra pelo convite e abre a conversa quando o TDLib devolver o chat.
            tgEnviar("{\"@type\":\"joinChatByInviteLink\",\"invite_link\":\"%s\",\"@extra\":\"link\"}", tgConviteLink);
            strcpy(tgAviso, "Entrando no grupo...");
        }
        else strcpy(tgAviso, "Convite de grupo inválido.");
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

static void tgMenuLinksCompletos(const char* r, long long id);
static void tgProcessar(const char* r) {
    long long id = 0;
    if (tgTipo(r, "updateFile") || tgTipo(r, "file")) { tgProcessarArquivo(r); return; }
    const char* menu = strstr(r, "\"@extra\":\"menulinks:");
    if (menu) { tgMenuLinksCompletos(r, atoll(menu + 20)); return; }
    const char* fila = strstr(r, "\"@extra\":\"fila:");
    if (fila) {
        if (tgTipo(r, "message")) tgFilaRestaurada(r, atoll(fila + 15));
        return;
    }
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
        long long mudo = 0;
        if (tgNumero(r, NULL, "\"mute_for\":", &mudo)) c->mudo = mudo > 0;
        c->tipo = strstr(r, "\"is_channel\":true") ? 2 : (strstr(r, "chatTypeSupergroup") || strstr(r, "chatTypeBasicGroup")) ? 1 : 0;
        long long g = 0;
        if (tgNumero(r, NULL, "\"supergroup_id\":", &g)) c->grupoId = g;
        else if (tgNumero(r, NULL, "\"basic_group_id\":", &g)) c->grupoId = -g;
    }
    else if (tgTipo(r, "updateDeleteMessages")) {
        long long chat = 0;
        tgNumero(r, NULL, "\"chat_id\":", &chat);
        if (chat == tgAberta && strstr(r, "\"is_permanent\":true")) {
            static long long ids[300];
            int n = tgLerIds(r, "\"message_ids\":[", ids, 300);
            tgRemoverMensagens(ids, n);
        }
    }
    else if (tgTipo(r, "updateMessageContent")) {
        // Mensagem editada: relê só o conteúdo e guarda o resto (autor, data...).
        long long chat = 0, msg = 0;
        tgNumero(r, NULL, "\"chat_id\":", &chat);
        tgNumero(r, NULL, "\"message_id\":", &msg);
        const char* chave = "\"new_content\":{\"@type\":\"";
        const char* c = strstr(r, chave);
        if (chat == tgAberta && c) {
            for (int i = 0; i < tgTotalMensagens; i++) {
                if (tgMensagens[i].id != msg) continue;
                TgMensagem n = tgMensagens[i];
                n.texto[0] = 0; n.rotulo[0] = 0; n.resumo[0] = 0; n.totalLinks = 0;
                n.alturaCache[0] = n.alturaCache[1] = 0;
                tgLerConteudo(c + strlen(chave), r + strlen(r), &n);
                tgLerLinks(c, r + strlen(r), &n);
                tgMensagens[i] = n;
                tgSujo = true;
                break;
            }
        }
    }
    else if (tgTipo(r, "updateSupergroup") || tgTipo(r, "updateBasicGroup")) {
        long long g = 0;
        bool super = tgTipo(r, "updateSupergroup");
        if (super ? tgNumero(r, NULL, "\"supergroup\":{\"@type\":\"supergroup\",\"id\":", &g) : tgNumero(r, NULL, "\"basic_group\":{\"@type\":\"basicGroup\",\"id\":", &g)) {
            bool fora = strstr(r, "chatMemberStatusLeft") || strstr(r, "chatMemberStatusBanned") || (strstr(r, "chatMemberStatusRestricted") && strstr(r, "\"is_member\":false"));
            tgGuardarStatusGrupo(super ? g : -g, !fora);
            tgSujo = true;
        }
    }
    else if (tgTipo(r, "updateChatNotificationSettings")) {
        if (!tgNumero(r, NULL, "\"chat_id\":", &id)) return;
        TgConversa* c = tgConversa(id);
        long long mudo = 0;
        if (c && tgNumero(r, NULL, "\"mute_for\":", &mudo)) c->mudo = mudo > 0;
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
    else if (tgTipo(r, "foundChatMessages") && strstr(r, "\"@extra\":\"historico\"")) tgProcessarMensagens(r, false);
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

static void tgIniciarCopia();
static void* threadTelegram(void*) {
    signal(SIGPIPE, SIG_IGN);
    // Acima do nível 1 o TDLib grava telefone e mensagens no arquivo.
    td_execute("{\"@type\":\"setLogStream\",\"log_stream\":{\"@type\":\"logStreamFile\",\"path\":\"" TELEGRAM_PASTA "/td.log\",\"max_file_size\":10485760}}");
    td_execute("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":1}");
    tgCliente = td_create_client_id();
    td_send(tgCliente, "{\"@type\":\"getOption\",\"name\":\"version\"}");
    for (;;) {
        const char* r = td_receive(1.0);
        pthread_mutex_lock(&tgTrava);
        if (r) tgProcessar(r);
        tgAndarFila();
        tgIniciarCopia();
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
    int faixa = -1;
    extern bool appSomEmpurrar(const int16_t* estereo, int quadros, volatile bool* parar);
    extern void appSomEsperarFim(volatile bool* parar), appSomLimpar();
    extern AVIOContext* telegramAbrirFluxo(const char* caminho);
    extern void telegramFecharFluxo();
    // Ainda baixando: lê pelo fluxo, que espera o trecho que falta chegar.
    AVIOContext* fluxo = telegramAbrirFluxo(caminho);
    tgAudioFluxo = fluxo != NULL;
    if (fluxo) {
        formato = avformat_alloc_context();
        if (formato) { formato->pb = fluxo; formato->flags |= AVFMT_FLAG_CUSTOM_IO; }
    }
    (void)global_uId;

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
    extern void pausarMusicaParaVideo(), retomarMusicaAposVideo();
    if (conversor) {
        // O som vai para a mistura do app (network.cpp), sem porta própria.
        pausarMusicaParaVideo();
        while (!tgAudioParar && av_read_frame(formato, pacote) >= 0) {
            if (pacote->stream_index == faixa && avcodec_send_packet(contexto, pacote) >= 0) {
                while (!tgAudioParar && avcodec_receive_frame(contexto, quadro) >= 0) {
                    uint8_t* saida[1] = { (uint8_t*)fila };
                    int feitos = swr_convert(conversor, saida, 16384, (const uint8_t**)quadro->extended_data, quadro->nb_samples);
                    if (feitos > 0) appSomEmpurrar(fila, feitos, &tgAudioParar);
                }
            }
            av_packet_unref(pacote);
        }
        if (!tgAudioParar) appSomEsperarFim(&tgAudioParar);
        else appSomLimpar();
        retomarMusicaAposVideo();
    }
    else {
        pthread_mutex_lock(&tgTrava);
        strcpy(tgAviso, "Não foi possível tocar este áudio.");
        pthread_mutex_unlock(&tgTrava);
    }
    if (conversor) swr_free(&conversor);
    if (contexto) avcodec_free_context(&contexto);
    if (formato) avformat_close_input(&formato);
    else if (fluxo) avformat_free_context(formato);
    if (tgAudioFluxo) { tgAudioFluxo = false; telegramFecharFluxo(); }
    av_frame_free(&quadro);
    av_packet_free(&pacote);
    tgAudioMensagem = 0;
    tgAudioRodando = false;
    tgSujo = true;
    return NULL;
}

static void tgPararAudio() {
    tgAudioParar = true;
    if (tgAudioFluxo) tgFluxo.parar = true;   // a leitura pode estar esperando o download
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

static long long tgPendenteMensagem = 0, tgVideoMensagem = 0;
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
    // Sem limite de espera: rede lenta não pode virar fim de vídeo. Só para quando o vídeo
    // fecha (parar) ou quando o arquivo inteiro já foi lido.
    for (;;) {
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
            if (lidos > 0) {
                tgFluxo.pos += lidos;
                return lidos;
            }
            clearerr(tgFluxo.f);   // o TDLib avisou antes de o trecho chegar ao disco: tenta de novo
        }
        sceKernelUsleep(20000);
    }
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
    // Vídeo vindo do celular/PC ou do DLNA (network.cpp) usa o mesmo caminho do player.
    extern AVIOContext* remotoAbrirFluxo(const char* caminho);
    AVIOContext* remoto = remotoAbrirFluxo(caminho);
    if (remoto) return remoto;
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

void telegramPararFluxo() { extern void remotoPararFluxo(); remotoPararFluxo(); if (tgFluxo.ativo) tgFluxo.parar = true; }

void telegramFecharFluxo() {
    extern void remotoFecharFluxo();
    remotoFecharFluxo();
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
        extern bool video_repetir, video_telegram, video_repetir_telegram;
        tgPararAudio();
        iniciarVideoMP4(tgPendenteCaminho);
        tgTrocandoVideo = false;
        pthread_mutex_lock(&tgTrava);
        tgVideoMensagem = mensagem;
        video_telegram = true;
        // GIF sempre repete; vídeo segue a escolha feita no menu do player.
        video_repetir = video_repetir_telegram;
        for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == mensagem && !strcmp(tgMensagens[i].rotulo, "GIF")) video_repetir = true;
        // A thread do vídeo não usa a fonte: a mensagem vai pronta, na ordem de cor da tela.
        extern uint32_t* video_legenda_pixels;
        extern volatile int video_legenda_altura;
        static uint32_t legenda[1920 * 300];
        int altura = 0;
        for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == mensagem) altura = tgMontarLegenda(&tgMensagens[i], legenda, 300);
        for (int i = 0; i < 1920 * altura; i++) {
            uint32_t c = legenda[i];
            legenda[i] = (c & 0xFF00FF00u) | ((c & 255) << 16) | ((c >> 16) & 255);
        }
        video_legenda_pixels = legenda;
        video_legenda_altura = altura;
        pthread_mutex_unlock(&tgTrava);
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
    // Áudio toca enquanto baixa (mesmo caminho do vídeo); para guardar: segure X > Baixar.
    if (m->midia == TG_M_AUDIO && !tgFluxo.ativo && !tgFluxo.arquivo) {
        tgFluxo.arquivo = a->id;
        tgFluxo.caminho[0] = '\0';
        tgFluxo.prefixo = 0; tgFluxo.inicio = 0; tgFluxo.total = 0;
        tgFluxo.completo = false;
        strcpy(tgAviso, "Carregando áudio...");
        tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":32,\"offset\":0,\"limit\":0,\"synchronous\":false}", a->id);
        return;
    }
    TgDownload* audio = tgNovoDownload(a->id, m->arquivoNome[0] ? m->arquivoNome : "Áudio", m->id);
    if (audio) audio->iniciado = true;   // áudio para ouvir agora não espera a fila
    strcpy(tgAviso, "Baixando 0%   Options: downloads");
    tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":32,\"offset\":0,\"limit\":0,\"synchronous\":false}", a->id);
}

static const TgMensagem* tgAcharVideo(int direcao) {
    int atual = -1;
    for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == tgVideoMensagem) atual = i;
    for (int i = atual + direcao; atual >= 0 && i >= 0 && i < tgTotalMensagens; i += direcao) {
        if (tgMensagens[i].midia == TG_M_VIDEO && strcmp(tgMensagens[i].rotulo, "GIF") != 0) return &tgMensagens[i];
    }
    return NULL;
}

static long long tgPreparadoMensagem = 0;
static int tgPreparadoArquivo = 0;

static void tgCancelarPreparado() {
    if (!tgPreparadoArquivo) return;
    if (!tgDownload(tgPreparadoArquivo)) {
        tgEnviar("{\"@type\":\"cancelDownloadFile\",\"file_id\":%d,\"only_if_pending\":false}", tgPreparadoArquivo);
        tgEnviar("{\"@type\":\"deleteFile\",\"file_id\":%d}", tgPreparadoArquivo);
    }
    tgPreparadoArquivo = 0;
}

// No último minuto baixa o começo do próximo vídeo, para ele abrir na hora da troca.
void telegramPrepararProximo() {
    extern bool video_repetir;
    extern double video_time_current, video_time_total;
    if (video_repetir || video_time_total <= 0 || video_time_total - video_time_current > 60.0) return;
    pthread_mutex_lock(&tgTrava);
    if (tgVideoMensagem && tgPreparadoMensagem != tgVideoMensagem) {
        tgPreparadoMensagem = tgVideoMensagem;
        const TgMensagem* m = tgAcharVideo(1);
        TgArquivo* a = m ? tgArquivo(m->grande) : NULL;
        if (a && !a->baixado && a->id != tgFluxo.arquivo) {
            tgPreparadoArquivo = a->id;
            tgEnviar("{\"@type\":\"downloadFile\",\"file_id\":%d,\"priority\":8,\"offset\":0,\"limit\":4194304,\"synchronous\":false}", a->id);
        }
    }
    pthread_mutex_unlock(&tgTrava);
}

// O vídeo acabou ou o usuário pediu outro: abre o próximo vídeo da conversa, sem GIFs.
// Devolve true quando há outro vídeo a caminho.
bool telegramTrocarVideo(int direcao, bool telaCheia) {
    pthread_mutex_lock(&tgTrava);
    const TgMensagem* m = tgAcharVideo(direcao);
    bool abrindo = false;
    if (m) {
        if (tgPreparadoArquivo == m->grande) tgPreparadoArquivo = 0;
        tgCancelarPreparado();
        tgSelId = m->id;
        tgAbrirMidia(m);
        abrindo = tgPendente != 0 || tgAbrirAgora;
    }
    else strcpy(tgAviso, direcao > 0 ? "Não há próximo vídeo nesta conversa." : "Não há vídeo anterior nesta conversa.");
    tgTrocandoVideo = abrindo && telaCheia;
    tgTrocaEspera = 0;
    pthread_mutex_unlock(&tgTrava);
    tgSujo = true;
    return abrindo;
}

static void telegramFecharFluxoTravado();

static void tgCancelarTroca() {
    extern void retomarMusicaAposVideo();
    pthread_mutex_lock(&tgTrava);
    if (tgPendente && tgPendente == tgFluxo.arquivo && !tgFluxo.ativo) telegramFecharFluxoTravado();
    tgPendente = 0;
    tgAbrirAgora = false;
    pthread_mutex_unlock(&tgTrava);
    tgTrocandoVideo = false;
    retomarMusicaAposVideo();
    tgSujo = true;
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
    d->conversa = tgAberta;
    a->pedido = true;
    strcpy(tgAviso, d->pkg ? "PKG na fila: instala ao terminar" : "Adicionado aos downloads");
    if (a->baixado) { d->estado = TG_D_COPIAR; return; }
    d->estado = TG_D_BAIXANDO;
    d->iniciado = false;
    tgAndarFila();
}

static void tgAbrirLink(const char* url) {
    if (strpbrk(url, "\"\\")) { strcpy(tgAviso, "Link inválido."); return; }
    snprintf(tgLinkAtual, sizeof(tgLinkAtual), "%s", url);
    strcpy(tgAviso, "Abrindo link...");
    tgEnviar("{\"@type\":\"getInternalLinkType\",\"link\":\"%s\",\"@extra\":\"link\"}", url);
}

static void tgMenuItem(int valor) {
    if (tgMenuTotal >= tgMenuCap) {
        int nova = tgMenuCap ? tgMenuCap * 2 : 32;
        int* lista = (int*)realloc(tgMenuLinks, sizeof(int) * (size_t)nova);
        if (!lista) return;
        tgMenuLinks = lista;
        tgMenuCap = nova;
    }
    tgMenuLinks[tgMenuTotal++] = valor;
}

static void tgMenuGuardarUrl(const char* url) {
    for (int i = 0; i < tgMenuTotalUrls; i++) if (strcmp(tgMenuUrls[i], url) == 0) return;
    if (tgMenuTotalUrls >= tgMenuCapUrls) {
        int nova = tgMenuCapUrls ? tgMenuCapUrls * 2 : 32;
        char (*lista)[200] = (char (*)[200])realloc(tgMenuUrls, sizeof(tgMenuUrls[0]) * (size_t)nova);
        if (!lista) return;
        tgMenuUrls = lista;
        tgMenuCapUrls = nova;
    }
    snprintf(tgMenuUrls[tgMenuTotalUrls++], sizeof(tgMenuUrls[0]), "%s", url);
}

// Monta os itens com os links que já estão em tgMenuUrls.
static void tgMontarMenu(const TgMensagem* m) {
    tgMenuTotal = 0;
    tgMenuItem(-2);
    tgMenuItem(-3);
    tgMenuItem(-4);
    if (m->texto[0]) tgMenuItem(-5);
    if (m->minha && m->texto[0] && !m->midia && !m->grande && !m->figurinha) tgMenuItem(-6);
    for (int i = 0; i < tgMenuTotalUrls; i++) tgMenuItem(i);
    if (m->grande) tgMenuItem(-1);
    if (tgMenuSel >= tgMenuTotal) tgMenuSel = tgMenuTotal - 1;
}

static void tgAbrirMenu(const TgMensagem* m) {
    tgMenuTotalUrls = 0;
    for (int i = 0; i < m->totalLinks; i++) tgMenuGuardarUrl(m->links[i]);
    tgMenuSel = 0;
    tgMontarMenu(m);
    tgMenuMensagem = m->id;
    tgMenuAberto = tgMenuTotal > 0;
    // A mensagem guardada tem texto e links cortados: pede a inteira para listar todos.
    tgEnviar("{\"@type\":\"getMessage\",\"chat_id\":%lld,\"message_id\":%lld,\"@extra\":\"menulinks:%lld\"}", tgAberta, m->id, m->id);
}

// Resposta do getMessage do menu: junta todos os links do texto inteiro, das palavras com link e dos botões.
static void tgMenuLinksCompletos(const char* r, long long id) {
    if (!tgMenuAberto || tgMenuMensagem != id || !tgTipo(r, "message")) return;
    const TgMensagem* m = NULL;
    for (int i = 0; i < tgTotalMensagens; i++) if (tgMensagens[i].id == id) m = &tgMensagens[i];
    if (!m) return;
    const char* fim = r + strlen(r);
    const char* chave = "\"content\":{\"@type\":\"";
    const char* conteudo = NULL;
    for (const char* q = r; (q = tgAcha(q, fim, chave)) != NULL; q++) conteudo = q;
    if (!conteudo) return;
    static char texto[65536];
    tgTexto(conteudo, fim, "\"@type\":\"formattedText\",\"text\":\"", texto, sizeof(texto));
    static TgMensagem descarte;
    int antes = tgMenuTotalUrls;
    tgColetando = true;
    tgLerLinksTexto(conteudo, fim, texto, &descarte);
    tgColetando = false;
    if (tgMenuTotalUrls != antes) tgMontarMenu(m);
    tgSujo = true;
}

static char tgColarNoTeclado[1800] = "";
static void tgAbrirTeclado(int32_t usuario, TgModoIme modo, const wchar_t* titulo, int limite) {
    static uint16_t titulo16[80];
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(tgTextoIme, 0, sizeof(tgTextoIme));
    if (modo == TG_IME_BUSCA) memcpy(tgTextoIme, tgUltimaBusca, sizeof(tgUltimaBusca));
    // O IME trabalha em UTF-16; o wchar_t deste build tem 4 bytes.
    int n = 0;
    while (titulo[n] && n < 79) { titulo16[n] = (uint16_t)titulo[n]; n++; }
    titulo16[n] = 0;
    if (tgColarNoTeclado[0]) {
        uint16_t* destino = (uint16_t*)tgTextoIme;
        int k = 0;
        for (const char* s = tgColarNoTeclado; *s && k < limite;) {
            uint32_t cp;
            s += tgLerCodigo(s, &cp);
            if (cp >= 0x10000) {
                if (k + 2 > limite) break;
                cp -= 0x10000;
                destino[k++] = (uint16_t)(0xD800 + (cp >> 10));
                destino[k++] = (uint16_t)(0xDC00 + (cp & 0x3FF));
            }
            else destino[k++] = (uint16_t)cp;
        }
        tgColarNoTeclado[0] = '\0';
    }
    param.userId = usuario;
    param.maxTextLength = limite;
    param.inputTextBuffer = tgTextoIme;
    param.title = (wchar_t*)titulo16;
    param.type = (OrbisImeType)0;
    int rc = sceImeDialogInit(&param, NULL);
    FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (registro) { fprintf(registro, "TG teclado modo=%d rc=%x\n", (int)modo, (unsigned)rc); fclose(registro); }
    if (rc >= 0) { tgImeAberto = true; tgModoIme = modo; }
    else snprintf(tgAviso, sizeof(tgAviso), "O teclado não abriu (%x).", (unsigned)rc);
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
        if (tgModoIme == TG_IME_MENSAGEM && tgEditarMsg) {
            tgEnviar("{\"@type\":\"editMessageText\",\"chat_id\":%lld,\"message_id\":%lld,\"input_message_content\":{\"@type\":\"inputMessageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\"}}}", tgAberta, tgEditarMsg, texto);
            strcpy(tgAviso, "Mensagem editada.");
        }
        else if (tgModoIme == TG_IME_MENSAGEM) {
            if (tgResponderA) tgEnviar("{\"@type\":\"sendMessage\",\"chat_id\":%lld,\"reply_to\":{\"@type\":\"inputMessageReplyToMessage\",\"message_id\":%lld},\"input_message_content\":{\"@type\":\"inputMessageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\"}}}", tgAberta, tgResponderA, texto);
            else tgEnviar("{\"@type\":\"sendMessage\",\"chat_id\":%lld,\"input_message_content\":{\"@type\":\"inputMessageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\"}}}", tgAberta, texto);
            tgSelId = 0;
        }
        else if (tgModoIme == TG_IME_BUSCA) {
            memcpy(tgUltimaBusca, tgTextoIme, sizeof(tgUltimaBusca));
            tgIniciarBusca(texto);
        }
        else if (tgModoIme == TG_IME_PASTA) {
            // Agora escolhe as conversas que entram nela (a marcada na lista já vem marcada).
            snprintf(tgNovaPastaNome, sizeof(tgNovaPastaNome), "%s", texto);
            tgNovaPastaTotal = 0;
            if (tgChatDaPasta) tgNovaPastaIds[tgNovaPastaTotal++] = tgChatDaPasta;
            tgListaModo = 3; tgListaMenuSel = 0; tgListaMenuAberto = true;
        }
        else if (false) {
            tgEnviar("{\"@type\":\"createChatFolder\",\"folder\":{\"@type\":\"chatFolder\",\"name\":{\"@type\":\"chatFolderName\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\",\"entities\":[]},\"animate_custom_emoji\":false},"
                "\"icon\":null,\"color_id\":-1,\"is_shareable\":false,\"pinned_chat_ids\":[],\"included_chat_ids\":[%lld],\"excluded_chat_ids\":[],\"exclude_muted\":false,\"exclude_read\":false,"
                "\"exclude_archived\":false,\"include_contacts\":false,\"include_non_contacts\":false,\"include_bots\":false,\"include_groups\":false,\"include_channels\":false}}", texto, tgChatDaPasta);
            strcpy(tgAviso, "Pasta criada.");
        }
        else if (tgModoIme == TG_IME_GRUPO || tgModoIme == TG_IME_CANAL) {
            // A resposta é o chat novo, que o tratamento de links abre.
            tgEnviar("{\"@type\":\"createNewSupergroupChat\",\"title\":\"%s\",\"is_forum\":false,\"is_channel\":%s,\"description\":\"\",\"location\":null,\"message_auto_delete_time\":0,\"for_import\":false,\"@extra\":\"link\"}",
                texto, tgModoIme == TG_IME_CANAL ? "true" : "false");
            strcpy(tgAviso, tgModoIme == TG_IME_CANAL ? "Criando canal..." : "Criando grupo...");
        }
        else {
            if (tgModoIme == TG_IME_TELEFONE) tgEnviar("{\"@type\":\"setAuthenticationPhoneNumber\",\"phone_number\":\"%s\"}", texto);
            else if (tgModoIme == TG_IME_CODIGO) tgEnviar("{\"@type\":\"checkAuthenticationCode\",\"code\":\"%s\"}", texto);
            else tgEnviar("{\"@type\":\"checkAuthenticationPassword\",\"password\":\"%s\"}", texto);
            strcpy(tgAviso, "Enviando...");
        }
        pthread_mutex_unlock(&tgTrava);
    }
    tgResponderA = 0;
    tgEditarMsg = 0;
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
    tgMaisAntiga = 0;
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
    if (tgFiltro) { tgAlvo = 0; tgPedirHistorico(0); return; }   // filtrado: sempre das mais novas
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
    if (tgFiltro) { tgPedirHistorico(0); return; }
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
    tgFiltro = 0;
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
// ---------- Seleção de várias mensagens ----------
static void tgMarcar(long long id) {
    for (int k = 0; k < tgTotalMarcadas; k++) {
        if (tgMarcadas[k] != id) continue;
        tgMarcadas[k] = tgMarcadas[--tgTotalMarcadas];
        return;
    }
    if (tgTotalMarcadas < 100) tgMarcadas[tgTotalMarcadas++] = id;
    else strcpy(tgAviso, "Máximo de 100 mensagens.");
}

static void tgSairSelecao() {
    tgSelecionando = false;
    tgSelMenuAberto = false;
    tgConfirmarApagar = false;
    tgTotalMarcadas = 0;
}

// Lista "[1,2,3]" em ordem de envio (a ordem das mensagens na conversa).
static void tgListaMarcadas(char* saida, size_t tamanho) {
    size_t n = (size_t)snprintf(saida, tamanho, "[");
    bool primeira = true;
    for (int i = 0; i < tgTotalMensagens && n + 24 < tamanho; i++) {
        for (int k = 0; k < tgTotalMarcadas; k++) {
            if (tgMarcadas[k] != tgMensagens[i].id) continue;
            n += (size_t)snprintf(saida + n, tamanho - n, primeira ? "%lld" : ",%lld", tgMensagens[i].id);
            primeira = false;
        }
    }
    snprintf(saida + n, tamanho - n, "]");
}

static void tgCopiarMarcadas() {
    size_t n = 0;
    tgCopiado[0] = '\0';
    for (int i = 0; i < tgTotalMensagens; i++) {
        bool marcada = false;
        for (int k = 0; k < tgTotalMarcadas && !marcada; k++) marcada = tgMarcadas[k] == tgMensagens[i].id;
        if (!marcada || !tgMensagens[i].texto[0]) continue;
        n += (size_t)snprintf(tgCopiado + n, sizeof(tgCopiado) - n, n ? "\n%s" : "%s", tgMensagens[i].texto);
        if (n >= sizeof(tgCopiado) - 1) break;
    }
    strcpy(tgAviso, tgCopiado[0] ? "Texto copiado. Quadrado > Colar texto copiado" : "As mensagens escolhidas não têm texto.");
}

static void tgControlesSelMenu(bool xis, bool bolinha, int passo) {
    if (bolinha) { tgSelMenuAberto = false; tgConfirmarApagar = false; return; }
    if (passo) { tgSelMenuSel = (tgSelMenuSel + passo + 3) % 3; tgConfirmarApagar = false; return; }
    if (!xis) return;
    if (tgTotalMarcadas == 0) { strcpy(tgAviso, "Nenhuma mensagem marcada."); tgSelMenuAberto = false; return; }
    if (tgSelMenuSel == 0) {
        tgEncaminharMsg = -1;   // várias: a lista sai de tgMarcadas
        tgEncaminharDe = tgAberta;
        tgSelMenuAberto = false;
        tgSelecionando = false;
    }
    else if (tgSelMenuSel == 1) {
        if (!tgConfirmarApagar) { tgConfirmarApagar = true; return; }
        static char lista[2600];
        tgListaMarcadas(lista, sizeof(lista));
        tgEnviar("{\"@type\":\"deleteMessages\",\"chat_id\":%lld,\"message_ids\":%s,\"revoke\":true}", tgAberta, lista);
        snprintf(tgAviso, sizeof(tgAviso), "%d mensagem(ns) apagada(s).", tgTotalMarcadas);
        tgRemoverMensagens(tgMarcadas, tgTotalMarcadas);
        tgSairSelecao();
    }
    else {
        tgCopiarMarcadas();
        tgSairSelecao();
    }
}

// ---------- Enviar arquivo ----------
static void tgSeletorListar() {
    tgSeletorTotal = 0;
    tgSeletorSel = 0;
    DIR* pasta = opendir(tgSeletorPasta);
    if (!pasta) return;
    struct dirent* e;
    while ((e = readdir(pasta)) != NULL && tgSeletorTotal < 300) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(tgSeletorItens[0].nome)) continue;
        TgEntrada* t = &tgSeletorItens[tgSeletorTotal++];
        snprintf(t->nome, sizeof(t->nome), "%s", e->d_name);
        t->pasta = e->d_type == DT_DIR;
    }
    closedir(pasta);
    // Pastas primeiro, depois arquivos, em ordem alfabética.
    for (int i = 1; i < tgSeletorTotal; i++) {
        TgEntrada atual = tgSeletorItens[i];
        int j = i - 1;
        while (j >= 0 && (tgSeletorItens[j].pasta < atual.pasta ||
               (tgSeletorItens[j].pasta == atual.pasta && strcasecmp(tgSeletorItens[j].nome, atual.nome) > 0))) {
            tgSeletorItens[j + 1] = tgSeletorItens[j];
            j--;
        }
        tgSeletorItens[j + 1] = atual;
    }
}

static void tgJsonEscapar(const char* entrada, char* saida, size_t tamanho) {
    size_t n = 0;
    for (const char* c = entrada; *c && n + 3 < tamanho; c++) {
        if (*c == '"' || *c == '\\') saida[n++] = '\\';
        saida[n++] = *c;
    }
    saida[n] = '\0';
}

static void tgControlesSeletor(int passo, bool xis, bool bolinha, bool triangulo) {
    if (triangulo) { tgSeletorAberto = false; return; }
    if (bolinha) {
        char* barra = strrchr(tgSeletorPasta, '/');
        if (!barra || strcmp(tgSeletorPasta, "/") == 0) { tgSeletorAberto = false; return; }
        if (barra == tgSeletorPasta) tgSeletorPasta[1] = '\0';
        else *barra = '\0';
        tgSeletorListar();
        return;
    }
    if (passo && tgSeletorTotal > 0) { tgSeletorSel = (tgSeletorSel + passo + tgSeletorTotal) % tgSeletorTotal; return; }
    if (!xis || tgSeletorTotal == 0) return;
    const TgEntrada* e = &tgSeletorItens[tgSeletorSel];
    char caminho[420];
    snprintf(caminho, sizeof(caminho), "%s/%s", strcmp(tgSeletorPasta, "/") == 0 ? "" : tgSeletorPasta, e->nome);
    if (e->pasta) {
        snprintf(tgSeletorPasta, sizeof(tgSeletorPasta), "%s", caminho);
        tgSeletorListar();
        return;
    }
    char escapado[840];
    tgJsonEscapar(caminho, escapado, sizeof(escapado));
    tgEnviar("{\"@type\":\"sendMessage\",\"chat_id\":%lld,\"input_message_content\":{\"@type\":\"inputMessageDocument\",\"document\":{\"@type\":\"inputFileLocal\",\"path\":\"%s\"}}}", tgAberta, escapado);
    tgSelId = 0;
    tgSeletorAberto = false;
    snprintf(tgAviso, sizeof(tgAviso), "Enviando %.80s...", e->nome);
}

// ---------- Gravar áudio: microfone a 16 kHz, Opus em OGG, enviado como mensagem de voz ----------
static int32_t tgGravacaoUsuario = 0;
static long long tgGravacaoConversa = 0;

static uint32_t tgOggCrc(const unsigned char* dados, int n) {
    static uint32_t tabela[256];
    static bool pronta = false;
    if (!pronta) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t r = i << 24;
            for (int k = 0; k < 8; k++) r = (r & 0x80000000u) ? (r << 1) ^ 0x04C11DB7u : (r << 1);
            tabela[i] = r;
        }
        pronta = true;
    }
    uint32_t crc = 0;
    for (int i = 0; i < n; i++) crc = (crc << 8) ^ tabela[((crc >> 24) & 255) ^ dados[i]];
    return crc;
}

// Uma página OGG com um pacote. tipo: 2 primeira página, 4 última.
static void tgOggPagina(FILE* f, const unsigned char* pacote, int tamanho, int tipo, uint64_t granulo, uint32_t sequencia) {
    static unsigned char pagina[27 + 255 + 8192];
    int segmentos = tamanho / 255 + 1;
    memcpy(pagina, "OggS", 4);
    pagina[4] = 0;
    pagina[5] = (unsigned char)tipo;
    for (int i = 0; i < 8; i++) pagina[6 + i] = (unsigned char)(granulo >> (8 * i));
    const uint32_t serie = 0x484E5654u;
    for (int i = 0; i < 4; i++) {
        pagina[14 + i] = (unsigned char)(serie >> (8 * i));
        pagina[18 + i] = (unsigned char)(sequencia >> (8 * i));
        pagina[22 + i] = 0;
    }
    pagina[26] = (unsigned char)segmentos;
    int resto = tamanho;
    for (int i = 0; i < segmentos; i++) { pagina[27 + i] = (unsigned char)(resto >= 255 ? 255 : resto); resto -= 255; }
    memcpy(pagina + 27 + segmentos, pacote, (size_t)tamanho);
    int total = 27 + segmentos + tamanho;
    uint32_t crc = tgOggCrc(pagina, total);
    for (int i = 0; i < 4; i++) pagina[22 + i] = (unsigned char)(crc >> (8 * i));
    fwrite(pagina, 1, (size_t)total, f);
}

static void* threadGravar(void*) {
    typedef int (*AbrirMic)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef int (*LerMic)(int32_t, void*);
    typedef int (*FecharMic)(int32_t);
    int mic = ((AbrirMic)sceAudioInOpen)(tgGravacaoUsuario, 0, 0, 256, 16000, 0);
    if (mic < 0) mic = ((AbrirMic)sceAudioInOpen)(255, 0, 0, 256, 16000, 0);
    int erroMic = mic;
    int erro = 0;
    OpusEncoder* codificador = mic >= 0 ? opus_encoder_create(16000, 1, 2048 /* OPUS_APPLICATION_VOIP */, &erro) : NULL;
    char caminho[160];
    snprintf(caminho, sizeof(caminho), TELEGRAM_PASTA "/voz_%llu.ogg", (unsigned long long)sceKernelGetProcessTime());
    FILE* f = codificador ? fopen(caminho, "wb") : NULL;
    if (!f) {
        if (codificador) opus_encoder_destroy(codificador);
        if (mic >= 0) ((FecharMic)sceAudioInClose)(mic);
        pthread_mutex_lock(&tgTrava);
        if (mic < 0) snprintf(tgAviso, sizeof(tgAviso), "Microfone não encontrado (%x). Ligue um fone com microfone no controle.", (unsigned)erroMic);
        else snprintf(tgAviso, sizeof(tgAviso), "Não foi possível gravar (opus %d).", erro);
        pthread_mutex_unlock(&tgTrava);
        tgGravando = false;
        tgSujo = true;
        return NULL;
    }
    int atraso = 0;
    opus_encoder_ctl(codificador, 4027 /* OPUS_GET_LOOKAHEAD */, &atraso);
    int preSalto = atraso * 3;   // em amostras de 48 kHz
    unsigned char cabecalho[19] = { 'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 1 };
    cabecalho[10] = (unsigned char)(preSalto & 255); cabecalho[11] = (unsigned char)(preSalto >> 8);
    cabecalho[12] = 16000 & 255; cabecalho[13] = (16000 >> 8) & 255; cabecalho[14] = 0; cabecalho[15] = 0;
    cabecalho[16] = 0; cabecalho[17] = 0; cabecalho[18] = 0;
    uint32_t sequencia = 0;
    tgOggPagina(f, cabecalho, 19, 2, 0, sequencia++);
    unsigned char etiquetas[26];
    memcpy(etiquetas, "OpusTags", 8);
    etiquetas[8] = 10; etiquetas[9] = etiquetas[10] = etiquetas[11] = 0;
    memcpy(etiquetas + 12, "HyperNeiva", 10);
    memset(etiquetas + 22, 0, 4);
    tgOggPagina(f, etiquetas, 26, 0, 0, sequencia++);

    static int16_t lido[256], quadro[320];
    static unsigned char pacotes[2][4000];
    int noQuadro = 0, pendente = -1, tamanhoPendente = 0, atual = 0, errosSeguidos = 0;
    uint64_t granulo = (uint64_t)preSalto, granuloPendente = 0, amostras = 0;
    while (!tgGravacaoParar && !tgGravacaoCancelar && amostras < 16000ULL * 600) {
        int n = ((LerMic)sceAudioInInput)(mic, lido);
        if (n < 0) { if (++errosSeguidos > 300) break; sceKernelUsleep(5000); continue; }
        errosSeguidos = 0;
        if (n == 0 || n > 256) n = 256;
        for (int i = 0; i < n; i++) {
            quadro[noQuadro++] = lido[i];
            if (noQuadro < 320) continue;
            noQuadro = 0;
            int t = opus_encode(codificador, quadro, 320, pacotes[atual], 4000);
            if (t <= 0) continue;
            granulo += 960;
            if (pendente >= 0) tgOggPagina(f, pacotes[pendente], tamanhoPendente, 0, granuloPendente, sequencia++);
            pendente = atual; tamanhoPendente = t; granuloPendente = granulo; atual ^= 1;
        }
        amostras += (uint64_t)n;
        int segundos = (int)(amostras / 16000);
        if (segundos != tgGravacaoSegundos) { tgGravacaoSegundos = segundos; tgSujo = true; }
    }
    if (pendente >= 0) tgOggPagina(f, pacotes[pendente], tamanhoPendente, 4, granuloPendente, sequencia++);
    fclose(f);
    opus_encoder_destroy(codificador);
    ((FecharMic)sceAudioInClose)(mic);
    pthread_mutex_lock(&tgTrava);
    if (tgGravacaoCancelar || amostras < 8000) {
        remove(caminho);
        strcpy(tgAviso, tgGravacaoCancelar ? "Gravação cancelada." : "Áudio muito curto.");
    }
    else {
        tgEnviar("{\"@type\":\"sendMessage\",\"chat_id\":%lld,\"input_message_content\":{\"@type\":\"inputMessageVoiceNote\",\"voice_note\":{\"@type\":\"inputFileLocal\",\"path\":\"%s\"},\"duration\":%d,\"waveform\":\"\"}}",
                 tgGravacaoConversa, caminho, (int)(amostras / 16000));
        tgSelId = 0;
        strcpy(tgAviso, "Enviando áudio...");
    }
    pthread_mutex_unlock(&tgTrava);
    tgGravando = false;
    tgSujo = true;
    return NULL;
}

static void tgIniciarGravacao(int32_t usuario) {
    if (tgGravando || !tgAberta) return;
    tgGravacaoUsuario = usuario;
    tgGravacaoConversa = tgAberta;
    tgGravacaoParar = false;
    tgGravacaoCancelar = false;
    tgGravacaoSegundos = 0;
    tgGravando = true;
    pthread_t t;
    pthread_attr_t atributos;
    pthread_attr_init(&atributos);
    pthread_attr_setstacksize(&atributos, 512 * 1024);
    if (pthread_create(&t, &atributos, threadGravar, NULL) == 0) pthread_detach(t);
    else { tgGravando = false; strcpy(tgAviso, "Não foi possível gravar."); }
    pthread_attr_destroy(&atributos);
}

// ---- Ações dos menus do Quadrado (com a trava pega) ----
static void tgSilenciar(TgConversa* c) {
    if (!c) return;
    bool mudo = !c->mudo;
    tgEnviar("{\"@type\":\"setChatNotificationSettings\",\"chat_id\":%lld,\"notification_settings\":{\"@type\":\"chatNotificationSettings\",\"use_default_mute_for\":false,\"mute_for\":%d,"
        "\"use_default_sound\":true,\"sound_id\":0,\"use_default_show_preview\":true,\"show_preview\":true,\"use_default_mute_stories\":true,\"mute_stories\":false,"
        "\"use_default_story_sound\":true,\"story_sound_id\":0,\"use_default_show_story_poster\":true,\"show_story_poster\":true,"
        "\"use_default_disable_pinned_message_notifications\":true,\"disable_pinned_message_notifications\":false,"
        "\"use_default_disable_mention_notifications\":true,\"disable_mention_notifications\":false}}", c->id, mudo ? 2147483647 : 0);
    c->mudo = mudo;
    strcpy(tgAviso, mudo ? "Conversa silenciada." : "Notificações ativadas.");
}
static void tgArquivar(TgConversa* c) {
    if (!c) return;
    bool arquivada = c->ordens[1] != 0;
    tgEnviar("{\"@type\":\"addChatToList\",\"chat_id\":%lld,\"chat_list\":{\"@type\":\"%s\"}}", c->id, arquivada ? "chatListMain" : "chatListArchive");
    strcpy(tgAviso, arquivada ? "Conversa desarquivada." : "Conversa arquivada.");
}
static void tgAplicarFiltro() {
    tgLimparJanela();
    tgPedirHistorico(0);
    snprintf(tgAviso, sizeof(tgAviso), "Mostrando: %s", tgNomesFiltro[tgFiltro]);
}
static void tgExecutarAcao(int item, int32_t usuario) {
    TgConversa* c = tgConversa(tgAberta);
    if (item != TG_A_SAIR) tgConfirmar = false;
    switch (item) {
    case TG_A_ESCREVER: tgAcoesAberto = false; tgAbrirTeclado(usuario, TG_IME_MENSAGEM, L"Mensagem", 500); break;
    case TG_A_ARQUIVO: tgAcoesAberto = false; tgSeletorAberto = true; tgSeletorListar(); break;
    case TG_A_GRAVAR: tgAcoesAberto = false; tgIniciarGravacao(usuario); break;
    case TG_A_COLAR:
        tgAcoesAberto = false;
        snprintf(tgColarNoTeclado, sizeof(tgColarNoTeclado), "%s", tgCopiado);
        tgAbrirTeclado(usuario, TG_IME_MENSAGEM, L"Mensagem", 500);
        break;
    case TG_A_FILTRO: tgFiltro = (tgFiltro + 1) % TG_TOTAL_FILTROS; tgAplicarFiltro(); break;
    case TG_A_MUDO: tgSilenciar(c); break;
    case TG_A_LIDO:
        if (tgTotalMensagens > 0) tgMarcarLida(tgMensagens[tgTotalMensagens - 1].id);
        tgEnviar("{\"@type\":\"readAllChatMentions\",\"chat_id\":%lld}", tgAberta);
        tgEnviar("{\"@type\":\"toggleChatIsMarkedAsUnread\",\"chat_id\":%lld,\"is_marked_as_unread\":false}", tgAberta);
        if (c) c->naoLidas = 0;
        strcpy(tgAviso, "Marcada como lida.");
        break;
    case TG_A_ARQUIVAR: tgArquivar(c); break;
    case TG_A_OPCOES: tgAcoesSub = true; tgAcoesSel = 0; break;
    case TG_A_ENTRAR: tgEnviar("{\"@type\":\"joinChat\",\"chat_id\":%lld}", tgAberta); strcpy(tgAviso, "Entrando..."); tgAcoesAberto = false; break;
    case TG_A_SAIR:
        if (!tgConfirmar) { tgConfirmar = true; break; }
        tgConfirmar = false;
        tgAcoesAberto = false;
        tgEnviar("{\"@type\":\"leaveChat\",\"chat_id\":%lld}", tgAberta);
        tgFecharConversa();
        strcpy(tgAviso, "Você saiu.");
        break;
    }
}
static void tgExecutarLista(int item, int32_t usuario) {
    TgConversa* c = tgConversaMarcadaNaLista();
    if (item != TG_L_SAIR && item != TG_L_APAGAR_PASTA) tgConfirmar = false;
    if (item >= TG_L_MARCAR) {
        long long id = tgConversas[tgIndice[item - TG_L_MARCAR]].id;
        int achou = -1;
        for (int i = 0; i < tgNovaPastaTotal; i++) if (tgNovaPastaIds[i] == id) achou = i;
        if (achou >= 0) { tgNovaPastaIds[achou] = tgNovaPastaIds[--tgNovaPastaTotal]; }
        else if (tgNovaPastaTotal < 100) tgNovaPastaIds[tgNovaPastaTotal++] = id;
        return;
    }
    if (item >= TG_L_PASTA && item < TG_L_NOVA_PASTA) {
        if (c) {
            tgEnviar("{\"@type\":\"addChatToList\",\"chat_id\":%lld,\"chat_list\":{\"@type\":\"chatListFolder\",\"chat_folder_id\":%d}}", c->id, tgPastas[item].id);
            snprintf(tgAviso, sizeof(tgAviso), "Adicionada à pasta %.60s.", tgPastas[item].nome);
        }
        tgListaMenuAberto = false;
        return;
    }
    switch (item) {
    case TG_L_NOVA_PASTA: tgListaMenuAberto = false; tgChatDaPasta = c ? c->id : 0; tgAbrirTeclado(usuario, TG_IME_PASTA, L"Nome da pasta", 12); break;
    case TG_L_ADD_PASTA: tgListaModo = 1; tgListaMenuSel = 0; break;
    case TG_L_OPCOES: tgListaModo = 2; tgListaMenuSel = 0; break;
    case TG_L_CRIAR_OK: {
        if (!tgNovaPastaTotal) { strcpy(tgAviso, "Marque pelo menos uma conversa."); break; }
        static char ids[2200];
        int n = snprintf(ids, sizeof(ids), "[");
        for (int i = 0; i < tgNovaPastaTotal && n < (int)sizeof(ids) - 24; i++) n += snprintf(ids + n, sizeof(ids) - n, "%s%lld", i ? "," : "", tgNovaPastaIds[i]);
        snprintf(ids + n, sizeof(ids) - n, "]");
        tgEnviar("{\"@type\":\"createChatFolder\",\"folder\":{\"@type\":\"chatFolder\",\"name\":{\"@type\":\"chatFolderName\",\"text\":{\"@type\":\"formattedText\",\"text\":\"%s\",\"entities\":[]},\"animate_custom_emoji\":false},"
            "\"icon\":null,\"color_id\":-1,\"is_shareable\":false,\"pinned_chat_ids\":[],\"included_chat_ids\":%s,\"excluded_chat_ids\":[],\"exclude_muted\":false,\"exclude_read\":false,"
            "\"exclude_archived\":false,\"include_contacts\":false,\"include_non_contacts\":false,\"include_bots\":false,\"include_groups\":false,\"include_channels\":false}}", tgNovaPastaNome, ids);
        snprintf(tgAviso, sizeof(tgAviso), "Pasta \"%.40s\" criada com %d conversa(s).", tgNovaPastaNome, tgNovaPastaTotal);
        tgListaMenuAberto = false; tgListaModo = 0;
        break;
    }
    case TG_L_APAGAR_PASTA:
        if (!tgConfirmar) { tgConfirmar = true; break; }
        tgConfirmar = false;
        tgListaMenuAberto = false;
        if (tgAba >= 1 && tgAba <= tgTotalPastas) {
            tgEnviar("{\"@type\":\"deleteChatFolder\",\"chat_folder_id\":%d,\"leave_chat_ids\":[]}", tgPastas[tgAba - 1].id);
            snprintf(tgAviso, sizeof(tgAviso), "Pasta %.60s apagada.", tgPastas[tgAba - 1].nome);
            tgAba = 0;
        }
        break;
    case TG_L_GRUPO: tgListaMenuAberto = false; tgAbrirTeclado(usuario, TG_IME_GRUPO, L"Nome do grupo", 128); break;
    case TG_L_CANAL: tgListaMenuAberto = false; tgAbrirTeclado(usuario, TG_IME_CANAL, L"Nome do canal", 128); break;
    case TG_L_MUDO: tgSilenciar(c); break;
    case TG_L_ARQUIVAR: tgArquivar(c); tgListaMenuAberto = false; break;
    case TG_L_SAIR:
        if (!tgConfirmar) { tgConfirmar = true; break; }
        tgConfirmar = false;
        tgListaMenuAberto = false;
        if (c) { tgEnviar("{\"@type\":\"leaveChat\",\"chat_id\":%lld}", c->id); snprintf(tgAviso, sizeof(tgAviso), "Você saiu de %.60s.", c->titulo); }
        break;
    }
}

bool telegramControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    static int segurado = 0;
    extern bool videoRodando, video_minimizado;
    tgAbrirPendente();
    if (tgImeAberto) return false;
    uint32_t novos = botoes & ~antigos;
    if (tgGravando) {
        if (novos & ORBIS_PAD_BUTTON_CROSS) tgGravacaoParar = true;
        else if (novos & ORBIS_PAD_BUTTON_CIRCLE) tgGravacaoCancelar = true;
        tgSujo = true;
        return false;
    }
    if (tgTrocandoVideo) {
        if ((novos & ORBIS_PAD_BUTTON_CIRCLE) || ++tgTrocaEspera > 60 * 40) tgCancelarTroca();
        return false;
    }
    {
        // O vídeo foi fechado: o começo do próximo não vai ser usado.
        extern bool videoRodando;
        if (tgPreparadoArquivo && !videoRodando && !tgPendente) {
            pthread_mutex_lock(&tgTrava);
            tgCancelarPreparado();
            pthread_mutex_unlock(&tgTrava);
        }
    }
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
    {
        // L1/R1 trocam de vídeo no player (dois toques). Logo depois de o vídeo fechar ou enquanto
        // o próximo carrega, o toque que sobrou não pode levar a conversa ao começo ou ao fim.
        static int semVideo = 0;
        if ((videoRodando && !video_minimizado) || tgPendente || tgAbrirAgora || tgTrocandoVideo) semVideo = 0;
        else if (semVideo < 1000) semVideo++;
        if (semVideo < 90) aba = 0;
    }

    // Na conversa o X decide ao soltar: toque faz a ação da mensagem, segurar abre as opções.
    static int xTempo = 0;
    static bool xNaConversa = false;
    bool xCurto = false, xLongo = false;
    if (xis && tgTela == TG_MENSAGENS && !tgVisor && !tgMenuAberto && !tgEmBusca && !tgFilaAberta && !tgAcoesAberto && !tgSeletorAberto && !tgEncaminharMsg && !tgSelecionando && !tgSelMenuAberto) { xNaConversa = true; xTempo = 0; }
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
                if (d->arquivo > 0) {
                    tgEnviar("{\"@type\":\"cancelDownloadFile\",\"file_id\":%d,\"only_if_pending\":false}", d->arquivo);
                    if (tgPendente == d->arquivo) { tgPendente = 0; tgAviso[0] = '\0'; }
                    TgArquivo* a = tgArquivo(d->arquivo);
                    if (a) a->pedido = false;
                }
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
            else if (opcao == -3) { tgEncaminharMsg = m->id; tgEncaminharDe = tgAberta; }
            else if (opcao == -4) { tgSelecionando = true; tgTotalMarcadas = 0; tgMarcar(m->id); }
            else if (opcao == -6) {
                // Abre o teclado já com o texto; o resultado vira editMessageText.
                tgEditarMsg = m->id;
                snprintf(tgColarNoTeclado, sizeof(tgColarNoTeclado), "%s", m->texto);
                tgAbrirTeclado(usuario, TG_IME_MENSAGEM, L"Editar mensagem", 500);
            }
            else if (opcao == -5) {
                snprintf(tgCopiado, sizeof(tgCopiado), "%s", m->texto);
                strcpy(tgAviso, "Texto copiado. Quadrado > Colar texto copiado");
            }
            else if (opcao < 0) tgBaixarParaPasta(m);
            else if (opcao < tgMenuTotalUrls) tgAbrirLink(tgMenuUrls[opcao]);
        }
    }
    else if (tgAcoesAberto) {
        int itens[16], total = tgAcoesItens(itens);
        if (bolinha && tgAcoesSub) { tgAcoesSub = false; tgAcoesSel = 0; tgConfirmar = false; }
        else if (bolinha || quadrado) { tgAcoesAberto = false; tgConfirmar = false; }
        else if (passo) { tgAcoesSel = (tgAcoesSel + passo + total) % total; tgConfirmar = false; }
        else if (xis && tgAcoesSel < total) tgExecutarAcao(itens[tgAcoesSel], usuario);
    }
    else if (tgListaMenuAberto) {
        tgOrdenarConversas();
        static int itens[TG_L_MAX_ITENS];
        int total = tgListaItens(itens);
        if (bolinha && tgListaModo != 0) { tgListaModo = 0; tgListaMenuSel = 0; tgConfirmar = false; }
        else if (bolinha || quadrado) { tgListaMenuAberto = false; tgListaModo = 0; tgConfirmar = false; }
        else if (passo) { tgListaMenuSel = (tgListaMenuSel + passo + total) % total; tgConfirmar = false; }
        else if (xis && tgListaMenuSel < total) tgExecutarLista(itens[tgListaMenuSel], usuario);
    }
    else if (tgSelMenuAberto) tgControlesSelMenu(xis, bolinha, passo);
    else if (tgSelecionando && tgTela == TG_MENSAGENS) {
        if (bolinha) { tgSairSelecao(); strcpy(tgAviso, "Seleção cancelada."); }
        else if (passo) tgMoverSelecao(passo);
        else if (xis && tgTotalMensagens > 0) tgMarcar(tgMensagens[tgIndiceSelecionado()].id);
        else if (quadrado) { tgSelMenuAberto = true; tgSelMenuSel = 0; tgConfirmarApagar = false; }
    }
    else if (tgSeletorAberto) tgControlesSeletor(passo, xis, bolinha, triangulo);
    else if (tgEncaminharMsg) {
        tgOrdenarConversas();
        if (bolinha) { if (tgEncaminharMsg == -1) tgSairSelecao(); tgEncaminharMsg = 0; }
        else if (passo && tgVisiveis > 0) {
            tgSelecao = (tgSelecao + passo + tgVisiveis) % tgVisiveis;
            tgOrdenarConversas();
            if (tgSelecao >= tgVisiveis - 5) tgCarregarConversas();
        }
        else if (xis && tgVisiveis > 0) {
            long long destino = tgConversas[tgIndice[tgSelecao]].id;
            static char lista[2600];
            if (tgEncaminharMsg == -1) tgListaMarcadas(lista, sizeof(lista));
            else snprintf(lista, sizeof(lista), "[%lld]", tgEncaminharMsg);
            tgEnviar("{\"@type\":\"forwardMessages\",\"chat_id\":%lld,\"from_chat_id\":%lld,\"message_ids\":%s,\"send_copy\":false,\"remove_caption\":false}", destino, tgEncaminharDe, lista);
            if (tgEncaminharMsg == -1) tgSairSelecao();
            tgEncaminharMsg = 0;
            const TgConversa* c = tgConversaPorId(destino);
            snprintf(tgAviso, sizeof(tgAviso), "Encaminhada para %.60s", c ? c->titulo : "a conversa");
        }
    }
    else if (tgEmBusca) tgControlesBusca(passo, xis, bolinha, triangulo, usuario);
    else if (tgTela == TG_MENSAGENS) {
        if (bolinha) { tgSairSelecao(); tgFecharConversa(); }
        else if (quadrado) { tgAcoesAberto = true; tgAcoesSub = false; tgAcoesSel = 0; tgConfirmar = false; }
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
        else if (quadrado) { tgListaMenuAberto = true; tgListaModo = 0; tgListaMenuSel = 0; tgConfirmar = false; }
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
    {
        static char visto[160] = "";
        static uint64_t desde = 0;
        uint64_t agora = tgAgoraMs();
        pthread_mutex_lock(&tgTrava);
        if (!tgAviso[0]) visto[0] = '\0';
        else if (strcmp(visto, tgAviso) != 0) { snprintf(visto, sizeof(visto), "%s", tgAviso); desde = agora; }
        else if (!strchr(tgAviso, '%') && agora - desde > 5000) { tgAviso[0] = '\0'; visto[0] = '\0'; tgSujo = true; }
        pthread_mutex_unlock(&tgTrava);
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
        if (!tgEmojis) {
            // Com o jailbreak o app enxerga a raiz real: o pacote fica em /mnt/sandbox/pfsmnt/<ID>-app0.
            DIR* montagens = opendir("/mnt/sandbox/pfsmnt");
            struct dirent* e;
            while (montagens && !tgEmojis && (e = readdir(montagens)) != NULL) {
                size_t n = strlen(e->d_name);
                if (n < 6 || strcmp(e->d_name + n - 5, "-app0") != 0) continue;
                char caminho[200];
                snprintf(caminho, sizeof(caminho), "/mnt/sandbox/pfsmnt/%s/assets/emoji.pack", e->d_name);
                FILE* teste = fopen(caminho, "rb");
                if (!teste) continue;
                fseek(teste, 0, SEEK_END);
                long tamanho = ftell(teste);
                fclose(teste);
                if (tamanho > 4000000) tgAbrirEmojis(caminho);
            }
            if (montagens) closedir(montagens);
        }
        extern void hnInstalarRegistradorCrashApp();
        hnInstalarRegistradorCrashApp();
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

void acaoCross_Extra() {
    if (sel == 0) {
        definirAudioModoEspecial(true);
        // Limpa os gráficos antes de entrar no Teste de Controle
        memset(trilhaL, 0, sizeof(trilhaL));
        memset(trilhaR, 0, sizeof(trilhaR));
        memset(trilhaTouch1, 0, sizeof(trilhaTouch1));
        memset(trilhaTouch2, 0, sizeof(trilhaTouch2));
        prevLx = 128; prevLy = 128;
        prevRx = 128; prevRy = 128;
        prevT1x = -1; prevT1y = -1;
        prevT2x = -1; prevT2y = -1;
        menuAtual = MENU_CONTROLE_TESTE;
    }
    else if (sel == 1) {
        definirAudioModoEspecial(true);
        // Abre a interface do Piano Virtual
        menuAtual = MENU_INSTRUMENTOS;
    }
    else if (sel == 2) iniciarTelegram();
    else if (sel == 3) menuAtual = MENU_VENTOINHA;
    else if (sel == 4) { extern void iaAbrir(); iaAbrir(); }
    else if (sel == 5) { extern void chavesAbrir(); chavesAbrir(); }
    else if (sel == 6) { extern void tempoAbrir(); tempoAbrir(); }
}

// =======================================================
// VENTOINHA
// =======================================================
// O limite é a temperatura em que a ventoinha acelera. O sistema guarda até tirar o PS4 da tomada.
// Abaixo de 60 °C ela ficaria acelerada o tempo todo, porque o console parado já passa de 50 °C.
#include <fcntl.h>
#include <sys/ioctl.h>
#define VENTOINHA_PADRAO 79
#define VENTOINHA_MINIMO 60
#define VENTOINHA_ARQUIVO "/data/HyperNeiva/configuracao/ventoinha.txt"
extern "C" int sceKernelGetCpuTemperature(uint32_t* temperatura);
extern "C" int sceKernelGetSocSensorTemperature(int sensor, uint32_t* temperatura);

static int ventoinhaEscolha = -1, ventoinhaAtiva = VENTOINHA_PADRAO;

static void ventoinhaCarregar() {
    if (ventoinhaEscolha >= 0) return;
    FILE* f = fopen(VENTOINHA_ARQUIVO, "r");
    if (f) {
        int lido = 0;
        if (fscanf(f, "%d", &lido) == 1 && lido >= VENTOINHA_MINIMO && lido <= VENTOINHA_PADRAO) ventoinhaAtiva = lido;
        fclose(f);
    }
    ventoinhaEscolha = ventoinhaAtiva;
}

static bool ventoinhaAplicar(int temperatura) {
    if (temperatura < VENTOINHA_MINIMO || temperatura > VENTOINHA_PADRAO) return false;
    int fd = open("/dev/icc_fan", O_RDONLY, 0);
    if (fd < 0) {
        snprintf(msgStatus, sizeof(msgStatus), "ERRO AO ABRIR A VENTOINHA (%d)", errno);
        msgTimer = 300;
        return false;
    }
    // O pedido lê mais que os 10 bytes do exemplo conhecido: o resto fica zerado.
    unsigned char dados[64];
    memset(dados, 0, sizeof(dados));
    dados[5] = (unsigned char)temperatura;
    int resposta = ioctl(fd, 0xC01C8F07, dados);
    close(fd);
    FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (registro) { fprintf(registro, "VENTOINHA limite=%d resposta=%d errno=%d\n", temperatura, resposta, errno); fclose(registro); }
    if (resposta < 0) {
        snprintf(msgStatus, sizeof(msgStatus), "O PS4 RECUSOU O AJUSTE (%d)", errno);
        msgTimer = 300;
        return false;
    }
    ventoinhaAtiva = temperatura;
    FILE* f = fopen(VENTOINHA_ARQUIVO, "w");
    if (f) { fprintf(f, "%d\n", temperatura); fclose(f); }
    snprintf(msgStatus, sizeof(msgStatus), "VENTOINHA AJUSTADA PARA %d C", temperatura);
    msgTimer = 240;
    return true;
}

// Devolve true quando o usuário sai da tela.
bool ventoinhaControles(uint32_t novos) {
    ventoinhaCarregar();
    if (novos & ORBIS_PAD_BUTTON_LEFT) { if (ventoinhaEscolha > VENTOINHA_MINIMO) ventoinhaEscolha--; }
    else if (novos & ORBIS_PAD_BUTTON_RIGHT) { if (ventoinhaEscolha < VENTOINHA_PADRAO) ventoinhaEscolha++; }
    else if (novos & ORBIS_PAD_BUTTON_CROSS) ventoinhaAplicar(ventoinhaEscolha);
    else if (novos & ORBIS_PAD_BUTTON_TRIANGLE) { ventoinhaEscolha = VENTOINHA_PADRAO; ventoinhaAplicar(VENTOINHA_PADRAO); }
    return (novos & ORBIS_PAD_BUTTON_CIRCLE) != 0;
}

void renderizarVentoinha(uint32_t* p) {
    static int leitura = 0;
    static uint32_t cpu = 0, soc = 0;
    ventoinhaCarregar();
    if (leitura-- <= 0) {
        leitura = 60;
        sceKernelGetCpuTemperature(&cpu);
        sceKernelGetSocSensorTemperature(0, &soc);
    }
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF101318;
    char texto[160];
    uint32_t branco = 0xFFFFFFFF, cinza = 0xFFB4BCCB, quente = cpu >= 75 ? 0xFFFF5040 : cpu >= 65 ? 0xFFFFC83C : 0xFF50E070;
    desenharTexto(p, "VENTOINHA E TEMPERATURA", 50, 110, 80, branco);
    desenharTexto(p, "Processador", 34, 110, 210, cinza);
    snprintf(texto, sizeof(texto), "%u °C", cpu);
    desenharTexto(p, texto, 120, 110, 260, quente);
    desenharTexto(p, "Sensor do chip", 34, 620, 210, cinza);
    snprintf(texto, sizeof(texto), "%u °C", soc);
    desenharTexto(p, texto, 120, 620, 260, branco);
    desenharTexto(p, "Acelerar a ventoinha a partir de", 34, 110, 470, cinza);
    snprintf(texto, sizeof(texto), "<   %d °C   >", ventoinhaEscolha);
    desenharTexto(p, texto, 76, 110, 530, branco);
    snprintf(texto, sizeof(texto), "Em uso: %d °C.  Padrão do PS4: %d °C.  Mínimo aqui: %d °C.", ventoinhaAtiva, VENTOINHA_PADRAO, VENTOINHA_MINIMO);
    desenharTexto(p, texto, 30, 110, 660, cinza);
    desenharTexto(p, "Valor baixo: PS4 mais frio, mais barulho e mais desgaste da ventoinha.", 30, 110, 720, cinza);
    desenharTexto(p, "O ajuste continua depois de reiniciar e volta ao padrão ao tirar da tomada.", 30, 110, 766, cinza);
    desenharTexto(p, "Esquerda / Direita  Escolher      X  Aplicar      Triângulo  Voltar ao padrão      O  Sair", 28, 110, 1000, cinza);
}

// =======================================================
// ALGORITMO BRESENHAM (Cria linhas contínuas geometricamente perfeitas)
// =======================================================
template<int W, int H>
void drawLineOnGrid(bool(&grid)[W][H], int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H) grid[x0][y0] = true;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void renderizarControleTeste(uint32_t* p) {
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;

    desenharTexto(p, "GERENCIADOR DE COMANDO (V1.00)", 40, 50, 40, 0xFF00AAFF);
    desenharTexto(p, "Pressione [OPTIONS] para Sair do Teste", 25, 50, 80, 0xFFAAAAAA);

    if (globalPadHandle < 0) {
        desenharTexto(p, "Controle nao inicializado pelo sistema!", 30, 50, 200, 0xFF0000FF);
        return;
    }

    OrbisPadData data;
    int res = scePadReadState(globalPadHandle, &data);
    if (res != 0) {
        desenharTexto(p, "Sinal do controle perdido. Reconecte o controle...", 30, 50, 200, 0xFF0000FF);
        return;
    }

    // =======================================================
    // LEITURA DOS SENSORES (MEMÓRIA BRUTA)
    // =======================================================
    uint8_t* raw = (uint8_t*)&data;
    uint32_t buttons = *(uint32_t*)(raw + 0x00);
    uint8_t lx = raw[0x04];
    uint8_t ly = raw[0x05];
    uint8_t rx = raw[0x06];
    uint8_t ry = raw[0x07];
    uint8_t l2 = raw[0x08];
    uint8_t r2 = raw[0x09];

    // GIROSCOPIO (Offset 0x12) - ACELEROMETRO (Offset 0x18)
    int16_t gx = *(int16_t*)(raw + 0x12);
    int16_t gy = *(int16_t*)(raw + 0x14);
    int16_t gz = *(int16_t*)(raw + 0x16);
    int16_t ax = *(int16_t*)(raw + 0x18);
    int16_t ay = *(int16_t*)(raw + 0x1A);
    int16_t az = *(int16_t*)(raw + 0x1C);

    // =======================================================
    // FEEDBACK DE HARDWARE (SOM, LUZ E MICROFONE)
    // =======================================================
    extern int32_t global_uId;
    if (padAudioPort < 0) {
        // Tipo 4 = BGM/Pad (Comum em drivers PS4) ou fallback 6
        padAudioPort = sceAudioOutOpen(global_uId, (OrbisAudioOutPort)4, 0, 1024, 48000, 1);
        ((void(*)(int, int))sceAudioOutSetMixLevelPadSpk)(padAudioPort, 255);
    }
    if (padMicPort < 0) {
        padMicPort = 1; // Usado como flag de "aberto"
        sceAudioInInit();
        ((void(*)())sceAudioInOpen)(); 
    }

    // Brilho dinâmico via Gatilhos (Base de 30% + Escala de 70%)
    float intensity = 0.3f + 0.7f * ((l2 > r2 ? l2 : r2) / 255.0f);

    bool anyButtonPressed = false;
    for (int i = 0; i < 8; i++) {
        uint32_t btn = colors[i].btn;
        if (buttons & btn) {
            anyButtonPressed = true;
            OrbisPadColor color = { (uint8_t)(colors[i].r * intensity), (uint8_t)(colors[i].g * intensity), (uint8_t)(colors[i].b * intensity) };
            scePadSetLightBar(globalPadHandle, &color);

            // Som apenas no primeiro pressionamento (Setas e Botões Principais)
            if (!(prevButtonsTest & btn)) {
                int16_t* sfx = NULL;
                if (btn == ORBIS_PAD_BUTTON_UP || btn == ORBIS_PAD_BUTTON_LEFT || btn == ORBIS_PAD_BUTTON_TRIANGLE) sfx = sfxUpData;
                else if (btn == ORBIS_PAD_BUTTON_DOWN || btn == ORBIS_PAD_BUTTON_RIGHT || btn == ORBIS_PAD_BUTTON_SQUARE) sfx = sfxDownData;
                else if (btn == ORBIS_PAD_BUTTON_CROSS) sfx = sfxCrossData;
                else if (btn == ORBIS_PAD_BUTTON_CIRCLE) sfx = sfxCircleData;
                if (sfx && padAudioPort >= 0) sceAudioOutOutput(padAudioPort, sfx);
            }
        }
    }
    if (!anyButtonPressed) {
        OrbisPadColor color = { 0, 0, 0 };
        scePadSetLightBar(globalPadHandle, &color);
    }

    prevButtonsTest = buttons;

    uint8_t touchNum = raw[0x34];
    uint16_t t0_x = 0; uint16_t t0_y = 0;
    uint16_t t1_x = 0; uint16_t t1_y = 0;

    if (touchNum > 0) {
        t0_x = *(uint16_t*)(raw + 0x3C);
        t0_y = *(uint16_t*)(raw + 0x3E);
    }
    if (touchNum > 1) {
        t1_x = *(uint16_t*)(raw + 0x44);
        t1_y = *(uint16_t*)(raw + 0x46);
    }

    // Funcao auxiliar para desenhar a base dos circulos
    auto drawCircle = [&](int cx, int cy, int r, uint32_t color, bool fill) {
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    if (fill || (x * x + y * y >= (r - 3) * (r - 3))) {
                        int px = cx + x; int py = cy + y;
                        if (px >= 0 && px < 1920 && py >= 0 && py < 1080) p[py * 1920 + px] = color;
                    }
                }
            }
        }
    };

    // =======================================================
    // PARTE 1: TOPO (BOTOES DIGITAIS TODOS ALINHADOS)
    // =======================================================
    int dX = 100, dY = 150;
    desenharTexto(p, "D-PAD", 30, dX, dY - 20, 0xFF00AAFF);
    desenharTexto(p, "[ CIMA ]", 35, dX, dY, (buttons & ORBIS_PAD_BUTTON_UP) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ BAIXO ]", 35, dX, dY + 45, (buttons & ORBIS_PAD_BUTTON_DOWN) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ ESQ ]", 35, dX, dY + 90, (buttons & ORBIS_PAD_BUTTON_LEFT) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ DIR ]", 35, dX, dY + 135, (buttons & ORBIS_PAD_BUTTON_RIGHT) ? 0xFFFFFFFF : 0xFF555555);

    int eX = 650, eY = 150;
    desenharTexto(p, "SISTEMA E OMBRO", 30, eX + 150, eY - 20, 0xFF00AAFF);
    desenharTexto(p, "[ L1 ]", 35, eX, eY, (buttons & ORBIS_PAD_BUTTON_L1) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ R1 ]", 35, eX, eY + 45, (buttons & ORBIS_PAD_BUTTON_R1) ? 0xFFFFFFFF : 0xFF555555);

    desenharTexto(p, "[ L3 CLICK ]", 35, eX + 200, eY, (buttons & ORBIS_PAD_BUTTON_L3) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ R3 CLICK ]", 35, eX + 200, eY + 45, (buttons & ORBIS_PAD_BUTTON_R3) ? 0xFFFFFFFF : 0xFF555555);

    desenharTexto(p, "[ TOUCH CLIQUE ]", 35, eX + 450, eY, (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ OPTIONS ] (SAIR)", 35, eX + 450, eY + 45, (buttons & ORBIS_PAD_BUTTON_OPTIONS) ? 0xFF0000FF : 0xFF555555);

    int aX = 1500, aY = 150;
    desenharTexto(p, "BOTOES DE ACAO", 30, aX, aY - 20, 0xFF00AAFF);
    desenharTexto(p, "[ TRIANGULO ]", 35, aX, aY, (buttons & ORBIS_PAD_BUTTON_TRIANGLE) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ BOLINHA ]", 35, aX, aY + 45, (buttons & ORBIS_PAD_BUTTON_CIRCLE) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ CRUZ (X) ]", 35, aX, aY + 90, (buttons & ORBIS_PAD_BUTTON_CROSS) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto(p, "[ QUADRADO ]", 35, aX, aY + 135, (buttons & ORBIS_PAD_BUTTON_SQUARE) ? 0xFFFFFFFF : 0xFF555555);

    // =======================================================
    // PARTE 2: CENTRO (L3 -> R3 -> TOUCHPAD)
    // =======================================================

    // Analógico L3
    if (lx >= 115 && lx <= 140 && ly >= 115 && ly <= 140) { memset(trilhaL, 0, sizeof(trilhaL)); prevLx = 128; prevLy = 128; }
    else { drawLineOnGrid(trilhaL, prevLx, prevLy, lx, ly); prevLx = lx; prevLy = ly; }

    int lCenterX = 220; int lCenterY = 600; int radius = 150;
    desenharTexto(p, "Centro Ideal: 128 | 128", 25, lCenterX - 130, lCenterY - 180, 0xFFFFFFFF);
    drawCircle(lCenterX, lCenterY, radius, 0xFF444444, false);
    drawCircle(lCenterX, lCenterY, 4, 0xFFFFFFFF, true);

    for (int tx = 0; tx < 256; tx++) {
        for (int ty = 0; ty < 256; ty++) {
            if (trilhaL[tx][ty]) {
                int px = lCenterX + ((tx - 128) * radius) / 128; int py = lCenterY + ((ty - 128) * radius) / 128;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int fpx = px + dx, fpy = py + dy;
                        if (fpx >= 0 && fpx < 1920 && fpy >= 0 && fpy < 1080) p[fpy * 1920 + fpx] = 0xFF00AAFF;
                    }
                }
            }
        }
    }

    int lxOff = ((lx - 128) * radius) / 128; int lyOff = ((ly - 128) * radius) / 128;
    drawCircle(lCenterX + lxOff, lCenterY + lyOff, 20, 0xFF00FF00, true);

    char txt[256]; sprintf(txt, "L3 X: %d | Y: %d", lx, ly);
    desenharTexto(p, txt, 30, lCenterX - 110, lCenterY + 180, 0xFFFFFFFF);

    // Analógico R3
    if (rx >= 115 && rx <= 140 && ry >= 115 && ry <= 140) { memset(trilhaR, 0, sizeof(trilhaR)); prevRx = 128; prevRy = 128; }
    else { drawLineOnGrid(trilhaR, prevRx, prevRy, rx, ry); prevRx = rx; prevRy = ry; }

    int rCenterX = 560; int rCenterY = 600;
    desenharTexto(p, "Centro Ideal: 128 | 128", 25, rCenterX - 130, rCenterY - 180, 0xFFFFFFFF);
    drawCircle(rCenterX, rCenterY, radius, 0xFF444444, false);
    drawCircle(rCenterX, rCenterY, 4, 0xFFFFFFFF, true);

    for (int tx = 0; tx < 256; tx++) {
        for (int ty = 0; ty < 256; ty++) {
            if (trilhaR[tx][ty]) {
                int px = rCenterX + ((tx - 128) * radius) / 128; int py = rCenterY + ((ty - 128) * radius) / 128;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int fpx = px + dx, fpy = py + dy;
                        if (fpx >= 0 && fpx < 1920 && fpy >= 0 && fpy < 1080) p[fpy * 1920 + fpx] = 0xFF00AAFF;
                    }
                }
            }
        }
    }

    int rxOff = ((rx - 128) * radius) / 128; int ryOff = ((ry - 128) * radius) / 128;
    drawCircle(rCenterX + rxOff, rCenterY + ryOff, 20, 0xFF00FF00, true);

    sprintf(txt, "R3 X: %d | Y: %d", rx, ry);
    desenharTexto(p, txt, 30, rCenterX - 110, rCenterY + 180, 0xFFFFFFFF);

    // -----------------------------------------
    // Renderizar Sensores (Giroscópio / Acelerômetro)
    // -----------------------------------------
    int sX = 1350; int sY = 460;
    desenharTexto(p, "SENSORES DE MOVIMENTO", 30, sX, sY - 40, 0xFF00AAFF);

    auto drawSensorBar = [&](int x, int y, const char* label, int16_t value, uint32_t color) {
        desenharTexto(p, label, 25, x, y, 0xFFFFFFFF);
        int barWidth = 200; int barHeight = 20;
        int fill = (value * barWidth) / 32768;
        if (fill > barWidth) fill = barWidth; if (fill < -barWidth) fill = -barWidth;

        for (int h = 0; h < barHeight; h++) {
            for (int w = 0; w < barWidth * 2; w++) {
                p[(y + 5 + h) * 1920 + (x + 100 + w)] = 0xFF333333;
            }
            if (fill >= 0) {
                for (int w = 0; w < fill; w++) p[(y + 5 + h) * 1920 + (x + 100 + barWidth + w)] = color;
            } else {
                for (int w = 0; w < -fill; w++) p[(y + 5 + h) * 1920 + (x + 100 + barWidth - w)] = color;
            }
        }
        char valStr[32]; sprintf(valStr, "%d", value);
        desenharTexto(p, valStr, 22, x + 100 + barWidth * 2 + 10, y, 0xFFFFFFFF);
    };

    drawSensorBar(sX, sY, "GYRO X:", gx, 0xFF00AAFF);
    drawSensorBar(sX, sY + 40, "GYRO Y:", gy, 0xFF00AAFF);
    drawSensorBar(sX, sY + 80, "GYRO Z:", gz, 0xFF00AAFF);

    drawSensorBar(sX, sY + 140, "ACCEL X:", ax, 0xFF00FF00);
    drawSensorBar(sX, sY + 180, "ACCEL Y:", ay, 0xFF00FF00);
    drawSensorBar(sX, sY + 220, "ACCEL Z:", az, 0xFF00FF00);

    // -----------------------------------------
    // Renderizar Touchpad (Multi-Touch)
    // -----------------------------------------
    int tpW = 350; int tpH = 200;
    int tpX = 850; int tpY = 500;

    desenharTexto(p, "TOUCHPAD PS4", 25, tpX + 60, tpY - 30, 0xFF00AAFF);

    for (int y = 0; y < tpH; y++) { for (int x = 0; x < tpW; x++) { p[(tpY + y) * 1920 + (tpX + x)] = 0xFF222222; } }
    for (int y = 0; y < tpH; y++) { for (int x = 0; x < tpW; x++) { if (x == 0 || y == 0 || x == tpW - 1 || y == tpH - 1) p[(tpY + y) * 1920 + (tpX + x)] = 0xFFAAAAAA; } }

    // Limpa a tela do touch se pressionar o "Click" central
    if (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) {
        memset(trilhaTouch1, 0, sizeof(trilhaTouch1));
        memset(trilhaTouch2, 0, sizeof(trilhaTouch2));
        prevT1x = -1; prevT1y = -1;
        prevT2x = -1; prevT2y = -1;
    }

    if (touchNum > 0) {
        int tx = (t0_x * tpW) / 1919; int ty = (t0_y * tpH) / 941;
        if (prevT1x == -1) { prevT1x = tx; prevT1y = ty; }
        drawLineOnGrid(trilhaTouch1, prevT1x, prevT1y, tx, ty);
        prevT1x = tx; prevT1y = ty;
    } else { prevT1x = -1; prevT1y = -1; }

    if (touchNum > 1) {
        int tx2 = (t1_x * tpW) / 1919; int ty2 = (t1_y * tpH) / 941;
        if (prevT2x == -1) { prevT2x = tx2; prevT2y = ty2; }
        drawLineOnGrid(trilhaTouch2, prevT2x, prevT2y, tx2, ty2);
        prevT2x = tx2; prevT2y = ty2;
    } else { prevT2x = -1; prevT2y = -1; }

    // Renderiza as trilhas
    for (int ty = 0; ty < tpH; ty++) {
        for (int tx = 0; tx < tpW; tx++) {
            if (trilhaTouch1[tx][ty]) {
                int fpx = tpX + tx; int fpy = tpY + ty;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ppx = fpx+dx, ppy = fpy+dy;
                        if (ppx >= 0 && ppx < 1920 && ppy >= 0 && ppy < 1080) p[ppy * 1920 + ppx] = 0xFF00AAFF;
                    }
                }
            }
            if (trilhaTouch2[tx][ty]) {
                int fpx = tpX + tx; int fpy = tpY + ty;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ppx = fpx+dx, ppy = fpy+dy;
                        if (ppx >= 0 && ppx < 1920 && ppy >= 0 && ppy < 1080) p[ppy * 1920 + ppx] = 0xFF00FF00;
                    }
                }
            }
        }
    }

    if (touchNum > 0) {
        int dotX = tpX + (t0_x * tpW) / 1919; int dotY = tpY + (t0_y * tpH) / 941;
        drawCircle(dotX, dotY, 12, 0xFF00AAFF, true);
        sprintf(txt, "DEDO 1: [%d, %d]", t0_x, t0_y);
        desenharTexto(p, txt, 25, tpX - 80, tpY + tpH + 30, 0xFF00AAFF);
    }
    if (touchNum > 1) {
        int dotX2 = tpX + (t1_x * tpW) / 1919; int dotY2 = tpY + (t1_y * tpH) / 941;
        drawCircle(dotX2, dotY2, 12, 0xFF00FF00, true);
        sprintf(txt, "DEDO 2: [%d, %d]", t1_x, t1_y);
        desenharTexto(p, txt, 25, tpX + 210, tpY + tpH + 30, 0xFF00FF00); // Movido mais para a esquerda
    }

    // -----------------------------------------
    // Renderizar Lista de Cores (Dividida em 2 Colunas)
    // -----------------------------------------
    int cXLabel = sX - 40; int cYLabel = sY + 300;
    desenharTexto(p, "LISTA DE CORES (LED)", 30, cXLabel, cYLabel - 40, 0xFF00AAFF);
    
    for (int i = 0; i < 8; i++) {
        int col = (i < 4) ? 0 : 1;
        int row = (i < 4) ? i : i - 4;
        int curX = cXLabel + (col * 320);
        int curY = cYLabel + (row * 35);

        uint32_t c = (buttons & colors[i].btn) ? 0xFFFFFFFF : 0xFF555555;
        uint32_t previewCol = 0xFF000000 | (colors[i].r << 16) | (colors[i].g << 8) | colors[i].b;
        
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) p[(curY + y) * 1920 + (curX + x)] = previewCol;
        }
        desenharTexto(p, colors[i].name, 22, curX + 30, curY, c);
    }

    char briTxt[64]; sprintf(briTxt, "BRILHO LED: %.0f%%", intensity * 100.0f);
    desenharTexto(p, briTxt, 25, cXLabel, cYLabel + 160, 0xFFFFFFFF);

    // -----------------------------------------
    // Renderizar Microfone (AudioIn)
    // -----------------------------------------
    int mX = tpX; int mY = tpY + 300;
    desenharTexto(p, "MICROFONE (INPUT)", 30, mX, mY - 30, 0xFF00AAFF);
    
    // Tenta ler amostras do microfone para o HUD
    static int16_t micBuffer[512];
    if (padMicPort >= 0) {
        ((int(*)(int, void*))sceAudioInInput)(0, micBuffer); 
        int peak = 0;
        for(int i=0; i<512; i++) { if(abs(micBuffer[i]) > peak) peak = abs(micBuffer[i]); }
        micLevelDisplay = peak;
    }

    int mBarW = 350; int mBarH = 20;
    for (int y = 0; y < mBarH; y++) {
        for (int x = 0; x < mBarW; x++) {
            uint32_t c = (x < (micLevelDisplay * mBarW) / 32768) ? 0xFF00FF00 : 0xFF333333;
            p[(mY + y) * 1920 + (mX + x)] = c;
        }
    }
    desenharTexto(p, "Fale no controle/headset para testar", 20, mX, mY + 30, 0xFFAAAAAA);

    // ===================================
    // PARTE 3: RODAPÉ (GATILHOS L2 E R2 EMPILHADOS)
    // =======================================================
    int l2X = 150, l2Y = 850;
    desenharTexto(p, "L2:", 30, l2X - 100, l2Y, 0xFFFFFFFF);
    for (int i = 0; i < 255; i++) {
        uint32_t c = (i < l2) ? 0xFF00AAFF : 0xFF333333;
        for (int y = 0; y < 40; y++) p[(l2Y + y) * 1920 + (l2X + i)] = c;
    }
    sprintf(txt, "%d / 255", l2); desenharTexto(p, txt, 25, l2X + 270, l2Y + 5, 0xFFFFFFFF);

    int r2X = 150, r2Y = 920;
    desenharTexto(p, "R2:", 30, r2X - 100, r2Y, 0xFFFFFFFF);
    for (int i = 0; i < 255; i++) {
        uint32_t c = (i < r2) ? 0xFF00AAFF : 0xFF333333;
        for (int y = 0; y < 40; y++) p[(r2Y + y) * 1920 + (r2X + i)] = c;
    }
    sprintf(txt, "%d / 255", r2); desenharTexto(p, txt, 25, r2X + 270, r2Y + 5, 0xFFFFFFFF);
}

struct DlLinha { char origem[16]; char nome[160]; float progresso; char info[96]; };   // gerenciador de downloads (network.cpp)
int dlTelegram(DlLinha* l, int max) {
    int n = 0;
    pthread_mutex_lock(&tgTrava);
    for (int i = 0; i < tgTotalDownloads && n < max; i++) {
        const TgDownload* d = &tgDownloads[i];
        snprintf(l[n].origem, sizeof(l[n].origem), "Telegram");
        snprintf(l[n].nome, sizeof(l[n].nome), "%s", d->nome);
        l[n].progresso = d->total > 0 ? (float)d->baixado / (float)d->total : -1;
        if (d->estado == TG_D_BAIXANDO && !d->iniciado) snprintf(l[n].info, sizeof(l[n].info), "na fila");
        else if (d->estado == TG_D_BAIXANDO) tgInfoDownload(d, l[n].info, sizeof(l[n].info));
        else if (d->estado == TG_D_COPIANDO || d->estado == TG_D_COPIAR) snprintf(l[n].info, sizeof(l[n].info), "salvando");
        else if (d->estado == TG_D_ERRO) snprintf(l[n].info, sizeof(l[n].info), "erro");
        else snprintf(l[n].info, sizeof(l[n].info), "concluído");
        n++;
    }
    pthread_mutex_unlock(&tgTrava);
    return n;
}
