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
#include <ctype.h>

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef SO_NOSIGPIPE
#define SO_NOSIGPIPE 0x0800 
#endif

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
#include "miniz.h" 

extern void preencherOpcoesContexto(const char* nomeArquivo);
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;

extern const char* listaOpcoes[10];
extern int mapOpcoes[10];
extern int totalOpcoes;

extern int cd;
extern void preencherExplorerHome();
extern void preencherRoot();
extern void atualizarBarra(float progresso);

extern float zoomMidia;
extern bool fullscreenMidia;
extern char caminhoNavegacaoMusicas[512];
static char caminhoMusicaTocando[512] = "";

extern int offEsq;
extern int off;

char caminhoImagemAberta[512] = "";

extern unsigned char* imgPreview;
extern int wP, hP, cP;

void carregarPreviewArquivo(const char* caminhoAbsoluto) {
    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }

    char tempPathAbs[512];
    strcpy(tempPathAbs, caminhoAbsoluto);
    for (int i = 0; tempPathAbs[i]; i++) {
        tempPathAbs[i] = tolower(tempPathAbs[i]);
    }

    if (strstr(tempPathAbs, ".png") || strstr(tempPathAbs, ".jpg") || strstr(tempPathAbs, ".jpeg") || strstr(tempPathAbs, ".bmp")) {
        imgPreview = stbi_load(caminhoAbsoluto, &wP, &hP, &cP, 4);
    }
    else if (strstr(tempPathAbs, ".xavatar")) {
        mz_zip_archive zip_archive; memset(&zip_archive, 0, sizeof(zip_archive));
        if (mz_zip_reader_init_file(&zip_archive, caminhoAbsoluto, 0)) {
            const char* tempPath = "/data/HyperNeiva/configuracao/temporario/temp_preview.png";
            bool extraiu = false;
            mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
            for (mz_uint i = 0; i < num_files; i++) {
                mz_zip_archive_file_stat file_stat;
                if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
                if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;

                char tempExtZip[256]; strcpy(tempExtZip, file_stat.m_filename);
                for (int k = 0; tempExtZip[k]; k++) tempExtZip[k] = tolower(tempExtZip[k]);

                if (strstr(tempExtZip, ".png") || strstr(tempExtZip, ".jpg") || strstr(tempExtZip, ".jpeg")) {
                    if (mz_zip_reader_extract_to_file(&zip_archive, i, tempPath, 0)) { extraiu = true; break; }
                }
            }
            if (extraiu) imgPreview = stbi_load(tempPath, &wP, &hP, &cP, 4);
            mz_zip_reader_end(&zip_archive);
        }
    }
}

char caminhoPkgAtual[512] = "";
bool servidorRodando = false;

void* handle_client(void* arg) {
    int client_fd = *(int*)arg; free(arg); int set = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
    char buffer_req[2048]; memset(buffer_req, 0, sizeof(buffer_req));
    recv(client_fd, buffer_req, sizeof(buffer_req) - 1, 0);

    if (strlen(caminhoPkgAtual) > 0 && strlen(buffer_req) > 0) {
        bool is_head_request = (strncmp(buffer_req, "HEAD", 4) == 0);
        FILE* f = fopen(caminhoPkgAtual, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); size_t fsize = ftell(f); fseek(f, 0, SEEK_SET);
            size_t start_range = 0; char* range_str = strstr(buffer_req, "Range: bytes=");
            if (range_str) sscanf(range_str, "Range: bytes=%zu-", &start_range);
            char header[512];
            if (start_range > 0) {
                sprintf(header, "HTTP/1.1 206 Partial Content\r\nContent-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %zu-%zu/%zu\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", start_range, fsize - 1, fsize, fsize - start_range);
                fseek(f, start_range, SEEK_SET);
            }
            else {
                sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", fsize);
            }
            send(client_fd, header, strlen(header), 0);
            if (!is_head_request) {
                size_t bytes_read; char* file_buffer = (char*)malloc(65536);
                while ((bytes_read = fread(file_buffer, 1, 65536, f)) > 0) { if (send(client_fd, file_buffer, bytes_read, 0) < 0) break; }
                free(file_buffer);
            } fclose(f);
        }
        else {
            char* not_found = (char*)"HTTP/1.1 404 Not Found\r\n\r\n"; send(client_fd, not_found, strlen(not_found), 0);
        }
    } close(client_fd); return NULL;
}

void* threadServidorHTTP(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); addr.sin_port = htons(8080);
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return NULL; listen(server_fd, 10);
    while (1) { int client_fd = accept(server_fd, NULL, NULL); if (client_fd < 0) continue; int* pclient = (int*)malloc(sizeof(int)); *pclient = client_fd; pthread_t tid; pthread_create(&tid, NULL, handle_client, pclient); pthread_detach(tid); }
    return NULL;
}

void ligarServidorSeNecessario() {
    if (!servidorRodando) { pthread_t tid; pthread_create(&tid, NULL, threadServidorHTTP, NULL); servidorRodando = true; }
}

void instalarPkgLocal(const char* caminhoAbsoluto) {
    sprintf(msgStatus, "ANALISANDO FILA DE DOWNLOADS..."); atualizarBarra(0.2f);
    strcpy(caminhoPkgAtual, caminhoAbsoluto); ligarServidorSeNecessario();
    char contentId[40]; memset(contentId, 0, sizeof(contentId)); uint32_t fileSize = 0; FILE* f = fopen(caminhoAbsoluto, "rb");
    if (f) { fseek(f, 0x40, SEEK_SET); fread(contentId, 1, 36, f); fseek(f, 0, SEEK_END); fileSize = (uint32_t)ftell(f); fclose(f); }
    else { strcpy(contentId, "UP0000-000000000_00-0000000000000000"); }
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL); sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
    static void* bgftHeap = NULL;
    if (!bgftHeap) { sceAppInstUtilInitialize(); OrbisBgftInitParams bgftInit; memset(&bgftInit, 0, sizeof(OrbisBgftInitParams)); bgftInit.heapSize = 2 * 1024 * 1024; bgftHeap = memalign(4096, bgftInit.heapSize); bgftInit.heap = bgftHeap; sceBgftServiceIntInit(&bgftInit); }
    OrbisBgftTaskId tarefaAntiga = -1; sceBgftServiceIntDownloadFindActiveTask(contentId, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga); if (tarefaAntiga != -1) { sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga); }
    int32_t userId = 0; sceUserServiceGetInitialUser(&userId);
    char urlPkg[1024]; sprintf(urlPkg, "http://127.0.0.1:8080/%s.pkg", contentId);
    OrbisBgftDownloadParam params; memset(&params, 0, sizeof(OrbisBgftDownloadParam)); params.userId = userId; params.entitlementType = 5; params.id = contentId; params.contentUrl = urlPkg; params.contentName = "Hyper Neiva - Instalando PKG"; params.playgoScenarioId = "0"; params.packageSize = fileSize;
    OrbisBgftTaskId taskId = -1; int res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);
    if (res == 0 && taskId >= 0) { sceBgftServiceDownloadStartTask(taskId); sprintf(msgStatus, "DOWNLOAD INICIADO! Verifique as Notificacoes."); atualizarBarra(1.0f); }
    else { sprintf(msgStatus, "ERRO: 0x%08X", res); atualizarBarra(0.0f); }
    msgTimer = 400;
}

void acaoL2_Explorar() { if (visualizandoMidiaImagem) return; painelDuplo = !painelDuplo; if (painelDuplo) { painelAtivo = 0; menuAtualEsq = MENU_EXPLORAR_HOME; selEsq = 0; } else { painelAtivo = 1; } }
void alternarPainelAtivo() { if (visualizandoMidiaImagem) return; if (painelDuplo && !showOpcoes) painelAtivo = (painelAtivo == 0) ? 1 : 0; }

void acaoCross_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0);
    MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;

    if ((mAtual == MENU_EXPLORAR || visualizandoMidiaImagem) && showOpcoes) { acaoArquivo(selOpcao); return; }
    if (visualizandoMidiaImagem) { fullscreenMidia = !fullscreenMidia; return; }

    int sAtual = ehEsq ? selEsq : sel; char (*nItems)[64] = ehEsq ? nomesEsq : nomes; char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

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
            char caminhoArquivo[512]; sprintf(caminhoArquivo, "%s/%s", pExplorar, nItems[sAtual]);

            char nomeBlindado[256]; strcpy(nomeBlindado, nItems[sAtual]);
            for (int i = 0; nomeBlindado[i]; i++) {
                nomeBlindado[i] = tolower(nomeBlindado[i]);
            }

            if (strstr(nomeBlindado, ".pkg")) {
                instalarPkgLocal(caminhoArquivo);
            }
            else if (strstr(nomeBlindado, ".zip")) {
                listaOpcoes[0] = "extrair zip";
                mapOpcoes[0] = 7;
                for (int k = 1; k < 10; k++) { listaOpcoes[k] = ""; mapOpcoes[k] = -1; }
                totalOpcoes = 1; showOpcoes = true; selOpcao = 0;
            }
            // ========================================================
            // MENU DO X: AS 4 OPÇÕES! (Visualizar, Usar, Fundos...)
            // ========================================================
            else if (strstr(nomeBlindado, ".png") || strstr(nomeBlindado, ".jpg") || strstr(nomeBlindado, ".jpeg") || strstr(nomeBlindado, ".bmp") || strstr(nomeBlindado, ".xavatar")) {
                strcpy(caminhoImagemAberta, caminhoArquivo);

                listaOpcoes[0] = "visualizar";
                if (strstr(nomeBlindado, ".xavatar")) mapOpcoes[0] = 13; else mapOpcoes[0] = 14;

                listaOpcoes[1] = "usar no perfil ps4";
                mapOpcoes[1] = 12;
                listaOpcoes[2] = "plano de fundo do ps4";
                mapOpcoes[2] = 11;
                listaOpcoes[3] = "plano de fundo hyper neiva";
                mapOpcoes[3] = 10;

                if (strstr(nomeBlindado, ".xavatar")) {
                    listaOpcoes[4] = "extrair zip / avatar";
                    mapOpcoes[4] = 7;
                    for (int k = 5; k < 10; k++) { listaOpcoes[k] = ""; mapOpcoes[k] = -1; }
                    totalOpcoes = 5;
                }
                else {
                    for (int k = 4; k < 10; k++) { listaOpcoes[k] = ""; mapOpcoes[k] = -1; }
                    totalOpcoes = 4;
                }

                showOpcoes = true;
                selOpcao = 0;
            }
            else if (strstr(nomeBlindado, ".mp3") || strstr(nomeBlindado, ".wav")) {
                if (strcmp(caminhoMusicaTocando, caminhoArquivo) == 0) { tocarMusicaNova("PARADO"); strcpy(caminhoMusicaTocando, ""); sprintf(msgStatus, "Musica Parada"); msgTimer = 90; }
                else { tocarMusicaNova(caminhoArquivo); strcpy(caminhoMusicaTocando, caminhoArquivo); sprintf(msgStatus, "Reproduzindo Audio"); msgTimer = 90; }
            }
            else if (strstr(nomeBlindado, ".txt") || strstr(nomeBlindado, ".xml") || strstr(nomeBlindado, ".json") || strstr(nomeBlindado, ".ini") || strstr(nomeBlindado, ".cfg") || strstr(nomeBlindado, ".log")) {
                editarArquivoExistente(pExplorar, nItems[sAtual]);
                if (ehEsq) menuAtualEsq = MENU_NOTEPAD; else menuAtual = MENU_NOTEPAD;
            }
        }
    }
}

void acaoCircle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    if (showOpcoes) { showOpcoes = false; return; }
    if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } return; }
    bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual; char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;
    if (mAtual == MENU_EXPLORAR_HOME) { if (painelDuplo) { painelDuplo = false; painelAtivo = 1; } if (!ehEsq) preencherRoot(); }
    else if (mAtual == MENU_EXPLORAR) { if (strcmp(pExplorar, baseRaiz) == 0 || strcmp(pExplorar, "/") == 0 || strcmp(pExplorar, "/mnt/usb0") == 0 || strcmp(pExplorar, "/mnt/usb1") == 0) { if (ehEsq) menuAtualEsq = MENU_EXPLORAR_HOME; else preencherExplorerHome(); } else { char temp[256]; strcpy(temp, pExplorar); char* last = strrchr(temp, '/'); if (last) { if (last == temp) strcpy(temp, "/"); else *last = '\0'; if (ehEsq) listarDiretorioEsq(temp); else listarDiretorio(temp); } } }
}

void acaoTriangle_Explorar() {
    if (esperandoNomePasta || esperandoRenomear) return;
    bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual;

    if (mAtual == MENU_EXPLORAR) {
        if (!showOpcoes) {
            int sAtual = ehEsq ? selEsq : sel; char (*nItems)[64] = ehEsq ? nomesEsq : nomes;
            preencherOpcoesContexto(nItems[sAtual]);
        } showOpcoes = !showOpcoes; selOpcao = 0;
    }
}

void acaoR1_Explorar() { if (esperandoNomePasta || esperandoRenomear) return; if (visualizandoMidiaImagem) return; bool ehEsq = (painelDuplo && painelAtivo == 0); MenuLevel mAtual = ehEsq ? menuAtualEsq : menuAtual; int sAtual = ehEsq ? selEsq : sel; bool* mItems = ehEsq ? marcadosEsq : marcados; if (mAtual == MENU_EXPLORAR) { if (cd <= 0) { mItems[sAtual] = !mItems[sAtual]; cd = 12; } } }