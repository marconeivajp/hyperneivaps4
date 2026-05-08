#ifndef TORRENT_H
#define TORRENT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicia o processamento e download do torrent em segundo plano
void iniciarDownloadTorrent(const char* caminhoTorrent);

// Cancela o download ativo
void pararDownloadTorrent();

// Retorna o progresso (0.0 a 1.0)
float obterProgressoTorrent();

#ifdef __cplusplus
}
#endif

#endif