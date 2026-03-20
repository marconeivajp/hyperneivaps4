#include "bloco_de_notas.h"
#include "graphics.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

int estadoNotepad = 0;
bool notepadSomenteLeitura = false;
char linhasNotepad[MAX_LINHAS][MAX_CHARS_LINHA];
int linhaSelecionada = 0;
int totalLinhasNotepad = 1;

char pastaAtualNotepad[512] = "";
char pastasNotepad[100][256];
int totalPastasNotepad = 0;
int pastaSelecionada = 0;
char pastaDestinoFinal[512] = "";

char nomeArquivo[256] = "";

void carregarAtalhosNotepad() {
    strcpy(pastaAtualNotepad, "ATALHOS_RAIZ");
    strcpy(pastasNotepad[0], "/data/HyperNeiva/");
    strcpy(pastasNotepad[1], "/user/data/");
    strcpy(pastasNotepad[2], "/mnt/usb0/");
    strcpy(pastasNotepad[3], "/mnt/usb1/");
    totalPastasNotepad = 4;
    pastaSelecionada = 0;
}

void lerDiretorioNotepad(const char* path) {
    DIR* d = opendir(path);
    if (!d) {
        strcpy(msgStatus, "Nao foi possivel abrir a pasta!");
        msgTimer = 120;
        return;
    }

    totalPastasNotepad = 0;
    strcpy(pastaAtualNotepad, path);
    pastaSelecionada = 0;

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_DIR) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                if (totalPastasNotepad < 100) {
                    snprintf(pastasNotepad[totalPastasNotepad], 256, "%s", dir->d_name);
                    totalPastasNotepad++;
                }
            }
        }
    }
    closedir(d);
    if (totalPastasNotepad == 0) {
        strcpy(pastasNotepad[0], "[PASTA VAZIA]");
        totalPastasNotepad = 1;
    }
}

void inicializarNotepad() {
    memset(linhasNotepad, 0, sizeof(linhasNotepad));
    linhaSelecionada = 0;
    totalLinhasNotepad = 1;
    estadoNotepad = 0;
    notepadSomenteLeitura = false; // Ativa edição por padrão
    carregarAtalhosNotepad();
    memset(nomeArquivo, 0, sizeof(nomeArquivo));
}

// NOVA FUNÇÃO: Quebra o arquivo gigante em linhas para o Bloco de Notas exibir
void abrirTextoNoNotepad(const char* textoCompleto) {
    memset(linhasNotepad, 0, sizeof(linhasNotepad));
    linhaSelecionada = 0;
    totalLinhasNotepad = 0;
    estadoNotepad = 0;
    notepadSomenteLeitura = true; // ATIVA TRAVA DE LEITURA

    int charCount = 0;
    int linhaAtual = 0;
    int len = strlen(textoCompleto);

    for (int i = 0; i < len; i++) {
        char c = textoCompleto[i];
        if (c == '\r') continue;

        if (c == '\n' || charCount >= (MAX_CHARS_LINHA - 2)) {
            linhasNotepad[linhaAtual][charCount] = '\0';
            linhaAtual++;
            charCount = 0;
            if (linhaAtual >= MAX_LINHAS) break;
        }
        else {
            linhasNotepad[linhaAtual][charCount] = c;
            charCount++;
        }
    }
    if (charCount > 0 && linhaAtual < MAX_LINHAS) {
        linhasNotepad[linhaAtual][charCount] = '\0';
        linhaAtual++;
    }

    totalLinhasNotepad = (linhaAtual == 0) ? 1 : linhaAtual;
}

void renderizarNotepad(uint32_t* pixels) {
    for (int i = 0; i < 1920 * 1080; i++) pixels[i] = 0xFF151515;
    for (int by = 0; by < 80; by++) for (int bx = 0; bx < 1920; bx++) pixels[by * 1920 + bx] = 0xFF303030;
    for (int by = 0; by < 60; by++) for (int bx = 0; bx < 1920; bx++) pixels[(1020 + by) * 1920 + bx] = 0xFF222222;

    if (estadoNotepad == 0) {
        if (notepadSomenteLeitura) {
            desenharTexto(pixels, "LEITOR DE ARQUIVOS (SOMENTE LEITURA)", 35, 50, 25, 0xFF00AAFF);
            char rodape[128];
            snprintf(rodape, sizeof(rodape), "[Cima/Baixo] Rolar Texto   |   [O] Voltar   |   Linha: %d / %d", linhaSelecionada + 1, totalLinhasNotepad);
            desenharTexto(pixels, rodape, 25, 50, 1035, 0xFF00AAFF);
        }
        else {
            desenharTexto(pixels, "BLOCO DE NOTAS", 35, 50, 25, 0xFF00AAFF);
            desenharTexto(pixels, "[X] Editar Linha   |   [QUADRADO] Salvar Arquivo   |   [O] Sair", 25, 50, 1035, 0xFF00AAFF);
        }

        // SISTEMA DE CÂMERA/SCROLL: Mostra apenas 21 linhas dependendo da rolagem
        int linhasVisiveis = 21;
        int linhaInicial = linhaSelecionada - (linhasVisiveis / 2);
        if (linhaInicial > totalLinhasNotepad - linhasVisiveis) linhaInicial = totalLinhasNotepad - linhasVisiveis;
        if (linhaInicial < 0) linhaInicial = 0;

        for (int i = 0; i < linhasVisiveis; i++) {
            int idx = linhaInicial + i;
            if (idx >= totalLinhasNotepad) break;

            uint32_t cor = (idx == linhaSelecionada) ? 0xFF00FF00 : 0xFFDDDDDD;
            char buffer[300];
            snprintf(buffer, sizeof(buffer), "%04d: %s%s", idx + 1, linhasNotepad[idx], (idx == linhaSelecionada && !notepadSomenteLeitura) ? " <" : "");
            desenharTexto(pixels, buffer, 30, 50, 120 + (i * 40), cor);
        }
    }
    else if (estadoNotepad == 1) {
        desenharTexto(pixels, "BLOCO DE NOTAS - SELECIONAR DESTINO", 35, 50, 25, 0xFF00AAFF);
        desenharTexto(pixels, "[X] Entrar na Pasta   |   [QUADRADO] Escolher Esta Pasta   |   [O] Voltar", 25, 50, 1035, 0xFF00AAFF);

        if (strcmp(pastaAtualNotepad, "ATALHOS_RAIZ") == 0) {
            desenharTexto(pixels, "Navegando: ATALHOS RAPIDOS", 35, 50, 120, 0xFFFFFFFF);
        }
        else {
            char nav[600]; snprintf(nav, sizeof(nav), "Navegando: %s", pastaAtualNotepad);
            desenharTexto(pixels, nav, 35, 50, 120, 0xFFFFFF00);
        }

        int offsetPastas = (pastaSelecionada > 15) ? pastaSelecionada - 15 : 0;
        for (int i = 0; i < 16; i++) {
            int idx = offsetPastas + i;
            if (idx >= totalPastasNotepad) break;
            uint32_t cor = (idx == pastaSelecionada) ? 0xFF00FF00 : 0xFFDDDDDD;
            char buffer[300];
            snprintf(buffer, sizeof(buffer), "%s%s", pastasNotepad[idx], (idx == pastaSelecionada) ? " <" : "");
            desenharTexto(pixels, buffer, 30, 50, 200 + (i * 40), cor);
        }
    }
    else if (estadoNotepad == 2) {
        desenharTexto(pixels, "BLOCO DE NOTAS - NOME DO ARQUIVO", 35, 50, 25, 0xFF00AAFF);
        desenharTexto(pixels, "[X] Digitar nome (Conclua para Salvar automaticamente)   |   [O] Voltar", 25, 50, 1035, 0xFF00AAFF);
        desenharTexto(pixels, "Digite o nome do arquivo (ex: config.xml):", 35, 50, 120, 0xFFFFFFFF);
        desenharTexto(pixels, nomeArquivo, 30, 50, 180, 0xFF00FF00);

        char destFinal[600];
        snprintf(destFinal, sizeof(destFinal), "Sera salvo em: %s", pastaDestinoFinal);
        desenharTexto(pixels, destFinal, 25, 50, 250, 0xFFAAAAAA);
    }
}

void aplicarTextoNotepad(const char* textoDigitado) {
    char bufferLimpo[256];
    memset(bufferLimpo, 0, 256);
    strncpy(bufferLimpo, textoDigitado, 255);

    for (int i = 0; i < 256; i++) {
        if (bufferLimpo[i] == '\n' || bufferLimpo[i] == '\r') {
            bufferLimpo[i] = '\0';
            break;
        }
    }

    if (estadoNotepad == 0) {
        strncpy(linhasNotepad[linhaSelecionada], bufferLimpo, MAX_CHARS_LINHA - 1);
    }
    else if (estadoNotepad == 2) {
        strncpy(nomeArquivo, bufferLimpo, sizeof(nomeArquivo) - 1);
        if (strlen(nomeArquivo) > 0) {
            salvarArquivoNotepad();
        }
        else {
            snprintf(msgStatus, sizeof(msgStatus), "O nome do arquivo nao pode ser vazio!");
            msgTimer = 120;
        }
    }
}

void salvarArquivoNotepad() {
    char caminhoCompleto[1024];
    int lenPasta = strlen(pastaDestinoFinal);
    const char* separador = (lenPasta > 0 && pastaDestinoFinal[lenPasta - 1] == '/') ? "" : "/";

    if (strchr(nomeArquivo, '.') != NULL) {
        snprintf(caminhoCompleto, sizeof(caminhoCompleto), "%s%s%s", pastaDestinoFinal, separador, nomeArquivo);
    }
    else {
        snprintf(caminhoCompleto, sizeof(caminhoCompleto), "%s%s%s.txt", pastaDestinoFinal, separador, nomeArquivo);
    }

    FILE* f = fopen(caminhoCompleto, "w");
    if (f) {
        for (int i = 0; i < totalLinhasNotepad; i++) {
            fprintf(f, "%s\n", linhasNotepad[i]);
        }
        fclose(f);
        snprintf(msgStatus, sizeof(msgStatus), "Salvo com sucesso!");
        msgTimer = 180;
        estadoNotepad = 0;
    }
    else {
        snprintf(msgStatus, sizeof(msgStatus), "Erro ao salvar o arquivo!");
        msgTimer = 180;
    }
}