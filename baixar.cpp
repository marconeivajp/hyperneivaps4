// --- INÍCIO DO ARQUIVO baixar.cpp ---
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
char linksAtuais[3000][1024]; // <-- ATUALIZADO PARA 1024
int totalLinksAtuais = 0;

char linksFavoritos[10][512];

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
    strcpy(nomes[3], "NAVEGADOR");
    totalItens = 4;
    menuAtual = MENU_BAIXAR;
    sel = 0; off = 0;
}

void preencherMenuNavegadorOpcoes() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "1. Pesquisar no Google");
    strcpy(nomes[1], "2. Digitar Site / URL");
    strcpy(nomes[2], "3. Links Salvos (Favoritos)");
    totalItens = 3;
    menuAtual = MENU_BAIXAR_NAVEGADOR_OPCOES;
    sel = 0; off = 0;
}

void preencherMenuNavegadorFavoritos() {
    memset(nomes, 0, sizeof(nomes));

    strcpy(nomes[0], "Google");
    strcpy(linksFavoritos[0], "https://www.google.com/");

    strcpy(nomes[1], "RetroArch Thumbnails");
    strcpy(linksFavoritos[1], "https://thumbnails.libretro.com/");

    totalItens = 2;
    menuAtual = MENU_BAIXAR_NAVEGADOR_FAVORITOS;
    sel = 0; off = 0;
}

void acessarSiteNavegador(const char* url) {
    if (!url || strlen(url) < 5) return;

    sprintf(msgStatus, "CARREGANDO SITE...");
    msgTimer = 120;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        FILE* f = fopen("/data/HyperNeiva/temp_site.html", "wb");
        if (f) {
            unsigned char buf[32768]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
            fclose(f);

            FILE* f2 = fopen("/data/HyperNeiva/temp_site.html", "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                char* h = (char*)malloc(sz + 1);
                if (h) {
                    fread(h, 1, sz, f2); h[sz] = '\0';
                    char* b = h;

                    while ((b = strstr(b, "href=\"")) && totalItens < 2900) {
                        b += 6;
                        char* e = strchr(b, '\"');
                        if (e) {
                            int l = (int)(e - b);
                            if (l > 1 && l < 1000) { // Limitado para segurança
                                char tempLink[1024]; // Buffer gigante para evitar vazamento
                                strncpy(tempLink, b, l); tempLink[l] = '\0';

                                // <-- CÓDIGO CORRIGIDO PARA NÃO CRASHAR NO GOOGLE -->
                                if (strncmp(tempLink, "/url?q=", 7) == 0) {
                                    char* startUrl = tempLink + 7;
                                    char* endUrl = strchr(startUrl, '&');
                                    if (endUrl) *endUrl = '\0';
                                    memmove(tempLink, startUrl, strlen(startUrl) + 1); // Memmove previne corrupção!
                                }

                                if (strstr(tempLink, ".css") || strstr(tempLink, ".js") || tempLink[0] == '#' || tempLink[0] == '?' ||
                                    strcasecmp(tempLink, "../") == 0 || strstr(tempLink, "google.com") || strstr(tempLink, "/search?") || tempLink[0] == '/') {
                                    b = e + 1; continue;
                                }

                                // <-- USANDO SNPRINTF PARA NUNCA ESTOURAR A MEMÓRIA -->
                                if (strncmp(tempLink, "http", 4) != 0) {
                                    if (url[strlen(url) - 1] == '/') snprintf(linksAtuais[totalItens], 1023, "%s%s", url, tempLink);
                                    else snprintf(linksAtuais[totalItens], 1023, "%s/%s", url, tempLink);
                                }
                                else {
                                    snprintf(linksAtuais[totalItens], 1023, "%s", tempLink);
                                }

                                char* nomeVisor = strrchr(tempLink, '/');
                                if (nomeVisor && strlen(nomeVisor) > 1 && tempLink[strlen(tempLink) - 1] != '/') {
                                    strncpy(nomes[totalItens], nomeVisor + 1, 63);
                                }
                                else {
                                    strncpy(nomes[totalItens], tempLink, 63);
                                }
                                nomes[totalItens][63] = '\0';

                                char* pN = nomes[totalItens], * qN = nomes[totalItens];
                                while (*pN) { if (*pN == '%' && *(pN + 1) == '2' && *(pN + 2) == '0') { *qN++ = ' '; pN += 3; } else { *qN++ = *pN++; } }
                                *qN = '\0';

                                totalItens++;
                            }
                        }
                        b = e ? e + 1 : b + 1;
                    }
                    free(h);
                }
                fclose(f2);
            }
        }
    }
    else {
        strcpy(nomes[0], "Erro de Conexao");
        totalItens = 1;
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhum arquivo encontrado");
        totalItens = 1;
    }

    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);

    menuAtual = MENU_BAIXAR_NAVEGADOR_LISTA;
    sel = 0; off = 0;
}

void preencherMenuRepositorios() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Games");
    totalItens = 1;
    menuAtual = MENU_BAIXAR_REPOS;
    sel = 0; off = 0;
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
    if (!fp) {
        memset(nomes, 0, sizeof(nomes));
        strcpy(nomes[0], "Erro ao abrir XML");
        totalItens = 1;
        menuAtual = MENU_BAIXAR_GAMES_LIST;
        return;
    }

    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);

    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    char* p = b;

    while (totalItens < 2000) {
        p = strstr(p, "<game name=\"");
        if (!p) break;

        p += 12;
        char* f = strchr(p, '\"');
        if (f) {
            int l = (int)(f - p);
            strncpy(nomes[totalItens], p, l);
            nomes[totalItens][l] = '\0';
            totalItens++;
            p = f;
        }
        else break;
    }
    free(b);

    if (totalItens == 0) {
        strcpy(nomes[0], "XML Vazio ou Invalido");
        totalItens = 1;
    }

    menuAtual = MENU_BAIXAR_GAMES_LIST;
    sel = 0; off = 0;
}

void mostrarLinksJogo(int gameIndex) {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0; totalLinksAtuais = 0;

    FILE* fp = fopen(caminhoXMLAtual, "rb");
    if (!fp) return;

    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* b = (char*)malloc(sz + 1); fread(b, 1, sz, fp); b[sz] = '\0'; fclose(fp);

    char* p = b;
    int currentIdx = -1;
    char* gameBlockStart = NULL;

    while (true) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        currentIdx++;
        if (currentIdx == gameIndex) {
            gameBlockStart = p;
            break;
        }
        p += 12;
    }

    if (gameBlockStart) {
        char* gameBlockEnd = strstr(gameBlockStart, "</game>");
        char* lPtr = gameBlockStart;

        while (totalLinksAtuais < 10) {
            lPtr = strstr(lPtr, "<link>");
            if (!lPtr || (gameBlockEnd && lPtr > gameBlockEnd)) break;

            lPtr += 6;
            char* lEnd = strstr(lPtr, "</link>");
            if (lEnd) {
                int len = (int)(lEnd - lPtr);
                char temp[512]; strncpy(temp, lPtr, len); temp[len] = '\0';

                char limpo[512] = { 0 }; int pos = 0;
                for (int i = 0; temp[i]; i++) {
                    if (strncmp(&temp[i], "&amp;", 5) == 0) { limpo[pos++] = '&'; i += 4; }
                    else limpo[pos++] = temp[i];
                }

                strcpy(linksAtuais[totalLinksAtuais], limpo);
                sprintf(nomes[totalItens], "Opcao %d", totalLinksAtuais + 1);
                totalLinksAtuais++; totalItens++;
                lPtr = lEnd;
            }
            else break;
        }
    }
    free(b);
    menuAtual = MENU_BAIXAR_LINKS;
    sel = 0; off = 0;
}

void iniciarDownload(const char* url) {
    if (!url || strlen(url) < 10) return;

    char nomeRepo[128] = "Geral";

    if (menuAtual == MENU_BAIXAR_LINK_DIRETO) {
        strcpy(nomeRepo, "Link_Direto");
    }
    else if (menuAtual == MENU_BAIXAR_NAVEGADOR_LISTA) {
        strcpy(nomeRepo, "Navegador");
    }
    else {
        char* ultimaBarra = strrchr(caminhoXMLAtual, '/');
        if (ultimaBarra) {
            strcpy(nomeRepo, ultimaBarra + 1);
            char* ponto = strstr(nomeRepo, ".xml");
            if (ponto) *ponto = '\0';
        }
    }

    char pathPasta[256];
    sprintf(pathPasta, "/data/HyperNeiva/baixado/%s", nomeRepo);
    sceKernelMkdir(pathPasta, 0777);

    char nomeArquivo[128] = "download.zip";
    char* ref = strrchr(url, '/');
    if (ref) {
        ref++;
        char* query = strchr(ref, '?');
        if (query) {
            int len = (int)(query - ref);
            strncpy(nomeArquivo, ref, len);
            nomeArquivo[len] = '\0';
        }
        else {
            strcpy(nomeArquivo, ref);
        }
    }

    char pathFinal[512];
    sprintf(pathFinal, "%s/%s", pathPasta, nomeArquivo);

    sprintf(msgStatus, "BAIXANDO: %s", nomeArquivo);
    msgTimer = 120;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        FILE* f = fopen(pathFinal, "wb");
        if (f) {
            unsigned char buf[32768];
            int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);
            sprintf(msgStatus, "CONCLUIDO: %s", nomeArquivo);
        }
        else {
            sprintf(msgStatus, "ERRO AO CRIAR ARQUIVO NO HD");
        }
    }
    else {
        sprintf(msgStatus, "ERRO DE CONEXAO");
    }

    msgTimer = 180;
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
}
// --- FIM DO ARQUIVO baixar.cpp ---