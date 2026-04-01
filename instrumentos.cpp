#include "instrumentos.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   
#include <math.h>     
#include <orbis/Pad.h>
#include "teclado.h"

// Bibliotecas necessárias para ler pastas e arquivos
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

extern int globalPadHandle;
extern MenuLevel menuAtual;

// ==========================================
// INTEGRAÇÃO COM O SISTEMA DE ÁUDIO
// ==========================================
extern volatile float audioTempoAtual;
extern char musicaAtual[256];
extern void tocarMusicaNova(const char* path);

// ==========================================
// NOVO: MOTOR DE LEITURA DE ACORDES REAIS (.TXT)
// ==========================================
#define MAX_CHORDS_MUSICA 1000 

struct AcordeTempo {
    float tempo;
    char nome[16];
};

static AcordeTempo acordesCarregados[MAX_CHORDS_MUSICA];
static int totalAcordesCarregados = 0;

static void carregarAcordesDaMusica(const char* audioPath) {
    totalAcordesCarregados = 0;
    char txtPath[512];
    strcpy(txtPath, audioPath);

    char* dot = strrchr(txtPath, '.');
    if (dot) {
        strcpy(dot, ".txt");
    }
    else {
        strcat(txtPath, ".txt");
    }

    FILE* f = fopen(txtPath, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && totalAcordesCarregados < MAX_CHORDS_MUSICA) {
            float sec = 0.0f;
            char nome[16] = "";
            if (sscanf(line, "%f | %15s", &sec, nome) == 2) {
                acordesCarregados[totalAcordesCarregados].tempo = sec;
                strcpy(acordesCarregados[totalAcordesCarregados].nome, nome);
                totalAcordesCarregados++;
            }
        }
        fclose(f);
    }

    if (totalAcordesCarregados == 0) {
        const char* fakeChords[] = { "C", "G", "Am", "F", "Dm", "E7", "Am", "G" };
        for (int i = 0; i < MAX_CHORDS_MUSICA; i++) {
            acordesCarregados[i].tempo = i * 4.0f;
            strcpy(acordesCarregados[i].nome, fakeChords[i % 8]);
            totalAcordesCarregados++;
        }
    }
}


// ==========================================
// VARIÁVEIS COMPARTILHADAS E ESTADOS
// ==========================================
static volatile int modoInstrumento = 0;
static volatile int violaoMenuEstado = 0;
static volatile int cursorMenuViolao = 0;
static bool touchPressionadoAnteriormente = false;
static bool l3PressionadoAnteriormente = false;

// ==========================================
// BANCO DE DADOS DE COLETÂNEAS CUSTOMIZADAS
// ==========================================
#define MAX_COLETANEAS 10
#define GRUPOS_POR_COLETANEA 7
struct ColetaneaCustom {
    char nome[64];
    int grupoOrigem[GRUPOS_POR_COLETANEA][6];
    int indiceOrigem[GRUPOS_POR_COLETANEA][6];
};
static ColetaneaCustom coletaneasCustomizadas[MAX_COLETANEAS];
static int totalColetaneas = 0;
static ColetaneaCustom coletaneaEmEdicao;
static volatile int grupoEdicaoAtual = 0;
static volatile int cursorEdicaoAcorde = 0;

// ==========================================
// SISTEMA DE NAVEGAÇÃO DE MÚSICAS (MINI-EXPLORER)
// ==========================================
static char instCaminhoAtual[512] = "/data/HyperNeiva/Musicas";
static char instNomes[500][64];
static char instPaths[500][256];
static bool instEhPasta[500];
static int instTotalItens = 0;
static int instSel = 0;
static int instScroll = 0;

static void instListarMusicas(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;

    instTotalItens = 0;

    if (strcmp(path, "/data/HyperNeiva/Musicas") != 0 && strcmp(path, "/data/HyperNeiva") != 0) {
        strcpy(instNomes[instTotalItens], "[ .. Voltar Pasta Anterior ]");
        strcpy(instPaths[instTotalItens], "..");
        instEhPasta[instTotalItens] = true;
        instTotalItens++;
    }

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && instTotalItens < 500) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);

        struct stat st;
        bool isDir = false;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
            isDir = true;
        }

        if (isDir) {
            snprintf(instNomes[instTotalItens], 64, "[ %s ]", dir->d_name);
            strcpy(instPaths[instTotalItens], fullPath);
            instEhPasta[instTotalItens] = true;
            instTotalItens++;
        }
        else {
            char temp[256]; strcpy(temp, dir->d_name);
            for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
            if (strstr(temp, ".wav") || strstr(temp, ".mp3")) {
                strncpy(instNomes[instTotalItens], dir->d_name, 63);
                strcpy(instPaths[instTotalItens], fullPath);
                instEhPasta[instTotalItens] = false;
                instTotalItens++;
            }
        }
    }
    closedir(d);

    for (int i = (strcmp(instPaths[0], "..") == 0 ? 1 : 0); i < instTotalItens - 1; i++) {
        for (int j = i + 1; j < instTotalItens; j++) {
            bool swap = false;
            if (instEhPasta[i] != instEhPasta[j]) {
                if (!instEhPasta[i] && instEhPasta[j]) swap = true;
            }
            else {
                if (strcasecmp(instNomes[i], instNomes[j]) > 0) swap = true;
            }
            if (swap) {
                char tNome[64]; strcpy(tNome, instNomes[i]); strcpy(instNomes[i], instNomes[j]); strcpy(instNomes[j], tNome);
                char tPath[256]; strcpy(tPath, instPaths[i]); strcpy(instPaths[i], instPaths[j]); strcpy(instPaths[j], tPath);
                bool tPasta = instEhPasta[i]; instEhPasta[i] = instEhPasta[j]; instEhPasta[j] = tPasta;
            }
        }
    }
    instSel = 0;
    instScroll = 0;
}

// ==========================================
// PIANO VIRTUAL E VIOLÃO
// ==========================================
static volatile bool pianoKeysPressionadas[13] = { false };
static volatile int oitavaOffset = 0;
static float pianoFases[13] = { 0.0f };
static float volumeTecla[13] = { 0.0f };
static const float pianoFreqsBase[13] = {
    261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f,
    369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f, 523.25f
};

#define NUM_GRUPOS_VIOLAO 7
static volatile int acordeAtual = 0;
static volatile int grupoAcordeAtual = 0;
static volatile bool usandoColetaneaCustomizada = false;
static volatile int indiceColetaneaAtual = 0;
static bool l1PressionadoAnteriormente = false;
static bool r1PressionadoAnteriormente = false;

static const char* nomeAcordes[NUM_GRUPOS_VIOLAO][6] = {
    { "C", "Dm", "Em", "F", "G", "Am" }, { "G", "Am", "Bm", "C", "D", "Em" },
    { "D", "Em", "F#m", "G", "A", "Bm" }, { "A", "Bm", "C#m", "D", "E", "F#m" },
    { "E", "F#m", "G#m", "A", "B", "C#m" }, { "F", "Gm", "Am", "Bb", "C", "Dm" },
    { "B", "C#m", "D#m", "E", "F#", "G#m" }
};
static const char* notasAcordes[NUM_GRUPOS_VIOLAO][6][6] = {
    { {"G2", "C3", "E3", "G3", "C4", "E4"}, {"A2", "D3", "A3", "D4", "F4", "A4"}, {"E2", "B2", "E3", "G3", "B3", "E4"}, {"F2", "C3", "F3", "A3", "C4", "F4"}, {"G2", "B2", "D3", "G3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C4", "E4"} },
    { {"G2", "B2", "D3", "G3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C4", "E4"}, {"F#2", "B2", "F#3", "B3", "D4", "F#4"},{"G2", "C3", "E3", "G3", "C4", "E4"}, {"A2", "D3", "A3", "D4", "F#4", "A4"},{"E2", "B2", "E3", "G3", "B3", "E4"} },
    { {"A2", "D3", "A3", "D4", "F#4", "A4"},{"E2", "B2", "E3", "G3", "B3", "E4"}, {"F#2", "C#3", "F#3", "A3", "C#4", "F#4"},{"G2", "B2", "D3", "G3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C#4", "E4"},{"F#2", "B2", "F#3", "B3", "D4", "F#4"} },
    { {"E2", "A2", "E3", "A3", "C#4", "E4"},{"F#2", "B2", "F#3", "B3", "D4", "F#4"},{"G#2", "C#3", "G#3", "C#4", "E4", "G#4"},{"A2", "D3", "A3", "D4", "F#4", "A4"}, {"E2", "B2", "E3", "G#3", "B3", "E4"},{"F#2", "C#3", "F#3", "A3", "C#4", "F#4"} },
    { {"E2", "B2", "E3", "G#3", "B3", "E4"},{"F#2", "C#3", "F#3", "A3", "C#4", "F#4"},{"G#2", "D#3", "G#3", "B3", "D#4", "G#4"},{"E2", "A2", "E3", "A3", "C#4", "E4"}, {"F#2", "B2", "F#3", "B3", "D#4", "F#4"},{"G#2", "C#3", "G#3", "C#4", "E4", "G#4"} },
    { {"F2", "C3", "F3", "A3", "C4", "F4"}, {"G2", "D3", "G3", "Bb3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C4", "E4"}, {"F2", "Bb2", "F3", "Bb3", "D4", "F4"}, {"G2", "C3", "E3", "G3", "C4", "E4"}, {"A2", "D3", "A3", "D4", "F4", "A4"} },
    { {"F#2", "B2", "F#3", "B3", "D#4", "F#4"},{"G#2", "C#3", "G#3", "C#4", "E4", "G#4"},{"A#2", "D#3", "A#3", "D#4", "F#4", "A#4"},{"E2", "B2", "E3", "G#3", "B3", "E4"}, {"F#2", "C#3", "F#3", "A#3", "C#4", "F#4"},{"G#2", "D#3", "G#3", "B3", "D#4", "G#4"} }
};
static const float frequenciasViolao[NUM_GRUPOS_VIOLAO][6][6] = {
    { {98.00f, 130.81f, 164.81f, 196.00f, 261.63f, 329.63f}, {110.00f, 146.83f, 220.00f, 293.66f, 349.23f, 440.00f}, {82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f}, {87.31f, 130.81f, 174.61f, 220.00f, 261.63f, 349.23f}, {98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 261.63f, 329.63f} },
    { {98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 261.63f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 293.66f, 369.99f}, {98.00f, 130.81f, 164.81f, 196.00f, 261.63f, 329.63f}, {110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f},{82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f} },
    { {110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f},{82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 220.00f, 277.18f, 369.99f}, {98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 277.18f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 293.66f, 369.99f} },
    { {82.41f, 110.00f, 164.81f, 220.00f, 277.18f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 293.66f, 369.99f}, {103.83f, 138.59f, 207.65f, 277.18f, 329.63f, 415.30f},{110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f}, {82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 220.00f, 277.18f, 369.99f} },
    { {82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 220.00f, 277.18f, 369.99f}, {103.83f, 155.56f, 207.65f, 246.94f, 311.13f, 415.30f},{82.41f, 110.00f, 164.81f, 220.00f, 277.18f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 311.13f, 369.99f}, {103.83f, 138.59f, 207.65f, 277.18f, 329.63f, 415.30f} },
    { {87.31f, 130.81f, 174.61f, 220.00f, 261.63f, 349.23f}, {98.00f, 146.83f, 196.00f, 233.08f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 261.63f, 329.63f}, {87.31f, 116.54f, 174.61f, 233.08f, 293.66f, 349.23f}, {98.00f, 130.81f, 164.81f, 196.00f, 261.63f, 329.63f}, {110.00f, 146.83f, 220.00f, 293.66f, 349.23f, 440.00f} },
    { {92.50f, 123.47f, 185.00f, 246.94f, 311.13f, 369.99f}, {103.83f, 138.59f, 207.65f, 277.18f, 329.63f, 415.30f}, {116.54f, 155.56f, 233.08f, 311.13f, 369.99f, 466.16f},{82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 233.08f, 277.18f, 369.99f}, {103.83f, 155.56f, 207.65f, 246.94f, 311.13f, 415.30f} }
};

static float ks_delayLines[6][2048];
static int ks_delayPtr[6] = { 0 };
static int ks_delayLength[6] = { 0 };
static volatile int ks_strumTimer[6] = { -1, -1, -1, -1, -1, -1 };


// =========================================================================
// RENDERIZAÇÃO GRÁFICA INTERNA E VISUALIZADOR DE ACORDES
// =========================================================================
static void desenharRetanguloAlpha(uint32_t* pixels, int x, int y, int w, int h, uint32_t cor) {
    if (!pixels) return;
    uint8_t a = (cor >> 24) & 0xFF;
    if (a == 0) return;

    for (int cy = 0; cy < h; cy++) {
        int pY = y + cy;
        if (pY < 0 || pY >= 1080) continue;
        for (int cx = 0; cx < w; cx++) {
            int pX = x + cx;
            if (pX < 0 || pX >= 1920) continue;

            if (a == 255) {
                pixels[pY * 1920 + pX] = cor;
            }
            else {
                uint32_t bg = pixels[pY * 1920 + pX];
                uint8_t br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
                uint8_t fr = (cor >> 16) & 0xFF, fg = (cor >> 8) & 0xFF, fb = cor & 0xFF;

                uint8_t nr = (fr * a + br * (255 - a)) / 255;
                uint8_t ng = (fg * a + bg_g * (255 - a)) / 255;
                uint8_t nb = (fb * a + bb * (255 - a)) / 255;

                pixels[pY * 1920 + pX] = (0xFF << 24) | (nr << 16) | (ng << 8) | nb;
            }
        }
    }
}

static void renderizarGrelhaAcordes(uint32_t* p) {
    int painelY = 820;
    int painelH = 260;

    if (audioTempoAtual <= 0.0f || strcmp(musicaAtual, "PARADO") == 0) {
        desenharRetanguloAlpha(p, 0, painelY, 1920, painelH, 0xEE020617);
        desenharRetanguloAlpha(p, 400, painelY, 4, painelH, 0xFF334155);
        desenharTexto(p, "HYPER NEIVA STUDIO v2", 30, 50, painelY + 40, 0xFF38BDF8);
        desenharTexto(p, "STATUS: Aguardando Faixa de Audio...", 20, 50, painelY + 80, 0xFF94A3B8);
        desenharTexto(p, "-> Pressione [TOUCHPAD] -> 'Escolher Musica' para carregar a musica e os acordes reais!", 18, 50, painelY + 120, 0xFF64748B);
        return;
    }

    int pixelsPerSecond = 200;
    int centerX = 400;

    // 1. Fundo Escuro do Painel Inferior
    desenharRetanguloAlpha(p, 0, painelY, 1920, painelH, 0xEE020617);

    // 2. GRELHA DE FUNDO ABSOLUTA
    float beatDuration = 60.0f / 120.0f; // Compassos de fundo a 120BPM
    int startBeatIndex = (int)((audioTempoAtual - 5.0f) / beatDuration);
    int endBeatIndex = (int)((audioTempoAtual + 10.0f) / beatDuration);

    for (int i = startBeatIndex; i <= endBeatIndex; i++) {
        if (i < 0) continue;
        float beatTime = i * beatDuration;
        int px = centerX + (int)((beatTime - audioTempoAtual) * pixelsPerSecond);

        if (px > -100 && px < 1920 + 100) {
            if (i % 4 == 0) {
                // Risco Mestre centralizado na altura
                desenharRetanguloAlpha(p, px - 1, painelY + 30, 3, 200, 0x7738BDF8);
            }
            else {
                // Riscos internos centralizados
                desenharRetanguloAlpha(p, px, painelY + 50, 2, 160, 0x6694A3B8);
            }
        }
    }

    // 3. DESENHA OS BLOCOS CARREGADOS DO ARQUIVO .TXT
    // Agora centralizados exatamente no meio dos 260px de altura do painel
    int blockH = 160;
    int blockY = painelY + 50; // Margem de 50 cima, 50 baixo

    if (totalAcordesCarregados > 0) {
        for (int i = 0; i < totalAcordesCarregados; i++) {
            float startT = acordesCarregados[i].tempo;
            float endT = (i + 1 < totalAcordesCarregados) ? acordesCarregados[i + 1].tempo : (startT + 4.0f);

            int startPx = centerX + (int)((startT - audioTempoAtual) * pixelsPerSecond);
            int endPx = centerX + (int)((endT - audioTempoAtual) * pixelsPerSecond);
            int boxWidth = endPx - startPx;

            if (endPx > -100 && startPx < 1920 + 100) {
                bool isActive = (audioTempoAtual >= startT && audioTempoAtual < endT);

                uint32_t corBloco = isActive ? 0x6638BDF8 : 0x881E293B;
                uint32_t corTexto = isActive ? 0xFFFFFFFF : 0xFF64748B;

                // Desenha o bloco centralizado verticalmente
                desenharRetanguloAlpha(p, startPx + 2, blockY, boxWidth - 4, blockH, corBloco);

                // Nome centralizado perfeitamente no bloco (blockY + 80 + compensação da fonte)
                desenharTexto(p, acordesCarregados[i].nome, 50, startPx + (boxWidth / 2) - 25, painelY + 145, corTexto);
            }
        }

        // 4. Desenha as Linhas Brancas fortes nas bordas (Z-INDEX 2)
        for (int i = 0; i < totalAcordesCarregados; i++) {
            float startT = acordesCarregados[i].tempo;
            int startPx = centerX + (int)((startT - audioTempoAtual) * pixelsPerSecond);
            if (startPx > -10 && startPx < 1920 + 10) {
                desenharRetanguloAlpha(p, startPx, painelY + 30, 3, 200, 0xEE38BDF8);
            }
        }
    }

    // 5. Linha do Tempo (Playhead) - VERDE LIMÃO
    desenharRetanguloAlpha(p, centerX - 1, painelY + 20, 4, 220, 0xFF00FF00);

    // 6. Painel de Status Inferior (Acorde Atual Real)
    const char* acordeTocando = "...";
    for (int i = totalAcordesCarregados - 1; i >= 0; i--) {
        if (audioTempoAtual >= acordesCarregados[i].tempo) {
            acordeTocando = acordesCarregados[i].nome;
            break;
        }
    }

    char textoAtual[128];
    sprintf(textoAtual, "ACORDE DA MUSICA: %s", acordeTocando);
    desenharTexto(p, textoAtual, 25, 50, painelY + 30, 0xFF38BDF8);
}


// =========================================================================
// RENDERIZAÇÃO DA UI PRINCIPAL
// =========================================================================
void renderizarInstrumentos(uint32_t* p) {
    if (globalPadHandle < 0) return;

    if (violaoMenuEstado == 0) {
        // TELA PRINCIPAL
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF151515;

        desenharTexto(p, "SISTEMA DE INSTRUMENTOS VIRTUAIS", 40, 50, 50, 0xFF00AAFF);
        desenharTexto(p, "[L3] Modo  |  [TOUCH] Menu / Musicas  |  [OPTIONS] Sair", 25, 50, 100, 0xFFAAAAAA);

        if (modoInstrumento == 0) {
            char txtOitava[128]; sprintf(txtOitava, "Escala Musical Atual: Oitava %d", 4 + oitavaOffset);
            desenharTexto(p, txtOitava, 30, 50, 150, 0xFF00FF00);

            int startX = 360; int startY = 220; int whiteW = 150; int whiteH = 500; int blackW = 80; int blackH = 300;
            int whiteMap[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
            const char* whiteLabels[8] = { "[ESQ]", "[BAIXO]", "[DIR]", "[CIMA]", "[QUAD]", "[ X ]", "[BOLA]", "[R3]" };
            const char* whiteNotes[8] = { "DO", "RE", "MI", "FA", "SOL", "LA", "SI", "DO" };

            for (int i = 0; i < 8; i++) {
                int keyIndex = whiteMap[i]; bool isPressed = pianoKeysPressionadas[keyIndex];
                uint32_t color = isPressed ? 0xFF00AAFF : 0xFFEEEEEE; uint32_t borderColor = 0xFF555555;
                int kX = startX + (i * whiteW);
                for (int y = 0; y < whiteH; y++) {
                    for (int x = 0; x < whiteW; x++) {
                        if (x == 0 || x == whiteW - 1 || y == 0 || y == whiteH - 1) p[(startY + y) * 1920 + (kX + x)] = borderColor;
                        else p[(startY + y) * 1920 + (kX + x)] = color;
                    }
                }
                desenharTexto(p, whiteNotes[i], 30, kX + 50, startY + whiteH - 80, isPressed ? 0xFFFFFFFF : 0xFF000000);
                desenharTexto(p, whiteLabels[i], 20, kX + 40, startY + whiteH - 30, isPressed ? 0xFFFFFFFF : 0xFF555555);
            }

            int blackMap[5] = { 1, 3, 6, 8, 10 }; int blackOffsets[5] = { 1, 2, 4, 5, 6 }; const char* blackLabels[5] = { "[L2]", "[L1]", "[R1]", "[R2]", "[TRI]" };
            for (int i = 0; i < 5; i++) {
                int keyIndex = blackMap[i]; bool isPressed = pianoKeysPressionadas[keyIndex];
                uint32_t color = isPressed ? 0xFF00AAFF : 0xFF222222; int kX = startX + (blackOffsets[i] * whiteW) - (blackW / 2);
                for (int y = 0; y < blackH; y++) { for (int x = 0; x < blackW; x++) p[(startY + y) * 1920 + (kX + x)] = color; }
                desenharTexto(p, blackLabels[i], 18, kX + 10, startY + blackH - 30, 0xFFFFFFFF);
            }
        }
        else {
            desenharTexto(p, "MAO ESQUERDA: Analogico (L3) escolhe o Acorde", 25, 100, 200, 0xFF00FF00);

            char txtGrupo[128];
            if (usandoColetaneaCustomizada) {
                sprintf(txtGrupo, "[L1] ou [R1] para trocar - Custom: %s (Grupo %d/7)", coletaneasCustomizadas[indiceColetaneaAtual].nome, grupoAcordeAtual + 1);
            }
            else {
                sprintf(txtGrupo, "[L1] ou [R1] para trocar - Coletanea Padrão: Grupo %d/7", grupoAcordeAtual + 1);
            }
            desenharTexto(p, txtGrupo, 25, 100, 250, 0xFF00FFFF);

            int cx = 350; int cy = 500; int radius = 180; int boxW = 120; int boxH = 60;
            int boxX[6] = { cx - boxW / 2, cx + (int)(radius * 0.866f) - boxW / 2, cx + (int)(radius * 0.866f) - boxW / 2, cx - boxW / 2, cx - (int)(radius * 0.866f) - boxW / 2, cx - (int)(radius * 0.866f) - boxW / 2 };
            int boxY[6] = { cy - radius - boxH / 2, cy - (int)(radius * 0.5f) - boxH / 2, cy + (int)(radius * 0.5f) - boxH / 2, cy + radius - boxH / 2, cy + (int)(radius * 0.5f) - boxH / 2, cy - (int)(radius * 0.5f) - boxH / 2 };

            for (int i = 0; i < 6; i++) {
                uint32_t color = (acordeAtual == i) ? 0xFF00AAFF : 0xFF333333;
                for (int y = 0; y < boxH; y++) { for (int x = 0; x < boxW; x++) p[(boxY[i] + y) * 1920 + (boxX[i] + x)] = color; }

                if (usandoColetaneaCustomizada) {
                    int gOrig = coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[grupoAcordeAtual][i];
                    int iOrig = coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[grupoAcordeAtual][i];
                    desenharTexto(p, nomeAcordes[gOrig][iOrig], 30, boxX[i] + 35, boxY[i] + 40, 0xFFFFFFFF);
                }
                else {
                    desenharTexto(p, nomeAcordes[grupoAcordeAtual][i], 30, boxX[i] + 35, boxY[i] + 40, 0xFFFFFFFF);
                }
            }

            int gridX = 1450; int gridY = 150;
            desenharTexto(p, "TABELA DE GRUPOS", 25, gridX, gridY, 0xFF00AAFF);
            for (int g = 0; g < GRUPOS_POR_COLETANEA; g++) {
                char headerG[8]; sprintf(headerG, "G%d", g + 1);
                int cX = gridX + (g % 4) * 80; int cY = gridY + 40 + (g / 4) * 250;
                desenharTexto(p, headerG, 22, cX, cY, (grupoAcordeAtual == g) ? 0xFF00FFFF : 0xFFFFFFFF);
                for (int c = 0; c < 6; c++) {
                    if (usandoColetaneaCustomizada) {
                        int gO = coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[g][c];
                        int iO = coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[g][c];
                        desenharTexto(p, nomeAcordes[gO][iO], 18, cX, cY + 30 + c * 30, 0xFFAAAAAA);
                    }
                    else {
                        desenharTexto(p, nomeAcordes[g][c], 18, cX, cY + 30 + c * 30, 0xFFAAAAAA);
                    }
                }
            }

            // AJUSTE DA MÃO DIREITA - AGORA POSICIONADA LÁ NO TOPO (Y=150)
            int rhY = 150;
            desenharTexto(p, "MAO DIREITA: Palhetada e Dedilhado", 30, 800, rhY, 0xFF00AAFF);
            desenharTexto(p, "Mova o Analogico Direito (R3) para palhetar!", 20, 800, rhY + 40, 0xFFFFFFFF);

            char bufCordas[128];
            int gReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[grupoAcordeAtual][acordeAtual] : grupoAcordeAtual;
            int iReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[grupoAcordeAtual][acordeAtual] : acordeAtual;

            sprintf(bufCordas, "[ L2 ]       - Corda 6 (%s)", notasAcordes[gReal][iReal][0]); desenharTexto(p, bufCordas, 20, 800, rhY + 100, 0xFFEEEEEE);
            sprintf(bufCordas, "[ ESQUERDA ] - Corda 5 (%s)", notasAcordes[gReal][iReal][1]); desenharTexto(p, bufCordas, 20, 800, rhY + 150, 0xFFEEEEEE);
            sprintf(bufCordas, "[ DIREITA ]  - Corda 4 (%s)", notasAcordes[gReal][iReal][2]); desenharTexto(p, bufCordas, 20, 800, rhY + 200, 0xFFEEEEEE);
            sprintf(bufCordas, "[ R2 ]       - Corda 3 (%s)", notasAcordes[gReal][iReal][3]); desenharTexto(p, bufCordas, 20, 800, rhY + 250, 0xFFEEEEEE);
            sprintf(bufCordas, "[QUADRADO]   - Corda 2 (%s)", notasAcordes[gReal][iReal][4]); desenharTexto(p, bufCordas, 20, 800, rhY + 300, 0xFFEEEEEE);
            sprintf(bufCordas, "[TRIANGULO]  - Corda 1 (%s)", notasAcordes[gReal][iReal][5]); desenharTexto(p, bufCordas, 20, 800, rhY + 350, 0xFFEEEEEE);
        }

        // RENDERIZA A FITA DE ACORDES
        renderizarGrelhaAcordes(p);
    }
    else {
        // MENUS CUSTOMIZADOS (Fundo Escurecido)
        for (int i = 0; i < 1920 * 1080; i++) {
            uint32_t cor = p[i]; p[i] = 0xFF000000 | ((((cor >> 16) & 0xFF) / 4) << 16) | ((((cor >> 8) & 0xFF) / 4) << 8) | ((cor & 0xFF) / 4);
        }

        if (violaoMenuEstado == 1) {
            desenharTexto(p, "OPCOES DO ESTUDIO (TOUCHPAD PARA FECHAR)", 40, 500, 200, 0xFF00AAFF);
            const char* menuOpts[] = {
                "Voltar ao Instrumento",
                "Escolher Musica de Fundo (MP3/WAV)",
                "Criar Nova Coletanea Customizada",
                "Restaurar Padrao Original"
            };
            int numBase = 4;

            for (int i = 0; i < numBase + totalColetaneas; i++) {
                uint32_t cor = (cursorMenuViolao == i) ? 0xFF00FFFF : 0xFFFFFFFF;
                if (i < numBase) {
                    desenharTexto(p, menuOpts[i], 30, 600, 300 + (i * 50), cor);
                }
                else {
                    char bufG[128]; sprintf(bufG, "Carregar: %s", coletaneasCustomizadas[i - numBase].nome);
                    desenharTexto(p, bufG, 30, 600, 300 + (i * 50), cor);
                }
            }
        }
        else if (violaoMenuEstado == 2) {
            desenharTexto(p, "CRIADOR DE COLETANEA VIRTUAL", 40, 500, 100, 0xFF00AAFF);
            char bufG[64]; sprintf(bufG, "GRUPO ATUAL: %d DE 7 (Use [L1] e [R1] para mudar de grupo)", grupoEdicaoAtual + 1);
            desenharTexto(p, bufG, 30, 500, 160, 0xFF00FFFF);
            desenharTexto(p, "[CIMA/BAIXO] Posicao | [ESQ/DIR] Acorde | [QUADRADO] Salvar e Nomear", 20, 500, 220, 0xFF00FF00);

            for (int i = 0; i < 6; i++) {
                uint32_t cor = (cursorEdicaoAcorde == i) ? 0xFF00FFFF : 0xFFFFFFFF;
                int gOrig = coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][i];
                int iOrig = coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][i];
                char bufL[128]; sprintf(bufL, "Posicao %d: < %s >", i + 1, nomeAcordes[gOrig][iOrig]);
                desenharTexto(p, bufL, 35, 600, 300 + (i * 60), cor);
            }
        }
        else if (violaoMenuEstado == 3) {
            // NAVEGADOR DE MÚSICAS (MINI-EXPLORER)
            desenharTexto(p, "SELECIONAR FAIXA DE AUDIO", 40, 400, 100, 0xFF00AAFF);
            char bufDir[512]; sprintf(bufDir, "PASTA ATUAL: %s", instCaminhoAtual);
            desenharTexto(p, bufDir, 20, 400, 150, 0xFFAAAAAA);
            desenharTexto(p, "[X] Carregar e Tocar Musica | [O] Voltar", 20, 400, 180, 0xFF00FF00);

            int maxVis = 15;
            for (int i = 0; i < maxVis && (i + instScroll) < instTotalItens; i++) {
                int idx = i + instScroll;
                uint32_t cor = (idx == instSel) ? 0xFF00FFFF : (instEhPasta[idx] ? 0xFFEAB308 : 0xFFFFFFFF);
                desenharTexto(p, instNomes[idx], 30, 400, 250 + (i * 45), cor);
            }

            if (instTotalItens == 0) {
                desenharTexto(p, "Nenhuma musica MP3 ou WAV encontrada nesta pasta.", 30, 400, 250, 0xFF555555);
            }
        }
    }
}

// =======================================================
// THREAD DE ÁUDIO & CONTROLE EM TEMPO REAL
// =======================================================
void misturarAudioPiano(int16_t* bufferAudio, size_t frames) {
    if (menuAtual != MENU_INSTRUMENTOS) {
        for (int k = 0; k < 13; k++) { pianoKeysPressionadas[k] = false; volumeTecla[k] = 0.0f; pianoFases[k] = 0.0f; }
        for (int k = 0; k < 6; k++) ks_delayLength[k] = 0;
        return;
    }

    if (globalPadHandle >= 0) {
        OrbisPadData data;
        if (scePadReadState(globalPadHandle, &data) == 0) {
            uint32_t b = data.buttons;

            bool touchAtual = (b & ORBIS_PAD_BUTTON_TOUCH_PAD);
            if (touchAtual && !touchPressionadoAnteriormente && modoInstrumento == 1) {
                if (violaoMenuEstado == 0) violaoMenuEstado = 1;
                else violaoMenuEstado = 0;
            }
            touchPressionadoAnteriormente = touchAtual;

            if (violaoMenuEstado == 0) {
                bool l3Atual = (b & ORBIS_PAD_BUTTON_L3);
                if (l3Atual && !l3PressionadoAnteriormente) modoInstrumento = (modoInstrumento == 0) ? 1 : 0;
                l3PressionadoAnteriormente = l3Atual;

                uint8_t* raw = (uint8_t*)&data;
                uint8_t lx = raw[0x04]; uint8_t ly = raw[0x05];
                uint8_t rx = raw[0x06]; uint8_t ry = raw[0x07];

                if (modoInstrumento == 0) {
                    pianoKeysPressionadas[0] = (b & ORBIS_PAD_BUTTON_LEFT); pianoKeysPressionadas[1] = (b & ORBIS_PAD_BUTTON_L2);
                    pianoKeysPressionadas[2] = (b & ORBIS_PAD_BUTTON_DOWN); pianoKeysPressionadas[3] = (b & ORBIS_PAD_BUTTON_L1);
                    pianoKeysPressionadas[4] = (b & ORBIS_PAD_BUTTON_RIGHT); pianoKeysPressionadas[5] = (b & ORBIS_PAD_BUTTON_UP);
                    pianoKeysPressionadas[6] = (b & ORBIS_PAD_BUTTON_R1); pianoKeysPressionadas[7] = (b & ORBIS_PAD_BUTTON_SQUARE);
                    pianoKeysPressionadas[8] = (b & ORBIS_PAD_BUTTON_R2); pianoKeysPressionadas[9] = (b & ORBIS_PAD_BUTTON_CROSS);
                    pianoKeysPressionadas[10] = (b & ORBIS_PAD_BUTTON_TRIANGLE); pianoKeysPressionadas[11] = (b & ORBIS_PAD_BUTTON_CIRCLE);
                    pianoKeysPressionadas[12] = (b & ORBIS_PAD_BUTTON_R3);

                    static int cooldownAnalogo = 0;
                    if (cooldownAnalogo > 0) cooldownAnalogo--;
                    else {
                        if (lx < 50 && oitavaOffset > -2) { oitavaOffset--; cooldownAnalogo = 40; }
                        else if (lx > 200 && oitavaOffset < 3) { oitavaOffset++; cooldownAnalogo = 40; }
                    }
                }
                else {
                    bool l1Atual = (b & ORBIS_PAD_BUTTON_L1) != 0;
                    bool r1Atual = (b & ORBIS_PAD_BUTTON_R1) != 0;
                    if (l1Atual && !l1PressionadoAnteriormente) { grupoAcordeAtual--; if (grupoAcordeAtual < 0) grupoAcordeAtual = GRUPOS_POR_COLETANEA - 1; }
                    if (r1Atual && !r1PressionadoAnteriormente) { grupoAcordeAtual++; if (grupoAcordeAtual >= GRUPOS_POR_COLETANEA) grupoAcordeAtual = 0; }
                    l1PressionadoAnteriormente = l1Atual; r1PressionadoAnteriormente = r1Atual;

                    int dx = lx - 128; int dy = ly - 128;
                    if (dx * dx + dy * dy > 2500) {
                        float angle = atan2f((float)dy, (float)dx) * 180.0f / 3.14159f;
                        if (angle > -120 && angle <= -60) acordeAtual = 0;
                        else if (angle > -60 && angle <= 0) acordeAtual = 1;
                        else if (angle > 0 && angle <= 60) acordeAtual = 2;
                        else if (angle > 60 && angle <= 120) acordeAtual = 3;
                        else if (angle > 120 && angle <= 180) acordeAtual = 4;
                        else acordeAtual = 5;
                    }

                    static uint8_t last_ry = 128;
                    if (ry > 180 && last_ry <= 180) { for (int i = 0; i < 6; i++) ks_strumTimer[i] = i * 250; }
                    else if (ry < 70 && last_ry >= 70) { for (int i = 0; i < 6; i++) ks_strumTimer[5 - i] = i * 250; }
                    last_ry = ry;

                    static bool last_fp_keys[6] = { false };
                    bool fp_keys[6] = {
                        (b & ORBIS_PAD_BUTTON_L2) != 0, (b & ORBIS_PAD_BUTTON_LEFT) != 0, (b & ORBIS_PAD_BUTTON_RIGHT) != 0,
                        (b & ORBIS_PAD_BUTTON_R2) != 0, (b & ORBIS_PAD_BUTTON_SQUARE) != 0, (b & ORBIS_PAD_BUTTON_TRIANGLE) != 0
                    };
                    for (int i = 0; i < 6; i++) {
                        if (fp_keys[i] && !last_fp_keys[i]) ks_strumTimer[i] = 1;
                        last_fp_keys[i] = fp_keys[i];
                    }
                }
            }
            else if (violaoMenuEstado == 1) {
                static int holdTimerUp1 = 0, holdTimerDown1 = 0;

                if (b & ORBIS_PAD_BUTTON_UP) {
                    if (holdTimerUp1 == 0 || holdTimerUp1 > 20) {
                        cursorMenuViolao--;
                        if (cursorMenuViolao < 0) cursorMenuViolao = 3 + totalColetaneas;
                        if (holdTimerUp1 > 20) holdTimerUp1 = 15;
                    }
                    holdTimerUp1++;
                }
                else holdTimerUp1 = 0;

                if (b & ORBIS_PAD_BUTTON_DOWN) {
                    if (holdTimerDown1 == 0 || holdTimerDown1 > 20) {
                        cursorMenuViolao++;
                        if (cursorMenuViolao > 3 + totalColetaneas) cursorMenuViolao = 0;
                        if (holdTimerDown1 > 20) holdTimerDown1 = 15;
                    }
                    holdTimerDown1++;
                }
                else holdTimerDown1 = 0;

                static bool x_prev = false;
                bool xBtn = (b & ORBIS_PAD_BUTTON_CROSS) != 0;
                if (xBtn && !x_prev) {
                    if (cursorMenuViolao == 0) violaoMenuEstado = 0;
                    else if (cursorMenuViolao == 1) {
                        instListarMusicas(instCaminhoAtual);
                        violaoMenuEstado = 3;
                    }
                    else if (cursorMenuViolao == 2) {
                        if (totalColetaneas < MAX_COLETANEAS) {
                            for (int g = 0; g < GRUPOS_POR_COLETANEA; g++) {
                                for (int c = 0; c < 6; c++) { coletaneaEmEdicao.grupoOrigem[g][c] = g; coletaneaEmEdicao.indiceOrigem[g][c] = c; }
                            }
                            cursorEdicaoAcorde = 0; grupoEdicaoAtual = 0; violaoMenuEstado = 2;
                        }
                    }
                    else if (cursorMenuViolao == 3) { totalColetaneas = 0; usandoColetaneaCustomizada = false; violaoMenuEstado = 0; }
                    else { indiceColetaneaAtual = cursorMenuViolao - 4; usandoColetaneaCustomizada = true; grupoAcordeAtual = 0; violaoMenuEstado = 0; }
                }
                x_prev = xBtn;
            }
            else if (violaoMenuEstado == 2) {
                static int htUp = 0, htDown = 0, htLeft = 0, htRight = 0;
                static bool l1_prev = false, r1_prev = false, square_prev = false;

                if (b & ORBIS_PAD_BUTTON_UP) { if (htUp == 0 || htUp > 20) { cursorEdicaoAcorde--; if (cursorEdicaoAcorde < 0) cursorEdicaoAcorde = 5; if (htUp > 20) htUp = 15; } htUp++; }
                else htUp = 0;

                if (b & ORBIS_PAD_BUTTON_DOWN) { if (htDown == 0 || htDown > 20) { cursorEdicaoAcorde++; if (cursorEdicaoAcorde > 5) cursorEdicaoAcorde = 0; if (htDown > 20) htDown = 15; } htDown++; }
                else htDown = 0;

                if (b & ORBIS_PAD_BUTTON_LEFT || b & ORBIS_PAD_BUTTON_RIGHT) {
                    int flatIndex = coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] * 6 + coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][cursorEdicaoAcorde];
                    if (b & ORBIS_PAD_BUTTON_LEFT) { if (htLeft == 0 || htLeft > 15) { flatIndex--; if (flatIndex < 0) flatIndex = 41; if (htLeft > 15) htLeft = 10; } htLeft++; }
                    else htLeft = 0;
                    if (b & ORBIS_PAD_BUTTON_RIGHT) { if (htRight == 0 || htRight > 15) { flatIndex++; if (flatIndex > 41) flatIndex = 0; if (htRight > 15) htRight = 10; } htRight++; }
                    else htRight = 0;
                    coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] = flatIndex / 6;
                    coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] = flatIndex % 6;
                }
                else { htLeft = 0; htRight = 0; }

                bool l1Btn = (b & ORBIS_PAD_BUTTON_L1) != 0; bool r1Btn = (b & ORBIS_PAD_BUTTON_R1) != 0;
                if (l1Btn && !l1_prev) { grupoEdicaoAtual--; if (grupoEdicaoAtual < 0) grupoEdicaoAtual = GRUPOS_POR_COLETANEA - 1; }
                if (r1Btn && !r1_prev) { grupoEdicaoAtual++; if (grupoEdicaoAtual >= GRUPOS_POR_COLETANEA) grupoEdicaoAtual = 0; }
                l1_prev = l1Btn; r1_prev = r1Btn;

                bool squareBtn = (b & ORBIS_PAD_BUTTON_SQUARE) != 0;
                if (squareBtn && !square_prev) {
                    char bufferNome[64];
                    sprintf(bufferNome, "Minha Coletanea %d", totalColetaneas + 1);

                    strcpy(coletaneaEmEdicao.nome, bufferNome);
                    coletaneasCustomizadas[totalColetaneas] = coletaneaEmEdicao;
                    indiceColetaneaAtual = totalColetaneas;
                    totalColetaneas++;
                    usandoColetaneaCustomizada = true;
                    grupoAcordeAtual = 0;
                    violaoMenuEstado = 0;
                }
                square_prev = squareBtn;
            }
            else if (violaoMenuEstado == 3) {
                static int htUp3 = 0, htDown3 = 0;

                if (b & ORBIS_PAD_BUTTON_UP) {
                    if (htUp3 == 0 || htUp3 > 20) {
                        instSel--;
                        if (instSel < 0) instSel = instTotalItens > 0 ? instTotalItens - 1 : 0;
                        if (instSel < instScroll) instScroll = instSel;
                        if (instSel > instScroll + 14) instScroll = instSel - 14;
                        if (htUp3 > 20) htUp3 = 15;
                    }
                    htUp3++;
                }
                else htUp3 = 0;

                if (b & ORBIS_PAD_BUTTON_DOWN) {
                    if (htDown3 == 0 || htDown3 > 20) {
                        instSel++;
                        if (instSel >= instTotalItens) instSel = 0;
                        if (instSel > instScroll + 14) instScroll = instSel - 14;
                        if (instSel < instScroll) instScroll = instSel;
                        if (htDown3 > 20) htDown3 = 15;
                    }
                    htDown3++;
                }
                else htDown3 = 0;

                static bool x_prev = false, o_prev = false;
                bool xBtn = (b & ORBIS_PAD_BUTTON_CROSS) != 0;
                if (xBtn && !x_prev) {
                    if (instTotalItens > 0) {
                        if (instEhPasta[instSel]) {
                            if (strcmp(instPaths[instSel], "..") == 0) {
                                char* lastSlash = strrchr(instCaminhoAtual, '/');
                                if (lastSlash && lastSlash != instCaminhoAtual) {
                                    *lastSlash = '\0';
                                    instListarMusicas(instCaminhoAtual);
                                }
                            }
                            else {
                                strcpy(instCaminhoAtual, instPaths[instSel]);
                                instListarMusicas(instCaminhoAtual);
                            }
                        }
                        else {
                            tocarMusicaNova(instPaths[instSel]);
                            carregarAcordesDaMusica(instPaths[instSel]);
                            violaoMenuEstado = 0;
                        }
                    }
                }
                x_prev = xBtn;

                bool oBtn = (b & ORBIS_PAD_BUTTON_CIRCLE) != 0;
                if (oBtn && !o_prev) {
                    violaoMenuEstado = 1;
                }
                o_prev = oBtn;
            }
        }
    }

    if (violaoMenuEstado != 0) return;

    float multiplicadorEscalaPiano = powf(2.0f, (float)oitavaOffset);

    for (size_t i = 0; i < frames; i++) {
        float mixTotal = 0.0f;

        if (modoInstrumento == 0) {
            for (int k = 0; k < 13; k++) {
                if (pianoKeysPressionadas[k]) { volumeTecla[k] += 0.005f; if (volumeTecla[k] > 1.0f) volumeTecla[k] = 1.0f; }
                else { volumeTecla[k] -= 0.002f; if (volumeTecla[k] < 0.0f) { volumeTecla[k] = 0.0f; pianoFases[k] = 0.0f; } }
                if (volumeTecla[k] > 0.0f) {
                    float freqAtual = pianoFreqsBase[k] * multiplicadorEscalaPiano;
                    mixTotal += sinf(pianoFases[k]) * (12000.0f * volumeTecla[k]);
                    pianoFases[k] += (2.0f * 3.1415926535f * freqAtual) / 48000.0f;
                    if (pianoFases[k] > 2.0f * 3.1415926535f) pianoFases[k] -= 2.0f * 3.1415926535f;
                }
            }
        }
        else {
            for (int k = 0; k < 6; k++) {
                if (ks_strumTimer[k] > 0) {
                    ks_strumTimer[k]--;
                    if (ks_strumTimer[k] == 0) {
                        int gReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[grupoAcordeAtual][acordeAtual] : grupoAcordeAtual;
                        int iReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[grupoAcordeAtual][acordeAtual] : acordeAtual;

                        int len = (int)(48000.0f / frequenciasViolao[gReal][iReal][k]);
                        if (len > 2047) len = 2047;
                        ks_delayLength[k] = len; ks_delayPtr[k] = 0;
                        for (int j = 0; j < len; j++) ks_delayLines[k][j] = ((rand() % 10000) / 5000.0f - 1.0f);
                    }
                }
                if (ks_delayLength[k] > 0) {
                    int ptr = ks_delayPtr[k]; int prevPtr = (ptr - 1 < 0) ? (ks_delayLength[k] - 1) : (ptr - 1);
                    float current = ks_delayLines[k][ptr]; float prev = ks_delayLines[k][prevPtr];
                    float newVal = 0.998f * 0.5f * (current + prev);
                    ks_delayLines[k][ptr] = newVal; ks_delayPtr[k] = (ptr + 1) % ks_delayLength[k];
                    mixTotal += newVal * 15000.0f;
                }
            }
        }

        if (mixTotal != 0.0f) {
            int32_t mixedL = bufferAudio[i * 2] + (int32_t)mixTotal; int32_t mixedR = bufferAudio[i * 2 + 1] + (int32_t)mixTotal;
            if (mixedL > 32767) mixedL = 32767; else if (mixedL < -32768) mixedL = -32768;
            if (mixedR > 32767) mixedR = 32767; else if (mixedR < -32768) mixedR = -32768;
            bufferAudio[i * 2] = (int16_t)mixedL; bufferAudio[i * 2 + 1] = (int16_t)mixedR;
        }
    }
}