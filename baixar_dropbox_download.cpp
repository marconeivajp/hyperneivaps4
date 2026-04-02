#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h> 

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

extern void instalarPkgLocal(const char* caminhoAbsoluto);

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
// VARIÁVEIS GLOBAIS E CONTROLES DA FILA
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
const char* caminhoFilaUpload = "/data/HyperNeiva/configuracao/temporario/fila_uploads.txt";

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

// ==============================================================
// LÓGICA INTELIGENTE DE RENOVAÇÃO DO DROPBOX TOKEN (OAUTH2)
// ==============================================================
static char tokenAcessoGlobal[2048] = { 0 };

void obterTokenValido(char* outToken) {
    // Se já temos um token válido carregado na memória, não precisamos renovar nesta sessão.
    if (strlen(tokenAcessoGlobal) > 10) {
        strcpy(outToken, tokenAcessoGlobal);
        return;
    }

    char txt_token[2048] = { 0 };
    FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < 2047) { fread(txt_token, 1, sz, fToken); txt_token[sz] = '\0'; }
        fclose(fToken);
    }

    if (strlen(txt_token) < 15) {
        strcpy(outToken, "");
        return;
    }

    // CREDENCIAIS PADRÃO DO HYPER NEIVA
    char appKey[128] = "345xl81u5bdp9x2";
    char appSecret[128] = "bhh9nfd8t7rvfuq";
    char refreshToken[512] = { 0 };
    bool temLabels = false;

    // Fazemos uma cópia para o strtok quebrar as linhas
    char txtCopy[2048];
    strcpy(txtCopy, txt_token);

    // Leitura inteligente Linha por Linha
    char* linha = strtok(txtCopy, "\r\n");
    while (linha) {
        if (strstr(linha, "Appkey:") || strstr(linha, "App key:") || strstr(linha, "AppKey:")) {
            char* val = strchr(linha, ':');
            if (val) sscanf(val + 1, " %s", appKey); // Puxa só a palavra, ignorando espaços
            temLabels = true;
        }
        else if (strstr(linha, "App secret:") || strstr(linha, "Appsecret:") || strstr(linha, "App Secret:")) {
            char* val = strchr(linha, ':');
            if (val) sscanf(val + 1, " %s", appSecret);
            temLabels = true;
        }
        else if (strstr(linha, "Token refresh:") || strstr(linha, "Tokenrefresh:") || strstr(linha, "Token Refresh:")) {
            char* val = strchr(linha, ':');
            if (val) sscanf(val + 1, " %s", refreshToken);
            temLabels = true;
        }
        linha = strtok(NULL, "\r\n");
    }

    // Se o TXT só tiver 1 linha sem labels, assumimos que é o Refresh Token (e usamos a Key e Secret Padrão)
    if (!temLabels) {
        sscanf(txt_token, "%s", refreshToken);
    }

    sprintf(msgStatus, "VERIFICANDO TOKEN E CONECTANDO...");

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/oauth2/token";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char postData[1024];
    sprintf(postData, "grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s", refreshToken, appKey, appSecret);

    sceHttpAddRequestHeader(req, "Content-Type", "application/x-www-form-urlencoded", 1);
    sceHttpSetRequestContentLength(req, strlen(postData));

    bool isRefreshToken = false;

    if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
        unsigned char buf[4096]; memset(buf, 0, sizeof(buf));
        if (sceHttpReadData(req, buf, sizeof(buf) - 1) > 0) {
            char* tokenStart = strstr((char*)buf, "\"access_token\": \"");
            if (tokenStart) {
                tokenStart += 17;
                char* tokenEnd = strchr(tokenStart, '\"');
                if (tokenEnd) {
                    int tokenLen = tokenEnd - tokenStart;
                    strncpy(tokenAcessoGlobal, tokenStart, tokenLen);
                    tokenAcessoGlobal[tokenLen] = '\0';
                    isRefreshToken = true;
                }
            }
        }
    }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);

    // Se a Dropbox não devolveu o Access_Token, significa que a string que mandámos NÃO ERA um Refresh Token.
    // Logo, assumimos que o utilizador colou o Token Temporário (ou o Token Legado antigo) diretamente, e usamos ele!
    if (!isRefreshToken) {
        strcpy(tokenAcessoGlobal, refreshToken);
    }

    strcpy(outToken, tokenAcessoGlobal);
}


// ==============================================================
// LÓGICA DA NOVA FILA DE UPLOAD 
// ==============================================================
void adicionarNaFilaUpload(const char* localPath, const char* remotePath) {
    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
    pthread_mutex_lock(&filaMutex);
    FILE* f = fopen(caminhoFilaUpload, "a");
    if (f) {
        fprintf(f, "%s|%s\n", localPath, remotePath);
        fclose(f);
    }
    pthread_mutex_unlock(&filaMutex);
}

void fazerUploadArquivoParaNuvem(const char* localPath, const char* remoteFilePath) {
    char token[2048] = { 0 };
    obterTokenValido(token);
    if (strlen(token) < 15) return;

    FILE* fLocal = fopen(localPath, "rb"); if (!fLocal) return;
    fseek(fLocal, 0, SEEK_END); long fileSize = ftell(fLocal); fseek(fLocal, 0, SEEK_SET);
    if (fileSize > 80 * 1024 * 1024) { fclose(fLocal); return; }

    char* nomeExibido = strrchr(localPath, '/'); if (nomeExibido) nomeExibido++; else nomeExibido = (char*)localPath;

    sprintf(msgDownloadBg, "UPLOAD [%d/%d]: %s", baixadosFilaSessao + 1, totalFilaSessao, nomeExibido);
    if (totalFilaSessao > 0) {
        progressoAtualDownload = (float)baixadosFilaSessao / (float)totalFilaSessao;
    }

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

    unsigned char* fileData = (unsigned char*)malloc(fileSize);
    if (fileData) {
        fread(fileData, 1, fileSize, fLocal);
        sceHttpSendRequest(req, fileData, fileSize);
        free(fileData);
    }
    fclose(fLocal);
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void* threadProcessarFilaUpload(void* arg) {
    processandoFilaUpload = true;
    downloadEmSegundoPlano = true;

    if (totalFilaSessao == 0) {
        downloadEmSegundoPlano = false;
        processandoFilaUpload = false;
        sprintf(msgStatus, "NENHUM ARQUIVO PARA ENVIAR!");
        msgTimer = 180;
        return NULL;
    }

    while (true) {
        char localPath[1024] = { 0 };
        char remotePath[1024] = { 0 };

        pthread_mutex_lock(&filaMutex);
        FILE* f = fopen(caminhoFilaUpload, "r");
        if (!f) { pthread_mutex_unlock(&filaMutex); break; }

        char linha[2048];
        if (fgets(linha, sizeof(linha), f)) {
            for (int i = 0; linha[i]; i++) if (linha[i] == '\r' || linha[i] == '\n') linha[i] = '\0';
            char* sep = strchr(linha, '|');
            if (sep) {
                *sep = '\0';
                strcpy(localPath, linha);
                strcpy(remotePath, sep + 1);
            }
        }
        else {
            fclose(f); pthread_mutex_unlock(&filaMutex); break;
        }
        fclose(f);
        pthread_mutex_unlock(&filaMutex);

        if (strlen(localPath) > 0 && strlen(remotePath) > 0) {
            fazerUploadArquivoParaNuvem(localPath, remotePath);
            baixadosFilaSessao++;
        }

        pthread_mutex_lock(&filaMutex);
        f = fopen(caminhoFilaUpload, "r");
        FILE* fTemp = fopen(caminhoFilaTemp, "w");
        if (f && fTemp) {
            bool first = true;
            while (fgets(linha, sizeof(linha), f)) {
                if (first) { first = false; continue; }
                fprintf(fTemp, "%s", linha);
            }
        }
        if (f) fclose(f);
        if (fTemp) fclose(fTemp);
        unlink(caminhoFilaUpload);
        rename(caminhoFilaTemp, caminhoFilaUpload);
        pthread_mutex_unlock(&filaMutex);
    }

    downloadEmSegundoPlano = false;
    processandoFilaUpload = false;
    unlink(caminhoFilaUpload);

    sprintf(msgStatus, "UPLOAD / BACKUP CONCLUIDO COM SUCESSO!");
    msgTimer = 300;
    return NULL;
}

// ==============================================================
// MAPEAMENTO (PREPARA A FILA)
// ==============================================================
void criarPastaDropbox(const char* remotePath) {
    char token[2048] = { 0 };
    obterTokenValido(token);
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

    sceHttpSendRequest(req, postData, strlen(postData));
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void mapearPastaParaUploadRecursivo(const char* localRoot, const char* currentLocal, const char* remoteBase) {
    DIR* d = opendir(currentLocal);
    if (!d) return;

    char remotePath[1024];
    const char* relativePart = currentLocal + strlen(localRoot);
    sprintf(remotePath, "%s%s", remoteBase, relativePart);

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", currentLocal, dir->d_name);

            struct stat st;
            bool isDir = false;
            if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
                isDir = true;
            }

            if (isDir) {
                mapearPastaParaUploadRecursivo(localRoot, fullPath, remoteBase);
            }
            else {
                char remoteFilePath[1024];
                sprintf(remoteFilePath, "%s/%s", remotePath, dir->d_name);
                adicionarNaFilaUpload(fullPath, remoteFilePath);

                totalFilaSessao++;
                if (totalFilaSessao % 5 == 0) {
                    sprintf(msgDownloadBg, "MAPEANDO ARQUIVOS: %d ENCONTRADOS...", totalFilaSessao);
                }
            }
        }
    }
    closedir(d);
}

// ==============================================================
// O BACKUP GERAL 
// ==============================================================
void* threadBackupTodos(void* arg) {
    unlink(caminhoFilaUpload);

    downloadEmSegundoPlano = true;
    progressoAtualDownload = 0.0f;
    totalFilaSessao = 0;
    baixadosFilaSessao = 0;

    sprintf(msgStatus, "MAPEANDO ESTRUTURA DO PS4... AGUARDE!");
    msgTimer = 900;

    sprintf(msgDownloadBg, "INICIANDO VARREDURA DOS ARQUIVOS...");

    mapearPastaParaUploadRecursivo("/user/appmeta", "/user/appmeta", "/HyperNeiva_Uploads/backup/user/appmeta");
    mapearPastaParaUploadRecursivo("/user/av_contents", "/user/av_contents", "/HyperNeiva_Uploads/backup/user/av_contents");
    mapearPastaParaUploadRecursivo("/user/home", "/user/home", "/HyperNeiva_Uploads/backup/user/home");
    mapearPastaParaUploadRecursivo("/user/trophy", "/user/trophy", "/HyperNeiva_Uploads/backup/user/trophy");
    mapearPastaParaUploadRecursivo("/data/HyperNeiva/configuracao", "/data/HyperNeiva/configuracao", "/HyperNeiva_Uploads/backup/data/HyperNeiva/configuracao");
    mapearPastaParaUploadRecursivo("/data/retroarch", "/data/retroarch", "/HyperNeiva_Uploads/backup/data/retroarch");
    mapearPastaParaUploadRecursivo("/system_data/priv/cache/profile", "/system_data/priv/cache/profile", "/HyperNeiva_Uploads/backup/system_data/priv/cache/profile");
    mapearPastaParaUploadRecursivo("/system_data/priv/home", "/system_data/priv/home", "/HyperNeiva_Uploads/backup/system_data/priv/home");

    adicionarNaFilaUpload("/system_data/priv/mms/app.db", "/HyperNeiva_Uploads/backup/system_data/priv/mms/app.db"); totalFilaSessao++;
    adicionarNaFilaUpload("/system_data/priv/mms/addcont.db", "/HyperNeiva_Uploads/backup/system_data/priv/mms/addcont.db"); totalFilaSessao++;
    adicionarNaFilaUpload("/system_data/priv/mms/av_content_bg.db", "/HyperNeiva_Uploads/backup/system_data/priv/mms/av_content_bg.db"); totalFilaSessao++;
    adicionarNaFilaUpload("/system_data/priv/mms/dbr_history.info", "/HyperNeiva_Uploads/backup/system_data/priv/mms/dbr_history.info"); totalFilaSessao++;
    adicionarNaFilaUpload("/system_data/priv/mms/notification.db", "/HyperNeiva_Uploads/backup/system_data/priv/mms/notification.db"); totalFilaSessao++;

    threadProcessarFilaUpload(NULL);
    return NULL;
}

void executarBackupTodos() {
    if (processandoFilaUpload) {
        sprintf(msgStatus, "JA EXISTE UM UPLOAD EM ANDAMENTO!"); msgTimer = 180; return;
    }
    sprintf(msgStatus, "PREPARANDO MAQUINA DO TEMPO...");
    msgTimer = 180;

    pthread_t tUp;
    pthread_create(&tUp, NULL, threadBackupTodos, NULL);
    pthread_detach(tUp);
}

// ==============================================================
// MULTI-UPLOAD DOS ITENS SELECIONADOS NO EXPLORADOR
// ==============================================================
void* threadUploadSelecionados(void* arg) {
    unlink(caminhoFilaUpload);
    downloadEmSegundoPlano = true;
    progressoAtualDownload = 0.0f;
    totalFilaSessao = 0;
    baixadosFilaSessao = 0;

    sprintf(msgStatus, "MAPEANDO ARQUIVOS SELECIONADOS...");
    msgTimer = 300;

    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) {
            char* alvo = linksAtuais[i];
            int len = strlen(alvo);
            if (len > 0 && alvo[len - 1] == '/') {
                char limpo[512]; strcpy(limpo, alvo); limpo[len - 1] = '\0';
                char* folderName = strrchr(limpo, '/'); if (folderName) folderName++; else folderName = limpo;
                char remoteBase[1024]; sprintf(remoteBase, "/HyperNeiva_Uploads/%s", folderName);
                mapearPastaParaUploadRecursivo(limpo, limpo, remoteBase);
            }
            else {
                char* nome = strrchr(alvo, '/'); if (nome) nome++; else nome = alvo;
                char remote[1024]; sprintf(remote, "/HyperNeiva_Uploads/%s", nome);
                adicionarNaFilaUpload(alvo, remote);
                totalFilaSessao++;
            }
            marcados[i] = false;
        }
    }

    threadProcessarFilaUpload(NULL);
    return NULL;
}

void fazerUploadSelecionados() {
    if (processandoFilaUpload) {
        sprintf(msgStatus, "AGUARDE O FIM DO UPLOAD ATUAL!");
        msgTimer = 180;
        return;
    }
    pthread_t tUp;
    pthread_create(&tUp, NULL, threadUploadSelecionados, NULL);
    pthread_detach(tUp);
}

void fazerUploadDropbox(const char* localPath) {
    if (processandoFilaUpload) { sprintf(msgStatus, "AGUARDE O FIM DO UPLOAD ATUAL!"); msgTimer = 180; return; }

    int len = strlen(localPath);
    if (localPath[len - 1] == '/') { sprintf(msgStatus, "ERRO: SELECIONE UM ARQUIVO, NAO PASTA"); msgTimer = 180; return; }

    unlink(caminhoFilaUpload);
    char* nomeArquivo = strrchr(localPath, '/'); if (nomeArquivo) nomeArquivo++; else nomeArquivo = (char*)localPath;
    char remoteDest[1024]; sprintf(remoteDest, "/HyperNeiva_Uploads/%s", nomeArquivo);

    adicionarNaFilaUpload(localPath, remoteDest);
    totalFilaSessao = 1;
    baixadosFilaSessao = 0;

    pthread_t t;
    pthread_create(&t, NULL, threadProcessarFilaUpload, NULL);
    pthread_detach(t);
}

void* threadUploadPastaAtual(void* arg) {
    unlink(caminhoFilaUpload);
    char* dirPath = (char*)arg;

    downloadEmSegundoPlano = true;
    progressoAtualDownload = 0.0f;
    totalFilaSessao = 0;
    baixadosFilaSessao = 0;

    sprintf(msgStatus, "MAPEANDO PASTA...");
    msgTimer = 300;

    char* folderName = strrchr(dirPath, '/'); if (folderName) folderName++; else folderName = dirPath;
    char remoteBase[1024]; sprintf(remoteBase, "/HyperNeiva_Uploads/backup/%s", folderName);

    mapearPastaParaUploadRecursivo(dirPath, dirPath, remoteBase);
    free(arg);

    threadProcessarFilaUpload(NULL);
    return NULL;
}

void fazerUploadPastaRecursivo(const char* dirPath) {
    if (processandoFilaUpload) {
        sprintf(msgStatus, "AGUARDE O FIM DO UPLOAD ATUAL!"); msgTimer = 180; return;
    }
    char* pathCopy = strdup(dirPath);
    pthread_t t;
    pthread_create(&t, NULL, threadUploadPastaAtual, pathCopy);
    pthread_detach(t);
}

// ==============================================================
// MENU PRINCIPAL DE CAMINHOS COMPLETOS DO EXPLORER
// ==============================================================
void preencherMenuBackup() {
    memset(nomes, 0, sizeof(nomes));

    // Explore: Opções 0, 1, 2, 3
    strcpy(nomes[0], "Hyper Neiva");
    strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB 0");
    strcpy(nomes[3], "USB 1");
    // Backup Direto: Opções 4, 5, 6, 7, 8
    strcpy(nomes[4], "Backup Automatico (Tudo)");
    strcpy(nomes[5], "Backup Saves");
    strcpy(nomes[6], "Backup Metadados");
    strcpy(nomes[7], "Backup Perfis");
    strcpy(nomes[8], "Backup Banco de Dados");

    totalItens = 9;
    menuAtual = MENU_BAIXAR_DROPBOX_BACKUP;
    sel = 0;
    off = 0;
}

// O RESTO DO DOWNLOAD FUNCIONA NORMALMENTE!
void processarPastaRecursiva(const char* localRoot, const char* currentLocal) {}

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

            if (strcmp(linha, url) == 0) { existe = true; break; }
        }
        fclose(fCheck);
    }

    if (!existe) {
        FILE* f = fopen(caminhoFila, "a");
        if (f) { fprintf(f, "%s|%d|%s\n", url, (int)menuOrigem, xmlOrigem); fclose(f); totalFilaSessao++; }
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
            *p1 = '\0'; strcpy(urlOut, linha); p1++; char* p2 = strchr(p1, '|');
            if (p2) { *p2 = '\0'; *menuOut = (MenuLevel)atoi(p1); p2++; strcpy(xmlOut, p2); }
            else { *menuOut = (MenuLevel)atoi(p1); strcpy(xmlOut, ""); }
        }
        else { strcpy(urlOut, linha); *menuOut = (MenuLevel)0; strcpy(xmlOut, ""); }
        fclose(f); pthread_mutex_unlock(&filaMutex); return true;
    }
    fclose(f); pthread_mutex_unlock(&filaMutex); return false;
}

void removerPrimeiroDaFila() {
    pthread_mutex_lock(&filaMutex);
    FILE* f = fopen(caminhoFila, "r");
    if (!f) { pthread_mutex_unlock(&filaMutex); return; }

    FILE* fTemp = fopen(caminhoFilaTemp, "w");
    if (fTemp) {
        char linha[2048]; bool primeiro = true;
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

void acessarDropbox(const char* path) {
    char cleanPath[256]; strncpy(cleanPath, path, 255); cleanPath[255] = '\0';
    for (int i = 0; i < strlen(cleanPath); i++) { if (cleanPath[i] == '\r' || cleanPath[i] == '\n') cleanPath[i] = '\0'; }
    strcpy(currentDropboxPath, cleanPath);

    char token[2048] = { 0 };
    obterTokenValido(token);

    if (strlen(token) < 15) { sprintf(msgStatus, "ERRO: TOKEN NO TXT INVALIDO"); memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "Verifique o arquivo .txt"); totalItens = 1; menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0; return; }

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
                        } strcpy(nomes[0], "Erro na API"); totalItens = 1;
                    }
                    else {
                        char* p = h;
                        while ((p = strstr(p, "\".tag\": \"")) && totalItens < 2900) {
                            p += 9; bool isFolder = (strncmp(p, "folder", 6) == 0);
                            char* namePtr = strstr(p, "\"name\": \""); if (!namePtr) break; namePtr += 9; char* nameEnd = strchr(namePtr, '\"');
                            char* pathPtr = strstr(nameEnd, "\"path_display\": \""); if (!pathPtr) break; pathPtr += 17; char* pathEnd = strchr(pathPtr, '\"');
                            if (nameEnd && pathEnd) {
                                int nameLen = nameEnd - namePtr; int pathLen = pathEnd - pathPtr;
                                strncpy(nomes[totalItens], namePtr, nameLen); nomes[totalItens][nameLen] = '\0';
                                strncpy(linksAtuais[totalItens], pathPtr, pathLen);
                                if (isFolder) { linksAtuais[totalItens][pathLen] = '/'; linksAtuais[totalItens][pathLen + 1] = '\0'; }
                                else { linksAtuais[totalItens][pathLen] = '\0'; }
                                totalItens++;
                            } p = pathEnd;
                        }
                        if (totalItens > 0) sprintf(msgStatus, "PASTAS CARREGADAS!"); else { strcpy(nomes[0], "Pasta vazia"); totalItens = 1; }
                    } free(h);
                } fclose(f2);
            }
        }
    }
    else sprintf(msgStatus, "ERRO: FALHA DE REDE");

    atualizarBarra(1.0f); msgTimer = 240; sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0;
}

bool tentarBgft(const char* urlFinal, const char* nomeExibicao) {
    sprintf(msgStatus, "TENTANDO BGFT DO PS4..."); atualizarBarra(0.2f);
    uint64_t fileSize = 0; char contentId[40] = { 0 };
    int tpl = sceHttpCreateTemplate(httpCtxId, "Mozilla/5.0 (PlayStation 4 9.00)", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();
    int conn = sceHttpCreateConnectionWithURL(tpl, urlFinal, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, urlFinal, 0);
    sceHttpAddRequestHeader(req, "Range", "bytes=0-16383", 1);
    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        unsigned char headerBuf[16384]; memset(headerBuf, 0, sizeof(headerBuf)); int totalRead = 0;
        while (totalRead < 0x200) { int n = sceHttpReadData(req, headerBuf + totalRead, sizeof(headerBuf) - totalRead); if (n <= 0) break; totalRead += n; }
        if (totalRead >= 0x80 && headerBuf[0] == 0x7F && headerBuf[1] == 'C' && headerBuf[2] == 'N' && headerBuf[3] == 'T') {
            memcpy(contentId, headerBuf + 0x40, 36); contentId[36] = '\0';
            for (int i = 0; i < 36; i++) { if (contentId[i] < 32 || contentId[i] > 126) contentId[i] = '0'; }
            uint64_t sizePkg = 0; sizePkg |= ((uint64_t)headerBuf[0x18] << 56); sizePkg |= ((uint64_t)headerBuf[0x19] << 48); sizePkg |= ((uint64_t)headerBuf[0x1A] << 40); sizePkg |= ((uint64_t)headerBuf[0x1B] << 32); sizePkg |= ((uint64_t)headerBuf[0x1C] << 24); sizePkg |= ((uint64_t)headerBuf[0x1D] << 16); sizePkg |= ((uint64_t)headerBuf[0x1E] << 8);  sizePkg |= ((uint64_t)headerBuf[0x1F]); fileSize = sizePkg;
        }
        else { sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl); return false; }
    }
    else { sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl); return false; }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    bool idValido = (strlen(contentId) == 36); if (!idValido) return false;

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL); sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
    static void* bgftHeapWeb = NULL;
    if (!bgftHeapWeb) { sceAppInstUtilInitialize(); OrbisBgftInitParams bgftInit; memset(&bgftInit, 0, sizeof(OrbisBgftInitParams)); bgftInit.heapSize = 2 * 1024 * 1024; bgftHeapWeb = memalign(4096, bgftInit.heapSize); bgftInit.heap = bgftHeapWeb; sceBgftServiceIntInit(&bgftInit); }

    OrbisBgftTaskId tarefaAntiga = -1; sceBgftServiceIntDownloadFindActiveTask(contentId, ORBIS_BGFT_TASK_SUB_TYPE_PACKAGE, &tarefaAntiga);
    if (tarefaAntiga != -1) sceBgftServiceIntDownloadUnregisterTask(tarefaAntiga);
    int32_t userId = 0; sceUserServiceGetInitialUser(&userId);

    OrbisBgftDownloadParam params; memset(&params, 0, sizeof(OrbisBgftDownloadParam));
    params.userId = userId; params.entitlementType = 5; params.id = contentId; params.contentUrl = urlFinal; params.contentName = nomeExibicao; params.playgoScenarioId = "0"; params.packageSize = fileSize;
    OrbisBgftTaskId taskId = -1;
    if (sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId) == 0 && taskId >= 0) { sceBgftServiceDownloadStartTask(taskId); sprintf(msgStatus, "DOWNLOAD DA WEB INICIADO!"); atualizarBarra(1.0f); msgTimer = 400; return true; }
    return false;
}

void* threadDownloadBackground(void* arg) {
    char urlDownloadBg[1024]; MenuLevel menuOrigemBg; char caminhoXMLOrigemBg[256];
    while (obterPrimeiroDaFila(urlDownloadBg, &menuOrigemBg, caminhoXMLOrigemBg)) {
        char nomeArquivo[128] = "arquivo.bin"; char* ref = strrchr(urlDownloadBg, '/'); if (ref) { strncpy(nomeArquivo, ref + 1, 127); nomeArquivo[127] = '\0'; }
        char* ptrInterrogacao = strchr(nomeArquivo, '?'); if (ptrInterrogacao) *ptrInterrogacao = '\0';
        char* ptrHash = strchr(nomeArquivo, '#'); if (ptrHash) *ptrHash = '\0';
        char nomeLimpo[128] = { 0 }; int j = 0;
        for (int i = 0; nomeArquivo[i] != '\0' && j < 127; i++) { if (nomeArquivo[i] == '%' && nomeArquivo[i + 1] == '2' && nomeArquivo[i + 2] == '0') { nomeLimpo[j++] = ' '; i += 2; } else { nomeLimpo[j++] = nomeArquivo[i]; } }
        strcpy(nomeArquivo, nomeLimpo);

        char pathPasta[512]; sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
        char* ext = strrchr(nomeArquivo, '.');
        if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) { sceKernelMkdir("/data/pkg", 0777); sprintf(pathPasta, "/data/pkg"); }
        else if (menuOrigemBg == MENU_BAIXAR_DROPBOX_LISTA || menuOrigemBg == MENU_BAIXAR_DROPBOX_BACKUP) { sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/dropbox"); }
        else if (menuOrigemBg == MENU_BAIXAR_LINKS) {
            char nomeRepo[128] = "Games"; char* refBarra = strrchr(caminhoXMLOrigemBg, '/');
            if (refBarra) { strncpy(nomeRepo, refBarra + 1, 127); char* dot = strrchr(nomeRepo, '.'); if (dot) *dot = '\0'; }
            sceKernelMkdir("/data/HyperNeiva/baixado/repositorios", 0777); sceKernelMkdir("/data/HyperNeiva/baixado/repositorios/games", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/repositorios/games/%s", nomeRepo); sceKernelMkdir(pathPasta, 0777);
        }
        else { sceKernelMkdir("/data/HyperNeiva/baixado/linkdireto", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/linkdireto"); }

        char pathFinal[1024]; sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);
        progressoAtualDownload = 0.0f; strcpy(msgDownloadBg, "CONECTANDO...");

        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1); sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect(); int conn, req;
        if (urlDownloadBg[0] == '/') {
            char token[2048] = { 0 };
            obterTokenValido(token);
            const char* apiUrl = "https://content.dropboxapi.com/2/files/download"; conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);
            char authHeader[2048]; sprintf(authHeader, "Bearer %s", token); sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
            char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", urlDownloadBg); sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);
        }
        else {
            char urlTratada[1024]; strcpy(urlTratada, urlDownloadBg); char* dbox = strstr(urlTratada, "dropbox.com"); if (dbox) { char* dl0 = strstr(urlTratada, "?dl=0"); if (dl0) dl0[4] = '1'; }
            conn = sceHttpCreateConnectionWithURL(tpl, urlTratada, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, urlTratada, 0);
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
                        if (tamanhoTotal > 0) { progressoAtualDownload = (float)baixado / (float)tamanhoTotal; sprintf(msgTemp, "BAIXANDO: %s", nomeArquivo); }
                        else { progressoAtualDownload = 0.5f; sprintf(msgTemp, "BAIXANDO: %s... (%.2f MB)", nomeArquivo, (float)baixado / 1048576.0f); }
                        strcpy(msgDownloadBg, msgTemp); sceKernelUsleep(1000);
                    } free(buf);
                } fclose(f);
                if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) { sprintf(msgStatus, "DOWNLOAD CONCLUIDO! INSTALANDO..."); msgTimer = 300; instalarPkgLocal(pathFinal); }
                else { sprintf(msgStatus, "ARQUIVO CONCLUIDO!"); msgTimer = 300; }
            }
        }
        else { sprintf(msgStatus, "ERRO NO ARQUIVO ATUAL!"); msgTimer = 240; }

        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
        removerPrimeiroDaFila(); baixadosFilaSessao++;
    }
    totalFilaSessao = 0; baixadosFilaSessao = 0; downloadEmSegundoPlano = false; return NULL;
}

void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 2) return;
    char nomeArquivo[128] = "arquivo.bin"; char* ref = strrchr(url, '/'); if (ref) { strncpy(nomeArquivo, ref + 1, 127); nomeArquivo[127] = '\0'; }
    char* ptrInterrogacao = strchr(nomeArquivo, '?'); if (ptrInterrogacao) *ptrInterrogacao = '\0';
    char* ptrHash = strchr(nomeArquivo, '#'); if (ptrHash) *ptrHash = '\0';
    char nomeLimpo[128] = { 0 }; int j = 0;
    for (int i = 0; nomeArquivo[i] != '\0' && j < 127; i++) { if (nomeArquivo[i] == '%' && nomeArquivo[i + 1] == '2' && nomeArquivo[i + 2] == '0') { nomeLimpo[j++] = ' '; i += 2; } else { nomeLimpo[j++] = nomeArquivo[i]; } }
    strcpy(nomeArquivo, nomeLimpo);
    char* ext = strrchr(nomeArquivo, '.');
    if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) {
        bool usarBgft = true; if (url[0] == '/' || strstr(url, "dropbox.com") != NULL) { usarBgft = false; }
        if (usarBgft) { if (tentarBgft(url, nomeArquivo)) return; sprintf(msgStatus, "BGFT FALHOU. USANDO DOWNLOADER INTERNO..."); msgTimer = 180; sceKernelUsleep(1000 * 1000); }
        else { sprintf(msgStatus, "LINK DROPBOX: USANDO DOWNLOADER INTERNO..."); msgTimer = 180; }
    }
    int antes = obterTamanhoFila();
    if (!downloadEmSegundoPlano) { totalFilaSessao = antes; baixadosFilaSessao = 0; }
    adicionarNaFila(url, menuAtual, caminhoXMLAtual);
    int depois = obterTamanhoFila();
    if (depois == antes) { sprintf(msgStatus, "LINK JA EXISTE NA FILA!"); msgTimer = 180; }
    else { sprintf(msgStatus, "ADICIONADO A FILA!"); msgTimer = 180; }
    if (!downloadEmSegundoPlano) { downloadEmSegundoPlano = true; progressoAtualDownload = 0.0f; strcpy(msgDownloadBg, "PREPARANDO O DOWNLOAD..."); pthread_t threadId; pthread_create(&threadId, NULL, threadDownloadBackground, NULL); pthread_detach(threadId); }
}

void* threadAdicionarSelecionados(void* arg) {
    processandoFila = true; int countAdicionados = 0; int countRepetidos = 0; pthread_mutex_lock(&filaMutex);
    char* conteudoFila = NULL; FILE* fCheck = fopen(caminhoFila, "r");
    if (fCheck) { fseek(fCheck, 0, SEEK_END); long sz = ftell(fCheck); fseek(fCheck, 0, SEEK_SET); if (sz > 0) { conteudoFila = (char*)malloc(sz + 1); fread(conteudoFila, 1, sz, fCheck); conteudoFila[sz] = '\0'; } fclose(fCheck); }
    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777); FILE* fAppend = fopen(caminhoFila, "a");
    for (int i = 0; i < totalItens; i++) {
        if (marcados[i]) {
            char* alvo = linksAtuais[i]; int len = strlen(alvo);
            if (len > 0 && alvo[len - 1] == '/') { pthread_mutex_unlock(&filaMutex); char limpo[512]; strcpy(limpo, alvo); limpo[len - 1] = '\0'; pthread_mutex_lock(&filaMutex); }
            else {
                bool existe = false; if (conteudoFila && strstr(conteudoFila, alvo) != NULL) existe = true;
                if (!existe) { if (fAppend) { fprintf(fAppend, "%s|%d|%s\n", alvo, (int)menuAtual, caminhoXMLAtual); totalFilaSessao++; countAdicionados++; } }
                else { countRepetidos++; }
            } marcados[i] = false;
        }
    }
    if (fAppend) fclose(fAppend); if (conteudoFila) free(conteudoFila); pthread_mutex_unlock(&filaMutex);
    if (countAdicionados > 0) {
        if (countRepetidos > 0) sprintf(msgStatus, "+%d NA FILA! (%d REPETIDOS)", countAdicionados, countRepetidos); else sprintf(msgStatus, "+%d ARQUIVOS ADICIONADOS A FILA!", countAdicionados);
        msgTimer = 240; if (!downloadEmSegundoPlano) { downloadEmSegundoPlano = true; progressoAtualDownload = 0.0f; strcpy(msgDownloadBg, "PREPARANDO O DOWNLOAD..."); pthread_t tDl; pthread_create(&tDl, NULL, threadDownloadBackground, NULL); pthread_detach(tDl); }
    }
    else if (countRepetidos > 0) { sprintf(msgStatus, "TODOS JA ESTAO NA FILA!"); msgTimer = 180; }
    processandoFila = false; return NULL;
}

void fazerDownloadSelecionados() { if (processandoFila) { sprintf(msgStatus, "AGUARDE, JA ESTAMOS PROCESSANDO..."); msgTimer = 180; return; } sprintf(msgStatus, "PROCESSANDO ITENS MARCADOS..."); msgTimer = 180; pthread_t threadId; pthread_create(&threadId, NULL, threadAdicionarSelecionados, NULL); pthread_detach(threadId); }

void listarArquivosUpload(const char* dirPath) {
    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;
    strcpy(currentUploadPath, dirPath);
    DIR* d = opendir(dirPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
            strcpy(nomes[totalItens], dir->d_name); sprintf(linksAtuais[totalItens], "%s/%s", dirPath, dir->d_name);
            struct stat st; char full[1024]; sprintf(full, "%s/%s", dirPath, dir->d_name);
            if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(full, &st) == 0 && S_ISDIR(st.st_mode))) { strcat(nomes[totalItens], " (Pasta)"); strcat(linksAtuais[totalItens], "/"); }
            totalItens++; if (totalItens >= 2900) break;
        } closedir(d);
    }
    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia / Acesso Negado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_DROPBOX_UPLOAD; sel = 0; off = 0; sprintf(msgStatus, "SELECIONE UM ARQUIVO PARA ENVIAR");
}

void fazerDownloadArquivoDaNuvem(const char* remotePath, const char* localPath) {
    char token[2048] = { 0 };
    obterTokenValido(token);
    if (strlen(token) < 15) return;
    char* nomeArquivo = strrchr(remotePath, '/'); if (nomeArquivo) nomeArquivo++; else nomeArquivo = (char*)remotePath;
    sprintf(msgStatus, "BAIXANDO: %s", nomeArquivo); atualizarBarra(0.5f);
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1); sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();
    const char* apiUrl = "https://content.dropboxapi.com/2/files/download"; int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1); int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);
    char authHeader[2048]; sprintf(authHeader, "Bearer %s", token); sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
    char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", remotePath); sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);
    if (sceHttpSendRequest(req, NULL, 0) >= 0) { FILE* f = fopen(localPath, "wb"); if (f) { unsigned char buf[32768]; int n; while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f); fclose(f); } }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void processarDownloadPastaRecursiva(const char* remotePath, const char* localPath) {
    char token[2048] = { 0 };
    obterTokenValido(token);
    if (strlen(token) < 15) return;
    sceKernelMkdir(localPath, 0777);
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1); sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();
    const char* apiUrl = "https://api.dropboxapi.com/2/files/list_folder"; int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1); int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);
    char postData[512]; sprintf(postData, "{\"path\": \"%s\"}", remotePath); char authHeader[2048]; sprintf(authHeader, "Bearer %s", token);
    sceHttpAddRequestHeader(req, "Authorization", authHeader, 1); sceHttpAddRequestHeader(req, "Content-Type", "application/json; charset=utf-8", 1); sceHttpSetRequestContentLength(req, strlen(postData));
    sprintf(msgStatus, "LENDO PASTA: %s", remotePath); atualizarBarra(0.3f);
    if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
        sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777); FILE* f = fopen("/data/HyperNeiva/configuracao/temporario/temp_down_folder.json", "wb");
        if (f) {
            unsigned char buf[32768]; int n; while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f); fclose(f);
            FILE* f2 = fopen("/data/HyperNeiva/configuracao/temporario/temp_down_folder.json", "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET); char* h = (char*)malloc(sz + 1);
                if (h) {
                    fread(h, 1, sz, f2); h[sz] = '\0'; char* p = h;
                    char** subFolders = (char**)malloc(100 * sizeof(char*)); char** files = (char**)malloc(200 * sizeof(char*)); char** subFoldersNames = (char**)malloc(100 * sizeof(char*)); char** filesNames = (char**)malloc(200 * sizeof(char*));
                    for (int i = 0; i < 100; i++) { subFolders[i] = (char*)malloc(512); subFoldersNames[i] = (char*)malloc(256); }
                    for (int i = 0; i < 200; i++) { files[i] = (char*)malloc(512); filesNames[i] = (char*)malloc(256); }
                    int numSubFolders = 0; int numFiles = 0;
                    while ((p = strstr(p, "\".tag\": \""))) {
                        p += 9; bool isFolder = (strncmp(p, "folder", 6) == 0); char* namePtr = strstr(p, "\"name\": \""); if (!namePtr) break; namePtr += 9; char* nameEnd = strchr(namePtr, '\"'); char* pathPtr = strstr(nameEnd, "\"path_display\": \""); if (!pathPtr) break; pathPtr += 17; char* pathEnd = strchr(pathPtr, '\"');
                        if (nameEnd && pathEnd) {
                            int nameLen = nameEnd - namePtr; int pathLen = pathEnd - pathPtr;
                            if (isFolder && numSubFolders < 100) { strncpy(subFolders[numSubFolders], pathPtr, pathLen); subFolders[numSubFolders][pathLen] = '\0'; strncpy(subFoldersNames[numSubFolders], namePtr, nameLen); subFoldersNames[numSubFolders][nameLen] = '\0'; numSubFolders++; }
                            else if (!isFolder && numFiles < 200) { strncpy(files[numFiles], pathPtr, pathLen); files[numFiles][pathLen] = '\0'; strncpy(filesNames[numFiles], namePtr, nameLen); filesNames[numFiles][nameLen] = '\0'; numFiles++; }
                        } p = pathEnd;
                    } free(h);
                    for (int i = 0; i < numSubFolders; i++) { char nextLocal[512]; sprintf(nextLocal, "%s/%s", localPath, subFoldersNames[i]); processarDownloadPastaRecursiva(subFolders[i], nextLocal); }
                    for (int i = 0; i < numFiles; i++) { char nextLocal[512]; sprintf(nextLocal, "%s/%s", localPath, filesNames[i]); fazerDownloadArquivoDaNuvem(files[i], nextLocal); }
                    for (int i = 0; i < 100; i++) { free(subFolders[i]); free(subFoldersNames[i]); } for (int i = 0; i < 200; i++) { free(files[i]); free(filesNames[i]); }
                    free(subFolders); free(files); free(subFoldersNames); free(filesNames);
                } fclose(f2);
            }
        }
    } sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void fazerDownloadPastaRecursivo(const char* remotePath, const char* folderName) {
    sprintf(msgStatus, "INICIANDO DOWNLOAD DA PASTA..."); atualizarBarra(0.1f);
    char localRoot[512]; sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777); sprintf(localRoot, "/data/HyperNeiva/baixado/dropbox/%s", folderName);
    processarDownloadPastaRecursiva(remotePath, localRoot);
    sprintf(msgStatus, "DOWNLOAD DE PASTA CONCLUIDO!"); atualizarBarra(1.0f); msgTimer = 240;
}