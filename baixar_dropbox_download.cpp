#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h> 

// --- NOVAS BIBLIOTECAS PARA O GERENCIADOR NATIVO (BGFT) ---
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/UserService.h>

#include "baixar_dropbox_download.h"
#include "menu.h"
#include "network.h"
#include <stdint.h>
#include <dirent.h>

extern "C" int sceHttpSetRequestContentLength(int reqId, uint64_t contentLength);
extern void atualizarBarra(float progresso);

extern char nomes[3000][64];
extern char linksAtuais[3000][1024];
extern int totalItens;
extern MenuLevel menuAtual;
extern int sel;
extern int off;

extern char currentDropboxPath[512];
extern char currentUploadPath[512];
extern char msgStatus[128];
extern int msgTimer;
extern bool marcados[3000];
extern char caminhoXMLAtual[256];

// ==============================================================
// VARIÁVEIS GLOBAIS E CONTROLES DA FILA EM ARQUIVO TXT
// ==============================================================
volatile bool downloadEmSegundoPlano = false;
volatile float progressoAtualDownload = 0.0f;
char msgDownloadBg[256] = "";

volatile int totalFilaSessao = 0;
volatile int baixadosFilaSessao = 0;
volatile bool processandoFila = false;
volatile bool processandoFilaUpload = false;

pthread_mutex_t filaMutex = PTHREAD_MUTEX_INITIALIZER;
const char* caminhoFila = "/data/HyperNeiva/configuracao/temporario/fila_downloads.txt";
const char* caminhoFilaTemp = "/data/HyperNeiva/configuracao/temporario/fila_temp.txt";

int obterTamanhoFila() {
    pthread_mutex_lock(&filaMutex);
    int linhas = 0;
    FILE* f = fopen(caminhoFila, "r");
    if (f) {
        char linha[2048];
        while (fgets(linha, sizeof(linha), f)) linhas++;
        fclose(f);
    }
    pthread_mutex_unlock(&filaMutex);
    return linhas;
}

void adicionarNaFila(const char* url, MenuLevel menuOrigem, const char* xmlOrigem) {
    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
    pthread_mutex_lock(&filaMutex);

    bool existe = false;
    FILE* fCheck = fopen(caminhoFila, "r");
    if (fCheck) {
        char linha[2048];
        while (fgets(linha, sizeof(linha), fCheck)) {
            for (int i = 0; linha[i]; i++) if (linha[i] == '\r' || linha[i] == '\n') linha[i] = '\0';
            char* p1 = strchr(linha, '|');
            if (p1) *p1 = '\0';

            if (strcmp(linha, url) == 0) {
                existe = true;
                break;
            }
        }
        fclose(fCheck);
    }

    if (!existe) {
        FILE* f = fopen(caminhoFila, "a");
        if (f) {
            fprintf(f, "%s|%d|%s\n", url, (int)menuOrigem, xmlOrigem);
            fclose(f);
            totalFilaSessao++;
        }
    }
    pthread_mutex_unlock(&filaMutex);
}

bool obterPrimeiroDaFila(char* urlOut, MenuLevel* menuOut, char* xmlOut) {
    pthread_mutex_lock(&filaMutex);
    FILE* f = fopen(caminhoFila, "r");
    if (!f) { pthread_mutex_unlock(&filaMutex); return false; }

    char linha[2048];
    if (fgets(linha, sizeof(linha), f)) {
        for (int i = 0; linha[i]; i++) if (linha[i] == '\r' || linha[i] == '\n') linha[i] = '\0';

        char* p1 = strchr(linha, '|');
        if (p1) {
            *p1 = '\0';
            strcpy(urlOut, linha);
            p1++;
            char* p2 = strchr(p1, '|');
            if (p2) {
                *p2 = '\0';
                *menuOut = (MenuLevel)atoi(p1);
                p2++;
                strcpy(xmlOut, p2);
            }
            else {
                *menuOut = (MenuLevel)atoi(p1);
                strcpy(xmlOut, "");
            }
        }
        else {
            strcpy(urlOut, linha);
            *menuOut = (MenuLevel)0;
            strcpy(xmlOut, "");
        }
        fclose(f);
        pthread_mutex_unlock(&filaMutex);
        return true;
    }
    fclose(f);
    pthread_mutex_unlock(&filaMutex);
    return false;
}

void removerPrimeiroDaFila() {
    pthread_mutex_lock(&filaMutex);
    FILE* f = fopen(caminhoFila, "r");
    if (!f) { pthread_mutex_unlock(&filaMutex); return; }

    FILE* fTemp = fopen(caminhoFilaTemp, "w");
    if (fTemp) {
        char linha[2048];
        bool primeiro = true;
        while (fgets(linha, sizeof(linha), f)) {
            if (primeiro) { primeiro = false; continue; }
            fprintf(fTemp, "%s", linha);
        }
        fclose(fTemp);
    }
    fclose(f);

    unlink(caminhoFila);
    rename(caminhoFilaTemp, caminhoFila);
    pthread_mutex_unlock(&filaMutex);
}

void preencherMenuBackup() {
    memset(nomes, 0, sizeof(nomes));

    strcpy(nomes[0], "Hyper Neiva");
    strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB0");
    strcpy(nomes[3], "USB1");
    strcpy(nomes[4], "Backup Automatico (Saves, App.db)");

    totalItens = 5;
    menuAtual = MENU_BAIXAR_DROPBOX_BACKUP;
    sel = 0;
    off = 0;
}

void acessarDropbox(const char* path) {
    char cleanPath[256]; strncpy(cleanPath, path, 255); cleanPath[255] = '\0';
    for (int i = 0; i < strlen(cleanPath); i++) { if (cleanPath[i] == '\r' || cleanPath[i] == '\n') cleanPath[i] = '\0'; }
    strcpy(currentDropboxPath, cleanPath);

    char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken);
    }
    for (int i = 0; i < strlen(token); i++) { if (token[i] == '\r' || token[i] == '\n') token[i] = '\0'; }

    if (strlen(token) < 15) {
        sprintf(msgStatus, "ERRO: TOKEN NO TXT INVALIDO");
        memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "Verifique o arquivo .txt");
        totalItens = 1; menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0; return;
    }

    sprintf(msgStatus, "CONECTANDO A API..."); atualizarBarra(0.3f); msgTimer = 180;
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/2/files/list_folder";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char postData[512]; memset(postData, 0, sizeof(postData)); sprintf(postData, "{\"path\": \"%s\"}", cleanPath);
    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);

    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    sceHttpAddRequestHeader(req, "Content-Type", "application/json; charset=utf-8", 1);
    sceHttpSetRequestContentLength(req, strlen(postData));

    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;
    atualizarBarra(0.6f);

    if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
        sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
        FILE* f = fopen("/data/HyperNeiva/configuracao/temporario/temp_dropbox.json", "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f); fclose(f);
            FILE* f2 = fopen("/data/HyperNeiva/configuracao/temporario/temp_dropbox.json", "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                char* h = (char*)malloc(sz + 1);
                if (h) {
                    fread(h, 1, sz, f2); h[sz] = '\0';
                    char* errSummary = strstr(h, "\"error_summary\": \"");
                    if (errSummary) {
                        errSummary += 18; char* errEnd = strchr(errSummary, '\"');
                        if (errEnd) {
                            int errLen = errEnd - errSummary; if (errLen > 100) errLen = 100;
                            char errMsg[128]; strncpy(errMsg, errSummary, errLen); errMsg[errLen] = '\0';
                            sprintf(msgStatus, "API ERRO: %s", errMsg);
                        }
                        strcpy(nomes[0], "Erro na API"); totalItens = 1;
                    }
                    else {
                        char* p = h;
                        while ((p = strstr(p, "\".tag\": \"")) && totalItens < 2900) {
                            p += 9; bool isFolder = (strncmp(p, "folder", 6) == 0);
                            char* namePtr = strstr(p, "\"name\": \""); if (!namePtr) break; namePtr += 9;
                            char* nameEnd = strchr(namePtr, '\"');
                            char* pathPtr = strstr(nameEnd, "\"path_display\": \""); if (!pathPtr) break; pathPtr += 17;
                            char* pathEnd = strchr(pathPtr, '\"');
                            if (nameEnd && pathEnd) {
                                int nameLen = nameEnd - namePtr; int pathLen = pathEnd - pathPtr;
                                strncpy(nomes[totalItens], namePtr, nameLen); nomes[totalItens][nameLen] = '\0';
                                strncpy(linksAtuais[totalItens], pathPtr, pathLen);
                                if (isFolder) { linksAtuais[totalItens][pathLen] = '/'; linksAtuais[totalItens][pathLen + 1] = '\0'; }
                                else { linksAtuais[totalItens][pathLen] = '\0'; }
                                totalItens++;
                            } p = pathEnd;
                        }
                        if (totalItens > 0) sprintf(msgStatus, "PASTAS CARREGADAS!");
                        else { strcpy(nomes[0], "Pasta vazia"); totalItens = 1; }
                    } free(h);
                } fclose(f2);
            }
        }
    }
    else sprintf(msgStatus, "ERRO: FALHA DE REDE");

    atualizarBarra(1.0f); msgTimer = 240; sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0;
}

// ====================================================================
// A MÁGICA: APLICA A MESMA LÓGICA DO EXPLORAR PRA LER O ID E JOGA PRO PS4
// ====================================================================
void instalarPkgRemoto(const char* urlDireta, const char* nomeExibicao) {
    sprintf(msgStatus, "PREPARANDO INSTALACAO DIRETA...");
    atualizarBarra(0.1f);

    char urlFinal[2048];
    uint64_t fileSize = 0;
    char contentId[40];
    memset(contentId, 0, sizeof(contentId));

    // 1. GERAR LINK DO DROPBOX OU REPOSITÓRIO E PEGAR TAMANHO
    if (urlDireta[0] == '/') {
        char token[2048] = { 0 };
        FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
        if (fToken) {
            fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
            if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; }
            fclose(fToken);
        }
        for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';

        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
        sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);

        const char* apiUrl = "https://api.dropboxapi.com/2/files/get_temporary_link";
        int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
        int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

        char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
        sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
        sceHttpAddRequestHeader(req, "Content-Type", "application/json", 1);

        char postData[1024]; sprintf(postData, "{\"path\": \"%s\"}", urlDireta);
        sceHttpSetRequestContentLength(req, strlen(postData));

        if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
            unsigned char buf[32768];
            int n = sceHttpReadData(req, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                char* linkPtr = strstr((char*)buf, "\"link\": \"");
                char* sizePtr = strstr((char*)buf, "\"size\": ");
                if (linkPtr && sizePtr) {
                    linkPtr += 9;
                    char* linkEnd = strchr(linkPtr, '\"');
                    if (linkEnd) {
                        int linkLen = linkEnd - linkPtr; strncpy(urlFinal, linkPtr, linkLen); urlFinal[linkLen] = '\0';
                        char urlClean[2048] = { 0 }; int j = 0;
                        for (int i = 0; i < strlen(urlFinal); i++) {
                            if (urlFinal[i] == '\\' && urlFinal[i + 1] == '/') { urlClean[j++] = '/'; i++; }
                            else { urlClean[j++] = urlFinal[i]; }
                        }
                        strcpy(urlFinal, urlClean);
                    }
                    sizePtr += 8; sscanf(sizePtr, "%lu", &fileSize);
                }
            }
        }
        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    }
    else {
        strcpy(urlFinal, urlDireta);
        char* dbox = strstr(urlFinal, "dropbox.com");
        if (dbox) { char* dl0 = strstr(urlFinal, "?dl=0"); if (dl0) dl0[4] = '1'; }

        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
        sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
        int conn = sceHttpCreateConnectionWithURL(tpl, urlFinal, 1);
        int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_HEAD, urlFinal, 0);
        if (sceHttpSendRequest(req, NULL, 0) >= 0) {
            size_t tamanhoTotal = 0; int32_t statusRes = 0;
            if (sceHttpGetResponseContentLength(req, &statusRes, &tamanhoTotal) == 0) fileSize = (uint64_t)tamanhoTotal;
        }
        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    }

    if (strlen(urlFinal) < 10 || fileSize == 0) {
        sprintf(msgStatus, "ERRO AO OBTER LINK DO JOGO!");
        atualizarBarra(0.0f); msgTimer = 240; return;
    }

    // 2. BAIXAR UM PEDAÇO DE 1KB E APLICAR A LÓGICA DO EXPLORAR!
    sprintf(msgStatus, "APLICANDO LOGICA DO EXPLORAR (EXTRAINDO ID)...");
    atualizarBarra(0.5f);

    int tplId = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tplId, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    int connId = sceHttpCreateConnectionWithURL(tplId, urlFinal, 1);
    int reqId = sceHttpCreateRequestWithURL(connId, ORBIS_METHOD_GET, urlFinal, 0);
    sceHttpAddRequestHeader(reqId, "Range", "bytes=0-1024", 1); // Pede só o comecinho

    if (sceHttpSendRequest(reqId, NULL, 0) >= 0) {
        sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
        FILE* fTemp = fopen("/data/HyperNeiva/configuracao/temporario/temp_id.bin", "wb");
        if (fTemp) {
            unsigned char buf[1024];
            int n = sceHttpReadData(reqId, buf, sizeof(buf));
            if (n > 0) fwrite(buf, 1, n, fTemp);
            fclose(fTemp);
        }
    }
    sceHttpDeleteRequest(reqId); sceHttpDeleteConnection(connId); sceHttpDeleteTemplate(tplId);

    // =========================================================
    // AQUI ENTRA A LÓGICA IDÊNTICA AO CONTROLE_EXPLORAR.CPP!
    // Usamos o fopen para ler o arquivo como se fosse o HDD local
    // =========================================================
    FILE* f = fopen("/data/HyperNeiva/configuracao/temporario/temp_id.bin", "rb");
    if (f) {
        fseek(f, 0x40, SEEK_SET);
        fread(contentId, 1, 36, f);
        fclose(f);
    }
    else {
        strcpy(contentId, "UP0000-000000000_00-0000000000000000"); // Fallback
    }
    contentId[36] = '\0';

    // 3. REGISTRAR DIRETO NO BGFT DA SONY
    sprintf(msgStatus, "REGISTRANDO NA SONY (ID: %s)...", contentId);
    atualizarBarra(0.8f);

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);

    static void* bgftHeapWeb = NULL;
    if (!bgftHeapWeb) {
        sceAppInstUtilInitialize();
        OrbisBgftInitParams bgftInit;
        memset(&bgftInit, 0, sizeof(OrbisBgftInitParams));
        bgftInit.heapSize = 2 * 1024 * 1024;
        bgftHeapWeb = memalign(4096, bgftInit.heapSize);
        bgftInit.heap = bgftHeapWeb;
        sceBgftServiceIntInit(&bgftInit);
    }

    OrbisBgftTaskId tarefaAntiga = -1;
    sceBgftServiceIntDownloadFindActiveTask(contentId, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga);
    if (tarefaAntiga != -1) sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga);

    int32_t userId = 0;
    sceUserServiceGetInitialUser(&userId);

    OrbisBgftDownloadParam params;
    memset(&params, 0, sizeof(OrbisBgftDownloadParam));
    params.userId = userId;
    params.entitlementType = 5;
    params.id = contentId;
    params.contentUrl = urlFinal; // Vai direto pra Sony sem servidor interno!
    params.contentName = nomeExibicao;
    params.playgoScenarioId = "0";
    params.packageSize = fileSize;

    OrbisBgftTaskId taskId = -1;
    int res = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);

    if (res == 0 && taskId >= 0) {
        sceBgftServiceDownloadStartTask(taskId);
        sprintf(msgStatus, "DOWNLOAD DO DROPBOX INICIADO NO PS4!");
        atualizarBarra(1.0f);
    }
    else {
        sprintf(msgStatus, "ERRO NO BGFT: 0x%08X", res);
        atualizarBarra(0.0f);
    }

    msgTimer = 400;
}
// ====================================================================

// O MOTOR DE DOWNLOAD EM SEGUNDO PLANO (MANTIDO INTACTO PARA OUTROS ARQUIVOS)
void* threadDownloadBackground(void* arg) {
    char urlDownloadBg[1024];
    MenuLevel menuOrigemBg;
    char caminhoXMLOrigemBg[256];

    while (obterPrimeiroDaFila(urlDownloadBg, &menuOrigemBg, caminhoXMLOrigemBg)) {

        char nomeArquivo[128] = "arquivo.bin";
        char* ref = strrchr(urlDownloadBg, '/');
        if (ref) { strncpy(nomeArquivo, ref + 1, 127); nomeArquivo[127] = '\0'; }

        char* ptrInterrogacao = strchr(nomeArquivo, '?'); if (ptrInterrogacao) *ptrInterrogacao = '\0';
        char* ptrHash = strchr(nomeArquivo, '#'); if (ptrHash) *ptrHash = '\0';

        char nomeLimpo[128] = { 0 }; int j = 0;
        for (int i = 0; nomeArquivo[i] != '\0' && j < 127; i++) {
            if (nomeArquivo[i] == '%' && nomeArquivo[i + 1] == '2' && nomeArquivo[i + 2] == '0') { nomeLimpo[j++] = ' '; i += 2; }
            else { nomeLimpo[j++] = nomeArquivo[i]; }
        }
        strcpy(nomeArquivo, nomeLimpo);

        char pathPasta[512];
        sceKernelMkdir("/data/HyperNeiva/baixado", 0777);

        if (menuOrigemBg == MENU_BAIXAR_DROPBOX_LISTA || menuOrigemBg == MENU_BAIXAR_DROPBOX_BACKUP) {
            sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777);
            sprintf(pathPasta, "/data/HyperNeiva/baixado/dropbox");
        }
        else if (menuOrigemBg == MENU_BAIXAR_LINKS) {
            char nomeRepo[128] = "Games";
            char* refBarra = strrchr(caminhoXMLOrigemBg, '/');
            if (refBarra) {
                strncpy(nomeRepo, refBarra + 1, 127);
                char* dot = strrchr(nomeRepo, '.');
                if (dot) *dot = '\0';
            }
            sceKernelMkdir("/data/HyperNeiva/baixado/repositorios", 0777);
            sceKernelMkdir("/data/HyperNeiva/baixado/repositorios/games", 0777);
            sprintf(pathPasta, "/data/HyperNeiva/baixado/repositorios/games/%s", nomeRepo);
            sceKernelMkdir(pathPasta, 0777);
        }
        else {
            sceKernelMkdir("/data/HyperNeiva/baixado/linkdireto", 0777);
            sprintf(pathPasta, "/data/HyperNeiva/baixado/linkdireto");
        }

        char pathFinal[1024];
        sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);

        progressoAtualDownload = 0.0f;
        strcpy(msgDownloadBg, "CONECTANDO...");

        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
        sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect(); int conn, req;

        if (urlDownloadBg[0] == '/') {
            char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
            if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
            for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';

            const char* apiUrl = "https://content.dropboxapi.com/2/files/download";
            conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);
            char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
            sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
            char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", urlDownloadBg);
            sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);
        }
        else {
            conn = sceHttpCreateConnectionWithURL(tpl, urlDownloadBg, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, urlDownloadBg, 0);
        }

        if (sceHttpSendRequest(req, NULL, 0) >= 0) {
            size_t tamanhoTotal = 0; int32_t statusRes = 0; sceHttpGetResponseContentLength(req, &statusRes, &tamanhoTotal);
            FILE* f = fopen(pathFinal, "wb");
            if (f) {
                unsigned char* buf = (unsigned char*)malloc(65536);
                if (buf) {
                    int n; uint64_t baixado = 0;
                    while ((n = sceHttpReadData(req, buf, 65536)) > 0) {
                        fwrite(buf, 1, n, f); baixado += n;

                        char msgTemp[256];
                        if (tamanhoTotal > 0) {
                            progressoAtualDownload = (float)baixado / (float)tamanhoTotal;
                            sprintf(msgTemp, "BAIXANDO: %s", nomeArquivo);
                        }
                        else {
                            progressoAtualDownload = 0.5f;
                            sprintf(msgTemp, "BAIXANDO: %s... (%.2f MB)", nomeArquivo, (float)baixado / 1048576.0f);
                        }
                        strcpy(msgDownloadBg, msgTemp);

                        sceKernelUsleep(1000);
                    }
                    free(buf);
                }
                fclose(f);

                if (strstr(nomeArquivo, ".pkg") != NULL || strstr(nomeArquivo, ".PKG") != NULL) {
                    sceKernelMkdir("/data/pkg", 0777);
                    char destino[512]; sprintf(destino, "/data/pkg/%s", nomeArquivo);
                    int resMove = rename(pathFinal, destino);
                    if (resMove == 0) { sprintf(msgStatus, "PKG PREPARADO! Va em GoldHEN"); }
                    else { sprintf(msgStatus, "BAIXADO, MAS ERRO AO MOVER!"); }
                    msgTimer = 400;
                }
                else {
                    sprintf(msgStatus, "ARQUIVO CONCLUIDO!");
                    msgTimer = 300;
                }
            }
        }
        else {
            sprintf(msgStatus, "ERRO NO ARQUIVO ATUAL!");
            msgTimer = 240;
        }

        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);

        removerPrimeiroDaFila();
        baixadosFilaSessao++;
    }

    totalFilaSessao = 0;
    baixadosFilaSessao = 0;
    downloadEmSegundoPlano = false;
    return NULL;
}

// INICIADOR DO DOWNLOAD SINGLE
void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 2) return;

    char nomeArquivo[128] = "arquivo.bin";
    char* ref = strrchr(url, '/');
    if (ref) { strncpy(nomeArquivo, ref + 1, 127); nomeArquivo[127] = '\0'; }

    char* ptrInterrogacao = strchr(nomeArquivo, '?'); if (ptrInterrogacao) *ptrInterrogacao = '\0';
    char* ptrHash = strchr(nomeArquivo, '#'); if (ptrHash) *ptrHash = '\0';

    char nomeLimpo[128] = { 0 }; int j = 0;
    for (int i = 0; nomeArquivo[i] != '\0' && j < 127; i++) {
        if (nomeArquivo[i] == '%' && nomeArquivo[i + 1] == '2' && nomeArquivo[i + 2] == '0') { nomeLimpo[j++] = ' '; i += 2; }
        else { nomeLimpo[j++] = nomeArquivo[i]; }
    }
    strcpy(nomeArquivo, nomeLimpo);

    char* ext = strrchr(nomeArquivo, '.');
    if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) {
        instalarPkgRemoto(url, nomeArquivo);
        return;
    }

    int antes = obterTamanhoFila();

    if (!downloadEmSegundoPlano) {
        totalFilaSessao = antes;
        baixadosFilaSessao = 0;
    }

    adicionarNaFila(url, menuAtual, caminhoXMLAtual);

    int depois = obterTamanhoFila();

    if (depois == antes) {
        sprintf(msgStatus, "LINK JA EXISTE NA FILA!");
        msgTimer = 180;
    }
    else {
        sprintf(msgStatus, "ADICIONADO A FILA!");
        msgTimer = 180;
    }

    if (!downloadEmSegundoPlano) {
        downloadEmSegundoPlano = true;
        progressoAtualDownload = 0.0f;
        strcpy(msgDownloadBg, "PREPARANDO O DOWNLOAD...");

        pthread_t threadId;
        pthread_create(&threadId, NULL, threadDownloadBackground, NULL);
        pthread_detach(threadId);
    }
}

// A THREAD QUE ADICIONA MÚLTIPLOS ITENS
void* threadAdicionarSelecionados(void* arg) {
    processandoFila = true;
    int countAdicionados = 0;
    int countRepetidos = 0;

    pthread_mutex_lock(&filaMutex);

    char* conteudoFila = NULL;
    FILE* fCheck = fopen(caminhoFila, "r");
    if (fCheck) {
        fseek(fCheck, 0, SEEK_END);
        long sz = ftell(fCheck);
        fseek(fCheck, 0, SEEK_SET);
        if (sz > 0) {
            conteudoFila = (char*)malloc(sz + 1);
            fread(conteudoFila, 1, sz, fCheck);
            conteudoFila[sz] = '\0';
        }
        fclose(fCheck);
    }

    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
    FILE* fAppend = fopen(caminhoFila, "a");

    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) {
            char* alvo = linksAtuais[i];
            int len = strlen(alvo);

            if (len > 0 && alvo[len - 1] == '/') {
                pthread_mutex_unlock(&filaMutex);
                char limpo[512]; strcpy(limpo, alvo); limpo[len - 1] = '\0';
                pthread_mutex_lock(&filaMutex);
            }
            else {
                bool existe = false;
                if (conteudoFila && strstr(conteudoFila, alvo) != NULL) {
                    existe = true;
                }

                if (!existe) {
                    if (fAppend) {
                        fprintf(fAppend, "%s|%d|%s\n", alvo, (int)menuAtual, caminhoXMLAtual);
                        totalFilaSessao++;
                        countAdicionados++;
                    }
                }
                else {
                    countRepetidos++;
                }
            }
            marcados[i] = false;
        }
    }

    if (fAppend) fclose(fAppend);
    if (conteudoFila) free(conteudoFila);

    pthread_mutex_unlock(&filaMutex);

    if (countAdicionados > 0) {
        if (countRepetidos > 0) {
            sprintf(msgStatus, "+%d NA FILA! (%d REPETIDOS IGNORADOS)", countAdicionados, countRepetidos);
        }
        else {
            sprintf(msgStatus, "+%d ARQUIVOS ADICIONADOS A FILA!", countAdicionados);
        }
        msgTimer = 240;

        if (!downloadEmSegundoPlano) {
            downloadEmSegundoPlano = true;
            progressoAtualDownload = 0.0f;
            strcpy(msgDownloadBg, "PREPARANDO O DOWNLOAD...");

            pthread_t tDl;
            pthread_create(&tDl, NULL, threadDownloadBackground, NULL);
            pthread_detach(tDl);
        }
    }
    else if (countRepetidos > 0) {
        sprintf(msgStatus, "TODOS OS ITENS SELECIONADOS JA ESTAO NA FILA!");
        msgTimer = 180;
    }

    processandoFila = false;
    return NULL;
}

void fazerDownloadSelecionados() {
    if (processandoFila) {
        sprintf(msgStatus, "AGUARDE, JA ESTAMOS PROCESSANDO UMA LISTA...");
        msgTimer = 180;
        return;
    }
    sprintf(msgStatus, "PROCESSANDO ITENS MARCADOS...");
    msgTimer = 180;

    pthread_t threadId;
    pthread_create(&threadId, NULL, threadAdicionarSelecionados, NULL);
    pthread_detach(threadId);
}

// UPLOAD EM SEGUNDO PLANO
void* threadUploadSelecionados(void* arg) {
    processandoFilaUpload = true;
    int count = 0;
    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) {
            char* alvo = linksAtuais[i];
            int len = strlen(alvo);
            if (len > 0 && alvo[len - 1] == '/') {
                char limpo[512]; strcpy(limpo, alvo); limpo[len - 1] = '\0';
            }
            else {
            }
            marcados[i] = false;
            count++;
        }
    }
    if (count > 0) {
        sprintf(msgStatus, "UPLOAD DE %d ITEM(S) CONCLUIDO!", count);
        msgTimer = 180;
    }
    processandoFilaUpload = false;
    return NULL;
}

void fazerUploadSelecionados() {
    if (processandoFilaUpload) {
        sprintf(msgStatus, "AGUARDE, JA EXISTE UM UPLOAD EM LOTE RODANDO!");
        msgTimer = 180;
        return;
    }
    sprintf(msgStatus, "INICIANDO UPLOAD DOS ARQUIVOS MARCADOS...");
    msgTimer = 180;
    pthread_t tUp;
    pthread_create(&tUp, NULL, threadUploadSelecionados, NULL);
    pthread_detach(tUp);
}

void listarArquivosUpload(const char* dirPath) {
    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;
    strcpy(currentUploadPath, dirPath);

    DIR* d = opendir(dirPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
            strcpy(nomes[totalItens], dir->d_name); sprintf(linksAtuais[totalItens], "%s/%s", dirPath, dir->d_name);
            if (dir->d_type == DT_DIR) { strcat(nomes[totalItens], " (Pasta)"); strcat(linksAtuais[totalItens], "/"); }
            totalItens++; if (totalItens >= 2900) break;
        } closedir(d);
    }
    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia / Acesso Negado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_DROPBOX_UPLOAD; sel = 0; off = 0;
    sprintf(msgStatus, "SELECIONE UM ARQUIVO PARA ENVIAR");
}

void fazerUploadDropbox(const char* localPath) {
    int len = strlen(localPath);
    if (localPath[len - 1] == '/') { sprintf(msgStatus, "ERRO: SELECIONE UM ARQUIVO, NAO PASTA"); msgTimer = 180; return; }

    char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken);
    }
    for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';

    if (strlen(token) < 15) { sprintf(msgStatus, "ERRO: TOKEN INVALIDO"); msgTimer = 180; return; }
    FILE* fLocal = fopen(localPath, "rb"); if (!fLocal) { sprintf(msgStatus, "ERRO AO LER ARQUIVO LOCAL"); msgTimer = 180; return; }

    fseek(fLocal, 0, SEEK_END); long fileSize = ftell(fLocal); fseek(fLocal, 0, SEEK_SET);
    if (fileSize > 80 * 1024 * 1024) { sprintf(msgStatus, "ERRO: ARQUIVO GRANDE DEMAIS (>80MB)"); msgTimer = 180; fclose(fLocal); return; }

    unsigned char* fileData = (unsigned char*)malloc(fileSize);
    if (!fileData) { sprintf(msgStatus, "ERRO: MEMORIA INSUFICIENTE"); msgTimer = 180; fclose(fLocal); return; }

    char nomeArquivo[128] = "upload.bin"; char* ref = strrchr(localPath, '/'); if (ref) strncpy(nomeArquivo, ref + 1, 127);

    long lido = 0; long chunk = 1024 * 1024;
    while (lido < fileSize) {
        long lerAgora = (fileSize - lido > chunk) ? chunk : (fileSize - lido);
        fread(fileData + lido, 1, lerAgora, fLocal); lido += lerAgora;
        float prog = ((float)lido / (float)fileSize) * 0.5f;
        sprintf(msgStatus, "LENDO ARQUIVO: %s (%d%%)", nomeArquivo, (int)(prog * 200)); atualizarBarra(prog);
    } fclose(fLocal);

    char remoteDest[1024];
    sprintf(remoteDest, "/HyperNeiva_Uploads/%s", nomeArquivo);
    fazerUploadArquivoParaNuvem(localPath, remoteDest);
    free(fileData);
}

void criarPastaDropbox(const char* remotePath) {
    char token[2048] = { 0 };
    FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
    for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';
    if (strlen(token) < 15) return;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/2/files/create_folder_v2";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    sceHttpAddRequestHeader(req, "Content-Type", "application/json", 1);

    char postData[1024]; sprintf(postData, "{\"path\": \"%s\", \"autorename\": false}", remotePath);
    sceHttpSetRequestContentLength(req, strlen(postData));

    sprintf(msgStatus, "CRIANDO PASTA NO DROPBOX...");
    atualizarBarra(0.3f);
    sceHttpSendRequest(req, postData, strlen(postData));

    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void fazerUploadArquivoParaNuvem(const char* localPath, const char* remoteFilePath) {
    char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
    for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';
    if (strlen(token) < 15) return;

    FILE* fLocal = fopen(localPath, "rb"); if (!fLocal) return;
    fseek(fLocal, 0, SEEK_END); long fileSize = ftell(fLocal); fseek(fLocal, 0, SEEK_SET);
    if (fileSize > 80 * 1024 * 1024) { fclose(fLocal); return; }

    unsigned char* fileData = (unsigned char*)malloc(fileSize); if (!fileData) { fclose(fLocal); return; }
    fread(fileData, 1, fileSize, fLocal); fclose(fLocal);

    char* ref = strrchr(localPath, '/'); const char* nomeExibido = ref ? ref + 1 : "arquivo";
    sprintf(msgStatus, "ENVIANDO: %s", nomeExibido); atualizarBarra(0.6f);

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();
    const char* apiUrl = "https://content.dropboxapi.com/2/files/upload";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    sceHttpAddRequestHeader(req, "Content-Type", "application/octet-stream", 1);

    char apiArg[2048];
    sprintf(apiArg, "{\"path\": \"%s\", \"mode\": \"add\", \"autorename\": true, \"mute\": false}", remoteFilePath);
    sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);
    sceHttpSetRequestContentLength(req, fileSize);

    sceHttpSendRequest(req, fileData, fileSize);

    free(fileData);
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void processarPastaRecursiva(const char* localRoot, const char* currentLocal) {
    DIR* d = opendir(currentLocal);
    if (!d) return;

    char remotePath[1024];
    const char* rootName = strrchr(localRoot, '/');
    if (!rootName) rootName = localRoot; else rootName++;
    const char* relativePart = currentLocal + strlen(localRoot);
    sprintf(remotePath, "/HyperNeiva_Uploads/%s%s", rootName, relativePart);

    criarPastaDropbox(remotePath);

    char** subFolders = (char**)malloc(100 * sizeof(char*));
    char** files = (char**)malloc(200 * sizeof(char*));
    for (int i = 0; i < 100; i++) subFolders[i] = (char*)malloc(512);
    for (int i = 0; i < 200; i++) files[i] = (char*)malloc(512);

    int numSubFolders = 0; int numFiles = 0;

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            char fullPath[512]; snprintf(fullPath, sizeof(fullPath), "%s/%s", currentLocal, dir->d_name);
            if (dir->d_type == DT_DIR && numSubFolders < 100) strcpy(subFolders[numSubFolders++], fullPath);
            else if (dir->d_type != DT_DIR && numFiles < 200) strcpy(files[numFiles++], fullPath);
        }
    }
    closedir(d);

    for (int i = 0; i < numSubFolders; i++) processarPastaRecursiva(localRoot, subFolders[i]);
    for (int i = 0; i < numFiles; i++) {
        char remoteFilePath[1024]; char* fileName = strrchr(files[i], '/');
        if (fileName) fileName++; else fileName = files[i];
        sprintf(remoteFilePath, "%s/%s", remotePath, fileName);
        fazerUploadArquivoParaNuvem(files[i], remoteFilePath);
    }

    for (int i = 0; i < 100; i++) free(subFolders[i]);
    for (int i = 0; i < 200; i++) free(files[i]);
    free(subFolders); free(files);
}

void fazerUploadPastaRecursivo(const char* dirPath) {
    sprintf(msgStatus, "INICIANDO UPLOAD DA PASTA..."); atualizarBarra(0.1f);
    processarPastaRecursiva(dirPath, dirPath);
    sprintf(msgStatus, "UPLOAD DE PASTA CONCLUIDO COM SUCESSO!"); atualizarBarra(1.0f); msgTimer = 240;
}

void fazerDownloadArquivoDaNuvem(const char* remotePath, const char* localPath) {
    char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
    for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';
    if (strlen(token) < 15) return;

    char* nomeArquivo = strrchr(remotePath, '/'); if (nomeArquivo) nomeArquivo++; else nomeArquivo = (char*)remotePath;
    sprintf(msgStatus, "BAIXANDO: %s", nomeArquivo); atualizarBarra(0.5f);

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();

    const char* apiUrl = "https://content.dropboxapi.com/2/files/download";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", remotePath);
    sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        FILE* f = fopen(localPath, "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f);
        }
    }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void processarDownloadPastaRecursiva(const char* remotePath, const char* localPath) {
    char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
    for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';
    if (strlen(token) < 15) return;

    sceKernelMkdir(localPath, 0777);

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/2/files/list_folder";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char postData[512]; sprintf(postData, "{\"path\": \"%s\"}", remotePath);
    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    sceHttpAddRequestHeader(req, "Content-Type", "application/json; charset=utf-8", 1);
    sceHttpSetRequestContentLength(req, strlen(postData));

    sprintf(msgStatus, "LENDO PASTA: %s", remotePath); atualizarBarra(0.3f);

    if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
        sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
        FILE* f = fopen("/data/HyperNeiva/configuracao/temporario/temp_down_folder.json", "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f); fclose(f);
            FILE* f2 = fopen("/data/HyperNeiva/configuracao/temporario/temp_down_folder.json", "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                char* h = (char*)malloc(sz + 1);
                if (h) {
                    fread(h, 1, sz, f2); h[sz] = '\0'; char* p = h;

                    char** subFolders = (char**)malloc(100 * sizeof(char*)); char** files = (char**)malloc(200 * sizeof(char*));
                    char** subFoldersNames = (char**)malloc(100 * sizeof(char*)); char** filesNames = (char**)malloc(200 * sizeof(char*));
                    for (int i = 0; i < 100; i++) { subFolders[i] = (char*)malloc(512); subFoldersNames[i] = (char*)malloc(256); }
                    for (int i = 0; i < 200; i++) { files[i] = (char*)malloc(512); filesNames[i] = (char*)malloc(256); }
                    int numSubFolders = 0; int numFiles = 0;

                    while ((p = strstr(p, "\".tag\": \""))) {
                        p += 9; bool isFolder = (strncmp(p, "folder", 6) == 0);
                        char* namePtr = strstr(p, "\"name\": \""); if (!namePtr) break; namePtr += 9; char* nameEnd = strchr(namePtr, '\"');
                        char* pathPtr = strstr(nameEnd, "\"path_display\": \""); if (!pathPtr) break; pathPtr += 17; char* pathEnd = strchr(pathPtr, '\"');

                        if (nameEnd && pathEnd) {
                            int nameLen = nameEnd - namePtr; int pathLen = pathEnd - pathPtr;
                            if (isFolder && numSubFolders < 100) {
                                strncpy(subFolders[numSubFolders], pathPtr, pathLen); subFolders[numSubFolders][pathLen] = '\0';
                                strncpy(subFoldersNames[numSubFolders], namePtr, nameLen); subFoldersNames[numSubFolders][nameLen] = '\0';
                                numSubFolders++;
                            }
                            else if (!isFolder && numFiles < 200) {
                                strncpy(files[numFiles], pathPtr, pathLen); files[numFiles][pathLen] = '\0';
                                strncpy(filesNames[numFiles], namePtr, nameLen); filesNames[numFiles][nameLen] = '\0';
                                numFiles++;
                            }
                        } p = pathEnd;
                    } free(h);

                    for (int i = 0; i < numSubFolders; i++) { char nextLocal[512]; sprintf(nextLocal, "%s/%s", localPath, subFoldersNames[i]); processarDownloadPastaRecursiva(subFolders[i], nextLocal); }
                    for (int i = 0; i < numFiles; i++) { char nextLocal[512]; sprintf(nextLocal, "%s/%s", localPath, filesNames[i]); fazerDownloadArquivoDaNuvem(files[i], nextLocal); }
                    for (int i = 0; i < 100; i++) { free(subFolders[i]); free(subFoldersNames[i]); }
                    for (int i = 0; i < 200; i++) { free(files[i]); free(filesNames[i]); }
                    free(subFolders); free(files); free(subFoldersNames); free(filesNames);
                } fclose(f2);
            }
        }
    } sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void fazerDownloadPastaRecursivo(const char* remotePath, const char* folderName) {
    sprintf(msgStatus, "INICIANDO DOWNLOAD DA PASTA..."); atualizarBarra(0.1f);
    char localRoot[512]; sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777);
    sprintf(localRoot, "/data/HyperNeiva/baixado/dropbox/%s", folderName);
    processarDownloadPastaRecursiva(remotePath, localRoot);
    sprintf(msgStatus, "DOWNLOAD DE PASTA CONCLUIDO!"); atualizarBarra(1.0f); msgTimer = 240;
}

void executarBackupTodos() {
    fazerUploadDropbox("/system_data/priv/mms/app.db");
    fazerUploadDropbox("/data/HyperNeiva/configuracao/dropbox_token.txt");
    fazerUploadDropbox("/data/retroarch/retroarch.cfg");
    sprintf(msgStatus, "BACKUP ESSENCIAL CONCLUIDO!"); msgTimer = 180;
}