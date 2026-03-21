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

// Definições manuais para garantir compilação caso o header esteja incompleto
#define IME_STATUS_RUNNING 1
#define IME_STATUS_FINISHED 2

// Instanciamento das variáveis
char pathExplorar[256] = "";
char baseRaiz[256] = "";
bool marcados[3000];
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

// Variáveis do Teclado
bool esperandoNomePasta = false;
wchar_t textoTeclado[64] = L"";

void copiarArquivoReal(const char* origem, const char* destino) {
    FILE* src = fopen(origem, "rb"); if (!src) return;
    FILE* dst = fopen(destino, "wb"); if (!dst) { fclose(src); return; }
    char buffer[65536]; size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    fclose(src); fclose(dst);
}

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
}

void acaoArquivo(int op) {
    bool temMarcado = false;
    for (int i = 0; i < totalItens; i++) if (marcados[i]) { temMarcado = true; break; }

    switch (op) {
    case 0: { // Nova Pasta com Teclado
        OrbisImeDialogSetting param;
        memset(&param, 0, sizeof(param));
        memset(textoTeclado, 0, sizeof(textoTeclado));

        param.maxTextLength = 63;
        param.inputTextBuffer = textoTeclado;
        param.title = L"Nome da Nova Pasta";
        param.type = (OrbisImeType)0; // Default

        if (sceImeDialogInit(&param, (OrbisImeSettingsExtended*)0) >= 0) {
            esperandoNomePasta = true;
            showOpcoes = false;
        }
        break;
    }
    case 6: // Deletar
        for (int i = 0; i < totalItens; i++) {
            if (temMarcado ? marcados[i] : (i == sel)) {
                char f[512]; char* l = (nomes[i][0] == '[') ? &nomes[i][1] : nomes[i];
                sprintf(f, "%s/%s", pathExplorar, l);
                if (nomes[i][0] == '[') { f[strlen(f) - 1] = '\0'; rmdir(f); }
                else unlink(f);
            }
        }
        listarDiretorio(pathExplorar); break;
    case 8: marcados[sel] = !marcados[sel]; break;
    case 9: {
        bool ligar = false; for (int i = 0; i < totalItens; i++) if (!marcados[i]) ligar = true;
        for (int i = 0; i < totalItens; i++) marcados[i] = ligar; break;
    }
    }
    if (!esperandoNomePasta) { showOpcoes = false; msgTimer = 120; }
}

void atualizarImePasta() {
    if (!esperandoNomePasta) return;

    // Se o status for diferente de "Rodando" (1), o diálogo terminou
    int status = (int)sceImeDialogGetStatus();
    if (status != IME_STATUS_RUNNING && status != 0) {
        OrbisDialogResult res;
        memset(&res, 0, sizeof(res));
        sceImeDialogGetResult(&res);

        // Lemos o código do botão (Geralmente 0 é OK/Concluir)
        int32_t buttonId = *(int32_t*)&res;

        if (buttonId == 0) {
            char nomeFinal[64];
            // Converte de wchar_t para char
            for (int i = 0; i < 63; i++) nomeFinal[i] = (char)textoTeclado[i];
            nomeFinal[63] = '\0';

            if (strlen(nomeFinal) > 0) {
                char nPath[512];
                sprintf(nPath, "%s/%s", pathExplorar, nomeFinal);
                sceKernelMkdir(nPath, 0777);
                listarDiretorio(pathExplorar);
                sprintf(msgStatus, "PASTA CRIADA!");
            }
        }

        // Finaliza o diálogo e DESBLOQUEIA os botões
        sceImeDialogTerm();
        esperandoNomePasta = false;
        msgTimer = 120;
    }
}