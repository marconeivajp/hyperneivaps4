// Interface em C da libtorrent para o Hyper Neiva.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HnTorrentInfo {
    char nome[256];
    char erro[128];
    float progresso;
    long long baixado;
    long long total;
    int taxa;
    int fontes;
    int pausado;
    int concluido;
    int estado;
} HnTorrentInfo;

int hn_torrent_iniciar(const char* dados, const char* destino);
int hn_torrent_adicionar(const char* origem, char* erroTexto, int tamanhoErro);
void hn_torrent_atualizar(void);
int hn_torrent_total(void);
int hn_torrent_info(int indice, HnTorrentInfo* saida);
void hn_torrent_alternar_pausa(int indice);
void hn_torrent_remover(int indice, int apagarArquivos);
void hn_torrent_encerrar(void);

#ifdef __cplusplus
}
#endif
