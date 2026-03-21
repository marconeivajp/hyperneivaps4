#include "baixar_dropbox_download.h"
#include "menu.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/libkernel.h>

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
    char cleanPath[256];
    strncpy(cleanPath, path, 255);
    cleanPath[255] = '\0';
    for (int i = 0; i < strlen(cleanPath); i++) {
        if (cleanPath[i] == '\r' || cleanPath[i] == '\n') cleanPath[i] = '\0';
    }

    strcpy(currentDropboxPath, cleanPath);

    char token[2048] = { 0 };
    FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; }
        fclose(fToken);
    }
    for (int i = 0; i < strlen(token); i++) { if (token[i] == '\r' || token[i] == '\n') token[i] = '\0'; }

    if (strlen(token) < 15) {
        sprintf(msgStatus, "ERRO: TOKEN NO TXT INVALIDO");
        memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "Verifique o arquivo .txt");
        totalItens = 1; menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0; return;
    }

    sprintf(msgStatus, "CONECTANDO A API...");
    atualizarBarra(0.3f);
    msgTimer = 180;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/2/files/list_folder";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char postData[512]; memset(postData, 0, sizeof(postData));
    sprintf(postData, "{\"path\": \"%s\"}", cleanPath);

    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);

    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    sceHttpAddRequestHeader(req, "Content-Type", "application/json; charset=utf-8", 1);
    sceHttpSetRequestContentLength(req, strlen(postData));

    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); totalItens = 0;

    atualizarBarra(0.6f);

    if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
        FILE* f = fopen("/data/HyperNeiva/temp_dropbox.json", "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f);

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
                            }
                            p = pathEnd;
                        }
                        if (totalItens > 0) sprintf(msgStatus, "PASTAS CARREGADAS!");
                        else { strcpy(nomes[0], "Pasta vazia"); totalItens = 1; }
                    }
                    free(h);
                }
                fclose(f2);
            }
        }
    }
    else sprintf(msgStatus, "ERRO: FALHA DE REDE");

    atualizarBarra(1.0f);
    msgTimer = 240; sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0;
}

void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 2) return;
    char pathPasta[256]; sprintf(pathPasta, "/data/HyperNeiva/baixado/Downloads");
    sceKernelMkdir(pathPasta, 0777);
    char nomeArquivo[128] = "arquivo.bin"; char* ref = strrchr(url, '/');
    if (ref) strncpy(nomeArquivo, ref + 1, 127);
    char pathFinal[512]; sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);
    sprintf(msgStatus, "CONECTANDO..."); msgTimer = 150;
    atualizarBarra(0.05f);

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();
    int conn, req;

    if (url[0] == '/') {
        char token[2048] = { 0 };
        FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
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
        size_t tamanhoTotal = 0;
        int32_t statusRes = 0;
        sceHttpGetResponseContentLength(req, &statusRes, &tamanhoTotal);

        FILE* f = fopen(pathFinal, "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            uint64_t baixado = 0;

            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
                baixado += n;

                if (tamanhoTotal > 0) {
                    float prog = (float)baixado / (float)tamanhoTotal;
                    sprintf(msgStatus, "BAIXANDO: %s (%d%%)", nomeArquivo, (int)(prog * 100));
                    atualizarBarra(prog);
                }
                else {
                    sprintf(msgStatus, "BAIXANDO: %s...", nomeArquivo);
                    atualizarBarra(0.5f);
                }
            }
            fclose(f);
            sprintf(msgStatus, "DOWNLOAD CONCLUIDO!");
            atualizarBarra(1.0f);
        }
    }
    else {
        sprintf(msgStatus, "ERRO NO DOWNLOAD");
        atualizarBarra(0.0f);
    }

    msgTimer = 180; sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void listarArquivosUpload(const char* dirPath) {
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    totalItens = 0;

    strcpy(currentUploadPath, dirPath);

    DIR* d = opendir(dirPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            strcpy(nomes[totalItens], dir->d_name);
            sprintf(linksAtuais[totalItens], "%s/%s", dirPath, dir->d_name);

            if (dir->d_type == DT_DIR) {
                strcat(nomes[totalItens], " (Pasta)");
                strcat(linksAtuais[totalItens], "/");
            }

            totalItens++;
            if (totalItens >= 2900) break;
        }
        closedir(d);
    }
    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia / Acesso Negado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_DROPBOX_UPLOAD; sel = 0; off = 0;
    sprintf(msgStatus, "SELECIONE UM ARQUIVO PARA ENVIAR");
}

void fazerUploadDropbox(const char* localPath) {
    int len = strlen(localPath);
    if (localPath[len - 1] == '/') {
        sprintf(msgStatus, "ERRO: SELECIONE UM ARQUIVO, NAO PASTA");
        msgTimer = 180;
        return;
    }

    char token[2048] = { 0 };
    FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < 2047) { fread(token, 1, sz, fToken); token[sz] = '\0'; }
        fclose(fToken);
    }
    for (int i = 0; i < strlen(token); i++) if (token[i] == '\r' || token[i] == '\n') token[i] = '\0';

    if (strlen(token) < 15) { sprintf(msgStatus, "ERRO: TOKEN INVALIDO"); msgTimer = 180; return; }

    FILE* fLocal = fopen(localPath, "rb");
    if (!fLocal) { sprintf(msgStatus, "ERRO AO LER ARQUIVO LOCAL"); msgTimer = 180; return; }

    fseek(fLocal, 0, SEEK_END);
    long fileSize = ftell(fLocal);
    fseek(fLocal, 0, SEEK_SET);

    if (fileSize > 80 * 1024 * 1024) {
        sprintf(msgStatus, "ERRO: ARQUIVO GRANDE DEMAIS (>80MB)"); msgTimer = 180; fclose(fLocal); return;
    }

    unsigned char* fileData = (unsigned char*)malloc(fileSize);
    if (!fileData) { sprintf(msgStatus, "ERRO: MEMORIA INSUFICIENTE"); msgTimer = 180; fclose(fLocal); return; }

    char nomeArquivo[128] = "upload.bin";
    char* ref = strrchr(localPath, '/');
    if (ref) strncpy(nomeArquivo, ref + 1, 127);

    long lido = 0;
    long chunk = 1024 * 1024;
    while (lido < fileSize) {
        long lerAgora = (fileSize - lido > chunk) ? chunk : (fileSize - lido);
        fread(fileData + lido, 1, lerAgora, fLocal);
        lido += lerAgora;

        float prog = ((float)lido / (float)fileSize) * 0.5f;
        sprintf(msgStatus, "LENDO ARQUIVO: %s (%d%%)", nomeArquivo, (int)(prog * 200));
        atualizarBarra(prog);
    }
    fclose(fLocal);

    sprintf(msgStatus, "ENVIANDO PARA A NUVEM... AGUARDE");
    atualizarBarra(0.75f);
    msgTimer = 300;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    const char* apiUrl = "https://content.dropboxapi.com/2/files/upload";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    sceHttpAddRequestHeader(req, "Content-Type", "application/octet-stream", 1);

    char apiArg[1024];
    sprintf(apiArg, "{\"path\": \"/HyperNeiva_Uploads/%s\", \"mode\": \"add\", \"autorename\": true, \"mute\": false}", nomeArquivo);
    sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);

    sceHttpSetRequestContentLength(req, fileSize);

    if (sceHttpSendRequest(req, fileData, fileSize) >= 0) {
        sprintf(msgStatus, "UPLOAD CONCLUIDO COM SUCESSO!");
        atualizarBarra(1.0f);
    }
    else {
        sprintf(msgStatus, "ERRO NO UPLOAD (FALHA DE REDE)");
        atualizarBarra(0.0f);
    }

    free(fileData);
    msgTimer = 240;
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void fazerUploadPastaDropbox(const char* dirPath) {
    DIR* d = opendir(dirPath);
    if (!d) {
        sprintf(msgStatus, "ERRO AO ABRIR A PASTA!");
        msgTimer = 180;
        return;
    }

    struct dirent* dir;
    char arquivos[200][512];
    int numArquivos = 0;

    while ((dir = readdir(d)) != NULL && numArquivos < 200) {
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            if (dir->d_type != DT_DIR) {
                int len = strlen(dirPath);
                const char* sep = (dirPath[len - 1] == '/') ? "" : "/";
                snprintf(arquivos[numArquivos], sizeof(arquivos[numArquivos]), "%s%s%s", dirPath, sep, dir->d_name);
                numArquivos++;
            }
        }
    }
    closedir(d);

    if (numArquivos == 0) {
        sprintf(msgStatus, "NENHUM ARQUIVO NA PASTA!");
        msgTimer = 180;
        return;
    }

    for (int i = 0; i < numArquivos; i++) {
        fazerUploadDropbox(arquivos[i]);
    }

    sprintf(msgStatus, "UPLOAD DA PASTA CONCLUIDO! (%d arquivos)", numArquivos);
    atualizarBarra(1.0f);
    msgTimer = 240;
}

void executarBackupTodos() {
    fazerUploadDropbox("/system_data/priv/mms/app.db");
    fazerUploadDropbox("/data/HyperNeiva/configuracao/dropbox_token.txt");
    fazerUploadDropbox("/data/retroarch/retroarch.cfg");
    sprintf(msgStatus, "BACKUP ESSENCIAL CONCLUIDO!");
    msgTimer = 180;
}