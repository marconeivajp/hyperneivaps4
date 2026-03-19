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
#include <stdarg.h> //

// ==========================================================
// VARIÁVEIS DO SCRAPING E RETROARCH
// ==========================================================
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

// ==========================================================
// VARIÁVEIS DOS REPOSITÓRIOS XML
// ==========================================================
char caminhoXMLAtual[256];
char linksAtuais[10][512];
int totalLinksAtuais = 0;

// ==========================================================
// LÓGICA DE SCRAPING DE IMAGENS (HTTP)
// ==========================================================
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

// ==========================================================
// REPOSITÓRIOS - LÓGICA COPIADA DO JOGAR.CPP
// ==========================================================

void preencherMenuBaixar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Repositorios");
    strcpy(nomes[1], "CAPAS");
    totalItens = 2;
    menuAtual = MENU_BAIXAR;
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

// Cópia 1:1 da lógica do seu carregarXML no jogar.cpp
void abrirXMLRepositorio(const char* xmlFile) {
    sprintf(caminhoXMLAtual, "/data/HyperNeiva/baixado/repositorio/games/%s", xmlFile);

    FILE* fp = fopen(caminhoXMLAtual, "rb"); // Usando "rb" como no seu código
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
        // Sua busca direta pela string
        p = strstr(p, "<game name=\"");
        if (!p) break;

        p += 12; // Salta <game name="
        char* f = strchr(p, '\"');
        if (f) {
            int l = (int)(f - p);
            strncpy(nomes[totalItens], p, l);
            nomes[totalItens][l] = '\0';
            totalItens++;
            p = f; // Continua a busca a partir da aspa final
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

// Mesma lógica para garantir que encontramos os links do jogo certo
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

    // Localiza o bloco do jogo pelo índice clicado
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

                // Limpeza manual do &amp; para a URL ficar correta
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

    // 1. Extrair o nome do Repositório do caminho do XML (ex: Sega_Master_System)
    char nomeRepo[128] = "Geral";
    char* ultimaBarra = strrchr(caminhoXMLAtual, '/');
    if (ultimaBarra) {
        strcpy(nomeRepo, ultimaBarra + 1);
        char* ponto = strstr(nomeRepo, ".xml");
        if (ponto) *ponto = '\0'; // Remove o .xml para ficar só o nome da pasta
    }

    // 2. Criar a pasta do repositório no HDD
    char pathPasta[256];
    sprintf(pathPasta, "/data/HyperNeiva/baixado/%s", nomeRepo);
    sceKernelMkdir(pathPasta, 0777);

    // 3. Extrair o nome original do arquivo da URL (Dropbox)
    char nomeArquivo[128] = "download.zip";
    char* ref = strrchr(url, '/');
    if (ref) {
        ref++; // Pula a barra
        char* query = strchr(ref, '?'); // Remove o ?dl=1 e outros parâmetros
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

    // 4. Iniciar a requisição HTTP
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
            unsigned char buf[32768]; // Buffer de 32KB para download rápido
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