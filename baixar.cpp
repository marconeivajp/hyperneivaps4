#include "baixar.h"
#include "network.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/libkernel.h>
#include "stb_image.h"
#include <stdarg.h> 

extern "C" int sceHttpSetRequestContentLength(int reqId, uint64_t contentLength);

Console listaConsoles[5] = {
    {"Sony - PlayStation", "Sony%20-%20PlayStation"},
    {"Sony - PlayStation Portable", "Sony%20-%20PlayStation%20Portable"},
    {"Nintendo - Super Nintendo Entertainment System", "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System"},
    {"Sega - Mega Drive - Genesis", "Sega%20-%20Mega%20Drive%20-%20Genesis"},
    {"Nintendo - Nintendo Entertainment System", "Nintendo%20-%20Nintendo%20Entertainment%20System"}
};

int consoleAtual = 0;
unsigned char* imgPreview = NULL;
int wP = 0, hP = 0, cP = 0;
char ultimoJogoCarregado[64] = "";
char caminhoXMLAtual[256];
char linksAtuais[3000][1024];
int totalLinksAtuais = 0;

char currentDropboxPath[512] = "";
char currentUploadPath[512] = "";

void acaoRede(const char* jogo, bool buscarLista, bool salvarNoHD) {
    char url[512];
    if (buscarLista) sprintf(url, "https://thumbnails.libretro.com/%s/Named_Boxarts/", listaConsoles[consoleAtual].pathServidor);
    else {
        char nURL[256] = { 0 };
        for (int i = 0, j = 0; jogo[i]; i++) { if (jogo[i] == ' ') { strcat(nURL, "%20"); j += 3; } else { nURL[j++] = jogo[i]; } }
        sprintf(url, "https://thumbnails.libretro.com/%s/Named_Boxarts/%s.png", listaConsoles[consoleAtual].pathServidor, nURL);
    }

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();
    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        char path[256];
        if (buscarLista) strcpy(path, "/data/HyperNeiva/remote_list.html");
        else if (salvarNoHD) {
            char sub[256]; sprintf(sub, "/data/HyperNeiva/baixado/%s", listaConsoles[consoleAtual].nome);
            sceKernelMkdir(sub, 0777); char box[256]; sprintf(box, "%s/Named_Boxarts", sub);
            sceKernelMkdir(box, 0777); sprintf(path, "%s/%s.png", box, jogo);
        }
        else strcpy(path, "/data/HyperNeiva/preview.png");

        FILE* f = fopen(path, "wb");
        if (f) {
            unsigned char buf[16384]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f);

            if (buscarLista) {
                FILE* f2 = fopen("/data/HyperNeiva/remote_list.html", "rb"); fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                char* h = (char*)malloc(sz + 1); fread(h, 1, sz, f2); h[sz] = '\0'; fclose(f2);
                memset(nomes, 0, sizeof(nomes)); totalItens = 0; char* b = h;
                while ((b = strstr(b, "href=\"")) && totalItens < 3000) {
                    b += 6; if (strstr(b, "Parent Directory") || b[0] == '?' || b[0] == '/') { b++; continue; }
                    char* e = strstr(b, ".png\"");
                    if (e) {
                        int l = (int)(e - b); strncpy(nomes[totalItens], b, l); nomes[totalItens][l] = '\0';
                        char* pN = nomes[totalItens], * qN = nomes[totalItens];
                        while (*pN) { if (*pN == '%' && *(pN + 1) == '2' && *(pN + 2) == '0') { *qN++ = ' '; pN += 3; } else { *qN++ = *pN++; } }
                        *qN = '\0'; totalItens++;
                    }
                    b = e + 5;
                }
                free(h); menuAtual = SCRAPER_LIST;
            }
            else {
                if (imgPreview) stbi_image_free(imgPreview);
                imgPreview = stbi_load(path, &wP, &hP, &cP, 4);
                strcpy(ultimoJogoCarregado, jogo);
            }
        }
    }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void preencherMenuBaixar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Repositorios");
    strcpy(nomes[1], "CAPAS");
    strcpy(nomes[2], "LINK DIRETO");
    strcpy(nomes[3], "DROPBOX (DOWNLOAD)");
    strcpy(nomes[4], "DROPBOX (MENU BACKUP)"); // Alterado para abrir o seu novo menu!
    totalItens = 5; menuAtual = MENU_BAIXAR; sel = 0; off = 0;
}

// -----------------------------------------------------
// NOVO MENU DE BACKUP
// -----------------------------------------------------
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

void executarBackupTodos() {
    // Faz o envio direto dos arquivos unicos vitais em sequencia
    fazerUploadDropbox("/system_data/priv/mms/app.db");
    fazerUploadDropbox("/data/HyperNeiva/configuracao/dropbox_token.txt");
    fazerUploadDropbox("/data/retroarch/retroarch.cfg");
    sprintf(msgStatus, "BACKUP ESSENCIAL CONCLUIDO!");
    msgTimer = 180;
}
// -----------------------------------------------------

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
    sprintf(msgStatus, "BAIXANDO: %s", nomeArquivo); msgTimer = 150;

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
        FILE* f = fopen(pathFinal, "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f); sprintf(msgStatus, "DOWNLOAD CONCLUIDO!");
        }
    }
    else sprintf(msgStatus, "ERRO NO DOWNLOAD");

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

    fread(fileData, 1, fileSize, fLocal);
    fclose(fLocal);

    char nomeArquivo[128] = "upload.bin";
    char* ref = strrchr(localPath, '/');
    if (ref) strncpy(nomeArquivo, ref + 1, 127);

    sprintf(msgStatus, "ENVIANDO %s...", nomeArquivo);
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
    }
    else {
        sprintf(msgStatus, "ERRO NO UPLOAD (FALHA DE REDE)");
    }

    free(fileData);
    msgTimer = 240;
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void preencherMenuRepositorios() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Games");
    totalItens = 1; menuAtual = MENU_BAIXAR_REPOS; sel = 0; off = 0;
}

void listarXMLsRepositorio() {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0;
    DIR* d = opendir("/data/HyperNeiva/baixado/repositorio/games");
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".xml")) { strcpy(nomes[totalItens], dir->d_name); totalItens++; }
        }
        closedir(d);
    }
    if (totalItens == 0) { strcpy(nomes[0], "Nenhum XML encontrado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_GAMES_XMLS; sel = 0; off = 0;
}

void abrirXMLRepositorio(const char* xmlFile) {
    sprintf(caminhoXMLAtual, "/data/HyperNeiva/baixado/repositorio/games/%s", xmlFile);
    FILE* fp = fopen(caminhoXMLAtual, "rb"); if (!fp) return;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);
    memset(nomes, 0, sizeof(nomes)); totalItens = 0; char* p = b;
    while (totalItens < 2000) {
        p = strstr(p, "<game name=\""); if (!p) break; p += 12; char* f = strchr(p, '\"');
        if (f) { int l = (int)(f - p); strncpy(nomes[totalItens], p, l); nomes[totalItens][l] = '\0'; totalItens++; p = f; }
        else break;
    }
    free(b); menuAtual = MENU_BAIXAR_GAMES_LIST; sel = 0; off = 0;
}

void mostrarLinksJogo(int gameIndex) {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0; totalLinksAtuais = 0;
    FILE* fp = fopen(caminhoXMLAtual, "rb"); if (!fp) return;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);
    char* p = b; int currentIdx = -1;
    while (true) {
        p = strstr(p, "<game name=\""); if (!p) break; currentIdx++;
        if (currentIdx == gameIndex) {
            char* gameBlockEnd = strstr(p, "</game>"); char* lPtr = p;
            while (totalLinksAtuais < 10) {
                lPtr = strstr(lPtr, "<link>"); if (!lPtr || (gameBlockEnd && lPtr > gameBlockEnd)) break;
                lPtr += 6; char* lEnd = strstr(lPtr, "</link>");
                if (lEnd) { int len = (int)(lEnd - lPtr); strncpy(linksAtuais[totalLinksAtuais], lPtr, len > 1023 ? 1023 : len); linksAtuais[totalLinksAtuais][len] = '\0'; sprintf(nomes[totalItens], "Opcao %d", totalLinksAtuais + 1); totalLinksAtuais++; totalItens++; lPtr = lEnd; }
                else break;
            }
            break;
        }
        p += 12;
    }
    free(b); menuAtual = MENU_BAIXAR_LINKS; sel = 0; off = 0;
}