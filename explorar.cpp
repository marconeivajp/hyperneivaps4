// --- CORREÇÕES DE COMPILAÇÃO DO SDK DO PS4 ---
#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

struct OrbisImeSettingsExtended;
// ---------------------------------------------

#include "explorar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include "ImeDialog.h"
#include "CommonDialog.h"
#include "bloco_de_notas.h" 

#define IME_STATUS_RUNNING 1
#define IME_STATUS_FINISHED 2

// Instanciamento das variáveis do Painel Direito (SEM DUPLICADAS)
char pathExplorar[256] = "";
char baseRaiz[256] = "";
bool marcados[3000];

// Instanciamento das variáveis do Painel Esquerdo e Controle de Foco
bool painelDuplo = false;
int painelAtivo = 1; // Começa no painel direito (padrão)
char pathExplorarEsq[256] = "";
char nomesEsq[3000][64];
bool marcadosEsq[3000];
int totalItensEsq = 0;
int selEsq = 0;
MenuLevel menuAtualEsq = MENU_EXPLORAR_HOME;

// Área de transferência
char clipboardPaths[100][256];
int clipboardCount = 0;
bool clipboardIsCut = false;
bool showOpcoes = false;
int selOpcao = 0;
const char* listaOpcoes[10] = {
    "nova pasta", "novo arquivo", "copiar", "recortar",
    "colar", "renomear", "deletar", "propriedades",
    "selecionar", "selecionar tudo"
};

// Variáveis do Teclado e Renomeação
bool esperandoNomePasta = false;
bool esperandoRenomear = false;
wchar_t textoTeclado[256] = L"";
char oldPathParaRenomear[512] = "";
char oldExtParaRenomear[64] = "";
bool ehPastaParaRenomear = false;

void copiarArquivoReal(const char* origem, const char* destino) {
    FILE* src = fopen(origem, "rb"); if (!src) return;
    FILE* dst = fopen(destino, "wb"); if (!dst) { fclose(src); return; }
    char buffer[65536]; size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    fclose(src); fclose(dst);
}

// Lista diretório do PAINEL DIREITO
void listarDiretorio(const char* path) {
    DIR* d = opendir(path);
    if (!d) { sprintf(msgStatus, "ERRO AO ACESSAR"); msgTimer = 90; return; }
    memset(marcados, 0, sizeof(marcados));
    memset(nomes, 0, sizeof(nomes));
    ItemLista temp[3000]; int count = 0; struct dirent* dir;

    while ((dir = readdir(d)) != NULL && count < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        strncpy(temp[count].nome, dir->d_name, 63);
        temp[count].ehPasta = (dir->d_type == DT_DIR);
        count++;
    }
    closedir(d);

    for (int i = 0; i < count - 1; i++) for (int j = 0; j < count - i - 1; j++) {
        bool trocar = false;
        if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
        else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
        if (trocar) { ItemLista aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
    }
    for (int i = 0; i < count; i++) {
        if (temp[i].ehPasta) sprintf(nomes[i], "[%s]", temp[i].nome);
        else strcpy(nomes[i], temp[i].nome);
    }
    totalItens = count; strcpy(pathExplorar, path); menuAtual = MENU_EXPLORAR;
    sel = 0;
}

// Lista diretório do PAINEL ESQUERDO
void listarDiretorioEsq(const char* path) {
    DIR* d = opendir(path);
    if (!d) { sprintf(msgStatus, "ERRO AO ACESSAR"); msgTimer = 90; return; }
    memset(marcadosEsq, 0, sizeof(marcadosEsq));
    memset(nomesEsq, 0, sizeof(nomesEsq));
    ItemLista temp[3000]; int count = 0; struct dirent* dir;

    while ((dir = readdir(d)) != NULL && count < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        strncpy(temp[count].nome, dir->d_name, 63);
        temp[count].ehPasta = (dir->d_type == DT_DIR);
        count++;
    }
    closedir(d);

    for (int i = 0; i < count - 1; i++) for (int j = 0; j < count - i - 1; j++) {
        bool trocar = false;
        if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
        else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
        if (trocar) { ItemLista aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
    }
    for (int i = 0; i < count; i++) {
        if (temp[i].ehPasta) sprintf(nomesEsq[i], "[%s]", temp[i].nome);
        else strcpy(nomesEsq[i], temp[i].nome);
    }
    totalItensEsq = count; strcpy(pathExplorarEsq, path); menuAtualEsq = MENU_EXPLORAR;
    selEsq = 0;
}

void acaoArquivo(int op) {
    // Determina dinamicamente sobre qual painel a ação vai agir
    bool ehEsq = (painelDuplo && painelAtivo == 0);
    int tItens = ehEsq ? totalItensEsq : totalItens;
    bool* mItems = ehEsq ? marcadosEsq : marcados;
    char (*nItems)[64] = ehEsq ? nomesEsq : nomes;
    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;
    int sAtual = ehEsq ? selEsq : sel;

    bool temMarcado = false;
    for (int i = 0; i < tItens; i++) if (mItems[i]) { temMarcado = true; break; }

    switch (op) {
    case 0: { // Nova Pasta com Teclado
        OrbisImeDialogSetting param;
        memset(&param, 0, sizeof(param));
        memset(textoTeclado, 0, sizeof(textoTeclado));

        param.maxTextLength = 255;
        param.inputTextBuffer = textoTeclado;
        param.title = L"Nome da Nova Pasta";
        param.type = (OrbisImeType)0;

        if (sceImeDialogInit(&param, NULL) >= 0) {
            esperandoNomePasta = true;
            showOpcoes = false;
        }
        break;
    }
    case 1: { // NOVO ARQUIVO
        inicializarNotepad();
        strcpy(pastaDestinoFinal, pExplorar);
        if (ehEsq) menuAtualEsq = MENU_NOTEPAD; else menuAtual = MENU_NOTEPAD;
        showOpcoes = false;
        break;
    }
    case 2: // Copiar
    case 3: { // Recortar
        clipboardCount = 0;
        clipboardIsCut = (op == 3);
        for (int i = 0; i < tItens; i++) {
            if (temMarcado ? mItems[i] : (i == sAtual)) {
                char* l = (nItems[i][0] == '[') ? &nItems[i][1] : nItems[i];
                sprintf(clipboardPaths[clipboardCount], "%s/%s", pExplorar, l);
                // Remove colchetes se for pasta
                if (nItems[i][0] == '[') clipboardPaths[clipboardCount][strlen(clipboardPaths[clipboardCount]) - 1] = '\0';
                clipboardCount++;
            }
        }
        sprintf(msgStatus, op == 2 ? "ARQUIVO(S) COPIADO(S)" : "ARQUIVO(S) RECORTADO(S)");
        msgTimer = 90;
        break;
    }
    case 4: { // Colar
        for (int i = 0; i < clipboardCount; i++) {
            char* baseName = strrchr(clipboardPaths[i], '/');
            if (baseName) baseName++; else baseName = clipboardPaths[i];
            char dest[512];
            sprintf(dest, "%s/%s", pExplorar, baseName);

            copiarArquivoReal(clipboardPaths[i], dest);

            if (clipboardIsCut) {
                unlink(clipboardPaths[i]); // Remove o original se foi "recortado"
            }
        }
        // Atualiza a listagem do painel ativo e limpa clipboard se foi recorte
        if (ehEsq) listarDiretorioEsq(pathExplorarEsq); else listarDiretorio(pathExplorar);
        if (clipboardIsCut) { clipboardCount = 0; clipboardIsCut = false; }

        sprintf(msgStatus, "ARQUIVO(S) COLADO(S)");
        msgTimer = 90;
        break;
    }
    case 5: { // Renomear
        int alvo = sAtual;
        for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }

        char* nomeReal = nItems[alvo];
        ehPastaParaRenomear = (nomeReal[0] == '[');

        char nomeLimpo[256];
        if (ehPastaParaRenomear) {
            strncpy(nomeLimpo, &nomeReal[1], strlen(nomeReal) - 2);
            nomeLimpo[strlen(nomeReal) - 2] = '\0';
        }
        else {
            strcpy(nomeLimpo, nomeReal);
        }

        sprintf(oldPathParaRenomear, "%s/%s", pExplorar, nomeLimpo);

        // Salva a extensão antiga se for arquivo
        memset(oldExtParaRenomear, 0, sizeof(oldExtParaRenomear));
        if (!ehPastaParaRenomear) {
            char* dot = strrchr(nomeLimpo, '.');
            if (dot) strcpy(oldExtParaRenomear, dot);
        }

        OrbisImeDialogSetting param;
        memset(&param, 0, sizeof(param));
        memset(textoTeclado, 0, sizeof(textoTeclado));

        // Escreve no teclado usando ponteiro de char normal (8 bits)
        char* bufWrite = (char*)textoTeclado;
        strncpy(bufWrite, nomeLimpo, 255);

        param.maxTextLength = 255;
        param.inputTextBuffer = textoTeclado;
        param.title = L"Renomear Arquivo/Pasta";
        param.type = (OrbisImeType)0;

        if (sceImeDialogInit(&param, NULL) >= 0) {
            esperandoRenomear = true;
            showOpcoes = false;
        }
        break;
    }
    case 6: { // Deletar
        for (int i = 0; i < tItens; i++) {
            if (temMarcado ? mItems[i] : (i == sAtual)) {
                char f[512]; char* l = (nItems[i][0] == '[') ? &nItems[i][1] : nItems[i];
                sprintf(f, "%s/%s", pExplorar, l);
                if (nItems[i][0] == '[') { f[strlen(f) - 1] = '\0'; rmdir(f); }
                else unlink(f);
            }
        }
        if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar);
        break;
    }
    case 8: { // Selecionar
        mItems[sAtual] = !mItems[sAtual];
        break;
    }
    case 9: { // Selecionar tudo
        bool ligar = false; for (int i = 0; i < tItens; i++) if (!mItems[i]) ligar = true;
        for (int i = 0; i < tItens; i++) mItems[i] = ligar;
        break;
    }
    }

    if (!esperandoNomePasta && !esperandoRenomear && menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD) {
        showOpcoes = false;
        msgTimer = 120;
    }
}

void atualizarImePasta() {
    if (!esperandoNomePasta && !esperandoRenomear) return;

    // Se o status for diferente de "Rodando" (1), o diálogo terminou
    int status = (int)sceImeDialogGetStatus();
    if (status != IME_STATUS_RUNNING && status != 0) {
        OrbisDialogResult res;
        memset(&res, 0, sizeof(res));
        sceImeDialogGetResult(&res);

        // Lemos o código do botão (Geralmente 0 é OK/Concluir)
        int32_t buttonId = *(int32_t*)&res;

        if (buttonId == 0) {
            char nomeFinal[256];
            memset(nomeFinal, 0, sizeof(nomeFinal));

            // A MÁGICA: O teclado devolve um char normal de 8 bits. Lemos diretamente!
            char* bufRead = (char*)textoTeclado;
            strncpy(nomeFinal, bufRead, 255);

            bool ehEsq = (painelDuplo && painelAtivo == 0);
            char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

            if (strlen(nomeFinal) > 0) {
                char nPath[512];

                if (esperandoNomePasta) {
                    sprintf(nPath, "%s/%s", pExplorar, nomeFinal);
                    sceKernelMkdir(nPath, 0777);
                    sprintf(msgStatus, "PASTA CRIADA!");
                }
                else if (esperandoRenomear) {
                    // Mantem a extensão antiga se o usuário não digitou um novo ponto "." e se é um arquivo
                    if (!ehPastaParaRenomear && strlen(oldExtParaRenomear) > 0) {
                        if (strrchr(nomeFinal, '.') == NULL) {
                            strcat(nomeFinal, oldExtParaRenomear);
                        }
                    }
                    sprintf(nPath, "%s/%s", pExplorar, nomeFinal);
                    rename(oldPathParaRenomear, nPath);
                    sprintf(msgStatus, "RENOMEADO COM SUCESSO!");
                }

                if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar);
            }
        }

        // Finaliza o diálogo e DESBLOQUEIA os botões
        sceImeDialogTerm();
        esperandoNomePasta = false;
        esperandoRenomear = false;
        msgTimer = 120;
    }
}