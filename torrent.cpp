#include "torrent.h"

extern "C" {
    #include "bitfiend.h"
}

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

extern void atualizarBarra(float progresso);
extern char msgStatus[128];
extern int msgTimer;

static bool torrentRodando = false;
static float progressoAtual = 0.0f;
static pthread_t threadTorrent;

void* threadDownloadTorrentNativo(void* arg) {
    char* caminhoTorrent = (char*)arg;
    
    char pastaDestino[256] = "/data/HyperNeiva/downloads";
    char caminhoLog[256] = "/data/HyperNeiva/downloads/bitfiend.log";
    sceKernelMkdir(pastaDestino, 0777);

    // Liga a rede nativa do PS4
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);

    torrentRodando = true;
    progressoAtual = 0.0f;

    if (bitfiend_init(caminhoLog) != BITFIEND_SUCCESS) {
        sprintf(msgStatus, "ERRO: FALHA AO LIGAR MOTOR P2P!");
        msgTimer = 240;
        torrentRodando = false;
        free(caminhoTorrent);
        return NULL;
    }

    bf_htorrent_t* t = bitfiend_add_torrent(caminhoTorrent, pastaDestino);
    if (!t) {
        sprintf(msgStatus, "ERRO AO LER O ARQUIVO .TORRENT");
        msgTimer = 240;
        bitfiend_shutdown();
        torrentRodando = false;
        free(caminhoTorrent);
        return NULL;
    }

    bf_stat_t stats;
    bool concluido = false;

    // Loop Real com Feedback Visual
    while (torrentRodando && !concluido) {
        if (bitfiend_stat_torrent(t, &stats) == BITFIEND_SUCCESS) {
            
            // Verifica o total de peças lidas da internet
            if (stats.tot_pieces > 0) {
                progressoAtual = (float)(stats.tot_pieces - stats.pieces_left) / (float)stats.tot_pieces;
                if (stats.pieces_left == 0) {
                    concluido = true;
                }
            }
            
            // ATUALIZA A MENSAGEM NA TELA EM TEMPO REAL!
            if (!concluido) {
                float mbBaixados = (float)stats.tot_downloaded / (1024.0f * 1024.0f);
                sprintf(msgStatus, "BAIXANDO: %.2f MB | PECAS: %u/%u", mbBaixados, (stats.tot_pieces - stats.pieces_left), stats.tot_pieces);
                msgTimer = 120; // Mantém a mensagem viva
            }
        }
        
        atualizarBarra(progressoAtual);
        
        // Espera 1 segundo para a próxima atualização de ecrã
        sceKernelUsleep(1000000); 
    }

    bitfiend_remove_torrent(t);
    bitfiend_shutdown();

    if (concluido) {
        sprintf(msgStatus, "DOWNLOAD P2P CONCLUIDO!");
        atualizarBarra(1.0f);
    } else {
        sprintf(msgStatus, "DOWNLOAD INTERROMPIDO!");
    }
    
    msgTimer = 300;
    torrentRodando = false;
    free(caminhoTorrent);
    return NULL;
}

void iniciarDownloadTorrent(const char* caminhoTorrent) {
    if (torrentRodando) return; // Evita abrir dois ao mesmo tempo
    char* caminhoCopy = strdup(caminhoTorrent);
    if (pthread_create(&threadTorrent, NULL, threadDownloadTorrentNativo, caminhoCopy) != 0) {
        free(caminhoCopy);
    } else {
        pthread_detach(threadTorrent); 
    }
}

void pararDownloadTorrent() {
    torrentRodando = false; 
}

float obterProgressoTorrent() {
    return progressoAtual;
}