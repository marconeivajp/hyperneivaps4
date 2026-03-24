#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

// --- BIBLIOTECAS PARA O SERVIDOR HTTP INTERNO ---
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Proteção Anti-Crash do FreeBSD/Orbis OS
#ifndef SO_NOSIGPIPE
#define SO_NOSIGPIPE 0x0800 
#endif

// --- BIBLIOTECAS PARA INSTALAÇÃO NATIVA (BGFT) ---
#include <orbis/Sysmodule.h>
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/UserService.h>

#include "controle_explorar.h"
#include "menu.h"
#include "explorar.h"
#include "stb_image.h" 
#include "audio.h"   
#include "bloco_de_notas.h"

extern void preencherOpcoesContexto(const char* nomeArquivo);
extern int cd;
extern void preencherExplorerHome();
extern void preencherRoot();
extern void atualizarBarra(float progresso);
extern char msgStatus[128];
extern int msgTimer;
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;
extern char caminhoNavegacaoMusicas[512];
static char caminhoMusicaTocando[512] = "";

// ====================================================================
// SERVIDOR HTTP INTERNO (O SEU CÓDIGO, MAS MULTITHREAD E COM CLOSE)
// ====================================================================
char caminhoPkgAtual[512] = "";
bool servidorRodando = false;

// Função que atende um pedido sem travar os outros
void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    // O SEGREDO DO ANTI-CRASH (Evita o CE-34878-0)
    int set = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));

    char buffer_req[2048];
    memset(buffer_req, 0, sizeof(buffer_req));
    recv(client_fd, buffer_req, sizeof(buffer_req) - 1, 0);

    if (strlen(caminhoPkgAtual) > 0 && strlen(buffer_req) > 0) {

        // O SEGREDO DO ERRO CE-37732-2: Verificar se é HEAD ou GET
        bool is_head_request = (strncmp(buffer_req, "HEAD", 4) == 0);

        FILE* f = fopen(caminhoPkgAtual, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            size_t start_range = 0;
            char* range_str = strstr(buffer_req, "Range: bytes=");
            if (range_str) sscanf(range_str, "Range: bytes=%zu-", &start_range);

            char header[512];
            if (start_range > 0) {
                // TROCADO KEEP-ALIVE POR CLOSE
                sprintf(header, "HTTP/1.1 206 Partial Content\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Range: bytes %zu-%zu/%zu\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n\r\n",
                    start_range, fsize - 1, fsize, fsize - start_range);
                fseek(f, start_range, SEEK_SET);
            }
            else {
                // TROCADO KEEP-ALIVE POR CLOSE
                sprintf(header, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n\r\n", fsize);
            }

            // Envia os cabeçalhos
            send(client_fd, header, strlen(header), 0);

            // SE FOR 'GET', NÓS ENVIAMOS O JOGO. SE FOR 'HEAD', ENCERRAMOS AQUI!
            if (!is_head_request) {
                size_t bytes_read;
                char* file_buffer = (char*)malloc(65536);
                while ((bytes_read = fread(file_buffer, 1, 65536, f)) > 0) {
                    if (send(client_fd, file_buffer, bytes_read, 0) < 0) {
                        break; // Se o PS4 abortar, saímos silenciosamente sem crashar!
                    }
                }
                free(file_buffer);
            }
            fclose(f);
        }
        else {
            char* not_found = (char*)"HTTP/1.1 404 Not Found\r\n\r\n";
            send(client_fd, not_found, strlen(not_found), 0);
        }
    }
    close(client_fd); // Fecha a conexão limpinha
    return NULL;
}

void* threadServidorHTTP(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    addr.sin_port = htons(8080); // Porta 8080

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return NULL;
    listen(server_fd, 10);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        // Distribui para a thread e volta a escutar a porta imediatamente
        int* pclient = (int*)malloc(sizeof(int));
        *pclient = client_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }
    return NULL;
}

void ligarServidorSeNecessario() {
    if (!servidorRodando) {
        pthread_t tid;
        pthread_create(&tid, NULL, threadServidorHTTP, NULL);
        servidorRodando = true;
    }
}

// ====================================================================
// MÉTODO DE INSTALAÇÃO NATIVA (BGFT) - EXATAMENTE O SEU CÓDIGO
// ====================================================================
void instalarPkgLocal(const char* caminhoAbsoluto) {
    sprintf(msgStatus, "ANALISANDO FILA DE DOWNLOADS...");
    atualizarBarra(0.2f);

    strcpy(caminhoPkgAtual, caminhoAbsoluto);
    ligarServidorSeNecessario();

    char contentId[40];
    memset(contentId, 0, sizeof(contentId));
    uint32_t fileSize = 0;
    FILE* f = fopen(caminhoAbsoluto, "rb");
    if (f) {
        fseek(f, 0x40, SEEK_SET);
        fread(contentId, 1, 36, f);
        fseek(f, 0, SEEK_END);
        fileSize = (uint32_t)ftell(f);
        fclose(f);
    }
    else {
        strcpy(contentId, "UP0000-000000000_00-0000000000000000");
    }

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);

    static void* bgftHeap = NULL;
    if (!bgftHeap) {
        sceAppInstUtilInitialize();
        OrbisBgftInitParams bgftInit;
        memset(&bgftInit, 0, sizeof(OrbisBgftInitParams));
        bgftInit.heapSize = 2 * 1024 * 1024;
        bgftHeap = memalign(4096, bgftInit.heapSize);
        bgftInit.heap = bgftHeap;
        sceBgftServiceIntInit(&bgftInit);
    }

    // Limpa tarefas antigas travadas
    OrbisBgftTaskId tarefaAntiga = -1;
    sceBgftServiceIntDownloadFindActiveTask(contentId, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga);
    if (tarefaAntiga != -1) {
        sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga);
    }

    int32_t userId = 0;
    sceUserServiceGetInitialUser(&userId);

    // O Link do nosso servidor agora aponta certinho
    char urlPkg[1024];
    sprintf(urlPkg, "http://127.0.0.1:8080/%s.pkg", contentId);

    OrbisBgftDownloadParam params;
    memset(&params, 0, sizeof(OrbisBgftDownloadParam));
    params.userId = userId;
    params.entitlementType = 5;
    params.id = contentId;
    params.contentUrl = urlPkg;
    params.contentName = "Hyper Neiva - Instalando PKG";
    params.playgoScenarioId = "0";
    params.packageSize = fileSize;

    OrbisBgftTaskId taskId = -1;
    int res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);

    if (res == 0 && taskId >= 0) {
        sceBgftServiceDownloadStartTask(taskId);
        sprintf(msgStatus, "DOWNLOAD INICIADO! Verifique as Notificacoes.");
        atualizarBarra(1.0f);
    }
    else {
        sprintf(msgStatus, "ERRO: 0x%08X", res);
        atualizarBarra(0.0f);
    }
    msgTimer = 400;
}
// ====================================================================

// RESTANTE DAS FUNÇÕES (acaoL2, acaoCross, etc) CONTINUAM IGUAIS...
void acaoL2_Explorar() { if (visualizandoMidiaImagem) return; painelDuplo = !painelDuplo; if (painelDuplo) { painelAtivo = 0; menuAtualEsq = MENU_EXPLORAR_HOME; selEsq = 0; } else { painelAtivo = 1; } }
void alternarPainelAtivo() { if (visualizandoMidiaImagem) return; if (painelDuplo && !showOpcoes) painelAtivo = (painelAtivo == 0) ? 1 : 0; }
void acaoCross_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    if (visualizandoMidiaImagem) { fullscreenMidia = !fullscreenMidia; return; }
    bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual; int sAtual = ehEsq ? selEsq : sel; char (*nItems)[64] = ehEsq ? nomesEsq : nomes; char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;
    if (mAtual == MENU_EXPLORAR && showOpcoes) { acaoArquivo(selOpcao); return; }
    if (mAtual == MENU_EXPLORAR_HOME) {
        char tempBase[256];
        if (sAtual == 0) strcpy(tempBase, "/data/HyperNeiva"); else if (sAtual == 1) strcpy(tempBase, "/"); else if (sAtual == 2) strcpy(tempBase, "/mnt/usb0"); else if (sAtual == 3) strcpy(tempBase, "/mnt/usb1");
        if (ehEsq) listarDiretorioEsq(tempBase); else { strcpy(baseRaiz, tempBase); listarDiretorio(baseRaiz); }
    }
    else if (mAtual == MENU_EXPLORAR) {
        if (nItems[sAtual][0] == '[') {
            char pL[128]; strncpy(pL, &nItems[sAtual][1], strlen(nItems[sAtual]) - 2); pL[strlen(nItems[sAtual]) - 2] = '\0';
            char nP[256]; sprintf(nP, "%s/%s", pExplorar, pL);
            if (ehEsq) listarDiretorioEsq(nP); else listarDiretorio(nP);
        }
        else {
            char caminhoArquivo[512]; sprintf(caminhoArquivo, "%s/%s", pExplorar, nItems[sAtual]); char* ext = strrchr(nItems[sAtual], '.');
            if (ext) {
                if (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0) instalarPkgLocal(caminhoArquivo);
                else if (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".ZIP") == 0) { preencherOpcoesContexto(nItems[sAtual]); showOpcoes = true; selOpcao = 0; }
                else if (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0) { if (strcmp(caminhoMusicaTocando, caminhoArquivo) == 0) { tocarMusicaNova("PARADO"); strcpy(caminhoMusicaTocando, ""); sprintf(msgStatus, "Musica Parada"); msgTimer = 90; } else { tocarMusicaNova(caminhoArquivo); strcpy(caminhoMusicaTocando, caminhoArquivo); sprintf(msgStatus, "Reproduzindo Audio"); msgTimer = 90; } }
                else if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".bmp") == 0) { if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } int canais; imgMidia = stbi_load(caminhoArquivo, &wM, &hM, &canais, 4); if (imgMidia) { visualizandoMidiaImagem = true; zoomMidia = 1.0f; fullscreenMidia = false; } else { sprintf(msgStatus, "ERRO AO CARREGAR IMAGEM"); msgTimer = 90; } }
                else if (strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".xml") == 0 || strcasecmp(ext, ".json") == 0 || strcasecmp(ext, ".ini") == 0 || strcasecmp(ext, ".cfg") == 0 || strcasecmp(ext, ".log") == 0) { editarArquivoExistente(pExplorar, nItems[sAtual]); if (ehEsq) menuAtualEsq = MENU_NOTEPAD; else menuAtual = MENU_NOTEPAD; }
            }
        }
    }
}
void acaoCircle_Explorar() { if (esperandoNomePasta || esperandoRenomear) return; if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } return; } bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual; char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar; if (mAtual == MENU_EXPLORAR_HOME) { if (painelDuplo) { painelDuplo = false; painelAtivo = 1; } if (!ehEsq) preencherRoot(); } else if (mAtual == MENU_EXPLORAR) { if (strcmp(pExplorar, baseRaiz) == 0 || strcmp(pExplorar, "/") == 0 || strcmp(pExplorar, "/mnt/usb0") == 0 || strcmp(pExplorar, "/mnt/usb1") == 0) { if (ehEsq) menuAtualEsq = MENU_EXPLORAR_HOME; else preencherExplorerHome(); } else { char temp[256]; strcpy(temp, pExplorar); char* last = strrchr(temp, '/'); if (last) { if (last == temp) strcpy(temp, "/"); else *last = '\0'; if (ehEsq) listarDiretorioEsq(temp); else listarDiretorio(temp); } } } }
void acaoTriangle_Explorar() { if (esperandoNomePasta || esperandoRenomear) return; if (visualizandoMidiaImagem) return; bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual; if (mAtual == MENU_EXPLORAR) { if (!showOpcoes) { int sAtual = ehEsq ? selEsq : sel; char (*nItems)[64] = ehEsq ? nomesEsq : nomes; preencherOpcoesContexto(nItems[sAtual]); } showOpcoes = !showOpcoes; selOpcao = 0; } }
void acaoR1_Explorar() { if (esperandoNomePasta || esperandoRenomear) return; if (visualizandoMidiaImagem) return; bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual; int sAtual = ehEsq ? selEsq : sel; bool* mItems = ehEsq ? marcadosEsq : marcados; if (mAtual == MENU_EXPLORAR) { if (cd <= 0) { mItems[sAtual] = !mItems[sAtual]; cd = 12; } } }