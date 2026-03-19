// --- INÍCIO DO ARQUIVO controle_root.cpp ---
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h> // ADICIONADO PARA VERIFICAR DIRETÓRIOS

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

#include "controle_root.h"
#include "menu.h"
#include "stb_image.h"

extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern char nomes[3000][64];
extern char ultimoJogoCarregado[64];
extern unsigned char* imgPreview;
extern char bufferTecladoC[128];
extern char caminhoXMLAtual[256];

extern char msgStatus[128];
extern int msgTimer;
extern char caminhoMidiaAtual[512]; // VARIÁVEL IMPORTADA

extern void carregarXML(const char* path);
extern void preencherMenuBaixar();
extern void preencherMenuEditar();
extern void preencherExplorerHome();
extern void preencherMenuMusicas();
extern void preencherRoot();
extern void preencherMenuMidia();
extern void abrirPastaMidia(const char* caminho); // FUNÇÃO IMPORTADA

void acaoCross_Root() {
    if (menuAtual == ROOT) {
        if (sel == 0) carregarXML("/app0/assets/lista.xml");
        else if (sel == 1) { preencherMenuMidia(); sel = 0; off = 0; }
        else if (sel == 2) { preencherMenuBaixar(); sel = 0; off = 0; }
        else if (sel == 3) { preencherMenuEditar(); sel = 0; off = 0; }
        else if (sel == 4) { preencherExplorerHome(); sel = 0; off = 0; }
        else if (sel == 5) {
            if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
            strcpy(ultimoJogoCarregado, ""); preencherMenuMusicas(); sel = 0; off = 0;
        }
        else if (sel == 6) {
            menuAtual = MENU_NOTEPAD;
            memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
        }
    }
    else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) {
        carregarXML("/app0/assets/sp.xml");
    }
    else if (menuAtual == MENU_MIDIA) { // <-- ENTRAR NAS PASTAS AQUI
        if (strcmp(nomes[sel], "Pasta vazia") == 0) return;

        char novoCaminho[512];
        sprintf(novoCaminho, "%s/%s", caminhoMidiaAtual, nomes[sel]);

        // Tenta abrir o diretório. Se o PS4 conseguir abrir, é uma pasta!
        DIR* chk = opendir(novoCaminho);
        if (chk) {
            closedir(chk);
            abrirPastaMidia(novoCaminho); // Entra na subpasta
        }
        else {
            // Se falhou ao abrir como diretório, é um arquivo (mp4, txt, etc).
            strcpy(msgStatus, "ARQUIVO SELECIONADO");
            msgTimer = 120;
        }
    }
}

void acaoCircle_Root() {
    if (menuAtual == JOGAR_XML) {
        if (strstr(caminhoXMLAtual, "sp.xml")) carregarXML("/app0/assets/lista.xml");
        else preencherRoot();
    }
    else if (menuAtual == MENU_MIDIA) { // <-- VOLTAR NAS PASTAS AQUI
        if (strcmp(caminhoMidiaAtual, "/data/HyperNeiva/midia") == 0) {
            preencherRoot(); // Se estava na raiz, volta pro menu principal
        }
        else {
            // Localiza a última barra e corta a string nela (simula o cd ..)
            char* ultimaBarra = strrchr(caminhoMidiaAtual, '/');
            if (ultimaBarra != NULL) {
                *ultimaBarra = '\0';
            }
            abrirPastaMidia(caminhoMidiaAtual); // Lê a pasta de trás
        }
    }
}
// --- FIM DO ARQUIVO controle_root.cpp ---