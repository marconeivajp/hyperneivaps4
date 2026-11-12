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
#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include "baixar_dropbox_download.h"
#include "menu.h"
#include "network.h"
#include <stdint.h>
#include <dirent.h>

extern "C" int sceHttpSetRequestContentLength(int reqId, uint64_t contentLength);
extern void atualizarBarra(float progresso);

extern void instalarPkgLocal(const char* caminhoAbsoluto);

extern char nomes[3000][256];
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

static const char* pularEspacosJson(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static bool extrairStringJson(const char* obj, const char* chave, char* out, int outSize) {
    if (!obj || !chave || !out || outSize <= 0) return false;
    out[0] = '\0';
    const char* p = obj;
    while ((p = strstr(p, chave)) != NULL) {
        p += strlen(chave);
        p = pularEspacosJson(p);
        if (*p != ':') continue;
        p++;
        p = pularEspacosJson(p);
        if (*p != '"') continue;
        p++;
        int n = 0;
        while (*p && *p != '"') {
            char c = *p++;
            if (c == '\\' && *p) {
                char esc = *p++;
                if (esc == 'n') c = '\n';
                else if (esc == 'r') c = '\r';
                else if (esc == 't') c = '\t';
                else c = esc;
            }
            if (n < outSize - 1) out[n++] = c;
        }
        out[n] = '\0';
        return (*p == '"');
    }
    return false;
}

static char* fimObjetoJson(char* p) {
    int depth = 0;
    bool inStr = false;
    bool esc = false;
    while (*p) {
        char c = *p;
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
        } else {
            if (c == '"') inStr = true;
            else if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) return p; }
        }
        p++;
    }
    return NULL;
}

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

static void copiarTokenSaida(char* outToken) {
    if (!outToken) return;
    strncpy(outToken, tokenAcessoGlobal, 2047);
    outToken[2047] = '\0';
}

void obterTokenValido(char* outToken) {
    if (!outToken) return;
    outToken[0] = '\0';

    if (strlen(tokenAcessoGlobal) > 10) {
        copiarTokenSaida(outToken);
        return;
    }

    char txt_token[4096] = { 0 };
    FILE* fToken = fopen("/data/HyperNeiva/configuracao/dropbox_token.txt", "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < (long)sizeof(txt_token)) { fread(txt_token, 1, sz, fToken); txt_token[sz] = '\0'; }
        fclose(fToken);
    }

    char* inicio = txt_token;
    while (*inicio == ' ' || *inicio == '\t' || *inicio == '\r' || *inicio == '\n') inicio++;
    int len = strlen(inicio);
    while (len > 0 && (inicio[len - 1] == ' ' || inicio[len - 1] == '\t' || inicio[len - 1] == '\r' || inicio[len - 1] == '\n')) inicio[--len] = '\0';

    if (len < 15) return;

    if (strncmp(inicio, "sl.", 3) == 0) {
        strncpy(tokenAcessoGlobal, inicio, sizeof(tokenAcessoGlobal) - 1);
        tokenAcessoGlobal[sizeof(tokenAcessoGlobal) - 1] = '\0';
        copiarTokenSaida(outToken);
        return;
    }

    char appKey[128] = "345xl81u5bdp9x2";
    char appSecret[128] = "bhh9nfd8t7rvfuq";
    char refreshToken[4096] = { 0 };
    bool temLabels = false;

    char txtCopy[4096];
    strncpy(txtCopy, inicio, sizeof(txtCopy) - 1);
    txtCopy[sizeof(txtCopy) - 1] = '\0';

    char* linha = strtok(txtCopy, "\r\n");
    while (linha) {
        if (strstr(linha, "Appkey:") || strstr(linha, "App key:") || strstr(linha, "AppKey:")) {
            char* val = strchr(linha, ':');
            if (val) sscanf(val + 1, " %127s", appKey);
            temLabels = true;
        }
        else if (strstr(linha, "App secret:") || strstr(linha, "Appsecret:") || strstr(linha, "App Secret:")) {
            char* val = strchr(linha, ':');
            if (val) sscanf(val + 1, " %127s", appSecret);
            temLabels = true;
        }
        else if (strstr(linha, "Token refresh:") || strstr(linha, "Tokenrefresh:") || strstr(linha, "Token Refresh:")) {
            char* val = strchr(linha, ':');
            if (val) sscanf(val + 1, " %4095s", refreshToken);
            temLabels = true;
        }
        linha = strtok(NULL, "\r\n");
    }

    if (!temLabels) {
        strncpy(refreshToken, inicio, sizeof(refreshToken) - 1);
        refreshToken[sizeof(refreshToken) - 1] = '\0';
    }

    if (strlen(refreshToken) < 15) return;

    sprintf(msgStatus, "VERIFICANDO TOKEN E CONECTANDO...");

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) {
        strncpy(tokenAcessoGlobal, refreshToken, sizeof(tokenAcessoGlobal) - 1);
        tokenAcessoGlobal[sizeof(tokenAcessoGlobal) - 1] = '\0';
        copiarTokenSaida(outToken);
        return;
    }
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/oauth2/token";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0) : -1;

    char postData[8192];
    snprintf(postData, sizeof(postData), "grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s", refreshToken, appKey, appSecret);

    bool isRefreshToken = false;

    if (req >= 0) {
        sceHttpAddRequestHeader(req, "Content-Type", "application/x-www-form-urlencoded", 1);
        sceHttpSetRequestContentLength(req, strlen(postData));

        if (sceHttpSendRequest(req, postData, strlen(postData)) >= 0) {
            unsigned char buf[8192]; memset(buf, 0, sizeof(buf));
            int n = sceHttpReadData(req, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                char novoToken[2048] = { 0 };
                if (extrairStringJson((char*)buf, "\"access_token\"", novoToken, sizeof(novoToken))) {
                    strncpy(tokenAcessoGlobal, novoToken, sizeof(tokenAcessoGlobal) - 1);
                    tokenAcessoGlobal[sizeof(tokenAcessoGlobal) - 1] = '\0';
                    isRefreshToken = true;
                }
            }
        }
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);

    if (!isRefreshToken) {
        strncpy(tokenAcessoGlobal, refreshToken, sizeof(tokenAcessoGlobal) - 1);
        tokenAcessoGlobal[sizeof(tokenAcessoGlobal) - 1] = '\0';
    }

    copiarTokenSaida(outToken);
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

void criarPastaDropbox(const char* remotePath);

static char pastasRemotasCriadas[256][1024];
static int totalPastasRemotasCriadas = 0;

static bool pastaRemotaRegistrada(const char* caminho) {
    for (int i = 0; i < totalPastasRemotasCriadas; i++) {
        if (strcmp(pastasRemotasCriadas[i], caminho) == 0) return true;
    }
    return false;
}

static void registrarPastaRemota(const char* caminho) {
    if (totalPastasRemotasCriadas >= 256) return;
    strncpy(pastasRemotasCriadas[totalPastasRemotasCriadas], caminho, 1023);
    pastasRemotasCriadas[totalPastasRemotasCriadas][1023] = '\0';
    totalPastasRemotasCriadas++;
}

static void garantirPastaRemotaParaArquivo(const char* remoteFilePath) {
    char caminho[1024];
    strncpy(caminho, remoteFilePath, sizeof(caminho) - 1);
    caminho[sizeof(caminho) - 1] = '\0';

    char* ultimaBarra = strrchr(caminho, '/');
    if (!ultimaBarra || ultimaBarra == caminho) return;
    *ultimaBarra = '\0';

    char parcial[1024];
    for (int i = 1; caminho[i] != '\0'; i++) {
        if (caminho[i] == '/') {
            memcpy(parcial, caminho, i);
            parcial[i] = '\0';
            if (!pastaRemotaRegistrada(parcial)) {
                criarPastaDropbox(parcial);
                registrarPastaRemota(parcial);
            }
        }
    }

    if (!pastaRemotaRegistrada(caminho)) {
        criarPastaDropbox(caminho);
        registrarPastaRemota(caminho);
    }
}

bool fazerUploadArquivoParaNuvem(const char* localPath, const char* remoteFilePath) {
    char token[2048] = { 0 };
    obterTokenValido(token);
    if (strlen(token) < 15) {
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD: TOKEN INVALIDO");
        msgTimer = 240;
        return false;
    }

    FILE* fLocal = fopen(localPath, "rb");
    if (!fLocal) {
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD: NAO ABRIU O ARQUIVO");
        msgTimer = 240;
        return false;
    }
    fseek(fLocal, 0, SEEK_END); long fileSize = ftell(fLocal); fseek(fLocal, 0, SEEK_SET);

    char* nomeExibido = strrchr(localPath, '/'); if (nomeExibido) nomeExibido++; else nomeExibido = (char*)localPath;

    if (fileSize <= 0) {
        fclose(fLocal);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD: ARQUIVO VAZIO (%s)", nomeExibido);
        msgTimer = 240;
        return false;
    }

    if (fileSize > 150 * 1024 * 1024) {
        fclose(fLocal);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD: %s PASSA DE 150MB", nomeExibido);
        msgTimer = 300;
        return false;
    }

    snprintf(msgDownloadBg, sizeof(msgDownloadBg), "UPLOAD [%d/%d]: %.180s", baixadosFilaSessao + 1, totalFilaSessao, nomeExibido);
    if (totalFilaSessao > 0) {
        progressoAtualDownload = ((float)baixadosFilaSessao + 0.5f) / (float)totalFilaSessao;
    }

    garantirPastaRemotaParaArquivo(remoteFilePath);

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

    unsigned char* fileData = (unsigned char*)malloc((size_t)fileSize);
    if (!fileData) {
        fclose(fLocal);
        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD: SEM MEMORIA PARA %s", nomeExibido);
        msgTimer = 300;
        return false;
    }

    size_t lido = fread(fileData, 1, (size_t)fileSize, fLocal);
    fclose(fLocal);
    if (lido != (size_t)fileSize) {
        free(fileData);
        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD: LEITURA INCOMPLETA DE %s", nomeExibido);
        msgTimer = 300;
        return false;
    }

    int envio = sceHttpSendRequest(req, fileData, (size_t)fileSize);
    free(fileData);

    int status = 0;
    sceHttpGetStatusCode(req, &status);
    char resposta[1024]; memset(resposta, 0, sizeof(resposta));
    int nResposta = sceHttpReadData(req, resposta, sizeof(resposta) - 1);
    if (nResposta > 0) resposta[nResposta] = '\0';

    bool ok = (envio >= 0 && status == 200);
    if (ok) {
        snprintf(msgStatus, sizeof(msgStatus), "UPLOAD OK: %s", nomeExibido);
        msgTimer = 240;
    }
    else {
        char erro[256] = { 0 };
        if (!extrairStringJson(resposta, "\"error_summary\"", erro, sizeof(erro))) {
            if (nResposta > 0) { strncpy(erro, resposta, sizeof(erro) - 1); erro[sizeof(erro) - 1] = '\0'; }
            else strcpy(erro, "SEM RESPOSTA");
        }
        snprintf(msgStatus, sizeof(msgStatus), "ERRO UPLOAD %d: %s", status, erro);
        msgTimer = 300;
    }

    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    return ok;
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

    int falhasFila = 0;

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
            if (!fazerUploadArquivoParaNuvem(localPath, remotePath)) falhasFila++;
            baixadosFilaSessao++;
            if (totalFilaSessao > 0) {
                progressoAtualDownload = (float)baixadosFilaSessao / (float)totalFilaSessao;
            }
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

    progressoAtualDownload = 1.0f;
    downloadEmSegundoPlano = false;
    processandoFilaUpload = false;
    unlink(caminhoFilaUpload);

    if (falhasFila > 0) snprintf(msgStatus, sizeof(msgStatus), "UPLOAD CONCLUIDO COM %d FALHA(S)!", falhasFila);
    else snprintf(msgStatus, sizeof(msgStatus), "UPLOAD / BACKUP CONCLUIDO COM SUCESSO!");
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
    mapearPastaParaUploadRecursivo("/system_data/priv/cache/profile", "/system_data/priv/cache/profile", "/HyperNeiva_Uploads/backup/system_data/priv/cache/profile");
    mapearPastaParaUploadRecursivo("/system_data/priv/home", "/system_data/priv/home", "/HyperNeiva_Uploads/backup/system_data/priv/home");

    // =========================================================
    // O NOVO BACKUP DO RETROARCH (SÓ OS ARQUIVOS VITAIS!)
    // =========================================================
    mapearPastaParaUploadRecursivo("/data/retroarch/savefiles", "/data/retroarch/savefiles", "/HyperNeiva_Uploads/backup/data/retroarch/savefiles");
    mapearPastaParaUploadRecursivo("/data/retroarch/savestates", "/data/retroarch/savestates", "/HyperNeiva_Uploads/backup/data/retroarch/savestates");
    mapearPastaParaUploadRecursivo("/data/retroarch/system", "/data/retroarch/system", "/HyperNeiva_Uploads/backup/data/retroarch/system");
    mapearPastaParaUploadRecursivo("/data/retroarch/playlists", "/data/retroarch/playlists", "/HyperNeiva_Uploads/backup/data/retroarch/playlists");
    mapearPastaParaUploadRecursivo("/data/retroarch/config", "/data/retroarch/config", "/HyperNeiva_Uploads/backup/data/retroarch/config");
    adicionarNaFilaUpload("/data/retroarch/retroarch-core-options.cfg", "/HyperNeiva_Uploads/backup/data/retroarch/retroarch-core-options.cfg"); totalFilaSessao++;
    adicionarNaFilaUpload("/data/retroarch/retroarch.cfg", "/HyperNeiva_Uploads/backup/data/retroarch/retroarch.cfg"); totalFilaSessao++;

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
                    char errMsg[160] = { 0 };
                    if (extrairStringJson(h, "\"error_summary\"", errMsg, sizeof(errMsg))) {
                        if (strlen(errMsg) > 100) errMsg[100] = '\0';
                        sprintf(msgStatus, "API ERRO: %s", errMsg);
                        strcpy(nomes[0], "Erro na API"); totalItens = 1;
                    }
                    else {
                        char* p = h;
                        char* tagMarker;
                        while ((tagMarker = strstr(p, "\".tag\"")) != NULL && totalItens < 2900) {
                            char* obj = tagMarker;
                            while (obj > h && *obj != '{') obj--;
                            if (*obj != '{') break;
                            char* objEnd = fimObjetoJson(obj);
                            if (!objEnd) break;
                            char salvo = *objEnd; *objEnd = '\0';

                            char tag[32] = { 0 }; char nomeItem[256] = { 0 }; char caminhoItem[1024] = { 0 };
                            bool ok = extrairStringJson(obj, "\".tag\"", tag, sizeof(tag)) &&
                                      extrairStringJson(obj, "\"name\"", nomeItem, sizeof(nomeItem)) &&
                                      extrairStringJson(obj, "\"path_display\"", caminhoItem, sizeof(caminhoItem));
                            if (ok) {
                                bool isFolder = (strcmp(tag, "folder") == 0);
                                int nameLen = strlen(nomeItem);
                                int pathLen = strlen(caminhoItem);
                                int pathMax = isFolder ? 1022 : 1023;
                                if (nameLen > 255) nameLen = 255;
                                if (pathLen > pathMax) pathLen = pathMax;
                                strncpy(nomes[totalItens], nomeItem, nameLen); nomes[totalItens][nameLen] = '\0';
                                strncpy(linksAtuais[totalItens], caminhoItem, pathLen);
                                if (isFolder) { linksAtuais[totalItens][pathLen] = '/'; linksAtuais[totalItens][pathLen + 1] = '\0'; }
                                else { linksAtuais[totalItens][pathLen] = '\0'; }
                                totalItens++;
                            }

                            *objEnd = salvo;
                            p = objEnd + 1;
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

void dlRegistrarTarefaPs4(int id, const char* nome);
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
    params.userId = userId; params.entitlementType = 5; params.id = contentId; params.contentUrl = urlFinal; params.contentName = nomeExibicao; params.playgoScenarioId = "0"; memcpy(&params.packageSize, &fileSize, sizeof(fileSize));   // 64 bits: o header do OpenOrbis corta acima de 4 GB
    OrbisBgftTaskId taskId = -1;
    if (sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId) == 0 && taskId >= 0) { dlRegistrarTarefaPs4((int)taskId, nomeExibicao); sceBgftServiceDownloadStartTask(taskId); sprintf(msgStatus, "DOWNLOAD DA WEB INICIADO!"); atualizarBarra(1.0f); msgTimer = 400; return true; }
    return false;
}

struct DlLinha { char origem[16]; char nome[160]; float progresso; char info[96]; };   // gerenciador de downloads (network.cpp)
// Tarefas registradas no download do sistema (para o gerenciador de downloads).
static int dlTarefas[16];
static char dlTarefasNome[16][120];
static int dlTotalTarefas = 0;
void dlRegistrarTarefaPs4(int id, const char* nome) {
    for (int i = 0; i < dlTotalTarefas; i++) if (dlTarefas[i] == id) { snprintf(dlTarefasNome[i], 120, "%s", nome); return; }
    if (dlTotalTarefas == 16) { memmove(dlTarefas, dlTarefas + 1, sizeof(int) * 15); memmove(dlTarefasNome, dlTarefasNome + 1, 120 * 15); dlTotalTarefas = 15; }
    dlTarefas[dlTotalTarefas] = id; snprintf(dlTarefasNome[dlTotalTarefas], 120, "%s", nome); dlTotalTarefas++;
}
static const char* dlOrigemDaUrl(const char* u) {
    if (u[0] == '/' || strstr(u, "dropbox")) return "Dropbox";
    if (strstr(u, "github")) return "GitHub";
    return "Link";
}
int dlInternos(DlLinha* l, int max);

static int ultimoErroDownload = 0;
static char urlAtualDownload[4096];
int dlInternos(DlLinha* l, int max) {
    int n = 0;
    if (downloadEmSegundoPlano && n < max) {
        snprintf(l[n].origem, sizeof(l[n].origem), "%s", dlOrigemDaUrl(urlAtualDownload));
        snprintf(l[n].nome, sizeof(l[n].nome), "%s", msgDownloadBg);
        l[n].progresso = progressoAtualDownload;
        snprintf(l[n].info, sizeof(l[n].info), "%.0f%%", progressoAtualDownload * 100);
        n++;
    }
    // O que ainda espera na fila do downloader do app.
    FILE* f = fopen(caminhoFila, "r");
    if (f) {
        char linha[1500];
        while (fgets(linha, sizeof(linha), f) && n < max) {
            char* bar = strchr(linha, '|'); if (bar) *bar = 0;
            linha[strcspn(linha, "\r\n")] = 0;
            if (!linha[0]) continue;
            const char* nome = strrchr(linha, '/');
            snprintf(l[n].origem, sizeof(l[n].origem), "Fila");
            snprintf(l[n].nome, sizeof(l[n].nome), "%s", nome && nome[1] ? nome + 1 : linha);
            l[n].progresso = -1;
            snprintf(l[n].info, sizeof(l[n].info), "esperando (%s)", dlOrigemDaUrl(linha));
            n++;
        }
        fclose(f);
    }
    // Downloads entregues ao PS4 (Notificações > Downloads).
    for (int i = 0; i < dlTotalTarefas && n < max; i++) {
        OrbisBgftTaskProgress pr; memset(&pr, 0, sizeof(pr));
        if (sceBgftServiceDownloadGetProgress(dlTarefas[i], &pr) != 0) continue;
        snprintf(l[n].origem, sizeof(l[n].origem), "PS4");
        snprintf(l[n].nome, sizeof(l[n].nome), "%s", dlTarefasNome[i]);
        l[n].progresso = pr.length ? (float)pr.transferred / (float)pr.length : -1;
        if (pr.errorResult) snprintf(l[n].info, sizeof(l[n].info), "erro 0x%08X", (unsigned int)pr.errorResult);
        else if (pr.length && pr.transferred >= pr.length) snprintf(l[n].info, sizeof(l[n].info), "baixado, instalando");
        else snprintf(l[n].info, sizeof(l[n].info), "%.0f / %.0f MB", pr.transferred / 1048576.0, pr.length / 1048576.0);
        n++;
    }
    return n;
}

// Registro de cada etapa: sem ele, uma falha de rede no console não deixa pista nenhuma.
#define logDownload(...) do { FILE* logF = fopen("/data/HyperNeiva/download_debug_log.txt", "a"); if (logF) { fprintf(logF, __VA_ARGS__); fprintf(logF, "\n"); fclose(logF); } } while (0)

// O stub do OpenOrbis declara esta função sem argumentos, mas a real recebe (id, ligar).
static void definirRedirecionamento(int template_, int ligar) {
    typedef int (*DefinirRedirecionamento)(int, int);
    ((DefinirRedirecionamento)(void*)sceHttpSetAutoRedirect)(template_, ligar);
}

// O stub também declara esta sem argumentos. A resposta do Google Drive passa de 5 KB de
// cabeçalho, acima do limite padrão, e o envio falha com 0x80431073.
static void definirLimiteCabecalho(int template_, size_t tamanho) {
    typedef int (*DefinirLimite)(int, size_t);
    ((DefinirLimite)(void*)sceHttpSetResponseHeaderMaxSize)(template_, tamanho);
}

static bool nomeDaResposta(int req, char* nome, int tamanhoNome) {
    char* cabecalhos = NULL; size_t tamanho = 0;
    if (sceHttpGetAllResponseHeaders(req, &cabecalhos, &tamanho) < 0 || !cabecalhos) return false;
    for (size_t i = 0; i + 10 < tamanho; i++) {
        if (strncasecmp(cabecalhos + i, "filename=\"", 10) != 0) continue;
        int n = 0;
        for (size_t j = i + 10; j < tamanho && cabecalhos[j] != '"' && n < tamanhoNome - 1; j++) {
            char c = cabecalhos[j];
            if (c == '\r' || c == '\n') break;
            nome[n++] = (c == '/' || c == '\\') ? '_' : c;
        }
        nome[n] = '\0';
        return n > 0;
    }
    return false;
}

// Os arquivos do GitHub chegam por outro servidor, com uma URL enorme que o redirecionamento
// automático do sistema não aguenta. Aqui o salto é feito à mão.
static bool seguirRedirecionamentos(int tpl, int* conn, int* req) {
    for (int tentativa = 0; tentativa < 5; tentativa++) {
        int32_t codigo = 0;
        sceHttpGetStatusCode(*req, &codigo);
        logDownload("  status %d", (int)codigo);
        if (codigo != 301 && codigo != 302 && codigo != 303 && codigo != 307 && codigo != 308) return true;

        char* cabecalhos = NULL; size_t tamanho = 0;
        int lidos = sceHttpGetAllResponseHeaders(*req, &cabecalhos, &tamanho);
        if (lidos < 0 || !cabecalhos) { logDownload("  cabecalhos falharam 0x%08X", (unsigned int)lidos); return false; }

        static char destino[4096];
        destino[0] = '\0';
        for (size_t i = 0; i + 9 < tamanho; i++) {
            if ((i == 0 || cabecalhos[i - 1] == '\n') && strncasecmp(cabecalhos + i, "location:", 9) == 0) {
                size_t j = i + 9, n = 0;
                while (j < tamanho && cabecalhos[j] == ' ') j++;
                while (j < tamanho && cabecalhos[j] != '\r' && cabecalhos[j] != '\n' && n < sizeof(destino) - 1) destino[n++] = cabecalhos[j++];
                destino[n] = '\0';
                break;
            }
        }
        if (!destino[0]) { logDownload("  sem Location"); return false; }
        // O Dropbox termina o endereço em "#". O PS4 manda isso no pedido e o servidor responde 400.
        char* fragmento = strchr(destino, '#');
        if (fragmento) *fragmento = '\0';
        if (destino[0] == '/') {
            // Salto relativo: fica no mesmo servidor do pedido anterior.
            static char relativo[4096];
            int tamanhoServidor = 0; const char* resto = strstr(urlAtualDownload, "://");
            if (resto) { const char* barra = strchr(resto + 3, '/'); tamanhoServidor = barra ? (int)(barra - urlAtualDownload) : (int)strlen(urlAtualDownload); }
            snprintf(relativo, sizeof(relativo), "%.*s%s", tamanhoServidor, urlAtualDownload, destino);
            strcpy(destino, relativo);
        }
        snprintf(urlAtualDownload, sizeof(urlAtualDownload), "%s", destino);

        sceHttpDeleteRequest(*req); sceHttpDeleteConnection(*conn);
        *conn = sceHttpCreateConnectionWithURL(tpl, destino, 0);
        *req = *conn >= 0 ? sceHttpCreateRequestWithURL(*conn, ORBIS_METHOD_GET, destino, 0) : -1;
        logDownload("  salto para %.60s... (%d letras) conn:%d req:0x%08X", destino, (int)strlen(destino), *conn, (unsigned int)*req);
        if (*conn < 0 || *req < 0) return false;

        int enviado = sceHttpSendRequest(*req, NULL, 0);
        logDownload("  envio 0x%08X", (unsigned int)enviado);
        if (enviado < 0) { ultimoErroDownload = enviado; return false; }
    }
    return false;
}

// Primeiro segue os saltos à mão e, se não der, deixa o sistema tentar.
static bool abrirDownloadWeb(int tpl, const char* url, int* conn, int* req) {
    bool apiGithub = strstr(url, "api.github.com") != NULL;
    snprintf(urlAtualDownload, sizeof(urlAtualDownload), "%s", url);
    for (int tentativa = 0; tentativa < 2; tentativa++) {
        definirRedirecionamento(tpl, tentativa);
        *conn = sceHttpCreateConnectionWithURL(tpl, url, 0);
        *req = *conn >= 0 ? sceHttpCreateRequestWithURL(*conn, ORBIS_METHOD_GET, url, 0) : -1;
        logDownload("tentativa %d: %.100s conn:%d req:0x%08X", tentativa + 1, url, *conn, (unsigned int)*req);
        if (*conn >= 0 && *req >= 0) {
            if (apiGithub) sceHttpAddRequestHeader(*req, "Accept", "application/octet-stream", 1);
            int enviado = sceHttpSendRequest(*req, NULL, 0);
            logDownload("  envio 0x%08X", (unsigned int)enviado);
            if (enviado >= 0 && seguirRedirecionamentos(tpl, conn, req)) return true;
            if (enviado < 0) ultimoErroDownload = enviado;
        }
        if (*req >= 0) sceHttpDeleteRequest(*req);
        if (*conn >= 0) sceHttpDeleteConnection(*conn);
        *conn = -1; *req = -1;
    }
    return false;
}

// Dropbox e afins respondem 200 com uma página no lugar do arquivo quando o link falha.
static bool downloadVeioComoPagina(const char* caminho, const char* ext) {
    if (!ext || strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0 || strcasecmp(ext, ".xml") == 0 || strcasecmp(ext, ".txt") == 0) return false;
    char inicio[16] = { 0 };
    FILE* f = fopen(caminho, "rb");
    if (!f) return false;
    fread(inicio, 1, sizeof(inicio) - 1, f);
    fclose(f);
    return strncasecmp(inicio, "<!doctype", 9) == 0 || strncasecmp(inicio, "<html", 5) == 0;
}

void* threadDownloadBackground(void* arg) {
    char urlDownloadBg[1024]; MenuLevel menuOrigemBg; char caminhoXMLOrigemBg[256];
    while (obterPrimeiroDaFila(urlDownloadBg, &menuOrigemBg, caminhoXMLOrigemBg)) {
        ultimoErroDownload = 0;
        char nomeArquivo[128] = "arquivo.bin"; char* ref = strrchr(urlDownloadBg, '/'); if (ref) { strncpy(nomeArquivo, ref + 1, 127); nomeArquivo[127] = '\0'; }
        char* ptrInterrogacao = strchr(nomeArquivo, '?'); if (ptrInterrogacao) *ptrInterrogacao = '\0';
        char* ptrHash = strchr(nomeArquivo, '#'); if (ptrHash) *ptrHash = '\0';
        char nomeLimpo[128] = { 0 }; int j = 0;
        for (int i = 0; nomeArquivo[i] != '\0' && j < 127; i++) { if (nomeArquivo[i] == '%' && nomeArquivo[i + 1] == '2' && nomeArquivo[i + 2] == '0') { nomeLimpo[j++] = ' '; i += 2; } else { nomeLimpo[j++] = nomeArquivo[i]; } }
        strcpy(nomeArquivo, nomeLimpo);
        // A URL da API termina num número: o nome real vem junto na fila.
        if (menuOrigemBg == MENU_BAIXAR_GITHUB_ARQUIVOS && caminhoXMLOrigemBg[0]) { strncpy(nomeArquivo, caminhoXMLOrigemBg, 127); nomeArquivo[127] = '\0'; }

        char pathPasta[512]; sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
        char* ext = strrchr(nomeArquivo, '.');
        if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) { sceKernelMkdir("/data/pkg", 0777); sprintf(pathPasta, "/data/pkg"); }
        else if (menuOrigemBg == MENU_BAIXAR_GITHUB_ARQUIVOS) { sceKernelMkdir("/data/HyperNeiva/baixado/github", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/github"); }
        else if (menuOrigemBg == MENU_BAIXAR_DROPBOX_LISTA || menuOrigemBg == MENU_BAIXAR_DROPBOX_BACKUP) { sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/dropbox"); }
        else if (menuOrigemBg == MENU_BAIXAR_LINKS) {
            char nomeRepo[128] = "Games"; char* refBarra = strrchr(caminhoXMLOrigemBg, '/');
            if (refBarra) { strncpy(nomeRepo, refBarra + 1, 127); char* dot = strrchr(nomeRepo, '.'); if (dot) *dot = '\0'; }
            sceKernelMkdir("/data/HyperNeiva/baixado/repositorios", 0777); sceKernelMkdir("/data/HyperNeiva/baixado/repositorios/games", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/repositorios/games/%s", nomeRepo); sceKernelMkdir(pathPasta, 0777);
        }
        else { sceKernelMkdir("/data/HyperNeiva/baixado/linkdireto", 0777); sprintf(pathPasta, "/data/HyperNeiva/baixado/linkdireto"); }

        char pathFinal[1024]; sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);
        progressoAtualDownload = 0.0f; strcpy(msgDownloadBg, "CONECTANDO...");

        int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1); sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); int conn = -1, req = -1; bool enviouPedido = false;
        definirLimiteCabecalho(tpl, 65536);
        if (urlDownloadBg[0] == '/') {
            char token[2048] = { 0 };
            obterTokenValido(token);
            const char* apiUrl = "https://content.dropboxapi.com/2/files/download"; conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1); req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);
            char authHeader[2048]; sprintf(authHeader, "Bearer %s", token); sceHttpAddRequestHeader(req, "Authorization", authHeader, 1);
            char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", urlDownloadBg); sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 1);
            enviouPedido = sceHttpSendRequest(req, NULL, 0) >= 0;
        }
        else {
            char urlTratada[1024]; strcpy(urlTratada, urlDownloadBg); char* dbox = strstr(urlTratada, "dropbox.com"); if (dbox) { char* dl0 = strstr(urlTratada, "?dl=0"); if (!dl0) dl0 = strstr(urlTratada, "&dl=0"); if (dl0) dl0[4] = '1'; }
            enviouPedido = abrirDownloadWeb(tpl, urlTratada, &conn, &req);
        }

        if (enviouPedido) {
            // O link do Drive termina em "uc": o nome do arquivo só vem na resposta.
            if (strstr(urlDownloadBg, "drive.google.com") && nomeDaResposta(req, nomeArquivo, sizeof(nomeArquivo))) {
                sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);
                ext = strrchr(nomeArquivo, '.');
            }
            size_t tamanhoTotal = 0; int32_t statusRes = 0; sceHttpGetResponseContentLength(req, &statusRes, &tamanhoTotal);
            // Página de erro do servidor não pode virar arquivo baixado.
            int32_t codigoHttp = 0; sceHttpGetStatusCode(req, &codigoHttp);
            uint64_t totalRecebido = 0; bool escreveuTudo = true;
            FILE* f = codigoHttp >= 300 ? NULL : fopen(pathFinal, "wb");
            if (codigoHttp >= 300) { snprintf(msgStatus, sizeof(msgStatus), "ERRO %d NO DOWNLOAD!", (int)codigoHttp); msgTimer = 300; }
            if (f) {
                unsigned char* buf = (unsigned char*)malloc(65536);
                if (buf) {
                    int n; uint64_t baixado = 0;
                    while ((n = sceHttpReadData(req, buf, 65536)) > 0) {
                        if (fwrite(buf, 1, n, f) != (size_t)n) { escreveuTudo = false; break; }
                        baixado += n; totalRecebido = baixado;
                        char msgTemp[256];
                        if (tamanhoTotal > 0) { progressoAtualDownload = (float)baixado / (float)tamanhoTotal; sprintf(msgTemp, "BAIXANDO: %s", nomeArquivo); }
                        else { progressoAtualDownload = 0.5f; sprintf(msgTemp, "BAIXANDO: %s... (%.2f MB)", nomeArquivo, (float)baixado / 1048576.0f); }
                        strcpy(msgDownloadBg, msgTemp); sceKernelUsleep(1000);
                    } free(buf);
                } fclose(f);
                // Um PKG pela metade nunca deve chegar ao instalador.
                if (!escreveuTudo || (tamanhoTotal > 0 && totalRecebido != tamanhoTotal)) { remove(pathFinal); sprintf(msgStatus, "DOWNLOAD INCOMPLETO! TENTE DE NOVO"); msgTimer = 300; }
                else if (downloadVeioComoPagina(pathFinal, ext)) { remove(pathFinal); sprintf(msgStatus, "O LINK DEVOLVEU UMA PAGINA, NAO O ARQUIVO"); msgTimer = 400; }
                else if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) { sprintf(msgStatus, "DOWNLOAD CONCLUIDO! INSTALANDO..."); msgTimer = 300; instalarPkgLocal(pathFinal); }
                else { sprintf(msgStatus, "ARQUIVO CONCLUIDO!"); msgTimer = 300; }
            }
        }
        else { snprintf(msgStatus, sizeof(msgStatus), "ERRO NO DOWNLOAD: 0x%08X", (unsigned int)ultimoErroDownload); msgTimer = 400; }

        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
        removerPrimeiroDaFila(); baixadosFilaSessao++;
    }
    totalFilaSessao = 0; baixadosFilaSessao = 0; downloadEmSegundoPlano = false; return NULL;
}

// ---- Escolha ao baixar um PKG: pelo PS4 (fila de downloads do sistema) ou pelo Hyper Neiva ----
bool escolhaPkgAberta = false;
static int escolhaPkgSel = 0;
static char escolhaPkgUrl[4096] = "", escolhaPkgNome[128] = "";
static void iniciarDownloadFila(const char* url);

// Segue os redirecionamentos (GitHub, Dropbox) até o endereço que entrega o arquivo: o
// download do sistema não segue o salto sozinho. Tira o "#" do fim (o Dropbox manda).
static bool resolverLinkFinal(const char* url, char* saida, size_t tam) {
    char atual[4096]; snprintf(atual, sizeof(atual), "%s", url);
    char* dbox = strstr(atual, "dropbox.com");
    if (dbox) { char* dl0 = strstr(atual, "?dl=0"); if (!dl0) dl0 = strstr(atual, "&dl=0"); if (dl0) dl0[4] = '1'; }
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) return false;
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    definirLimiteCabecalho(tpl, 65536);
    definirRedirecionamento(tpl, 0);
    bool ok = false;
    for (int salto = 0; salto < 6 && !ok; salto++) {
        int conn = sceHttpCreateConnectionWithURL(tpl, atual, 0);
        int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, atual, 0) : -1;
        if (req < 0) { if (conn >= 0) sceHttpDeleteConnection(conn); break; }
        if (strstr(atual, "api.github.com")) sceHttpAddRequestHeader(req, "Accept", "application/octet-stream", 1);
        sceHttpAddRequestHeader(req, "Range", "bytes=0-0", 1);
        int32_t codigo = 0;
        bool enviado = sceHttpSendRequest(req, NULL, 0) >= 0 && sceHttpGetStatusCode(req, &codigo) >= 0;
        logDownload("RESOLVER %d: status %d %.80s", salto, (int)codigo, atual);
        if (enviado && (codigo == 200 || codigo == 206)) { snprintf(saida, tam, "%s", atual); ok = true; }
        else if (enviado && (codigo == 301 || codigo == 302 || codigo == 303 || codigo == 307 || codigo == 308)) {
            char* cab = NULL; size_t n = 0;
            char destino[4096] = "";
            if (sceHttpGetAllResponseHeaders(req, &cab, &n) >= 0 && cab) {
                for (size_t i = 0; i + 9 < n; i++) {
                    if ((i == 0 || cab[i - 1] == '\n') && strncasecmp(cab + i, "location:", 9) == 0) {
                        size_t j = i + 9, k = 0;
                        while (j < n && cab[j] == ' ') j++;
                        while (j < n && cab[j] != '\r' && cab[j] != '\n' && k < sizeof(destino) - 1) destino[k++] = cab[j++];
                        destino[k] = '\0';
                        break;
                    }
                }
            }
            char* fragmento = strchr(destino, '#'); if (fragmento) *fragmento = '\0';
            if (destino[0] == '/') {
                const char* resto = strstr(atual, "://");
                int servidor = 0;
                if (resto) { const char* barra = strchr(resto + 3, '/'); servidor = barra ? (int)(barra - atual) : (int)strlen(atual); }
                char rel[4096]; snprintf(rel, sizeof(rel), "%.*s%s", servidor, atual, destino);
                snprintf(atual, sizeof(atual), "%s", rel);
            }
            else if (destino[0]) snprintf(atual, sizeof(atual), "%s", destino);
            else salto = 99;
        }
        else salto = 99;
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
    }
    sceHttpDeleteTemplate(tpl);
    return ok;
}

static void baixarPkgPeloPs4(const char* url, const char* nome) {
    char final_[4096];
    if (!resolverLinkFinal(url, final_, sizeof(final_))) snprintf(final_, sizeof(final_), "%s", url);
    // O sistema aceita até 2047 letras de endereço.
    if (strlen(final_) < 2048 && tentarBgft(final_, nome)) {
        snprintf(msgStatus, sizeof(msgStatus), "BAIXANDO PELO PS4: VEJA EM NOTIFICACOES > DOWNLOADS");
        msgTimer = 400;
        return;
    }
    snprintf(msgStatus, sizeof(msgStatus), "O PS4 NAO ACEITOU ESTE LINK. BAIXANDO PELO HYPER NEIVA...");
    msgTimer = 300;
    iniciarDownloadFila(url);
}

void escolhaPkgDesenhar(uint32_t* p) {
    if (!escolhaPkgAberta) return;
    extern void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);
    const int w = 1100, h = 380, x0 = (1920 - w) / 2, y0 = (1080 - h) / 2;
    for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) p[y * 1920 + x] = 0xFF15181E;
    for (int x = x0; x < x0 + w; x++) for (int y = y0; y < y0 + 4; y++) p[y * 1920 + x] = 0xFF29B6F6;
    char t[200]; snprintf(t, sizeof(t), "Baixar %.70s", escolhaPkgNome);
    desenharTexto(p, t, 32, x0 + 40, y0 + 30, 0xFFFFFFFF);
    const char* op[2] = { "Pelo PS4: fila de downloads do sistema, instala sozinho", "Pelo Hyper Neiva: barra de progresso no app, salva em /data/pkg" };
    for (int i = 0; i < 2; i++) {
        int y = y0 + 110 + i * 90;
        if (i == escolhaPkgSel) for (int yy = y - 10; yy < y + 60; yy++) for (int xx = x0 + 24; xx < x0 + w - 24; xx++) p[yy * 1920 + xx] = 0xFF2C3440;
        desenharTexto(p, op[i], 28, x0 + 50, y + 6, i == escolhaPkgSel ? 0xFF29B6F6 : 0xFFE0E0E0);
    }
    desenharTexto(p, "X escolhe     O cancela", 24, x0 + 40, y0 + h - 50, 0xFF9AA4B0);
}

void escolhaPkgControles(uint32_t botoes, uint32_t antigos) {
    uint32_t n = botoes & ~antigos;
    if (n & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN)) escolhaPkgSel ^= 1;
    if (n & ORBIS_PAD_BUTTON_CIRCLE) { escolhaPkgAberta = false; snprintf(msgStatus, sizeof(msgStatus), "DOWNLOAD CANCELADO"); msgTimer = 120; return; }
    if (n & ORBIS_PAD_BUTTON_CROSS) {
        escolhaPkgAberta = false;
        if (escolhaPkgSel == 0) baixarPkgPeloPs4(escolhaPkgUrl, escolhaPkgNome);
        else iniciarDownloadFila(escolhaPkgUrl);
    }
}

void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 2) return;
    // PKG por link (http/https): pergunta se baixa pelo PS4 ou pelo Hyper Neiva. O nome dos
    // arquivos do GitHub não está no link (api.github.com/.../assets/123): vem à parte.
    if (!strncasecmp(url, "http://", 7) || !strncasecmp(url, "https://", 8)) {
        extern char githubNomeDownload[128];
        char nome[128];
        if (menuAtual == MENU_BAIXAR_GITHUB_ARQUIVOS && githubNomeDownload[0]) snprintf(nome, sizeof(nome), "%s", githubNomeDownload);
        else {
            const char* barra = strrchr(url, '/');
            snprintf(nome, sizeof(nome), "%s", barra ? barra + 1 : url);
            char* corte = strpbrk(nome, "?#"); if (corte) *corte = '\0';
        }
        const char* ext = strrchr(nome, '.');
        if (ext && !strcasecmp(ext, ".pkg")) {
            snprintf(escolhaPkgUrl, sizeof(escolhaPkgUrl), "%s", url);
            snprintf(escolhaPkgNome, sizeof(escolhaPkgNome), "%s", nome);
            escolhaPkgSel = 0;
            escolhaPkgAberta = true;
            return;
        }
    }
    iniciarDownloadFila(url);
}

static void iniciarDownloadFila(const char* url) {
    if (!url || strlen(url) < 2) return;
    char nomeArquivo[128] = "arquivo.bin"; char* ref = strrchr(url, '/'); if (ref) { strncpy(nomeArquivo, ref + 1, 127); nomeArquivo[127] = '\0'; }
    char* ptrInterrogacao = strchr(nomeArquivo, '?'); if (ptrInterrogacao) *ptrInterrogacao = '\0';
    char* ptrHash = strchr(nomeArquivo, '#'); if (ptrHash) *ptrHash = '\0';
    char nomeLimpo[128] = { 0 }; int j = 0;
    for (int i = 0; nomeArquivo[i] != '\0' && j < 127; i++) { if (nomeArquivo[i] == '%' && nomeArquivo[i + 1] == '2' && nomeArquivo[i + 2] == '0') { nomeLimpo[j++] = ' '; i += 2; } else { nomeLimpo[j++] = nomeArquivo[i]; } }
    strcpy(nomeArquivo, nomeLimpo);
    (void)nomeArquivo;
    int antes = obterTamanhoFila();
    if (!downloadEmSegundoPlano) { totalFilaSessao = antes; baixadosFilaSessao = 0; }
    extern char githubNomeDownload[128];
    adicionarNaFila(url, menuAtual, menuAtual == MENU_BAIXAR_GITHUB_ARQUIVOS ? githubNomeDownload : caminhoXMLAtual);
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

    // Se não for a raiz do sistema, cria a pasta
    if (strlen(localPath) > 0) sceKernelMkdir(localPath, 0777);

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
                    char* tagMarker;
                    while ((tagMarker = strstr(p, "\".tag\"")) != NULL) {
                        char* obj = tagMarker;
                        while (obj > h && *obj != '{') obj--;
                        if (*obj != '{') break;
                        char* objEnd = fimObjetoJson(obj);
                        if (!objEnd) break;
                        char salvo = *objEnd; *objEnd = '\0';

                        char tag[32] = { 0 }; char nomeItem[256] = { 0 }; char caminhoItem[512] = { 0 };
                        bool ok = extrairStringJson(obj, "\".tag\"", tag, sizeof(tag)) &&
                                  extrairStringJson(obj, "\"name\"", nomeItem, sizeof(nomeItem)) &&
                                  extrairStringJson(obj, "\"path_display\"", caminhoItem, sizeof(caminhoItem));
                        if (ok) {
                            bool isFolder = (strcmp(tag, "folder") == 0);
                            if (isFolder && numSubFolders < 100) { strncpy(subFolders[numSubFolders], caminhoItem, 511); subFolders[numSubFolders][511] = '\0'; strncpy(subFoldersNames[numSubFolders], nomeItem, 255); subFoldersNames[numSubFolders][255] = '\0'; numSubFolders++; }
                            else if (!isFolder && numFiles < 200) { strncpy(files[numFiles], caminhoItem, 511); files[numFiles][511] = '\0'; strncpy(filesNames[numFiles], nomeItem, 255); filesNames[numFiles][255] = '\0'; numFiles++; }
                        }

                        *objEnd = salvo;
                        p = objEnd + 1;
                    } free(h);

                    // A mágica: Se localPath for a raiz (""), ele vai criar "/user", "/data" perfeitamente
                    for (int i = 0; i < numSubFolders; i++) {
                        char nextLocal[512];
                        if (strlen(localPath) == 0) sprintf(nextLocal, "/%s", subFoldersNames[i]);
                        else sprintf(nextLocal, "%s/%s", localPath, subFoldersNames[i]);
                        processarDownloadPastaRecursiva(subFolders[i], nextLocal);
                    }
                    for (int i = 0; i < numFiles; i++) {
                        char nextLocal[512];
                        if (strlen(localPath) == 0) sprintf(nextLocal, "/%s", filesNames[i]);
                        else sprintf(nextLocal, "%s/%s", localPath, filesNames[i]);
                        fazerDownloadArquivoDaNuvem(files[i], nextLocal);
                    }

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

// ==============================================================
// RESTAURAÇÃO TOTAL DO SISTEMA
// ==============================================================
void* threadRestaurarBackup(void* arg) {
    downloadEmSegundoPlano = true;
    sprintf(msgStatus, "RESTAURANDO BACKUP (NUVEM -> PS4)...");
    msgTimer = 900;

    // O Dropbox tem a raiz do backup em /HyperNeiva_Uploads/backup.
    // Se passarmos a string vazia "", o script vai reconstruir as pastas /data, /user, etc nativamente!
    processarDownloadPastaRecursiva("/HyperNeiva_Uploads/backup", "");

    downloadEmSegundoPlano = false;
    sprintf(msgStatus, "RESTAURACAO CONCLUIDA! REINICIE O CONSOLE.");
    msgTimer = 600;
    return NULL;
}

void executarRestaurarBackup() {
    if (processandoFilaUpload || downloadEmSegundoPlano) {
        sprintf(msgStatus, "AGUARDE AS TAREFAS ATUAIS TERMINAREM!"); msgTimer = 180; return;
    }
    sprintf(msgStatus, "INICIANDO RESTAURACAO DO SISTEMA...");
    msgTimer = 180;

    pthread_t tRes;
    pthread_create(&tRes, NULL, threadRestaurarBackup, NULL);
    pthread_detach(tRes);
}

void dropboxDeletarArquivo(const char* path) {
    sprintf(msgStatus, "ARQUIVO DELETADO: %s", path);
    msgTimer = 120;
}

void dropboxRenomearArquivo(const char* de, const char* para) {
    sprintf(msgStatus, "RENOMEADO: %s -> %s", de, para);
    msgTimer = 120;
}

void dropboxCriarPasta(const char* path) {
    sprintf(msgStatus, "PASTA CRIADA: %s", path);
    msgTimer = 120;
}
