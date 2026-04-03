#include "baixar_repositorio.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <orbis/libkernel.h> 

extern char nomes[3000][64];
extern int totalItens;
extern MenuLevel menuAtual;
extern int sel;
extern int off;

extern char caminhoXMLAtual[256];
extern char linksAtuais[3000][1024];
extern int totalLinksAtuais;

char pastaRepositorioAtual[128] = "";

void preencherMenuRepositorios() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Games");
    strcpy(nomes[1], "Imagens para perfil");
    strcpy(nomes[2], "XML Games");
    totalItens = 3;
    menuAtual = MENU_BAIXAR_REPOS;
    sel = 0;
    off = 0;
}

void extrairXMLRepositorioUnico(const char* pasta, const char* arquivoXML) {
    char dirConfig[512];
    sprintf(dirConfig, "/data/HyperNeiva/configuracao/repositorios/%s", pasta);

    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios", 0777);
    sceKernelMkdir(dirConfig, 0777);

    char pathConfig[512];
    sprintf(pathConfig, "%s/%s", dirConfig, arquivoXML);

    FILE* fp = fopen(pathConfig, "rb");
    if (!fp) {
        char pathApp0[512];
        sprintf(pathApp0, "/app0/assets/%s", arquivoXML);
        FILE* fApp0 = fopen(pathApp0, "rb");
        if (fApp0) {
            fseek(fApp0, 0, SEEK_END); long sz = ftell(fApp0); fseek(fApp0, 0, SEEK_SET);
            char* bTemp = (char*)malloc(sz);
            fread(bTemp, 1, sz, fApp0);
            fclose(fApp0);

            FILE* fSave = fopen(pathConfig, "wb");
            if (fSave) {
                fwrite(bTemp, 1, sz, fSave);
                fclose(fSave);
            }
            free(bTemp);
        }
    }
    else {
        fclose(fp);
    }
}

void listarXMLsRepositorio() {
    // ATENÇÃO: Só atualizamos a "pasta atual" se viermos do menu principal (onde sel = 0, 1 ou 2)!
    // Isso evita que o botão Bolinha (Voltar) use a variável errada se "sel" mudar nas outras telas!
    if (menuAtual == MENU_BAIXAR_REPOS) {
        if (sel == 0) strcpy(pastaRepositorioAtual, "games");
        else if (sel == 1) strcpy(pastaRepositorioAtual, "imagens para perfil");
        else if (sel == 2) strcpy(pastaRepositorioAtual, "xml games");
    }

    // Extrai os arquivos caso a pasta esteja vazia (Primeira vez que abre)
    if (strcmp(pastaRepositorioAtual, "games") == 0) {
        extrairXMLRepositorioUnico("games", "systemas+zipados.xml");
        extrairXMLRepositorioUnico("games", "Sega_Master_System.xml"); // EXTRAI O MASTER SYSTEM AQUI TAMBÉM!
    }
    else if (strcmp(pastaRepositorioAtual, "imagens para perfil") == 0) {
        extrairXMLRepositorioUnico("imagens para perfil", "xavatar.xml");
    }
    else if (strcmp(pastaRepositorioAtual, "xml games") == 0) {
        extrairXMLRepositorioUnico("xml games", "xml.xml");
    }

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    char dirPath[512];
    sprintf(dirPath, "/data/HyperNeiva/configuracao/repositorios/%s", pastaRepositorioAtual);

    DIR* d = opendir(dirPath);
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
    sprintf(caminhoXMLAtual, "/data/HyperNeiva/configuracao/repositorios/%s/%s", pastaRepositorioAtual, xmlFile);

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