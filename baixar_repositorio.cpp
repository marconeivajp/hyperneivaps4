#include "baixar_repositorio.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

// Puxamos as variáveis globais que estas funções precisam ler/alterar
extern char nomes[3000][64];
extern int totalItens;
extern MenuLevel menuAtual;
extern int sel;
extern int off;

extern char caminhoXMLAtual[256];
extern char linksAtuais[3000][1024];
extern int totalLinksAtuais;

void preencherMenuRepositorios() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Games");
    totalItens = 1;
    menuAtual = MENU_BAIXAR_REPOS;
    sel = 0;
    off = 0;
}

void listarXMLsRepositorio() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    // Aponta para a nova pasta de repositórios em configuracao
    DIR* d = opendir("/data/HyperNeiva/configuracao/repositorios");
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
    // Aponta para a nova pasta de repositórios em configuracao
    sprintf(caminhoXMLAtual, "/data/HyperNeiva/configuracao/repositorios/%s", xmlFile);
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