#include "jogar.h"
#include "explorar.h" // Acesso às variáveis globais da interface (nomes, totalItens, menuAtual)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char xmlCaminhoAtual[256] = "";

void carregarXML(const char* path) {
    FILE* fp = fopen(path, "rb"); if (!fp) return;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    char* p = b;

    while (totalItens < 2000) {
        p = strstr(p, "<game name=\""); if (!p) break;
        p += 12; char* f = strchr(p, '\"');
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
    menuAtual = JOGAR_XML;
    strcpy(xmlCaminhoAtual, path);
}