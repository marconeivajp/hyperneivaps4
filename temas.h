// Temas da interface. Desenho próprio, inspirado só na disposição de cada sistema: sem logos nem arte de terceiros.
// Incluído uma vez, em menu_grafico_layout.cpp, e também pelo teste no PC.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TEMA_PADRAO, TEMA_XMB, TEMA_PS4, TEMA_PS5, TEMA_OPL, TEMA_X360, TEMA_NETFLIX, TEMA_TOTAL };
enum { CENA_INICIO, CENA_JOGOS, CENA_LISTA, CENA_BIBLIOTECA };
#define TEMA_BIBLIOTECA TEMA_TOTAL

struct TemaCena {
    char (*nomes)[256];
    bool* marcados;
    int total, sel, tipo;
    const char* titulo;
    unsigned int quadro;
    const char* subtitulo;
    const char* const* opcoes;
    int totalOpcoes, selOpcao, aba;
    const char* tituloPainel;
    unsigned char* previa;
    int previaW, previaH;
    int temaJogos;
    const char* sistema;
};

// Vêm de quem inclui este arquivo.
void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);
int medirLarguraTexto(const char* texto, int tam);
void desenharRedimensionado(uint32_t* pixels, unsigned char* img, int imgW, int imgH, int dW, int dH, int posX, int posY);
void temaIcone(uint32_t* p, const char* nome, int tipo, int tam, int x, int y);
bool temaCapa(int indice, const char* nome, unsigned char** img, int* w, int* h);
bool temaFavorito(const char* nome);
// tipo 0 capa, 1 tela do jogo, 2 título, 3 logo.
bool temaImagemJogo(int indice, const char* nome, int tipo, unsigned char** img, int* w, int* h);

static inline uint32_t tmRgb(int r, int g, int b) { return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }
static inline uint32_t tmAlfa(uint32_t cor, int a) { return (cor & 0xFFFFFFu) | ((uint32_t)a << 24); }

static inline uint32_t tmMistura(uint32_t fundo, uint32_t cor) {
    uint32_t a = (cor >> 24) + 1, i = 256 - a;
    uint32_t rb = (((cor & 0xFF00FFu) * a + (fundo & 0xFF00FFu) * i) >> 8) & 0xFF00FFu;
    uint32_t g = (((cor & 0xFF00u) * a + (fundo & 0xFF00u) * i) >> 8) & 0xFF00u;
    return 0xFF000000u | rb | g;
}

static void tmCaixa(uint32_t* p, int x, int y, int w, int h, int raio, uint32_t cor) {
    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= 1080) continue;
        int recuo = 0;
        int d = j < raio ? raio - j : (j >= h - raio ? j - (h - raio) + 1 : 0);
        if (d > 0) recuo = raio - (int)sqrtf((float)(raio * raio - d * d));
        int de = x + recuo, ate = x + w - recuo;
        if (de < 0) de = 0;
        if (ate > 1920) ate = 1920;
        uint32_t* linha = p + py * 1920;
        for (int i = de; i < ate; i++) linha[i] = tmMistura(linha[i], cor);
    }
}

// Corta sem partir um caractere UTF-8.
static void tmTexto(uint32_t* p, const char* texto, int tam, int x, int y, int largura, uint32_t cor) {
    char t[260];
    snprintf(t, sizeof(t), "%s", texto);
    if (largura > 0 && medirLarguraTexto(t, tam) > largura) {
        int n = (int)strlen(t);
        while (n > 1 && medirLarguraTexto(t, tam) > largura - tam) {
            n -= 4;
            if (n < 1) n = 1;
            while (n > 1 && ((unsigned char)t[n] & 0xC0) == 0x80) n--;
            t[n] = '\0';
        }
        strcat(t, "...");
    }
    desenharTexto(p, t, tam, x, y, cor);
}

static void tmCentro(uint32_t* p, const char* texto, int tam, int centro, int y, int largura, uint32_t cor) {
    int w = medirLarguraTexto(texto, tam);
    if (w > largura) w = largura;
    tmTexto(p, texto, tam, centro - w / 2, y, largura, cor);
}

static void tmNomeLimpo(const char* nome, char* saida, size_t tamanho) {
    snprintf(saida, tamanho, "%s", nome);
    size_t n = strlen(saida);
    if (n > 2 && saida[0] == '[' && saida[n - 1] == ']') {
        memmove(saida, saida + 1, n - 2);
        saida[n - 2] = '\0';
    }
}

static uint32_t* tmFundoPronto = NULL;
static int tmFundoTema = -1;

static inline float tmLimite(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static uint32_t tmDegrade(const int* a, const int* b, float t) {
    return tmRgb((int)(a[0] + (b[0] - a[0]) * t), (int)(a[1] + (b[1] - a[1]) * t), (int)(a[2] + (b[2] - a[2]) * t));
}

// O fundo é calculado uma vez: refazer dois milhões de pixels por quadro pesaria no console.
static void tmPrepararFundo(int tema) {
    if (tmFundoTema == tema && tmFundoPronto) return;
    if (!tmFundoPronto) tmFundoPronto = (uint32_t*)malloc(1920 * 1080 * 4);
    if (!tmFundoPronto) return;
    static const int cores[TEMA_TOTAL + 1][2][3] = {
        { {0, 0, 0}, {0, 0, 0} },
        { {12, 42, 92}, {66, 22, 96} },
        { {6, 34, 92}, {10, 92, 158} },
        { {6, 7, 12}, {28, 34, 48} },
        { {14, 18, 28}, {24, 30, 44} },
        { {10, 42, 10}, {118, 190, 40} },
        { {0, 0, 0}, {22, 22, 22} },
        { {7, 8, 20}, {30, 22, 66} },
    };
    for (int y = 0; y < 1080; y++) {
        for (int x = 0; x < 1920; x++) {
            float t;
            if (tema == TEMA_XMB) t = tmLimite((x * 0.35f + y) / 1500.0f);
            else if (tema == TEMA_PS4) t = tmLimite(1.0f - (y * 0.9f + (1920 - x) * 0.25f) / 1400.0f);
            else if (tema == TEMA_X360) {
                float dx = (x - 960) / 1100.0f, dy = (y - 470) / 720.0f;
                t = tmLimite(1.0f - sqrtf(dx * dx + dy * dy));
            }
            else t = tmLimite((float)y / 1080.0f);
            tmFundoPronto[y * 1920 + x] = tmDegrade(cores[tema][0], cores[tema][1], t);
        }
    }
    tmFundoTema = tema;
}

static void tmFaixaOnda(uint32_t* p, float fase, int centro, int altura, int forca) {
    uint32_t tabela[160];
    for (int d = -altura; d <= altura; d++) {
        float q = 1.0f - fabsf((float)d) / altura;
        tabela[d + altura] = tmAlfa(0xFFFFFF, (int)(q * q * forca));
    }
    for (int x = 0; x < 1920; x++) {
        int meio = centro + (int)(sinf(x * 0.0041f + fase) * 74.0f + sinf(x * 0.0017f - fase * 0.6f) * 42.0f);
        for (int d = -altura; d <= altura; d++) {
            int y = meio + d;
            if (y < 0 || y >= 1080) continue;
            p[y * 1920 + x] = tmMistura(p[y * 1920 + x], tabela[d + altura]);
        }
    }
}

static float tmSuave = 0.0f;
static bool tmCapaPendente = false;
static bool tmRedesenhar = false;
static int tmUltimoTipo = -1, tmUltimoTotal = -1;

// Devolve true enquanto a seleção ainda desliza.
static bool tmAndar(const TemaCena* c) {
    if (c->tipo != tmUltimoTipo || c->total != tmUltimoTotal) {
        tmSuave = (float)c->sel;
        tmUltimoTipo = c->tipo;
        tmUltimoTotal = c->total;
    }
    if (tmSuave == (float)c->sel) return false;
    tmSuave += (c->sel - tmSuave) * 0.3f;
    if (fabsf(c->sel - tmSuave) < 0.02f) tmSuave = (float)c->sel;
    return true;
}

// Lista vertical com o item escolhido sempre no mesmo lugar, usada pelos temas nas telas sem cartões.
static void tmListaFixa(uint32_t* p, const TemaCena* c, int x, int yFoco, int passo, int largura, int tam, int icone, uint32_t corFoco, uint32_t corFundo, int raio) {
    int visiveis = 1080 / passo / 2 + 2;
    int base = c->sel - visiveis;
    if (base < 0) base = 0;
    for (int i = base; i < c->total && i <= c->sel + visiveis; i++) {
        float dist = i - tmSuave;
        int y = yFoco + (int)(dist * passo);
        if (y < -passo || y > 1080) continue;
        bool foco = i == c->sel;
        float d = fabsf(dist);
        int luz = 235 - (int)(d * 46.0f);
        if (luz < 110) luz = 110;
        int topo = y + tam / 2 - (passo - 2) / 2;
        if (foco && corFundo) tmCaixa(p, x - 18, topo, largura + 36, passo - 2, raio, corFundo);
        if (c->marcados && c->marcados[i]) tmCaixa(p, x - 18, topo, 8, passo - 2, 0, tmRgb(255, 210, 60));
        char nome[256];
        tmNomeLimpo(c->nomes[i], nome, sizeof(nome));
        if (icone > 0) temaIcone(p, c->nomes[i], c->tipo, icone, x, y + (tam - icone) / 2);
        tmTexto(p, nome, tam, x + (icone > 0 ? icone + 22 : 0), y, largura - icone - 30, foco ? corFoco : tmRgb(luz, luz, luz));
    }
}

static void tmCartao(uint32_t* p, const TemaCena* c, int i, int x, int y, int tam, uint32_t corFundo, int raio) {
    unsigned char* img = NULL;
    int w = 0, h = 0;
    if (c->tipo == CENA_JOGOS && temaCapa(i, c->nomes[i], &img, &w, &h) && img) {
        desenharRedimensionado(p, img, w, h, tam, tam, x, y);
        return;
    }
    tmCaixa(p, x, y, tam, tam, raio, corFundo);
    int icone = tam * 56 / 100;
    temaIcone(p, c->nomes[i], c->tipo, icone, x + (tam - icone) / 2, y + (tam - icone) / 2);
    if (c->tipo == CENA_JOGOS) {
        char nome[256];
        tmNomeLimpo(c->nomes[i], nome, sizeof(nome));
        tmCentro(p, nome, tam / 11 + 8, x + tam / 2, y + tam - tam / 5, tam - 20, tmRgb(235, 235, 235));
    }
}

static void tmRodape(uint32_t* p, const char* texto, int y, uint32_t cor) {
    int w = medirLarguraTexto(texto, 24);
    desenharTexto(p, texto, 24, 1860 - w, y, cor);
}

static void tmDesenharXmb(uint32_t* p, const TemaCena* c) {
    float fase = c->quadro * 0.012f;
    tmFaixaOnda(p, fase, 640, 62, 34);
    tmFaixaOnda(p, fase * 0.7f + 2.1f, 700, 40, 26);
    if (c->tipo == CENA_INICIO) {
        int foco = 470;
        for (int i = 0; i < c->total; i++) {
            float dist = i - tmSuave;
            int tam = i == c->sel ? 132 : 76;
            int x = foco + (int)(dist * 226.0f) - tam / 2 + (dist > 0 ? 60 : dist < 0 ? -60 : 0);
            if (x < -tam || x > 1920) continue;
            temaIcone(p, c->nomes[i], c->tipo, tam, x, 300 - tam / 2);
            if (i == c->sel) tmCentro(p, c->nomes[i], 34, x + tam / 2, 382, 360, tmRgb(255, 255, 255));
        }
        return;
    }
    temaIcone(p, c->titulo, CENA_INICIO, 92, 230, 126);
    tmTexto(p, c->titulo, 30, 212, 232, 300, tmRgb(235, 235, 235));
    tmListaFixa(p, c, 520, 430, 84, 1180, 36, 52, tmRgb(255, 255, 255), 0, 0);
}

static void tmFilaCartoes(uint32_t* p, const TemaCena* c, int foco, int y, int menor, int maior, int folga, uint32_t corFundo, uint32_t corFoco, int raio) {
    for (int i = 0; i < c->total; i++) {
        float dist = i - tmSuave;
        if (dist < -0.95f || dist > 9.0f) continue;
        int tam = i == c->sel ? maior : menor;
        int x = foco + (int)(dist * (menor + folga));
        if (dist > 0) x += (int)(tmLimite(dist) * (maior - menor));
        if (i == c->sel) tmCaixa(p, x - 7, y - 7, tam + 14, tam + 14, raio + 4, corFoco);
        tmCartao(p, c, i, x, y, tam, corFundo, raio);
    }
}

static void tmDesenharPs4(uint32_t* p, const TemaCena* c) {
    tmTexto(p, c->tipo == CENA_INICIO ? "HYPER NEIVA" : c->titulo, 30, 92, 62, 900, tmRgb(235, 242, 255));
    if (c->tipo == CENA_LISTA) {
        tmCaixa(p, 76, 120, 1768, 2, 0, tmAlfa(0xFFFFFF, 90));
        tmListaFixa(p, c, 150, 470, 92, 1620, 36, 52, tmRgb(255, 255, 255), tmAlfa(0xFFFFFF, 44), 8);
        return;
    }
    tmFilaCartoes(p, c, 150, 310, 232, 322, 26, tmAlfa(0x0A2A5C, 210), tmRgb(255, 255, 255), 6);
    char nome[256];
    if (c->total > 0) {
        tmNomeLimpo(c->nomes[c->sel], nome, sizeof(nome));
        tmTexto(p, nome, 44, 150, 668, 1600, tmRgb(255, 255, 255));
    }
    tmRodape(p, "X  Abrir     O  Voltar", 1010, tmRgb(205, 222, 245));
}

static void tmDesenharPs5(uint32_t* p, const TemaCena* c) {
    char nome[256] = "";
    if (c->total > 0) tmNomeLimpo(c->nomes[c->sel], nome, sizeof(nome));
    if (c->tipo == CENA_LISTA) {
        tmTexto(p, c->titulo, 62, 110, 80, 1500, tmRgb(255, 255, 255));
        tmListaFixa(p, c, 130, 480, 86, 1240, 34, 46, tmRgb(10, 12, 18), tmRgb(238, 240, 245), 34);
        return;
    }
    tmFilaCartoes(p, c, 110, 96, 112, 164, 18, tmAlfa(0x262C3A, 235), tmRgb(250, 250, 250), 14);
    unsigned char* img = NULL;
    int w = 0, h = 0;
    if (c->tipo == CENA_JOGOS && c->total > 0 && temaCapa(c->sel, c->nomes[c->sel], &img, &w, &h) && img) desenharRedimensionado(p, img, w, h, 520, 520, 1290, 390);
    else if (c->total > 0 && tmSuave == (float)c->sel) temaIcone(p, c->nomes[c->sel], c->tipo, 300, 1400, 480);
    tmTexto(p, nome, 76, 110, 560, 1140, tmRgb(255, 255, 255));
    tmCaixa(p, 110, 700, 250, 76, 38, tmRgb(245, 246, 250));
    tmCentro(p, "Abrir", 34, 235, 718, 220, tmRgb(12, 14, 20));
}

static void tmDesenharOpl(uint32_t* p, const TemaCena* c) {
    tmCaixa(p, 0, 0, 1920, 92, 0, tmAlfa(0x000000, 130));
    tmTexto(p, c->tipo == CENA_INICIO ? "Hyper Neiva" : c->titulo, 36, 60, 26, 1400, tmRgb(255, 255, 255));
    tmCaixa(p, 60, 130, 1020, 830, 10, tmAlfa(0x000000, 110));
    int passo = 62, linhas = 13, inicio = c->sel - linhas / 2;
    if (inicio > c->total - linhas) inicio = c->total - linhas;
    if (inicio < 0) inicio = 0;
    for (int n = 0; n < linhas && inicio + n < c->total; n++) {
        int i = inicio + n, y = 146 + n * passo;
        if (i == c->sel) tmCaixa(p, 72, y - 4, 996, passo - 4, 6, tmRgb(38, 92, 170));
        if (c->marcados && c->marcados[i]) tmCaixa(p, 72, y - 4, 8, passo - 4, 0, tmRgb(255, 210, 60));
        char nome[256];
        tmNomeLimpo(c->nomes[i], nome, sizeof(nome));
        tmTexto(p, nome, 32, 96, y + 6, 950, i == c->sel ? tmRgb(255, 255, 255) : tmRgb(186, 194, 208));
    }
    tmCaixa(p, 1120, 130, 740, 830, 10, tmAlfa(0x000000, 110));
    if (c->total > 0) {
        unsigned char* img = NULL;
        int w = 0, h = 0;
        if (c->tipo == CENA_JOGOS && temaCapa(c->sel, c->nomes[c->sel], &img, &w, &h) && img) desenharRedimensionado(p, img, w, h, 520, 620, 1230, 170);
        else temaIcone(p, c->nomes[c->sel], c->tipo, 280, 1350, 300);
        char nome[256];
        tmNomeLimpo(c->nomes[c->sel], nome, sizeof(nome));
        tmCentro(p, nome, 34, 1490, 840, 680, tmRgb(255, 255, 255));
    }
    tmCaixa(p, 0, 988, 1920, 92, 0, tmAlfa(0x000000, 130));
    tmTexto(p, "X  Abrir      O  Voltar      Triângulo  Opções", 28, 60, 1016, 1400, tmRgb(220, 226, 236));
}

static void tmDesenharX360(uint32_t* p, const TemaCena* c) {
    if (c->tipo == CENA_LISTA) {
        tmCaixa(p, 0, 0, 1920, 120, 0, tmAlfa(0x000000, 120));
        tmTexto(p, c->titulo, 42, 110, 36, 1500, tmRgb(255, 255, 255));
        tmCaixa(p, 90, 170, 1740, 800, 16, tmAlfa(0xFFFFFF, 225));
        int passo = 70, linhas = 11, inicio = c->sel - linhas / 2;
        if (inicio > c->total - linhas) inicio = c->total - linhas;
        if (inicio < 0) inicio = 0;
        for (int n = 0; n < linhas && inicio + n < c->total; n++) {
            int i = inicio + n, y = 190 + n * passo;
            if (i == c->sel) tmCaixa(p, 110, y - 4, 1700, passo - 6, 10, tmRgb(92, 170, 24));
            if (c->marcados && c->marcados[i]) tmCaixa(p, 110, y - 4, 8, passo - 6, 0, tmRgb(255, 170, 0));
            char nome[256];
            tmNomeLimpo(c->nomes[i], nome, sizeof(nome));
            temaIcone(p, c->nomes[i], c->tipo, 44, 132, y + 6);
            tmTexto(p, nome, 32, 196, y + 10, 1580, i == c->sel ? tmRgb(255, 255, 255) : tmRgb(44, 52, 44));
        }
        return;
    }
    int x = 150;
    int inicio = c->sel - 2;
    if (inicio < 0) inicio = 0;
    for (int i = inicio; i < c->total && x < 1800; i++) {
        char nome[256];
        tmNomeLimpo(c->nomes[i], nome, sizeof(nome));
        bool foco = i == c->sel;
        int w = medirLarguraTexto(nome, foco ? 40 : 30);
        if (w > 330) w = 330;
        tmTexto(p, nome, foco ? 40 : 30, x, foco ? 92 : 100, 330, foco ? tmRgb(255, 255, 255) : tmAlfa(0xE6F5D0, 255));
        if (foco) tmCaixa(p, x, 148, w, 5, 2, tmRgb(255, 255, 255));
        x += w + 70;
    }
    for (int lado = -2; lado <= 2; lado++) {
        if (lado == 0) continue;
        int i = c->sel + lado;
        if (i < 0 || i >= c->total) continue;
        int w = lado == -2 || lado == 2 ? 110 : 190;
        int px = lado == -2 ? 80 : lado == -1 ? 210 : lado == 1 ? 1520 : 1730;
        tmCaixa(p, px, 280, w, 560, 8, tmAlfa(0xFFFFFF, lado == -2 || lado == 2 ? 60 : 110));
        int icone = w * 6 / 10;
        temaIcone(p, c->nomes[i], c->tipo, icone, px + (w - icone) / 2, 520);
    }
    tmCaixa(p, 410, 230, 1100, 660, 12, tmAlfa(0xFFFFFF, 236));
    if (c->total > 0) {
        unsigned char* img = NULL;
        int w = 0, h = 0;
        char nome[256];
        tmNomeLimpo(c->nomes[c->sel], nome, sizeof(nome));
        if (c->tipo == CENA_JOGOS && temaCapa(c->sel, c->nomes[c->sel], &img, &w, &h) && img) desenharRedimensionado(p, img, w, h, 440, 500, 740, 256);
        else temaIcone(p, c->nomes[c->sel], c->tipo, 320, 800, 300);
        tmCentro(p, nome, 44, 960, 790, 1040, tmRgb(36, 62, 24));
    }
}

static void tmDesenharNetflix(uint32_t* p, const TemaCena* c) {
    uint32_t vermelho = tmRgb(229, 9, 20), branco = tmRgb(255, 255, 255);
    char nome[256] = "";
    if (c->total > 0) tmNomeLimpo(c->nomes[c->sel], nome, sizeof(nome));
    if (c->tipo == CENA_LISTA) {
        tmTexto(p, c->titulo, 56, 110, 70, 1500, branco);
        tmCaixa(p, 110, 146, 90, 6, 0, vermelho);
        tmListaFixa(p, c, 130, 470, 86, 1560, 36, 50, branco, tmAlfa(0xE50914, 225), 4);
        return;
    }
    tmTexto(p, "HYPER NEIVA", 46, 110, 48, 700, vermelho);
    unsigned char* img = NULL;
    int w = 0, h = 0;
    bool parado = tmSuave == (float)c->sel;
    if (c->tipo == CENA_JOGOS && c->total > 0 && temaCapa(c->sel, c->nomes[c->sel], &img, &w, &h) && img) desenharRedimensionado(p, img, w, h, 380, 500, 1400, 80);
    else if (c->total > 0 && parado) temaIcone(p, c->nomes[c->sel], c->tipo, 300, 1440, 150);
    tmTexto(p, nome, 88, 110, 210, 1220, branco);
    tmCaixa(p, 110, 350, 220, 72, 6, branco);
    tmCentro(p, "X  Abrir", 32, 220, 368, 200, tmRgb(12, 12, 12));
    tmCaixa(p, 350, 350, 230, 72, 6, tmAlfa(0x6D6D6E, 200));
    tmCentro(p, "O  Voltar", 32, 465, 368, 210, branco);
    tmTexto(p, c->tipo == CENA_JOGOS ? c->titulo : "Menu principal", 34, 110, 610, 900, tmRgb(229, 229, 229));

    bool jogos = c->tipo == CENA_JOGOS;
    int cartaoW = jogos ? 200 : 300, cartaoH = jogos ? 270 : 170, passo = cartaoW + 20;
    for (int i = 0; i < c->total; i++) {
        float dist = i - tmSuave;
        if (dist < -0.95f || dist > 8.0f) continue;
        int x = 110 + (int)(dist * passo), y = 670;
        if (x > 1920) continue;
        if (i == c->sel) tmCaixa(p, x - 6, y - 6, cartaoW + 12, cartaoH + 12, 8, branco);
        unsigned char* capa = NULL;
        int cw = 0, ch = 0;
        if (jogos && temaCapa(i, c->nomes[i], &capa, &cw, &ch) && capa) {
            desenharRedimensionado(p, capa, cw, ch, cartaoW, cartaoH, x, y);
            continue;
        }
        tmCaixa(p, x, y, cartaoW, cartaoH, 5, tmRgb(38, 38, 38));
        temaIcone(p, c->nomes[i], c->tipo, 92, x + (cartaoW - 92) / 2, y + 22);
        char rotulo[256];
        tmNomeLimpo(c->nomes[i], rotulo, sizeof(rotulo));
        tmCentro(p, rotulo, 26, x + cartaoW / 2, y + cartaoH - 42, cartaoW - 20, tmRgb(235, 235, 235));
    }
}

static void tmSemExtensao(const char* nome, char* saida, size_t tamanho) {
    snprintf(saida, tamanho, "%s", nome);
    char* ponto = strrchr(saida, '.');
    if (ponto) *ponto = '\0';
}

static void tmCapaBiblioteca(uint32_t* p, const TemaCena* c, int i, int centro, int y, int tam, bool foco) {
    uint32_t destaque = tmRgb(122, 104, 255);
    unsigned char* img = NULL;
    int w = 0, h = 0;
    int largura = tam, altura = tam;
    tmCapaPendente = false;
    bool temCapa = temaCapa(i, c->nomes[i], &img, &w, &h) && img && w > 0 && h > 0;
    bool esperando = tmCapaPendente;
    if (foco && c->previa) { img = c->previa; w = c->previaW; h = c->previaH; temCapa = true; }
    if (temCapa) {
        if (w > h) altura = tam * h / w;
        else largura = tam * w / h;
    }
    int x = centro - largura / 2, topo = y + (tam - altura) / 2;
    if (foco) {
        tmCaixa(p, x - 16, topo - 16, largura + 32, altura + 32, 14, tmAlfa(destaque, 70));
        tmCaixa(p, x - 6, topo - 6, largura + 12, altura + 12, 6, destaque);
    }
    if (temCapa) desenharRedimensionado(p, img, w, h, largura, altura, x, topo);
    else {
        char nome[256];
        tmSemExtensao(c->nomes[i], nome, sizeof(nome));
        tmCaixa(p, x, topo, largura, altura, 6, tmRgb(34, 32, 60));
        // Enquanto a capa não abre, o cartão fica liso: nome e depois capa parecia piscar.
        if (!esperando) tmCentro(p, nome, tam / 16 + 6, centro, topo + altura / 2 - 14, largura - 30, tmRgb(205, 205, 228));
    }
    if (temaFavorito(c->nomes[i])) temaIcone(p, "EXTRA", CENA_INICIO, tam / 8, x + largura - tam / 8 - 8, topo + 8);
}

static void tmAba(uint32_t* p, const char* texto, int x, bool foco) {
    int w = medirLarguraTexto(texto, 28) + 56;
    if (foco) tmCaixa(p, x, 44, w, 56, 12, tmRgb(70, 58, 170));
    desenharTexto(p, texto, 28, x + 28, 56, foco ? tmRgb(255, 255, 255) : tmRgb(170, 170, 196));
}

// Desenha a imagem inteira dentro do retângulo, sem esticar.
static bool tmImagemCaixa(uint32_t* p, unsigned char* img, int w, int h, int x, int y, int largura, int altura) {
    if (!img || w <= 0 || h <= 0) return false;
    int dw = largura, dh = largura * h / w;
    if (dh > altura) { dh = altura; dw = altura * w / h; }
    desenharRedimensionado(p, img, w, h, dw, dh, x + (largura - dw) / 2, y + (altura - dh) / 2);
    return true;
}

static bool tmImagemDoJogo(uint32_t* p, const TemaCena* c, int i, int tipo, int x, int y, int largura, int altura) {
    unsigned char* img = NULL;
    int w = 0, h = 0;
    return temaImagemJogo(i, c->nomes[i], tipo, &img, &w, &h) && tmImagemCaixa(p, img, w, h, x, y, largura, altura);
}

// Cartão da grade: capa inteira no centro da célula, nome só quando não há capa.
static void tmCelulaJogo(uint32_t* p, const TemaCena* c, int i, int x, int y, int largura, int altura, bool foco) {
    uint32_t destaque = tmRgb(255, 255, 255);
    if (foco) tmCaixa(p, x - 8, y - 8, largura + 16, altura + 16, 10, destaque);
    tmCaixa(p, x, y, largura, altura, 6, tmRgb(28, 30, 44));
    tmCapaPendente = false;
    unsigned char* img = NULL;
    int w = 0, h = 0;
    bool capa = temaCapa(i, c->nomes[i], &img, &w, &h) && img;
    bool esperando = tmCapaPendente;
    if (capa) tmImagemCaixa(p, img, w, h, x, y, largura, altura);
    else if (!esperando) {
        char nome[256];
        tmSemExtensao(c->nomes[i], nome, sizeof(nome));
        tmCentro(p, nome, 24, x + largura / 2, y + altura / 2 - 12, largura - 20, tmRgb(205, 205, 228));
    }
    if (temaFavorito(c->nomes[i])) temaIcone(p, "EXTRA", CENA_INICIO, 36, x + largura - 44, y + 8);
}

static float tmLinhaSuave = 0.0f;

// Grade com rolagem suave por fileira, para a seleção nunca sair da tela.
static void tmGradeJogos(uint32_t* p, const TemaCena* c, int x0, int y0, int largura, int colunas, int baseTela) {
    int espaco = 26;
    int largCelula = (largura - (colunas - 1) * espaco) / colunas;
    int altCelula = largCelula * 4 / 3;
    int passo = altCelula + espaco;
    float alvo = (float)(c->sel / colunas);
    tmLinhaSuave += (alvo - tmLinhaSuave) * 0.3f;
    if (fabsf(alvo - tmLinhaSuave) < 0.02f) tmLinhaSuave = alvo;
    else tmRedesenhar = true;
    int visiveis = (baseTela - y0) / passo;
    float topo = tmLinhaSuave - (visiveis > 2 ? 1.0f : 0.0f);
    if (topo < 0) topo = 0;
    int primeira = (int)topo;
    for (int linha = primeira; linha <= primeira + visiveis + 1; linha++) {
        int y = y0 + (int)((linha - topo) * passo);
        if (y > baseTela) break;
        for (int col = 0; col < colunas; col++) {
            int i = linha * colunas + col;
            if (i >= c->total) break;
            tmCelulaJogo(p, c, i, x0 + col * (largCelula + espaco), y, largCelula, altCelula, i == c->sel);
        }
    }
}

// Repinta o fundo nas faixas de cima e de baixo, por cima das células que rolaram para fora.
static void tmRefazerFaixa(uint32_t* p, int de, int ate) {
    if (!tmFundoPronto) return;
    if (de < 0) de = 0;
    if (ate > 1080) ate = 1080;
    if (ate > de) memcpy(p + de * 1920, tmFundoPronto + de * 1920, (size_t)(ate - de) * 1920 * 4);
}

static void tmCabecalhoJogos(uint32_t* p, const TemaCena* c, int x) {
    char texto[160];
    desenharTexto(p, "HYPER NEIVA", 30, x, 34, tmRgb(255, 255, 255));
    tmTexto(p, c->sistema, 30, x + 300, 34, 700, tmRgb(164, 156, 230));
    snprintf(texto, sizeof(texto), "%s%s", c->aba ? "Favoritos  -  " : "", c->subtitulo);
    tmTexto(p, texto, 24, x, 84, 1100, tmRgb(205, 205, 228));
}

static void tmRodapeJogos(uint32_t* p, const TemaCena* c, int x, int largura) {
    char nome[256] = "";
    if (c->total > 0) tmSemExtensao(c->nomes[c->sel], nome, sizeof(nome));
    if (c->total == 1 && strncmp(nome, "Nenhuma", 7) == 0) snprintf(nome, sizeof(nome), "Nenhum jogo encontrado");
    tmCaixa(p, 0, 960, 1920, 120, 0, tmAlfa(0x000000, 170));
    tmTexto(p, nome, 36, x, 972, largura, tmRgb(255, 255, 255));
    desenharTexto(p, c->aba == 2 ? "X  Abrir     Triângulo  Opções     Quadrado  Jogos XML     O  Voltar" : "X  Jogar     Triângulo  Opções     L2  Favorito     R2  Pesquisar     Options  Ordenar     L1/R1  Favoritos", 24, x, 1030, tmRgb(205, 205, 228));
}

static void tmPainelOpcoes(uint32_t* p, const TemaCena* c) {
    uint32_t branco = tmRgb(255, 255, 255);
    tmCaixa(p, 1270, 0, 650, 1080, 0, tmAlfa(0x05060F, 252));
    tmCaixa(p, 1270, 0, 2, 1080, 0, tmRgb(122, 104, 255));
    desenharTexto(p, c->tituloPainel, 38, 1320, 90, branco);
    int linhas = 13, inicio = c->selOpcao - linhas / 2;
    if (inicio > c->totalOpcoes - linhas) inicio = c->totalOpcoes - linhas;
    if (inicio < 0) inicio = 0;
    for (int n = 0; n < linhas && inicio + n < c->totalOpcoes; n++) {
        int i = inicio + n, y = 170 + n * 62;
        if (i == c->selOpcao) tmCaixa(p, 1300, y - 12, 590, 58, 10, tmRgb(56, 48, 140));
        tmTexto(p, c->opcoes[i], 30, 1330, y, 540, i == c->selOpcao ? branco : tmRgb(205, 205, 228));
    }
    desenharTexto(p, "X  Escolher     O  Voltar", 26, 1330, 1000, tmRgb(205, 205, 228));
}

static void tmJogosGrade(uint32_t* p, const TemaCena* c) {
    tmGradeJogos(p, c, 110, 140, 1700, 6, 960);
    tmRefazerFaixa(p, 0, 128);
    tmCabecalhoJogos(p, c, 110);
    tmRodapeJogos(p, c, 110, 1700);
}

static void tmJogosDetalhes(uint32_t* p, const TemaCena* c) {
    tmGradeJogos(p, c, 760, 140, 1100, 4, 960);
    tmRefazerFaixa(p, 0, 128);
    tmCaixa(p, 0, 0, 700, 960, 0, tmAlfa(0x000000, 150));
    tmCabecalhoJogos(p, c, 60);
    if (c->total > 0) {
        char nome[256];
        tmSemExtensao(c->nomes[c->sel], nome, sizeof(nome));
        bool parado = tmSuave == (float)c->sel;
        if (!parado || !tmImagemDoJogo(p, c, c->sel, 3, 60, 150, 580, 200)) {
            tmCaixa(p, 60, 150, 580, 200, 10, tmAlfa(0xFFFFFF, 18));
            tmCentro(p, nome, 40, 350, 230, 560, tmRgb(255, 255, 255));
        }
        tmCaixa(p, 56, 386, 588, 448, 8, tmRgb(70, 58, 170));
        tmCaixa(p, 60, 390, 580, 440, 6, tmRgb(10, 10, 16));
        if (!parado || !tmImagemDoJogo(p, c, c->sel, 1, 60, 390, 580, 440))
            tmCentro(p, "Imagem do jogo", 26, 350, 596, 560, tmRgb(120, 120, 150));
        tmTexto(p, nome, 30, 60, 860, 600, tmRgb(255, 255, 255));
        tmTexto(p, c->sistema, 24, 60, 906, 600, tmRgb(164, 156, 230));
    }
    tmRodapeJogos(p, c, 60, 1800);
}

// Roda de logos à direita, como nos frontends de fliperama; à esquerda, a tela do jogo e a capa.
static void tmJogosRoda(uint32_t* p, const TemaCena* c) {
    tmCaixa(p, 1200, 0, 720, 1080, 0, tmAlfa(0x000000, 150));
    for (int d = -4; d <= 4; d++) {
        int i = c->sel + d;
        if (i < 0 || i >= c->total) continue;
        float distancia = i - tmSuave;
        int y = 540 + (int)(distancia * 150.0f);
        bool foco = i == c->sel;
        int largura = foco ? 600 : 440, altura = foco ? 170 : 110;
        int x = 1560 - largura / 2;
        if (foco) tmCaixa(p, x - 20, y - altura / 2 - 14, largura + 40, altura + 28, 18, tmAlfa(0xFFFFFF, 34));
        if (!tmImagemDoJogo(p, c, i, 3, x, y - altura / 2, largura, altura)) {
            char nome[256];
            tmSemExtensao(c->nomes[i], nome, sizeof(nome));
            tmCentro(p, nome, foco ? 40 : 28, 1560, y - (foco ? 20 : 14), largura, foco ? tmRgb(255, 255, 255) : tmRgb(170, 170, 196));
        }
    }
    desenharTexto(p, "HYPER NEIVA", 34, 70, 40, tmRgb(255, 255, 255));
    tmTexto(p, c->sistema, 34, 380, 40, 760, tmRgb(164, 156, 230));
    tmCaixa(p, 64, 124, 1032, 652, 10, tmRgb(150, 150, 180));
    tmCaixa(p, 70, 130, 1020, 640, 6, tmRgb(8, 8, 12));
    if (c->total > 0) {
        bool parado = tmSuave == (float)c->sel;
        if (!parado || !tmImagemDoJogo(p, c, c->sel, 1, 70, 130, 1020, 640))
            tmCentro(p, "Imagem do jogo", 28, 580, 430, 900, tmRgb(120, 120, 150));
        tmImagemDoJogo(p, c, c->sel, 0, 860, 520, 320, 420);
        char nome[256];
        tmSemExtensao(c->nomes[c->sel], nome, sizeof(nome));
        if (c->total == 1 && strncmp(nome, "Nenhuma", 7) == 0) snprintf(nome, sizeof(nome), "Nenhum jogo encontrado");
        tmTexto(p, nome, 40, 70, 800, 780, tmRgb(255, 255, 255));
        tmTexto(p, c->subtitulo, 24, 70, 860, 780, tmRgb(205, 205, 228));
    }
    tmCaixa(p, 0, 1000, 1200, 80, 0, tmAlfa(0x000000, 160));
    desenharTexto(p, "Cima / Baixo  Navegar     X  Jogar     Triângulo  Opções     L2  Favorito     R2  Pesquisar", 24, 70, 1026, tmRgb(205, 205, 228));
}

static void tmDesenharCarrossel(uint32_t* p, const TemaCena* c);

static void tmDesenharBiblioteca(uint32_t* p, const TemaCena* c) {
    if (c->temaJogos == 1) tmJogosGrade(p, c);
    else if (c->temaJogos == 2) tmJogosDetalhes(p, c);
    else if (c->temaJogos == 3) tmJogosRoda(p, c);
    else tmDesenharCarrossel(p, c);
    if (c->temaJogos >= 1 && c->temaJogos <= 3 && c->previa) {
        // Prévia da capa sob o seletor, grande, à esquerda do painel.
        tmCaixa(p, 300, 150, 880, 760, 14, tmAlfa(0x000000, 220));
        tmImagemCaixa(p, c->previa, c->previaW, c->previaH, 330, 180, 820, 700);
    }
    if (c->temaJogos >= 1 && c->temaJogos <= 3 && c->totalOpcoes > 0) tmPainelOpcoes(p, c);
}

static void tmDesenharCarrossel(uint32_t* p, const TemaCena* c) {
    uint32_t branco = tmRgb(255, 255, 255), suave = tmRgb(164, 156, 230);
    desenharTexto(p, "HYPER NEIVA", 34, 90, 54, branco);
    tmCaixa(p, 600, 36, 560, 72, 16, tmAlfa(0xFFFFFF, 14));
    tmAba(p, c->aba == 2 ? "Sistemas" : "Biblioteca", 630, c->aba != 1);
    tmAba(p, "Favoritos", 880, c->aba == 1);
    desenharTexto(p, "L1", 22, 556, 62, tmRgb(170, 170, 196));
    desenharTexto(p, "R1", 22, 1176, 62, tmRgb(170, 170, 196));
    desenharTexto(p, c->aba == 2 ? "Seus sistemas" : c->aba ? "Seus favoritos" : "Seus jogos", 42, 90, 138, branco);
    tmTexto(p, c->subtitulo, 26, 90, 192, 900, suave);
    char posicao[32];
    snprintf(posicao, sizeof(posicao), "%d / %d", c->total > 0 ? c->sel + 1 : 0, c->total);
    desenharTexto(p, posicao, 26, 1760, 192, tmRgb(205, 205, 228));

    for (int distancia = 5; distancia >= 0; distancia--) {
        for (int lado = -1; lado <= 1; lado += 2) {
            if (distancia == 0 && lado > 0) continue;
            int i = c->sel + distancia * lado;
            if (i < 0 || i >= c->total) continue;
            float d = i - tmSuave, ad = fabsf(d);
            float perto = tmLimite(1.0f - ad);
            int tam = 226 + (int)(perto * 254.0f);
            int centro = 960 + (int)(d * 250.0f) + (d > 0 ? 130 : d < 0 ? -130 : 0) - (int)(d > 0 ? perto * 130 : d < 0 ? -perto * 130 : 0);
            if (centro < -200 || centro > 2120) continue;
            tmCapaBiblioteca(p, c, i, centro, 240 + (480 - tam) / 2, tam, i == c->sel);
        }
    }
    if (c->total > 0) {
        char nome[256];
        tmSemExtensao(c->nomes[c->sel], nome, sizeof(nome));
        if (c->total == 1 && strncmp(nome, "Nenhuma", 7) == 0) snprintf(nome, sizeof(nome), "Nenhum jogo encontrado");
        tmCentro(p, nome, 44, 960, 760, 1300, branco);
        tmCentro(p, c->sistema, 26, 960, 820, 800, tmRgb(205, 205, 228));
        tmCaixa(p, 840, 866, 240, 68, 12, tmRgb(70, 58, 170));
        tmCentro(p, c->aba == 2 ? "X  Abrir" : "X  Jogar", 30, 960, 884, 220, branco);
    }
    tmCaixa(p, 60, 972, 1800, 2, 0, tmAlfa(0xFFFFFF, 40));
    desenharTexto(p, c->aba == 2 ? "X  Abrir      Triângulo  Opções      Quadrado  Jogos XML      O  Voltar" : "X  Jogar      Triângulo  Opções      L2  Favorito      R2  Pesquisar      Options  Ordenar      O  Voltar", 26, 90, 1000, tmRgb(205, 205, 228));

    if (c->totalOpcoes > 0) {
        tmCaixa(p, 1270, 0, 650, 1080, 0, tmAlfa(0x05060F, 252));
        tmCaixa(p, 1270, 0, 2, 1080, 0, tmRgb(122, 104, 255));
        desenharTexto(p, c->tituloPainel, 38, 1320, 90, branco);
        int linhas = 13, inicio = c->selOpcao - linhas / 2;
        if (inicio > c->totalOpcoes - linhas) inicio = c->totalOpcoes - linhas;
        if (inicio < 0) inicio = 0;
        for (int n = 0; n < linhas && inicio + n < c->totalOpcoes; n++) {
            int i = inicio + n, y = 170 + n * 62;
            if (i == c->selOpcao) tmCaixa(p, 1300, y - 12, 590, 58, 10, tmRgb(56, 48, 140));
            tmTexto(p, c->opcoes[i], 30, 1330, y, 540, i == c->selOpcao ? branco : tmRgb(205, 205, 228));
        }
        desenharTexto(p, "X  Escolher     O  Voltar", 26, 1330, 1000, tmRgb(205, 205, 228));
    }
}

// A tela de saída é memória lenta de ler: tudo é montado em RAM e copiado uma vez só.
// Sem nada mudando, o quadro pronto é reaproveitado.
static uint32_t* tmQuadro = NULL;
static uint32_t tmAssinatura = 0;
static int tmIdade = 0;

static uint32_t tmResumo(int tema, const TemaCena* c) {
    uint32_t h = 2166136261u ^ (uint32_t)(tema * 31 + c->tipo * 7 + c->total * 131 + c->sel * 8191);
    int de = c->sel > 12 ? c->sel - 12 : 0;
    for (int i = de; i < c->total && i <= c->sel + 12; i++) {
        for (const char* s = c->nomes[i]; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
        if (c->marcados && c->marcados[i]) h = (h ^ 1u) * 16777619u;
    }
    for (const char* s = c->titulo; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    if (c->tipo == CENA_BIBLIOTECA) {
        h = (h ^ (uint32_t)(c->totalOpcoes * 977 + c->selOpcao * 131 + c->aba + c->temaJogos * 7919 + (uint32_t)(uintptr_t)c->previa)) * 16777619u;
        for (const char* s = c->subtitulo; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
        for (int i = 0; i < c->totalOpcoes; i++) for (const char* s = c->opcoes[i]; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    }
    return h;
}

static void temaDesenhar(uint32_t* p, int tema, const TemaCena* c) {
    tmPrepararFundo(tema);
    if (!tmQuadro) tmQuadro = (uint32_t*)malloc(1920 * 1080 * 4);
    if (!tmFundoPronto || !tmQuadro) return;
    bool mexendo = tmAndar(c);
    uint32_t assinatura = tmResumo(tema, c);
    bool onda = tema == TEMA_XMB && (c->quadro & 1) == 0;
    if (mexendo || onda || tmRedesenhar || assinatura != tmAssinatura || ++tmIdade >= 60) {
        tmIdade = 0;
        tmRedesenhar = false;
        tmAssinatura = assinatura;
        memcpy(tmQuadro, tmFundoPronto, 1920 * 1080 * 4);
        if (tema == TEMA_XMB) tmDesenharXmb(tmQuadro, c);
        else if (tema == TEMA_PS4) tmDesenharPs4(tmQuadro, c);
        else if (tema == TEMA_PS5) tmDesenharPs5(tmQuadro, c);
        else if (tema == TEMA_OPL) tmDesenharOpl(tmQuadro, c);
        else if (tema == TEMA_X360) tmDesenharX360(tmQuadro, c);
        else if (tema == TEMA_NETFLIX) tmDesenharNetflix(tmQuadro, c);
        else if (tema == TEMA_BIBLIOTECA) tmDesenharBiblioteca(tmQuadro, c);
    }
    memcpy(p, tmQuadro, 1920 * 1080 * 4);
}
