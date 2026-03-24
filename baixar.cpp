#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h> 

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>

#include "baixar.h"
#include "network.h"
#include "menu.h"
#include "graphics.h"
#include "stb_image.h"

extern int barX, barY, barW, barH;
extern int barBg, barFill;

extern uint32_t* obterBufferVideo();
extern void desenharInterface(uint32_t* p);
extern void submeterTela();

extern unsigned char* backImg;
extern int wB, hB;

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
char caminhoXMLAtual[256] = "";
char linksAtuais[3000][1024];
int totalLinksAtuais = 0;

char currentDropboxPath[512] = "";
char currentUploadPath[512] = "";

bool emSubmenuLojas = false;
bool emSubmenuDropbox = false; // <-- Nova flag para a pasta Dropbox
bool emApolloSaves = false;
char currentApolloUrl[1024] = "";

uint32_t getSysColorBarra(int index) {
    uint32_t sysColors[] = {
        0xAA222222, 0xAA000000, 0xAA000044, 0xAA440000, 0xAA004400, 0x00000000,
        0xFF444444, 0xFF00D83A, 0xAAFFFF99, 0xFF00FF00, 0xFF00AAFF, 0xAA999933,
        0xFFFFFFFF, 0xFFFF0000, 0xFF0000FF
    };
    if (index < 0 || index > 14) return sysColors[0];
    return sysColors[index];
}

void atualizarBarra(float progresso) {
    atualizarBarra(progresso, 1, 1);
}

void atualizarBarra(float progresso, int arquivoAtual, int totalArquivos) {
    uint32_t* p = obterBufferVideo();

    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;

    if (backImg) desenharRedimensionado(p, backImg, wB, hB, 1920, 1080, 0, 0);

    desenharInterface(p);

    int bX = barX; int bY = barY; int bW = barW; int bH = barH;

    for (int y = bY; y < bY + bH; y++) {
        for (int x = bX; x < bX + bW; x++) p[y * 1920 + x] = getSysColorBarra(barBg);
    }

    int fill = (int)(bW * progresso);
    if (fill > bW) fill = bW;
    if (fill < 0) fill = 0;

    for (int y = bY; y < bY + bH; y++) {
        for (int x = bX; x < bX + fill; x++) p[y * 1920 + x] = getSysColorBarra(barFill);
    }

    int porcentagem = (int)(progresso * 100.0f);
    if (porcentagem > 100) porcentagem = 100;
    if (porcentagem < 0) porcentagem = 0;

    char textoLoad[128];
    snprintf(textoLoad, sizeof(textoLoad), "%d%%   -   %d / %d", porcentagem, arquivoAtual, totalArquivos);
    desenharTexto(p, textoLoad, 25, bX + bW + 20, bY - 2, 0xFFFFFFFF);
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
            float pFake = 0.1f;

            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
                pFake += 0.05f;
                if (pFake > 0.99f) pFake = 0.99f;
                atualizarBarra(pFake, 1, 1);
            }
            fclose(f);
            atualizarBarra(1.0f, 1, 1);

            if (buscarLista) {
                FILE* f2 = fopen("/data/HyperNeiva/remote_list.html", "rb");
                if (f2) {
                    fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
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
                    free(h);
                }
                menuAtual = SCRAPER_LIST; sel = 0; off = 0;
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
    emSubmenuLojas = false;
    emSubmenuDropbox = false; // Reset da pasta Dropbox
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Repositorios");
    strcpy(nomes[1], "LINK DIRETO");
    strcpy(nomes[2], "Dropbox"); // <-- Pasta Dropbox criada
    strcpy(nomes[3], "Lojas");
    totalItens = 4;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

void preencherMenuDropbox() {
    emSubmenuLojas = false;
    emSubmenuDropbox = true; // Ativa a pasta Dropbox
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Download");
    strcpy(nomes[1], "Menu Backup (Upload)");
    totalItens = 2;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

void preencherMenuLojas() {
    emSubmenuLojas = true;
    emSubmenuDropbox = false;
    emApolloSaves = false;
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "HB Store");
    strcpy(nomes[1], "Apollo Saves");
    strcpy(nomes[2], "Retroarch");
    totalItens = 3;
    menuAtual = MENU_BAIXAR;
    sel = 0;
    off = 0;
}

const char* pathFilaTxt = "/data/HyperNeiva/configuracao/fila_downloads.txt";
const char* pathTempTxt = "/data/HyperNeiva/configuracao/fila_temp.txt";

bool adicionarLinkFila(const char* link) {
    FILE* fIn = fopen(pathFilaTxt, "r");
    if (fIn) {
        char linha[1024];
        while (fgets(linha, sizeof(linha), fIn)) {
            linha[strcspn(linha, "\r\n")] = 0;
            if (strcmp(linha, link) == 0) { fclose(fIn); return false; }
        }
        fclose(fIn);
    }

    FILE* fOut = fopen(pathFilaTxt, "a");
    if (fOut) { fprintf(fOut, "%s\n", link); fclose(fOut); return true; }
    return false;
}

bool obterPrimeiroLinkFila(char* linkSaida) {
    FILE* f = fopen(pathFilaTxt, "r");
    if (!f) return false;
    if (fgets(linkSaida, 1024, f)) { linkSaida[strcspn(linkSaida, "\r\n")] = 0; fclose(f); return (strlen(linkSaida) > 0); }
    fclose(f); return false;
}

void removerPrimeiroLinkFila() {
    FILE* fIn = fopen(pathFilaTxt, "r"); if (!fIn) return;
    FILE* fOut = fopen(pathTempTxt, "w"); if (!fOut) { fclose(fIn); return; }
    char linha[1024]; bool primeiraLinha = true;
    while (fgets(linha, sizeof(linha), fIn)) {
        if (primeiraLinha) { primeiraLinha = false; continue; }
        fprintf(fOut, "%s", linha);
    }
    fclose(fIn); fclose(fOut);
    unlink(pathFilaTxt); rename(pathTempTxt, pathFilaTxt);
}

int contarLinksFila() {
    FILE* f = fopen(pathFilaTxt, "r"); if (!f) return 0;
    int contagem = 0; char linha[1024];
    while (fgets(linha, sizeof(linha), f)) { if (strlen(linha) > 2) contagem++; }
    fclose(f); return contagem;
}

void processarFilaDownloads() {
    char linkAtual[1024];
    while (obterPrimeiroLinkFila(linkAtual)) {
        int totalRestante = contarLinksFila();
        removerPrimeiroLinkFila();
    }
}