#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h> 

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include "ImeDialog.h"

// --- BIBLIOTECAS DE REDE (PARA PEGAR O IP LOCAL) ---
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "baixar.h"
#include "network.h"
#include "menu.h"
#include "graphics.h"
#include "stb_image.h"
#include "remoto_ime.h"   // teclado do PS4 recebe texto do celular

extern int barX, barY, barW, barH;
extern int barBg, barFill, barTextCol;
extern uint32_t getSysColor(int index);

extern uint32_t* obterBufferVideo();
extern void desenharInterface(uint32_t* p);
extern void submeterTela();

extern unsigned char* backImg;
extern int wB, hB;

char ipDoPS4[64] = "Calculando IP..."; // <-- NOVA VARIVEL QUE GUARDA O IP

Console listaConsoles[5] = {
    {"Sony - PlayStation", "Sony%20-%20PlayStation"},
    {"Sony - PlayStation Portable", "Sony%20-%20PlayStation%20Portable"},
    {"Nintendo - Super Nintendo Entertainment System", "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System"},
    {"Sega - Mega Drive - Genesis", "Sega%20-%20Mega%20Drive%20-%20Genesis"},
    {"Nintendo - Nintendo Entertainment System", "Nintendo%20-%20Nintendo%20Entertainment%20System"}
};

int consoleAtual = 0;
unsigned char* imgPreview = NULL;
int wP = 0, hP = 0, cP = 0;
char ultimoJogoCarregado[64] = "";
char caminhoXMLAtual[256] = "";
char linksAtuais[3000][1024];
int totalLinksAtuais = 0;

char currentDropboxPath[512] = "";
char currentUploadPath[512] = "";

bool emSubmenuLojas = false;
bool emSubmenuDropbox = false;
bool emSubmenuFTP = false;
bool emApolloSaves = false;
char currentApolloUrl[1024] = "";

uint32_t getSysColorBarra(int index) {
    uint32_t sysColors[] = {
        0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000,
        0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933,
        0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF
    };
    if (index < 0 || index > 14) return sysColors[0];
    return sysColors[index];
}

void atualizarBarra(float progresso) {
    atualizarBarra(progresso, 1, 1);
}

void atualizarBarra(float progresso, int arquivoAtual, int totalArquivos) {
    // A extração roda em outra thread e não pode desenhar: só atualiza os dados da barra.
    extern bool hnNaThreadExtracao();
    extern void hnProgressoExtracao(float progresso);
    if (hnNaThreadExtracao()) { hnProgressoExtracao(progresso); return; }
    uint32_t* p = obterBufferVideo();
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;
    if (backImg) desenharRedimensionado(p, backImg, wB, hB, 1920, 1080, 0, 0);
    desenharInterface(p);

    int bX = barX; int bY = barY; int bW = barW; int bH = barH;

    for (int y = bY; y < bY + bH; y++) {
        for (int x = bX; x < bX + bW; x++) p[y * 1920 + x] = getSysColorBarra(barBg);
    }

    int fill = (int)(bW * progresso);
    if (fill > bW) fill = bW;
    if (fill < 0) fill = 0;

    for (int y = bY; y < bY + bH; y++) {
        for (int x = bX; x < bX + fill; x++) p[y * 1920 + x] = getSysColorBarra(barFill);
    }

    int porcentagem = (int)(progresso * 100.0f);
    if (porcentagem > 100) porcentagem = 100;
    if (porcentagem < 0) porcentagem = 0;

    char textoLoad[128];
    snprintf(textoLoad, sizeof(textoLoad), "%d%%   -   %d / %d", porcentagem, arquivoAtual, totalArquivos);
    uint32_t corTextLoad = getSysColor(barTextCol);
    if (barTextCol <= 0 || barTextCol == 5 || barTextCol > 14) corTextLoad = 0xFFFFFFFF;
    int txtLoadX = bX + bW + 20;
    int larguraLoadTexto = (int)strlen(textoLoad) * 14;
    int txtLoadXMax = 1908 - larguraLoadTexto;
    if (txtLoadX > txtLoadXMax) txtLoadX = txtLoadXMax;
    if (txtLoadX < 10) txtLoadX = 10;
    desenharTexto(p, textoLoad, 25, txtLoadX + 2, bY, 0xFF000000);
    desenharTexto(p, textoLoad, 25, txtLoadX, bY - 2, corTextLoad);
    submeterTela();
}

void acaoRede(const char* jogo, bool buscarLista, bool salvarNoHD) {
    char url[512];
    if (buscarLista) sprintf(url, "https://thumbnails.libretro.com/%s/Named_Boxarts/", listaConsoles[consoleAtual].pathServidor);
    else {
        char nURL[256] = { 0 };
        for (int i = 0, j = 0; jogo[i]; i++) { if (jogo[i] == ' ') { strcat(nURL, "%20"); j += 3; } else { nURL[j++] = jogo[i]; } }
        sprintf(url, "https://thumbnails.libretro.com/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].pathServidor, nURL);
    }

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        char path[256];
        if (buscarLista) strcpy(path, "/data/HyperNeiva/remote_list.html");
        else if (salvarNoHD) {
            char sub[256]; sprintf(sub, "/data/HyperNeiva/baixado/%s", listaConsoles[consoleAtual].nome);
            sceKernelMkdir(sub, 0777); char box[256]; sprintf(box, "%s/Named_Boxarts", sub);
            sceKernelMkdir(box, 0777); sprintf(path, "%s/%s.png", box, jogo);
        }
        else strcpy(path, "/data/HyperNeiva/preview.png");

        FILE* f = fopen(path, "wb");
        if (f) {
            unsigned char buf[16384]; int n;
            float pFake = 0.1f;

            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
                pFake += 0.05f;
                if (pFake > 0.99f) pFake = 0.99f;
                atualizarBarra(pFake, 1, 1);
            }
            fclose(f);
            atualizarBarra(1.0f, 1, 1);

            if (buscarLista) {
                FILE* f2 = fopen("/data/HyperNeiva/remote_list.html", "rb");
                if (f2) {
                    fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                    char* h = (char*)malloc(sz + 1); fread(h, 1, sz, f2); h[sz] = '\0'; fclose(f2);
                    memset(nomes, 0, sizeof(nomes)); totalItens = 0; char* b = h;
                    while ((b = strstr(b, "href=\"")) && totalItens < 3000) {
                        b += 6; if (strstr(b, "Parent Directory") || b[0] == '?' || b[0] == '/') { b++; continue; }
                        char* e = strstr(b, ".png\"");
                        if (e) {
                            int l = (int)(e - b); if (l > 255) l = 255; strncpy(nomes[totalItens], b, l); nomes[totalItens][l] = '\0';
                            char* pN = nomes[totalItens], * qN = nomes[totalItens];
                            while (*pN) { if (*pN == '%' && *(pN + 1) == '2' && *(pN + 2) == '0') { *qN++ = ' '; pN += 3; } else { *qN++ = *pN++; } }
                            *qN = '\0'; totalItens++;
                        }
                        b = e + 5;
                    }
                    free(h);
                }
                menuAtual = SCRAPER_LIST; sel = 0; off = 0;
            }
            else {
                if (imgPreview) stbi_image_free(imgPreview);
                imgPreview = stbi_load(path, &wP, &hP, &cP, 4);
                strcpy(ultimoJogoCarregado, jogo);
            }
        }
    }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

// --- FUNO NINJA PARA PEGAR O IP LOCAL DO PS4 ---
void pegarIPLocalPS4() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);

    if (connect(sock, (const struct sockaddr*)&serv, sizeof(serv)) == 0) {
        struct sockaddr_in name;
        socklen_t namelen = sizeof(name);
        if (getsockname(sock, (struct sockaddr*)&name, &namelen) == 0) {
            const char* ipStr = inet_ntoa(name.sin_addr);
            if (ipStr) strcpy(ipDoPS4, ipStr);
        }
    }
    close(sock);
}

// ===== TORRENT =====
// A libtorrent fica em libhntorrent.a; o app só conhece esta interface em C.
extern "C" {
    typedef struct HnTorrentInfo {
        char nome[256];
        char erro[128];
        float progresso;
        long long baixado;
        long long total;
        int taxa;
        int fontes;
        int pausado;
        int concluido;
        int estado;
    } HnTorrentInfo;
    int hn_torrent_iniciar(const char* dados, const char* destino);
    int hn_torrent_adicionar(const char* origem, char* erroTexto, int tamanhoErro);
    void hn_torrent_atualizar(void);
    int hn_torrent_total(void);
    int hn_torrent_info(int indice, HnTorrentInfo* saida);
    void hn_torrent_alternar_pausa(int indice);
    void hn_torrent_remover(int indice, int apagarArquivos);
}

#define TORRENT_DADOS "/data/HyperNeiva/torrent"
#define TORRENT_DESTINO "/data/HyperNeiva/baixado/torrent"
#define TORRENT_ITENS_FIXOS 1
#define TORRENT_OP_PAUSAR 0
#define TORRENT_OP_REMOVER 1
#define TORRENT_OP_APAGAR 2
#define TORRENT_OP_CANCELAR 3

static bool torrentLigado = false;
static bool torrentEsperandoTeclado = false;
static wchar_t torrentTexto[2050];
static int torrentOpcaoItem = -1;

static bool torrentLigar() {
    if (torrentLigado) return true;
    if (hn_torrent_iniciar(TORRENT_DADOS, TORRENT_DESTINO) != 0) {
        sprintf(msgStatus, "ERRO AO LIGAR O TORRENT");
        msgTimer = 300;
        return false;
    }
    torrentLigado = true;
    return true;
}

static void torrentTextoItem(const HnTorrentInfo* t, char* saida, size_t tamanho) {
    int pct = (int)(t->progresso * 100.0f);
    if (t->erro[0]) snprintf(saida, tamanho, "ERRO  %.120s  %.100s", t->erro, t->nome);
    else if (t->concluido) snprintf(saida, tamanho, "CONCLUIDO  %.200s", t->nome);
    else if (t->pausado) snprintf(saida, tamanho, "PAUSADO %d%%  %.200s", pct, t->nome);
    else if (t->estado == 2) snprintf(saida, tamanho, "BUSCANDO DADOS  %d fontes  %.180s", t->fontes, t->nome);
    else if (t->estado == 1 || t->estado == 7) snprintf(saida, tamanho, "VERIFICANDO %d%%  %.190s", pct, t->nome);
    else snprintf(saida, tamanho, "%d%%  %.2f MB/s  %d fontes  %.170s", pct, t->taxa / 1048576.0, t->fontes, t->nome);
}

void preencherMenuTorrent() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "ADICIONAR LINK MAGNET");
    totalItens = TORRENT_ITENS_FIXOS;
    int total = hn_torrent_total();
    for (int i = 0; i < total && totalItens < 2900; i++) {
        HnTorrentInfo t;
        if (hn_torrent_info(i, &t) == 0) torrentTextoItem(&t, nomes[totalItens++], 256);
    }
    menuAtual = MENU_BAIXAR_TORRENT;
    if (sel >= totalItens) sel = totalItens - 1;
}

void abrirMenuTorrent() {
    if (!torrentLigar()) return;
    sel = 0;
    off = 0;
    preencherMenuTorrent();
}

// Arquivo .torrent aberto no Explorar, ou link magnet digitado.
void adicionarTorrent(const char* origem) {
    if (!torrentLigar()) return;
    char erro[120];
    extern void conquistaGanhar(const char* chave);
    if (hn_torrent_adicionar(origem, erro, sizeof(erro)) >= 0) { sprintf(msgStatus, "TORRENT ADICIONADO. VEJA EM BAIXAR > TORRENT"); conquistaGanhar("torrent_novo"); }
    else snprintf(msgStatus, sizeof(msgStatus), "TORRENT RECUSADO: %.90s", erro);
    msgTimer = 300;
}

void acaoCross_Torrent(int32_t uId) {
    if (sel != 0) { hn_torrent_alternar_pausa(sel - TORRENT_ITENS_FIXOS); return; }
    if (torrentEsperandoTeclado) return;
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(torrentTexto, 0, sizeof(torrentTexto));
    param.userId = uId;
    param.maxTextLength = 2000;
    param.inputTextBuffer = torrentTexto;
    param.title = L"Cole o link magnet";
    param.type = (OrbisImeType)0;
    if (sceImeDialogInit(&param, NULL) >= 0) torrentEsperandoTeclado = true;
}

void acaoTriangle_Torrent() {
    extern const char* listaOpcoes[150];
    extern int mapOpcoes[150], totalOpcoes, selOpcao;
    extern bool showOpcoes;
    if (showOpcoes) { showOpcoes = false; return; }
    if (sel < TORRENT_ITENS_FIXOS) return;
    torrentOpcaoItem = sel - TORRENT_ITENS_FIXOS;
    int n = 0;
    listaOpcoes[n] = "pausar / continuar"; mapOpcoes[n++] = TORRENT_OP_PAUSAR;
    listaOpcoes[n] = "remover da lista"; mapOpcoes[n++] = TORRENT_OP_REMOVER;
    listaOpcoes[n] = "remover e apagar arquivos"; mapOpcoes[n++] = TORRENT_OP_APAGAR;
    listaOpcoes[n] = "cancelar"; mapOpcoes[n++] = TORRENT_OP_CANCELAR;
    for (int i = n; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
    totalOpcoes = n;
    selOpcao = 0;
    showOpcoes = true;
}

void acaoOpcaoTorrent(int indice) {
    extern int mapOpcoes[150];
    extern bool showOpcoes;
    int op = mapOpcoes[indice];
    showOpcoes = false;
    if (op == TORRENT_OP_PAUSAR) hn_torrent_alternar_pausa(torrentOpcaoItem);
    else if (op == TORRENT_OP_REMOVER || op == TORRENT_OP_APAGAR) {
        hn_torrent_remover(torrentOpcaoItem, op == TORRENT_OP_APAGAR);
        preencherMenuTorrent();
    }
}

// Chamada a cada quadro dos menus.
void atualizarTorrent() {
    if (torrentEsperandoTeclado) {
        int status = (int)sceImeDialogGetStatus();
        if (status != 0 && status != 1) {
            OrbisDialogResult resultado;
            memset(&resultado, 0, sizeof(resultado));
            sceImeDialogGetResult(&resultado);
            int32_t botao = *(int32_t*)&resultado;
            static char texto[2050];
            int n = 0;
            uint16_t* lido = (uint16_t*)torrentTexto;
            for (; n < 2049 && lido[n]; n++) texto[n] = (char)lido[n];
            texto[n] = '\0';
            sceImeDialogTerm();
            torrentEsperandoTeclado = false;
            if (botao == 0 && n > 0) {
                adicionarTorrent(texto);
                if (menuAtual == MENU_BAIXAR_TORRENT) preencherMenuTorrent();
            }
        }
    }
    if (!torrentLigado) return;
    static int quadros = 0;
    if (++quadros < 60) return;
    quadros = 0;
    hn_torrent_atualizar();
    if (menuAtual == MENU_BAIXAR_TORRENT) preencherMenuTorrent();
}

// ===== PACOTES DO RETROARCH =====
// Os mesmos pacotes do "Atualizador online" do RetroArch, extraídos nas pastas do retroarch.cfg do PS4.
#include <pthread.h>
#include <ctype.h>
#include <sys/stat.h>
#define RA_SERVIDOR "https://buildbot.libretro.com/assets/"
#define RA_DOWNLOADS "/data/retroarch/downloads"

struct PacoteRetroArch { const char* nome; const char* arquivo; const char* destino; const char* tamanho; };
static const PacoteRetroArch pacotesRetroArch[] = {
    { "Trapaças de todos os sistemas", "frontend/cheats.zip", "/data/retroarch/cheats", "35 MB" },
    { "Banco de jogos (database)", "frontend/database-rdb.zip", "/data/retroarch/database/rdb", "50 MB" },
    { "Cursores do banco", "frontend/database-cursors.zip", "/data/retroarch/database/cursors", "2 KB" },
    { "Assets do menu (ícones e temas)", "frontend/assets.zip", "/data/retroarch/assets", "72 MB" },
    { "Assets mínimos (GLUI)", "frontend/glui_minimal_assets.zip", "/data/retroarch/assets", "pequeno" },
    { "Overlays e molduras", "frontend/overlays.zip", "/data/retroarch/overlays", "26 MB" },
    { "Informação dos núcleos", "frontend/info.zip", "/data/retroarch/info", "300 KB" },
    { "Configuração de controles", "frontend/autoconfig.zip", "/data/retroarch/autoconfig", "700 KB" },
    { "Shaders GLSL", "frontend/shaders_glsl.zip", "/data/retroarch/shaders/shaders_glsl", "26 MB" },
    { "Shaders Slang", "frontend/shaders_slang.zip", "/data/retroarch/shaders/shaders_slang", "grande" },
    { "Shaders Cg", "frontend/shaders_cg.zip", "/data/retroarch/shaders/shaders_cg", "grande" },
};
#define TOTAL_PACOTES_RA ((int)(sizeof(pacotesRetroArch) / sizeof(pacotesRetroArch[0])))
#define RA_ITEM_SISTEMA TOTAL_PACOTES_RA

// Arquivos de sistema dos núcleos (não são BIOS): lista lida do servidor.
static char raSistema[200][128];
static int raTotalSistema = 0;
static bool raEmSistema = false;
static volatile bool raBaixando = false;
static char raUrl[600], raZip[400], raDestino[300], raNome[160];

static bool raPastaTemArquivos(const char* pasta) {
    DIR* d = opendir(pasta);
    if (!d) return false;
    int n = 0;
    struct dirent* item;
    while ((item = readdir(d)) != NULL && n < 3) if (item->d_name[0] != '.') n++;
    closedir(d);
    return n > 0;
}

static void raCriarPastas(const char* caminho) {
    char parcial[400];
    for (size_t i = 1; caminho[i]; i++) {
        if (caminho[i] != '/') continue;
        snprintf(parcial, sizeof(parcial), "%.*s", (int)i, caminho);
        sceKernelMkdir(parcial, 0777);
    }
    sceKernelMkdir(caminho, 0777);
}

// Baixa mostrando a barra de download do app; progresso pelo tamanho que o servidor informa.
static bool raBaixar(const char* url, const char* destino, bool barra) {
    extern volatile bool downloadEmSegundoPlano;
    extern volatile float progressoAtualDownload;
    extern char msgDownloadBg[256];
    char temporario[500];
    snprintf(temporario, sizeof(temporario), "%s.tmp", destino);
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0) : -1;
    bool certo = false;
    if (req >= 0 && sceHttpSendRequest(req, NULL, 0) >= 0) {
        int32_t codigo = 0;
        sceHttpGetStatusCode(req, &codigo);
        int resultado = 0;
        size_t total = 0;
        sceHttpGetResponseContentLength(req, &resultado, &total);
        FILE* f = codigo == 200 ? fopen(temporario, "wb") : NULL;
        if (f) {
            unsigned char bloco[32768];
            int lidos;
            uint64_t feito = 0;
            if (barra) { snprintf(msgDownloadBg, sizeof(msgDownloadBg), "%.200s", raNome); progressoAtualDownload = 0.0f; downloadEmSegundoPlano = true; }
            while ((lidos = sceHttpReadData(req, bloco, sizeof(bloco))) > 0) {
                fwrite(bloco, 1, (size_t)lidos, f);
                feito += (uint64_t)lidos;
                if (barra && total > 0) progressoAtualDownload = (float)((double)feito / (double)total);
            }
            fclose(f);
            certo = lidos == 0 && feito > 0;
            if (barra) downloadEmSegundoPlano = false;
        }
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    if (certo) rename(temporario, destino);
    else unlink(temporario);
    return certo;
}

static void* raThread(void*) {
    raCriarPastas(RA_DOWNLOADS);
    raCriarPastas(raDestino);
    unlink(raZip);
    if (raBaixar(raUrl, raZip, true)) {
        extern void hnEnfileirarExtracao(const char* arquivo, const char* destino);
        hnEnfileirarExtracao(raZip, raDestino);
        snprintf(msgStatus, sizeof(msgStatus), "BAIXADO. EXTRAINDO EM %.80s", raDestino);
    }
    else snprintf(msgStatus, sizeof(msgStatus), "NAO FOI POSSIVEL BAIXAR %.80s", raNome);
    msgTimer = 400;
    raBaixando = false;
    return NULL;
}

static void raIniciar(const char* arquivo, const char* destino, const char* nome) {
    if (raBaixando) { sprintf(msgStatus, "AGUARDE O DOWNLOAD ATUAL TERMINAR"); msgTimer = 200; return; }
    char codificado[400];
    size_t n = 0;
    for (const unsigned char* p = (const unsigned char*)arquivo; *p && n + 4 < sizeof(codificado); p++) {
        if (isalnum(*p) || strchr("-_./~", *p)) codificado[n++] = (char)*p;
        else n += (size_t)snprintf(codificado + n, 4, "%%%02X", *p);
    }
    codificado[n] = '\0';
    snprintf(raUrl, sizeof(raUrl), "%s%s", RA_SERVIDOR, codificado);
    const char* barra = strrchr(arquivo, '/');
    snprintf(raZip, sizeof(raZip), "%s/%s", RA_DOWNLOADS, barra ? barra + 1 : arquivo);
    snprintf(raDestino, sizeof(raDestino), "%s", destino);
    snprintf(raNome, sizeof(raNome), "RetroArch: %.140s", nome);
    raBaixando = true;
    pthread_t id;
    pthread_attr_t atributos;
    pthread_attr_init(&atributos);
    pthread_attr_setstacksize(&atributos, 256 * 1024);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&id, &atributos, raThread, NULL) != 0) raBaixando = false;
    pthread_attr_destroy(&atributos);
    sprintf(msgStatus, "BAIXANDO %.100s...", nome);
    msgTimer = 200;
}

int raTotalPacotes() { return TOTAL_PACOTES_RA; }
const char* raNomePacote(int i) { return i >= 0 && i < TOTAL_PACOTES_RA ? pacotesRetroArch[i].nome : ""; }
const char* raTamanhoPacote(int i) { return i >= 0 && i < TOTAL_PACOTES_RA ? pacotesRetroArch[i].tamanho : ""; }
bool raPacoteInstalado(int i) { return i >= 0 && i < TOTAL_PACOTES_RA && raPastaTemArquivos(pacotesRetroArch[i].destino); }
// Pelo nome do arquivo, como "cheats.zip": a biblioteca pede pacotes sem saber a posição na lista.
void raBaixarPacote(const char* arquivo) {
    for (int i = 0; i < TOTAL_PACOTES_RA; i++) {
        if (strstr(pacotesRetroArch[i].arquivo, arquivo)) { raIniciar(pacotesRetroArch[i].arquivo, pacotesRetroArch[i].destino, pacotesRetroArch[i].nome); return; }
    }
}
void raBaixarPacoteNumero(int i) { if (i >= 0 && i < TOTAL_PACOTES_RA) raIniciar(pacotesRetroArch[i].arquivo, pacotesRetroArch[i].destino, pacotesRetroArch[i].nome); }

void preencherMenuRetroArch() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    if (!raEmSistema) {
        for (int i = 0; i < TOTAL_PACOTES_RA; i++) {
            bool tem = raPastaTemArquivos(pacotesRetroArch[i].destino);
            snprintf(nomes[totalItens++], 256, "%s  (%s)%s", pacotesRetroArch[i].nome, pacotesRetroArch[i].tamanho, tem ? "  [já tem]" : "");
        }
        snprintf(nomes[totalItens++], 256, "Arquivos de sistema dos núcleos  >");
    }
    else {
        for (int i = 0; i < raTotalSistema; i++) snprintf(nomes[totalItens++], 256, "%s", raSistema[i]);
        if (raTotalSistema == 0) snprintf(nomes[totalItens++], 256, "Nada encontrado no servidor");
    }
    menuAtual = MENU_BAIXAR_RETROARCH;
}

void abrirMenuRetroArch() {
    raEmSistema = false;
    sel = 0;
    off = 0;
    preencherMenuRetroArch();
}

// Lê a página do servidor com os .zip de arquivos de sistema.
static void raListarSistema() {
    raTotalSistema = 0;
    const char* lista = RA_DOWNLOADS "/lista_sistema.html";
    raCriarPastas(RA_DOWNLOADS);
    if (!raBaixar(RA_SERVIDOR "system/", lista, false)) return;
    FILE* f = fopen(lista, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long tamanho = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* dados = (char*)malloc((size_t)tamanho + 1);
    if (dados) dados[fread(dados, 1, (size_t)tamanho, f)] = '\0';
    fclose(f);
    for (char* p = dados; p && raTotalSistema < 200 && (p = strstr(p, "href=\"/assets/system/")) != NULL;) {
        p += 21;
        char* fim = strstr(p, ".zip\"");
        if (!fim) break;
        char nome[256];
        int n = (int)(fim - p);
        if (n > 120) n = 120;
        int k = 0;
        for (int i = 0; i < n && k < 120; i++) {
            unsigned int valor;
            if (p[i] == '%' && sscanf(p + i + 1, "%2x", &valor) == 1) { nome[k++] = (char)valor; i += 2; }
            else nome[k++] = p[i];
        }
        nome[k] = '\0';
        snprintf(raSistema[raTotalSistema++], 128, "%s", nome);
        p = fim + 5;
    }
    free(dados);
}

void acaoCross_RetroArch() {
    if (raEmSistema) {
        if (sel < 0 || sel >= raTotalSistema) return;
        char arquivo[200];
        snprintf(arquivo, sizeof(arquivo), "system/%s.zip", raSistema[sel]);
        raIniciar(arquivo, "/data/retroarch/system", raSistema[sel]);
        return;
    }
    if (sel == RA_ITEM_SISTEMA) {
        raListarSistema();
        raEmSistema = true;
        sel = 0;
        off = 0;
        preencherMenuRetroArch();
        return;
    }
    if (sel >= 0 && sel < TOTAL_PACOTES_RA) raIniciar(pacotesRetroArch[sel].arquivo, pacotesRetroArch[sel].destino, pacotesRetroArch[sel].nome);
}

// O Círculo dentro dos arquivos de sistema volta para a lista de pacotes.
bool acaoCircle_RetroArch() {
    if (!raEmSistema) return false;
    raEmSistema = false;
    sel = RA_ITEM_SISTEMA;
    off = 0;
    preencherMenuRetroArch();
    return true;
}

void preencherMenuBaixar() {
    pegarIPLocalPS4(); // <-- AQUI! ELE PEGA O IP EXATAMENTE QUANDO VOC CLICA NO MENU "BAIXAR"

    emSubmenuLojas = false;
    emSubmenuDropbox = false;
    emSubmenuFTP = false;
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Repositorios");
    strcpy(nomes[1], "LINK DIRETO");
    strcpy(nomes[2], "Dropbox");
    strcpy(nomes[3], "Lojas");
    strcpy(nomes[4], "FTP (PC / Android)");
    strcpy(nomes[5], "GitHub");
    strcpy(nomes[6], "Torrent");
    strcpy(nomes[7], "RetroArch (assets, trapaças, banco...)");
    strcpy(nomes[8], "Gerenciador de downloads");
    strcpy(nomes[9], "Google Drive");
    totalItens = 10;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

void preencherMenuFTP() {
    emSubmenuLojas = false;
    emSubmenuDropbox = false;
    emSubmenuFTP = true;
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Download (Do PC pro PS4)");
    strcpy(nomes[1], "Upload (Do PS4 pro PC)");
    totalItens = 2;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

void preencherMenuDropbox() {
    emSubmenuLojas = false;
    emSubmenuDropbox = true;
    emSubmenuFTP = false;
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Download");
    strcpy(nomes[1], "Upload (Do PS4 pro PC)");
    strcpy(nomes[2], "Menu Backup");
    totalItens = 3;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

void preencherMenuLojas() {
    emSubmenuLojas = true;
    emSubmenuDropbox = false;
    emSubmenuFTP = false;
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "HB Store");
    strcpy(nomes[1], "Apollo Saves");
    strcpy(nomes[2], "Retroarch");
    totalItens = 3;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

const char* pathFilaTxt = "/data/HyperNeiva/configuracao/fila_downloads.txt";
const char* pathTempTxt = "/data/HyperNeiva/configuracao/fila_temp.txt";

bool adicionarLinkFila(const char* link) {
    FILE* fIn = fopen(pathFilaTxt, "r");
    if (fIn) {
        char linha[1024];
        while (fgets(linha, sizeof(linha), fIn)) {
            linha[strcspn(linha, "\r\n")] = 0;
            if (strcmp(linha, link) == 0) { fclose(fIn); return false; }
        }
        fclose(fIn);
    }

    FILE* fOut = fopen(pathFilaTxt, "a");
    if (fOut) { fprintf(fOut, "%s\n", link); fclose(fOut); return true; }
    return false;
}

bool obterPrimeiroLinkFila(char* linkSaida) {
    FILE* f = fopen(pathFilaTxt, "r");
    if (!f) return false;
    if (fgets(linkSaida, 1024, f)) { linkSaida[strcspn(linkSaida, "\r\n")] = 0; fclose(f); return (strlen(linkSaida) > 0); }
    fclose(f); return false;
}

void removerPrimeiroLinkFila() {
    FILE* fIn = fopen(pathFilaTxt, "r"); if (!fIn) return;
    FILE* fOut = fopen(pathTempTxt, "w"); if (!fOut) { fclose(fIn); return; }
    char linha[1024]; bool primeiraLinha = true;
    while (fgets(linha, sizeof(linha), fIn)) {
        if (primeiraLinha) { primeiraLinha = false; continue; }
        fprintf(fOut, "%s", linha);
    }
    fclose(fIn); fclose(fOut);
    unlink(pathFilaTxt); rename(pathTempTxt, pathFilaTxt);
}

int contarLinksFila() {
    FILE* f = fopen(pathFilaTxt, "r"); if (!f) return 0;
    int contagem = 0; char linha[1024];
    while (fgets(linha, sizeof(linha), f)) { if (strlen(linha) > 2) contagem++; }
    fclose(f); return contagem;
}

void processarFilaDownloads() {
    char linkAtual[1024];
    while (obterPrimeiroLinkFila(linkAtual)) {
        int totalRestante = contarLinksFila();
        removerPrimeiroLinkFila();
    }
}
struct DlLinha { char origem[16]; char nome[160]; float progresso; char info[96]; };   // gerenciador de downloads (network.cpp)
int dlTorrents(DlLinha* l, int max) {
    if (!torrentLigado) return 0;
    int n = 0, total = hn_torrent_total();
    for (int i = 0; i < total && n < max; i++) {
        HnTorrentInfo t;
        if (hn_torrent_info(i, &t) != 0) continue;
        snprintf(l[n].origem, sizeof(l[n].origem), "Torrent");
        snprintf(l[n].nome, sizeof(l[n].nome), "%s", t.nome);
        l[n].progresso = t.progresso;
        if (t.concluido) snprintf(l[n].info, sizeof(l[n].info), "Concluído");
        else if (t.pausado) snprintf(l[n].info, sizeof(l[n].info), "Pausado  %.0f%%", t.progresso * 100);
        else snprintf(l[n].info, sizeof(l[n].info), "%.0f%%  %d KB/s  %d fontes", t.progresso * 100, t.taxa / 1024, t.fontes);
        n++;
    }
    return n;
}
