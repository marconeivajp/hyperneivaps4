#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "explorar.h" // Necessário para o tipo MenuLevel

// --- DEFINIÇÕES DE FALLBACK PARA MENUS ---
// Garante que estas IDs de menu sejam visíveis em todo o projeto
#ifndef MENU_MUSICAS
#define MENU_MUSICAS ((MenuLevel)11)
#endif
#ifndef MENU_AUDIO_OPCOES
#define MENU_AUDIO_OPCOES ((MenuLevel)12)
#endif
#ifndef MENU_NOTEPAD
#define MENU_NOTEPAD ((MenuLevel)13)
#endif

// --- VARIÁVEIS GLOBAIS (EXTERN) ---
// Estas variáveis são definidas no menu.cpp e acessadas pelo main e controle
extern MenuLevel menuAtual;
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern int off;

// Mensagens de Status do Sistema
extern char msgStatus[128];
extern int msgTimer;

// --- PROTÓTIPOS DAS FUNÇÕES ---
// Funções responsáveis por alterar o estado dos itens exibidos na tela
void preencherRoot();
void preencherExplorerHome();

#endif