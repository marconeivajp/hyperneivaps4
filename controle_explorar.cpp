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

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x20000 
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
// SERVIDOR HTTP INTERNO (Com Resposta 206 Rigorosa para o PS4)
// ====================================================================
char caminhoPkgAtual[512] = "";
bool servidorRodando = false;

void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char buffer_req[2048];
    memset(buffer_req, 0, sizeof(buffer_req));
    int bytes_recvd = recv(client_fd, buffer_req, sizeof(buffer_req) - 1, 0);

    if (bytes_recvd > 0 && strlen(caminhoPkgAtual) > 0) {

        bool is_head_request = (strncmp(buffer_req, "HEAD", 4) == 0);

        FILE* f = fopen(caminhoPkgAtual, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            size_t start_range = 0;
            size_t end_range = fsize - 1;
            bool has_range = false;

            // Lendo EXATAMENTE o que o PS4 pede
            char* range_str = strstr(buffer_req, "Range: bytes=");
            if (range_str) {
                has_range = true;
                if (sscanf(range_str, "Range: bytes=%zu-%zu", &start_range, &end_range) == 1) {
                    end_range = fsize - 1; // Se não tiver final, vai até o fim
                }
            }

            if (end_range >= fsize) end_range = fsize - 1;

            size_t content_length = (end_range - start_range) + 1;
            char header[1024];

            // O PULO DO GATO: Sempre manda 206 se o PS4 pediu Range (Mesmo que seja do zero)
            if (has_range) {
                sprintf(header, "HTTP/1.1 206 Partial Content\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Range: bytes %zu-%zu/%zu\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n\r\n",
                    start_range, end_range, fsize, content_length);
            }
            else {
                sprintf(header, "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n\r\n", fsize);
            }

            send(client_fd, header, strlen(header), MSG_NOSIGNAL);

            if (!is_head_request) {
                fseek(f, start_range, SEEK_SET);

                size_t bytes_to_send = content_length;
                char* file_buffer = (char*)malloc(65536);

                while (bytes_to_send > 0) {
                    size_t chunk_size = (bytes_to_send > 65536) ? 65536 : bytes_to_send;
                    size_t bytes_read = fread(file_buffer, 1, chunk_size, f);

                    if (bytes_read <= 0) break;

                    char* p = file_buffer;
                    size_t to_send = bytes_read;
                    while (to_send > 0) {
                        ssize_t sent = send(client_fd, p, to_send, MSG_NOSIGNAL);
                        if (sent <= 0) break;
                        p += sent;
                        to_send -= sent;
                    }
                    if (to_send > 0) break; // PS4 fechou a conexão

                    bytes_to_send -= bytes_read;
                }
                free(file_buffer);
            }
            fclose(f);
        }
        else {
            char* not_found = (char*)"HTTP/1.1 404 Not Found\r\n\r\n";
            send(client_fd, not_found, strlen(not_found), MSG_NOSIGNAL);
        }
    }
    close(client_fd);
    return NULL;
}

void* threadServidorHTTP(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(8080);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return NULL;
    listen(server_fd, 10);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

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
// INSTALAÇÃO NATIVA (BGFT)
// ====================================================================
static char s_contentId[40];
static char s_urlPkg[256];
static char s_contentName[128];

void instalarPkgLocal(const char* caminhoAbsoluto) {
    sprintf(msgStatus, "REGISTRANDO TAREFA...");
    atualizarBarra(0.2f);

    strcpy(caminhoPkgAtual, caminhoAbsoluto);
    ligarServidorSeNecessario();

    memset(s_contentId, 0, sizeof(s_contentId));
    uint32_t fileSize = 0;
    FILE* f = fopen(caminhoAbsoluto, "rb");
    if (f) {
        fseek(f, 0x40, SEEK_SET);
        fread(s_contentId, 1, 36, f);
        fseek(f, 0, SEEK_END);
        fileSize = (uint32_t)ftell(f);
        fclose(f);
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

    OrbisBgftTaskId tarefaAntiga = -1;
    sceBgftServiceIntDownloadFindActiveTask(s_contentId, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga);
    if (tarefaAntiga != -1) sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga);

    int32_t userId = 0;
    sceUserServiceGetInitialUser(&userId);

    sprintf(s_urlPkg, "http://127.0.0.1:8080/download.pkg");
    sprintf(s_contentName, "Hyper Neiva - Instalando Jogo");

    OrbisBgftDownloadParam params;
    memset(&params, 0, sizeof(OrbisBgftDownloadParam));
    params.userId = userId;
    params.entitlementType = 5;
    params.id = s_contentId;
    params.contentUrl = s_urlPkg;
    params.contentName = s_contentName;
    params.packageSize = fileSize;
    params.playgoScenarioId = "0";

    OrbisBgftTaskId taskId = -1;
    int res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);

    if (res == 0 && taskId >= 0) {
        sceBgftServiceDownloadStartTask(taskId);
        sprintf(msgStatus, "INSTALACAO INICIADA! Acompanhe as Notificacoes.");
        atualizarBarra(1.0f);
    }
    else {
        sprintf(msgStatus, "ERRO: 0x%08X", res);
        atualizarBarra(0.0f);
    }
    msgTimer = 400;
}
// ====================================================================

// RESTO DA INTERFACE MANTIDA IGUAL
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