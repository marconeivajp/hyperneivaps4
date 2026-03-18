#ifndef PASTAS_H
#define PASTAS_H

#include <stdint.h>

// 1. IDs de Menu isolados
typedef enum {
    M_ROOT,
    M_BAIXAR,
    M_RETROARCH,
    M_CONSOLES,
    M_EXPLORAR_HOME,
    M_MUSICAS,
    M_MUSICAS_LISTA
} MenuID;

// 2. Estrutura de Consoles (necessária para a lógica de baixar)
struct ConsoleLocal {
    const char* nome;
    const char* pathServidor;
};

// 3. O "Cérebro" do Menu (Tudo o que o pastas.cpp manipulava antes)
struct MenuContext {
    MenuID menuAtual;
    int totalItens;
    int sel;
    int off;
    char nomes[3000][64];
    char msgStatus[128];
    int msgTimer;
    char musicaAtualStatus[256]; // Para saber se exibe "RETOMAR" ou "PARAR"
};

// 4. Protótipos das suas funções (Agora recebendo o Contexto)
void preencherRoot(MenuContext* ctx);
void entrarMenuMusicas(MenuContext* ctx);
void listarMusicas(MenuContext* ctx);
void entrarMenuBaixar(MenuContext* ctx);
void entrarMenuRetroarch(MenuContext* ctx);
void listarConsolesBaixar(MenuContext* ctx);
void preencherExplorerHome(MenuContext* ctx);

#endif