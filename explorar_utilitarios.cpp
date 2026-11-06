#include "explorar_utilitarios.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include "miniz.h"
#include <pthread.h>
#include "menu.h"
#include <orbis/Bgft.h>
#include <strings.h>

extern void atualizarBarra(float progresso);
extern char msgStatus[128];
extern int msgTimer;

void copiarArquivoReal(const char* origem, const char* destino) {
    FILE* src = fopen(origem, "rb"); if (!src) return;
    FILE* dst = fopen(destino, "wb"); if (!dst) { fclose(src); return; }
    char buffer[65536]; size_t n; while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    fclose(src); fclose(dst);
}

void deletarPastaRecursivamente(const char* path) {
    DIR* d = opendir(path);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                char fullPath[1024]; snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);
                struct stat st;
                if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) deletarPastaRecursivamente(fullPath);
                else unlink(fullPath);
            }
        } closedir(d);
    } rmdir(path);
}

void criarCaminho(const char* filepath) {
    char tmp[1024]; strncpy(tmp, filepath, sizeof(tmp));
    for (char* p = strchr(tmp + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0'; sceKernelMkdir(tmp, 0777); *p = '/';
    }
}

static void hnAvisarCopia(float progresso);

// HN_PARTES_INICIO
#define HN_MAX_PARTES 256
#define HN_PASSO 1024
enum { HN_NORMAL, HN_RAR_VOLUMES, HN_JUNTAR, HN_ZIP_DIVIDIDO };

static bool hnExiste(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (f) fclose(f);
    return f != NULL;
}

static bool hnTermina(const char* texto, const char* fim) {
    size_t a = strlen(texto), b = strlen(fim);
    return a >= b && strcasecmp(texto + a - b, fim) == 0;
}

// Procura as partes de um arquivo. Devolve o tipo; faltando recebe o número da parte que não existe.
static int hnListarPartes(const char* caminho, char* lista, int* total, int* faltando) {
    const char* nome = strrchr(caminho, '/');
    nome = nome ? nome + 1 : caminho;
    size_t tamanho = strlen(caminho);
    *total = 0;
    *faltando = 0;
    char* item = lista;

    // nome.part01.rar
    if (hnTermina(caminho, ".rar")) {
        size_t fimDigitos = tamanho - 4, inicio = fimDigitos;
        while (inicio > 0 && caminho[inicio - 1] >= '0' && caminho[inicio - 1] <= '9') inicio--;
        if (inicio < fimDigitos && inicio >= 5 && strncasecmp(caminho + inicio - 5, ".part", 5) == 0) {
            int largura = (int)(fimDigitos - inicio);
            char prefixo[HN_PASSO];
            snprintf(prefixo, sizeof(prefixo), "%.*s", (int)inicio, caminho);
            int n = 1;
            while (n <= HN_MAX_PARTES) {
                snprintf(item, HN_PASSO, "%s%0*d%s", prefixo, largura, n, caminho + fimDigitos);
                if (!hnExiste(item)) break;
                item += HN_PASSO; (*total)++; n++;
            }
            // Se a próxima não existe mas uma depois existe, faltou uma do meio.
            char teste[HN_PASSO + 16];
            for (int k = n + 1; k <= n + 8; k++) {
                snprintf(teste, sizeof(teste), "%s%0*d%s", prefixo, largura, k, caminho + fimDigitos);
                if (hnExiste(teste)) { *faltando = n; break; }
            }
            if (*total == 0) *faltando = 1;
            return HN_RAR_VOLUMES;
        }
    }

    // nome.rar, nome.r00, nome.r01...
    const char* ponto = strrchr(nome, '.');
    bool extensaoR = ponto && strlen(ponto) == 4 && (ponto[1] == 'r' || ponto[1] == 'R' || ponto[1] == 's' || ponto[1] == 'S') && ponto[2] >= '0' && ponto[2] <= '9' && ponto[3] >= '0' && ponto[3] <= '9';
    if (hnTermina(caminho, ".rar") || extensaoR) {
        char base[HN_PASSO];
        snprintf(base, sizeof(base), "%.*s", (int)(tamanho - 4), caminho);
        char teste[HN_PASSO + 16];
        snprintf(teste, sizeof(teste), "%s.r00", base);
        bool minusculo = hnExiste(teste);
        snprintf(teste, sizeof(teste), "%s.R00", base);
        bool maiusculo = hnExiste(teste);
        if (minusculo || maiusculo || extensaoR) {
            snprintf(item, HN_PASSO, "%s.rar", base);
            if (!hnExiste(item)) snprintf(item, HN_PASSO, "%s.RAR", base);
            if (!hnExiste(item)) { *faltando = 1; return HN_RAR_VOLUMES; }
            item += HN_PASSO; (*total)++;
            for (int k = 0; k < HN_MAX_PARTES - 1; k++) {
                char letra = (char)((maiusculo ? 'R' : 'r') + k / 100);
                snprintf(item, HN_PASSO, "%s.%c%02d", base, letra, k % 100);
                if (!hnExiste(item)) {
                    snprintf(teste, sizeof(teste), "%s.%c%02d", base, letra, (k + 1) % 100);
                    if (hnExiste(teste)) *faltando = k + 2;
                    break;
                }
                item += HN_PASSO; (*total)++;
            }
            return HN_RAR_VOLUMES;
        }
    }

    // nome.7z.001, nome.zip.001, nome.pkg.001...
    if (ponto && strlen(ponto) == 4 && ponto[1] >= '0' && ponto[1] <= '9' && ponto[2] >= '0' && ponto[2] <= '9' && ponto[3] >= '0' && ponto[3] <= '9') {
        char base[HN_PASSO];
        snprintf(base, sizeof(base), "%.*s", (int)(tamanho - 4), caminho);
        int n = 1;
        while (n <= HN_MAX_PARTES) {
            snprintf(item, HN_PASSO, "%s.%03d", base, n);
            if (!hnExiste(item)) break;
            item += HN_PASSO; (*total)++; n++;
        }
        char teste[HN_PASSO + 16];
        for (int k = n + 1; k <= n + 8; k++) {
            snprintf(teste, sizeof(teste), "%s.%03d", base, k);
            if (hnExiste(teste)) { *faltando = n; break; }
        }
        if (*total == 0) *faltando = 1;
        return HN_JUNTAR;
    }

    // nome.z01 com nome.zip no fim: o ZIP dividido guarda posições por disco.
    if (hnTermina(caminho, ".zip") || (ponto && strlen(ponto) == 4 && (ponto[1] == 'z' || ponto[1] == 'Z') && ponto[2] >= '0' && ponto[2] <= '9')) {
        char teste[HN_PASSO + 16];
        snprintf(teste, sizeof(teste), "%.*s.z01", (int)(tamanho - 4), caminho);
        if (hnExiste(teste)) return HN_ZIP_DIVIDIDO;
    }
    return HN_NORMAL;
}

static long long hnTamanhoTotal(const char* caminho) {
    struct stat st;
    if (stat(caminho, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (long long)st.st_size;
    long long total = 0;
    DIR* d = opendir(caminho);
    if (!d) return 0;
    struct dirent* item;
    while ((item = readdir(d)) != NULL) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        char filho[1024];
        snprintf(filho, sizeof(filho), "%s/%s", caminho, item->d_name);
        total += hnTamanhoTotal(filho);
    }
    closedir(d);
    return total;
}

static long long hnCopiaFeita = 0, hnCopiaTotal = 0;

static bool hnCopiarRecursivo(const char* origem, const char* destino) {
    struct stat st;
    if (stat(origem, &st) != 0) return false;
    if (S_ISDIR(st.st_mode)) {
        sceKernelMkdir(destino, 0777);
        DIR* d = opendir(origem);
        if (!d) return false;
        bool certo = true;
        struct dirent* item;
        while (certo && (item = readdir(d)) != NULL) {
            if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
            char a[1024], b[1024];
            snprintf(a, sizeof(a), "%s/%s", origem, item->d_name);
            snprintf(b, sizeof(b), "%s/%s", destino, item->d_name);
            certo = hnCopiarRecursivo(a, b);
        }
        closedir(d);
        return certo;
    }
    FILE* f = fopen(origem, "rb");
    FILE* g = f ? fopen(destino, "wb") : NULL;
    bool certo = f && g;
    static char bloco[1 << 20];
    size_t n;
    while (certo && (n = fread(bloco, 1, sizeof(bloco), f)) > 0) {
        certo = fwrite(bloco, 1, n, g) == n;
        hnCopiaFeita += (long long)n;
        hnAvisarCopia(hnCopiaTotal > 0 ? (float)((double)hnCopiaFeita / (double)hnCopiaTotal) : 0.0f);
    }
    if (f) fclose(f);
    if (g) fclose(g);
    if (!certo) remove(destino);
    return certo;
}

// Recortar só apaga a origem depois de copiar tudo; na mesma partição apenas renomeia.
static bool hnCopiarOuMover(const char* origem, const char* destino, bool recortar) {
    size_t tamanho = strlen(origem);
    if (strcmp(origem, destino) == 0 || (strncmp(origem, destino, tamanho) == 0 && destino[tamanho] == '/')) return false;
    if (recortar && rename(origem, destino) == 0) return true;
    hnCopiaFeita = 0;
    hnCopiaTotal = hnTamanhoTotal(origem);
    if (!hnCopiarRecursivo(origem, destino)) return false;
    if (recortar) {
        struct stat st;
        if (stat(origem, &st) == 0 && S_ISDIR(st.st_mode)) deletarPastaRecursivamente(origem);
        else unlink(origem);
    }
    return true;
}

bool hnEhParteCompactada(const char* nomeMinusculo) {
    const char* ponto = strrchr(nomeMinusculo, '.');
    if (!ponto || strlen(ponto) != 4) return false;
    if (strcmp(ponto, ".001") == 0) return true;
    return ponto[1] == 'r' && ponto[2] >= '0' && ponto[2] <= '9' && ponto[3] >= '0' && ponto[3] <= '9';
}

static unsigned int hnLerBE(const unsigned char* p) { return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3]; }
static unsigned int hnLerLE(const unsigned char* p) { return ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0]; }

// Lê um texto do param.sfo que fica dentro do PKG.
static bool hnValorSfo(const char* caminho, const char* chaveBuscada, char* saida, int tamanho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return false;
    bool achou = false;
    unsigned char cabecalho[0x20];
    if (fread(cabecalho, 1, sizeof(cabecalho), f) == sizeof(cabecalho) && hnLerBE(cabecalho) == 0x7F434E54) {
        unsigned int entradas = hnLerBE(cabecalho + 0x10), tabela = hnLerBE(cabecalho + 0x18);
        fseeko(f, tabela, SEEK_SET);
        for (unsigned int i = 0; i < entradas && i < 4096; i++) {
            unsigned char e[32];
            if (fread(e, 1, 32, f) != 32) break;
            if (hnLerBE(e) != 0x1000) continue;
            unsigned int inicio = hnLerBE(e + 16), bytes = hnLerBE(e + 20);
            unsigned char* sfo = bytes > 0x14 && bytes < 1024 * 1024 ? (unsigned char*)malloc(bytes) : NULL;
            if (sfo && fseeko(f, inicio, SEEK_SET) == 0 && fread(sfo, 1, bytes, f) == bytes && hnLerBE(sfo) == 0x00505346) {
                unsigned int chaves = hnLerLE(sfo + 8), dados = hnLerLE(sfo + 12), total = hnLerLE(sfo + 16);
                for (unsigned int k = 0; k < total && 0x14 + (k + 1) * 16 <= bytes; k++) {
                    const unsigned char* r = sfo + 0x14 + k * 16;
                    unsigned int chave = chaves + (r[0] | (r[1] << 8)), valor = dados + hnLerLE(r + 12), usado = hnLerLE(r + 4);
                    if (chave + strlen(chaveBuscada) + 1 > bytes || valor + usado > bytes || strcmp((const char*)sfo + chave, chaveBuscada) != 0 || usado < 2) continue;
                    snprintf(saida, tamanho, "%.*s", (int)usado, (const char*)sfo + valor);
                    achou = true;
                    break;
                }
            }
            free(sfo);
            break;
        }
    }
    fclose(f);
    return achou;
}

// O nome do jogo vem do PKG; sem ele, usa o nome do arquivo.
void hnTituloPkg(const char* caminho, char* saida, int tamanho) {
    if (hnValorSfo(caminho, "TITLE", saida, tamanho)) return;
    const char* nome = strrchr(caminho, '/');
    nome = nome ? nome + 1 : caminho;
    snprintf(saida, tamanho, "%s", nome);
    char* ponto = strrchr(saida, '.');
    if (ponto && strcasecmp(ponto, ".pkg") == 0) *ponto = '\0';
}

// Atualização (categoria gp) precisa do jogo base instalado antes.
bool hnPkgEhAtualizacao(const char* caminho) {
    char categoria[8] = "";
    return hnValorSfo(caminho, "CATEGORY", categoria, sizeof(categoria)) && strncmp(categoria, "gp", 2) == 0;
}

// O cabeçalho guarda o tamanho total em 0x430. Devolve quantos bytes faltam; 0 está completo.
long long hnBytesFaltandoPkg(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return 0;
    unsigned char c[8];
    long long declarado = 0;
    fseeko(f, 0, SEEK_END);
    long long real = (long long)ftello(f);
    if (fseeko(f, 0x430, SEEK_SET) == 0 && fread(c, 1, 8, f) == 8) {
        declarado = ((long long)hnLerBE(c) << 32) | hnLerBE(c + 4);
    }
    fclose(f);
    return declarado > real ? declarado - real : 0;
}
// HN_PARTES_FIM

// ---- Fila de extração: roda fora da thread da tela e faz um arquivo por vez ----
extern "C" void* hnrar_abrir_lista(const char* lista, int total, int passo);
static void extrairRarAberto(void* h, const char* rarPath, const char* outPath);
#define HN_MAX_FILA 128
struct HnExtracao { int colar; bool recortar; char arquivo[1024], destino[1024]; };
static HnExtracao hnFila[HN_MAX_FILA];
static volatile int hnFilaTotal = 0;
static char hnPkgs[16][1024];
static volatile int hnPkgsTotal = 0;
static volatile bool hnListaMudou = false;
volatile bool hnBarraAtiva = false;
volatile float hnBarraProgresso = 0.0f;
volatile int hnBarraFeitos = 0, hnBarraTotal = 0;
char hnBarraTexto[256] = "";
static pthread_mutex_t hnTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_t hnThread;
static volatile bool hnThreadViva = false;

bool hnNaThreadExtracao() { return hnThreadViva && pthread_equal(pthread_self(), hnThread); }

// Chamada por atualizarBarra na thread de extração: alimenta a mesma barra dos downloads.
void hnProgressoExtracao(float progresso) {
    hnBarraProgresso = progresso;
    snprintf(hnBarraTexto, sizeof(hnBarraTexto), "%s", msgStatus);
    msgTimer = 0;
}

static void hnAvisarCopia(float progresso) {
    static int ultimo = -1;
    int pct = (int)(progresso * 100);
    if (pct == ultimo) return;
    ultimo = pct;
    snprintf(msgStatus, sizeof(msgStatus), "COPIANDO: %d%%", pct);
    hnProgressoExtracao(progresso);
}

static void hnAnotarPkg(const char* caminho) {
    if (!hnTermina(caminho, ".pkg")) return;
    pthread_mutex_lock(&hnTrava);
    if (hnPkgsTotal < 16) snprintf(hnPkgs[hnPkgsTotal++], sizeof(hnPkgs[0]), "%s", caminho);
    pthread_mutex_unlock(&hnTrava);
}

static bool hnJuntar(const char* lista, int total, const char* destino) {
    static char bloco[1 << 20];
    FILE* saida = fopen(destino, "wb");
    if (!saida) return false;
    bool certo = true;
    for (int i = 0; i < total && certo; i++) {
        FILE* f = fopen(lista + (size_t)i * HN_PASSO, "rb");
        if (!f) { certo = false; break; }
        size_t n;
        while (certo && (n = fread(bloco, 1, sizeof(bloco), f)) > 0) certo = fwrite(bloco, 1, n, saida) == n;
        fclose(f);
        snprintf(msgStatus, sizeof(msgStatus), "JUNTANDO PARTES: %d DE %d", i + 1, total);
        hnProgressoExtracao((float)(i + 1) / (float)total);
    }
    fclose(saida);
    if (!certo) remove(destino);
    return certo;
}

static void hnExtrairComPartes(const char* arquivo, const char* destino) {
    char* lista = (char*)malloc((size_t)HN_MAX_PARTES * HN_PASSO);
    if (!lista) { snprintf(msgStatus, sizeof(msgStatus), "SEM MEMORIA PARA LER AS PARTES"); msgTimer = 300; return; }
    int total = 0, faltando = 0;
    int tipo = hnListarPartes(arquivo, lista, &total, &faltando);
    if (faltando) { snprintf(msgStatus, sizeof(msgStatus), "FALTA A PARTE %d: BAIXE TODAS AS PARTES", faltando); msgTimer = 500; }
    else if (tipo == HN_ZIP_DIVIDIDO) { snprintf(msgStatus, sizeof(msgStatus), "ZIP DIVIDIDO (.Z01) NAO E SUPORTADO: JUNTE NO PC"); msgTimer = 500; }
    else if (tipo == HN_RAR_VOLUMES) {
        snprintf(msgStatus, sizeof(msgStatus), "LENDO RAR EM %d PARTES...", total);
        hnProgressoExtracao(0.0f);
        extrairRarAberto(hnrar_abrir_lista(lista, total, HN_PASSO), lista, destino);
    }
    else if (tipo == HN_JUNTAR) {
        char junto[1024];
        snprintf(junto, sizeof(junto), "%s", lista);
        junto[strlen(junto) - 4] = '\0';
        bool jaExistia = hnExiste(junto);
        if (jaExistia) snprintf(junto + strlen(junto), sizeof(junto) - strlen(junto), ".junto");
        if (!hnJuntar(lista, total, junto)) { snprintf(msgStatus, sizeof(msgStatus), "ERRO AO JUNTAR: VEJA O ESPACO LIVRE"); msgTimer = 500; }
        else if (hnTermina(junto, ".pkg")) hnAnotarPkg(junto);
        else {
            extrairZip(junto, destino);
            remove(junto);
        }
    }
    else extrairZip(arquivo, destino);
    free(lista);
}

static void* hnThreadFila(void*) {
    static HnExtracao atual;
    for (;;) {
        pthread_mutex_lock(&hnTrava);
        bool tem = hnFilaTotal > 0;
        if (tem) {
            atual = hnFila[0];
            memmove(&hnFila[0], &hnFila[1], sizeof(HnExtracao) * (hnFilaTotal - 1));
            hnFilaTotal--;
        }
        pthread_mutex_unlock(&hnTrava);
        if (!tem) {
            hnBarraAtiva = false;
            hnBarraFeitos = 0;
            hnBarraTotal = 0;
            sceKernelUsleep(100000);
            continue;
        }
        hnBarraProgresso = 0.0f;
        const char* nome = strrchr(atual.arquivo, '/');
        snprintf(hnBarraTexto, sizeof(hnBarraTexto), "%s %.200s", atual.colar ? "COPIANDO" : "EXTRAINDO", nome ? nome + 1 : atual.arquivo);
        hnBarraAtiva = true;
        if (atual.colar) {
            bool certo = hnCopiarOuMover(atual.arquivo, atual.destino, atual.recortar);
            snprintf(msgStatus, sizeof(msgStatus), certo ? "COLADO: %.100s" : "NAO FOI POSSIVEL COLAR: %.90s", nome ? nome + 1 : atual.arquivo);
            msgTimer = 240;
        }
        else hnExtrairComPartes(atual.arquivo, atual.destino);
        hnBarraFeitos++;
        hnListaMudou = true;
    }
    return NULL;
}

static void hnEnfileirar(int colar, bool recortar, const char* arquivo, const char* destino) {
    pthread_mutex_lock(&hnTrava);
    bool repetido = false;
    for (int i = 0; i < hnFilaTotal; i++) if (strcmp(hnFila[i].arquivo, arquivo) == 0 && strcmp(hnFila[i].destino, destino) == 0) repetido = true;
    if (!repetido && hnFilaTotal < HN_MAX_FILA) {
        hnFila[hnFilaTotal].colar = colar;
        hnFila[hnFilaTotal].recortar = recortar;
        snprintf(hnFila[hnFilaTotal].arquivo, sizeof(hnFila[0].arquivo), "%s", arquivo);
        snprintf(hnFila[hnFilaTotal].destino, sizeof(hnFila[0].destino), "%s", destino);
        hnFilaTotal++;
        hnBarraTotal++;
        snprintf(msgStatus, sizeof(msgStatus), "ADICIONADO A FILA: %d", hnFilaTotal);
    }
    else snprintf(msgStatus, sizeof(msgStatus), repetido ? "ESTE ARQUIVO JA ESTA NA FILA" : "FILA CHEIA");
    msgTimer = 180;
    pthread_mutex_unlock(&hnTrava);
    if (!hnThreadViva) {
        pthread_attr_t atributos;
        pthread_attr_init(&atributos);
        pthread_attr_setstacksize(&atributos, 4 * 1024 * 1024);
        if (pthread_create(&hnThread, &atributos, hnThreadFila, NULL) == 0) hnThreadViva = true;
        pthread_attr_destroy(&atributos);
    }
}

void hnEnfileirarExtracao(const char* arquivo, const char* destino) { hnEnfileirar(0, false, arquivo, destino); }
void hnEnfileirarColar(const char* origem, const char* destino, bool recortar) { hnEnfileirar(1, recortar, origem, destino); }

volatile int hnTarefaPkg = -1;

// O console só dá retorno da tarefa em andamento: sem dado confiável, segue a fila.
static bool hnInstalacaoEmAndamento() {
    static int esperas = 0;
    if (hnTarefaPkg < 0) return false;
    OrbisBgftTaskProgress progresso;
    memset(&progresso, 0, sizeof(progresso));
    int r = sceBgftServiceDownloadGetProgress(hnTarefaPkg, &progresso);
    bool ativa = r == 0 && progresso.errorResult == 0 && progresso.length > 0 && progresso.transferred < progresso.length;
    if (ativa && ++esperas < 1200) return true;
    FILE* f = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (f) { fprintf(f, "TAREFA %d fim r=0x%08X erro=0x%08X %u/%u esperas=%d\n", hnTarefaPkg, (unsigned int)r, (unsigned int)progresso.errorResult, progresso.transferred, progresso.length, esperas); fclose(f); }
    esperas = 0;
    hnTarefaPkg = -1;
    return false;
}

// Roda na thread principal: lista e instalador não podem ser usados de outra thread.
void hnAtualizarExtracao() {
    extern void instalarPkgLocal(const char* caminhoAbsoluto);
    extern void listarDiretorio(const char* caminho);
    extern void listarDiretorioEsq(const char* caminho);
    extern char pathExplorar[256], pathExplorarEsq[256];
    extern MenuLevel menuAtualEsq;
    extern bool painelDuplo;
    extern int sel, off, totalItens;
    if (hnListaMudou) {
        hnListaMudou = false;
        if (menuAtual == MENU_EXPLORAR) {
            int selAntes = sel, offAntes = off;
            listarDiretorio(pathExplorar);
            sel = selAntes < totalItens ? selAntes : 0;
            off = offAntes;
        }
        if (painelDuplo && menuAtualEsq == MENU_EXPLORAR) listarDiretorioEsq(pathExplorarEsq);
    }
    static char caminho[1024];
    // Um PKG por vez, depois de tudo extraído: o jogo base e a atualização saem do mesmo arquivo.
    static int esperaEntrePkgs = 0;
    if (esperaEntrePkgs > 0) { esperaEntrePkgs--; return; }
    if (hnBarraAtiva || hnFilaTotal > 0 || hnPkgsTotal == 0) return;
    if (hnInstalacaoEmAndamento()) {
        esperaEntrePkgs = 120;
        snprintf(msgStatus, sizeof(msgStatus), "INSTALANDO. PROXIMOS NA FILA: %d", hnPkgsTotal);
        msgTimer = 150;
        return;
    }
    // Começa pelo jogo base, que costuma ser o maior; atualizações vão depois, da maior à menor.
    pthread_mutex_lock(&hnTrava);
    int escolhido = 0;
    bool melhorEhAtualizacao = true;
    long long melhorTamanho = -1;
    for (int i = 0; i < hnPkgsTotal; i++) {
        bool atualizacao = hnPkgEhAtualizacao(hnPkgs[i]);
        long long tamanho = hnTamanhoTotal(hnPkgs[i]);
        if ((melhorEhAtualizacao && !atualizacao) || (atualizacao == melhorEhAtualizacao && tamanho > melhorTamanho)) {
            escolhido = i;
            melhorEhAtualizacao = atualizacao;
            melhorTamanho = tamanho;
        }
    }
    strcpy(caminho, hnPkgs[escolhido]);
    memmove(&hnPkgs[escolhido], &hnPkgs[escolhido + 1], sizeof(hnPkgs[0]) * (hnPkgsTotal - escolhido - 1));
    hnPkgsTotal--;
    pthread_mutex_unlock(&hnTrava);
    esperaEntrePkgs = 300;
    instalarPkgLocal(caminho);
}

struct HnZipSaida { FILE* f; unsigned long long feito, total; int indice, arquivos; };

static size_t hnEscreverZip(void* dados, mz_uint64 posicao, const void* bloco, size_t tamanho) {
    HnZipSaida* s = (HnZipSaida*)dados;
    (void)posicao;
    size_t n = fwrite(bloco, 1, tamanho, s->f);
    s->feito += n;
    static int contador = 0;
    if (s->total && ++contador % 256 == 0) {
        float prog = ((float)s->indice + (float)((double)s->feito / (double)s->total)) / (float)s->arquivos;
        snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO: %d%%", (int)(prog * 100));
        atualizarBarra(prog);
    }
    return n;
}

void extrairZip(const char* zipPath, const char* outPath) {
    mz_zip_archive zip_archive; memset(&zip_archive, 0, sizeof(zip_archive));
    snprintf(msgStatus, sizeof(msgStatus), "LENDO O ARQUIVO ZIP..."); atualizarBarra(0.01f);
    if (!mz_zip_reader_init_file(&zip_archive, zipPath, 0)) {
        int cod = (int)mz_zip_get_last_error(&zip_archive);
        unsigned char hdr[8] = { 0 }; size_t hn = 0; long fsz = -1;
        FILE* fh = fopen(zipPath, "rb");
        if (fh) { fseek(fh, 0, SEEK_END); fsz = ftell(fh); fseek(fh, 0, SEEK_SET); hn = fread(hdr, 1, sizeof(hdr), fh); fclose(fh); }
        if (fsz == 0) { snprintf(msgStatus, sizeof(msgStatus), "ARQUIVO VAZIO (0 bytes)"); }
        else if (hn >= 4 && hdr[0] == 'R' && hdr[1] == 'a' && hdr[2] == 'r' && hdr[3] == '!') { extrairRar(zipPath, outPath); return; }
        else if (hn >= 6 && hdr[0] == 0x37 && hdr[1] == 0x7A && hdr[2] == 0xBC && hdr[3] == 0xAF && hdr[4] == 0x27 && hdr[5] == 0x1C) { extrair7z(zipPath, outPath); return; }
        else if (hn >= 2 && hdr[0] == 'P' && hdr[1] == 'K') { snprintf(msgStatus, sizeof(msgStatus), "ZIP INCOMPLETO/CORROMPIDO - BAIXE NOVAMENTE (cod %d)", cod); }
        else if (hn >= 2 && hdr[0] == '<' && (hdr[1] == '!' || hdr[1] == 'h' || hdr[1] == 'H')) { snprintf(msgStatus, sizeof(msgStatus), "ISTO E UMA PAGINA DE ERRO, NAO O ARQUIVO - BAIXE DE NOVO"); }
        else { snprintf(msgStatus, sizeof(msgStatus), "NAO E UM ZIP VALIDO (cod %d)", cod); }
        FILE* dbg = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
        if (dbg) { fprintf(dbg, "FALHA init cod=%d size=%ld hdr=%02X%02X%02X%02X path=%s\n", cod, fsz, hdr[0], hdr[1], hdr[2], hdr[3], zipPath); fclose(dbg); }
        msgTimer = 360; return;
    }
    int totalArquivos = (int)mz_zip_reader_get_num_files(&zip_archive);
    if (totalArquivos <= 0) {
        mz_zip_reader_end(&zip_archive);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO: ZIP VAZIO OU SEM ARQUIVOS!"); msgTimer = 300; return;
    }
    int extraidos = 0, falhas = 0, semSuporte = 0;
    for (int i = 0; i < totalArquivos; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) { falhas++; continue; }
        const char* nome = file_stat.m_filename;
        if (!nome || nome[0] == '\0') { falhas++; continue; }

        char nomeNorm[1024];
        snprintf(nomeNorm, sizeof(nomeNorm), "%s", nome);
        for (char* q = nomeNorm; *q; q++) { if (*q == '\\') *q = '/'; }
        while (nomeNorm[0] == '/') memmove(nomeNorm, nomeNorm + 1, strlen(nomeNorm));
        while (strncmp(nomeNorm, "./", 2) == 0) memmove(nomeNorm, nomeNorm + 2, strlen(nomeNorm + 2) + 1);

        bool inseguro = false;
        char tmpNome[1024]; snprintf(tmpNome, sizeof(tmpNome), "%s", nomeNorm);
        char* tok = strtok(tmpNome, "/");
        while (tok) { if (strcmp(tok, "..") == 0) { inseguro = true; break; } tok = strtok(NULL, "/"); }
        if (inseguro || nomeNorm[0] == '\0') { falhas++; continue; }

        char out_file[2048];
        snprintf(out_file, sizeof(out_file), "%s/%s", outPath, nomeNorm);
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            criarCaminho(out_file); sceKernelMkdir(out_file, 0777); extraidos++;
        } else if (!mz_zip_reader_is_file_supported(&zip_archive, i)) {
            semSuporte++;
        } else {
            criarCaminho(out_file);
            HnZipSaida saida = { fopen(out_file, "wb"), 0, file_stat.m_uncomp_size, i, totalArquivos };
            bool certo = saida.f && mz_zip_reader_extract_to_callback(&zip_archive, i, hnEscreverZip, &saida, 0);
            if (saida.f) fclose(saida.f);
            if (certo) { extraidos++; hnAnotarPkg(out_file); }
            else { falhas++; remove(out_file); }
        }
        float prog = (float)(i + 1) / (float)totalArquivos;
        snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO: %d%%", (int)(prog * 100)); atualizarBarra(prog);
    }
    mz_zip_reader_end(&zip_archive);
    if (semSuporte > 0 && extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ZIP COM COMPRESSAO NAO SUPORTADA (use Normal/Deflate)"); msgTimer = 360; }
    else if (falhas > 0 && extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ERRO AO EXTRAIR: NENHUM ARQUIVO (%d falhas)", falhas); msgTimer = 300; }
    else if (semSuporte > 0) { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO: %d OK, %d sem suporte", extraidos, semSuporte); msgTimer = 300; }
    else if (falhas > 0) { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO: %d OK, %d falhas", extraidos, falhas); msgTimer = 300; }
    else { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO COM SUCESSO! (%d arquivos)", extraidos); msgTimer = 180; }
}

extern "C" {
int hn7z_open(const char* path, void** out);
int hn7z_num(void* h);
int hn7z_info(void* h, int i, char* name, int nameSize, int* isDir, unsigned long long* size);
long hn7z_ler(void* h, int i, void* destino, long tamanho);
int hn7z_last_error(void* h);
void hn7z_close(void* h);
}

static bool nomeEntrada7zSeguro(const char* nome, char* out, int outSize) {
    if (!nome || !nome[0] || outSize <= 0) return false;
    snprintf(out, outSize, "%s", nome);
    for (char* q = out; *q; q++) { if (*q == '\\') *q = '/'; }
    while (out[0] == '/') memmove(out, out + 1, strlen(out));
    while (strncmp(out, "./", 2) == 0) memmove(out, out + 2, strlen(out + 2) + 1);
    if (out[0] == '\0') return false;
    char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", out);
    char* tok = strtok(tmp, "/");
    while (tok) { if (strcmp(tok, "..") == 0) return false; tok = strtok(NULL, "/"); }
    return true;
}

extern "C" {
void* hnrar_abrir(const char* caminho);
int hnrar_proximo(void* h, char* nome, int tamanhoNome, int* ehPasta);
long hnrar_ler(void* h, void* destino, long tamanho);
int hnrar_progresso(void* h);
const char* hnrar_erro(void* h);
void hnrar_fechar(void* h);
void* hnrar_abrir_lista(const char* lista, int total, int passo);
}

static void extrairRarAberto(void* h, const char* rarPath, const char* outPath);

void extrairRar(const char* rarPath, const char* outPath) {
    snprintf(msgStatus, sizeof(msgStatus), "LENDO ARQUIVO RAR..."); atualizarBarra(0.01f);
    extrairRarAberto(hnrar_abrir(rarPath), rarPath, outPath);
}

static void extrairRarAberto(void* h, const char* rarPath, const char* outPath) {
    unsigned char* buf = (unsigned char*)malloc(262144);
    if (!h || !buf) {
        free(buf); hnrar_fechar(h);
        snprintf(msgStatus, sizeof(msgStatus), "RAR CORROMPIDO, COM SENHA OU INCOMPLETO"); msgTimer = 360;
        FILE* dbg = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
        if (dbg) { fprintf(dbg, "RAR FALHA abrir path=%s\n", rarPath); fclose(dbg); }
        return;
    }

    int extraidos = 0, falhas = 0; bool comSenha = false;
    char ultimoErro[160] = "";
    while (true) {
        char nome[1024], nomeNorm[1024]; int ehPasta = 0;
        int r = hnrar_proximo(h, nome, sizeof(nome), &ehPasta);
        if (r == 0) break;
        if (r < 0) { falhas++; snprintf(ultimoErro, sizeof(ultimoErro), "%s", hnrar_erro(h)); break; }
        if (!nomeEntrada7zSeguro(nome, nomeNorm, sizeof(nomeNorm))) { falhas++; continue; }

        char out_file[2048];
        snprintf(out_file, sizeof(out_file), "%s/%s", outPath, nomeNorm);
        criarCaminho(out_file);
        if (ehPasta) { sceKernelMkdir(out_file, 0777); extraidos++; continue; }

        FILE* fo = fopen(out_file, "wb");
        if (!fo) { falhas++; continue; }
        long n; bool ok = true; int blocos = 0;
        while ((n = hnrar_ler(h, buf, 262144)) > 0) {
            if (fwrite(buf, 1, (size_t)n, fo) != (size_t)n) { ok = false; break; }
            if (++blocos % 64 == 0) { int pct = hnrar_progresso(h); snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO RAR: %d%%", pct); atualizarBarra(pct / 100.0f); }
        }
        fclose(fo);
        if (n < 0) { ok = false; snprintf(ultimoErro, sizeof(ultimoErro), "%s", hnrar_erro(h)); }
        if (ok) { extraidos++; hnAnotarPkg(out_file); } else { falhas++; remove(out_file); }
        if (strstr(ultimoErro, "ncrypt") || strstr(ultimoErro, "assword")) { comSenha = true; break; }
        int pct = hnrar_progresso(h);
        snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO RAR: %d%%", pct); atualizarBarra(pct / 100.0f);
    }
    free(buf); hnrar_fechar(h);

    if (ultimoErro[0]) {
        FILE* dbg = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
        if (dbg) { fprintf(dbg, "RAR erro: %s path=%s\n", ultimoErro, rarPath); fclose(dbg); }
    }
    if (comSenha) { snprintf(msgStatus, sizeof(msgStatus), "RAR COM SENHA NAO SUPORTADO"); msgTimer = 360; }
    else if (falhas > 0 && extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ERRO AO EXTRAIR RAR: NENHUM ARQUIVO (%d falhas)", falhas); msgTimer = 360; }
    else if (falhas > 0) { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO RAR: %d OK, %d falhas", extraidos, falhas); msgTimer = 300; }
    else if (extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ERRO: RAR VAZIO OU SEM ARQUIVOS!"); msgTimer = 300; }
    else { snprintf(msgStatus, sizeof(msgStatus), "RAR EXTRAIDO COM SUCESSO! (%d arquivos)", extraidos); msgTimer = 180; }
}

void extrair7z(const char* zipPath, const char* outPath) {
    snprintf(msgStatus, sizeof(msgStatus), "LENDO ARQUIVO 7Z..."); atualizarBarra(0.01f);
    void* h = NULL;
    int rc = hn7z_open(zipPath, &h);
    if (rc != 0 || !h) {
        if (rc == -1001) snprintf(msgStatus, sizeof(msgStatus), "ERRO 7Z: NAO ABRIU O ARQUIVO");
        else if (rc == -1003) snprintf(msgStatus, sizeof(msgStatus), "ARQUIVO 7Z VAZIO (0 bytes)");
        else if (rc == -1005) snprintf(msgStatus, sizeof(msgStatus), "ERRO 7Z: LEITURA INCOMPLETA - BAIXE NOVAMENTE");
        else if (rc == 17) snprintf(msgStatus, sizeof(msgStatus), "NAO E UM ARQUIVO 7Z VALIDO");
        else if (rc == 16) snprintf(msgStatus, sizeof(msgStatus), "7Z CORROMPIDO/INCOMPLETO - BAIXE NOVAMENTE");
        else if (rc == 4) snprintf(msgStatus, sizeof(msgStatus), "7Z COM METODO NAO SUPORTADO (ex: PPMd)");
        else if (rc == 2) snprintf(msgStatus, sizeof(msgStatus), "SEM MEMORIA PARA ABRIR O 7Z");
        else snprintf(msgStatus, sizeof(msgStatus), "ERRO 7Z (cod %d)", rc);
        FILE* dbg = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
        if (dbg) { fprintf(dbg, "7Z FALHA open rc=%d path=%s\n", rc, zipPath); fclose(dbg); }
        msgTimer = 360; return;
    }
    int totalArquivos = hn7z_num(h);
    if (totalArquivos <= 0) {
        hn7z_close(h);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO: 7Z VAZIO OU SEM ARQUIVOS!"); msgTimer = 300; return;
    }
    unsigned char* buf = (unsigned char*)malloc(262144);
    if (!buf) { hn7z_close(h); snprintf(msgStatus, sizeof(msgStatus), "SEM MEMORIA PARA EXTRAIR O 7Z"); msgTimer = 300; return; }
    int extraidos = 0, falhas = 0, ultimoPct = -1;
    for (int i = 0; i < totalArquivos; i++) {
        char nome[1024]; int isDir = 0; unsigned long long fsz = 0;
        if (hn7z_info(h, i, nome, sizeof(nome), &isDir, &fsz) != 0) { falhas++; continue; }
        char nomeNorm[1024];
        if (!nomeEntrada7zSeguro(nome, nomeNorm, sizeof(nomeNorm))) { falhas++; continue; }

        char out_file[2048];
        snprintf(out_file, sizeof(out_file), "%s/%s", outPath, nomeNorm);
        criarCaminho(out_file);
        if (isDir) { sceKernelMkdir(out_file, 0777); extraidos++; }
        else {
            FILE* fo = fopen(out_file, "wb");
            if (!fo) { falhas++; continue; }
            long n; bool ok = true;
            unsigned long long feito = 0;
            int blocos = 0;
            while ((n = hn7z_ler(h, i, buf, 262144)) > 0) {
                if (fwrite(buf, 1, (size_t)n, fo) != (size_t)n) { ok = false; break; }
                feito += (unsigned long long)n;
                if (fsz && ++blocos % 64 == 0) {
                    float prog = ((float)i + (float)((double)feito / (double)fsz)) / (float)totalArquivos;
                    snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO 7Z: %d%%", (int)(prog * 100));
                    atualizarBarra(prog);
                }
            }
            fclose(fo);
            if (n < 0) ok = false;
            if (ok) { extraidos++; hnAnotarPkg(out_file); } else { falhas++; remove(out_file); }
            // Num bloco sólido, depois de um erro de dados nada mais sai certo.
            if (n < 0) break;
        }
        int pct = (i + 1) * 100 / totalArquivos;
        if (pct != ultimoPct) { ultimoPct = pct; snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO 7Z: %d%%", pct); atualizarBarra(pct / 100.0f); }
    }
    free(buf);
    int lastErr = hn7z_last_error(h);
    hn7z_close(h);
    if (falhas > 0 && extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ERRO AO EXTRAIR 7Z: NENHUM ARQUIVO (%d falhas, cod %d)", falhas, lastErr); msgTimer = 360; }
    else if (falhas > 0) { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO 7Z: %d OK, %d falhas", extraidos, falhas); msgTimer = 300; }
    else { snprintf(msgStatus, sizeof(msgStatus), "7Z EXTRAIDO COM SUCESSO! (%d arquivos)", extraidos); msgTimer = 180; }
}