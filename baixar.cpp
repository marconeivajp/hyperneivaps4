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

extern uint32_t* obterBufferVideo();
extern void desenharInterface(uint32_t* p);
extern void submeterTela();

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

// Variáveis agora expostas para serem usadas no outro arquivo
char currentDropboxPath[512] = "";
char currentUploadPath[512] = "";

void atualizarBarra(float progresso) {
    uint32_t* p = obterBufferVideo();
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;
    desenharInterface(p);

    int bX = 50;
    int bY = 940;
    int bW = 400;
    int bH = 15;

    for (int y = bY; y < bY + bH; y++) {
        for (int x = bX; x < bX + bW; x++) {
            p[y * 1920 + x] = 0xFF444444;
        }
    }

    int fill = (int)(bW * progresso);
    if (fill > bW) fill = bW;
    if (fill < 0) fill = 0;

    for (int y = bY; y < bY + bH; y++) {
        for (int x = bX; x < bX + fill; x++) {
            p[y * 1920 + x] = 0xFF00D83A;
        }
    }
    submeterTela();
}

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
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
                atualizarBarra(0.5f);
            }
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
    strcpy(nomes[4], "DROPBOX (MENU BACKUP)");
    totalItens = 5; menuAtual = MENU_BAIXAR; sel = 0; off = 0;
}