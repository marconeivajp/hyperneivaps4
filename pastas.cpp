#include "globals.h"

// --- 1. DEFINIÇÃO DA LISTA DE CONSOLES (Servidor Libretro) ---
// Note: Esta lista é usada pelo network.cpp para montar a URL de download
Console listaConsolesLocal[5] = {
    {"Sony - PlayStation", "Sony%20-%20PlayStation"},
    {"Sony - PlayStation Portable", "Sony%20-%20PlayStation%20Portable"},
    {"Nintendo - Super Nintendo Entertainment System", "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System"},
    {"Sega - Mega Drive - Genesis", "Sega%20-%20Mega%20Drive%20-%20Genesis"},
    {"Nintendo - Nintendo Entertainment System", "Nintendo%20-%20Nintendo%20Entertainment%20System"}
};

// --- 2. FUNÇÕES DE PREENCHIMENTO DE MENU ---

// Menu Principal do Hyper Neiva
void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "JOGAR");
    strcpy(nomes[1], "BAIXAR");
    strcpy(nomes[2], "EDITAR");
    strcpy(nomes[3], "EXPLORAR");
    totalItens = 4;
    menuAtual = ROOT;
    sel = 0; off = 0; // Reseta o cursor para o topo
}

// Menu Inicial do Scraper (Baixar)
void entrarMenuBaixar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "CAPAS");
    totalItens = 1;
    menuAtual = MENU_BAIXAR;
    sel = 0; off = 0;
}

// Sub-menu de Categorias
void entrarMenuRetroarch() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "RETROARCH");
    totalItens = 1;
    menuAtual = MENU_CAPAS;
    sel = 0; off = 0;
}

// Lista de Consoles para o Scraper
void listarConsolesBaixar() {
    memset(nomes, 0, sizeof(nomes));
    for (int i = 0; i < 5; i++) {
        strcpy(nomes[i], listaConsolesLocal[i].nome);
    }
    totalItens = 5;
    menuAtual = MENU_CONSOLES;
    sel = 0; off = 0;
}

// Menu de Entrada do Explorador (Seleção de Unidade)
void preencherExplorerHome() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva (/data)");
    strcpy(nomes[1], "Raiz (/)");
    strcpy(nomes[2], "USB0 (/mnt/usb0)");
    strcpy(nomes[3], "USB1 (/mnt/usb1)");
    totalItens = 4;
    menuAtual = MENU_EXPLORAR_HOME;
    sel = 0; off = 0;
}