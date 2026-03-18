#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "explorar.h" // Importante para o tipo MenuLevel

// Fallbacks de Menu
#ifndef MENU_MUSICAS
#define MENU_MUSICAS ((MenuLevel)11)
#endif
#ifndef MENU_AUDIO_OPCOES
#define MENU_AUDIO_OPCOES ((MenuLevel)12)
#endif
#ifndef MENU_NOTEPAD
#define MENU_NOTEPAD ((MenuLevel)13)
#endif

// Variáveis Globais (extern)
extern MenuLevel menuAtual;
extern char nomes[3000][64];
extern int totalItens, sel, off;
extern char msgStatus[128];
extern int msgTimer;

void preencherRoot();
void preencherExplorerHome();

#endif