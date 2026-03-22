#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/UserService.h>  
#include <orbis/Sysmodule.h> 
#include <orbis/Bgft.h> // Inclui o BGFT para Downloads Nativos

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

void preencherMenuBackup() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Selecionar Manualmente");
    strcpy(nomes[1], "Backup Todos (App.db e Configs)");
    strcpy(nomes[2], "Backup de Saves (PS4)");
    strcpy(nomes[3], "Backup do RetroArch");
    strcpy(nomes[4], "Backup Hyper Neiva Configuracao");
    strcpy(nomes[5], "Backup do Banco de Dados (App.db)");
    strcpy(nomes[6], "Backup de Screenshots");
    strcpy(nomes[7], "Trofeus (Trophy)");
    strcpy(nomes[8], "Saves Apollo");
    totalItens = 9;
    menuAtual = MENU_BAIXAR_DROPBOX_BACKUP;
    sel = 0; off = 0;
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
        FILE* f = fopen("/data/HyperNeiva/temp_dropbox.json", "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f); fclose(f);
            FILE* f2 = fopen("/data/HyperNeiva/temp_dropbox.json", "rb");
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


// =======================================================================
// O SISTEMA HÍBRIDO (DOWNLOAD LOCAL P/ MIDIA e FPKGi PARA JOGOS)
// =======================================================================
void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 2) return;

    char nomeArquivo[128] = "arquivo.bin";
    char* ref = strrchr(url, '/');
    if (ref) strncpy(nomeArquivo, ref + 1, 127);

    // =====================================================================
    // ROTA 1: MÉTODO FPKGi (Apenas para Arquivos .PKG)
    // =====================================================================
    if (strstr(nomeArquivo, ".pkg") != NULL || strstr(nomeArquivo, ".PKG") != NULL) {

        sprintf(msgStatus, "PREPARANDO LINK DO DROPBOX...");
        msgTimer = 150; atualizarBarra(0.2f);

        static char directUrl[4096];
        memset(directUrl, 0, sizeof(directUrl));

        if (url[0] == '/') {
            static char token[2048];
            memset(token, 0, sizeof(token));
            FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
            if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
            for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';

            int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
            sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();
            const char* apiUrl = "https://api.dropboxapi.com/2/files/get_temporary_link";
            int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
            int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

            static char authHeader[2048];
            sprintf(authHeader, "Bearer %s", token);
            sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
            sceHttpAddRequestHeader(req, "Content-Type", "application/json", 1);

            static char postData[1024];
            sprintf(postData, "{\"path\": \"%s\"}", url);
            sceHttpSetRequestContentLength(req, strlen(postData));

            if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
                static unsigned char buf[8192];
                memset(buf, 0, sizeof(buf));

                int n = sceHttpReadData(req, buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    char* linkPtr = strstr((char*)buf, "\"link\": \"");
                    if (linkPtr) {
                        linkPtr += 9;
                        char* linkEnd = strchr(linkPtr, '\"');
                        if (linkEnd) {
                            static char tempUrl[4096];
                            memset(tempUrl, 0, sizeof(tempUrl));

                            int copyLen = linkEnd - linkPtr;
                            if (copyLen > 4000) copyLen = 4000;
                            strncpy(tempUrl, linkPtr, copyLen);

                            int c_idx = 0;
                            for (int i = 0; i < copyLen && c_idx < 4000; i++) {
                                if (tempUrl[i] == '\\') {
                                    if (i + 1 < copyLen && tempUrl[i + 1] == '/') {
                                        directUrl[c_idx++] = '/';
                                        i++;
                                        continue;
                                    }
                                    if (i + 5 < copyLen && tempUrl[i + 1] == 'u' && tempUrl[i + 2] == '0' && tempUrl[i + 3] == '0' && tempUrl[i + 4] == '2' && tempUrl[i + 5] == '6') {
                                        directUrl[c_idx++] = '&';
                                        i += 5;
                                        continue;
                                    }
                                }
                                else if (tempUrl[i] >= 33 && tempUrl[i] <= 126) {
                                    directUrl[c_idx++] = tempUrl[i];
                                }
                            }
                            directUrl[c_idx] = '\0';
                        }
                    }
                }
            }
            sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);

            if (strlen(directUrl) < 10) {
                sprintf(msgStatus, "ERRO: FALHA AO OBTER LINK TEMPORARIO DO PKG");
                msgTimer = 180; atualizarBarra(0.0f);
                return;
            }
        }
        else {
            strncpy(directUrl, url, 4000);
        }

        if (strstr(directUrl, ".pkg") == NULL && strstr(directUrl, ".PKG") == NULL) {
            if (strchr(directUrl, '?') != NULL) {
                strcat(directUrl, "&fake=.pkg");
            }
            else {
                strcat(directUrl, "?fake=.pkg");
            }
        }

        static char nomeLimpo[256];
        memset(nomeLimpo, 0, sizeof(nomeLimpo));
        int n_idx = 0;
        for (int i = 0; i < strlen(nomeArquivo) && n_idx < 255; i++) {
            char c = nomeArquivo[i];
            if (c >= 33 && c <= 126) {
                nomeLimpo[n_idx++] = c;
            }
        }
        nomeLimpo[n_idx] = '\0';

        sprintf(msgStatus, "ENVIANDO PARA O DOWNLOADER DO PS4...");
        atualizarBarra(0.5f);

        // CORREÇÃO: Usamos sceKernelLoadStartModule ao invés de SysmoduleLoadInternal
        // O ID do BGFT é 0x0041, mas vamos puxar o módulo "libSceBgft.sprx" pelo nome!
        int bgftHandle = sceKernelLoadStartModule("libSceBgft.sprx", 0, NULL, 0, NULL, NULL);
        if (bgftHandle <= 0) {
            bgftHandle = sceKernelLoadStartModule("/system/common/lib/libSceBgft.sprx", 0, NULL, 0, NULL, NULL);
        }

        static bool bgft_inicializado = false;
        static void* bgft_heap = NULL;

        if (!bgft_inicializado) {
            if (!bgft_heap) {
                void* raw = malloc((1024 * 1024) + 4096);
                if (raw) {
                    bgft_heap = (void*)(((uintptr_t)raw + 4095) & ~4095);
                }
            }

            if (bgft_heap) {
                OrbisBgftInitParams bgftInit;
                memset(&bgftInit, 0, sizeof(bgftInit));
                bgftInit.heap = bgft_heap;
                bgftInit.heapSize = 1024 * 1024;

                int retInit = sceBgftServiceIntInit(&bgftInit);
                if (retInit == 0 || retInit == 0x80431003) {
                    bgft_inicializado = true;
                }
                else {
                    sprintf(msgStatus, "ERRO INIT BGFT: 0x%08X", retInit);
                    msgTimer = 300; atualizarBarra(0.0f);
                    return;
                }
            }
            else {
                sprintf(msgStatus, "ERRO DE MEMORIA MALLOC");
                return;
            }
        }

        int32_t userId = 0x10000000;
        sceUserServiceGetInitialUser(&userId);
        if (userId == 0 || userId == -1) {
            userId = 0x10000000;
        }

        static OrbisBgftDownloadParam bgftParam;
        memset(&bgftParam, 0, sizeof(bgftParam));

        bgftParam.userId = userId;
        bgftParam.entitlementType = 5;
        bgftParam.id = "UP0001-CUSA00001_00-0000000000000000";
        bgftParam.contentUrl = directUrl;
        bgftParam.contentExUrl = "";
        bgftParam.contentName = nomeLimpo;
        bgftParam.iconPath = "";
        bgftParam.skuId = "";
        bgftParam.option = ORBIS_BGFT_TASK_OPT_NONE;
        bgftParam.playgoScenarioId = "0";
        bgftParam.releaseDate = "";
        bgftParam.packageType = "xg";
        bgftParam.packageSubType = "";
        bgftParam.packageSize = 0;

        OrbisBgftTaskId taskId = -1;

        int res = sceBgftServiceIntDebugDownloadRegisterPkg(&bgftParam, &taskId);
        if (res != 0) {
            res = sceBgftServiceIntDownloadRegisterTask(&bgftParam, &taskId);
        }

        if (res == 0) {
            sprintf(msgStatus, "SUCESSO! VEJA AS NOTIFICACOES DO PS4!");
            atualizarBarra(1.0f);
        }
        else {
            sprintf(msgStatus, "ERRO AO INSERIR NO PS4: 0x%08X", res);
            atualizarBarra(0.0f);
        }

        msgTimer = 500;
        return;
    }

    // =====================================================================
    // ROTA 2: MÉTODO CLÁSSICO (Para Imagens, Músicas, Arquivos do App)
    // =====================================================================
    char pathPasta[256];
    sprintf(pathPasta, "/data/HyperNeiva/baixado/Downloads");
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir(pathPasta, 0777);

    char pathFinal[512];
    sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);

    sprintf(msgStatus, "CONECTANDO AO DROPBOX..."); msgTimer = 150; atualizarBarra(0.05f);

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect(); int conn, req;

    if (url[0] == '/') {
        char token[2048] = { 0 }; FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
        if (fToken) { fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET); if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; } fclose(fToken); }
        for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';

        const char* apiUrl = "https://content.dropboxapi.com/2/files/download";
        conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);
        char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
        sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
        char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", url);
        sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);
    }
    else {
        conn = sceHttpCreateConnectionWithURL(tpl, url, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    }

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        size_t tamanhoTotal = 0; int32_t statusRes = 0; sceHttpGetResponseContentLength(req, &statusRes, &tamanhoTotal);
        FILE* f = fopen(pathFinal, "wb");
        if (f) {
            unsigned char buf[32768]; int n; uint64_t baixado = 0;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f); baixado += n;
                if (tamanhoTotal > 0) {
                    float prog = (float)baixado / (float)tamanhoTotal;
                    sprintf(msgStatus, "BAIXANDO: %s (%d%%)", nomeArquivo, (int)(prog * 100)); atualizarBarra(prog);
                }
                else { sprintf(msgStatus, "BAIXANDO: %s...", nomeArquivo); atualizarBarra(0.5f); }
            } fclose(f);

            sprintf(msgStatus, "DOWNLOAD CONCLUIDO!");
            atualizarBarra(1.0f);
        }
    }
    else { sprintf(msgStatus, "ERRO NO DOWNLOAD"); atualizarBarra(0.0f); }

    msgTimer = 240; sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
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
        FILE* f = fopen("/data/HyperNeiva/temp_down_folder.json", "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f); fclose(f);
            FILE* f2 = fopen("/data/HyperNeiva/temp_down_folder.json", "rb");
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
    char localRoot[512]; sceKernelMkdir("/data/HyperNeiva/baixado/Downloads", 0777);
    sprintf(localRoot, "/data/HyperNeiva/baixado/Downloads/%s", folderName);
    processarDownloadPastaRecursiva(remotePath, localRoot);
    sprintf(msgStatus, "DOWNLOAD DE PASTA CONCLUIDO!"); atualizarBarra(1.0f); msgTimer = 240;
}

void fazerUploadSelecionados() {
    int count = 0;
    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) {
            char* alvo = linksAtuais[i];
            int len = strlen(alvo);
            if (len > 0 && alvo[len - 1] == '/') {
                char limpo[512]; strcpy(limpo, alvo); limpo[len - 1] = '\0';
                fazerUploadPastaRecursivo(limpo);
            }
            else {
                fazerUploadDropbox(alvo);
            }
            marcados[i] = false;
            count++;
        }
    }
    if (count > 0) {
        sprintf(msgStatus, "UPLOAD DE %d ITEM(S) CONCLUIDO!", count);
        msgTimer = 180;
    }
}

void fazerDownloadSelecionados() {
    int count = 0;
    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) {
            char* alvo = linksAtuais[i];
            int len = strlen(alvo);
            if (len > 0 && alvo[len - 1] == '/') {
                char limpo[512]; strcpy(limpo, alvo); limpo[len - 1] = '\0';
                fazerDownloadPastaRecursivo(limpo, nomes[i]);
            }
            else {
                iniciarDownload(alvo);
            }
            marcados[i] = false;
            count++;
        }
    }
    if (count > 0) {
        sprintf(msgStatus, "DOWNLOAD DE %d ITEM(S) CONCLUIDO!", count);
        msgTimer = 180;
    }
}

void executarBackupTodos() {
    fazerUploadDropbox("/system_data/priv/mms/app.db");
    fazerUploadDropbox("/data/HyperNeiva/configuracao/dropbox_token.txt");
    fazerUploadDropbox("/data/retroarch/retroarch.cfg");
    sprintf(msgStatus, "BACKUP ESSENCIAL CONCLUIDO!"); msgTimer = 180;
}