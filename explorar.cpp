#include "explorar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/libkernel.h>

// Instanciamento das variáveis do Explorador
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

    // SORT (Pastas > Arquivos)
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
    case 0: { // Nova Pasta
        char nPath[512]; int tent = 0;
        sprintf(nPath, "%s/nova_pasta", pathExplorar);
        while (sceKernelMkdir(nPath, 0777) < 0 && tent < 10) {
            tent++; sprintf(nPath, "%s/nova_pasta_%d", pathExplorar, tent);
        }
        listarDiretorio(pathExplorar); break;
    }
    case 2: // Copiar
    case 3: // Recortar
        clipboardCount = 0; clipboardIsCut = (op == 3);
        for (int i = 0; i < totalItens; i++) {
            if (temMarcado ? marcados[i] : (i == sel)) {
                char* l = (nomes[i][0] == '[') ? &nomes[i][1] : nomes[i];
                sprintf(clipboardPaths[clipboardCount], "%s/%s", pathExplorar, l);
                if (nomes[i][0] == '[') clipboardPaths[clipboardCount][strlen(clipboardPaths[clipboardCount]) - 1] = '\0';
                clipboardCount++; if (clipboardCount >= 100) break;
            }
        }
        sprintf(msgStatus, "%d ITENS NO CLIPBOARD", clipboardCount); break;

    case 4: // Colar Real
        for (int i = 0; i < clipboardCount; i++) {
            char* fName = strrchr(clipboardPaths[i], '/');
            char dPath[512]; sprintf(dPath, "%s%s", pathExplorar, fName);
            copiarArquivoReal(clipboardPaths[i], dPath);
            if (clipboardIsCut) unlink(clipboardPaths[i]);
        }
        strcpy(msgStatus, "COLADO COM SUCESSO"); clipboardCount = 0; listarDiretorio(pathExplorar); break;

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
    showOpcoes = false; msgTimer = 120;
}