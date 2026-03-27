#ifndef JOGAR_H
#define JOGAR_H

// Variável que guarda o caminho do XML atual
extern char xmlCaminhoAtual[256];

// Funções de Jogos e Listas
void carregarXML(const char* path);

// A NOSSA NOVA FUNÇÃO MÁGICA DE LANÇAMENTO
void chamarJogo(const char* titleId, const char* romPath);

#endif