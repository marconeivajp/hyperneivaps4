// QR Code mínimo para links curtos (até 106 bytes): modo byte, correção L, versões 1 a 5,
// máscara 0. Nessas versões com correção L há um bloco só, sem intercalar códigos.
// Usado pelo PS4 (network.cpp, QR na tela) e pelo HyperNeivaPC.exe (QR no console).
#pragma once
#include <stdint.h>
#include <string.h>

#define QR_MAX_LADO 37

static uint8_t qrMul(uint8_t x, uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return (uint8_t)z;
}

// Reed-Solomon: 'grau' bytes de correção para 'n' bytes de dados.
static void qrReedSolomon(const uint8_t* dados, int n, int grau, uint8_t* saida) {
    uint8_t gerador[32];
    memset(gerador, 0, sizeof(gerador));
    gerador[grau - 1] = 1;
    uint8_t raiz = 1;
    for (int i = 0; i < grau; i++) {
        for (int j = 0; j < grau; j++) {
            gerador[j] = qrMul(gerador[j], raiz);
            if (j + 1 < grau) gerador[j] ^= gerador[j + 1];
        }
        raiz = qrMul(raiz, 0x02);
    }
    memset(saida, 0, grau);
    for (int i = 0; i < n; i++) {
        uint8_t fator = dados[i] ^ saida[0];
        memmove(saida, saida + 1, grau - 1);
        saida[grau - 1] = 0;
        for (int j = 0; j < grau; j++) saida[j] ^= qrMul(gerador[j], fator);
    }
}

// Bits do formato (correção L = 01, máscara 0..7) já com BCH e a máscara 0x5412.
static int qrBitsFormato(int mascara) {
    int dados = (1 << 3) | mascara;
    int resto = dados;
    for (int i = 0; i < 10; i++) resto = (resto << 1) ^ ((resto >> 9) * 0x537);
    return ((dados << 10) | resto) ^ 0x5412;
}

// Monta o QR de 'texto'. 'mod' recebe lado*lado bytes (1 = escuro). Devolve o lado ou 0.
static int qrGerar(const char* texto, uint8_t* mod) {
    static const int dadosV[6] = { 0, 19, 34, 55, 80, 108 };
    static const int correcaoV[6] = { 0, 7, 10, 15, 20, 26 };
    int len = (int)strlen(texto);
    int v = 1;
    while (v <= 5 && 4 + 8 + 8 * len > dadosV[v] * 8) v++;
    if (v > 5) return 0;
    int lado = 17 + 4 * v;
    int nDados = dadosV[v], nCorr = correcaoV[v];

    // Bits: modo byte (0100), tamanho (8 bits), bytes, terminador e preenchimento.
    uint8_t cod[140];
    memset(cod, 0, sizeof(cod));
    int bit = 0;
#define QR_POR(valor, n) for (int _k = (n) - 1; _k >= 0; _k--, bit++) if (((valor) >> _k) & 1) cod[bit >> 3] |= (uint8_t)(0x80 >> (bit & 7))
    QR_POR(4, 4);
    QR_POR(len, 8);
    for (int i = 0; i < len; i++) QR_POR((uint8_t)texto[i], 8);
    int capBits = nDados * 8;
    int term = capBits - bit < 4 ? capBits - bit : 4;
    bit += term;
    bit = (bit + 7) & ~7;
    for (int pad = 0xEC; bit < capBits; pad ^= 0xEC ^ 0x11) { QR_POR(pad, 8); }
#undef QR_POR
    qrReedSolomon(cod, nDados, nCorr, cod + nDados);

    // Padrões fixos. 'func' marca o que não recebe dados.
    uint8_t func[QR_MAX_LADO * QR_MAX_LADO];
    memset(mod, 0, lado * lado);
    memset(func, 0, sizeof(func));
#define QR_POE(x, y, escuro) do { mod[(y) * lado + (x)] = (escuro) ? 1 : 0; func[(y) * lado + (x)] = 1; } while (0)
    for (int i = 0; i < lado; i++) { QR_POE(6, i, i % 2 == 0); QR_POE(i, 6, i % 2 == 0); }
    int cantos[3][2] = { { 3, 3 }, { lado - 4, 3 }, { 3, lado - 4 } };
    for (int c = 0; c < 3; c++) {
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int x = cantos[c][0] + dx, y = cantos[c][1] + dy;
            if (x < 0 || y < 0 || x >= lado || y >= lado) continue;
            int d = dx < 0 ? -dx : dx; int e = dy < 0 ? -dy : dy; int m = d > e ? d : e;
            QR_POE(x, y, m != 2 && m != 4);
        }
    }
    if (v >= 2) {
        int p = lado - 7;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int d = dx < 0 ? -dx : dx; int e = dy < 0 ? -dy : dy; int m = d > e ? d : e;
            QR_POE(p + dx, p + dy, m != 1);
        }
    }
    // Formato: reservado agora, escrito depois da máscara.
    int fmt = qrBitsFormato(0);
    for (int i = 0; i <= 5; i++) QR_POE(8, i, (fmt >> i) & 1);
    QR_POE(8, 7, (fmt >> 6) & 1);
    QR_POE(8, 8, (fmt >> 7) & 1);
    QR_POE(7, 8, (fmt >> 8) & 1);
    for (int i = 9; i < 15; i++) QR_POE(14 - i, 8, (fmt >> i) & 1);
    for (int i = 0; i < 8; i++) QR_POE(lado - 1 - i, 8, (fmt >> i) & 1);
    for (int i = 8; i < 15; i++) QR_POE(8, lado - 15 + i, (fmt >> i) & 1);
    QR_POE(8, lado - 8, 1);
#undef QR_POE

    // Dados em zigue-zague de baixo para cima, duas colunas por vez, pulando a coluna 6.
    int total = (nDados + nCorr) * 8, i = 0;
    for (int dir = lado - 1; dir >= 1; dir -= 2) {
        if (dir == 6) dir = 5;
        for (int vert = 0; vert < lado; vert++) {
            for (int j = 0; j < 2; j++) {
                int x = dir - j;
                int subindo = ((dir + 1) & 2) == 0;
                int y = subindo ? lado - 1 - vert : vert;
                if (func[y * lado + x]) continue;
                if (i < total) { mod[y * lado + x] = (cod[i >> 3] >> (7 - (i & 7))) & 1; i++; }
            }
        }
    }
    // Máscara 0: inverte onde (x + y) é par, fora dos padrões fixos.
    for (int y = 0; y < lado; y++) for (int x = 0; x < lado; x++)
        if (!func[y * lado + x] && ((x + y) % 2 == 0)) mod[y * lado + x] ^= 1;
    return lado;
}
