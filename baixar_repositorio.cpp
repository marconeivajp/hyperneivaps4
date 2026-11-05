#include "baixar_repositorio.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <orbis/libkernel.h> 
#include <orbis/Http.h>
#include "network.h"
#include "ImeDialog.h"
#include "CommonDialog.h"
#include <orbis/UserService.h>

extern char nomes[3000][256];
extern int totalItens;
extern MenuLevel menuAtual;
extern int sel;
extern int off;

extern char caminhoXMLAtual[256];
extern char linksAtuais[3000][1024];
extern int totalLinksAtuais;

extern void atualizarBarra(float progresso);
extern char msgStatus[128];
extern int msgTimer;
char pastaRepositorioAtual[128] = "";

void preencherMenuRepositorios() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Games");
    strcpy(nomes[1], "Imagens para perfil");
    strcpy(nomes[2], "XML Games");
    totalItens = 3;
    menuAtual = MENU_BAIXAR_REPOS;
    sel = 0;
    off = 0;
}

void extrairXMLRepositorioUnico(const char* pasta, const char* arquivoXML) {
    char dirConfig[512];
    sprintf(dirConfig, "/data/HyperNeiva/configuracao/repositorios/%s", pasta);

    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios", 0777);
    sceKernelMkdir(dirConfig, 0777);

    char pathConfig[512];
    sprintf(pathConfig, "%s/%s", dirConfig, arquivoXML);

    FILE* fp = fopen(pathConfig, "rb");
    if (!fp) {
        char pathApp0[512];
        sprintf(pathApp0, "/app0/assets/%s", arquivoXML);
        FILE* fApp0 = fopen(pathApp0, "rb");
        if (fApp0) {
            fseek(fApp0, 0, SEEK_END); long sz = ftell(fApp0); fseek(fApp0, 0, SEEK_SET);
            char* bTemp = (char*)malloc(sz);
            fread(bTemp, 1, sz, fApp0);
            fclose(fApp0);

            FILE* fSave = fopen(pathConfig, "wb");
            if (fSave) {
                fwrite(bTemp, 1, sz, fSave);
                fclose(fSave);
            }
            free(bTemp);
        }
    }
    else {
        fclose(fp);
    }
}

void listarXMLsRepositorio() {
    // ATENÇÃO: Só atualizamos a "pasta atual" se viermos do menu principal (onde sel = 0, 1 ou 2)!
    // Isso evita que o botão Bolinha (Voltar) use a variável errada se "sel" mudar nas outras telas!
    if (menuAtual == MENU_BAIXAR_REPOS) {
        if (sel == 0) strcpy(pastaRepositorioAtual, "games");
        else if (sel == 1) strcpy(pastaRepositorioAtual, "imagens para perfil");
        else if (sel == 2) strcpy(pastaRepositorioAtual, "xml games");
    }

    // Extrai os arquivos caso a pasta esteja vazia (Primeira vez que abre)
    if (strcmp(pastaRepositorioAtual, "games") == 0) {
        extrairXMLRepositorioUnico("games", "systemas+zipados.xml");
        extrairXMLRepositorioUnico("games", "Sega_Master_System.xml"); // EXTRAI O MASTER SYSTEM AQUI TAMBÉM!
        extrairXMLRepositorioUnico("games", "retrocast_brasil.xml");
    }
    else if (strcmp(pastaRepositorioAtual, "imagens para perfil") == 0) {
        extrairXMLRepositorioUnico("imagens para perfil", "xavatar.xml");
    }
    else if (strcmp(pastaRepositorioAtual, "xml games") == 0) {
        extrairXMLRepositorioUnico("xml games", "xml.xml");
    }

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    char dirPath[512];
    sprintf(dirPath, "/data/HyperNeiva/configuracao/repositorios/%s", pastaRepositorioAtual);

    DIR* d = opendir(dirPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".xml")) {
                strcpy(nomes[totalItens], dir->d_name);
                totalItens++;
            }
        }
        closedir(d);
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhum XML encontrado");
        totalItens = 1;
    }

    menuAtual = MENU_BAIXAR_GAMES_XMLS;
    sel = 0;
    off = 0;
}

void abrirXMLRepositorio(const char* xmlFile) {
    sprintf(caminhoXMLAtual, "/data/HyperNeiva/configuracao/repositorios/%s/%s", pastaRepositorioAtual, xmlFile);

    FILE* fp = fopen(caminhoXMLAtual, "rb");
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* b = (char*)malloc(sz + 1);
    fread(b, 1, sz, fp);
    b[sz] = '\0';
    fclose(fp);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    char* p = b;

    while (totalItens < 2000) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        p += 12;
        char* f = strchr(p, '\"');
        if (f) {
            int l = (int)(f - p);
            if (l > 255) l = 255;
            strncpy(nomes[totalItens], p, l);
            nomes[totalItens][l] = '\0';
            totalItens++;
            p = f;
        }
        else break;
    }
    free(b);
    menuAtual = MENU_BAIXAR_GAMES_LIST;
    sel = 0;
    off = 0;
}

// A Bolinha volta para a lista de jogos, mas "nomes" ainda guarda as opções do jogo.
void reabrirXMLRepositorio() {
    const char* barra = strrchr(caminhoXMLAtual, '/');
    if (!barra) return;
    char arquivo[256]; snprintf(arquivo, sizeof(arquivo), "%s", barra + 1);
    abrirXMLRepositorio(arquivo);
}

void mostrarLinksJogo(int gameIndex) {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    totalLinksAtuais = 0;

    FILE* fp = fopen(caminhoXMLAtual, "rb");
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* b = (char*)malloc(sz + 1);
    fread(b, 1, sz, fp);
    b[sz] = '\0';
    fclose(fp);

    char* p = b;
    int currentIdx = -1;

    while (true) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        currentIdx++;

        if (currentIdx == gameIndex) {
            char* gameBlockEnd = strstr(p, "</game>");
            char* lPtr = p;
            while (totalLinksAtuais < 10) {
                lPtr = strstr(lPtr, "<link>");
                if (!lPtr || (gameBlockEnd && lPtr > gameBlockEnd)) break;
                lPtr += 6;
                char* lEnd = strstr(lPtr, "</link>");
                if (lEnd) {
                    int len = (int)(lEnd - lPtr);
                    if (len > 1023) len = 1023;
                    strncpy(linksAtuais[totalLinksAtuais], lPtr, len);
                    linksAtuais[totalLinksAtuais][len] = '\0';
                    // No XML o "&" do link pode vir como "&amp;", e o servidor responde 400.
                    for (char* amp = strstr(linksAtuais[totalLinksAtuais], "&amp;"); amp; amp = strstr(amp + 1, "&amp;")) memmove(amp + 1, amp + 5, strlen(amp + 5) + 1);
                    sprintf(nomes[totalItens], "Opcao %d", totalLinksAtuais + 1);
                    totalLinksAtuais++;
                    totalItens++;
                    lPtr = lEnd;
                }
                else break;
            }
            break;
        }
        p += 12;
    }
    free(b);
    menuAtual = MENU_BAIXAR_LINKS;
    sel = 0;
    off = 0;
}

// ==========================================
// GITHUB: PESQUISA E DOWNLOAD DE RELEASES
// ==========================================
#define GITHUB_RESPOSTA_MAX (3 * 1024 * 1024)

static char* githubRespostaRepos = NULL;
static char* githubRespostaReleases = NULL;
static char githubRepoAtual[160] = "";

// Leitor de JSON mínimo. Respeita a estrutura porque "name" aparece em vários níveis da resposta.
static const char* jsonEspaco(const char* p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    return p;
}

static const char* jsonFimTexto(const char* p) {
    p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    return *p ? p + 1 : p;
}

static const char* jsonPular(const char* p) {
    p = jsonEspaco(p);
    if (*p == '"') return jsonFimTexto(p);
    if (*p == '{' || *p == '[') {
        int nivel = 0;
        while (*p) {
            if (*p == '"') { p = jsonFimTexto(p); continue; }
            if (*p == '{' || *p == '[') nivel++;
            else if (*p == '}' || *p == ']') { nivel--; if (nivel == 0) return p + 1; }
            p++;
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

static const char* jsonCampo(const char* objeto, const char* chave) {
    if (!objeto) return NULL;
    objeto = jsonEspaco(objeto);
    if (*objeto != '{') return NULL;
    const char* p = objeto + 1;
    size_t tamanho = strlen(chave);
    for (;;) {
        p = jsonEspaco(p);
        if (*p != '"') return NULL;
        const char* inicio = p + 1;
        p = jsonEspaco(jsonFimTexto(p));
        bool achou = strncmp(inicio, chave, tamanho) == 0 && inicio[tamanho] == '"';
        if (*p != ':') return NULL;
        p = jsonEspaco(p + 1);
        if (achou) return p;
        p = jsonEspaco(jsonPular(p));
        if (*p != ',') return NULL;
        p++;
    }
}

static const char* jsonPrimeiro(const char* lista) {
    if (!lista) return NULL;
    lista = jsonEspaco(lista);
    if (*lista != '[') return NULL;
    const char* p = jsonEspaco(lista + 1);
    return *p == ']' || !*p ? NULL : p;
}

static const char* jsonProximo(const char* item) {
    const char* p = jsonEspaco(jsonPular(item));
    if (*p != ',') return NULL;
    return jsonEspaco(p + 1);
}

// Copia só ASCII: a fonte do app não desenha acentos vindos em UTF-8.
static void jsonTexto(const char* valor, char* saida, int tamanho) {
    int n = 0;
    saida[0] = '\0';
    if (!valor || *valor != '"') return;
    const char* p = valor + 1;
    while (*p && *p != '"' && n < tamanho - 1) {
        char c = *p++;
        if (c == '\\' && *p) {
            char e = *p++;
            if (e == 'u') { for (int i = 0; i < 4 && *p; i++) p++; c = '?'; }
            else if (e == 'n' || e == 'r' || e == 't') c = ' ';
            else c = e;
        }
        if ((unsigned char)c >= 128) { while (((unsigned char)*p & 0xC0) == 0x80) p++; c = '?'; }
        saida[n++] = c;
    }
    saida[n] = '\0';
}

static long long jsonNumero(const char* valor) { return valor ? atoll(valor) : 0; }
static bool jsonVerdadeiro(const char* valor) { return valor && strncmp(valor, "true", 4) == 0; }

static char* githubBaixarJson(const char* url, int* status) {
    *status = -1;
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    sceHttpAddRequestHeader(req, "Accept", "application/vnd.github+json", 1);

    char* dados = NULL;
    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        int32_t codigo = 0;
        sceHttpGetStatusCode(req, &codigo);
        *status = codigo;
        size_t capacidade = 256 * 1024, usado = 0;
        dados = (char*)malloc(capacidade + 1);
        int n = 0;
        while (dados && (n = sceHttpReadData(req, dados + usado, (unsigned int)(capacidade - usado))) > 0) {
            usado += n;
            if (usado == capacidade) {
                if (capacidade >= GITHUB_RESPOSTA_MAX) break;
                capacidade *= 2;
                char* maior = (char*)realloc(dados, capacidade + 1);
                if (!maior) { free(dados); dados = NULL; break; }
                dados = maior;
            }
        }
        if (dados) dados[usado] = '\0';
    }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    return dados;
}

static void githubAvisarErro(int status) {
    if (status == 403 || status == 429) sprintf(msgStatus, "LIMITE DO GITHUB! TENTE MAIS TARDE");
    else if (status == 404) sprintf(msgStatus, "REPOSITORIO NAO ENCONTRADO");
    else if (status < 0) sprintf(msgStatus, "SEM CONEXAO COM O GITHUB");
    else sprintf(msgStatus, "ERRO %d NO GITHUB", status);
    msgTimer = 300;
}

static void limparListaGithub() {
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    totalItens = 0;
}

#define GITHUB_FAVORITOS "/data/HyperNeiva/configuracao/github_favoritos.txt"
#define GITHUB_FAVORITOS_TEMP "/data/HyperNeiva/configuracao/github_favoritos.tmp"

void githubAbrirReleases(const char* repo);
void abrirTecladoGithub(int32_t uId);
static void abrirTecladoNome(int32_t uId, int modo, const wchar_t* titulo, const char* inicial);
extern int numSlotsPadrao;
static int numSlotsGithub() { return numSlotsPadrao > 0 ? numSlotsPadrao : 8; }
#define GITHUB_REPO_FIXO "marconeivajp/hyperneivaps4"

// Aceita "dono/repositorio" ou qualquer link do GitHub e devolve "dono/repositorio".
static bool githubNormalizarRepo(const char* texto, char* repo, int tamanho) {
    while (*texto == ' ' || *texto == '\t') texto++;
    const char* link = strstr(texto, "github.com/");
    const char* inicio = link ? link + 11 : texto;
    if (!link && (strchr(texto, ' ') || !strchr(texto, '/'))) return false;

    int n = 0, barras = 0;
    for (; inicio[n] && n < tamanho - 1; n++) {
        char c = inicio[n];
        if (c == '/' && ++barras == 2) break;
        if (c == '?' || c == '#' || c == ' ' || c == '\r' || c == '\n') break;
        repo[n] = c;
    }
    repo[n] = '\0';
    return barras >= 1 && n > 2 && repo[n - 1] != '/' && repo[0] != '/';
}

// FAVORITOS-INICIO
// O txt guarda a árvore: "[Pasta/Subpasta]" abre uma pasta e as linhas seguintes são
// "dono/repo" ou "dono/repo | Nome". Linhas antes da primeira pasta ficam na raiz.
#define FAV_MAX_PASTAS 64
#define FAV_MAX_ITENS 400
#define FAV_TAM_PASTA 128

struct FavItem { char repo[160]; char nome[96]; char pasta[FAV_TAM_PASTA]; };
static char favPastas[FAV_MAX_PASTAS][FAV_TAM_PASTA];
static int favTotalPastas = 0;
static FavItem favItens[FAV_MAX_ITENS];
static int favTotalItens = 0;

struct FavPadrao { const char* pasta; const char* repo; const char* nome; };
static const FavPadrao favPadroes[] = {
    { "Emuladores", "iHaiDeeZ/M33", "PSP (M33)" },
    { "Emuladores", "iHaiDeeZ/Crema", "Wii U (Crema)" },
    { "Emuladores", "iHaiDeeZ/Resonance", "Dreamcast (Resonance)" },
    { "Emuladores", "iHaiDeeZ/DolphinPS4", "GameCube e Wii (Dolphin)" },
    { "Emuladores", "Phoenixx1202/PS4SX2", "PS2 (PS4SX2)" },
    { "Emuladores", "RoberixStudios/rpcs3-port-for-ps4", "PS3 (RPCS3)" },
    { "Emuladores", "alechurri/nx-on-orbis", "Switch (nx-on-orbis)" },
    { "Emuladores", "arturofuentes1988/EmuSuNi", "EmuSuNi" },
    { "Emuladores", "iHaiDeeZ/HENkaku-PS4", "PS Vita (HENkaku)" },
    { "Recomps/Nintendo", "iHaiDeeZ/Conker-PS4", "Conker's Bad Fur Day" },
    { "Recomps/Xbox", "neiove/Sonic-Unleashed-Recompiled-Ps4", "Sonic Unleashed (neiove)" },
    { "Ports", "alechurri/shipofharkinian-ps4", "Zelda: Ocarina of Time" },
    { "Ports", "alechurri/2ship2harkinian-ps4", "Zelda: Majora's Mask" },
    { "Ports", "iHaiDeeZ/G-Diffuser-PS4", "F-Zero X" },
    { "Ports", "diasurgical/DevilutionX", "Diablo" },
};

static bool favDentroDe(const char* caminho, const char* base) {
    size_t n = strlen(base);
    return strncmp(caminho, base, n) == 0 && (caminho[n] == '\0' || caminho[n] == '/');
}

static const char* favFolha(const char* caminho) {
    const char* barra = strrchr(caminho, '/');
    return barra ? barra + 1 : caminho;
}

static void favPai(const char* caminho, char* pai) {
    snprintf(pai, FAV_TAM_PASTA, "%s", caminho);
    char* barra = strrchr(pai, '/');
    if (barra) *barra = '\0'; else pai[0] = '\0';
}

static int favAcharPasta(const char* caminho) {
    for (int i = 0; i < favTotalPastas; i++) if (strcasecmp(favPastas[i], caminho) == 0) return i;
    return -1;
}

static int favAcharItem(const char* repo) {
    for (int i = 0; i < favTotalItens; i++) if (strcasecmp(favItens[i].repo, repo) == 0) return i;
    return -1;
}

// Registra a pasta e as pastas acima dela.
static bool favGarantirPasta(const char* caminho) {
    if (!caminho[0]) return true;
    char parcial[FAV_TAM_PASTA];
    for (int i = 0; ; i++) {
        if (i >= FAV_TAM_PASTA - 1) return false;
        if (caminho[i] != '/' && caminho[i] != '\0') continue;
        memcpy(parcial, caminho, i); parcial[i] = '\0';
        if (favAcharPasta(parcial) < 0) {
            if (favTotalPastas >= FAV_MAX_PASTAS) return false;
            strcpy(favPastas[favTotalPastas++], parcial);
        }
        if (!caminho[i]) return true;
    }
}

void favSalvar() {
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    FILE* f = fopen(GITHUB_FAVORITOS_TEMP, "w");
    if (!f) return;
    fprintf(f, "# Favoritos do GitHub. [Pasta/Subpasta] abre uma pasta. Cada linha: dono/repo ou dono/repo | Nome\n");
    for (int p = -1; p < favTotalPastas; p++) {
        const char* pasta = p < 0 ? "" : favPastas[p];
        if (p >= 0) fprintf(f, "\n[%s]\n", pasta);
        for (int i = 0; i < favTotalItens; i++) {
            if (strcmp(favItens[i].pasta, pasta) != 0) continue;
            if (favItens[i].nome[0]) fprintf(f, "%s | %s\n", favItens[i].repo, favItens[i].nome);
            else fprintf(f, "%s\n", favItens[i].repo);
        }
    }
    fclose(f);
    remove(GITHUB_FAVORITOS);
    rename(GITHUB_FAVORITOS_TEMP, GITHUB_FAVORITOS);
}

static void favAplicarPadroes() {
    for (unsigned int i = 0; i < sizeof(favPadroes) / sizeof(favPadroes[0]); i++) {
        const FavPadrao* d = &favPadroes[i];
        favGarantirPasta(d->pasta);
        int achado = favAcharItem(d->repo);
        if (achado < 0) {
            if (favTotalItens >= FAV_MAX_ITENS) continue;
            achado = favTotalItens++;
            snprintf(favItens[achado].repo, sizeof(favItens[achado].repo), "%s", d->repo);
            favItens[achado].nome[0] = '\0';
        }
        snprintf(favItens[achado].pasta, FAV_TAM_PASTA, "%s", d->pasta);
        if (!favItens[achado].nome[0]) snprintf(favItens[achado].nome, sizeof(favItens[achado].nome), "%s", d->nome);
    }
}

void favCarregar() {
    favTotalPastas = 0; favTotalItens = 0;
    FILE* f = fopen(GITHUB_FAVORITOS, "r");
    bool arvore = false;
    if (f) {
        char linha[512], pasta[FAV_TAM_PASTA] = "";
        while (fgets(linha, sizeof(linha), f)) {
            size_t n = strlen(linha);
            while (n > 0 && (linha[n - 1] == '\n' || linha[n - 1] == '\r' || linha[n - 1] == ' ')) linha[--n] = '\0';
            if (!n || linha[0] == '#') continue;
            if (linha[0] == '[' && linha[n - 1] == ']') {
                linha[n - 1] = '\0';
                snprintf(pasta, sizeof(pasta), "%s", linha + 1);
                favGarantirPasta(pasta);
                arvore = true;
                continue;
            }
            if (favTotalItens >= FAV_MAX_ITENS) continue;
            char* nome = strchr(linha, '|');
            if (nome) {
                char* fim = nome;
                while (fim > linha && fim[-1] == ' ') fim--;
                *fim = '\0';
                nome++;
                while (*nome == ' ') nome++;
            }
            char repo[160];
            if (!githubNormalizarRepo(linha, repo, sizeof(repo)) || favAcharItem(repo) >= 0) continue;
            FavItem* item = &favItens[favTotalItens++];
            snprintf(item->repo, sizeof(item->repo), "%s", repo);
            snprintf(item->nome, sizeof(item->nome), "%s", nome ? nome : "");
            snprintf(item->pasta, sizeof(item->pasta), "%s", pasta);
        }
        fclose(f);
    }
    // Sem nenhuma pasta é o primeiro uso ou o txt antigo, que era só uma lista.
    if (!arvore) { favAplicarPadroes(); favSalvar(); }
}

// Troca o começo do caminho de uma pasta e de tudo que está dentro dela.
static void favTrocarPrefixo(const char* antigo, const char* novo) {
    size_t n = strlen(antigo);
    char resultado[FAV_TAM_PASTA * 2];
    for (int i = 0; i < favTotalPastas; i++) {
        if (!favDentroDe(favPastas[i], antigo)) continue;
        snprintf(resultado, sizeof(resultado), "%s%s", novo, favPastas[i] + n);
        snprintf(favPastas[i], FAV_TAM_PASTA, "%s", resultado);
    }
    for (int i = 0; i < favTotalItens; i++) {
        if (!favDentroDe(favItens[i].pasta, antigo)) continue;
        snprintf(resultado, sizeof(resultado), "%s%s", novo, favItens[i].pasta + n);
        snprintf(favItens[i].pasta, FAV_TAM_PASTA, "%s", resultado);
    }
}

static void favMontarCaminho(const char* pai, const char* nome, char* caminho) {
    if (pai[0]) snprintf(caminho, FAV_TAM_PASTA, "%s/%s", pai, nome);
    else snprintf(caminho, FAV_TAM_PASTA, "%s", nome);
}

bool favCriarPasta(const char* pai, const char* nome) {
    char caminho[FAV_TAM_PASTA];
    favMontarCaminho(pai, nome, caminho);
    if (!nome[0] || favAcharPasta(caminho) >= 0 || !favGarantirPasta(caminho)) return false;
    favSalvar();
    return true;
}

bool favRenomearPasta(const char* caminho, const char* nome) {
    char pai[FAV_TAM_PASTA], novo[FAV_TAM_PASTA], antigo[FAV_TAM_PASTA];
    snprintf(antigo, sizeof(antigo), "%s", caminho);
    favPai(antigo, pai);
    favMontarCaminho(pai, nome, novo);
    if (!nome[0] || favAcharPasta(antigo) < 0 || favAcharPasta(novo) >= 0) return false;
    favTrocarPrefixo(antigo, novo);
    favSalvar();
    return true;
}

bool favMoverPasta(const char* caminho, const char* destino) {
    char novo[FAV_TAM_PASTA], antigo[FAV_TAM_PASTA];
    snprintf(antigo, sizeof(antigo), "%s", caminho);
    favMontarCaminho(destino, favFolha(antigo), novo);
    if (favAcharPasta(antigo) < 0 || favDentroDe(destino, antigo) || favAcharPasta(novo) >= 0) return false;
    favTrocarPrefixo(antigo, novo);
    favSalvar();
    return true;
}

int favContar(const char* caminho) {
    int total = 0;
    for (int i = 0; i < favTotalItens; i++) if (favDentroDe(favItens[i].pasta, caminho)) total++;
    return total;
}

void favExcluirPasta(const char* caminho) {
    char alvo[FAV_TAM_PASTA];
    snprintf(alvo, sizeof(alvo), "%s", caminho);
    int fica = 0;
    for (int i = 0; i < favTotalPastas; i++) if (!favDentroDe(favPastas[i], alvo)) { if (fica != i) strcpy(favPastas[fica], favPastas[i]); fica++; }
    favTotalPastas = fica;
    fica = 0;
    for (int i = 0; i < favTotalItens; i++) if (!favDentroDe(favItens[i].pasta, alvo)) { if (fica != i) favItens[fica] = favItens[i]; fica++; }
    favTotalItens = fica;
    favSalvar();
}

bool favAdicionar(const char* repo, const char* pasta) {
    if (favAcharItem(repo) >= 0 || favTotalItens >= FAV_MAX_ITENS) return false;
    FavItem* item = &favItens[favTotalItens++];
    snprintf(item->repo, sizeof(item->repo), "%s", repo);
    item->nome[0] = '\0';
    snprintf(item->pasta, sizeof(item->pasta), "%s", pasta);
    favSalvar();
    return true;
}

void favRemover(const char* repo) {
    int achado = favAcharItem(repo);
    if (achado < 0) return;
    for (int i = achado; i < favTotalItens - 1; i++) favItens[i] = favItens[i + 1];
    favTotalItens--;
    favSalvar();
}

void favMover(const char* repo, const char* pasta) {
    int achado = favAcharItem(repo);
    if (achado < 0) return;
    snprintf(favItens[achado].pasta, FAV_TAM_PASTA, "%s", pasta);
    favSalvar();
}

void favRenomear(const char* repo, const char* nome) {
    int achado = favAcharItem(repo);
    if (achado < 0) return;
    snprintf(favItens[achado].nome, sizeof(favItens[achado].nome), "%s", nome);
    favSalvar();
}
// FAVORITOS-FIM

static char githubPasta[FAV_TAM_PASTA] = "";

void preencherMenuGithub() {
    favCarregar();
    if (githubPasta[0] && favAcharPasta(githubPasta) < 0) githubPasta[0] = '\0';
    limparListaGithub();
    totalItens = 0;
    if (!githubPasta[0]) {
        strcpy(nomes[0], "Pesquisar no GitHub");
        strcpy(nomes[1], GITHUB_REPO_FIXO);
        strcpy(linksAtuais[1], GITHUB_REPO_FIXO);
        totalItens = 2;
    }
    for (int i = 0; i < favTotalPastas; i++) {
        char pai[FAV_TAM_PASTA];
        favPai(favPastas[i], pai);
        if (strcmp(pai, githubPasta) != 0) continue;
        snprintf(nomes[totalItens], 256, "[%s]", favFolha(favPastas[i]));
        snprintf(linksAtuais[totalItens], 1024, "@%s", favPastas[i]);
        totalItens++;
    }
    for (int i = 0; i < favTotalItens; i++) {
        if (strcmp(favItens[i].pasta, githubPasta) != 0 || strcasecmp(favItens[i].repo, GITHUB_REPO_FIXO) == 0) continue;
        snprintf(nomes[totalItens], 256, "%s", favItens[i].nome[0] ? favItens[i].nome : favItens[i].repo);
        snprintf(linksAtuais[totalItens], 1024, "%s", favItens[i].repo);
        totalItens++;
    }
    if (totalItens == 0) { strcpy(nomes[0], "Pasta vazia"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_GITHUB;
    sel = 0; off = 0;
}

void abrirMenuGithub() {
    githubPasta[0] = '\0';
    preencherMenuGithub();
}

static void githubMostrarSelecionando(const char* link, int selAntes, int offAntes) {
    preencherMenuGithub();
    sel = selAntes < totalItens ? selAntes : totalItens - 1;
    off = offAntes;
    if (link) for (int i = 0; i < totalItens; i++) if (strcmp(linksAtuais[i], link) == 0) { sel = i; break; }
}

// Sobe uma pasta. Na raiz devolve false, para a Bolinha sair do menu.
bool githubVoltarPasta() {
    if (!githubPasta[0]) return false;
    char link[FAV_TAM_PASTA + 1];
    snprintf(link, sizeof(link), "@%s", githubPasta);
    char pai[FAV_TAM_PASTA];
    favPai(githubPasta, pai);
    strcpy(githubPasta, pai);
    githubMostrarSelecionando(link, 0, 0);
    off = sel >= numSlotsGithub() ? sel - numSlotsGithub() + 1 : 0;
    return true;
}

void githubAbrirItem(int32_t uId) {
    const char* link = linksAtuais[sel];
    if (link[0] == '@') {
        snprintf(githubPasta, sizeof(githubPasta), "%s", link + 1);
        preencherMenuGithub();
    }
    else if (link[0]) githubAbrirReleases(link);
    else if (!githubPasta[0] && sel == 0) abrirTecladoGithub(uId);
}

// Opções do Triângulo. Os números ficam em mapOpcoes; destino de pasta usa 1000 + índice.
enum { OP_CANCELAR = 901, OP_NOVA_PASTA = 911, OP_RENOMEAR_PASTA = 912, OP_MOVER_PASTA = 913, OP_EXCLUIR_PASTA = 914, OP_CONFIRMAR_EXCLUSAO = 915,
       OP_ADICIONAR = 920, OP_RENOMEAR_FAVORITO = 921, OP_MOVER_FAVORITO = 922, OP_REMOVER_FAVORITO = 923,
       OP_DESTINO_NOVA_PASTA = 998, OP_DESTINO_RAIZ = 999, OP_DESTINO_PASTA = 1000 };
enum { TECLADO_PESQUISA, TECLADO_NOVA_PASTA, TECLADO_RENOMEAR_PASTA, TECLADO_RENOMEAR_FAVORITO };
enum { DESTINO_ADICIONAR, DESTINO_MOVER_FAVORITO, DESTINO_MOVER_PASTA };

static char githubRepoOpcao[160] = "";
static char githubPastaOpcao[FAV_TAM_PASTA] = "";
static int githubDestinoModo = DESTINO_ADICIONAR;
static bool githubAdicionarNaNovaPasta = false;
static char githubTextoExclusao[96];

static void githubOpcoes(int total) {
    extern const char* listaOpcoes[150];
    extern int mapOpcoes[150], totalOpcoes, selOpcao;
    extern bool showOpcoes;
    for (int i = total; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
    totalOpcoes = total; selOpcao = 0; showOpcoes = true;
}

static void githubAbrirDestinos(int modo) {
    extern const char* listaOpcoes[150];
    extern int mapOpcoes[150];
    githubDestinoModo = modo;
    int n = 0;
    listaOpcoes[n] = "favoritos (sem pasta)"; mapOpcoes[n++] = OP_DESTINO_RAIZ;
    for (int i = 0; i < favTotalPastas && n < 148; i++) {
        if (modo == DESTINO_MOVER_PASTA && favDentroDe(favPastas[i], githubPastaOpcao)) continue;
        listaOpcoes[n] = favPastas[i]; mapOpcoes[n++] = OP_DESTINO_PASTA + i;
    }
    if (modo != DESTINO_MOVER_PASTA) { listaOpcoes[n] = "+ nova pasta"; mapOpcoes[n++] = OP_DESTINO_NOVA_PASTA; }
    githubOpcoes(n);
}

void acaoTriangle_Github() {
    extern const char* listaOpcoes[150];
    extern int mapOpcoes[150];
    extern bool showOpcoes;

    if (showOpcoes) { showOpcoes = false; return; }
    favCarregar();

    int n = 0;
    const char* repo = githubRepoAtual;
    if (menuAtual == MENU_BAIXAR_GITHUB) {
        const char* link = linksAtuais[sel];
        if (link[0] == '@') {
            snprintf(githubPastaOpcao, sizeof(githubPastaOpcao), "%s", link + 1);
            listaOpcoes[n] = "nova subpasta"; mapOpcoes[n++] = OP_NOVA_PASTA;
            listaOpcoes[n] = "editar nome da pasta"; mapOpcoes[n++] = OP_RENOMEAR_PASTA;
            listaOpcoes[n] = "mover pasta"; mapOpcoes[n++] = OP_MOVER_PASTA;
            listaOpcoes[n] = "excluir pasta"; mapOpcoes[n++] = OP_EXCLUIR_PASTA;
            listaOpcoes[n] = "cancelar"; mapOpcoes[n++] = OP_CANCELAR;
            githubOpcoes(n);
            return;
        }
        snprintf(githubPastaOpcao, sizeof(githubPastaOpcao), "%s", githubPasta);
        if (!link[0] || strcasecmp(link, GITHUB_REPO_FIXO) == 0) {
            listaOpcoes[n] = "nova pasta aqui"; mapOpcoes[n++] = OP_NOVA_PASTA;
            listaOpcoes[n] = "cancelar"; mapOpcoes[n++] = OP_CANCELAR;
            githubOpcoes(n);
            return;
        }
        repo = link;
    }
    else if (menuAtual == MENU_BAIXAR_GITHUB_REPOS) repo = linksAtuais[sel];
    if (!repo[0]) return;
    if (strcasecmp(repo, GITHUB_REPO_FIXO) == 0) { sprintf(msgStatus, "HYPER NEIVA FICA SEMPRE NA LISTA"); msgTimer = 180; return; }
    snprintf(githubRepoOpcao, sizeof(githubRepoOpcao), "%s", repo);

    if (favAcharItem(repo) >= 0) {
        listaOpcoes[n] = "editar nome"; mapOpcoes[n++] = OP_RENOMEAR_FAVORITO;
        listaOpcoes[n] = "mover para pasta"; mapOpcoes[n++] = OP_MOVER_FAVORITO;
        listaOpcoes[n] = "remover dos favoritos"; mapOpcoes[n++] = OP_REMOVER_FAVORITO;
        if (menuAtual == MENU_BAIXAR_GITHUB) { listaOpcoes[n] = "nova pasta aqui"; mapOpcoes[n++] = OP_NOVA_PASTA; }
    }
    else { listaOpcoes[n] = "adicionar aos favoritos"; mapOpcoes[n++] = OP_ADICIONAR; }
    listaOpcoes[n] = "cancelar"; mapOpcoes[n++] = OP_CANCELAR;
    githubOpcoes(n);
}

static void githubAtualizarLista(const char* link) {
    if (menuAtual != MENU_BAIXAR_GITHUB) return;
    githubMostrarSelecionando(link, sel, off);
}

static void githubAplicarDestino(const char* destino) {
    if (githubDestinoModo == DESTINO_ADICIONAR) {
        if (favAdicionar(githubRepoOpcao, destino)) sprintf(msgStatus, "SALVO NOS FAVORITOS");
        else sprintf(msgStatus, "NAO FOI POSSIVEL SALVAR");
    }
    else if (githubDestinoModo == DESTINO_MOVER_FAVORITO) { favMover(githubRepoOpcao, destino); sprintf(msgStatus, "FAVORITO MOVIDO"); }
    else if (favMoverPasta(githubPastaOpcao, destino)) sprintf(msgStatus, "PASTA MOVIDA");
    else sprintf(msgStatus, "JA EXISTE UMA PASTA COM ESSE NOME LA");
    msgTimer = 180;
    githubAtualizarLista(NULL);
}

void acaoOpcaoGithub(int indice) {
    extern int mapOpcoes[150];
    extern const char* listaOpcoes[150];
    extern bool showOpcoes;
    int op = mapOpcoes[indice];
    showOpcoes = false;
    int32_t usuario = 0;
    sceUserServiceGetInitialUser(&usuario);

    if (op >= OP_DESTINO_PASTA) {
        char destino[FAV_TAM_PASTA];
        snprintf(destino, sizeof(destino), "%s", favPastas[op - OP_DESTINO_PASTA]);
        githubAplicarDestino(destino);
    }
    else if (op == OP_DESTINO_RAIZ) githubAplicarDestino("");
    else if (op == OP_DESTINO_NOVA_PASTA) {
        githubAdicionarNaNovaPasta = true;
        githubPastaOpcao[0] = '\0';
        abrirTecladoNome(usuario, TECLADO_NOVA_PASTA, L"Nome da nova pasta", "");
    }
    else if (op == OP_NOVA_PASTA) { githubAdicionarNaNovaPasta = false; abrirTecladoNome(usuario, TECLADO_NOVA_PASTA, L"Nome da nova pasta", ""); }
    else if (op == OP_RENOMEAR_PASTA) abrirTecladoNome(usuario, TECLADO_RENOMEAR_PASTA, L"Nome da pasta", favFolha(githubPastaOpcao));
    else if (op == OP_MOVER_PASTA) githubAbrirDestinos(DESTINO_MOVER_PASTA);
    else if (op == OP_EXCLUIR_PASTA) {
        int total = favContar(githubPastaOpcao);
        snprintf(githubTextoExclusao, sizeof(githubTextoExclusao), "excluir pasta e %d favorito%s", total, total == 1 ? "" : "s");
        listaOpcoes[0] = "cancelar"; mapOpcoes[0] = OP_CANCELAR;
        listaOpcoes[1] = githubTextoExclusao; mapOpcoes[1] = OP_CONFIRMAR_EXCLUSAO;
        githubOpcoes(2);
    }
    else if (op == OP_CONFIRMAR_EXCLUSAO) {
        favExcluirPasta(githubPastaOpcao);
        sprintf(msgStatus, "PASTA EXCLUIDA"); msgTimer = 180;
        githubAtualizarLista(NULL);
    }
    else if (op == OP_ADICIONAR) githubAbrirDestinos(DESTINO_ADICIONAR);
    else if (op == OP_MOVER_FAVORITO) githubAbrirDestinos(DESTINO_MOVER_FAVORITO);
    else if (op == OP_RENOMEAR_FAVORITO) {
        int achado = favAcharItem(githubRepoOpcao);
        abrirTecladoNome(usuario, TECLADO_RENOMEAR_FAVORITO, L"Nome do favorito", achado >= 0 ? favItens[achado].nome : "");
    }
    else if (op == OP_REMOVER_FAVORITO) {
        favRemover(githubRepoOpcao);
        sprintf(msgStatus, "REMOVIDO DOS FAVORITOS"); msgTimer = 180;
        githubAtualizarLista(NULL);
    }
}

char githubNomeDownload[128] = "";

// O item da lista é "12.3 MB - nome.pkg": o nome do arquivo é o que vem depois do traço.
void githubBaixarArquivo(int indice) {
    if (!linksAtuais[indice][0]) return;
    const char* traco = strstr(nomes[indice], " - ");
    snprintf(githubNomeDownload, sizeof(githubNomeDownload), "%s", traco ? traco + 3 : nomes[indice]);
    extern void iniciarDownload(const char* url);
    iniciarDownload(linksAtuais[indice]);
}

void mostrarGithubRepos() {
    limparListaGithub();
    for (const char* item = jsonPrimeiro(jsonCampo(githubRespostaRepos, "items")); item && totalItens < 100; item = jsonProximo(item)) {
        jsonTexto(jsonCampo(item, "full_name"), linksAtuais[totalItens], 1024);
        if (!linksAtuais[totalItens][0]) continue;
        snprintf(nomes[totalItens], 256, "%s  (%lld estrelas)", linksAtuais[totalItens], jsonNumero(jsonCampo(item, "stargazers_count")));
        totalItens++;
    }
    if (totalItens == 0) { strcpy(nomes[0], "Nenhum repositorio encontrado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_GITHUB_REPOS;
    sel = 0; off = 0;
}

void mostrarGithubReleases() {
    limparListaGithub();
    for (const char* item = jsonPrimeiro(githubRespostaReleases); item && totalItens < 100; item = jsonProximo(item)) {
        char tag[96], titulo[120];
        jsonTexto(jsonCampo(item, "tag_name"), tag, sizeof(tag));
        jsonTexto(jsonCampo(item, "name"), titulo, sizeof(titulo));
        int arquivos = 0;
        for (const char* a = jsonPrimeiro(jsonCampo(item, "assets")); a; a = jsonProximo(a)) arquivos++;
        snprintf(nomes[totalItens], 256, "%s%s - %d arquivo%s%s%s", tag, jsonVerdadeiro(jsonCampo(item, "prerelease")) ? " (teste)" : "",
            arquivos, arquivos == 1 ? "" : "s", titulo[0] && strcmp(titulo, tag) != 0 ? " - " : "", strcmp(titulo, tag) != 0 ? titulo : "");
        totalItens++;
    }
    if (totalItens == 0) { strcpy(nomes[0], "Nenhuma release neste repositorio"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_GITHUB_RELEASES;
    sel = 0; off = 0;
}

void githubAbrirReleases(const char* repo) {
    char copia[160];
    snprintf(copia, sizeof(copia), "%s", repo);
    char url[320];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases?per_page=30", copia);
    sprintf(msgStatus, "BUSCANDO RELEASES..."); msgTimer = 120;
    atualizarBarra(0.3f);

    int status = 0;
    char* resposta = githubBaixarJson(url, &status);
    if (status != 200 || !resposta) { free(resposta); githubAvisarErro(status); return; }
    free(githubRespostaReleases);
    githubRespostaReleases = resposta;
    strcpy(githubRepoAtual, copia);
    mostrarGithubReleases();
}

void githubAbrirRelease(int indice) {
    const char* item = jsonPrimeiro(githubRespostaReleases);
    for (int i = 0; item && i < indice; i++) item = jsonProximo(item);
    if (!item) return;

    limparListaGithub();
    for (const char* a = jsonPrimeiro(jsonCampo(item, "assets")); a && totalItens < 200; a = jsonProximo(a)) {
        char nome[180];
        jsonTexto(jsonCampo(a, "name"), nome, sizeof(nome));
        jsonTexto(jsonCampo(a, "url"), linksAtuais[totalItens], 1024);
        snprintf(nomes[totalItens], 256, "%.1f MB - %s", (double)jsonNumero(jsonCampo(a, "size")) / 1048576.0, nome);
        totalItens++;
    }
    if (totalItens == 0) { strcpy(nomes[0], "Esta release nao tem arquivos"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_GITHUB_ARQUIVOS;
    sel = 0; off = 0;
}

// Aceita palavras, "dono/repositorio" ou um link colado do GitHub.
static void githubPesquisar(const char* texto) {
    char repo[160];
    if (githubNormalizarRepo(texto, repo, sizeof(repo))) { githubAbrirReleases(repo); return; }

    char url[640]; int n = snprintf(url, sizeof(url), "https://api.github.com/search/repositories?per_page=30&q=");
    for (int i = 0; texto[i] && n < (int)sizeof(url) - 4; i++) {
        unsigned char c = (unsigned char)texto[i];
        if (c == ' ') url[n++] = '+';
        else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.') url[n++] = (char)c;
        else n += sprintf(url + n, "%%%02X", c);
    }
    url[n] = '\0';

    sprintf(msgStatus, "PESQUISANDO NO GITHUB..."); msgTimer = 120;
    atualizarBarra(0.3f);
    int status = 0;
    char* resposta = githubBaixarJson(url, &status);
    if (status != 200 || !resposta) { free(resposta); githubAvisarErro(status); return; }
    free(githubRespostaRepos);
    githubRespostaRepos = resposta;
    mostrarGithubRepos();
}

static volatile bool githubEsperandoTeclado = false;
static wchar_t githubTextoTeclado[256] = L"";

static int githubModoTeclado = TECLADO_PESQUISA;

// Nome de pasta não pode ter o que tem significado no txt.
static void githubLimparNome(char* nome) {
    int n = 0;
    for (int i = 0; nome[i]; i++) if (!strchr("/\\[]|@#\r\n", nome[i])) nome[n++] = nome[i];
    while (n > 0 && nome[n - 1] == ' ') n--;
    nome[n] = '\0';
    char* inicio = nome;
    while (*inicio == ' ') inicio++;
    if (inicio != nome) memmove(nome, inicio, strlen(inicio) + 1);
}

static void abrirTecladoNome(int32_t uId, int modo, const wchar_t* titulo, const char* inicial) {
    if (githubEsperandoTeclado) return;
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(githubTextoTeclado, 0, sizeof(githubTextoTeclado));
    for (int i = 0; inicial[i] && i < 90; i++) githubTextoTeclado[i] = (unsigned char)inicial[i];
    param.userId = uId;
    param.maxTextLength = 90;
    param.inputTextBuffer = githubTextoTeclado;
    param.title = titulo;
    param.type = (OrbisImeType)0;
    if (sceImeDialogInit(&param, NULL) >= 0) { githubEsperandoTeclado = true; githubModoTeclado = modo; }
}

static void githubAplicarNome(char* texto) {
    githubLimparNome(texto);
    favCarregar();
    if (githubModoTeclado == TECLADO_RENOMEAR_FAVORITO) {
        favRenomear(githubRepoOpcao, texto);
        sprintf(msgStatus, "NOME ALTERADO");
    }
    else if (githubModoTeclado == TECLADO_RENOMEAR_PASTA) {
        if (favRenomearPasta(githubPastaOpcao, texto)) sprintf(msgStatus, "PASTA RENOMEADA");
        else sprintf(msgStatus, "NOME INVALIDO OU JA USADO");
    }
    else if (!favCriarPasta(githubPastaOpcao, texto)) sprintf(msgStatus, "NOME INVALIDO OU JA USADO");
    else if (githubAdicionarNaNovaPasta) {
        char caminho[FAV_TAM_PASTA];
        favMontarCaminho(githubPastaOpcao, texto, caminho);
        favAdicionar(githubRepoOpcao, caminho);
        sprintf(msgStatus, "SALVO NOS FAVORITOS");
    }
    else sprintf(msgStatus, "PASTA CRIADA");
    githubAdicionarNaNovaPasta = false;
    msgTimer = 180;
    githubAtualizarLista(NULL);
}

void abrirTecladoGithub(int32_t uId) {
    if (githubEsperandoTeclado) return;
    githubModoTeclado = TECLADO_PESQUISA;
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(githubTextoTeclado, 0, sizeof(githubTextoTeclado));
    param.userId = uId;
    param.maxTextLength = 200;
    param.inputTextBuffer = githubTextoTeclado;
    param.title = L"Pesquisar no GitHub";
    param.type = (OrbisImeType)0;
    if (sceImeDialogInit(&param, NULL) >= 0) githubEsperandoTeclado = true;
}

void atualizarImeGithub() {
    if (!githubEsperandoTeclado) return;
    int status = (int)sceImeDialogGetStatus();
    if (status == 0 || status == 1) return;

    OrbisDialogResult resultado;
    memset(&resultado, 0, sizeof(resultado));
    sceImeDialogGetResult(&resultado);
    int32_t botao = *(int32_t*)&resultado;

    char texto[256]; int n = 0;
    uint16_t* lido = (uint16_t*)githubTextoTeclado;
    for (; n < 255 && lido[n]; n++) texto[n] = (char)lido[n];
    texto[n] = '\0';

    sceImeDialogTerm();
    githubEsperandoTeclado = false;

    if (botao == 0 && githubModoTeclado != TECLADO_PESQUISA) { githubAplicarNome(texto); return; }
    githubAdicionarNaNovaPasta = false;
    if (botao == 0 && n > 0) {
        extern void empilharMenu(MenuLevel menu, int selAntes, int offAntes);
        MenuLevel menuAntes = menuAtual; int selAntes = sel, offAntes = off;
        githubPesquisar(texto);
        if (menuAtual != menuAntes) empilharMenu(menuAntes, selAntes, offAntes);
    }
}
