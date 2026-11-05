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
                    strncpy(linksAtuais[totalLinksAtuais], lPtr, len > 1023 ? 1023 : len);
                    linksAtuais[totalLinksAtuais][len] = '\0';
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
#define GITHUB_FAVORITOS_MAX 200
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

static void githubGravarFavoritos(char lista[][160], int total) {
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    FILE* f = fopen(GITHUB_FAVORITOS_TEMP, "w");
    if (!f) return;
    for (int i = 0; i < total; i++) fprintf(f, "%s\n", lista[i]);
    fclose(f);
    remove(GITHUB_FAVORITOS);
    rename(GITHUB_FAVORITOS_TEMP, GITHUB_FAVORITOS);
}

static int githubLerFavoritos(char lista[][160]) {
    static const char* sugeridos[] = {
        "iHaiDeeZ/M33", "iHaiDeeZ/Crema", "iHaiDeeZ/Resonance", "iHaiDeeZ/DolphinPS4", "arturofuentes1988/EmuSuNi",
        "Phoenixx1202/PS4SX2", "RoberixStudios/rpcs3-port-for-ps4", "alechurri/nx-on-orbis"
    };
    int total = 0;
    FILE* f = fopen(GITHUB_FAVORITOS, "r");
    if (!f) {
        // Primeira vez: cria o arquivo. Depois disso ele manda, mesmo que fique vazio.
        for (unsigned int i = 0; i < sizeof(sugeridos) / sizeof(sugeridos[0]); i++) strcpy(lista[total++], sugeridos[i]);
        githubGravarFavoritos(lista, total);
        return total;
    }

    char linha[512];
    while (fgets(linha, sizeof(linha), f) && total < GITHUB_FAVORITOS_MAX) {
        if (linha[0] == '#') continue;
        char repo[160];
        if (!githubNormalizarRepo(linha, repo, sizeof(repo))) continue;
        bool repetido = false;
        for (int i = 0; i < total; i++) if (strcasecmp(lista[i], repo) == 0) repetido = true;
        if (!repetido) strcpy(lista[total++], repo);
    }
    fclose(f);
    return total;
}

void preencherMenuGithub() {
    static char favoritos[GITHUB_FAVORITOS_MAX][160];
    int total = githubLerFavoritos(favoritos);
    limparListaGithub();
    strcpy(nomes[0], "Pesquisar no GitHub");
    strcpy(nomes[1], GITHUB_REPO_FIXO);
    strcpy(linksAtuais[1], GITHUB_REPO_FIXO);
    totalItens = 2;
    for (int i = 0; i < total; i++) {
        if (strcasecmp(favoritos[i], GITHUB_REPO_FIXO) == 0) continue;
        strcpy(nomes[totalItens], favoritos[i]);
        strcpy(linksAtuais[totalItens], favoritos[i]);
        totalItens++;
    }
    menuAtual = MENU_BAIXAR_GITHUB;
    sel = 0; off = 0;
}

static void githubAlternarFavorito(const char* repo) {
    static char favoritos[GITHUB_FAVORITOS_MAX][160];
    if (!repo[0]) return;
    if (strcasecmp(repo, GITHUB_REPO_FIXO) == 0) { sprintf(msgStatus, "HYPER NEIVA FICA SEMPRE NA LISTA"); msgTimer = 180; return; }
    int total = githubLerFavoritos(favoritos), achado = -1;
    for (int i = 0; i < total; i++) if (strcasecmp(favoritos[i], repo) == 0) achado = i;

    if (achado >= 0) {
        for (int i = achado; i < total - 1; i++) strcpy(favoritos[i], favoritos[i + 1]);
        total--;
        sprintf(msgStatus, "REMOVIDO DOS FAVORITOS");
    }
    else if (total >= GITHUB_FAVORITOS_MAX) { sprintf(msgStatus, "FAVORITOS CHEIOS!"); msgTimer = 180; return; }
    else {
        snprintf(favoritos[total++], 160, "%s", repo);
        sprintf(msgStatus, "SALVO NOS FAVORITOS");
    }
    msgTimer = 180;
    githubGravarFavoritos(favoritos, total);
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

static char githubRepoOpcao[160] = "";

// Triângulo abre o menu de opções do repositório selecionado, como no Explorar e no FTP.
void acaoTriangle_Github() {
    extern const char* listaOpcoes[150];
    extern int mapOpcoes[150], totalOpcoes, selOpcao;
    extern bool showOpcoes;

    if (showOpcoes) { showOpcoes = false; return; }

    const char* repo = githubRepoAtual;
    if (menuAtual == MENU_BAIXAR_GITHUB) { if (sel <= 0) return; repo = linksAtuais[sel]; }
    else if (menuAtual == MENU_BAIXAR_GITHUB_REPOS) repo = linksAtuais[sel];
    if (!repo[0]) return;
    if (strcasecmp(repo, GITHUB_REPO_FIXO) == 0) { sprintf(msgStatus, "HYPER NEIVA FICA SEMPRE NA LISTA"); msgTimer = 180; return; }
    snprintf(githubRepoOpcao, sizeof(githubRepoOpcao), "%s", repo);

    static char favoritos[GITHUB_FAVORITOS_MAX][160];
    int total = githubLerFavoritos(favoritos);
    bool salvo = false;
    for (int i = 0; i < total; i++) if (strcasecmp(favoritos[i], repo) == 0) salvo = true;

    listaOpcoes[0] = salvo ? "remover dos favoritos" : "adicionar aos favoritos"; mapOpcoes[0] = 900;
    listaOpcoes[1] = "cancelar"; mapOpcoes[1] = 901;
    totalOpcoes = 2; selOpcao = 0; showOpcoes = true;
}

void acaoOpcaoGithub(int indice) {
    extern bool showOpcoes;
    showOpcoes = false;
    if (indice != 0) return;

    githubAlternarFavorito(githubRepoOpcao);
    if (menuAtual == MENU_BAIXAR_GITHUB) {
        int selAntes = sel, offAntes = off;
        preencherMenuGithub();
        sel = selAntes < totalItens ? selAntes : totalItens - 1;
        off = offAntes;
    }
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

void abrirTecladoGithub(int32_t uId) {
    if (githubEsperandoTeclado) return;
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

    if (botao == 0 && n > 0) {
        extern void empilharMenu(MenuLevel menu, int selAntes, int offAntes);
        MenuLevel menuAntes = menuAtual; int selAntes = sel, offAntes = off;
        githubPesquisar(texto);
        if (menuAtual != menuAntes) empilharMenu(menuAntes, selAntes, offAntes);
    }
}
