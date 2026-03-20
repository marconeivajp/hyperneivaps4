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
char currentDropboxPath[512] = ""; // Iniciando o GPS do Dropbox vazio

const char* TOKEN_DROPBOX = "sl.u.AGWEjkVibWPkb4m1X63L4hjpy4iEaPLXj7PZzUBsiqDLmkbxaLwaf-0aTlv0xOxn3-vFQT54Aujj1mp7DtCVXRuva4O-bPkmR19xMJP1keTENeH371mkb80w7pLKnilKkwKcCqsQBEPv4ZBPgqx_cxAw-Ky0nwmT8I1nqtloKObCzosWX4xXqWgnmdoyXIPVmRlEKFe5H9kTJE-1s4ZQlCpgXvTyI9KUji6DuQtNO45moKnX3a6nsG_qgCTA30gkpoYVjLo3641JjsA7pqhLCMl6Kg3e_IX4H-7ST1QzE50relygyiiOqDm7a_N2lfZclKbhE-4rdYsZS3ocKnvFCuMlxGsHdRREzXIZfRgGgM8hWoDl8T9tkdB7nQWbWV9PODOqpYBNutjpgWk9aaS68VQuAyawlG1WNxrusFYxEwet9GYbTtVx5HaSaBBBelp9YtO_piW92GsKqqfq3jC31hZ1AKHhcXg5WWHE-rkm3-7E2pzQItVZ5XyuQ0oP0noryZ4menTeEwiwqHYyYxuQmv5FD0AYok1pJeizQPuXy9Vtw3PkIvm71RkvxFDfV4arRF4VphVcFo8ug6Aqh2Xinm5t6zNMzVMIwWhzheGierfhBGkR48aXoaqJU2oeiG6gyAgi032qD2C4CGxTCEES5P03iw4LlSVwlsGCpL4WAIf-1ZbwgqWOuStKYJdzxXamaPf7UZUpaYtTcevfIEQ7k1Fcya0LAEo4eScglifn6FeWM5iD7-TP68Yywu3d5sOvKub2_bwAylIFi_FjwdKXg2p9gOVzhAYYxlt7ekScKzuvS8z_OV4YM0Yx7EXpT3Pb8cjFE3YYUvlUXNCOW9VHe9B_wXEeHM3AoN_ACtf_vkSwCRCYYKORg1PUF_jvCPVLxtfHFAdVhc_okFk3fOcIk6ddvHCAg1epUkV-AAQR53Z8_Bp_GSc7Dl6qx_k3J1ZZKVmq7gnTtaRoZEesduUkyY_hTLJt9sPlQa8R_50LVD_Gp5nrMWLirORK7MBDGJgjEaRaVpHdvA8kwWEQDV4mw8xITlDF1d2CY8Z4OSq3lcCDaQnhAOCd45wbe_3eL9bGD0jClmM3jE-zUS2oZyR-80HFbfS3tYdKr9sFP3MyAxa6CNn89DPfWLICXuImhlk5mW5hStMbngQAzDqLVGSyBsAaKWR1fR4h5iecK3ClAI7ovPIuguMmU7jaSOW8kAnuYoAjPLC4cDAlOByaEj6mDQMUJMdPfQKFWco8o0CdRmmfEYwlJ0H_ueebF6-vMWj3qD6duoWcknfBv4wIfHCKCE21Ze9BifdQb1miQwANqrSe7A";

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
    strcpy(nomes[3], "DROPBOX");
    totalItens = 4; menuAtual = MENU_BAIXAR; sel = 0; off = 0;
}

void acessarDropbox(const char* path) {
    char cleanPath[256];
    strncpy(cleanPath, path, 255);
    cleanPath[255] = '\0';
    for (int i = 0; i < strlen(cleanPath); i++) {
        if (cleanPath[i] == '\r' || cleanPath[i] == '\n') cleanPath[i] = '\0';
    }

    // Salva o caminho atual na memória para o botão bolinha poder voltar depois
    strcpy(currentDropboxPath, cleanPath);

    sprintf(msgStatus, "CONECTANDO A API...");
    msgTimer = 180;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    const char* apiUrl = "https://api.dropboxapi.com/2/files/list_folder";
    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

    char postData[512];
    memset(postData, 0, sizeof(postData));
    sprintf(postData, "{\"path\": \"%s\"}", cleanPath);

    char authHeader[2048];
    sprintf(authHeader, "Bearer %s", TOKEN_DROPBOX);

    sceHttpAddRequestHeader(req, "Authorization", authHeader, 0);
    sceHttpAddRequestHeader(req, "Content-Type", "application/json; charset=utf-8", 0);
    sceHttpSetRequestContentLength(req, strlen(postData));

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    totalItens = 0;

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
                        errSummary += 18;
                        char* errEnd = strchr(errSummary, '\"');
                        if (errEnd) {
                            int errLen = errEnd - errSummary;
                            if (errLen > 100) errLen = 100;
                            char errMsg[128]; strncpy(errMsg, errSummary, errLen); errMsg[errLen] = '\0';
                            sprintf(msgStatus, "API ERRO: %s", errMsg);
                        }
                        else {
                            sprintf(msgStatus, "API RETORNOU ERRO DESCONHECIDO");
                        }
                        strcpy(nomes[0], "Erro na API (veja notificação)");
                        totalItens = 1;
                    }
                    else {
                        char* p = h;
                        while ((p = strstr(p, "\".tag\": \"")) && totalItens < 2900) {
                            p += 9;
                            bool isFolder = (strncmp(p, "folder", 6) == 0);

                            char* namePtr = strstr(p, "\"name\": \"");
                            if (!namePtr) break;
                            namePtr += 9;
                            char* nameEnd = strchr(namePtr, '\"');

                            char* pathPtr = strstr(nameEnd, "\"path_display\": \"");
                            if (!pathPtr) break;
                            pathPtr += 17;
                            char* pathEnd = strchr(pathPtr, '\"');

                            if (nameEnd && pathEnd) {
                                int nameLen = nameEnd - namePtr;
                                int pathLen = pathEnd - pathPtr;

                                strncpy(nomes[totalItens], namePtr, nameLen);
                                nomes[totalItens][nameLen] = '\0';

                                strncpy(linksAtuais[totalItens], pathPtr, pathLen);
                                if (isFolder) {
                                    linksAtuais[totalItens][pathLen] = '/';
                                    linksAtuais[totalItens][pathLen + 1] = '\0';
                                }
                                else {
                                    linksAtuais[totalItens][pathLen] = '\0';
                                }
                                totalItens++;
                            }
                            p = pathEnd;
                        }

                        if (totalItens > 0) {
                            sprintf(msgStatus, "PASTAS CARREGADAS!");
                        }
                        else {
                            if (strstr(h, "erro 400") || strstr(h, "incidente")) {
                                sprintf(msgStatus, "ERRO 400: O DROPBOX REJEITOU O PEDIDO");
                            }
                            else {
                                sprintf(msgStatus, "PASTA VAZIA (SEM ARQUIVOS)");
                            }
                            strcpy(nomes[0], "Nenhum ficheiro encontrado");
                            totalItens = 1;
                        }
                    }
                    free(h);
                }
                fclose(f2);
            }
        }
    }
    else {
        sprintf(msgStatus, "ERRO: FALHA DE REDE (SEM RESPOSTA)");
    }

    msgTimer = 240;
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    menuAtual = MENU_BAIXAR_DROPBOX_LISTA;
    sel = 0; off = 0;
}

void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 2) return;

    char pathPasta[256]; sprintf(pathPasta, "/data/HyperNeiva/baixado/Downloads");
    sceKernelMkdir(pathPasta, 0777);

    char nomeArquivo[128] = "arquivo.bin";
    char* ref = strrchr(url, '/');
    if (ref) strncpy(nomeArquivo, ref + 1, 127);

    char pathFinal[512]; sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);
    sprintf(msgStatus, "BAIXANDO: %s", nomeArquivo); msgTimer = 150;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    int conn, req;

    if (url[0] == '/') {
        const char* apiUrl = "https://content.dropboxapi.com/2/files/download";
        conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
        req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_POST, apiUrl, 0);

        char authHeader[2048]; sprintf(authHeader, "Bearer %s", TOKEN_DROPBOX);
        sceHttpAddRequestHeader(req, "Authorization", authHeader, 0);

        char apiArg[1024]; sprintf(apiArg, "{\"path\": \"%s\"}", url);
        sceHttpAddRequestHeader(req, "Dropbox-API-Arg", apiArg, 0);
    }
    else {
        conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
        req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    }

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        FILE* f = fopen(pathFinal, "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f); sprintf(msgStatus, "DOWNLOAD CONCLUIDO!");
        }
    }
    else {
        sprintf(msgStatus, "ERRO NO DOWNLOAD");
    }

    msgTimer = 180;
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

void preencherMenuRepositorios() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Games");
    totalItens = 1; menuAtual = MENU_BAIXAR_REPOS; sel = 0; off = 0;
}

void listarXMLsRepositorio() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    DIR* d = opendir("/data/HyperNeiva/baixado/repositorio/games");
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".xml")) {
                strcpy(nomes[totalItens], dir->d_name);
                totalItens++;
            }
        }
        closedir(d);
    }
    if (totalItens == 0) { strcpy(nomes[0], "Nenhum XML encontrado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_GAMES_XMLS; sel = 0; off = 0;
}

void abrirXMLRepositorio(const char* xmlFile) {
    sprintf(caminhoXMLAtual, "/data/HyperNeiva/baixado/repositorio/games/%s", xmlFile);
    FILE* fp = fopen(caminhoXMLAtual, "rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);
    memset(nomes, 0, sizeof(nomes)); totalItens = 0; char* p = b;
    while (totalItens < 2000) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        p += 12; char* f = strchr(p, '\"');
        if (f) {
            int l = (int)(f - p); strncpy(nomes[totalItens], p, l);
            nomes[totalItens][l] = '\0'; totalItens++; p = f;
        }
        else break;
    }
    free(b); menuAtual = MENU_BAIXAR_GAMES_LIST; sel = 0; off = 0;
}

void mostrarLinksJogo(int gameIndex) {
    memset(nomes, 0, sizeof(nomes)); totalItens = 0; totalLinksAtuais = 0;
    FILE* fp = fopen(caminhoXMLAtual, "rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);
    char* p = b; int currentIdx = -1;
    while (true) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        currentIdx++;
        if (currentIdx == gameIndex) {
            char* gameBlockEnd = strstr(p, "</game>");
            char* lPtr = p;
            while (totalLinksAtuais < 10) {
                lPtr = strstr(lPtr, "<link>");
                if (!lPtr || (gameBlockEnd && lPtr > gameBlockEnd)) break;
                lPtr += 6; char* lEnd = strstr(lPtr, "</link>");
                if (lEnd) {
                    int len = (int)(lEnd - lPtr);
                    strncpy(linksAtuais[totalLinksAtuais], lPtr, len > 1023 ? 1023 : len);
                    linksAtuais[totalLinksAtuais][len] = '\0';
                    sprintf(nomes[totalItens], "Opcao %d", totalLinksAtuais + 1);
                    totalLinksAtuais++; totalItens++; lPtr = lEnd;
                }
                else break;
            }
            break;
        }
        p += 12;
    }
    free(b); menuAtual = MENU_BAIXAR_LINKS; sel = 0; off = 0;
}