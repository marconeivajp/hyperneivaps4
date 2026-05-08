#include "menu.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>

MenuLevel menuAtual = ROOT;
char nomes[3000][64];
int totalItens = 0;
int sel = 0;
int off = 0;
int offEsq = 0;

char msgStatus[128] = "SISTEMA PRONTO";
int msgTimer = 0;

char caminhoMidiaAtual[512] = "/data/HyperNeiva/midia";
char caminhoROMsAtual[512] = ""; // Novo: rastreia a pasta de ROMs ativa

void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "JOGAR"); // <--- Agora agrupa PS4 Nativo e XML
    strcpy(nomes[1], "MIDIA");
    strcpy(nomes[2], "BAIXAR");
    strcpy(nomes[3], "EDITAR");
    strcpy(nomes[4], "EXPLORAR");
    strcpy(nomes[5], "EXTRA");
    strcpy(nomes[6], "INFORMACOES");
    totalItens = 7;
    menuAtual = ROOT;
}

void preencherMenuJogar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "PS4 NATIVO");
    strcpy(nomes[1], "JOGOS XML");
    totalItens = 2;
    menuAtual = MENU_TIPO_JOGO;
}

void preencherExplorerHome() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva");
    strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB0");
    strcpy(nomes[3], "USB1");
    totalItens = 4;
    menuAtual = MENU_EXPLORAR_HOME;
}

void abrirPastaMidia(const char* caminho) {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    strcpy(caminhoMidiaAtual, caminho);

    DIR* d = opendir(caminhoMidiaAtual);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                strcpy(nomes[totalItens], dir->d_name);
                totalItens++;
            }
        }
        closedir(d);
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Radio & Podcast");
        totalItens = 1;
    } else {
        // Shifting for Radio
        for (int i = totalItens; i > 0; i--) {
            strcpy(nomes[i], nomes[i-1]);
        }
        strcpy(nomes[0], "Radio & Podcast");
        totalItens++;
    }

    menuAtual = MENU_MIDIA;
    sel = 0;
    off = 0;
}

void preencherMenuGBA() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    const char* romPath = "/data/retroarch/Games/gba";
    strcpy(caminhoROMsAtual, romPath); // Registra que estamos no GBA
    DIR* d = opendir(romPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && totalItens < 3000) {
            if (dir->d_type == DT_REG) {
                const char* ext = strrchr(dir->d_name, '.');
                if (ext && (strcasecmp(ext, ".gba") == 0 || strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".7z") == 0)) {
                    strncpy(nomes[totalItens], dir->d_name, 63);
                    nomes[totalItens][63] = '\0';
                    totalItens++;
                }
            }
        }
        closedir(d);
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhuma ROM GBA encontrada");
        totalItens = 1;
    }

    menuAtual = MENU_EMULADOR; // Reaproveita o menu de lista de ROMs
    sel = 0;
    off = 0;
}

void preencherMenuMidia() {
    abrirPastaMidia("/data/HyperNeiva/midia");
}

void preencherMenuEmulador() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    const char* romPath = "/data/retroarch/Games/roms Genesis";
    strcpy(caminhoROMsAtual, romPath); // Registra que estamos no Genesis
    DIR* d = opendir(romPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && totalItens < 3000) {
            if (dir->d_type == DT_REG) {
                const char* ext = strrchr(dir->d_name, '.');
                if (ext && (strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".bin") == 0 || strcasecmp(ext, ".gen") == 0 || strcasecmp(ext, ".smd") == 0 || strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".rar") == 0 || strcasecmp(ext, ".7z") == 0)) {
                    strncpy(nomes[totalItens], dir->d_name, 63);
                    nomes[totalItens][63] = '\0';
                    totalItens++;
                }
            }
        }
        closedir(d);
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhuma ROM encontrada");
        totalItens = 1;
    }

    menuAtual = MENU_EMULADOR;
    sel = 0;
    off = 0;
}