#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include "baixar_lojas.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h> // Necessário para o conversor das capas

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>

#include "menu.h"
#include "network.h"
#include "baixar.h" 
#include "stb_image.h" // Necessário para carregar a capa

extern void atualizarBarra(float progresso);
extern char nomes[3000][64];
extern char linksAtuais[3000][1024];
extern bool marcados[3000];
extern int totalItens;
extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern char msgStatus[128];
extern int msgTimer;

extern char currentApolloUrl[1024];

extern unsigned char* imgPreview;
extern int wP, hP, cP;

char iconesAtuais[3000][1024]; // Guarda as URLs das capas

// Declarações antecipadas
void lerCacheHBStore();
void atualizarHBStore();

// =======================================================================
// MOTOR DE CAPAS (SALVA PERMANENTEMENTE NA PASTA IMAGENS)
// =======================================================================
void carregarCapaLoja(int index) {
    if (strlen(iconesAtuais[index]) < 10) {
        if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
        return;
    }

    // Gera um ID único para a imagem baseado no URL
    uint32_t hash = 0;
    for (int i = 0; iconesAtuais[index][i] != '\0'; i++) {
        hash = hash * 31 + iconesAtuais[index][i];
    }

    char localPath[256];
    // Salva exatamente onde pediste para ficar definitivo
    sprintf(localPath, "/data/HyperNeiva/configuracao/imagens/capa_%u.png", hash);

    FILE* check = fopen(localPath, "rb");
    if (check) {
        fclose(check);
        if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
        imgPreview = stbi_load(localPath, &wP, &hP, &cP, 4);
        return; // Já existe no HD, abre instantaneamente!
    }

    // Se não existir, baixa da net e salva para sempre
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();
    int conn = sceHttpCreateConnectionWithURL(tpl, iconesAtuais[index], 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, iconesAtuais[index], 0);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        FILE* f = fopen(localPath, "wb");
        if (f) {
            unsigned char buf[16384]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f);

            if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
            imgPreview = stbi_load(localPath, &wP, &hP, &cP, 4);
        }
    }
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
}

// =======================================================================
// MOTOR DA HB-STORE (COM CORREÇÃO DE NOMES E ÍCONES)
// =======================================================================
void lerCacheHBStore() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/hbstore_cache.txt", "r");
    if (!f) {
        atualizarHBStore();
        return;
    }

    char magic[128];
    if (!fgets(magic, sizeof(magic), f) || strncmp(magic, "V2", 2) != 0) {
        fclose(f);
        atualizarHBStore();
        return;
    }

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    memset(iconesAtuais, 0, sizeof(iconesAtuais));
    memset(marcados, 0, sizeof(marcados));
    totalItens = 0;

    strcpy(nomes[totalItens], "[ ATUALIZAR LOJA ]");
    strcpy(linksAtuais[totalItens], "UPDATE_HB_STORE");
    strcpy(iconesAtuais[totalItens], "");
    totalItens++;

    char linhaNome[256]; char linhaLink[1024]; char linhaIcone[1024];

    while (fgets(linhaNome, sizeof(linhaNome), f)) {
        if (!fgets(linhaLink, sizeof(linhaLink), f)) break;
        if (!fgets(linhaIcone, sizeof(linhaIcone), f)) break;

        linhaNome[strcspn(linhaNome, "\r\n")] = '\0';
        linhaLink[strcspn(linhaLink, "\r\n")] = '\0';
        linhaIcone[strcspn(linhaIcone, "\r\n")] = '\0';

        if (strlen(linhaNome) > 0 && strlen(linhaLink) > 0) {
            strncpy(nomes[totalItens], linhaNome, 63); nomes[totalItens][63] = '\0';
            strncpy(linksAtuais[totalItens], linhaLink, 1023); linksAtuais[totalItens][1023] = '\0';
            strncpy(iconesAtuais[totalItens], linhaIcone, 1023); iconesAtuais[totalItens][1023] = '\0';
            totalItens++;
            if (totalItens >= 2900) break;
        }
    }
    fclose(f);

    if (totalItens > 1) sprintf(msgStatus, "LOJA CARREGADA (CACHE: %d APPS)", totalItens - 1);
    else sprintf(msgStatus, "CACHE VAZIO. CLIQUE EM ATUALIZAR LOJA!");

    atualizarBarra(1.0f); msgTimer = 180; menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0;
    carregarCapaLoja(sel);
}

void atualizarHBStore() {
    sprintf(msgStatus, "BAIXANDO DADOS DA HB-STORE...");
    atualizarBarra(0.1f); msgTimer = 120;
    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL); sceHttpSetAutoRedirect();

    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais));
    memset(iconesAtuais, 0, sizeof(iconesAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;

    for (int pagina = 1; pagina <= 100; pagina++) {
        char apiUrl[256];
        if (pagina == 1) strcpy(apiUrl, "https://pkg-zone.com/");
        else sprintf(apiUrl, "https://pkg-zone.com/?page=%d", pagina);

        int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
        int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, apiUrl, 0);

        float pBar = 0.1f + (pagina * 0.02f); if (pBar > 0.95f) pBar = 0.95f; atualizarBarra(pBar);

        if (sceHttpSendRequest(req, NULL, 0) >= 0) {
            sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
            char tempPath[256]; sprintf(tempPath, "/data/HyperNeiva/configuracao/temporario/loja_p%d.html", pagina);
            FILE* f = fopen(tempPath, "wb");
            if (f) {
                unsigned char buf[16384]; int n;
                while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
                fclose(f);

                FILE* f2 = fopen(tempPath, "rb");
                if (f2) {
                    fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                    char* h = (char*)malloc(sz + 1);
                    if (h) {
                        fread(h, 1, sz, f2); h[sz] = '\0'; char* b = h;
                        int itensNestaPagina = 0;
                        while ((b = strstr(b, "href=\"")) && totalItens < 2900) {
                            b += 6; char* aspasFim = strchr(b, '\"'); if (!aspasFim) break;
                            int tamanhoLink = aspasFim - b;

                            if (tamanhoLink > 5 && tamanhoLink < 500) {
                                char linkBruto[512]; strncpy(linkBruto, b, tamanhoLink); linkBruto[tamanhoLink] = '\0';
                                char* tagDetalhes = strstr(linkBruto, "/details/");
                                if (!tagDetalhes) tagDetalhes = strstr(linkBruto, "/title/");

                                if (tagDetalhes) {
                                    char titleId[64];
                                    if (strstr(linkBruto, "/details/")) strcpy(titleId, tagDetalhes + 9);
                                    else strcpy(titleId, tagDetalhes + 7);
                                    char* lixo = strpbrk(titleId, "/?#\"\'"); if (lixo) *lixo = '\0';

                                    if (strlen(titleId) >= 4 && strlen(titleId) < 20) {
                                        bool jaExiste = false;
                                        for (int i = 0; i < totalItens; i++) {
                                            if (strstr(linksAtuais[i], titleId) != NULL) { jaExiste = true; break; }
                                        }

                                        if (!jaExiste) {
                                            char nomeTelaBruto[256]; strcpy(nomeTelaBruto, titleId);
                                            char* limiteBusca = aspasFim + 600;
                                            bool achouNome = false;

                                            // 1. OBTÉM A CAPA E O NOME REAL (INFALÍVEL PELA TAG DA IMAGEM)
                                            char imgUrl[512] = "";
                                            char* tagImg = strstr(aspasFim, "<img");
                                            if (tagImg && tagImg < limiteBusca) {
                                                char* tagSrc = strstr(tagImg, "src=\"");
                                                if (tagSrc && tagSrc < limiteBusca) {
                                                    tagSrc += 5; char* fimSrc = strchr(tagSrc, '\"');
                                                    if (fimSrc && (fimSrc - tagSrc) < 500) {
                                                        strncpy(imgUrl, tagSrc, fimSrc - tagSrc); imgUrl[fimSrc - tagSrc] = '\0';
                                                        if (imgUrl[0] == '/') {
                                                            char tempImg[512]; sprintf(tempImg, "https://pkg-zone.com%s", imgUrl); strcpy(imgUrl, tempImg);
                                                        }
                                                        else if (strncmp(imgUrl, "http", 4) != 0) {
                                                            char tempImg[512]; sprintf(tempImg, "https://pkg-zone.com/%s", imgUrl); strcpy(imgUrl, tempImg);
                                                        }
                                                    }
                                                }
                                                // Pesca o Nome Verdadeiro pelo ALT
                                                char* tagAlt = strstr(tagImg, "alt=\"");
                                                if (tagAlt && tagAlt < limiteBusca) {
                                                    char* startAlt = tagAlt + 5; char* endAlt = strchr(startAlt, '\"');
                                                    if (endAlt && (endAlt - startAlt) > 2) {
                                                        strncpy(nomeTelaBruto, startAlt, endAlt - startAlt); nomeTelaBruto[endAlt - startAlt] = '\0';
                                                        if (strcasecmp(nomeTelaBruto, "icon") != 0 && strcasecmp(nomeTelaBruto, "image") != 0) {
                                                            achouNome = true;
                                                        }
                                                    }
                                                }
                                            }

                                            // Fallback se não tiver ALT na imagem
                                            if (!achouNome) {
                                                char* tagTitle = strstr(aspasFim, "card-title");
                                                if (!tagTitle || tagTitle > limiteBusca) tagTitle = strstr(aspasFim, "<h5");
                                                if (!tagTitle || tagTitle > limiteBusca) tagTitle = strstr(aspasFim, "<h4");
                                                if (tagTitle && tagTitle < limiteBusca) {
                                                    char* startText = strchr(tagTitle, '>');
                                                    if (startText) {
                                                        startText++; char* endText = strchr(startText, '<');
                                                        if (endText && (endText - startText) < 120) {
                                                            strncpy(nomeTelaBruto, startText, endText - startText); nomeTelaBruto[endText - startText] = '\0';
                                                            achouNome = true;
                                                        }
                                                    }
                                                }
                                            }

                                            char nomeLimpo[128]; int idxLimpo = 0; bool ultimoFoiEspaco = true;
                                            for (int i = 0; nomeTelaBruto[i] != '\0'; i++) {
                                                char c = nomeTelaBruto[i];
                                                if (c == '%' && nomeTelaBruto[i + 1] == '2' && nomeTelaBruto[i + 2] == '0') { c = ' '; i += 2; }
                                                else if (c == '\n' || c == '\r' || c == '\t') { c = ' '; }
                                                if (c == ' ') { if (!ultimoFoiEspaco) { nomeLimpo[idxLimpo++] = ' '; ultimoFoiEspaco = true; } }
                                                else { nomeLimpo[idxLimpo++] = c; ultimoFoiEspaco = false; }
                                            }
                                            if (idxLimpo > 0 && nomeLimpo[idxLimpo - 1] == ' ') idxLimpo--; nomeLimpo[idxLimpo] = '\0';
                                            if (strlen(nomeLimpo) == 0) strcpy(nomeLimpo, titleId);
                                            if (strlen(nomeLimpo) > 63) nomeLimpo[63] = '\0';

                                            strcpy(nomes[totalItens], nomeLimpo);
                                            sprintf(linksAtuais[totalItens], "https://pkg-zone.com/download/ps4/%s/latest", titleId);
                                            strcpy(iconesAtuais[totalItens], imgUrl);
                                            totalItens++; itensNestaPagina++;
                                        }
                                    }
                                }
                                else if (linkBruto[tamanhoLink - 4] == '.' && (linkBruto[tamanhoLink - 3] == 'p' || linkBruto[tamanhoLink - 3] == 'P') &&
                                    (linkBruto[tamanhoLink - 2] == 'k' || linkBruto[tamanhoLink - 2] == 'K') && (linkBruto[tamanhoLink - 1] == 'g' || linkBruto[tamanhoLink - 1] == 'G')) {

                                    char nomeTela[128]; char* ultimaBarra = strrchr(linkBruto, '/');
                                    if (ultimaBarra) strcpy(nomeTela, ultimaBarra + 1); else strcpy(nomeTela, linkBruto);
                                    char* ext = strstr(nomeTela, ".pkg"); if (!ext) ext = strstr(nomeTela, ".PKG"); if (ext) *ext = '\0';

                                    bool jaExiste = false;
                                    for (int i = 0; i < totalItens; i++) { if (strcasecmp(nomes[i], nomeTela) == 0) { jaExiste = true; break; } }

                                    if (!jaExiste && strlen(nomeTela) > 0) {
                                        strcpy(nomes[totalItens], nomeTela);
                                        if (strncmp(linkBruto, "http", 4) == 0) strcpy(linksAtuais[totalItens], linkBruto);
                                        else if (linkBruto[0] == '/') sprintf(linksAtuais[totalItens], "https://pkg-zone.com%s", linkBruto);
                                        else sprintf(linksAtuais[totalItens], "https://pkg-zone.com/%s", linkBruto);
                                        strcpy(iconesAtuais[totalItens], "");
                                        totalItens++; itensNestaPagina++;
                                    }
                                }
                            }
                            b = aspasFim + 1;
                        }
                        free(h);
                        if (itensNestaPagina == 0) { sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); goto fim_paginacao; }
                    }
                    fclose(f2);
                }
            }
        }
        else { sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); goto fim_paginacao; }
        sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn);
    }

fim_paginacao:
    sceHttpDeleteTemplate(tpl);

    if (totalItens > 0) {
        FILE* fCache = fopen("/data/HyperNeiva/configuracao/hbstore_cache.txt", "w");
        if (fCache) {
            fprintf(fCache, "V2\n");
            for (int i = 0; i < totalItens; i++) {
                fprintf(fCache, "%s\n%s\n%s\n", nomes[i], linksAtuais[i], iconesAtuais[i]);
            }
            fclose(fCache);
        }
        lerCacheHBStore();
    }
    else {
        sprintf(msgStatus, "ERRO AO ACESSAR A LOJA"); strcpy(nomes[0], "Verifique a sua internet"); totalItens = 1;
        atualizarBarra(1.0f); msgTimer = 180; menuAtual = MENU_BAIXAR_DROPBOX_LISTA; sel = 0; off = 0;
    }
}

void acessarHBStore() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/hbstore_cache.txt", "r");
    if (f) { fclose(f); lerCacheHBStore(); }
    else { atualizarHBStore(); }
}

// =======================================================================
// MOTOR DO GITHUB (APOLLO SAVES COM CORREÇÃO DEFINITIVA DOS NOMES _00)
// =======================================================================
void acessarApolloSaves(const char* url) {
    strcpy(currentApolloUrl, url);
    sprintf(msgStatus, "CONECTANDO A API DO GITHUB...");
    atualizarBarra(0.1f);
    msgTimer = 120;

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    memset(iconesAtuais, 0, sizeof(iconesAtuais));
    memset(marcados, 0, sizeof(marcados));
    totalItens = 0;

    char repo[128] = "bucanero/apollo-saves";
    char path[256] = "";
    char branch[64] = "master";

    const char* ptr = strstr(url, "github.com/");
    if (ptr) {
        ptr += 11;
        char temp[512];
        strncpy(temp, ptr, 511);
        temp[511] = '\0';

        if (strlen(temp) > 0 && temp[strlen(temp) - 1] == '/') temp[strlen(temp) - 1] = '\0';

        char* treePos = strstr(temp, "/tree/");
        if (!treePos) treePos = strstr(temp, "/blob/");

        if (treePos) {
            *treePos = '\0';
            strcpy(repo, temp);

            char* afterTree = treePos + 6;
            char* slashPos = strchr(afterTree, '/');
            if (slashPos) {
                *slashPos = '\0';
                strcpy(branch, afterTree);
                strcpy(path, slashPos + 1);
            }
            else {
                strcpy(branch, afterTree);
            }
        }
        else {
            strcpy(repo, temp);
        }
    }

    if (strlen(path) > 0 && path[strlen(path) - 1] == '/') path[strlen(path) - 1] = '\0';

    char apiUrl[512];
    if (strlen(path) > 0) {
        sprintf(apiUrl, "https://api.github.com/repos/%s/contents/%s?ref=%s", repo, path, branch);
    }
    else {
        sprintf(apiUrl, "https://api.github.com/repos/%s/contents?ref=%s", repo, branch);
    }

    char* bufMap = (char*)malloc(256 * 1024);
    bool hasMap = false;

    if (bufMap && strlen(path) > 0) {
        memset(bufMap, 0, 256 * 1024);
        char rawUrl[512];

        int tplRaw = sceHttpCreateTemplate(httpCtxId, "Mozilla/5.0", ORBIS_HTTP_VERSION_1_1, 1);
        sceHttpsSetSslCallback(tplRaw, skipSslCallback, NULL);
        sceHttpSetAutoRedirect();

        sprintf(rawUrl, "https://raw.githubusercontent.com/%s/%s/%s/saves.txt", repo, branch, path);
        int connRaw = sceHttpCreateConnectionWithURL(tplRaw, rawUrl, 1);
        int reqRaw = sceHttpCreateRequestWithURL(connRaw, ORBIS_METHOD_GET, rawUrl, 0);

        if (sceHttpSendRequest(reqRaw, NULL, 0) >= 0) {
            int code = 0; sceHttpGetStatusCode(reqRaw, &code);
            if (code == 200) {
                int nRaw = 0, totalRaw = 0;
                while ((nRaw = sceHttpReadData(reqRaw, (unsigned char*)(bufMap + totalRaw), 256 * 1024 - totalRaw - 1)) > 0) {
                    totalRaw += nRaw;
                    if (totalRaw >= 256 * 1024 - 1) break;
                }
                if (totalRaw > 0) hasMap = true;
            }
        }
        sceHttpDeleteRequest(reqRaw); sceHttpDeleteConnection(connRaw);

        if (!hasMap) {
            sprintf(rawUrl, "https://raw.githubusercontent.com/%s/%s/%s/games.txt", repo, branch, path);
            connRaw = sceHttpCreateConnectionWithURL(tplRaw, rawUrl, 1);
            reqRaw = sceHttpCreateRequestWithURL(connRaw, ORBIS_METHOD_GET, rawUrl, 0);

            if (sceHttpSendRequest(reqRaw, NULL, 0) >= 0) {
                int code = 0; sceHttpGetStatusCode(reqRaw, &code);
                if (code == 200) {
                    int nRaw = 0, totalRaw = 0;
                    while ((nRaw = sceHttpReadData(reqRaw, (unsigned char*)(bufMap + totalRaw), 256 * 1024 - totalRaw - 1)) > 0) {
                        totalRaw += nRaw;
                        if (totalRaw >= 256 * 1024 - 1) break;
                    }
                    if (totalRaw > 0) hasMap = true;
                }
            }
            sceHttpDeleteRequest(reqRaw); sceHttpDeleteConnection(connRaw);
        }
        sceHttpDeleteTemplate(tplRaw);
    }

    int tpl = sceHttpCreateTemplate(httpCtxId, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, apiUrl, 0);

    atualizarBarra(0.4f);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
        const char* tempPath = "/data/HyperNeiva/configuracao/temporario/apollo.html";
        FILE* f = fopen(tempPath, "wb");
        if (f) {
            unsigned char buf[16384]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f);

            atualizarBarra(0.8f);

            FILE* f2 = fopen(tempPath, "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                char* h = (char*)malloc(sz + 1);
                if (h) {
                    fread(h, 1, sz, f2); h[sz] = '\0';

                    char* p = h;
                    while ((p = strstr(p, "\"name\""))) {
                        char* colon = strchr(p, ':');
                        if (colon && (colon - p) < 15) {
                            char* qStart = strchr(colon, '\"');
                            if (qStart) {
                                char* qEnd = strchr(qStart + 1, '\"');
                                if (qEnd) {
                                    char itemName[128];
                                    int len = qEnd - (qStart + 1);
                                    if (len > 120) len = 120;
                                    strncpy(itemName, qStart + 1, len);
                                    itemName[len] = '\0';

                                    char translatedName[128];
                                    strncpy(translatedName, itemName, 127);
                                    translatedName[127] = '\0';
                                    bool isTranslated = false;

                                    if (hasMap) {
                                        // 1. CORREÇÃO MAGNÍFICA DO APOLLO:
                                        // O PS4 tem pastas com sufixos _00 (ex: CUSA12345_00) mas o games.txt só tem "CUSA12345".
                                        // Isto tira o _00 para o código encontrar o nome certo!
                                        char searchId[128];
                                        strncpy(searchId, itemName, 127); searchId[127] = '\0';
                                        char* uscore = strchr(searchId, '_');
                                        if (uscore) *uscore = '\0'; // Corta tudo a partir do '_'

                                        char* found = strstr(bufMap, searchId);
                                        while (found) {
                                            if (found == bufMap || *(found - 1) == '\n' || *(found - 1) == '\r') break;
                                            found = strstr(found + 1, searchId);
                                        }

                                        if (found) {
                                            char* linePtr = found + strlen(searchId);
                                            while (*linePtr == ' ' || *linePtr == '\t' || *linePtr == '-' || *linePtr == '=' || *linePtr == ':' || *linePtr == '\"') {
                                                linePtr++;
                                            }
                                            char* lineEnd = linePtr;
                                            while (*lineEnd != '\r' && *lineEnd != '\n' && *lineEnd != '\0') {
                                                lineEnd++;
                                            }
                                            if (lineEnd > linePtr && *(lineEnd - 1) == '\"') {
                                                lineEnd--;
                                            }
                                            int nameLen = lineEnd - linePtr;
                                            if (nameLen > 0 && nameLen < 120) {
                                                strncpy(translatedName, linePtr, nameLen);
                                                translatedName[nameLen] = '\0';
                                                isTranslated = true;
                                            }
                                        }
                                    }

                                    bool isDir = false;
                                    char* tPtr = strstr(p, "\"type\"");
                                    if (tPtr && tPtr < p + 500) {
                                        char* tColon = strchr(tPtr, ':');
                                        if (tColon) {
                                            char* tQ = strchr(tColon, '\"');
                                            if (tQ && strncmp(tQ + 1, "dir", 3) == 0) isDir = true;
                                        }
                                    }

                                    char itemUrl[512] = "";
                                    char* uPtr = NULL;

                                    if (isDir) uPtr = strstr(p, "\"html_url\"");
                                    else uPtr = strstr(p, "\"download_url\"");

                                    if (uPtr && uPtr < p + 1000) {
                                        char* uColon = strchr(uPtr, ':');
                                        if (uColon) {
                                            char* uQ1 = strchr(uColon, '\"');
                                            if (uQ1) {
                                                char* uQ2 = strchr(uQ1 + 1, '\"');
                                                if (uQ2) {
                                                    int uLen = uQ2 - (uQ1 + 1);
                                                    if (uLen > 500) uLen = 500;
                                                    strncpy(itemUrl, uQ1 + 1, uLen);
                                                    itemUrl[uLen] = '\0';
                                                    if (isDir) strcat(itemUrl, "/");
                                                }
                                            }
                                        }
                                    }

                                    if (strlen(itemName) > 0 && strlen(itemUrl) > 0) {
                                        if (itemName[0] != '.' && strcmp(itemName, "README.md") != 0 && strcmp(itemName, "LICENSE") != 0 && strcmp(itemName, "_config.yml") != 0 && strcmp(itemName, "gen_markdown.c") != 0 && strcmp(itemName, "games.txt") != 0 && strcmp(itemName, "saves.txt") != 0) {

                                            char displayStr[128];
                                            if (isTranslated) {
                                                sprintf(displayStr, "%s (%s)", translatedName, itemName);
                                            }
                                            else {
                                                strcpy(displayStr, itemName);
                                            }

                                            if (strlen(displayStr) > 58) {
                                                displayStr[58] = '\0';
                                            }

                                            char nomeFormatado[128];
                                            if (isDir) sprintf(nomeFormatado, "[ %s ]", displayStr);
                                            else strcpy(nomeFormatado, displayStr);

                                            bool exists = false;
                                            for (int i = 0; i < totalItens; i++) {
                                                if (strcmp(nomes[i], nomeFormatado) == 0) { exists = true; break; }
                                            }

                                            if (!exists && totalItens < 2900) {
                                                strcpy(nomes[totalItens], nomeFormatado);
                                                strcpy(linksAtuais[totalItens], itemUrl);
                                                strcpy(iconesAtuais[totalItens], "");
                                                totalItens++;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        p += 6;
                    }

                    if (totalItens == 0) {
                        if (strstr(h, "API rate limit exceeded")) {
                            strcpy(nomes[0], "Github Erro: Limite de acessos excedido! Tenta mais tarde.");
                        }
                        else if (strstr(h, "\"message\": \"Not Found\"")) {
                            strcpy(nomes[0], "Github Erro: Ficheiro ou Pasta nao encontrados (404)");
                        }
                        else if (strstr(h, "\"message\": \"Forbidden\"")) {
                            strcpy(nomes[0], "Github Erro: Acesso Negado (403 Forbidden)");
                        }
                        else {
                            char lixo[64];
                            strncpy(lixo, h, 60); lixo[60] = '\0';
                            for (int k = 0; k < strlen(lixo); k++) { if (lixo[k] == '\n' || lixo[k] == '\r') lixo[k] = ' '; }
                            sprintf(nomes[0], "DEBUG: %s", lixo);
                        }
                        totalItens = 1;
                    }
                    else {
                        sprintf(msgStatus, "PASTAS CARREGADAS COM SUCESSO!");
                    }

                    free(h);
                }
                fclose(f2);
            }
        }
    }
    else {
        sprintf(msgStatus, "ERRO AO ACESSAR O GITHUB");
        strcpy(nomes[0], "Servidor Offline ou Erro de Rede");
        totalItens = 1;
    }

    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);
    if (bufMap) free(bufMap);

    atualizarBarra(1.0f); msgTimer = 180;
    menuAtual = MENU_BAIXAR_DROPBOX_LISTA;
    sel = 0; off = 0;
}