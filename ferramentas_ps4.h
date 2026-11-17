// HYPER NEIVA - FERRAMENTAS DO PS4
// Calculadora, bloco de notas, relogio (relogio/cronometro/timer/alarme/calendario),
// gravador de audio, afinador, contatos, caixa de entrada, terminal do app, IA,
// QR/WhatsApp/codigo de barras e atalho da previsao do tempo. Tudo em um cabecalho
// para nao criar .cpp novo (o build.bat tem lista fixa de arquivos).
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <orbis/Pad.h>
#include <orbis/libkernel.h>
#include <png.h>

#include "menu.h"
#include "graphics.h"
#include "bloco_de_notas.h"
#include "ImeDialog.h"
#include "remoto_qr.h"

extern int sel;
extern MenuLevel menuAtual;
extern int totalItens;
extern char nomes[3000][256];
extern char msgStatus[128];
extern int msgTimer;
extern int32_t global_uId;
extern void empilharMenu(MenuLevel menu, int selAntes, int offAntes);
extern void iaAbrir();
extern void tempoAbrir();
extern void remotoDesenharQrTexto(uint32_t* p, const char* texto, int x, int y, int modulo);
extern int hnHttpGet(const char* url, char** resp, size_t* tamResp);

#define FT_RAIZ "/data/HyperNeiva"
#define FT_CONF FT_RAIZ "/configuracao"
#define FT_NOTAS FT_RAIZ "/notas"
#define FT_GRAVACOES FT_RAIZ "/gravacoes"
#define FT_BAIXADO FT_RAIZ "/baixado"

#define FT_LARGURA 1920
#define FT_ALTURA 1080

#define FT_COR_FUNDO 0xFF0D1117u
#define FT_COR_TOPO 0xFF161D26u
#define FT_COR_CARTAO 0xFF1C2530u
#define FT_COR_CARTAO_SEL 0xFF0B3A55u
#define FT_COR_AZUL 0xFF00AAFFu
#define FT_COR_BRANCO 0xFFFFFFFFu
#define FT_COR_CINZA 0xFF9AA7B4u
#define FT_COR_VERDE 0xFF42D77Du
#define FT_COR_VERMELHO 0xFFFF5555u
#define FT_COR_AMARELO 0xFFFFD166u

enum {
    FT_TELA_MENU = 0,
    FT_TELA_CALC,
    FT_TELA_RELOGIO,
    FT_TELA_GRAVADOR,
    FT_TELA_AFINADOR,
    FT_TELA_CONTATOS,
    FT_TELA_CAIXA,
    FT_TELA_TERMINAL,
    FT_TELA_QR
};

enum {
    FT_IME_NENHUM = 0,
    FT_IME_CONTATO_NOME,
    FT_IME_CONTATO_FONE,
    FT_IME_MSG_DE,
    FT_IME_MSG_ASSUNTO,
    FT_IME_MSG_TEXTO,
    FT_IME_TERMINAL,
    FT_IME_QR_TEXTO,
    FT_IME_QR_FONE,
    FT_IME_QR_MSG,
    FT_IME_CALC_VALOR,
    FT_IME_QUOTE_BUSCA,
    FT_IME_QUOTE_COPIAR
};

static int ftTela = FT_TELA_MENU;
static int ftSel = 0;
static int ftOff = 0;

// ---------------------------------------------------------------- utilidades
static void ftMkdirs(const char* caminho) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", caminho);
    size_t n = strlen(tmp);
    if (n == 0) return;
    for (size_t i = 1; i < n; i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0777);
}

static void ftLog(const char* texto) {
    FILE* f = fopen(FT_RAIZ "/ferramentas_log.txt", "a");
    if (!f) return;
    fprintf(f, "%s\n", texto);
    fclose(f);
}

static void ftCaixa(uint32_t* p, int x, int y, int w, int h, uint32_t cor) {
    if (!p) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > FT_LARGURA) w = FT_LARGURA - x;
    if (y + h > FT_ALTURA) h = FT_ALTURA - y;
    for (int yy = y; yy < y + h; yy++) {
        uint32_t* linha = p + (size_t)yy * FT_LARGURA;
        for (int xx = x; xx < x + w; xx++) linha[xx] = cor;
    }
}

static void ftFundo(uint32_t* p) {
    if (!p) return;
    for (int i = 0; i < FT_LARGURA * FT_ALTURA; i++) p[i] = FT_COR_FUNDO;
}

static void ftTextoCentral(uint32_t* p, const char* texto, int tam, int y, uint32_t cor) {
    int largura = (int)(strlen(texto) * tam * 0.56f);
    desenharTexto(p, texto, tam, (FT_LARGURA - largura) / 2, y, cor);
}

static void ftTopo(uint32_t* p, const char* titulo, const char* dica) {
    ftCaixa(p, 0, 0, FT_LARGURA, 86, FT_COR_TOPO);
    ftCaixa(p, 0, 84, FT_LARGURA, 2, FT_COR_AZUL);
    desenharTexto(p, titulo, 34, 46, 22, FT_COR_BRANCO);
    if (dica && dica[0]) desenharTexto(p, dica, 22, 1240, 32, FT_COR_CINZA);
}

static void ftRodape(uint32_t* p, const char* texto) {
    ftCaixa(p, 0, FT_ALTURA - 58, FT_LARGURA, 58, FT_COR_TOPO);
    desenharTexto(p, texto, 22, 46, FT_ALTURA - 42, FT_COR_CINZA);
}

static void ftAviso(const char* texto) {
    snprintf(msgStatus, sizeof(msgStatus), "%s", texto);
    msgTimer = 180;
}

static void ftDataHora(struct tm* saida) {
    time_t agora = time(NULL);
    struct tm* t = localtime(&agora);
    if (t) *saida = *t;
    else memset(saida, 0, sizeof(*saida));
}

static double ftSegundos(void) {
    return (double)sceKernelGetProcessTime() / 1000000.0;
}

static void ftHorario(double segundos, char* texto, int tam, bool centesimos) {
    if (segundos < 0) segundos = 0;
    int total = (int)segundos;
    int h = total / 3600, m = (total / 60) % 60, s = total % 60;
    int cs = (int)((segundos - total) * 100.0);
    if (centesimos) snprintf(texto, tam, "%02d:%02d:%02d.%02d", h, m, s, cs);
    else snprintf(texto, tam, "%02d:%02d:%02d", h, m, s);
}

static void ftListar(const char* pasta, char* saida, int tam) {
    saida[0] = 0;
    DIR* d = opendir(pasta);
    if (!d) { snprintf(saida, tam, "(pasta vazia)"); return; }
    struct dirent* e;
    int n = 0;
    while ((e = readdir(d)) && n < 40) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (n) strncat(saida, "  ", tam - strlen(saida) - 1);
        strncat(saida, e->d_name, tam - strlen(saida) - 1);
        n++;
    }
    closedir(d);
    if (!n) snprintf(saida, tam, "(pasta vazia)");
}

// ---------------------------------------------------------------- teclado IME
static bool ftImeAberto = false;
static int ftImeModo = FT_IME_NENHUM;
static uint16_t ftImeBuffer[2048];
static uint16_t ftImeTitulo[96];

static void ftImeResultado(int modo, const char* texto);

static void ftImeAbrir(int modo, const char* titulo, const char* inicial, int maximo) {
    if (ftImeAberto) return;
    if (maximo > 2046) maximo = 2046;
    memset(ftImeBuffer, 0, sizeof(ftImeBuffer));
    if (inicial) {
        for (int i = 0; inicial[i] && i < maximo; i++) ftImeBuffer[i] = (uint8_t)inicial[i];
    }
    memset(ftImeTitulo, 0, sizeof(ftImeTitulo));
    int i = 0;
    for (; titulo && titulo[i] && i < 94; i++) ftImeTitulo[i] = (uint8_t)titulo[i];

    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    param.userId = global_uId;
    param.maxTextLength = (uint32_t)maximo;
    param.inputTextBuffer = (wchar_t*)ftImeBuffer;
    param.title = (wchar_t*)ftImeTitulo;
    param.type = (OrbisImeType)0;
    int rc = sceImeDialogInit(&param, NULL);
    if (rc >= 0) {
        ftImeAberto = true;
        ftImeModo = modo;
    }
    else ftAviso("O teclado do PS4 nao abriu.");
}

static void ftImeProcessar(void) {
    if (!ftImeAberto) return;
    int status = (int)sceImeDialogGetStatus();
    if (status == 0 || status == 1) return;

    OrbisDialogResult resultado;
    memset(&resultado, 0, sizeof(resultado));
    sceImeDialogGetResult(&resultado);
    int32_t botao = *(int32_t*)&resultado;

    static char texto[2048];
    int n = 0;
    for (; n < 2047 && ftImeBuffer[n]; n++) texto[n] = (char)ftImeBuffer[n];
    texto[n] = 0;
    sceImeDialogTerm();
    ftImeAberto = false;
    int modo = ftImeModo;
    ftImeModo = FT_IME_NENHUM;
    if (botao == 0 && n > 0) ftImeResultado(modo, texto);
}

// ---------------------------------------------------------------- menu
static const char* ftNomes[] = {
    "CALCULADORA", "BLOCO DE NOTAS", "RELOGIO", "GRAVADOR", "AFINADOR",
    "CONTATOS", "CAIXA DE ENTRADA", "TERMINAL", "IA / GEMINI", "QR / CODIGOS",
    "PREVISAO DO TEMPO"
};
static const char* ftDesc[] = {
    "Cientifica, conversoes, cotacoes e acoes", "Editar e salvar texto", "Hora, cronometro, timer e alarme",
    "Gravar audio do microfone", "Afine violao e instrumentos", "Lista de contatos",
    "Mensagens recebidas", "Comandos do Hyper Neiva", "Perguntas, voz e imagens",
    "QR, WhatsApp e barras", "Abrir a previsao do tempo"
};
#define FT_TOTAL 11

static void ftDesenharMenu(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "FERRAMENTAS", "Tudo o que o Hyper Neiva tem para o dia a dia");
    const int colunas = 5, larg = 330, alt = 190, gapX = 22, gapY = 26;
    const int inicioX = (FT_LARGURA - (colunas * larg + (colunas - 1) * gapX)) / 2;
    const int inicioY = 210;
    for (int i = 0; i < FT_TOTAL; i++) {
        int cx = inicioX + (i % colunas) * (larg + gapX);
        int cy = inicioY + (i / colunas) * (alt + gapY);
        bool escolhido = (i == ftSel);
        ftCaixa(p, cx, cy, larg, alt, escolhido ? FT_COR_CARTAO_SEL : FT_COR_CARTAO);
        ftCaixa(p, cx, cy, 6, alt, escolhido ? FT_COR_AZUL : 0xFF2A3644);
        desenharTexto(p, ftNomes[i], 28, cx + 30, cy + 34, escolhido ? FT_COR_AZUL : FT_COR_BRANCO);
        desenharTexto(p, ftDesc[i], 20, cx + 30, cy + 88, FT_COR_CINZA);
        char numero[16];
        snprintf(numero, sizeof(numero), "%d", i + 1);
        desenharTexto(p, numero, 22, cx + larg - 42, cy + alt - 42, 0xFF536273);
    }
    ftRodape(p, "X abrir   Direcional escolher   Bolinha voltar ao EXTRA");
    if (msgTimer > 0) desenharTexto(p, msgStatus, 24, 120, 960, FT_COR_AMARELO);
}

// ---------------------------------------------------------------- calculadora
#define FT_CALC_MAX_HIST 8
#define FT_CALC_MAX_TECLAS 40
#define FT_CALC_CAMPOS_MAX 8
#define FT_CALC_LINHAS_MAX 5
#define FT_CALC_UNIDADES_MAX 12

enum {
    FT_CT_VALOR = 0,
    FT_CT_LIMPAR,
    FT_CT_APAGAR,
    FT_CT_IGUAL,
    FT_CT_2ND,
    FT_CT_GRAU,
    FT_CT_CIENTIFICA
};

enum {
    FT_CAT_IDADE = 0,
    FT_CAT_AREA,
    FT_CAT_IMC,
    FT_CAT_DADOS,
    FT_CAT_DATA,
    FT_CAT_DESCONTO,
    FT_CAT_COMPRIMENTO,
    FT_CAT_MASSA,
    FT_CAT_BASE,
    FT_CAT_VELOCIDADE,
    FT_CAT_TEMPERATURA,
    FT_CAT_TEMPO,
    FT_CAT_VOLUME,
    FT_CAT_MOEDA,
    FT_CAT_CRIPTO,
    FT_CAT_INVESTIMENTO,
    FT_CAT_EMPRESTIMO,
    FT_CAT_TOTAL
};

struct FtCalcTecla {
    char rotulo[20];
    char token[24];
    int x, y, w, h;
    int tipo;
};

struct FtCalcUnidade {
    const char* nome;
    double fator;
};

typedef struct {
    const char* s;
    int i;
    bool graus;
    bool erro;
} FtCalcParser;

static int ftCalcRecurso = 0;
static bool ftCalcCientifica = false;
static bool ftCalc2nd = false;
static bool ftCalcGraus = true;
static char ftCalcExpr[256] = "";
static char ftCalcResultado[64] = "0";
static bool ftCalcTemResultado = false;
static char ftCalcHist[FT_CALC_MAX_HIST][168];
static int ftCalcHistTotal = 0;
static int ftCalcBotao = 0;

static const char* ftCalcCatNomes[FT_CAT_TOTAL] = {
    "Idade", "Area", "IMC", "Dados", "Data", "Desconto", "Comprimento",
    "Massa", "Sistema numerico", "Velocidade", "Temperatura", "Tempo",
    "Volume", "Moeda", "Cripto", "Investimento", "Emprestimo"
};

static double ftCalcCampos[FT_CAT_TOTAL][FT_CALC_CAMPOS_MAX];
static bool ftCalcCamposInit[FT_CAT_TOTAL];
static int ftCalcCategoria = FT_CAT_IDADE;
static int ftCalcCampo = 0;
static int ftCalcImeCat = -1;
static int ftCalcImeCampo = -1;
static bool ftCalcListaAberta = false;
static int ftCalcListaSel = 0;
static int ftCalcListaOffset = 0;

static const FtCalcUnidade ftCalcTabArea[] = {
    { "m2", 1.0 }, { "km2", 1000000.0 }, { "cm2", 0.0001 },
    { "hectare", 10000.0 }, { "acre", 4046.8564224 }, { "ft2", 0.09290304 }
};
static const FtCalcUnidade ftCalcTabDados[] = {
    { "B", 1.0 }, { "KB", 1024.0 }, { "MB", 1048576.0 },
    { "GB", 1073741824.0 }, { "TB", 1099511627776.0 }
};
static const FtCalcUnidade ftCalcTabComprimento[] = {
    { "m", 1.0 }, { "km", 1000.0 }, { "cm", 0.01 }, { "mm", 0.001 },
    { "mi", 1609.344 }, { "ft", 0.3048 }, { "in", 0.0254 }
};
static const FtCalcUnidade ftCalcTabMassa[] = {
    { "kg", 1.0 }, { "g", 0.001 }, { "mg", 0.000001 }, { "ton", 1000.0 },
    { "lb", 0.45359237 }, { "oz", 0.028349523125 }
};
static const FtCalcUnidade ftCalcTabVelocidade[] = {
    { "km/h", 1.0 }, { "m/s", 3.6 }, { "mph", 1.609344 }, { "no", 1.852 }
};
static const FtCalcUnidade ftCalcTabTempo[] = {
    { "seg", 1.0 }, { "min", 60.0 }, { "hora", 3600.0 },
    { "dia", 86400.0 }, { "semana", 604800.0 }
};
static const FtCalcUnidade ftCalcTabVolume[] = {
    { "L", 1.0 }, { "mL", 0.001 }, { "m3", 1000.0 }, { "cm3", 0.001 },
    { "gal", 3.785411784 }, { "fl oz", 0.0295735295625 }
};
static const FtCalcUnidade ftCalcTabMoeda[] = {
    { "BRL", 1.0 }, { "USD", 5.40 }, { "EUR", 5.90 },
    { "GBP", 7.00 }, { "ARS", 0.0055 }, { "JPY", 0.036 }
};
static const FtCalcUnidade ftCalcTabCripto[] = {
    { "BRL", 1.0 }, { "BTC", 350000.0 }, { "ETH", 18000.0 },
    { "USDT", 5.40 }, { "BNB", 3000.0 }, { "SOL", 900.0 },
    { "XRP", 12.0 }, { "ADA", 3.0 }, { "DOGE", 1.20 }, { "LTC", 500.0 }
};
static const char* ftCalcTempNomes[3] = { "C", "F", "K" };

static void ftCalcPular(FtCalcParser* p) {
    while (p->s[p->i] == ' ' || p->s[p->i] == '\t') p->i++;
}

static bool ftCalcComeca(FtCalcParser* p, const char* texto) {
    size_t n = strlen(texto);
    if (strncmp(p->s + p->i, texto, n) != 0) return false;
    p->i += (int)n;
    return true;
}

static bool ftCalcComer(FtCalcParser* p, char c) {
    ftCalcPular(p);
    if (p->s[p->i] != c) return false;
    p->i++;
    return true;
}

static double ftCalcParserExpr(FtCalcParser* p);
static double ftCalcParserTermo(FtCalcParser* p);
static double ftCalcParserPot(FtCalcParser* p);
static double ftCalcParserUnario(FtCalcParser* p);
static double ftCalcParserPos(FtCalcParser* p);
static double ftCalcParserPrimario(FtCalcParser* p);

static bool ftCalcFatorial(double v, double* saida) {
    if (v < 0.0 || v > 170.0 || fabs(v - floor(v + 0.5)) > 0.000001) return false;
    double r = 1.0;
    int n = (int)(v + 0.5);
    for (int i = 2; i <= n; i++) r *= i;
    *saida = r;
    return true;
}

static double ftCalcParserExpr(FtCalcParser* p) {
    double v = ftCalcParserTermo(p);
    while (!p->erro) {
        ftCalcPular(p);
        char c = p->s[p->i];
        if (c != '+' && c != '-') break;
        p->i++;
        double b = ftCalcParserTermo(p);
        if (p->erro) return 0.0;
        if (c == '+') v += b;
        else v -= b;
    }
    return v;
}

static double ftCalcParserTermo(FtCalcParser* p) {
    double v = ftCalcParserPot(p);
    while (!p->erro) {
        ftCalcPular(p);
        char c = p->s[p->i];
        bool implicito = (c == '(' || c == '.' || isdigit((unsigned char)c) || isalpha((unsigned char)c));
        if (c == '*' || c == '/') {
            p->i++;
            double b = ftCalcParserPot(p);
            if (p->erro) return 0.0;
            if (c == '*') v *= b;
            else {
                if (b == 0.0) { p->erro = true; return 0.0; }
                v /= b;
            }
        }
        else if (implicito) {
            double b = ftCalcParserPot(p);
            if (p->erro) return 0.0;
            v *= b;
        }
        else break;
    }
    return v;
}

static double ftCalcParserPot(FtCalcParser* p) {
    double v = ftCalcParserUnario(p);
    if (p->erro) return 0.0;
    ftCalcPular(p);
    if (p->s[p->i] == '^') {
        p->i++;
        double b = ftCalcParserPot(p);
        if (p->erro) return 0.0;
        double r = pow(v, b);
        if (!(r > -1.0e308 && r < 1.0e308)) { p->erro = true; return 0.0; }
        return r;
    }
    return v;
}

static double ftCalcParserUnario(FtCalcParser* p) {
    ftCalcPular(p);
    char c = p->s[p->i];
    if (c == '+' || c == '-') {
        p->i++;
        double v = ftCalcParserUnario(p);
        return c == '-' ? -v : v;
    }
    return ftCalcParserPos(p);
}

static double ftCalcParserPos(FtCalcParser* p) {
    double v = ftCalcParserPrimario(p);
    while (!p->erro) {
        ftCalcPular(p);
        char c = p->s[p->i];
        if (c == '!') {
            p->i++;
            if (!ftCalcFatorial(v, &v)) { p->erro = true; return 0.0; }
        }
        else if (c == '%') {
            p->i++;
            v /= 100.0;
        }
        else break;
    }
    return v;
}

static double ftCalcParserPrimario(FtCalcParser* p) {
    ftCalcPular(p);
    if (p->erro) return 0.0;
    if (ftCalcComer(p, '(')) {
        double v = ftCalcParserExpr(p);
        if (!ftCalcComer(p, ')')) p->erro = true;
        return v;
    }

    static const char* funcoes[] = { "asin", "acos", "atan", "sqrt", "sin", "cos", "tan", "log", "ln", "abs" };
    for (int f = 0; f < 10; f++) {
        if (!ftCalcComeca(p, funcoes[f])) continue;
        if (!ftCalcComer(p, '(')) { p->erro = true; return 0.0; }
        double v = ftCalcParserExpr(p);
        if (!ftCalcComer(p, ')') || p->erro) { p->erro = true; return 0.0; }
        double rad = p->graus ? v * 3.14159265358979323846 / 180.0 : v;
        if (!strcmp(funcoes[f], "sin")) return sin(rad);
        if (!strcmp(funcoes[f], "cos")) return cos(rad);
        if (!strcmp(funcoes[f], "tan")) return tan(rad);
        if (!strcmp(funcoes[f], "asin") || !strcmp(funcoes[f], "acos")) {
            if (v < -1.0 || v > 1.0) { p->erro = true; return 0.0; }
            double r = !strcmp(funcoes[f], "asin") ? asin(v) : acos(v);
            return p->graus ? r * 180.0 / 3.14159265358979323846 : r;
        }
        if (!strcmp(funcoes[f], "atan")) { double r = atan(v); return p->graus ? r * 180.0 / 3.14159265358979323846 : r; }
        if (!strcmp(funcoes[f], "sqrt")) { if (v < 0.0) { p->erro = true; return 0.0; } return sqrt(v); }
        if (!strcmp(funcoes[f], "log") || !strcmp(funcoes[f], "ln")) {
            if (v <= 0.0) { p->erro = true; return 0.0; }
            return !strcmp(funcoes[f], "log") ? log10(v) : log(v);
        }
        return fabs(v);
    }

    if (ftCalcComeca(p, "pi")) return 3.14159265358979323846;
    if (ftCalcComeca(p, "e")) return 2.71828182845904523536;

    char* fim = NULL;
    double v = strtod(p->s + p->i, &fim);
    if (fim == p->s + p->i) { p->erro = true; return 0.0; }
    p->i = (int)(fim - p->s);
    return v;
}

static bool ftCalcAvaliar(const char* texto, double* saida) {
    if (!texto || !texto[0]) return false;
    FtCalcParser p;
    p.s = texto;
    p.i = 0;
    p.graus = ftCalcGraus;
    p.erro = false;
    double v = ftCalcParserExpr(&p);
    ftCalcPular(&p);
    if (p.erro || p.s[p.i] != 0 || !(v > -1.0e308 && v < 1.0e308)) return false;
    *saida = v;
    return true;
}

static void ftCalcNumeroTexto(double v, char* saida, int tam) {
    if (!(v > -1.0e308 && v < 1.0e308)) { snprintf(saida, tam, "Erro"); return; }
    if (fabs(v) < 0.000000000001) v = 0.0;
    snprintf(saida, tam, "%.12g", v);
}

static bool ftCalcTextoNumero(const char* texto, double* saida) {
    char temp[80];
    int n = 0;
    for (int i = 0; texto[i] && n < 79; i++) {
        char c = texto[i];
        if (c == ',') c = '.';
        if (c == ' ' || c == '\t') continue;
        temp[n++] = c;
    }
    temp[n] = 0;
    if (!n) return false;
    char* fim = NULL;
    double v = strtod(temp, &fim);
    if (fim == temp) return false;
    while (*fim == ' ' || *fim == '\t') fim++;
    if (*fim || !(v > -1.0e308 && v < 1.0e308)) return false;
    *saida = v;
    return true;
}

static void ftCalcHistoricoAdicionar(const char* expr, const char* resultado) {
    int limite = ftCalcHistTotal < FT_CALC_MAX_HIST ? ftCalcHistTotal : FT_CALC_MAX_HIST - 1;
    for (int i = limite; i > 0; i--) snprintf(ftCalcHist[i], sizeof(ftCalcHist[i]), "%s", ftCalcHist[i - 1]);
    snprintf(ftCalcHist[0], sizeof(ftCalcHist[0]), "%s = %s", expr, resultado);
    if (ftCalcHistTotal < FT_CALC_MAX_HIST) ftCalcHistTotal++;
}

static void ftCalcAtualizarPrevia(void) {
    if (!ftCalcExpr[0]) { snprintf(ftCalcResultado, sizeof(ftCalcResultado), "0"); return; }
    double v = 0.0;
    if (ftCalcAvaliar(ftCalcExpr, &v)) ftCalcNumeroTexto(v, ftCalcResultado, sizeof(ftCalcResultado));
}

static bool ftCalcEhFuncao(const char* tok) {
    return !strcmp(tok, "sqrt(") || !strcmp(tok, "sin(") || !strcmp(tok, "cos(") ||
           !strcmp(tok, "tan(") || !strcmp(tok, "asin(") || !strcmp(tok, "acos(") ||
           !strcmp(tok, "atan(") || !strcmp(tok, "log(") || !strcmp(tok, "ln(") ||
           !strcmp(tok, "1/(");
}

static void ftCalcInserir(const char* tok) {
    if (!tok || !tok[0]) return;
    bool funcao = ftCalcEhFuncao(tok);
    if (ftCalcTemResultado) {
        if (funcao) {
            char temp[512];
            snprintf(temp, sizeof(temp), "%s%s)", tok, ftCalcResultado);
            snprintf(ftCalcExpr, sizeof(ftCalcExpr), "%s", temp);
            ftCalcTemResultado = false;
            ftCalcAtualizarPrevia();
            return;
        }
        if (isdigit((unsigned char)tok[0]) || tok[0] == '.' || tok[0] == '(' ||
            !strcmp(tok, "pi") || !strcmp(tok, "e")) {
            ftCalcExpr[0] = 0;
            snprintf(ftCalcResultado, sizeof(ftCalcResultado), "0");
        }
        else snprintf(ftCalcExpr, sizeof(ftCalcExpr), "%s", ftCalcResultado);
        ftCalcTemResultado = false;
    }

    size_t n = strlen(ftCalcExpr);
    bool operador = !strcmp(tok, "+") || !strcmp(tok, "-") || !strcmp(tok, "*") ||
                    !strcmp(tok, "/") || !strcmp(tok, "^");
    if (operador && n > 0 && strchr("+-*/^", ftCalcExpr[n - 1])) { ftCalcExpr[n - 1] = 0; n--; }
    if (operador && n == 0) { snprintf(ftCalcExpr, sizeof(ftCalcExpr), "0"); n = 1; }
    if (n + strlen(tok) >= sizeof(ftCalcExpr)) return;
    strncat(ftCalcExpr, tok, sizeof(ftCalcExpr) - n - 1);
    ftCalcAtualizarPrevia();
}

static void ftCalcLimpar(void) {
    ftCalcExpr[0] = 0;
    snprintf(ftCalcResultado, sizeof(ftCalcResultado), "0");
    ftCalcTemResultado = false;
}

static void ftCalcApagar(void) {
    if (ftCalcTemResultado) {
        if (!strcmp(ftCalcResultado, "Erro")) { ftCalcLimpar(); return; }
        snprintf(ftCalcExpr, sizeof(ftCalcExpr), "%s", ftCalcResultado);
        ftCalcTemResultado = false;
    }
    size_t n = strlen(ftCalcExpr);
    if (n > 0) ftCalcExpr[n - 1] = 0;
    if (!ftCalcExpr[0]) snprintf(ftCalcResultado, sizeof(ftCalcResultado), "0");
    else ftCalcAtualizarPrevia();
}

static void ftCalcIgual(void) {
    if (!ftCalcExpr[0]) return;
    char original[256];
    snprintf(original, sizeof(original), "%s", ftCalcExpr);
    double v = 0.0;
    if (ftCalcAvaliar(original, &v)) {
        ftCalcNumeroTexto(v, ftCalcResultado, sizeof(ftCalcResultado));
        ftCalcHistoricoAdicionar(original, ftCalcResultado);
    }
    else snprintf(ftCalcResultado, sizeof(ftCalcResultado), "Erro");
    ftCalcTemResultado = true;
}

static void ftCalcAddTecla(FtCalcTecla* lista, int* n, int max, int x, int y, int w, int h,
                           const char* rotulo, const char* token, int tipo) {
    if (*n >= max) return;
    FtCalcTecla* t = &lista[*n];
    snprintf(t->rotulo, sizeof(t->rotulo), "%s", rotulo ? rotulo : "");
    snprintf(t->token, sizeof(t->token), "%s", token ? token : "");
    t->x = x;
    t->y = y;
    t->w = w;
    t->h = h;
    t->tipo = tipo;
    (*n)++;
}

static int ftCalcMontarTeclas(FtCalcTecla* lista, int max) {
    int n = 0;
    const int colW = 220, gap = 16;
    const int rowH = ftCalcCientifica ? 82 : 104;
    const int rowGap = ftCalcCientifica ? 10 : 14;
    const int y0 = ftCalcCientifica ? 366 : 360;
    const int colunas = ftCalcCientifica ? 5 : 4;
    const int totalW = colunas * colW + (colunas - 1) * gap;
    int startX = 480 + (1380 - totalW) / 2;
    if (startX < 470) startX = 470;
    int baseX = ftCalcCientifica ? startX + colW + gap : startX;

    if (ftCalcCientifica) {
        int extraX = startX;
        int r0 = y0;
        int r1 = y0 + rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, extraX, r0, colW, rowH, ftCalc2nd ? "2nd*" : "2nd", "", FT_CT_2ND);
        ftCalcAddTecla(lista, &n, max, baseX, r0, colW, rowH, ftCalcGraus ? "DEG" : "RAD", "", FT_CT_GRAU);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), r0, colW, rowH, ftCalc2nd ? "sin-1" : "sin", ftCalc2nd ? "asin(" : "sin(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), r0, colW, rowH, ftCalc2nd ? "cos-1" : "cos", ftCalc2nd ? "acos(" : "cos(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), r0, colW, rowH, ftCalc2nd ? "tan-1" : "tan", ftCalc2nd ? "atan(" : "tan(", FT_CT_VALOR);

        ftCalcAddTecla(lista, &n, max, extraX, r1, colW, rowH, "x^y", "^", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX, r1, colW, rowH, "lg", "log(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), r1, colW, rowH, "ln", "ln(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), r1, colW, rowH, "(", "(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), r1, colW, rowH, ")", ")", FT_CT_VALOR);

        int extraR = r1 + rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, extraX, extraR, colW, rowH, "sqrt", "sqrt(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, extraX, extraR + (rowH + rowGap), colW, rowH, "x!", "!", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, extraX, extraR + 2 * (rowH + rowGap), colW, rowH, "1/x", "1/(", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, extraX, extraR + 3 * (rowH + rowGap), colW, rowH, "pi", "pi", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, extraX, extraR + 4 * (rowH + rowGap), colW, rowH, "f(x)", "", FT_CT_CIENTIFICA);

        int baseR = extraR;
        ftCalcAddTecla(lista, &n, max, baseX, baseR, colW, rowH, "C", "", FT_CT_LIMPAR);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), baseR, colW, rowH, "DEL", "", FT_CT_APAGAR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), baseR, colW, rowH, "%", "%", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), baseR, colW, rowH, "/", "/", FT_CT_VALOR);
        baseR += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, baseX, baseR, colW, rowH, "7", "7", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), baseR, colW, rowH, "8", "8", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), baseR, colW, rowH, "9", "9", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), baseR, colW, rowH, "*", "*", FT_CT_VALOR);
        baseR += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, baseX, baseR, colW, rowH, "4", "4", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), baseR, colW, rowH, "5", "5", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), baseR, colW, rowH, "6", "6", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), baseR, colW, rowH, "-", "-", FT_CT_VALOR);
        baseR += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, baseX, baseR, colW, rowH, "1", "1", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), baseR, colW, rowH, "2", "2", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), baseR, colW, rowH, "3", "3", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), baseR, colW, rowH, "+", "+", FT_CT_VALOR);
        baseR += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, baseX, baseR, colW, rowH, ftCalc2nd ? "E" : "e", ftCalc2nd ? "*10^(" : "e", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + (colW + gap), baseR, colW, rowH, "0", "0", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 2 * (colW + gap), baseR, colW, rowH, ".", ".", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, baseX + 3 * (colW + gap), baseR, colW, rowH, "=", "=", FT_CT_IGUAL);
    }
    else {
        int row = y0;
        ftCalcAddTecla(lista, &n, max, startX, row, colW, rowH, "C", "", FT_CT_LIMPAR);
        ftCalcAddTecla(lista, &n, max, startX + (colW + gap), row, colW, rowH, "DEL", "", FT_CT_APAGAR);
        ftCalcAddTecla(lista, &n, max, startX + 2 * (colW + gap), row, colW, rowH, "%", "%", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 3 * (colW + gap), row, colW, rowH, "/", "/", FT_CT_VALOR);
        row += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, startX, row, colW, rowH, "7", "7", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + (colW + gap), row, colW, rowH, "8", "8", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 2 * (colW + gap), row, colW, rowH, "9", "9", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 3 * (colW + gap), row, colW, rowH, "*", "*", FT_CT_VALOR);
        row += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, startX, row, colW, rowH, "4", "4", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + (colW + gap), row, colW, rowH, "5", "5", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 2 * (colW + gap), row, colW, rowH, "6", "6", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 3 * (colW + gap), row, colW, rowH, "-", "-", FT_CT_VALOR);
        row += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, startX, row, colW, rowH, "1", "1", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + (colW + gap), row, colW, rowH, "2", "2", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 2 * (colW + gap), row, colW, rowH, "3", "3", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 3 * (colW + gap), row, colW, rowH, "+", "+", FT_CT_VALOR);
        row += rowH + rowGap;
        ftCalcAddTecla(lista, &n, max, startX, row, colW, rowH, "f(x)", "", FT_CT_CIENTIFICA);
        ftCalcAddTecla(lista, &n, max, startX + (colW + gap), row, colW, rowH, "0", "0", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 2 * (colW + gap), row, colW, rowH, ".", ".", FT_CT_VALOR);
        ftCalcAddTecla(lista, &n, max, startX + 3 * (colW + gap), row, colW, rowH, "=", "=", FT_CT_IGUAL);
    }
    return n;
}

static void ftCalcMover(int direcao) {
    FtCalcTecla lista[FT_CALC_MAX_TECLAS];
    int n = ftCalcMontarTeclas(lista, FT_CALC_MAX_TECLAS);
    if (n <= 0) return;
    if (ftCalcBotao < 0 || ftCalcBotao >= n) ftCalcBotao = 0;
    int cx = lista[ftCalcBotao].x * 2 + lista[ftCalcBotao].w;
    int cy = lista[ftCalcBotao].y * 2 + lista[ftCalcBotao].h;
    int melhor = -1;
    long melhorNota = 0x7FFFFFFF;
    for (int i = 0; i < n; i++) {
        if (i == ftCalcBotao) continue;
        int dx = lista[i].x * 2 + lista[i].w - cx;
        int dy = lista[i].y * 2 + lista[i].h - cy;
        int principal = 0, secundario = 0;
        if (direcao == 0) { if (dx >= 0) continue; principal = -dx; secundario = dy < 0 ? -dy : dy; }
        else if (direcao == 1) { if (dx <= 0) continue; principal = dx; secundario = dy < 0 ? -dy : dy; }
        else if (direcao == 2) { if (dy >= 0) continue; principal = -dy; secundario = dx < 0 ? -dx : dx; }
        else { if (dy <= 0) continue; principal = dy; secundario = dx < 0 ? -dx : dx; }
        long nota = (long)principal + (long)secundario * 3L;
        if (nota < melhorNota) { melhorNota = nota; melhor = i; }
    }
    if (melhor >= 0) ftCalcBotao = melhor;
}

static void ftCalcPressionar(void) {
    FtCalcTecla lista[FT_CALC_MAX_TECLAS];
    int n = ftCalcMontarTeclas(lista, FT_CALC_MAX_TECLAS);
    if (n <= 0) return;
    if (ftCalcBotao < 0 || ftCalcBotao >= n) ftCalcBotao = 0;
    FtCalcTecla* t = &lista[ftCalcBotao];
    if (t->tipo == FT_CT_LIMPAR) ftCalcLimpar();
    else if (t->tipo == FT_CT_APAGAR) ftCalcApagar();
    else if (t->tipo == FT_CT_IGUAL) ftCalcIgual();
    else if (t->tipo == FT_CT_2ND) { ftCalc2nd = !ftCalc2nd; ftCalcBotao = 0; }
    else if (t->tipo == FT_CT_GRAU) { ftCalcGraus = !ftCalcGraus; ftCalcAtualizarPrevia(); }
    else if (t->tipo == FT_CT_CIENTIFICA) { ftCalcCientifica = !ftCalcCientifica; ftCalcBotao = 0; }
    else ftCalcInserir(t->token);
}

static void ftCalcIniciarCampos(int cat) {
    if (cat < 0 || cat >= FT_CAT_TOTAL || ftCalcCamposInit[cat]) return;
    double* c = ftCalcCampos[cat];
    for (int i = 0; i < FT_CALC_CAMPOS_MAX; i++) c[i] = 0.0;
    if (cat == FT_CAT_IDADE) { c[0] = 1; c[1] = 1; c[2] = 2000; }
    else if (cat == FT_CAT_IMC) { c[0] = 70; c[1] = 170; }
    else if (cat == FT_CAT_DATA) {
        struct tm t;
        ftDataHora(&t);
        c[0] = t.tm_mday; c[1] = t.tm_mon + 1; c[2] = t.tm_year + 1900;
        c[3] = t.tm_mday; c[4] = t.tm_mon + 1; c[5] = t.tm_year + 1900;
    }
    else if (cat == FT_CAT_DESCONTO) { c[0] = 100; c[1] = 10; }
    else if (cat == FT_CAT_BASE) { c[0] = 10; }
    else if (cat == FT_CAT_TEMPERATURA) { c[0] = 25; c[1] = 0; c[2] = 1; }
    else if (cat == FT_CAT_MOEDA) { c[0] = 100; c[1] = 0; c[2] = 1; }
    else if (cat == FT_CAT_CRIPTO) { c[0] = 1; c[1] = 1; c[2] = 0; }
    else if (cat == FT_CAT_INVESTIMENTO) { c[0] = 1000; c[1] = 100; c[2] = 10; c[3] = 12; }
    else if (cat == FT_CAT_EMPRESTIMO) { c[0] = 10000; c[1] = 2; c[2] = 12; }
    else { c[0] = 1; c[1] = 0; c[2] = 1; }
    ftCalcCamposInit[cat] = true;
}

static void ftCalcSetCampo(int cat, int campo, double valor) {
    if (cat < 0 || cat >= FT_CAT_TOTAL || campo < 0 || campo >= FT_CALC_CAMPOS_MAX) return;
    ftCalcIniciarCampos(cat);
    ftCalcCampos[cat][campo] = valor;
}

static bool ftCalcTemUnidades(int cat) {
    return cat == FT_CAT_AREA || cat == FT_CAT_DADOS || cat == FT_CAT_COMPRIMENTO ||
           cat == FT_CAT_MASSA || cat == FT_CAT_VELOCIDADE || cat == FT_CAT_TEMPERATURA ||
           cat == FT_CAT_TEMPO || cat == FT_CAT_VOLUME || cat == FT_CAT_MOEDA ||
           cat == FT_CAT_CRIPTO;
}

static const FtCalcUnidade* ftCalcTabela(int cat, int* total) {
    *total = 0;
    if (cat == FT_CAT_AREA) { *total = 6; return ftCalcTabArea; }
    if (cat == FT_CAT_DADOS) { *total = 5; return ftCalcTabDados; }
    if (cat == FT_CAT_COMPRIMENTO) { *total = 7; return ftCalcTabComprimento; }
    if (cat == FT_CAT_MASSA) { *total = 6; return ftCalcTabMassa; }
    if (cat == FT_CAT_VELOCIDADE) { *total = 4; return ftCalcTabVelocidade; }
    if (cat == FT_CAT_TEMPO) { *total = 5; return ftCalcTabTempo; }
    if (cat == FT_CAT_VOLUME) { *total = 6; return ftCalcTabVolume; }
    if (cat == FT_CAT_MOEDA) { *total = 6; return ftCalcTabMoeda; }
    if (cat == FT_CAT_CRIPTO) { *total = 10; return ftCalcTabCripto; }
    return NULL;
}

static int ftCalcUnidadeTotal(int cat) {
    if (cat == FT_CAT_TEMPERATURA) return 3;
    int total = 0;
    ftCalcTabela(cat, &total);
    return total;
}

static const char* ftCalcUnidadeNome(int cat, int idx) {
    if (cat == FT_CAT_TEMPERATURA) {
        if (idx < 0 || idx > 2) idx = 0;
        return ftCalcTempNomes[idx];
    }
    int total = 0;
    const FtCalcUnidade* tab = ftCalcTabela(cat, &total);
    if (!tab || total <= 0) return "";
    if (idx < 0 || idx >= total) idx = 0;
    return tab[idx].nome;
}

static double ftCalcConverter(int cat, double valor, int de, int para) {
    if (cat == FT_CAT_TEMPERATURA) {
        if (de < 0 || de > 2) de = 0;
        if (para < 0 || para > 2) para = 1;
        double c = de == 0 ? valor : (de == 1 ? (valor - 32.0) * 5.0 / 9.0 : valor - 273.15);
        if (para == 0) return c;
        if (para == 1) return c * 9.0 / 5.0 + 32.0;
        return c + 273.15;
    }
    int total = 0;
    const FtCalcUnidade* tab = ftCalcTabela(cat, &total);
    if (!tab || total <= 0) return valor;
    if (de < 0 || de >= total) de = 0;
    if (para < 0 || para >= total) para = 0;
    return valor * tab[de].fator / tab[para].fator;
}

static int ftCalcDiasNoMes(int mes, int ano) {
    static const int dias[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (mes < 1 || mes > 12) return 30;
    if (mes == 2 && ((ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0)) return 29;
    return dias[mes - 1];
}

static bool ftCalcDataValida(int dia, int mes, int ano) {
    return ano >= 1900 && ano <= 2100 && mes >= 1 && mes <= 12 && dia >= 1 && dia <= ftCalcDiasNoMes(mes, ano);
}

static time_t ftCalcDataTm(int dia, int mes, int ano) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_mday = dia;
    t.tm_mon = mes - 1;
    t.tm_year = ano - 1900;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    return mktime(&t);
}

static int ftCalcCampoTotal(int cat) {
    if (ftCalcTemUnidades(cat)) return 3;
    if (cat == FT_CAT_IDADE) return 3;
    if (cat == FT_CAT_IMC) return 2;
    if (cat == FT_CAT_DATA) return 6;
    if (cat == FT_CAT_DESCONTO) return 2;
    if (cat == FT_CAT_BASE) return 1;
    if (cat == FT_CAT_INVESTIMENTO) return 4;
    if (cat == FT_CAT_EMPRESTIMO) return 3;
    return 0;
}

static const char* ftCalcCampoRotulo(int cat, int idx) {
    if (ftCalcTemUnidades(cat)) {
        if (idx == 0) return "Valor";
        if (idx == 1) return "De";
        return "Para";
    }
    if (cat == FT_CAT_IDADE) {
        if (idx == 0) return "Dia de nascimento";
        if (idx == 1) return "Mes de nascimento";
        return "Ano de nascimento";
    }
    if (cat == FT_CAT_IMC) return idx == 0 ? "Peso (kg)" : "Altura (cm)";
    if (cat == FT_CAT_DATA) {
        if (idx == 0) return "Dia inicial";
        if (idx == 1) return "Mes inicial";
        if (idx == 2) return "Ano inicial";
        if (idx == 3) return "Dia final";
        if (idx == 4) return "Mes final";
        return "Ano final";
    }
    if (cat == FT_CAT_DESCONTO) return idx == 0 ? "Preco (R$)" : "Desconto (%)";
    if (cat == FT_CAT_BASE) return "Numero decimal";
    if (cat == FT_CAT_INVESTIMENTO) {
        if (idx == 0) return "Valor inicial (R$)";
        if (idx == 1) return "Aporte mensal (R$)";
        if (idx == 2) return "Juros ao ano (%)";
        return "Meses";
    }
    if (cat == FT_CAT_EMPRESTIMO) {
        if (idx == 0) return "Valor emprestado (R$)";
        if (idx == 1) return "Juros ao ano (%)";
        return "Meses";
    }
    return "";
}

static void ftCalcCampoTexto(int cat, int idx, char* saida, int tam) {
    ftCalcIniciarCampos(cat);
    double v = ftCalcCampos[cat][idx];
    if (ftCalcTemUnidades(cat) && idx >= 1) {
        snprintf(saida, tam, "%s", ftCalcUnidadeNome(cat, (int)v));
        return;
    }
    if (cat == FT_CAT_IDADE || cat == FT_CAT_DATA) {
        int pos = (cat == FT_CAT_DATA && idx >= 3) ? idx - 3 : idx;
        int valor = (int)(v + (v >= 0 ? 0.5 : -0.5));
        if (pos == 0) snprintf(saida, tam, "%02d", valor);
        else if (pos == 1) snprintf(saida, tam, "%02d", valor);
        else snprintf(saida, tam, "%04d", valor);
        return;
    }
    if (cat == FT_CAT_IMC) {
        if (idx == 0) snprintf(saida, tam, "%.1f", v);
        else snprintf(saida, tam, "%.0f", v);
        return;
    }
    if (cat == FT_CAT_DESCONTO) {
        if (idx == 0) snprintf(saida, tam, "%.2f", v);
        else snprintf(saida, tam, "%.1f", v);
        return;
    }
    if (cat == FT_CAT_BASE) { snprintf(saida, tam, "%.0f", v); return; }
    if (cat == FT_CAT_INVESTIMENTO) {
        if (idx == 0 || idx == 1) snprintf(saida, tam, "%.2f", v);
        else if (idx == 2) snprintf(saida, tam, "%.2f", v);
        else snprintf(saida, tam, "%.0f", v);
        return;
    }
    if (cat == FT_CAT_EMPRESTIMO) {
        if (idx == 0) snprintf(saida, tam, "%.2f", v);
        else if (idx == 1) snprintf(saida, tam, "%.2f", v);
        else snprintf(saida, tam, "%.0f", v);
        return;
    }
    if (cat == FT_CAT_MOEDA) snprintf(saida, tam, "%.2f", v);
    else snprintf(saida, tam, "%.6g", v);
}

static bool ftCalcCampoNumerico(int cat, int idx) {
    if (ftCalcTemUnidades(cat) && idx >= 1) return false;
    return true;
}

static void ftCalcAjustarCampo(int delta) {
    int cat = ftCalcCategoria;
    ftCalcIniciarCampos(cat);
    int total = ftCalcCampoTotal(cat);
    if (total <= 0) return;
    if (ftCalcCampo < 0 || ftCalcCampo >= total) ftCalcCampo = 0;
    double* c = ftCalcCampos[cat];
    int idx = ftCalcCampo;

    if (ftCalcTemUnidades(cat)) {
        if (idx >= 1) {
            int n = ftCalcUnidadeTotal(cat);
            int u = (int)(c[idx] + (c[idx] >= 0 ? 0.5 : -0.5));
            if (n > 0) u = (u + delta) % n;
            if (u < 0) u += n;
            c[idx] = u;
        }
        else {
            c[0] += delta;
            if (cat == FT_CAT_MOEDA || cat == FT_CAT_CRIPTO) { if (c[0] < 0) c[0] = 0; }
            else if (cat == FT_CAT_TEMPERATURA) { if (c[0] < -273.15) c[0] = -273.15; }
            else if (c[0] < 0) c[0] = 0;
        }
        return;
    }

    if (cat == FT_CAT_IDADE || cat == FT_CAT_DATA) {
        int base = (cat == FT_CAT_DATA && idx >= 3) ? 3 : 0;
        int sub = idx - base;
        int dia = (int)(c[base] + 0.5), mes = (int)(c[base + 1] + 0.5), ano = (int)(c[base + 2] + 0.5);
        if (sub == 0) {
            int maximo = ftCalcDiasNoMes(mes, ano);
            dia += delta;
            if (dia < 1) dia = maximo;
            if (dia > maximo) dia = 1;
            c[base] = dia;
        }
        else if (sub == 1) {
            mes += delta;
            while (mes < 1) mes += 12;
            while (mes > 12) mes -= 12;
            c[base + 1] = mes;
            int maximo = ftCalcDiasNoMes(mes, ano);
            if (dia > maximo) c[base] = maximo;
        }
        else {
            ano += delta;
            if (ano < 1900) ano = 1900;
            if (ano > 2100) ano = 2100;
            c[base + 2] = ano;
            int maximo = ftCalcDiasNoMes(mes, ano);
            if (dia > maximo) c[base] = maximo;
        }
        return;
    }

    if (cat == FT_CAT_IMC) {
        if (idx == 0) { c[0] += delta; if (c[0] < 1) c[0] = 1; if (c[0] > 500) c[0] = 500; }
        else { c[1] += delta; if (c[1] < 30) c[1] = 30; if (c[1] > 250) c[1] = 250; }
        return;
    }
    if (cat == FT_CAT_DESCONTO) {
        if (idx == 0) { c[0] += delta; if (c[0] < 0) c[0] = 0; }
        else { c[1] += delta; if (c[1] < 0) c[1] = 0; if (c[1] > 100) c[1] = 100; }
        return;
    }
    if (cat == FT_CAT_BASE) {
        c[0] += delta;
        if (c[0] < 0) c[0] = 0;
        if (c[0] > 9007199254740992.0) c[0] = 9007199254740992.0;
        return;
    }
    if (cat == FT_CAT_INVESTIMENTO) {
        if (idx == 0 || idx == 1) { c[idx] += delta * (idx == 0 ? 100 : 10); if (c[idx] < 0) c[idx] = 0; }
        else if (idx == 2) { c[idx] += delta * 0.1; if (c[idx] < -99) c[idx] = -99; if (c[idx] > 1000) c[idx] = 1000; }
        else { c[idx] += delta; if (c[idx] < 1) c[idx] = 1; if (c[idx] > 1200) c[idx] = 1200; }
        return;
    }
    if (cat == FT_CAT_EMPRESTIMO) {
        if (idx == 0) { c[idx] += delta * 100; if (c[idx] < 0) c[idx] = 0; }
        else if (idx == 1) { c[idx] += delta * 0.1; if (c[idx] < 0) c[idx] = 0; if (c[idx] > 1000) c[idx] = 1000; }
        else { c[idx] += delta; if (c[idx] < 1) c[idx] = 1; if (c[idx] > 1200) c[idx] = 1200; }
    }
}

static void ftCalcBinario(long long valor, char* saida, int tam) {
    unsigned long long u = valor < 0 ? (unsigned long long)(-valor) : (unsigned long long)valor;
    char temp[80];
    int n = 0;
    if (u == 0) temp[n++] = '0';
    while (u > 0 && n < 78) { temp[n++] = (char)('0' + (u & 1)); u >>= 1; }
    int p = 0;
    if (valor < 0 && p < tam - 1) saida[p++] = '-';
    while (n > 0 && p < tam - 1) saida[p++] = temp[--n];
    saida[p] = 0;
}

static int ftCalcResultados(int cat, char linhas[FT_CALC_LINHAS_MAX][128]) {
    ftCalcIniciarCampos(cat);
    double* c = ftCalcCampos[cat];
    int n = 0;
    if (ftCalcTemUnidades(cat)) {
        int total = ftCalcUnidadeTotal(cat);
        if (total <= 0) return 0;
        int de = (int)(c[1] + (c[1] >= 0 ? 0.5 : -0.5));
        int para = (int)(c[2] + (c[2] >= 0 ? 0.5 : -0.5));
        if (de < 0 || de >= total) de = 0;
        if (para < 0 || para >= total) para = 0;
        double r = ftCalcConverter(cat, c[0], de, para);
        snprintf(linhas[n++], 128, "%.6g %s = %.6g %s", c[0], ftCalcUnidadeNome(cat, de), r, ftCalcUnidadeNome(cat, para));
        if (cat == FT_CAT_MOEDA) snprintf(linhas[n++], 128, "Taxa de referencia; veja COTACOES ao vivo.");
        if (cat == FT_CAT_CRIPTO) snprintf(linhas[n++], 128, "Valor de referencia; veja COTACOES ao vivo.");
        return n;
    }
    if (cat == FT_CAT_IDADE) {
        int dia = (int)(c[0] + 0.5), mes = (int)(c[1] + 0.5), ano = (int)(c[2] + 0.5);
        if (!ftCalcDataValida(dia, mes, ano)) { snprintf(linhas[n++], 128, "Data de nascimento invalida."); return n; }
        struct tm hoje;
        ftDataHora(&hoje);
        int hd = hoje.tm_mday, hm = hoje.tm_mon + 1, ha = hoje.tm_year + 1900;
        if (ano > ha || (ano == ha && (mes > hm || (mes == hm && dia > hd)))) {
            snprintf(linhas[n++], 128, "A data de nascimento esta no futuro.");
            return n;
        }
        int anos = ha - ano, meses = hm - mes, dias = hd - dia;
        if (dias < 0) {
            meses--;
            int pm = hm - 1, pa = ha;
            if (pm == 0) { pm = 12; pa--; }
            dias += ftCalcDiasNoMes(pm, pa);
        }
        if (meses < 0) { anos--; meses += 12; }
        snprintf(linhas[n++], 128, "Idade: %d anos, %d meses e %d dias.", anos, meses, dias);
        snprintf(linhas[n++], 128, "Nascimento: %02d/%02d/%04d", dia, mes, ano);
        time_t t1 = ftCalcDataTm(dia, mes, ano);
        time_t t2 = ftCalcDataTm(hd, hm, ha);
        long long total = (long long)((double)(t2 - t1) / 86400.0 + 0.5);
        if (total >= 0) snprintf(linhas[n++], 128, "Total de dias vividos: %lld", total);
        return n;
    }
    if (cat == FT_CAT_IMC) {
        double peso = c[0], altura = c[1] / 100.0;
        if (peso <= 0 || altura <= 0) { snprintf(linhas[n++], 128, "Informe peso e altura validos."); return n; }
        double imc = peso / (altura * altura);
        const char* classe = "Obesidade grau III";
        if (imc < 18.5) classe = "Abaixo do peso";
        else if (imc < 25.0) classe = "Peso normal";
        else if (imc < 30.0) classe = "Sobrepeso";
        else if (imc < 35.0) classe = "Obesidade grau I";
        else if (imc < 40.0) classe = "Obesidade grau II";
        snprintf(linhas[n++], 128, "IMC: %.1f", imc);
        snprintf(linhas[n++], 128, "Classificacao: %s", classe);
        return n;
    }
    if (cat == FT_CAT_DATA) {
        int d1 = (int)(c[0] + 0.5), m1 = (int)(c[1] + 0.5), a1 = (int)(c[2] + 0.5);
        int d2 = (int)(c[3] + 0.5), m2 = (int)(c[4] + 0.5), a2 = (int)(c[5] + 0.5);
        if (!ftCalcDataValida(d1, m1, a1) || !ftCalcDataValida(d2, m2, a2)) {
            snprintf(linhas[n++], 128, "Data invalida.");
            return n;
        }
        time_t t1 = ftCalcDataTm(d1, m1, a1);
        time_t t2 = ftCalcDataTm(d2, m2, a2);
        long long dias = (long long)((double)(t2 - t1) / 86400.0 + (t2 >= t1 ? 0.5 : -0.5));
        snprintf(linhas[n++], 128, "Diferenca: %lld dias", dias);
        long long absd = dias < 0 ? -dias : dias;
        snprintf(linhas[n++], 128, "Semanas: %lld e %lld dias", absd / 7, absd % 7);
        snprintf(linhas[n++], 128, "%02d/%02d/%04d ate %02d/%02d/%04d", d1, m1, a1, d2, m2, a2);
        return n;
    }
    if (cat == FT_CAT_DESCONTO) {
        double preco = c[0], pct = c[1];
        double final = preco * (1.0 - pct / 100.0);
        snprintf(linhas[n++], 128, "Preco final: R$ %.2f", final);
        snprintf(linhas[n++], 128, "Economia: R$ %.2f", preco - final);
        return n;
    }
    if (cat == FT_CAT_BASE) {
        long long v = (long long)(c[0] + (c[0] >= 0 ? 0.5 : -0.5));
        char bin[96];
        ftCalcBinario(v, bin, sizeof(bin));
        snprintf(linhas[n++], 128, "Decimal: %lld", v);
        snprintf(linhas[n++], 128, "Binario: %s", bin);
        snprintf(linhas[n++], 128, "Octal: %llo", (unsigned long long)v);
        snprintf(linhas[n++], 128, "Hexadecimal: %llx", (unsigned long long)v);
        return n;
    }
    if (cat == FT_CAT_INVESTIMENTO) {
        double inicial = c[0], aporte = c[1], taxaAno = c[2], meses = c[3];
        int m = (int)(meses + 0.5);
        if (m < 1) { snprintf(linhas[n++], 128, "Informe os meses."); return n; }
        double i = taxaAno / 1200.0;
        if (i <= -1.0) { snprintf(linhas[n++], 128, "Taxa invalida."); return n; }
        double fator = pow(1.0 + i, m);
        double montante = fabs(i) < 0.000000000001 ? inicial + aporte * m : inicial * fator + aporte * (fator - 1.0) / i;
        double investido = inicial + aporte * m;
        snprintf(linhas[n++], 128, "Montante: R$ %.2f", montante);
        snprintf(linhas[n++], 128, "Investido: R$ %.2f", investido);
        snprintf(linhas[n++], 128, "Juros: R$ %.2f", montante - investido);
        return n;
    }
    if (cat == FT_CAT_EMPRESTIMO) {
        double valor = c[0], taxaAno = c[1], meses = c[2];
        int m = (int)(meses + 0.5);
        if (valor <= 0 || m < 1) { snprintf(linhas[n++], 128, "Informe valor e meses."); return n; }
        double i = taxaAno / 1200.0;
        if (i <= -1.0) { snprintf(linhas[n++], 128, "Taxa invalida."); return n; }
        double parcela = fabs(i) < 0.000000000001 ? valor / m : valor * i / (1.0 - pow(1.0 + i, -m));
        double total = parcela * m;
        snprintf(linhas[n++], 128, "Parcela: R$ %.2f", parcela);
        snprintf(linhas[n++], 128, "Total: R$ %.2f", total);
        snprintf(linhas[n++], 128, "Juros: R$ %.2f", total - valor);
        return n;
    }
    return n;
}

// ---------------------------------------------------------------- cotacoes e acoes
#define FT_QUOTE_MAX 40
#define FT_QUOTE_PONTOS 512
#define FT_QUOTE_FONTE_YAHOO 0
#define FT_QUOTE_FONTE_COINGECKO 1
#define FT_QUOTE_FONTE_NENHUMA 2

struct FtQuote {
    const char* simbolo;
    const char* nome;
    const char* yahoo;
    int fonte;
    char preco[32];
    char variacao[32];
    double pontos[FT_QUOTE_PONTOS];
    int total;
    double minimo, maximo;
    double quando;
    int erro;
    bool ok;
    volatile bool baixando;
};

struct FtQuoteDef {
    const char* simbolo;
    const char* nome;
    const char* yahoo;
    int grupo;
    int fonte;
};

static const FtQuoteDef ftQuoteDefs[] = {
    { "USD/BRL", "Dolar americano", "BRL=X", 0, FT_QUOTE_FONTE_YAHOO },
    { "EUR/BRL", "Euro", "EURBRL=X", 0, FT_QUOTE_FONTE_YAHOO },
    { "GBP/BRL", "Libra esterlina", "GBPBRL=X", 0, FT_QUOTE_FONTE_YAHOO },
    { "ARS/BRL", "Peso argentino", "ARSBRL=X", 0, FT_QUOTE_FONTE_YAHOO },
    { "JPY/BRL", "Iene", "JPYBRL=X", 0, FT_QUOTE_FONTE_YAHOO },
    { "BNB/BRL", "BNB", "binancecoin", 0, FT_QUOTE_FONTE_COINGECKO },
    { "BTC/BRL", "Bitcoin", "bitcoin", 0, FT_QUOTE_FONTE_COINGECKO },
    { "ETH/BRL", "Ethereum", "ethereum", 0, FT_QUOTE_FONTE_COINGECKO },
    { "SOL/BRL", "Solana", "solana", 0, FT_QUOTE_FONTE_COINGECKO },
    { "XRP/BRL", "XRP", "ripple", 0, FT_QUOTE_FONTE_COINGECKO },
    { "DOGE/BRL", "Dogecoin", "dogecoin", 0, FT_QUOTE_FONTE_COINGECKO },
    { "USDT/BRL", "Tether", "tether", 0, FT_QUOTE_FONTE_COINGECKO },
    { "TTWO", "Take-Two Interactive", "TTWO", 1, FT_QUOTE_FONTE_YAHOO },
    { "ROCKSTAR", "Rockstar (Take-Two)", "TTWO", 1, FT_QUOTE_FONTE_YAHOO },
    { "RBLX", "Roblox", "RBLX", 1, FT_QUOTE_FONTE_YAHOO },
    { "EA", "Electronic Arts", "EA", 1, FT_QUOTE_FONTE_YAHOO },
    { "NTDOY", "Nintendo", "NTDOY", 1, FT_QUOTE_FONTE_YAHOO },
    { "SONY", "Sony", "SONY", 1, FT_QUOTE_FONTE_YAHOO },
    { "U", "Unity", "U", 1, FT_QUOTE_FONTE_YAHOO },
    { "TCEHY", "Tencent", "TCEHY", 1, FT_QUOTE_FONTE_YAHOO },
    { "MSFT", "Microsoft / Xbox", "MSFT", 1, FT_QUOTE_FONTE_YAHOO },
    { "AAPL", "Apple", "AAPL", 1, FT_QUOTE_FONTE_YAHOO },
    { "NVDA", "Nvidia", "NVDA", 1, FT_QUOTE_FONTE_YAHOO },
    { "GOOGL", "Google", "GOOGL", 1, FT_QUOTE_FONTE_YAHOO },
    { "META", "Facebook / Meta", "META", 1, FT_QUOTE_FONTE_YAHOO },
    { "MELI", "Mercado Livre", "MELI", 1, FT_QUOTE_FONTE_YAHOO },
    { "SE", "Sea Ltd (Shopee)", "SE", 1, FT_QUOTE_FONTE_YAHOO },
    { "NU", "Nubank", "NU", 1, FT_QUOTE_FONTE_YAHOO },
    { "BYTEDANCE", "TikTok (ByteDance, privada)", "", 1, FT_QUOTE_FONTE_NENHUMA },
    { "PETR4", "Petrobras PN", "PETR4.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "VALE3", "Vale ON", "VALE3.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "ITUB4", "Itau PN", "ITUB4.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "BBDC4", "Bradesco PN", "BBDC4.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "BBAS3", "Banco do Brasil", "BBAS3.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "ABEV3", "Ambev ON", "ABEV3.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "MGLU3", "Magazine Luiza", "MGLU3.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "WEGE3", "WEG ON", "WEGE3.SA", 1, FT_QUOTE_FONTE_YAHOO },
    { "TSLA", "Tesla", "TSLA", 1, FT_QUOTE_FONTE_YAHOO }
};

static FtQuote ftQuotes[FT_QUOTE_MAX];
static int ftQuoteAtivos[FT_QUOTE_MAX];
static int ftQuoteAtivosTotal = 0;
static int ftQuoteTotal = 0;
static int ftQuoteSel = 0;
static int ftQuoteOff = 0;
static int ftQuoteGrupo = -1;
static int ftQuotePeriodo = 0;
static int ftQuoteDias = 30;
static long long ftQuoteInicioEpoch = 0;
static long long ftQuoteFimEpoch = 0;
static bool ftQuoteCalAberto = false;
static int ftQuoteCalEtapa = 0;
static int ftQuoteCalAno = 0;
static int ftQuoteCalMes = 0;
static int ftQuoteCalDia = 1;

static int ftQuoteDefTotal(void) {
    return (int)(sizeof(ftQuoteDefs) / sizeof(ftQuoteDefs[0]));
}

static int ftQuoteDefIndex(int grupo, const char* simbolo) {
    if (!simbolo) return -1;
    for (int i = 0; i < ftQuoteDefTotal(); i++) {
        if (ftQuoteDefs[i].grupo == grupo && !strcmp(ftQuoteDefs[i].simbolo, simbolo)) return i;
    }
    return -1;
}

static const char* ftQuoteArquivoLista(int grupo) {
    return grupo == 0 ? FT_CONF "/cotacoes_lista.txt" : FT_CONF "/acoes_lista.txt";
}

static void ftQuoteListaPadrao(int grupo, char lista[FT_QUOTE_MAX][64], int* total) {
    static const char* g0[] = {
        "USD/BRL", "EUR/BRL", "GBP/BRL", "ARS/BRL", "JPY/BRL",
        "BNB/BRL", "BTC/BRL", "ETH/BRL", "SOL/BRL", "XRP/BRL", "DOGE/BRL", "USDT/BRL"
    };
    static const char* g1[] = {
        "TTWO", "ROCKSTAR", "RBLX", "EA", "NTDOY", "SONY", "U", "TCEHY", "MSFT",
        "AAPL", "NVDA", "GOOGL", "META", "MELI", "SE", "NU", "BYTEDANCE"
    };
    const char** src = grupo == 0 ? g0 : g1;
    int n = grupo == 0 ? (int)(sizeof(g0) / sizeof(g0[0])) : (int)(sizeof(g1) / sizeof(g1[0]));
    *total = 0;
    for (int i = 0; i < n && *total < FT_QUOTE_MAX; i++) {
        snprintf(lista[*total], 64, "%s", src[i]);
        (*total)++;
    }
}

static void ftQuoteSalvarLista(int grupo) {
    ftMkdirs(FT_CONF);
    FILE* f = fopen(ftQuoteArquivoLista(grupo), "w");
    if (!f) return;
    for (int i = 0; i < ftQuoteAtivosTotal; i++) {
        int idx = ftQuoteAtivos[i];
        if (idx >= 0 && idx < ftQuoteDefTotal()) fprintf(f, "%s\n", ftQuoteDefs[idx].simbolo);
    }
    fclose(f);
}

static void ftQuoteCarregarLista(int grupo) {
    char lista[FT_QUOTE_MAX][64];
    int total = 0;
    ftQuoteAtivosTotal = 0;
    ftMkdirs(FT_CONF);
    FILE* f = fopen(ftQuoteArquivoLista(grupo), "r");
    if (f) {
        char linha[128];
        while (fgets(linha, sizeof(linha), f) && ftQuoteAtivosTotal < FT_QUOTE_MAX) {
            char* s = linha;
            while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
            char* e = s + strlen(s);
            while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
            *e = 0;
            int idx = ftQuoteDefIndex(grupo, s);
            if (idx >= 0) ftQuoteAtivos[ftQuoteAtivosTotal++] = idx;
        }
        fclose(f);
    }
    if (ftQuoteAtivosTotal == 0) {
        ftQuoteListaPadrao(grupo, lista, &total);
        for (int i = 0; i < total; i++) {
            int idx = ftQuoteDefIndex(grupo, lista[i]);
            if (idx >= 0) ftQuoteAtivos[ftQuoteAtivosTotal++] = idx;
        }
        ftQuoteSalvarLista(grupo);
    }
}

static void ftQuoteIniciarGrupo(int grupo) {
    int periodo = ftQuotePeriodo, dias = ftQuoteDias;
    ftQuoteGrupo = grupo;
    ftQuoteTotal = 0;
    ftQuoteSel = 0;
    ftQuoteOff = 0;
    ftQuotePeriodo = periodo;
    ftQuoteDias = dias;
    ftQuoteCarregarLista(grupo);
    for (int i = 0; i < ftQuoteAtivosTotal && ftQuoteTotal < FT_QUOTE_MAX; i++) {
        const FtQuoteDef* def = &ftQuoteDefs[ftQuoteAtivos[i]];
        FtQuote* q = &ftQuotes[ftQuoteTotal++];
        memset(q, 0, sizeof(*q));
        q->simbolo = def->simbolo;
        q->nome = def->nome;
        q->yahoo = def->yahoo;
        q->fonte = def->fonte;
        snprintf(q->preco, sizeof(q->preco), "--");
        snprintf(q->variacao, sizeof(q->variacao), "--");
    }
}

static void ftQuoteUrl(const FtQuote* q, char* url, int tam) {
    int dias = ftQuotePeriodo == 0 ? 1 : (ftQuotePeriodo == 1 ? 30 : (ftQuotePeriodo == 2 ? 365 : ftQuoteDias));
    if (ftQuotePeriodo == 3 && ftQuoteInicioEpoch > 0 && ftQuoteFimEpoch > ftQuoteInicioEpoch) {
        long long calculado = (ftQuoteFimEpoch - ftQuoteInicioEpoch) / 86400LL;
        if (calculado > 0) dias = calculado > 3650 ? 3650 : (int)calculado;
    }
    if (dias < 1) dias = 1;
    if (dias > 3650) dias = 3650;
    if (q->fonte == FT_QUOTE_FONTE_COINGECKO) {
        snprintf(url, tam, "https://api.coingecko.com/api/v3/coins/%s/market_chart?vs_currency=brl&days=%d%s", q->yahoo, dias, dias >= 2 ? "&interval=daily" : "");
        return;
    }
    char query[180];
    if (ftQuotePeriodo == 0) snprintf(query, sizeof(query), "range=1d&interval=5m");
    else if (ftQuotePeriodo == 1) snprintf(query, sizeof(query), "range=1mo&interval=1d");
    else if (ftQuotePeriodo == 2) snprintf(query, sizeof(query), "range=1y&interval=1wk");
    else {
        const char* intervalo = dias <= 2 ? "5m" : (dias <= 45 ? "1d" : (dias <= 400 ? "1wk" : "1mo"));
        long long fim = ftQuoteFimEpoch > 0 ? ftQuoteFimEpoch : (long long)time(NULL);
        long long inicio = ftQuoteInicioEpoch > 0 ? ftQuoteInicioEpoch : fim - (long long)dias * 86400LL;
        if (inicio >= fim) inicio = fim - 86400LL;
        snprintf(query, sizeof(query), "period1=%lld&period2=%lld&interval=%s", inicio, fim, intervalo);
    }
    snprintf(url, tam, "https://query1.finance.yahoo.com/v8/finance/chart/%s?%s&includePrePost=false", q->yahoo, query);
}

static bool ftQuoteNumero(const char* base, const char* campo, double* valor) {
    if (!base || !campo) return false;
    const char* p = strstr(base, campo);
    if (!p) return false;
    p += strlen(campo);
    while (*p == ' ' || *p == ':') p++;
    if (strncmp(p, "null", 4) == 0) return false;
    char* fim = NULL;
    double v = strtod(p, &fim);
    if (fim == p) return false;
    *valor = v;
    return true;
}

static void ftQuotePrecoTexto(double v, char* saida, int tam) {
    if (v >= 1000.0) snprintf(saida, tam, "%.2f", v);
    else if (v >= 1.0) snprintf(saida, tam, "%.4f", v);
    else snprintf(saida, tam, "%.6f", v);
}

static void* ftQuoteThread(void* arg) {
    int cod = (int)(intptr_t)arg;
    int grupo = cod >> 8;
    int idx = cod & 255;
    if (grupo != ftQuoteGrupo || idx < 0 || idx >= ftQuoteTotal) return NULL;
    FtQuote* q = &ftQuotes[idx];
    char url[600];
    ftQuoteUrl(q, url, sizeof(url));
    char* r = NULL;
    size_t tam = 0;
    int st = hnHttpGet(url, &r, &tam);
    if (grupo != ftQuoteGrupo) { free(r); return NULL; }
    q->ok = false;
    if (st == 200 && r) {
        q->total = 0;
        q->minimo = 0.0;
        q->maximo = 0.0;
        double preco = 0.0, anterior = 0.0, pct = 0.0;
        bool temPct = false;
        if (q->fonte == 1) {
            const char* prices = strstr(r, "\"prices\":[");
            if (prices) {
                const char* p = prices + 10;
                while (*p && q->total < FT_QUOTE_PONTOS) {
                    while (*p == ' ' || *p == ',' || *p == '[' || *p == '\n' || *p == '\r' ||
                           (*p == ']' && p[1] == ',')) p++;
                    if (!*p || *p == ']') break;
                    char* fim = NULL;
                    double ts = strtod(p, &fim);
                    if (fim == p) break;
                    p = fim;
                    while (*p == ' ' || *p == ',') p++;
                    if (!*p || *p == ']') break;
                    double v = strtod(p, &fim);
                    if (fim == p) break;
                    p = fim;
                    (void)ts;
                    if (q->total == 0) { q->minimo = v; q->maximo = v; }
                    else {
                        if (v < q->minimo) q->minimo = v;
                        if (v > q->maximo) q->maximo = v;
                    }
                    q->pontos[q->total++] = v;
                }
            }
            if (q->total > 0) {
                preco = q->pontos[q->total - 1];
                anterior = q->pontos[0];
            }
        }
        else {
            const char* meta = strstr(r, "\"meta\":{");
            bool temPreco = meta && ftQuoteNumero(meta, "\"regularMarketPrice\"", &preco);
            if (meta) {
                ftQuoteNumero(meta, "\"chartPreviousClose\"", &anterior);
                temPct = ftQuoteNumero(meta, "\"regularMarketChangePercent\"", &pct);
            }
            const char* close = strstr(r, "\"close\":[");
            if (close) {
                const char* p = close + 9;
                while (*p && *p != ']' && q->total < FT_QUOTE_PONTOS) {
                    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
                    if (!*p || *p == ']') break;
                    if (strncmp(p, "null", 4) == 0) { p += 4; continue; }
                    char* fim = NULL;
                    double v = strtod(p, &fim);
                    if (fim == p) break;
                    p = fim;
                    if (q->total == 0) { q->minimo = v; q->maximo = v; }
                    else {
                        if (v < q->minimo) q->minimo = v;
                        if (v > q->maximo) q->maximo = v;
                    }
                    q->pontos[q->total++] = v;
                }
            }
            if (!temPreco && q->total > 0) preco = q->pontos[q->total - 1];
        }
        if (preco > 0.0) {
            ftQuotePrecoTexto(preco, q->preco, sizeof(q->preco));
            if (temPct) snprintf(q->variacao, sizeof(q->variacao), "%+.2f%%", pct);
            else if (anterior > 0.0) snprintf(q->variacao, sizeof(q->variacao), "%+.2f%%", (preco - anterior) * 100.0 / anterior);
            else snprintf(q->variacao, sizeof(q->variacao), "--");
            q->ok = true;
        }
        q->erro = q->ok ? 0 : -1;
    }
    else {
        q->erro = st;
        q->ok = false;
    }
    free(r);
    q->quando = ftSegundos();
    q->baixando = false;
    return NULL;
}

static void ftQuoteAtualizar(int idx) {
    if (idx < 0 || idx >= ftQuoteTotal || ftQuotes[idx].baixando) return;
    FtQuote* q = &ftQuotes[idx];
    if (q->fonte == FT_QUOTE_FONTE_NENHUMA) {
        q->quando = ftSegundos();
        q->ok = false;
        q->erro = 0;
        return;
    }
    q->baixando = true;
    q->erro = 0;
    int cod = (ftQuoteGrupo << 8) | idx;
    pthread_t th;
    if (pthread_create(&th, NULL, ftQuoteThread, (void*)(intptr_t)cod) != 0) q->baixando = false;
    else pthread_detach(th);
}

static void ftQuoteAtualizarTodos(void) {
    for (int i = 0; i < ftQuoteTotal; i++) ftQuoteAtualizar(i);
}

static void ftQuoteRecarregar(int grupo) {
    int sel = ftQuoteSel;
    int periodo = ftQuotePeriodo, dias = ftQuoteDias;
    ftQuoteSalvarLista(grupo);
    ftQuoteIniciarGrupo(grupo);
    ftQuotePeriodo = periodo;
    ftQuoteDias = dias;
    if (sel < 0) sel = 0;
    if (sel >= ftQuoteTotal) sel = ftQuoteTotal - 1;
    ftQuoteSel = sel;
    ftQuoteAtualizarTodos();
}

static uint32_t* ftTelaPixels = NULL;
static char ftQuoteClipboard[256] = "";
static char ftQuoteAviso[160] = "";

enum {
    FT_QMENU_NENHUM = 0,
    FT_QMENU_COPIAR,
    FT_QMENU_OPCOES,
    FT_QMENU_BUSCA
};

static int ftQuoteMenu = FT_QMENU_NENHUM;
static int ftQuoteMenuSel = 0;
static bool ftQuotePrintPendente = false;
static char ftQuoteBuscaTexto[64] = "";
static int ftQuoteBuscaRes[FT_QUOTE_MAX];
static int ftQuoteBuscaTotal = 0;
static int ftQuoteBuscaSel = 0;

static bool ftQuoteContemSemCaixa(const char* texto, const char* procurado) {
    if (!texto || !procurado || !procurado[0]) return true;
    char a[128], b[64];
    int n = 0;
    for (; texto[n] && n < 127; n++) a[n] = (char)tolower((unsigned char)texto[n]);
    a[n] = 0;
    n = 0;
    for (; procurado[n] && n < 63; n++) b[n] = (char)tolower((unsigned char)procurado[n]);
    b[n] = 0;
    return strstr(a, b) != NULL;
}

static void ftQuoteCopiarAtual(void) {
    if (ftQuoteSel < 0 || ftQuoteSel >= ftQuoteTotal) return;
    FtQuote* q = &ftQuotes[ftQuoteSel];
    if (q->ok) snprintf(ftQuoteClipboard, sizeof(ftQuoteClipboard), "%s - %s: R$ %s (%s)", q->simbolo, q->nome, q->preco, q->variacao);
    else snprintf(ftQuoteClipboard, sizeof(ftQuoteClipboard), "%s - %s: sem cotacao", q->simbolo, q->nome);
    snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Copiado: %.120s", ftQuoteClipboard);
}

static bool ftQuotePrintTela(void) {
    if (!ftTelaPixels) {
        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Print indisponivel neste quadro.");
        return false;
    }
    ftMkdirs(FT_BAIXADO);
    struct tm t;
    ftDataHora(&t);
    char nome[80], caminho[320];
    snprintf(nome, sizeof(nome), "print_%04d%02d%02d_%02d%02d%02d.png",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    snprintf(caminho, sizeof(caminho), "%s/%s", FT_BAIXADO, nome);
    png_image imagem;
    memset(&imagem, 0, sizeof(imagem));
    imagem.version = PNG_IMAGE_VERSION;
    imagem.width = FT_LARGURA;
    imagem.height = FT_ALTURA;
    imagem.format = PNG_FORMAT_BGRA;
    int ok = png_image_write_to_file(&imagem, caminho, 0, ftTelaPixels, FT_LARGURA * 4, NULL);
    if (ok) snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Print salvo em baixado/%s", nome);
    else snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Falha ao salvar o print.");
    return ok != 0;
}

static void ftQuoteBuscarTitulos(const char* texto) {
    snprintf(ftQuoteBuscaTexto, sizeof(ftQuoteBuscaTexto), "%s", texto ? texto : "");
    ftQuoteBuscaTotal = 0;
    ftQuoteBuscaSel = 0;
    for (int i = 0; i < ftQuoteDefTotal() && ftQuoteBuscaTotal < FT_QUOTE_MAX; i++) {
        const FtQuoteDef* d = &ftQuoteDefs[i];
        if (d->grupo != ftQuoteGrupo) continue;
        if (ftQuoteContemSemCaixa(d->nome, texto) || ftQuoteContemSemCaixa(d->simbolo, texto)) {
            ftQuoteBuscaRes[ftQuoteBuscaTotal++] = i;
        }
    }
    if (ftQuoteBuscaTotal > 0) ftQuoteMenu = FT_QMENU_BUSCA;
    else {
        ftQuoteMenu = FT_QMENU_NENHUM;
        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Nenhum titulo encontrado.");
    }
}

static void ftQuoteAdicionarDef(int defIdx) {
    if (defIdx < 0 || defIdx >= ftQuoteDefTotal()) return;
    int jaTem = 0;
    for (int i = 0; i < ftQuoteAtivosTotal; i++) if (ftQuoteAtivos[i] == defIdx) jaTem = 1;
    if (jaTem) {
        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Esse titulo ja esta na lista.");
        return;
    }
    if (ftQuoteAtivosTotal >= FT_QUOTE_MAX) {
        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Lista cheia. Remova algum titulo.");
        return;
    }
    ftQuoteAtivos[ftQuoteAtivosTotal++] = defIdx;
    int novo = ftQuoteAtivosTotal - 1;
    ftQuoteRecarregar(ftQuoteGrupo);
    ftQuoteSel = novo;
    snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Adicionado: %s", ftQuoteDefs[defIdx].simbolo);
}

static void ftQuoteRemoverAtual(void) {
    if (ftQuoteSel < 0 || ftQuoteSel >= ftQuoteAtivosTotal) return;
    int removido = ftQuoteAtivos[ftQuoteSel];
    for (int i = ftQuoteSel; i + 1 < ftQuoteAtivosTotal; i++) ftQuoteAtivos[i] = ftQuoteAtivos[i + 1];
    ftQuoteAtivosTotal--;
    ftQuoteRecarregar(ftQuoteGrupo);
    snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Removido: %s", ftQuoteDefs[removido].simbolo);
}

static void ftQuoteMoverAtual(int delta) {
    if (ftQuoteSel < 0 || ftQuoteSel >= ftQuoteAtivosTotal) return;
    int novo = ftQuoteSel + delta;
    if (novo < 0 || novo >= ftQuoteAtivosTotal) return;
    int tmp = ftQuoteAtivos[ftQuoteSel];
    ftQuoteAtivos[ftQuoteSel] = ftQuoteAtivos[novo];
    ftQuoteAtivos[novo] = tmp;
    ftQuoteRecarregar(ftQuoteGrupo);
    ftQuoteSel = novo;
    snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Posicao: %d de %d", novo + 1, ftQuoteAtivosTotal);
}

static int ftQuoteDiasNoMes(int ano, int mes) {
    while (mes < 0) { mes += 12; ano--; }
    while (mes > 11) { mes -= 12; ano++; }
    if (mes == 3 || mes == 5 || mes == 8 || mes == 10) return 30;
    if (mes == 1) return ((ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0) ? 29 : 28;
    return 31;
}

static long long ftQuoteEpochData(int ano, int mes, int dia) {
    while (mes < 0) { mes += 12; ano--; }
    while (mes > 11) { mes -= 12; ano++; }
    int maximo = ftQuoteDiasNoMes(ano, mes);
    if (dia < 1) dia = 1;
    if (dia > maximo) dia = maximo;
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = ano - 1900;
    t.tm_mon = mes;
    t.tm_mday = dia;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    return (long long)mktime(&t);
}

static void ftQuoteEpochParaDatas(long long epoch, int* ano, int* mes, int* dia) {
    time_t bruto = (time_t)epoch;
    struct tm* t = localtime(&bruto);
    if (t) {
        *ano = t->tm_year + 1900;
        *mes = t->tm_mon;
        *dia = t->tm_mday;
    }
    else {
        struct tm agora;
        ftDataHora(&agora);
        *ano = agora.tm_year + 1900;
        *mes = agora.tm_mon;
        *dia = agora.tm_mday;
    }
}

static void ftQuoteTmParaCal(const struct tm* t) {
    ftQuoteCalAno = t->tm_year + 1900;
    ftQuoteCalMes = t->tm_mon;
    ftQuoteCalDia = t->tm_mday;
}

static void ftQuoteEpochParaCal(long long epoch) {
    time_t bruto = (time_t)epoch;
    struct tm* t = localtime(&bruto);
    if (t) ftQuoteTmParaCal(t);
    else {
        struct tm agora;
        ftDataHora(&agora);
        ftQuoteTmParaCal(&agora);
    }
}

static void ftQuoteFormatarData(long long epoch, char* saida, int tam) {
    time_t bruto = (time_t)epoch;
    struct tm* t = localtime(&bruto);
    if (t) snprintf(saida, tam, "%02d/%02d/%04d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    else snprintf(saida, tam, "--/--/----");
}

static void ftQuoteAbrirCalendario(int etapa) {
    long long epoch;
    if (etapa == 0) {
        epoch = ftQuoteInicioEpoch;
        if (epoch <= 0) epoch = (long long)time(NULL) - (long long)ftQuoteDias * 86400LL;
    }
    else {
        epoch = ftQuoteFimEpoch;
        if (epoch <= 0) epoch = ftQuoteInicioEpoch > 0 ? ftQuoteInicioEpoch : (long long)time(NULL);
    }
    ftQuoteCalEtapa = etapa;
    ftQuoteEpochParaCal(epoch);
    ftQuoteCalAberto = true;
    ftQuoteMenu = FT_QMENU_NENHUM;
    snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Escolha a data de %s.", etapa == 0 ? "inicio" : "fim");
}

static void ftQuoteCalMover(int tipo, int delta) {
    if (tipo == 0) {
        long long epoch = ftQuoteEpochData(ftQuoteCalAno, ftQuoteCalMes, ftQuoteCalDia) + (long long)delta * 86400LL;
        ftQuoteEpochParaCal(epoch);
    }
    else if (tipo == 1) {
        ftQuoteCalMes += delta;
        while (ftQuoteCalMes < 0) { ftQuoteCalMes += 12; ftQuoteCalAno--; }
        while (ftQuoteCalMes > 11) { ftQuoteCalMes -= 12; ftQuoteCalAno++; }
        int maximo = ftQuoteDiasNoMes(ftQuoteCalAno, ftQuoteCalMes);
        if (ftQuoteCalDia > maximo) ftQuoteCalDia = maximo;
    }
    else {
        ftQuoteCalAno += delta;
        if (ftQuoteCalAno < 2000) ftQuoteCalAno = 2000;
        if (ftQuoteCalAno > 2100) ftQuoteCalAno = 2100;
        int maximo = ftQuoteDiasNoMes(ftQuoteCalAno, ftQuoteCalMes);
        if (ftQuoteCalDia > maximo) ftQuoteCalDia = maximo;
    }
}

static void ftQuoteCalConfirmar(void) {
    long long epoch = ftQuoteEpochData(ftQuoteCalAno, ftQuoteCalMes, ftQuoteCalDia);
    char data[32];
    ftQuoteFormatarData(epoch, data, sizeof(data));
    if (ftQuoteCalEtapa == 0) {
        ftQuoteInicioEpoch = epoch;
        if (ftQuoteFimEpoch > 0 && ftQuoteFimEpoch <= epoch) ftQuoteFimEpoch = 0;
        ftQuoteAbrirCalendario(1);
        return;
    }
    if (ftQuoteInicioEpoch <= 0 || epoch <= ftQuoteInicioEpoch) {
        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "A data de fim deve ser maior que a de inicio.");
        return;
    }
    ftQuoteFimEpoch = epoch;
    ftQuoteDias = (int)((ftQuoteFimEpoch - ftQuoteInicioEpoch) / 86400LL);
    if (ftQuoteDias < 1) ftQuoteDias = 1;
    if (ftQuoteDias > 3650) ftQuoteDias = 3650;
    ftQuoteCalAberto = false;
    char inicio[32];
    ftQuoteFormatarData(ftQuoteInicioEpoch, inicio, sizeof(inicio));
    snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Periodo: %s ate %s", inicio, data);
    ftQuoteAtualizar(ftQuoteSel);
}

static void ftQuoteAbrir(int grupo) {
    if (grupo != ftQuoteGrupo) {
        ftQuoteIniciarGrupo(grupo);
        ftQuoteAtualizarTodos();
        return;
    }
    if (ftQuoteTotal <= 0) return;
    if (ftQuoteSel < 0 || ftQuoteSel >= ftQuoteTotal) ftQuoteSel = 0;
    if (!ftQuotes[ftQuoteSel].baixando && ftSegundos() - ftQuotes[ftQuoteSel].quando > 60.0) ftQuoteAtualizar(ftQuoteSel);
}

static int ftCalcCategoriaInicio(void) {
    return ftCalcRecurso == 2 ? FT_CAT_MOEDA : (ftCalcRecurso == 1 ? FT_CAT_IDADE : 0);
}

static int ftCalcCategoriaTotal(void) {
    if (ftCalcRecurso == 1) return 13;
    if (ftCalcRecurso == 2) return 4;
    return 0;
}

static void ftCalcTrocarRecurso(int passo) {
    ftCalcRecurso = (ftCalcRecurso + passo + 5) % 5;
    ftCalcListaAberta = false;
    ftQuoteMenu = FT_QMENU_NENHUM;
    if (ftCalcRecurso == 1) ftCalcCategoria = FT_CAT_IDADE;
    else if (ftCalcRecurso == 2) ftCalcCategoria = FT_CAT_MOEDA;
    else ftCalcCategoria = FT_CAT_IDADE;
    ftCalcCampo = 0;
    if (ftCalcRecurso == 3) ftQuoteAbrir(0);
    else if (ftCalcRecurso == 4) ftQuoteAbrir(1);
    ftCalcIniciarCampos(ftCalcCategoria);
}

static void ftCalcTrocarCategoria(int passo) {
    int inicio = ftCalcCategoriaInicio();
    int total = ftCalcCategoriaTotal();
    if (total <= 0) return;
    int atual = ftCalcCategoria - inicio;
    if (atual < 0 || atual >= total) atual = 0;
    atual = (atual + passo + total) % total;
    ftCalcCategoria = inicio + atual;
    ftCalcCampo = 0;
    ftCalcIniciarCampos(ftCalcCategoria);
}

static void ftCalcAbrirLista(void) {
    int cat = ftCalcCategoria;
    if (!ftCalcTemUnidades(cat) || ftCalcCampo < 1) return;
    ftCalcIniciarCampos(cat);
    int total = ftCalcUnidadeTotal(cat);
    if (total <= 0) return;
    int sel = (int)(ftCalcCampos[cat][ftCalcCampo] + 0.5);
    if (sel < 0 || sel >= total) sel = 0;
    ftCalcListaSel = sel;
    ftCalcListaOffset = sel > 4 ? sel - 4 : 0;
    if (ftCalcListaOffset + 9 > total) ftCalcListaOffset = total > 9 ? total - 9 : 0;
    ftCalcListaAberta = true;
}

static void ftCalcListaMover(int delta) {
    int total = ftCalcUnidadeTotal(ftCalcCategoria);
    if (total <= 0) return;
    ftCalcListaSel = (ftCalcListaSel + delta + total) % total;
    if (ftCalcListaSel < ftCalcListaOffset) ftCalcListaOffset = ftCalcListaSel;
    if (ftCalcListaSel >= ftCalcListaOffset + 9) ftCalcListaOffset = ftCalcListaSel - 8;
}

static void ftCalcListaConfirmar(void) {
    int cat = ftCalcCategoria;
    int total = ftCalcUnidadeTotal(cat);
    if (total > 0 && ftCalcCampo >= 1) {
        ftCalcCampos[cat][ftCalcCampo] = ftCalcListaSel;
    }
    ftCalcListaAberta = false;
}

static void ftCalcEditarCampo(void) {
    int cat = ftCalcCategoria;
    int total = ftCalcCampoTotal(cat);
    if (total <= 0) return;
    if (ftCalcCampo < 0 || ftCalcCampo >= total) ftCalcCampo = 0;
    if (!ftCalcCampoNumerico(cat, ftCalcCampo)) { ftCalcAbrirLista(); return; }
    char inicial[40];
    ftCalcCampoTexto(cat, ftCalcCampo, inicial, sizeof(inicial));
    ftCalcImeCat = cat;
    ftCalcImeCampo = ftCalcCampo;
    ftImeAbrir(FT_IME_CALC_VALOR, "Digite o valor", inicial, 24);
}

static void ftCalcDesenharLimitado(uint32_t* p, const char* texto, int tam, int x, int y, int largura, uint32_t cor) {
    if (!texto || !texto[0]) return;
    int maximo = (int)(largura / (tam * 0.56f));
    if (maximo < 1) maximo = 1;
    char temp[256];
    snprintf(temp, sizeof(temp), "%s", texto);
    size_t n = strlen(temp);
    if ((int)n > maximo) {
        if (maximo > 3) temp[maximo - 3] = '.'; temp[maximo - 2] = '.'; temp[maximo - 1] = '.';
        temp[maximo] = 0;
    }
    desenharTexto(p, temp, tam, x, y, cor);
}

static void ftCalcDesenharAbas(uint32_t* p) {
    const char* nomes[5] = { "BASICA", "CONVERSOES", "FINANCEIRO", "COTACOES", "ACOES" };
    for (int i = 0; i < 5; i++) {
        int x = 60 + i * 300;
        bool ativo = (i == ftCalcRecurso);
        ftCaixa(p, x, 98, 286, 44, ativo ? FT_COR_AZUL : FT_COR_CARTAO);
        int largura = (int)(strlen(nomes[i]) * 22 * 0.56f);
        desenharTexto(p, nomes[i], 22, x + (286 - largura) / 2, 108, ativo ? 0xFF001018 : FT_COR_CINZA);
    }
}

static void ftCalcDesenharPadrao(uint32_t* p) {
    ftCaixa(p, 60, 176, 360, 812, FT_COR_CARTAO);
    desenharTexto(p, "HISTORICO", 28, 84, 198, FT_COR_AZUL);
    if (ftCalcHistTotal == 0) {
        desenharTexto(p, "Sem contas ainda.", 20, 84, 260, FT_COR_CINZA);
        desenharTexto(p, "As contas resolvidas", 18, 84, 300, FT_COR_CINZA);
        desenharTexto(p, "aparecem aqui.", 18, 84, 326, FT_COR_CINZA);
    }
    else {
        for (int i = 0; i < ftCalcHistTotal && i < FT_CALC_MAX_HIST; i++) {
            int y = 246 + i * 92;
            if (i > 0) ftCaixa(p, 84, y - 14, 312, 1, 0xFF2A3644);
            ftCalcDesenharLimitado(p, ftCalcHist[i], 18, 84, y, 312, i == 0 ? FT_COR_BRANCO : FT_COR_CINZA);
        }
    }

    ftCaixa(p, 460, 176, 1400, 150, FT_COR_CARTAO);
    if (ftCalcExpr[0]) ftCalcDesenharLimitado(p, ftCalcExpr, 24, 484, 194, 1360, FT_COR_CINZA);
    else desenharTexto(p, "Digite uma conta", 24, 484, 194, FT_COR_CINZA);
    ftCalcDesenharLimitado(p, ftCalcResultado, 60, 484, 230, 1340, ftCalcResultado[0] == 'E' ? FT_COR_VERMELHO : FT_COR_BRANCO);
    desenharTexto(p, ftCalcGraus ? "DEG" : "RAD", 22, 1770, 194, FT_COR_CINZA);
    if (ftCalc2nd) desenharTexto(p, "2nd", 22, 1770, 222, FT_COR_AMARELO);

    FtCalcTecla lista[FT_CALC_MAX_TECLAS];
    int n = ftCalcMontarTeclas(lista, FT_CALC_MAX_TECLAS);
    for (int i = 0; i < n; i++) {
        FtCalcTecla* t = &lista[i];
        bool escolhido = (i == ftCalcBotao);
        uint32_t cor = FT_COR_CARTAO;
        if (t->tipo == FT_CT_IGUAL) cor = 0xFF1F7A4D;
        else if (t->tipo == FT_CT_LIMPAR) cor = 0xFF4A2733;
        else if (t->tipo == FT_CT_APAGAR) cor = 0xFF2A3644;
        else if (t->tipo == FT_CT_2ND || t->tipo == FT_CT_GRAU || t->tipo == FT_CT_CIENTIFICA) cor = 0xFF23384D;
        if (escolhido) cor = FT_COR_AZUL;
        ftCaixa(p, t->x, t->y, t->w, t->h, cor);
        int tam = 36;
        size_t len = strlen(t->rotulo);
        if (len > 6) tam = 22;
        else if (len > 4) tam = 26;
        else if (len > 2) tam = 30;
        int largura = (int)(len * tam * 0.56f);
        desenharTexto(p, t->rotulo, tam, t->x + (t->w - largura) / 2, t->y + (t->h - tam) / 2, escolhido ? 0xFF001018 : FT_COR_BRANCO);
    }
    ftRodape(p, "Setas escolhem   X aperta   DEL apaga   Quadrado cientifica   R1/L1 recursos   Bolinha volta");
}

static void ftCalcDesenharLista(uint32_t* p) {
    int cat = ftCalcCategoria;
    int total = ftCalcUnidadeTotal(cat);
    if (!ftCalcListaAberta || total <= 0) return;
    const int x = 1010, y = 250, w = 690, h = 570;
    ftCaixa(p, x - 4, y - 4, w + 8, h + 8, FT_COR_AZUL);
    ftCaixa(p, x, y, w, h, 0xFF111A24);
    desenharTexto(p, "ESCOLHA A UNIDADE", 26, x + 24, y + 18, FT_COR_AZUL);
    for (int i = 0; i < 9; i++) {
        int idx = ftCalcListaOffset + i;
        if (idx >= total) break;
        int ly = y + 68 + i * 50;
        bool ativo = (idx == ftCalcListaSel);
        if (ativo) ftCaixa(p, x + 16, ly - 5, w - 32, 42, FT_COR_CARTAO_SEL);
        ftCalcDesenharLimitado(p, ftCalcUnidadeNome(cat, idx), 26, x + 34, ly, w - 70, ativo ? FT_COR_AZUL : FT_COR_BRANCO);
    }
    desenharTexto(p, "Cima/Baixo escolhe   X confirma   Bolinha fecha", 18, x + 24, y + h - 32, FT_COR_CINZA);
}

static void ftCalcDesenharRecurso(uint32_t* p) {
    ftCaixa(p, 60, 176, 380, 812, FT_COR_CARTAO);
    desenharTexto(p, ftCalcRecurso == 2 ? "FINANCAS" : "CALCULOS", 28, 84, 198, FT_COR_AZUL);
    int inicio = ftCalcCategoriaInicio();
    int total = ftCalcCategoriaTotal();
    for (int i = 0; i < total; i++) {
        int cat = inicio + i;
        int y = 246 + i * 55;
        bool ativo = (cat == ftCalcCategoria);
        if (ativo) ftCaixa(p, 80, y - 6, 340, 44, FT_COR_CARTAO_SEL);
        ftCalcDesenharLimitado(p, ftCalcCatNomes[cat], 22, 96, y, 310, ativo ? FT_COR_AZUL : FT_COR_CINZA);
    }

    int cat = ftCalcCategoria;
    ftCalcIniciarCampos(cat);
    ftCaixa(p, 460, 176, 1400, 812, FT_COR_CARTAO);
    desenharTexto(p, ftCalcCatNomes[cat], 40, 490, 196, FT_COR_BRANCO);
    desenharTexto(p, "L2/R2 categoria   Cima/Baixo campo   Esq/Dir muda   X lista/digitar", 20, 490, 242, FT_COR_CINZA);

    int campos = ftCalcCampoTotal(cat);
    for (int i = 0; i < campos; i++) {
        int y = 284 + i * 72;
        bool ativo = (i == ftCalcCampo);
        if (ativo) ftCaixa(p, 490, y - 8, 1340, 60, FT_COR_CARTAO_SEL);
        if (ativo) ftCaixa(p, 490, y - 8, 6, 60, FT_COR_AZUL);
        desenharTexto(p, ftCalcCampoRotulo(cat, i), 24, 514, y + 8, ativo ? FT_COR_BRANCO : FT_COR_CINZA);
        char valor[64];
        ftCalcCampoTexto(cat, i, valor, sizeof(valor));
        ftCalcDesenharLimitado(p, valor, 30, 1240, y, 560, ativo ? FT_COR_AZUL : FT_COR_BRANCO);
    }

    int resultadoY = 284 + campos * 72 + 18;
    if (resultadoY < 534) resultadoY = 534;
    if (resultadoY > 744) resultadoY = 744;
    ftCaixa(p, 490, resultadoY, 1340, 960 - resultadoY, 0xFF16212C);
    desenharTexto(p, "RESULTADO", 24, 514, resultadoY + 14, FT_COR_VERDE);
    char linhas[FT_CALC_LINHAS_MAX][128];
    for (int i = 0; i < FT_CALC_LINHAS_MAX; i++) linhas[i][0] = 0;
    int nl = ftCalcResultados(cat, linhas);
    for (int i = 0; i < nl && i < FT_CALC_LINHAS_MAX; i++) {
        ftCalcDesenharLimitado(p, linhas[i], 28, 514, resultadoY + 52 + i * 38, 1280, FT_COR_BRANCO);
    }
    if (nl == 0) desenharTexto(p, "Sem resultado.", 24, 514, resultadoY + 52, FT_COR_CINZA);

    ftCalcDesenharLista(p);
    ftRodape(p, "L2/R2 calculo   Cima/Baixo campo   Esq/Dir muda   X lista/digitar   Quadrado zera   R1/L1 aba");
}

static void ftQuoteLinha(uint32_t* p, int x0, int y0, int x1, int y1, uint32_t cor) {
    if (!p) return;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < FT_LARGURA && y0 >= 0 && y0 < FT_ALTURA) p[(size_t)y0 * FT_LARGURA + x0] = cor;
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void ftQuoteCalDesenharUm(uint32_t* p, const char* titulo, int ano, int mes, int selecionado, int x, int y, bool ativo) {
    static const char* meses[] = { "JANEIRO", "FEVEREIRO", "MARCO", "ABRIL", "MAIO", "JUNHO", "JULHO", "AGOSTO", "SETEMBRO", "OUTUBRO", "NOVEMBRO", "DEZEMBRO" };
    static const char* semana[] = { "D", "S", "T", "Q", "Q", "S", "S" };
    ftCaixa(p, x, y, 450, 430, ativo ? 0xFF102A3D : 0xFF0B1118);
    ftCaixa(p, x, y, 450, 4, ativo ? FT_COR_AZUL : 0xFF24303B);
    ftCalcDesenharLimitado(p, titulo, 24, x + 20, y + 18, 410, ativo ? FT_COR_AMARELO : FT_COR_CINZA);
    while (mes < 0) { mes += 12; ano--; }
    while (mes > 11) { mes -= 12; ano++; }
    char cabecalho[80];
    snprintf(cabecalho, sizeof(cabecalho), "%s %d", meses[mes], ano);
    ftCalcDesenharLimitado(p, cabecalho, 22, x + 20, y + 54, 410, FT_COR_BRANCO);
    for (int c = 0; c < 7; c++) desenharTexto(p, semana[c], 20, x + 22 + c * 60, y + 92, FT_COR_CINZA);
    long long primeiro = ftQuoteEpochData(ano, mes, 1);
    time_t bruto = (time_t)primeiro;
    struct tm* tmPrimeiro = localtime(&bruto);
    int inicio = tmPrimeiro ? tmPrimeiro->tm_wday : 0;
    int total = ftQuoteDiasNoMes(ano, mes);
    for (int d = 1; d <= total; d++) {
        int pos = inicio + d - 1;
        int cx = x + 16 + (pos % 7) * 60;
        int cy = y + 124 + (pos / 7) * 44;
        bool marcado = (d == selecionado);
        if (marcado) ftCaixa(p, cx, cy, 48, 34, ativo ? FT_COR_AZUL : 0xFF24303B);
        char num[8];
        snprintf(num, sizeof(num), "%d", d);
        desenharTexto(p, num, 22, cx + 12, cy + 5, marcado && ativo ? 0xFF001018 : FT_COR_BRANCO);
    }
    ftCalcDesenharLimitado(p, ativo ? "CALENDARIO ATIVO" : "", 18, x + 20, y + 396, 410, ativo ? FT_COR_VERDE : FT_COR_CINZA);
}

static void ftQuoteDesenharCalendario(uint32_t* p) {
    if (!ftQuoteCalAberto) return;
    ftCaixa(p, 330, 140, 1260, 540, FT_COR_FUNDO);
    ftCaixa(p, 330, 140, 1260, 4, FT_COR_AZUL);
    ftCalcDesenharLimitado(p, "PERSONALIZADO - DATA DE INICIO E DATA DE FIM", 28, 360, 162, 1200, FT_COR_BRANCO);

    long long agora = (long long)time(NULL);
    long long ini = ftQuoteInicioEpoch > 0 ? ftQuoteInicioEpoch : agora - (long long)ftQuoteDias * 86400LL;
    long long fim = ftQuoteFimEpoch > 0 ? ftQuoteFimEpoch : agora;
    int ai, mi, di, af, mf, df;
    ftQuoteEpochParaDatas(ini, &ai, &mi, &di);
    ftQuoteEpochParaDatas(fim, &af, &mf, &df);
    if (ftQuoteCalEtapa == 0) {
        ai = ftQuoteCalAno; mi = ftQuoteCalMes; di = ftQuoteCalDia;
    }
    else {
        af = ftQuoteCalAno; mf = ftQuoteCalMes; df = ftQuoteCalDia;
    }
    ftQuoteCalDesenharUm(p, "INICIO", ai, mi, di, 360, 210, ftQuoteCalEtapa == 0);
    ftQuoteCalDesenharUm(p, "FIM", af, mf, df, 1110, 210, ftQuoteCalEtapa == 1);
    ftCalcDesenharLimitado(p, "X confirma   Bolinha cancela   Esq/Dir mes   Cima/Baixo semana   L2/R2 ano", 20, 360, 650, 1200, FT_COR_CINZA);
}

static void ftQuoteDesenharMenu(uint32_t* p) {
    if (ftQuoteMenu == FT_QMENU_NENHUM) return;
    char rotulos[FT_QUOTE_MAX][96];
    int total = 0;
    const char* titulo = "MENU";
    const char* dica = "Cima/Baixo escolhe   X confirma   Bolinha volta";

    if (ftQuoteMenu == FT_QMENU_COPIAR) {
        titulo = "COPIAR / PRINT";
        snprintf(rotulos[total++], 96, "COPIAR TITULO E VALOR");
        snprintf(rotulos[total++], 96, "PRINT DA TELA");
        snprintf(rotulos[total++], 96, "ABRIR TECLADO COM O TEXTO");
        snprintf(rotulos[total++], 96, "VOLTAR");
    }
    else if (ftQuoteMenu == FT_QMENU_OPCOES) {
        titulo = "ATUALIZAR / BUSCA / LISTA";
        snprintf(rotulos[total++], 96, "ATUALIZAR");
        snprintf(rotulos[total++], 96, "BUSCAR E ADICIONAR");
        snprintf(rotulos[total++], 96, "REMOVER ATUAL");
        snprintf(rotulos[total++], 96, "SUBIR NA LISTA");
        snprintf(rotulos[total++], 96, "DESCER NA LISTA");
        snprintf(rotulos[total++], 96, "VOLTAR");
    }
    else if (ftQuoteMenu == FT_QMENU_BUSCA) {
        titulo = ftQuoteBuscaTexto;
        dica = "Cima/Baixo escolhe   X adiciona   Bolinha volta";
        for (int i = 0; i < ftQuoteBuscaTotal && total < FT_QUOTE_MAX; i++) {
            const FtQuoteDef* d = &ftQuoteDefs[ftQuoteBuscaRes[i]];
            snprintf(rotulos[total++], 96, "%s - %s", d->simbolo, d->nome);
        }
    }
    if (total == 0) return;

    const int x = 350, y = 150, w = 1220, h = 730;
    ftCaixa(p, x, y, w, h, FT_COR_FUNDO);
    ftCaixa(p, x, y, w, 4, FT_COR_AZUL);
    ftCaixa(p, x, y + 4, w, 66, FT_COR_TOPO);
    ftCalcDesenharLimitado(p, titulo, 30, x + 30, y + 26, w - 60, FT_COR_BRANCO);

    int sel = ftQuoteMenuSel;
    if (sel < 0) sel = 0;
    if (sel >= total) sel = total - 1;
    const int visiveis = 8;
    int off = 0;
    if (sel >= visiveis) off = sel - visiveis + 1;
    for (int i = 0; i < visiveis && off + i < total; i++) {
        int idx = off + i;
        int ly = y + 92 + i * 70;
        uint32_t cor = idx == sel ? FT_COR_AZUL : FT_COR_TOPO;
        ftCaixa(p, x + 24, ly, w - 48, 58, cor);
        ftCalcDesenharLimitado(p, rotulos[idx], 24, x + 44, ly + 17, w - 88, FT_COR_BRANCO);
    }
    ftCalcDesenharLimitado(p, dica, 20, x + 30, y + h - 50, w - 60, FT_COR_CINZA);
}

static void ftQuoteDesenhar(uint32_t* p) {
    int grupo = ftCalcRecurso == 4 ? 1 : 0;
    if (ftQuoteGrupo != grupo || ftQuoteTotal <= 0) ftQuoteAbrir(grupo);
    if (ftQuoteTotal <= 0) return;
    if (ftQuoteSel < 0 || ftQuoteSel >= ftQuoteTotal) ftQuoteSel = 0;
    FtQuote* q = &ftQuotes[ftQuoteSel];

    ftCaixa(p, 60, 176, 520, 812, FT_COR_CARTAO);
    desenharTexto(p, grupo == 0 ? "COTACOES" : "ACOES", 28, 84, 198, FT_COR_AZUL);
    desenharTexto(p, grupo == 0 ? "Dolar, euro e cripto" : "Bolsa e tecnologia", 18, 84, 226, FT_COR_CINZA);
    for (int i = 0; i < ftQuoteTotal; i++) {
        FtQuote* a = &ftQuotes[i];
        int y = 258 + i * 58;
        bool ativo = (i == ftQuoteSel);
        if (ativo) ftCaixa(p, 80, y - 8, 480, 50, FT_COR_CARTAO_SEL);
        if (ativo) ftCaixa(p, 80, y - 8, 5, 50, FT_COR_AZUL);
        desenharTexto(p, a->simbolo, 22, 96, y - 2, ativo ? FT_COR_AZUL : FT_COR_BRANCO);
        desenharTexto(p, a->nome, 16, 96, y + 22, FT_COR_CINZA);
        if (a->baixando) desenharTexto(p, "baixando", 17, 360, y + 8, FT_COR_AMARELO);
        else if (a->ok) {
            char valor[64];
            snprintf(valor, sizeof(valor), "R$ %s", a->preco);
            desenharTexto(p, valor, 20, 330, y - 2, FT_COR_BRANCO);
            uint32_t cor = a->variacao[0] == '-' ? FT_COR_VERMELHO : FT_COR_VERDE;
            desenharTexto(p, a->variacao, 17, 430, y + 20, cor);
        }
        else desenharTexto(p, "sem dados", 17, 360, y + 8, FT_COR_VERMELHO);
    }

    ftCaixa(p, 600, 176, 1260, 812, FT_COR_CARTAO);
    desenharTexto(p, q->simbolo, 46, 630, 196, FT_COR_BRANCO);
    desenharTexto(p, q->nome, 22, 630, 246, FT_COR_CINZA);
    if (q->ok) {
        char valor[96];
        snprintf(valor, sizeof(valor), "R$ %s", q->preco);
        desenharTexto(p, valor, 38, 630, 280, FT_COR_BRANCO);
        uint32_t cor = q->variacao[0] == '-' ? FT_COR_VERMELHO : FT_COR_VERDE;
        desenharTexto(p, q->variacao, 28, 1240, 288, cor);
    }
    else {
        desenharTexto(p, q->baixando ? "Baixando cotacao..." : "Sem cotacao. Veja a internet.", 24, 630, 284, FT_COR_AMARELO);
        if (q->erro) {
            char erro[80];
            snprintf(erro, sizeof(erro), "HTTP %d", q->erro);
            desenharTexto(p, erro, 20, 630, 316, FT_COR_VERMELHO);
        }
    }

    const char* periodos[4] = { "DIA", "MES", "ANO", "PERSONALIZADO" };
    for (int i = 0; i < 4; i++) {
        int x = 630 + i * 296;
        bool ativo = (i == ftQuotePeriodo);
        ftCaixa(p, x, 330, 280, 38, ativo ? FT_COR_AZUL : 0xFF23384D);
        char rotulo[48];
        if (i == 3) snprintf(rotulo, sizeof(rotulo), "PERSONALIZADO %dd", ftQuoteDias);
        else snprintf(rotulo, sizeof(rotulo), "%s", periodos[i]);
        desenharTexto(p, rotulo, 17, x + 16, 340, ativo ? 0xFF001018 : FT_COR_CINZA);
    }
    if (ftQuotePeriodo == 3) {
        char de[32], ate[32], intervalo[80];
        if (ftQuoteInicioEpoch > 0) ftQuoteFormatarData(ftQuoteInicioEpoch, de, sizeof(de));
        else snprintf(de, sizeof(de), "--/--/----");
        if (ftQuoteFimEpoch > 0) ftQuoteFormatarData(ftQuoteFimEpoch, ate, sizeof(ate));
        else snprintf(ate, sizeof(ate), "--/--/----");
        snprintf(intervalo, sizeof(intervalo), "DE %s ATE %s   (X escolhe no calendario)", de, ate);
        desenharTexto(p, intervalo, 17, 630, 370, FT_COR_AMARELO);
    }

    const int gx = 630, gy = 392, gw = 1190, gh = 500;
    ftCaixa(p, gx, gy, gw, gh, 0xFF0B1118);
    for (int i = 0; i <= 4; i++) {
        int ly = gy + (gh * i) / 4;
        ftCaixa(p, gx, ly, gw, 1, 0xFF24303B);
    }
    if (q->total >= 2) {
        double minimo = q->minimo, maximo = q->maximo;
        if (maximo - minimo < 0.0000001) { maximo = minimo + 1.0; }
        int n = q->total;
        int px = 0, py = 0;
        for (int i = 0; i < n; i++) {
            int x = gx + 12 + (int)((long long)(gw - 24) * i / (n > 1 ? n - 1 : 1));
            int y = gy + gh - 16 - (int)((q->pontos[i] - minimo) * (gh - 40) / (maximo - minimo));
            if (y < gy + 8) y = gy + 8;
            if (y > gy + gh - 8) y = gy + gh - 8;
            if (i > 0) ftQuoteLinha(p, px, py, x, y, q->pontos[n - 1] >= q->pontos[0] ? FT_COR_VERDE : FT_COR_VERMELHO);
            px = x; py = y;
        }
        char txt[40];
        ftQuotePrecoTexto(maximo, txt, sizeof(txt));
        desenharTexto(p, txt, 17, gx + 8, gy + 6, FT_COR_CINZA);
        ftQuotePrecoTexto(minimo, txt, sizeof(txt));
        desenharTexto(p, txt, 17, gx + 8, gy + gh - 26, FT_COR_CINZA);
    }
    else {
        desenharTexto(p, q->baixando ? "Baixando grafico..." : "Sem grafico para este ativo.", 24, gx + 40, gy + 220, q->baixando ? FT_COR_AMARELO : FT_COR_CINZA);
    }

    ftRodape(p, "X atualiza   Quadrado copiar/print   Triangulo busca/lista   Esq/Dir periodo   R1/L1 aba   Bolinha volta");
    if (ftQuotePrintPendente) {
        ftQuotePrintPendente = false;
        ftQuotePrintTela();
    }
    else ftQuoteDesenharMenu(p);
    ftQuoteDesenharCalendario(p);
    if (ftQuoteAviso[0]) ftCalcDesenharLimitado(p, ftQuoteAviso, 18, 630, 910, 1180, FT_COR_AMARELO);
}

static void ftDesenharCalc(uint32_t* p) {
    ftFundo(p);
    const char* titulo = ftCalcRecurso == 3 ? "COTACOES" : (ftCalcRecurso == 4 ? "ACOES" : "CALCULADORA");
    ftTopo(p, titulo, "R1/L1 troca a aba   Bolinha volta");
    ftCalcDesenharAbas(p);
    if (ftCalcRecurso == 0) ftCalcDesenharPadrao(p);
    else if (ftCalcRecurso <= 2) ftCalcDesenharRecurso(p);
    else ftQuoteDesenhar(p);
}

static void ftCalcAbrir(void) {
    ftCalcRecurso = 0;
    ftCalcCientifica = false;
    ftCalc2nd = false;
    ftCalcGraus = true;
    ftCalcExpr[0] = 0;
    snprintf(ftCalcResultado, sizeof(ftCalcResultado), "0");
    ftCalcTemResultado = false;
    ftCalcBotao = 0;
    ftCalcCategoria = FT_CAT_IDADE;
    ftCalcCampo = 0;
    ftCalcListaAberta = false;
    ftQuoteGrupo = -1;
    ftQuoteTotal = 0;
    for (int i = 0; i < FT_CAT_TOTAL; i++) ftCalcCamposInit[i] = false;
    ftCalcIniciarCampos(FT_CAT_IDADE);
}

// ---------------------------------------------------------------- relogio
static int ftRelogioAba = 0;
static double ftCronoAcumulado = 0.0;
static double ftCronoInicio = 0.0;
static bool ftCronoRodando = false;
static double ftCronoVoltas[5];
static int ftCronoTotalVoltas = 0;
static int ftTimerCampo = 0;
static int ftTimerH = 0, ftTimerM = 5, ftTimerS = 0;
static bool ftTimerRodando = false;
static double ftTimerFim = 0.0;
static double ftTimerRestante = 0.0;
static bool ftTimerConcluido = false;

struct FtAlarme { int hora, minuto; bool ligado; };
#define FT_MAX_ALARMES 20
static FtAlarme ftAlarmes[FT_MAX_ALARMES];
static int ftAlarmeTotal = 0;
static int ftAlarmeSel = 0;
static float ftAlarmeFlash = 0.0f;
static int ftAlarmeUltimoMinuto = -1;

static void ftAlarmesSalvar(void) {
    ftMkdirs(FT_CONF);
    FILE* f = fopen(FT_CONF "/alarmes_hn.txt", "w");
    if (!f) return;
    for (int i = 0; i < ftAlarmeTotal; i++) fprintf(f, "%d %d %d\n", ftAlarmes[i].hora, ftAlarmes[i].minuto, ftAlarmes[i].ligado ? 1 : 0);
    fclose(f);
}

static void ftAlarmesCarregar(void) {
    ftAlarmeTotal = 0;
    FILE* f = fopen(FT_CONF "/alarmes_hn.txt", "r");
    if (f) {
        while (ftAlarmeTotal < FT_MAX_ALARMES) {
            int h, m, on;
            if (fscanf(f, "%d %d %d", &h, &m, &on) != 3) break;
            if (h < 0 || h > 23 || m < 0 || m > 59) continue;
            ftAlarmes[ftAlarmeTotal].hora = h;
            ftAlarmes[ftAlarmeTotal].minuto = m;
            ftAlarmes[ftAlarmeTotal].ligado = on != 0;
            ftAlarmeTotal++;
        }
        fclose(f);
    }
    if (ftAlarmeTotal == 0) {
        ftAlarmes[0].hora = 7; ftAlarmes[0].minuto = 0; ftAlarmes[0].ligado = true;
        ftAlarmeTotal = 1;
        ftAlarmesSalvar();
    }
    if (ftAlarmeSel >= ftAlarmeTotal) ftAlarmeSel = ftAlarmeTotal - 1;
    if (ftAlarmeSel < 0) ftAlarmeSel = 0;
}

static double ftCronoTempo(void) {
    return ftCronoAcumulado + (ftCronoRodando ? (ftSegundos() - ftCronoInicio) : 0.0);
}

static void ftRelogioAtualizar(void) {
    if (ftTimerRodando) {
        double falta = ftTimerFim - ftSegundos();
        if (falta <= 0.0) {
            ftTimerRodando = false;
            ftTimerRestante = 0.0;
            ftTimerConcluido = true;
            ftAviso("TIMER CONCLUIDO!");
        }
        else ftTimerRestante = falta;
    }
    if (ftAlarmeFlash > 0.0f) ftAlarmeFlash -= 1.0f / 60.0f;

    struct tm t;
    ftDataHora(&t);
    int minutoAtual = t.tm_hour * 60 + t.tm_min;
    if (minutoAtual != ftAlarmeUltimoMinuto) {
        for (int i = 0; i < ftAlarmeTotal; i++) {
            if (ftAlarmes[i].ligado && ftAlarmes[i].hora == t.tm_hour && ftAlarmes[i].minuto == t.tm_min) {
                ftAlarmeFlash = 6.0f;
                snprintf(msgStatus, sizeof(msgStatus), "ALARME %02d:%02d", ftAlarmes[i].hora, ftAlarmes[i].minuto);
                msgTimer = 600;
                break;
            }
        }
        ftAlarmeUltimoMinuto = minutoAtual;
    }
}

static void ftDesenharCalendario(uint32_t* p, const struct tm* hoje, int x, int y) {
    int mes = hoje->tm_mon, ano = hoje->tm_year + 1900;
    static const char* meses[] = { "JANEIRO", "FEVEREIRO", "MARCO", "ABRIL", "MAIO", "JUNHO", "JULHO", "AGOSTO", "SETEMBRO", "OUTUBRO", "NOVEMBRO", "DEZEMBRO" };
    char titulo[64];
    snprintf(titulo, sizeof(titulo), "%s %d", meses[mes], ano);
    desenharTexto(p, titulo, 28, x, y, FT_COR_AZUL);

    struct tm primeiro;
    memset(&primeiro, 0, sizeof(primeiro));
    primeiro.tm_year = ano - 1900;
    primeiro.tm_mon = mes;
    primeiro.tm_mday = 1;
    primeiro.tm_hour = 12;
    mktime(&primeiro);
    int inicio = primeiro.tm_wday;
    int dias = 31;
    if (mes == 3 || mes == 5 || mes == 8 || mes == 10) dias = 30;
    if (mes == 1) dias = ((ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0) ? 29 : 28;

    static const char* semana[] = { "D", "S", "T", "Q", "Q", "S", "S" };
    for (int c = 0; c < 7; c++) desenharTexto(p, semana[c], 22, x + c * 52 + 18, y + 52, FT_COR_CINZA);
    for (int d = 0; d < dias; d++) {
        int pos = inicio + d;
        int cx = x + (pos % 7) * 52;
        int cy = y + 92 + (pos / 7) * 46;
        bool ehHoje = (d + 1 == hoje->tm_mday);
        if (ehHoje) ftCaixa(p, cx - 4, cy - 4, 46, 38, FT_COR_AZUL);
        char num[8];
        snprintf(num, sizeof(num), "%d", d + 1);
        desenharTexto(p, num, 24, cx + 10, cy, ehHoje ? 0xFF001018 : FT_COR_BRANCO);
    }
}

static void ftDesenharRelogio(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "RELOGIO & TEMPO", "L1/R1 muda a aba   X acao   Quadrado zera   Bolinha volta");
    static const char* abas[] = { "RELOGIO", "CRONOMETRO", "TIMER", "ALARME" };
    for (int i = 0; i < 4; i++) {
        int x = 300 + i * 300;
        bool ativo = (i == ftRelogioAba);
        ftCaixa(p, x, 120, 270, 56, ativo ? FT_COR_AZUL : FT_COR_CARTAO);
        int largura = (int)(strlen(abas[i]) * 24 * 0.56f);
        desenharTexto(p, abas[i], 24, x + (270 - largura) / 2, 134, ativo ? 0xFF001018 : FT_COR_BRANCO);
    }

    struct tm t;
    ftDataHora(&t);
    if (ftRelogioAba == 0) {
        char hora[32], data[64];
        snprintf(hora, sizeof(hora), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        static const char* dias[] = { "DOMINGO", "SEGUNDA", "TERCA", "QUARTA", "QUINTA", "SEXTA", "SABADO" };
        snprintf(data, sizeof(data), "%s, %02d/%02d/%04d", dias[t.tm_wday], t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        desenharTexto(p, hora, 120, 280, 250, FT_COR_BRANCO);
        desenharTexto(p, data, 34, 300, 400, FT_COR_CINZA);
        ftDesenharCalendario(p, &t, 1180, 210);
    }
    else if (ftRelogioAba == 1) {
        char tempo[48];
        ftHorario(ftCronoTempo(), tempo, sizeof(tempo), true);
        desenharTexto(p, tempo, 110, 430, 250, ftCronoRodando ? FT_COR_VERDE : FT_COR_BRANCO);
        desenharTexto(p, ftCronoRodando ? "CONTANDO" : "PARADO", 30, 840, 300, FT_COR_CINZA);
        desenharTexto(p, "X iniciar/parar   Quadrado zerar   Triangulo volta", 24, 300, 440, FT_COR_CINZA);
        for (int i = 0; i < ftCronoTotalVoltas; i++) {
            char linha[64];
            ftHorario(ftCronoVoltas[i], linha, sizeof(linha), true);
            char texto[96];
            snprintf(texto, sizeof(texto), "VOLTA %d   %s", i + 1, linha);
            desenharTexto(p, texto, 26, 430, 520 + i * 44, FT_COR_BRANCO);
        }
    }
    else if (ftRelogioAba == 2) {
        char tempo[48];
        if (ftTimerRodando) ftHorario(ftTimerRestante, tempo, sizeof(tempo), true);
        else {
            double total = ftTimerH * 3600.0 + ftTimerM * 60.0 + ftTimerS;
            if (ftTimerRestante > 0.0) ftHorario(ftTimerRestante, tempo, sizeof(tempo), true);
            else ftHorario(total, tempo, sizeof(tempo), true);
        }
        desenharTexto(p, tempo, 110, 430, 240, ftTimerRodando ? FT_COR_AMARELO : FT_COR_BRANCO);
        desenharTexto(p, ftTimerConcluido ? "TEMPO ESGOTADO!" : (ftTimerRodando ? "CONTANDO" : "PRONTO"), 32, 760, 290, ftTimerConcluido ? FT_COR_VERMELHO : FT_COR_CINZA);

        char campos[64];
        snprintf(campos, sizeof(campos), "%02d : %02d : %02d", ftTimerH, ftTimerM, ftTimerS);
        desenharTexto(p, campos, 46, 760, 400, FT_COR_AZUL);
        desenharTexto(p, "Esquerda/direita escolhe   Cima/baixo ajusta", 24, 700, 480, FT_COR_CINZA);
        desenharTexto(p, "X iniciar/pausar   Quadrado zerar", 24, 700, 520, FT_COR_CINZA);
        const char* marcador = ftTimerCampo == 0 ? "   ^^^        " : ftTimerCampo == 1 ? "        ^^^   " : "              ^^^";
        desenharTexto(p, marcador, 24, 760, 450, FT_COR_AMARELO);
    }
    else {
        desenharTexto(p, "ALARMES", 34, 300, 210, FT_COR_AZUL);
        int inicio = ftAlarmeSel - 5;
        if (inicio < 0) inicio = 0;
        if (inicio > ftAlarmeTotal - 12) inicio = ftAlarmeTotal - 12;
        if (inicio < 0) inicio = 0;
        int fim = inicio + 12;
        if (fim > ftAlarmeTotal) fim = ftAlarmeTotal;
        for (int i = inicio; i < fim; i++) {
            char linha[96];
            snprintf(linha, sizeof(linha), "%02d:%02d   %s", ftAlarmes[i].hora, ftAlarmes[i].minuto, ftAlarmes[i].ligado ? "LIGADO" : "desligado");
            uint32_t cor = (i == ftAlarmeSel) ? FT_COR_AMARELO : (ftAlarmes[i].ligado ? FT_COR_BRANCO : FT_COR_CINZA);
            if (i == ftAlarmeSel) ftCaixa(p, 280, 250 + (i - inicio) * 58, 760, 48, FT_COR_CARTAO_SEL);
            desenharTexto(p, linha, 30, 310, 258 + (i - inicio) * 58, cor);
        }
        desenharTexto(p, "Cima/baixo escolhe   X liga/desliga", 24, 300, 760, FT_COR_CINZA);
        desenharTexto(p, "Quadrado novo (hora do timer)   Triangulo apaga", 24, 300, 800, FT_COR_CINZA);
    }

    if (ftAlarmeFlash > 0.0f) {
        int a = (int)(ftAlarmeFlash * 40.0f);
        if (a > 90) a = 90;
        ftCaixa(p, 0, 86, FT_LARGURA, 60, (uint32_t)(a << 24) | 0x00FF3333u);
        ftTextoCentral(p, "ALARME TOCANDO", 34, 96, FT_COR_BRANCO);
    }
    ftRodape(p, "Relogio com calendario, cronometro com voltas, timer e alarmes salvos no PS4.");
}

// ---------------------------------------------------------------- gravador
static volatile bool ftGravRodando = false;
static volatile bool ftGravParar = false;
static volatile int ftGravSegundos = 0;
static volatile int ftGravErro = 0;
static char ftGravArquivo[256] = "";
static int ftGravTotal = 0;

static void ftWavCabecalho(unsigned char* h, uint32_t dados, uint32_t taxa) {
    memset(h, 0, 44);
    memcpy(h, "RIFF", 4);
    uint32_t riff = 36 + dados;
    h[4] = (unsigned char)riff; h[5] = (unsigned char)(riff >> 8); h[6] = (unsigned char)(riff >> 16); h[7] = (unsigned char)(riff >> 24);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = 1;
    h[24] = (unsigned char)taxa; h[25] = (unsigned char)(taxa >> 8); h[26] = (unsigned char)(taxa >> 16); h[27] = (unsigned char)(taxa >> 24);
    uint32_t bytes = taxa * 2;
    h[28] = (unsigned char)bytes; h[29] = (unsigned char)(bytes >> 8); h[30] = (unsigned char)(bytes >> 16); h[31] = (unsigned char)(bytes >> 24);
    h[32] = 2; h[34] = 16;
    memcpy(h + 36, "data", 4);
    h[40] = (unsigned char)dados; h[41] = (unsigned char)(dados >> 8); h[42] = (unsigned char)(dados >> 16); h[43] = (unsigned char)(dados >> 24);
}

static void* ftGravThread(void*) {
    typedef int (*AbrirMic)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef int (*LerMic)(int32_t, void*);
    typedef int (*FecharMic)(int32_t);

    int mic = ((AbrirMic)sceAudioInOpen)(global_uId, 0, 0, 256, 16000, 0);
    if (mic < 0) mic = ((AbrirMic)sceAudioInOpen)(255, 0, 0, 256, 16000, 0);
    if (mic < 0) {
        ftGravErro = (int)(unsigned)mic;
        ftGravRodando = false;
        return NULL;
    }

    ftMkdirs(FT_GRAVACOES);
    time_t agora = time(NULL);
    struct tm* t = localtime(&agora);
    snprintf(ftGravArquivo, sizeof(ftGravArquivo), FT_GRAVACOES "/gravacao_%04d%02d%02d_%02d%02d%02d.wav",
             t ? t->tm_year + 1900 : 0, t ? t->tm_mon + 1 : 0, t ? t->tm_mday : 0,
             t ? t->tm_hour : 0, t ? t->tm_min : 0, t ? t->tm_sec : 0);
    FILE* f = fopen(ftGravArquivo, "wb");
    if (!f) {
        ((FecharMic)sceAudioInClose)(mic);
        ftGravErro = -2;
        ftGravRodando = false;
        return NULL;
    }
    unsigned char cab[44];
    ftWavCabecalho(cab, 0, 16000);
    fwrite(cab, 1, sizeof(cab), f);

    static int16_t lido[256];
    uint32_t escritos = 0;
    int erros = 0;
    ftGravSegundos = 0;
    while (!ftGravParar && escritos < 16000u * 600u * 2u) {
        int r = ((LerMic)sceAudioInInput)(mic, lido);
        if (r < 0) {
            if (++erros > 300) break;
            sceKernelUsleep(5000);
            continue;
        }
        erros = 0;
        int k = (r == 0 || r > 256) ? 256 : r;
        if (escritos + (uint32_t)k * 2u > 16000u * 600u * 2u) k = (int)((16000u * 600u * 2u - escritos) / 2u);
        size_t gravados = fwrite(lido, 2, (size_t)k, f);
        escritos += (uint32_t)gravados * 2u;
        ftGravSegundos = (int)(escritos / 2u / 16000u);
    }
    ((FecharMic)sceAudioInClose)(mic);
    fseek(f, 0, SEEK_SET);
    ftWavCabecalho(cab, escritos, 16000);
    fwrite(cab, 1, sizeof(cab), f);
    fclose(f);
    ftGravRodando = false;
    return NULL;
}

static void ftGravIniciar(void) {
    if (ftGravRodando) return;
    ftGravErro = 0;
    ftGravSegundos = 0;
    ftGravParar = false;
    ftGravArquivo[0] = 0;
    ftGravRodando = true;
    pthread_t th;
    if (pthread_create(&th, NULL, ftGravThread, NULL) != 0) {
        ftGravRodando = false;
        ftGravErro = -3;
    }
    else pthread_detach(th);
}

static void ftGravContar(void) {
    ftGravTotal = 0;
    DIR* d = opendir(FT_GRAVACOES);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (strstr(e->d_name, ".wav")) ftGravTotal++;
    }
    closedir(d);
}

static void ftDesenharGravador(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "GRAVADOR DE AUDIO", "Microfone do controle, fone ou camera do PS4");
    ftGravContar();
    ftCaixa(p, 260, 170, 1400, 500, FT_COR_CARTAO);
    ftCaixa(p, 260, 170, 8, 500, ftGravRodando ? FT_COR_VERMELHO : FT_COR_AZUL);
    desenharTexto(p, ftGravRodando ? "GRAVANDO" : (ftGravErro ? "ERRO" : "PRONTO"), 42, 320, 230, ftGravRodando ? FT_COR_VERMELHO : (ftGravErro ? FT_COR_VERMELHO : FT_COR_VERDE));
    char tempo[32];
    ftHorario(ftGravSegundos, tempo, sizeof(tempo), false);
    desenharTexto(p, tempo, 96, 320, 310, FT_COR_BRANCO);
    if (ftGravArquivo[0]) desenharTexto(p, ftGravArquivo, 22, 320, 450, FT_COR_CINZA);
    if (ftGravErro) {
        char e[160];
        snprintf(e, sizeof(e), "Microfone nao encontrado (0x%X). Ligue um fone com microfone.", (unsigned)ftGravErro);
        desenharTexto(p, e, 26, 320, 520, FT_COR_VERMELHO);
    }
    char total[96];
    snprintf(total, sizeof(total), "Gravacoes em " FT_GRAVACOES ": %d", ftGravTotal);
    desenharTexto(p, total, 24, 320, 600, FT_COR_CINZA);
    desenharTexto(p, "X iniciar/parar   Quadrado apagar a ultima   Bolinha volta", 24, 320, 720, FT_COR_CINZA);
    ftRodape(p, "O audio fica em WAV 16 kHz, mono. Tambem da para ouvir no PC.");
}

// ---------------------------------------------------------------- afinador
#define FT_AF_TAXA 16000
#define FT_AF_BLOCO 256
#define FT_AF_JANELA 2048
#define FT_AF_MAXLAG 250

static int16_t ftAfinAmostras[FT_AF_JANELA];
static volatile bool ftAfinRodando = false;
static volatile bool ftAfinParar = false;
static volatile int ftAfinErro = 0;
static volatile float ftAfinFreq = 0.0f;
static volatile float ftAfinNivel = 0.0f;
static volatile bool ftAfinSinal = false;
static int ftAfinModo = 0; // 0 = cromatico, 1 = violao
static float ftAfinMostrar = 0.0f;

static const char* ftAfinCordasNomes[] = { "E2", "A2", "D3", "G3", "B3", "E4" };
static const float ftAfinCordasFreq[] = { 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f };

static const char* ftAfinNomeIngles(int midi) {
    static const char* nomes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int n = midi % 12;
    if (n < 0) n += 12;
    return nomes[n];
}

static const char* ftAfinNomePortugues(int midi) {
    static const char* nomes[] = { "DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SOL", "SOL#", "LA", "LA#", "SI" };
    int n = midi % 12;
    if (n < 0) n += 12;
    return nomes[n];
}

static float ftAfinDetectar(const int16_t* dados, int n, float* nivelSaida) {
    double media = 0.0;
    for (int i = 0; i < n; i++) media += dados[i];
    media /= (double)n;

    double energia = 0.0;
    for (int i = 0; i < n; i++) {
        double v = (double)dados[i] - media;
        energia += v * v;
    }
    float nivel = (float)sqrt(energia / (double)n);
    if (nivelSaida) *nivelSaida = nivel;
    if (nivel < 80.0f) return 0.0f;

    const int minLag = (int)(FT_AF_TAXA / 1000.0f);
    const int maxLag = (int)(FT_AF_TAXA / 65.0f);
    const int janela = n - maxLag;
    if (janela < 128) return 0.0f;

    static double d[FT_AF_MAXLAG + 2];
    static double cm[FT_AF_MAXLAG + 2];
    for (int tau = 1; tau <= maxLag; tau++) {
        double soma = 0.0;
        for (int i = 0; i < janela; i++) {
            double delta = (double)dados[i] - (double)dados[i + tau];
            soma += delta * delta;
        }
        d[tau] = soma;
    }

    cm[0] = 1.0;
    double corrida = 0.0;
    for (int tau = 1; tau <= maxLag; tau++) {
        corrida += d[tau];
        cm[tau] = corrida > 0.0 ? (d[tau] * tau / corrida) : 1.0;
    }

    int escolhido = -1;
    for (int tau = minLag; tau <= maxLag; tau++) {
        if (cm[tau] < 0.20) {
            while (tau + 1 <= maxLag && cm[tau + 1] < cm[tau]) tau++;
            escolhido = tau;
            break;
        }
    }
    if (escolhido < 0) {
        double melhor = 1e9;
        for (int tau = minLag; tau <= maxLag; tau++) {
            if (cm[tau] < melhor) { melhor = cm[tau]; escolhido = tau; }
        }
        if (melhor > 0.45) return 0.0f;
    }
    if (escolhido <= 1 || escolhido >= maxLag) return 0.0f;

    double s0 = cm[escolhido - 1], s1 = cm[escolhido], s2 = cm[escolhido + 1];
    double denom = s0 - 2.0 * s1 + s2;
    double ajuste = fabs(denom) > 1e-9 ? (s0 - s2) / (2.0 * denom) : 0.0;
    if (ajuste < -0.5) ajuste = -0.5;
    if (ajuste > 0.5) ajuste = 0.5;

    double freq = (double)FT_AF_TAXA / ((double)escolhido + ajuste);
    if (freq < 60.0 || freq > 1100.0) return 0.0f;
    return (float)freq;
}

static void* ftAfinThread(void*) {
    typedef int (*AbrirMic)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef int (*LerMic)(int32_t, void*);
    typedef int (*FecharMic)(int32_t);

    int mic = ((AbrirMic)sceAudioInOpen)(global_uId, 0, 0, 256, 16000, 0);
    if (mic < 0) mic = ((AbrirMic)sceAudioInOpen)(255, 0, 0, 256, 16000, 0);
    if (mic < 0) {
        ftAfinErro = (int)(unsigned)mic;
        ftAfinRodando = false;
        return NULL;
    }

    int16_t bloco[FT_AF_BLOCO];
    int total = 0;
    int erros = 0;
    while (!ftAfinParar) {
        int r = ((LerMic)sceAudioInInput)(mic, bloco);
        if (r < 0) {
            if (++erros > 300) { ftAfinErro = -4; break; }
            sceKernelUsleep(5000);
            continue;
        }
        erros = 0;
        int n = (r == 0 || r > FT_AF_BLOCO) ? FT_AF_BLOCO : r;
        if (total + n > FT_AF_JANELA) {
            int sobra = total + n - FT_AF_JANELA;
            if (sobra > total) sobra = total;
            memmove(ftAfinAmostras, ftAfinAmostras + sobra,
                    (size_t)(total - sobra) * sizeof(int16_t));
            total -= sobra;
        }
        if (n > FT_AF_JANELA - total) n = FT_AF_JANELA - total;
        memcpy(ftAfinAmostras + total, bloco, (size_t)n * sizeof(int16_t));
        total += n;

        if (total >= FT_AF_JANELA) {
            float nivel = 0.0f;
            float freq = ftAfinDetectar(ftAfinAmostras, FT_AF_JANELA, &nivel);
            ftAfinNivel = nivel;
            ftAfinFreq = freq;
            ftAfinSinal = (freq > 0.0f);
            total = 0;
        }
    }

    ((FecharMic)sceAudioInClose)(mic);
    ftAfinFreq = 0.0f;
    ftAfinNivel = 0.0f;
    ftAfinSinal = false;
    ftAfinRodando = false;
    return NULL;
}

static void ftAfinIniciar(void) {
    if (ftAfinRodando) return;
    ftAfinErro = 0;
    ftAfinFreq = 0.0f;
    ftAfinNivel = 0.0f;
    ftAfinSinal = false;
    ftAfinMostrar = 0.0f;
    ftAfinParar = false;
    ftAfinRodando = true;
    pthread_t th;
    if (pthread_create(&th, NULL, ftAfinThread, NULL) != 0) {
        ftAfinRodando = false;
        ftAfinErro = -3;
    }
    else pthread_detach(th);
}

static void ftAfinPararThread(void) {
    if (!ftAfinRodando) return;
    ftAfinParar = true;
    for (int i = 0; i < 100 && ftAfinRodando; i++) sceKernelUsleep(10000);
    ftAfinMostrar = 0.0f;
}

static void ftDesenharAfinador(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "AFINADOR", "Referencia A4 = 440 Hz");
    ftCaixa(p, 210, 150, 1500, 780, FT_COR_CARTAO);
    ftCaixa(p, 210, 150, 8, 780, ftAfinSinal ? FT_COR_VERDE : FT_COR_AZUL);

    char notaGrande[16] = "--";
    char notaPt[24] = "";
    char freqTxt[64] = "Sem leitura";
    char centsTxt[32] = "";
    char estado[40] = "OUVINDO";
    uint32_t corNota = FT_COR_CINZA;
    uint32_t corEstado = FT_COR_AZUL;
    float cents = 0.0f;
    int cordaAtual = -1;
    float freq = ftAfinFreq;

    if (ftAfinSinal && freq > 0.0f) {
        if (ftAfinMostrar <= 0.0f) ftAfinMostrar = freq;
        else ftAfinMostrar = ftAfinMostrar * 0.82f + freq * 0.18f;
        freq = ftAfinMostrar;

        float alvo = 440.0f;
        if (ftAfinModo == 1) {
            float melhor = 1e9f;
            for (int i = 0; i < 6; i++) {
                float c = (float)fabs(1200.0 * log((double)freq / (double)ftAfinCordasFreq[i]) / log(2.0));
                if (c < melhor) { melhor = c; cordaAtual = i; }
            }
            alvo = ftAfinCordasFreq[cordaAtual];
            snprintf(notaGrande, sizeof(notaGrande), "%s", ftAfinCordasNomes[cordaAtual]);
            snprintf(notaPt, sizeof(notaPt), "CORDA %d", 6 - cordaAtual);
        }
        else {
            int midi = (int)floor(69.0 + 12.0 * log((double)freq / 440.0) / log(2.0) + 0.5);
            alvo = (float)(440.0 * pow(2.0, (midi - 69) / 12.0));
            snprintf(notaGrande, sizeof(notaGrande), "%s%d", ftAfinNomeIngles(midi), midi / 12 - 1);
            snprintf(notaPt, sizeof(notaPt), "%s", ftAfinNomePortugues(midi));
        }

        cents = (float)(1200.0 * log((double)freq / (double)alvo) / log(2.0));
        snprintf(freqTxt, sizeof(freqTxt), "%.1f Hz", freq);
        float centsMostrar = cents;
        if (centsMostrar > 50.0f) centsMostrar = 50.0f;
        if (centsMostrar < -50.0f) centsMostrar = -50.0f;
        snprintf(centsTxt, sizeof(centsTxt), "%+.0f cents", centsMostrar);

        if (fabs(cents) <= 4.0f) {
            snprintf(estado, sizeof(estado), "AFINADO");
            corNota = FT_COR_VERDE;
            corEstado = FT_COR_VERDE;
        }
        else if (cents < 0.0f) {
            snprintf(estado, sizeof(estado), "ABAIXO");
            corNota = FT_COR_AMARELO;
            corEstado = FT_COR_AMARELO;
        }
        else {
            snprintf(estado, sizeof(estado), "ACIMA");
            corNota = FT_COR_AMARELO;
            corEstado = FT_COR_AMARELO;
        }
    }
    else if (!ftAfinRodando) {
        snprintf(estado, sizeof(estado), "%s", ftAfinErro ? "MICROFONE INDISPONIVEL" : "PAUSADO");
        corEstado = ftAfinErro ? FT_COR_VERMELHO : FT_COR_CINZA;
        if (ftAfinErro) snprintf(freqTxt, sizeof(freqTxt), "Erro 0x%X", (unsigned)ftAfinErro);
    }
    else {
        snprintf(estado, sizeof(estado), "SEM SOM");
        corEstado = FT_COR_AMARELO;
        snprintf(freqTxt, sizeof(freqTxt), "Toque uma corda perto do microfone");
    }

    ftTextoCentral(p, notaGrande, 140, 180, corNota);
    if (notaPt[0]) ftTextoCentral(p, notaPt, 34, 330, FT_COR_CINZA);
    ftTextoCentral(p, freqTxt, ftAfinSinal ? 44 : 30, 382, ftAfinSinal ? FT_COR_BRANCO : FT_COR_CINZA);
    ftTextoCentral(p, estado, 34, 440, corEstado);

    ftCaixa(p, 560, 540, 800, 4, 0xFF536273);
    ftCaixa(p, 930, 536, 60, 12, 0xFF1E5B3A);
    ftCaixa(p, 958, 528, 4, 28, FT_COR_VERDE);
    for (int i = -50; i <= 50; i += 25) {
        int tx = 960 + i * 8;
        ftCaixa(p, tx, 534, 2, 16, i == 0 ? FT_COR_VERDE : 0xFF536273);
    }
    if (ftAfinSinal) {
        float cm = cents;
        if (cm > 50.0f) cm = 50.0f;
        if (cm < -50.0f) cm = -50.0f;
        int nx = 960 + (int)(cm * 8.0f);
        ftCaixa(p, nx - 4, 500, 8, 90, fabs(cents) <= 4.0f ? FT_COR_VERDE : FT_COR_AMARELO);
    }
    if (centsTxt[0]) ftTextoCentral(p, centsTxt, 30, 600, corEstado);

    if (ftAfinModo == 1) {
        char modo[64];
        snprintf(modo, sizeof(modo), "MODO VIOLAO - corda mais proxima: %s", cordaAtual >= 0 ? ftAfinCordasNomes[cordaAtual] : "--");
        ftTextoCentral(p, modo, 24, 660, FT_COR_CINZA);
        for (int i = 0; i < 6; i++) {
            int bx = 300 + i * 220;
            bool ativa = (i == cordaAtual);
            ftCaixa(p, bx, 710, 200, 90, ativa ? FT_COR_CARTAO_SEL : FT_COR_FUNDO);
            ftCaixa(p, bx, 710, 6, 90, ativa ? FT_COR_VERDE : 0xFF2A3644);
            desenharTexto(p, ftAfinCordasNomes[i], 30, bx + 22, 730, ativa ? FT_COR_VERDE : FT_COR_BRANCO);
            char hz[32];
            snprintf(hz, sizeof(hz), "%.1f Hz", ftAfinCordasFreq[i]);
            desenharTexto(p, hz, 20, bx + 22, 772, FT_COR_CINZA);
        }
    }
    else {
        ftTextoCentral(p, "MODO CROMATICO - qualquer nota musical", 24, 660, FT_COR_CINZA);
        ftTextoCentral(p, "Quadrado alterna cromatico / violao", 24, 720, FT_COR_CINZA);
    }

    if (ftAfinErro) {
        ftTextoCentral(p, "Ligue um fone com microfone ou use o microfone do controle.", 24, 840, FT_COR_VERMELHO);
    }
    ftRodape(p, "X ouvir/parar   Quadrado cromatico/violao   Bolinha voltar");
}

// ---------------------------------------------------------------- contatos
#define FT_MAX_CONTATOS 60
struct FtContato { char nome[64]; char fone[48]; };
static FtContato ftContatos[FT_MAX_CONTATOS];
static int ftContTotal = 0;
static int ftContSel = 0;
static bool ftContDetalhe = false;
static char ftContNomeTemp[64];

static void ftContatosSalvar(void) {
    ftMkdirs(FT_CONF);
    FILE* f = fopen(FT_CONF "/contatos_hn.txt", "w");
    if (!f) return;
    for (int i = 0; i < ftContTotal; i++) fprintf(f, "%s|%s\n", ftContatos[i].nome, ftContatos[i].fone);
    fclose(f);
}

static void ftContatosCarregar(void) {
    ftContTotal = 0;
    FILE* f = fopen(FT_CONF "/contatos_hn.txt", "r");
    if (f) {
        char linha[256];
        while (fgets(linha, sizeof(linha), f) && ftContTotal < FT_MAX_CONTATOS) {
            linha[strcspn(linha, "\r\n")] = 0;
            char* barra = strchr(linha, '|');
            if (!barra) continue;
            *barra = 0;
            snprintf(ftContatos[ftContTotal].nome, sizeof(ftContatos[0].nome), "%s", linha);
            snprintf(ftContatos[ftContTotal].fone, sizeof(ftContatos[0].fone), "%s", barra + 1);
            ftContTotal++;
        }
        fclose(f);
    }
    if (ftContSel >= ftContTotal) ftContSel = ftContTotal - 1;
    if (ftContSel < 0) ftContSel = 0;
}

static void ftDesenharContatos(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "CONTATOS", "X abrir   Quadrado novo   Triangulo apagar   Bolinha volta");
    if (ftContDetalhe && ftContTotal > 0) {
        ftCaixa(p, 260, 170, 1400, 620, FT_COR_CARTAO);
        desenharTexto(p, ftContatos[ftContSel].nome, 48, 320, 230, FT_COR_AZUL);
        desenharTexto(p, ftContatos[ftContSel].fone, 40, 320, 320, FT_COR_BRANCO);
        desenharTexto(p, "X ou Bolinha volta para a lista", 24, 320, 430, FT_COR_CINZA);
    }
    else {
        int inicio = ftContSel - 10;
        if (inicio < 0) inicio = 0;
        if (inicio > ftContTotal - 12) inicio = ftContTotal - 12;
        if (inicio < 0) inicio = 0;
        int fim = inicio + 12;
        if (fim > ftContTotal) fim = ftContTotal;
        for (int i = inicio; i < fim; i++) {
            int y = 170 + (i - inicio) * 64;
            if (i == ftContSel) ftCaixa(p, 220, y - 8, 1480, 54, FT_COR_CARTAO_SEL);
            desenharTexto(p, ftContatos[i].nome, 30, 260, y, i == ftContSel ? FT_COR_AMARELO : FT_COR_BRANCO);
            desenharTexto(p, ftContatos[i].fone, 26, 900, y + 4, FT_COR_CINZA);
        }
        if (!ftContTotal) ftTextoCentral(p, "Nenhum contato. Aperte Quadrado para criar.", 30, 360, FT_COR_CINZA);
    }
    ftRodape(p, "Contatos locais do Hyper Neiva. O Telegram continua no menu EXTRA.");
}

// ---------------------------------------------------------------- caixa de entrada
#define FT_MAX_MSGS 60
struct FtMsg { char de[64]; char assunto[80]; char texto[400]; };
static FtMsg ftMsgs[FT_MAX_MSGS];
static int ftMsgTotal = 0;
static int ftMsgSel = 0;
static bool ftMsgDetalhe = false;
static char ftMsgDeTemp[64];
static char ftMsgAssuntoTemp[80];

static void ftMsgsSalvar(void) {
    ftMkdirs(FT_CONF);
    FILE* f = fopen(FT_CONF "/caixa_hn.txt", "w");
    if (!f) return;
    for (int i = 0; i < ftMsgTotal; i++) {
        fprintf(f, "%s|%s|", ftMsgs[i].de, ftMsgs[i].assunto);
        for (const char* s = ftMsgs[i].texto; *s; s++) fputc(*s == '\n' ? ' ' : *s, f);
        fputc('\n', f);
    }
    fclose(f);
}

static void ftMsgsCarregar(void) {
    ftMsgTotal = 0;
    FILE* f = fopen(FT_CONF "/caixa_hn.txt", "r");
    if (f) {
        char linha[600];
        while (fgets(linha, sizeof(linha), f) && ftMsgTotal < FT_MAX_MSGS) {
            linha[strcspn(linha, "\r\n")] = 0;
            char* p1 = strchr(linha, '|'); if (!p1) continue;
            char* p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            *p1 = 0; *p2 = 0;
            snprintf(ftMsgs[ftMsgTotal].de, sizeof(ftMsgs[0].de), "%s", linha);
            snprintf(ftMsgs[ftMsgTotal].assunto, sizeof(ftMsgs[0].assunto), "%s", p1 + 1);
            snprintf(ftMsgs[ftMsgTotal].texto, sizeof(ftMsgs[0].texto), "%s", p2 + 1);
            ftMsgTotal++;
        }
        fclose(f);
    }
    if (ftMsgSel >= ftMsgTotal) ftMsgSel = ftMsgTotal - 1;
    if (ftMsgSel < 0) ftMsgSel = 0;
}

static void ftDesenharCaixa(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "CAIXA DE ENTRADA", "X ler   Quadrado nova   Triangulo apagar   Bolinha volta");
    if (ftMsgDetalhe && ftMsgTotal > 0) {
        ftCaixa(p, 220, 150, 1480, 720, FT_COR_CARTAO);
        char cab[160];
        snprintf(cab, sizeof(cab), "De: %s", ftMsgs[ftMsgSel].de);
        desenharTexto(p, cab, 30, 280, 190, FT_COR_AZUL);
        snprintf(cab, sizeof(cab), "Assunto: %s", ftMsgs[ftMsgSel].assunto);
        desenharTexto(p, cab, 30, 280, 240, FT_COR_BRANCO);
        desenharTexto(p, ftMsgs[ftMsgSel].texto, 26, 280, 320, FT_COR_BRANCO);
        desenharTexto(p, "X ou Bolinha volta para a lista", 24, 280, 810, FT_COR_CINZA);
    }
    else {
        int inicio = ftMsgSel - 9;
        if (inicio < 0) inicio = 0;
        if (inicio > ftMsgTotal - 11) inicio = ftMsgTotal - 11;
        if (inicio < 0) inicio = 0;
        int fim = inicio + 11;
        if (fim > ftMsgTotal) fim = ftMsgTotal;
        for (int i = inicio; i < fim; i++) {
            int y = 150 + (i - inicio) * 70;
            if (i == ftMsgSel) ftCaixa(p, 180, y - 8, 1560, 60, FT_COR_CARTAO_SEL);
            char linha[160];
            snprintf(linha, sizeof(linha), "%s - %s", ftMsgs[i].de, ftMsgs[i].assunto);
            desenharTexto(p, linha, 28, 220, y, i == ftMsgSel ? FT_COR_AMARELO : FT_COR_BRANCO);
        }
        if (!ftMsgTotal) ftTextoCentral(p, "Nenhuma mensagem. Aperte Quadrado para escrever.", 30, 360, FT_COR_CINZA);
    }
    ftRodape(p, "Caixa local de mensagens do Hyper Neiva.");
}

// ---------------------------------------------------------------- terminal
#define FT_TERM_LINHAS 80
#define FT_TERM_TAM 220
static char ftTermLinhas[FT_TERM_LINHAS][FT_TERM_TAM];
static int ftTermTotal = 0;
static int ftTermScroll = 0;
static char ftTermUltimo[256] = "";

static void ftTermAdd(const char* fmt, ...) {
    if (ftTermTotal >= FT_TERM_LINHAS) {
        memmove(ftTermLinhas[0], ftTermLinhas[1], sizeof(ftTermLinhas[0]) * (FT_TERM_LINHAS - 1));
        ftTermTotal = FT_TERM_LINHAS - 1;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ftTermLinhas[ftTermTotal], FT_TERM_TAM, fmt, ap);
    va_end(ap);
    ftTermTotal++;
    ftTermScroll = 0;
}

static void ftTermCmd(const char* comando) {
    char copia[256];
    snprintf(copia, sizeof(copia), "%s", comando);
    char* p = copia;
    while (*p == ' ') p++;
    char* arg = strchr(p, ' ');
    if (arg) { *arg = 0; arg++; while (*arg == ' ') arg++; }
    else arg = (char*)"";

    ftTermAdd("> %s", comando);
    if (!strcmp(p, "help") || !strcmp(p, "ajuda")) {
        ftTermAdd("Comandos: help, cls, echo, date, hora, ver, pwd, dir [pasta], cat [arquivo]");
        ftTermAdd("O opencode e o cmd do Windows nao rodam no PS4.");
    }
    else if (!strcmp(p, "cls") || !strcmp(p, "limpar")) {
        ftTermTotal = 0;
        ftTermScroll = 0;
    }
    else if (!strcmp(p, "echo")) ftTermAdd("%s", arg);
    else if (!strcmp(p, "date") || !strcmp(p, "data") || !strcmp(p, "hora")) {
        struct tm t; ftDataHora(&t);
        ftTermAdd("%02d/%02d/%04d %02d:%02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
    }
    else if (!strcmp(p, "ver")) ftTermAdd("Hyper Neiva 3.79 - PS4 (OpenOrbis)");
    else if (!strcmp(p, "pwd")) ftTermAdd(FT_RAIZ);
    else if (!strcmp(p, "dir") || !strcmp(p, "ls")) {
        const char* pasta = arg[0] ? arg : FT_RAIZ;
        DIR* d = opendir(pasta);
        if (!d) { ftTermAdd("Nao consegui abrir: %s", pasta); return; }
        ftTermAdd("Conteudo de %s:", pasta);
        struct dirent* e;
        int n = 0;
        while ((e = readdir(d)) && n < 18) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            ftTermAdd("  %s", e->d_name);
            n++;
        }
        closedir(d);
        if (!n) ftTermAdd("  (vazia)");
    }
    else if (!strcmp(p, "cat")) {
        if (!arg[0]) { ftTermAdd("Use: cat /data/HyperNeiva/arquivo.txt"); return; }
        if (strncmp(arg, FT_RAIZ, strlen(FT_RAIZ)) != 0) { ftTermAdd("So leio arquivos dentro de " FT_RAIZ); return; }
        FILE* f = fopen(arg, "r");
        if (!f) { ftTermAdd("Nao achei: %s", arg); return; }
        char linha[200];
        int n = 0;
        while (fgets(linha, sizeof(linha), f) && n < 16) { ftTermAdd("  %s", linha); n++; }
        fclose(f);
        if (!n) ftTermAdd("  (arquivo vazio)");
    }
    else if (!strcmp(p, "opencode") || !strcmp(p, "cmd")) {
        ftTermAdd("O opencode/cmd do PC nao existe no PS4. Use o terminal do PC para isso.");
    }
    else if (!p[0]) {
    }
    else ftTermAdd("Comando desconhecido: %s (digite help)", p);
}

static void ftDesenharTerminal(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "TERMINAL DO HYPER NEIVA", "X digitar comando   Cima/baixo rolar   Bolinha volta");
    ftCaixa(p, 120, 110, 1680, 820, 0xFF05080Bu);
    ftCaixa(p, 120, 110, 1680, 2, FT_COR_AZUL);
    int maxLinhas = 24;
    int inicio = ftTermTotal - maxLinhas - ftTermScroll;
    if (inicio < 0) inicio = 0;
    int fim = inicio + maxLinhas;
    if (fim > ftTermTotal) fim = ftTermTotal;
    for (int i = inicio; i < fim; i++) {
        desenharTexto(p, ftTermLinhas[i], 23, 150, 140 + (i - inicio) * 31, FT_COR_VERDE);
    }
    if (ftTermTotal == 0) {
        desenharTexto(p, "Hyper Neiva Terminal. Digite help para ver os comandos.", 26, 150, 160, FT_COR_CINZA);
    }
    desenharTexto(p, "X: novo comando", 24, 150, 950, FT_COR_CINZA);
    ftRodape(p, "Console do app (nao e o shell do Windows). Comandos seguros e somente leitura.");
}

// ---------------------------------------------------------------- QR / barras
static int ftQrAba = 0;
static int ftQrCampo = 0;
static char ftQrTexto[128] = "https://github.com/hyperneiva";
static char ftQrFone[40] = "";
static char ftQrMsg[160] = "Ola! Estou usando o Hyper Neiva.";
static char ftQrAviso[96] = "";

static void ftUrlCodificar(const char* entrada, char* saida, int tam) {
    static const char* hex = "0123456789ABCDEF";
    int n = 0;
    for (const unsigned char* s = (const unsigned char*)entrada; *s && n < tam - 4; s++) {
        if (isalnum(*s) || *s == '-' || *s == '_' || *s == '.' || *s == '~') saida[n++] = *s;
        else {
            saida[n++] = '%';
            saida[n++] = hex[*s >> 4];
            saida[n++] = hex[*s & 15];
        }
    }
    saida[n] = 0;
}

static void ftQrMontarLink(char* saida, int tam) {
    char fone[40];
    int n = 0;
    for (const char* s = ftQrFone; *s && n < 39; s++) if (isdigit((unsigned char)*s)) fone[n++] = *s;
    fone[n] = 0;
    char msg[600];
    ftUrlCodificar(ftQrMsg, msg, sizeof(msg));
    if (fone[0]) snprintf(saida, tam, "https://wa.me/%s?text=%s", fone, msg);
    else snprintf(saida, tam, "https://wa.me/?text=%s", msg);
}

static void ftDesenharCodigoBarras(uint32_t* p, const char* dados, int x, int y, int largura, int altura) {
    static const char* padrao[10] = { "NNWWN", "WNNNW", "NWNNW", "WWNNN", "NNWNW", "WNWNN", "NWWNN", "NNNWW", "WNNWN", "NWNWN" };
    char digitos[64];
    int n = 0;
    for (const char* s = dados; *s && n < 60; s++) if (isdigit((unsigned char)*s)) digitos[n++] = *s;
    if (n == 0) return;
    if (n % 2) {
        memmove(digitos + 1, digitos, n + 1);
        digitos[0] = '0';
        n++;
    }
    digitos[n] = 0;
    int unidade = largura / (n / 2 * 14 + 8);
    if (unidade < 2) unidade = 2;
    int cx = x;
    int total = 0;
    // Inicio: barra estreita, espaco estreito, barra estreita, espaco estreito.
    ftCaixa(p, cx, y, unidade, altura, FT_COR_BRANCO); cx += unidade;
    cx += unidade;
    ftCaixa(p, cx, y, unidade, altura, FT_COR_BRANCO); cx += unidade;
    cx += unidade;
    total += 4;
    for (int par = 0; par < n; par += 2) {
        const char* a = padrao[digitos[par] - '0'];
        const char* b = padrao[digitos[par + 1] - '0'];
        for (int k = 0; k < 5; k++) {
            int wa = a[k] == 'W' ? 3 : 1;
            int wb = b[k] == 'W' ? 3 : 1;
            ftCaixa(p, cx, y, wa * unidade, altura, FT_COR_BRANCO); cx += wa * unidade;
            cx += wb * unidade;
            total += wa + wb;
        }
    }
    // Fim: barra larga, espaco estreito, barra estreita.
    ftCaixa(p, cx, y, 3 * unidade, altura, FT_COR_BRANCO); cx += 3 * unidade;
    cx += unidade;
    ftCaixa(p, cx, y, unidade, altura, FT_COR_BRANCO); cx += unidade;
    total += 5;
    ftCaixa(p, x, y + altura + 8, total * unidade, 2, FT_COR_CINZA);
    desenharTexto(p, digitos, 24, x, y + altura + 20, FT_COR_CINZA);
}

static void ftDesenharQr(uint32_t* p) {
    ftFundo(p);
    ftTopo(p, "QR E CODIGO DE BARRAS", "L1/R1 muda a aba   X digitar   Bolinha volta");
    static const char* abas[] = { "QR CODE", "WHATSAPP", "COD. BARRAS", "LER" };
    for (int i = 0; i < 4; i++) {
        int x = 230 + i * 350;
        bool ativo = (i == ftQrAba);
        ftCaixa(p, x, 120, 320, 56, ativo ? FT_COR_AZUL : FT_COR_CARTAO);
        int largura = (int)(strlen(abas[i]) * 24 * 0.56f);
        desenharTexto(p, abas[i], 24, x + (320 - largura) / 2, 134, ativo ? 0xFF001018 : FT_COR_BRANCO);
    }

    if (ftQrAba == 0) {
        desenharTexto(p, "Texto ou link:", 26, 240, 230, FT_COR_CINZA);
        desenharTexto(p, ftQrTexto, 26, 240, 280, FT_COR_BRANCO);
        if (strlen(ftQrTexto) > 0 && strlen(ftQrTexto) <= 106) remotoDesenharQrTexto(p, ftQrTexto, 760, 300, 9);
        else ftTextoCentral(p, "X para digitar um texto/link (ate 106 caracteres)", 28, 420, FT_COR_CINZA);
        desenharTexto(p, "X digitar texto ou link", 24, 240, 820, FT_COR_CINZA);
    }
    else if (ftQrAba == 1) {
        char link[700];
        ftQrMontarLink(link, sizeof(link));
        desenharTexto(p, "Telefone (com DDI/DDD):", 26, 240, 220, ftQrCampo == 0 ? FT_COR_AMARELO : FT_COR_CINZA);
        desenharTexto(p, ftQrFone[0] ? ftQrFone : "(vazio)", 30, 240, 260, FT_COR_BRANCO);
        desenharTexto(p, "Mensagem:", 26, 240, 330, ftQrCampo == 1 ? FT_COR_AMARELO : FT_COR_CINZA);
        desenharTexto(p, ftQrMsg, 26, 240, 370, FT_COR_BRANCO);
        if (strlen(link) <= 106) remotoDesenharQrTexto(p, link, 1300, 300, 7);
        else ftTextoCentral(p, "Link muito longo para o QR. Encurte a mensagem.", 28, 440, FT_COR_VERMELHO);
        desenharTexto(p, "Esquerda/direita escolhe o campo   X digitar", 24, 240, 820, FT_COR_CINZA);
    }
    else if (ftQrAba == 2) {
        desenharTexto(p, "Codigo 2 de 5 intercalado (numeros):", 26, 240, 230, FT_COR_CINZA);
        desenharTexto(p, ftQrTexto, 28, 240, 280, FT_COR_BRANCO);
        ftDesenharCodigoBarras(p, ftQrTexto, 320, 380, 1280, 220);
        desenharTexto(p, "X digitar os numeros", 24, 240, 820, FT_COR_CINZA);
    }
    else {
        ftTextoCentral(p, "LEITOR DE CODIGO", 40, 260, FT_COR_AZUL);
        ftTextoCentral(p, "O leitor precisa da camera do PS4 e ainda nao foi", 28, 330, FT_COR_BRANCO);
        ftTextoCentral(p, "integrado nesta versao. Use o QR para gerar codigos.", 28, 370, FT_COR_BRANCO);
        ftTextoCentral(p, "O Telegram e o Meet ja leem QR pelo celular.", 26, 440, FT_COR_CINZA);
    }
    if (ftQrAviso[0]) desenharTexto(p, ftQrAviso, 24, 240, 880, FT_COR_AMARELO);
    ftRodape(p, "QR com correcao L para links curtos. WhatsApp abre no celular de quem ler.");
}

// ---------------------------------------------------------------- IME resultados
static void ftImeResultado(int modo, const char* texto) {
    if (modo == FT_IME_CONTATO_NOME) {
        snprintf(ftContNomeTemp, sizeof(ftContNomeTemp), "%s", texto);
        ftImeAbrir(FT_IME_CONTATO_FONE, "Telefone do contato", "", 40);
        return;
    }
    if (modo == FT_IME_CONTATO_FONE) {
        if (ftContTotal < FT_MAX_CONTATOS && ftContNomeTemp[0]) {
            snprintf(ftContatos[ftContTotal].nome, sizeof(ftContatos[0].nome), "%s", ftContNomeTemp);
            snprintf(ftContatos[ftContTotal].fone, sizeof(ftContatos[0].fone), "%s", texto);
            ftContTotal++;
            ftContSel = ftContTotal - 1;
            ftContatosSalvar();
            ftAviso("Contato salvo.");
        }
        ftContNomeTemp[0] = 0;
        return;
    }
    if (modo == FT_IME_MSG_DE) {
        snprintf(ftMsgDeTemp, sizeof(ftMsgDeTemp), "%s", texto);
        ftImeAbrir(FT_IME_MSG_ASSUNTO, "Assunto da mensagem", "", 70);
        return;
    }
    if (modo == FT_IME_MSG_ASSUNTO) {
        snprintf(ftMsgAssuntoTemp, sizeof(ftMsgAssuntoTemp), "%s", texto);
        ftImeAbrir(FT_IME_MSG_TEXTO, "Texto da mensagem", "", 380);
        return;
    }
    if (modo == FT_IME_MSG_TEXTO) {
        if (ftMsgTotal < FT_MAX_MSGS) {
            snprintf(ftMsgs[ftMsgTotal].de, sizeof(ftMsgs[0].de), "%s", ftMsgDeTemp[0] ? ftMsgDeTemp : "Eu");
            snprintf(ftMsgs[ftMsgTotal].assunto, sizeof(ftMsgs[0].assunto), "%s", ftMsgAssuntoTemp[0] ? ftMsgAssuntoTemp : "(sem assunto)");
            snprintf(ftMsgs[ftMsgTotal].texto, sizeof(ftMsgs[0].texto), "%s", texto);
            ftMsgTotal++;
            ftMsgSel = ftMsgTotal - 1;
            ftMsgsSalvar();
            ftAviso("Mensagem salva na caixa.");
        }
        ftMsgDeTemp[0] = ftMsgAssuntoTemp[0] = 0;
        return;
    }
    if (modo == FT_IME_TERMINAL) {
        snprintf(ftTermUltimo, sizeof(ftTermUltimo), "%s", texto);
        ftTermCmd(texto);
        return;
    }
    if (modo == FT_IME_QR_TEXTO) {
        snprintf(ftQrTexto, sizeof(ftQrTexto), "%s", texto);
        ftQrAviso[0] = 0;
        return;
    }
    if (modo == FT_IME_QR_FONE) {
        snprintf(ftQrFone, sizeof(ftQrFone), "%s", texto);
        ftQrCampo = 1;
        return;
    }
    if (modo == FT_IME_QR_MSG) {
        snprintf(ftQrMsg, sizeof(ftQrMsg), "%s", texto);
        return;
    }
    if (modo == FT_IME_CALC_VALOR) {
        double valor = 0.0;
        if (ftCalcTextoNumero(texto, &valor) && ftCalcImeCat >= 0 && ftCalcImeCampo >= 0) {
            ftCalcSetCampo(ftCalcImeCat, ftCalcImeCampo, valor);
        }
        ftCalcImeCat = -1;
        ftCalcImeCampo = -1;
        return;
    }
    if (modo == FT_IME_QUOTE_BUSCA) {
        ftQuoteBuscarTitulos(texto);
        return;
    }
    if (modo == FT_IME_QUOTE_COPIAR) {
        snprintf(ftQuoteClipboard, sizeof(ftQuoteClipboard), "%s", texto);
        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Texto pronto no teclado: %.120s", ftQuoteClipboard);
        return;
    }
}

// ---------------------------------------------------------------- abrir ferramenta
static void ftEmpilharFerramentas(void) {
    empilharMenu(MENU_FERRAMENTAS, sel, 0);
}

static void ftAbrirFerramenta(void) {
    if (ftSel == 0) {
        ftTela = FT_TELA_CALC;
        ftCalcAbrir();
    }
    else if (ftSel == 1) {
        ftMkdirs(FT_NOTAS);
        inicializarNotepad();
        snprintf(pastaDestinoFinal, sizeof(pastaDestinoFinal), "%s", FT_NOTAS);
        nomeArquivo[0] = 0;
        ftEmpilharFerramentas();
        menuAtual = MENU_NOTEPAD;
    }
    else if (ftSel == 2) {
        ftTela = FT_TELA_RELOGIO;
        ftRelogioAba = 0;
        ftAlarmesCarregar();
    }
    else if (ftSel == 3) {
        ftTela = FT_TELA_GRAVADOR;
        ftGravErro = 0;
        ftGravArquivo[0] = 0;
    }
    else if (ftSel == 4) {
        ftTela = FT_TELA_AFINADOR;
        ftAfinModo = 0;
        ftAfinIniciar();
    }
    else if (ftSel == 5) {
        ftTela = FT_TELA_CONTATOS;
        ftContatosCarregar();
        ftContDetalhe = false;
    }
    else if (ftSel == 6) {
        ftTela = FT_TELA_CAIXA;
        ftMsgsCarregar();
        ftMsgDetalhe = false;
    }
    else if (ftSel == 7) {
        ftTela = FT_TELA_TERMINAL;
        if (ftTermTotal == 0) {
            ftTermAdd("Hyper Neiva Terminal 3.87");
            ftTermAdd("Digite help para ver os comandos.");
        }
    }
    else if (ftSel == 8) {
        ftEmpilharFerramentas();
        iaAbrir();
    }
    else if (ftSel == 9) {
        ftTela = FT_TELA_QR;
        ftQrAba = 0;
        ftQrAviso[0] = 0;
    }
    else if (ftSel == 10) {
        ftEmpilharFerramentas();
        tempoAbrir();
    }
}

// ---------------------------------------------------------------- controles
static bool ftControles(uint32_t botoes, uint32_t antigos, int32_t uId) {
    (void)uId;
    ftImeProcessar();
    if (ftImeAberto) return false;
    uint32_t novo = botoes & ~antigos;

    if (ftTela == FT_TELA_MENU) {
        if (novo & ORBIS_PAD_BUTTON_LEFT) ftSel = (ftSel + FT_TOTAL - 1) % FT_TOTAL;
        if (novo & ORBIS_PAD_BUTTON_RIGHT) ftSel = (ftSel + 1) % FT_TOTAL;
        if (novo & ORBIS_PAD_BUTTON_UP) ftSel = (ftSel + FT_TOTAL - 5) % FT_TOTAL;
        if (novo & ORBIS_PAD_BUTTON_DOWN) ftSel = (ftSel + 5) % FT_TOTAL;
        if (novo & ORBIS_PAD_BUTTON_CROSS) ftAbrirFerramenta();
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) return true;
        return false;
    }

    if (ftTela == FT_TELA_CALC) {
        if (ftCalcListaAberta) {
            if (novo & ORBIS_PAD_BUTTON_UP) ftCalcListaMover(-1);
            if (novo & ORBIS_PAD_BUTTON_DOWN) ftCalcListaMover(1);
            if (novo & ORBIS_PAD_BUTTON_CROSS) ftCalcListaConfirmar();
            if (novo & ORBIS_PAD_BUTTON_CIRCLE) ftCalcListaAberta = false;
            return false;
        }
        if (ftCalcRecurso >= 3 && ftQuoteCalAberto) {
            if (novo & ORBIS_PAD_BUTTON_LEFT) ftQuoteCalMover(1, -1);
            if (novo & ORBIS_PAD_BUTTON_RIGHT) ftQuoteCalMover(1, 1);
            if (novo & ORBIS_PAD_BUTTON_UP) ftQuoteCalMover(0, -7);
            if (novo & ORBIS_PAD_BUTTON_DOWN) ftQuoteCalMover(0, 7);
            if (novo & ORBIS_PAD_BUTTON_L2) ftQuoteCalMover(2, -1);
            if (novo & ORBIS_PAD_BUTTON_R2) ftQuoteCalMover(2, 1);
            if (novo & ORBIS_PAD_BUTTON_CROSS) ftQuoteCalConfirmar();
            if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
                ftQuoteCalAberto = false;
                snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Calendario cancelado.");
            }
            return false;
        }
        if (ftCalcRecurso >= 3 && ftQuoteMenu != FT_QMENU_NENHUM) {
            int total = 0;
            if (ftQuoteMenu == FT_QMENU_COPIAR) total = 4;
            else if (ftQuoteMenu == FT_QMENU_OPCOES) total = 6;
            else total = ftQuoteBuscaTotal;
            if (novo & ORBIS_PAD_BUTTON_UP) {
                if (total > 0) ftQuoteMenuSel = (ftQuoteMenuSel + total - 1) % total;
            }
            if (novo & ORBIS_PAD_BUTTON_DOWN) {
                if (total > 0) ftQuoteMenuSel = (ftQuoteMenuSel + 1) % total;
            }
            if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
                if (ftQuoteMenu == FT_QMENU_BUSCA) ftQuoteMenu = FT_QMENU_OPCOES;
                else ftQuoteMenu = FT_QMENU_NENHUM;
                return false;
            }
            if (novo & ORBIS_PAD_BUTTON_CROSS) {
                if (ftQuoteMenu == FT_QMENU_COPIAR) {
                    if (ftQuoteMenuSel == 0) ftQuoteCopiarAtual();
                    else if (ftQuoteMenuSel == 1) {
                        ftQuotePrintPendente = true;
                        snprintf(ftQuoteAviso, sizeof(ftQuoteAviso), "Gerando print da tela...");
                    }
                    else if (ftQuoteMenuSel == 2) ftImeAbrir(FT_IME_QUOTE_COPIAR, "Texto copiado", ftQuoteClipboard, 220);
                    ftQuoteMenu = FT_QMENU_NENHUM;
                }
                else if (ftQuoteMenu == FT_QMENU_OPCOES) {
                    if (ftQuoteMenuSel == 0) {
                        ftQuoteAtualizar(ftQuoteSel);
                        ftQuoteMenu = FT_QMENU_NENHUM;
                    }
                    else if (ftQuoteMenuSel == 1) {
                        ftImeAbrir(FT_IME_QUOTE_BUSCA, "Buscar titulo", "", 60);
                        ftQuoteMenu = FT_QMENU_NENHUM;
                    }
                    else if (ftQuoteMenuSel == 2) {
                        ftQuoteRemoverAtual();
                        ftQuoteMenu = FT_QMENU_NENHUM;
                    }
                    else if (ftQuoteMenuSel == 3) {
                        ftQuoteMoverAtual(-1);
                        ftQuoteMenu = FT_QMENU_NENHUM;
                    }
                    else if (ftQuoteMenuSel == 4) {
                        ftQuoteMoverAtual(1);
                        ftQuoteMenu = FT_QMENU_NENHUM;
                    }
                    else ftQuoteMenu = FT_QMENU_NENHUM;
                }
                else if (ftQuoteMenu == FT_QMENU_BUSCA) {
                    if (ftQuoteMenuSel >= 0 && ftQuoteMenuSel < ftQuoteBuscaTotal) {
                        ftQuoteAdicionarDef(ftQuoteBuscaRes[ftQuoteMenuSel]);
                    }
                    ftQuoteMenu = FT_QMENU_NENHUM;
                }
                return false;
            }
            return false;
        }
        if (novo & ORBIS_PAD_BUTTON_R1) ftCalcTrocarRecurso(1);
        if (novo & ORBIS_PAD_BUTTON_L1) ftCalcTrocarRecurso(-1);
        if (ftCalcRecurso == 0) {
            if (novo & ORBIS_PAD_BUTTON_LEFT) ftCalcMover(0);
            if (novo & ORBIS_PAD_BUTTON_RIGHT) ftCalcMover(1);
            if (novo & ORBIS_PAD_BUTTON_UP) ftCalcMover(2);
            if (novo & ORBIS_PAD_BUTTON_DOWN) ftCalcMover(3);
            if (novo & ORBIS_PAD_BUTTON_CROSS) ftCalcPressionar();
            if (novo & ORBIS_PAD_BUTTON_SQUARE) { ftCalcCientifica = !ftCalcCientifica; ftCalcBotao = 0; }
        }
        else if (ftCalcRecurso <= 2) {
            if (novo & ORBIS_PAD_BUTTON_R2) ftCalcTrocarCategoria(1);
            if (novo & ORBIS_PAD_BUTTON_L2) ftCalcTrocarCategoria(-1);
            int total = ftCalcCampoTotal(ftCalcCategoria);
            if (total > 0) {
                if (novo & ORBIS_PAD_BUTTON_UP) ftCalcCampo = (ftCalcCampo + total - 1) % total;
                if (novo & ORBIS_PAD_BUTTON_DOWN) ftCalcCampo = (ftCalcCampo + 1) % total;
                if (novo & ORBIS_PAD_BUTTON_LEFT) ftCalcAjustarCampo(-1);
                if (novo & ORBIS_PAD_BUTTON_RIGHT) ftCalcAjustarCampo(1);
                if (novo & ORBIS_PAD_BUTTON_CROSS) ftCalcEditarCampo();
            }
            if (novo & ORBIS_PAD_BUTTON_SQUARE) {
                ftCalcCamposInit[ftCalcCategoria] = false;
                ftCalcIniciarCampos(ftCalcCategoria);
                ftCalcCampo = 0;
            }
        }
        else {
            int grupo = ftCalcRecurso == 4 ? 1 : 0;
            ftQuoteAbrir(grupo);
            if (ftQuoteTotal > 0) {
                if (novo & ORBIS_PAD_BUTTON_UP) { ftQuoteSel = (ftQuoteSel + ftQuoteTotal - 1) % ftQuoteTotal; ftQuoteAtualizar(ftQuoteSel); }
                if (novo & ORBIS_PAD_BUTTON_DOWN) { ftQuoteSel = (ftQuoteSel + 1) % ftQuoteTotal; ftQuoteAtualizar(ftQuoteSel); }
                if (novo & ORBIS_PAD_BUTTON_LEFT) {
                    ftQuotePeriodo = (ftQuotePeriodo + 3) % 4;
                    ftQuoteAtualizar(ftQuoteSel);
                }
                if (novo & ORBIS_PAD_BUTTON_RIGHT) {
                    ftQuotePeriodo = (ftQuotePeriodo + 1) % 4;
                    ftQuoteAtualizar(ftQuoteSel);
                }
                if (ftQuotePeriodo == 3 && (novo & ORBIS_PAD_BUTTON_R2)) {
                    ftQuoteInicioEpoch = 0;
                    ftQuoteFimEpoch = 0;
                    ftQuoteDias += 30; if (ftQuoteDias > 3650) ftQuoteDias = 3650;
                    ftQuoteAtualizar(ftQuoteSel);
                }
                if (ftQuotePeriodo == 3 && (novo & ORBIS_PAD_BUTTON_L2)) {
                    ftQuoteInicioEpoch = 0;
                    ftQuoteFimEpoch = 0;
                    ftQuoteDias -= 30; if (ftQuoteDias < 7) ftQuoteDias = 7;
                    ftQuoteAtualizar(ftQuoteSel);
                }
                if (novo & ORBIS_PAD_BUTTON_CROSS) {
                    if (ftQuotePeriodo == 3) ftQuoteAbrirCalendario(0);
                    else ftQuoteAtualizar(ftQuoteSel);
                }
                if (novo & ORBIS_PAD_BUTTON_SQUARE) {
                    ftQuoteMenu = FT_QMENU_COPIAR;
                    ftQuoteMenuSel = 0;
                }
                if (novo & ORBIS_PAD_BUTTON_TRIANGLE) {
                    ftQuoteMenu = FT_QMENU_OPCOES;
                    ftQuoteMenuSel = 0;
                }
            }
        }
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) ftTela = FT_TELA_MENU;
        return false;
    }

    if (ftTela == FT_TELA_RELOGIO) {
        if (novo & ORBIS_PAD_BUTTON_L1) ftRelogioAba = (ftRelogioAba + 3) % 4;
        if (novo & ORBIS_PAD_BUTTON_R1) ftRelogioAba = (ftRelogioAba + 1) % 4;
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) ftTela = FT_TELA_MENU;
        if (ftRelogioAba == 0) {
            // so mostra
        }
        else if (ftRelogioAba == 1) {
            if (novo & ORBIS_PAD_BUTTON_CROSS) {
                if (ftCronoRodando) { ftCronoAcumulado = ftCronoTempo(); ftCronoRodando = false; }
                else { ftCronoInicio = ftSegundos(); ftCronoRodando = true; }
            }
            if (novo & ORBIS_PAD_BUTTON_SQUARE) {
                ftCronoRodando = false; ftCronoAcumulado = 0; ftCronoTotalVoltas = 0;
            }
            if (novo & ORBIS_PAD_BUTTON_TRIANGLE) {
                if (ftCronoTotalVoltas < 5) ftCronoVoltas[ftCronoTotalVoltas++] = ftCronoTempo();
            }
        }
        else if (ftRelogioAba == 2) {
            if (novo & ORBIS_PAD_BUTTON_LEFT) ftTimerCampo = (ftTimerCampo + 2) % 3;
            if (novo & ORBIS_PAD_BUTTON_RIGHT) ftTimerCampo = (ftTimerCampo + 1) % 3;
            if (novo & ORBIS_PAD_BUTTON_UP) {
                if (ftTimerCampo == 0) ftTimerH = (ftTimerH + 1) % 24;
                else if (ftTimerCampo == 1) ftTimerM = (ftTimerM + 1) % 60;
                else ftTimerS = (ftTimerS + 1) % 60;
                ftTimerConcluido = false;
            }
            if (novo & ORBIS_PAD_BUTTON_DOWN) {
                if (ftTimerCampo == 0) ftTimerH = (ftTimerH + 23) % 24;
                else if (ftTimerCampo == 1) ftTimerM = (ftTimerM + 59) % 60;
                else ftTimerS = (ftTimerS + 59) % 60;
                ftTimerConcluido = false;
            }
            if (novo & ORBIS_PAD_BUTTON_CROSS) {
                if (ftTimerRodando) {
                    ftTimerRestante = ftTimerFim - ftSegundos();
                    ftTimerRodando = false;
                }
                else {
                    double total = ftTimerRestante > 0.0 ? ftTimerRestante : ftTimerH * 3600.0 + ftTimerM * 60.0 + ftTimerS;
                    if (total <= 0.0) total = 60.0;
                    ftTimerRestante = total;
                    ftTimerFim = ftSegundos() + total;
                    ftTimerRodando = true;
                    ftTimerConcluido = false;
                }
            }
            if (novo & ORBIS_PAD_BUTTON_SQUARE) {
                ftTimerRodando = false;
                ftTimerRestante = 0.0;
                ftTimerConcluido = false;
            }
        }
        else {
            if (novo & ORBIS_PAD_BUTTON_UP) { ftAlarmeSel = (ftAlarmeSel + ftAlarmeTotal - 1) % (ftAlarmeTotal > 0 ? ftAlarmeTotal : 1); }
            if (novo & ORBIS_PAD_BUTTON_DOWN) { ftAlarmeSel = (ftAlarmeSel + 1) % (ftAlarmeTotal > 0 ? ftAlarmeTotal : 1); }
            if ((novo & ORBIS_PAD_BUTTON_CROSS) && ftAlarmeTotal > 0) {
                ftAlarmes[ftAlarmeSel].ligado = !ftAlarmes[ftAlarmeSel].ligado;
                ftAlarmesSalvar();
            }
            if (novo & ORBIS_PAD_BUTTON_SQUARE) {
                if (ftAlarmeTotal < FT_MAX_ALARMES) {
                    ftAlarmes[ftAlarmeTotal].hora = ftTimerH;
                    ftAlarmes[ftAlarmeTotal].minuto = ftTimerM;
                    ftAlarmes[ftAlarmeTotal].ligado = true;
                    ftAlarmeTotal++;
                    ftAlarmeSel = ftAlarmeTotal - 1;
                    ftAlarmesSalvar();
                    ftAviso("Alarme criado com a hora do timer.");
                }
            }
            if ((novo & ORBIS_PAD_BUTTON_TRIANGLE) && ftAlarmeTotal > 0) {
                for (int i = ftAlarmeSel; i < ftAlarmeTotal - 1; i++) ftAlarmes[i] = ftAlarmes[i + 1];
                ftAlarmeTotal--;
                if (ftAlarmeSel >= ftAlarmeTotal) ftAlarmeSel = ftAlarmeTotal - 1;
                if (ftAlarmeSel < 0) ftAlarmeSel = 0;
                ftAlarmesSalvar();
            }
        }
        return false;
    }

    if (ftTela == FT_TELA_GRAVADOR) {
        if (novo & ORBIS_PAD_BUTTON_CROSS) {
            if (ftGravRodando) ftGravParar = true;
            else ftGravIniciar();
        }
        if (novo & ORBIS_PAD_BUTTON_SQUARE) {
            if (!ftGravRodando) {
                ftGravContar();
                if (ftGravTotal <= 0) ftAviso("Nenhuma gravacao para apagar.");
                else ftAviso("Para apagar, use o Explorar de arquivos.");
            }
        }
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) ftTela = FT_TELA_MENU;
        return false;
    }

    if (ftTela == FT_TELA_AFINADOR) {
        if (novo & ORBIS_PAD_BUTTON_CROSS) {
            if (ftAfinRodando) ftAfinPararThread();
            else ftAfinIniciar();
        }
        if (novo & ORBIS_PAD_BUTTON_SQUARE) {
            ftAfinModo = (ftAfinModo + 1) % 2;
            ftAfinMostrar = 0.0f;
        }
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
            ftAfinPararThread();
            ftTela = FT_TELA_MENU;
        }
        return false;
    }

    if (ftTela == FT_TELA_CONTATOS) {
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
            if (ftContDetalhe) ftContDetalhe = false;
            else ftTela = FT_TELA_MENU;
            return false;
        }
        if (novo & ORBIS_PAD_BUTTON_CROSS) {
            if (ftContTotal > 0) ftContDetalhe = !ftContDetalhe;
            return false;
        }
        if (novo & ORBIS_PAD_BUTTON_UP) ftContSel = (ftContSel + ftContTotal - 1) % (ftContTotal > 0 ? ftContTotal : 1);
        if (novo & ORBIS_PAD_BUTTON_DOWN) ftContSel = (ftContSel + 1) % (ftContTotal > 0 ? ftContTotal : 1);
        if (novo & ORBIS_PAD_BUTTON_SQUARE) ftImeAbrir(FT_IME_CONTATO_NOME, "Nome do contato", "", 60);
        if ((novo & ORBIS_PAD_BUTTON_TRIANGLE) && ftContTotal > 0) {
            for (int i = ftContSel; i < ftContTotal - 1; i++) ftContatos[i] = ftContatos[i + 1];
            ftContTotal--;
            if (ftContSel >= ftContTotal) ftContSel = ftContTotal - 1;
            if (ftContSel < 0) ftContSel = 0;
            ftContatosSalvar();
        }
        return false;
    }

    if (ftTela == FT_TELA_CAIXA) {
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) {
            if (ftMsgDetalhe) ftMsgDetalhe = false;
            else ftTela = FT_TELA_MENU;
            return false;
        }
        if (novo & ORBIS_PAD_BUTTON_CROSS) {
            if (ftMsgTotal > 0) ftMsgDetalhe = !ftMsgDetalhe;
            return false;
        }
        if (novo & ORBIS_PAD_BUTTON_UP) ftMsgSel = (ftMsgSel + ftMsgTotal - 1) % (ftMsgTotal > 0 ? ftMsgTotal : 1);
        if (novo & ORBIS_PAD_BUTTON_DOWN) ftMsgSel = (ftMsgSel + 1) % (ftMsgTotal > 0 ? ftMsgTotal : 1);
        if (novo & ORBIS_PAD_BUTTON_SQUARE) ftImeAbrir(FT_IME_MSG_DE, "De quem e a mensagem", "", 60);
        if ((novo & ORBIS_PAD_BUTTON_TRIANGLE) && ftMsgTotal > 0) {
            for (int i = ftMsgSel; i < ftMsgTotal - 1; i++) ftMsgs[i] = ftMsgs[i + 1];
            ftMsgTotal--;
            if (ftMsgSel >= ftMsgTotal) ftMsgSel = ftMsgTotal - 1;
            if (ftMsgSel < 0) ftMsgSel = 0;
            ftMsgsSalvar();
        }
        return false;
    }

    if (ftTela == FT_TELA_TERMINAL) {
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) { ftTela = FT_TELA_MENU; return false; }
        if (novo & ORBIS_PAD_BUTTON_CROSS) ftImeAbrir(FT_IME_TERMINAL, "Comando do Hyper Neiva", "", 200);
        if (novo & ORBIS_PAD_BUTTON_UP) { ftTermScroll += 3; if (ftTermScroll > ftTermTotal) ftTermScroll = ftTermTotal; }
        if (novo & ORBIS_PAD_BUTTON_DOWN) { ftTermScroll -= 3; if (ftTermScroll < 0) ftTermScroll = 0; }
        return false;
    }

    if (ftTela == FT_TELA_QR) {
        if (novo & ORBIS_PAD_BUTTON_CIRCLE) { ftTela = FT_TELA_MENU; return false; }
        if (novo & ORBIS_PAD_BUTTON_L1) ftQrAba = (ftQrAba + 3) % 4;
        if (novo & ORBIS_PAD_BUTTON_R1) ftQrAba = (ftQrAba + 1) % 4;
        if (ftQrAba == 0 || ftQrAba == 2) {
            if (novo & ORBIS_PAD_BUTTON_CROSS) ftImeAbrir(FT_IME_QR_TEXTO, ftQrAba == 2 ? "Numeros do codigo de barras" : "Texto ou link do QR", ftQrTexto, 105);
        }
        else if (ftQrAba == 1) {
            if (novo & ORBIS_PAD_BUTTON_LEFT) ftQrCampo = (ftQrCampo + 1) % 2;
            if (novo & ORBIS_PAD_BUTTON_RIGHT) ftQrCampo = (ftQrCampo + 1) % 2;
            if (novo & ORBIS_PAD_BUTTON_CROSS) {
                if (ftQrCampo == 0) ftImeAbrir(FT_IME_QR_FONE, "Telefone do WhatsApp", ftQrFone, 30);
                else ftImeAbrir(FT_IME_QR_MSG, "Mensagem do WhatsApp", ftQrMsg, 140);
            }
        }
        return false;
    }

    return false;
}

// ---------------------------------------------------------------- render
static void ftRender(uint32_t* p) {
    if (!p) return;
    ftTelaPixels = p;
    ftRelogioAtualizar();
    switch (ftTela) {
        case FT_TELA_MENU: ftDesenharMenu(p); break;
        case FT_TELA_CALC: ftDesenharCalc(p); break;
        case FT_TELA_RELOGIO: ftDesenharRelogio(p); break;
        case FT_TELA_GRAVADOR: ftDesenharGravador(p); break;
        case FT_TELA_AFINADOR: ftDesenharAfinador(p); break;
        case FT_TELA_CONTATOS: ftDesenharContatos(p); break;
        case FT_TELA_CAIXA: ftDesenharCaixa(p); break;
        case FT_TELA_TERMINAL: ftDesenharTerminal(p); break;
        case FT_TELA_QR: ftDesenharQr(p); break;
        default: ftDesenharMenu(p); break;
    }
}

static void ftAbrir(void) {
    ftTela = FT_TELA_MENU;
    ftSel = 0;
    ftOff = 0;
    ftImeAberto = false;
    ftImeModo = FT_IME_NENHUM;
    ftMkdirs(FT_NOTAS);
    ftMkdirs(FT_GRAVACOES);
    ftMkdirs(FT_BAIXADO);
    menuAtual = MENU_FERRAMENTAS;
    ftLog("FERRAMENTAS ABERTAS");
}

static void ftAoVoltar(void) {
    ftAfinPararThread();
    ftTela = FT_TELA_MENU;
}
