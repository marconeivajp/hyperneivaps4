#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

extern char msgStatus[128];
extern int msgTimer;
extern void atualizarBarra(float p);

static bool torrentAtivo = false;
static float progressoDownload = 0.0f;

// Estrutura para gerenciar os Trackers (A sua announce-list)
struct Tracker {
    char url[256];
    bool ativo;
};

void* motorP2PNativo(void* arg) {
    char* path = (char*)arg;
    torrentAtivo = true;
    
    // 1. Ligar Rede do PS4
    // sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);

    sprintf(msgStatus, "MOTOR P2P NEIVA INICIADO");
    msgTimer = 180;

    // TODO: Implementar o Handshake real com o Tracker
    // Por enquanto, vamos garantir que a UI responde perfeitamente
    // enquanto integramos os sockets de rede abaixo.

    for(int i=0; i<=100; i++) {
        if(!torrentAtivo) break;
        progressoDownload = i / 100.0f;
        
        sprintf(msgStatus, "BAIXANDO: %d%% | PEERS: 12", i);
        msgTimer = 60;
        
        atualizarBarra(progressoDownload);
        sceKernelUsleep(800000);
    }

    if(torrentAtivo) {
        sprintf(msgStatus, "DOWNLOAD COMPLETO!");
    } else {
        sprintf(msgStatus, "DOWNLOAD CANCELADO");
    }
    
    msgTimer = 240;
    torrentAtivo = false;
    free(path);
    return NULL;
}

extern "C" {
    void iniciarDownloadTorrent(const char* caminho) {
        if(torrentAtivo) return;
        pthread_t t;
        pthread_create(&t, NULL, motorP2PNativo, strdup(caminho));
        pthread_detach(t);
    }

    void pararDownloadTorrent() { torrentAtivo = false; }
    float obterProgressoTorrent() { return progressoDownload; }
}
