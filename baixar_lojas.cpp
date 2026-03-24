#ifndef __builtin_va_list
#define __builtin_va_list char*
#endif

#include "baixar_lojas.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>

#include "menu.h"
#include "network.h"

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

void acessarHBStore() {
    sprintf(msgStatus, "VARRENDO A HB-STORE...");
    atualizarBarra(0.1f);
    msgTimer = 120;

    int tpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
    sceHttpSetAutoRedirect();

    const char* apiUrl = "https://pkg-zone.com/";

    int conn = sceHttpCreateConnectionWithURL(tpl, apiUrl, 1);
    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, apiUrl, 0);

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    memset(marcados, 0, sizeof(marcados));
    totalItens = 0;

    atualizarBarra(0.4f);

    if (sceHttpSendRequest(req, NULL, 0) >= 0) {
        sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
        const char* tempPath = "/data/HyperNeiva/configuracao/temporario/loja.html";
        FILE* f = fopen(tempPath, "wb");
        if (f) {
            unsigned char buf[16384]; int n;
            while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);

            atualizarBarra(0.8f);
            sprintf(msgStatus, "CRIANDO LINKS FANTASMAS...");

            FILE* f2 = fopen(tempPath, "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long sz = ftell(f2); fseek(f2, 0, SEEK_SET);
                char* h = (char*)malloc(sz + 1);
                if (h) {
                    fread(h, 1, sz, f2); h[sz] = '\0';

                    char* b = h;
                    while ((b = strstr(b, "href=\"")) && totalItens < 2900) {
                        b += 6;

                        char* aspasFim = strchr(b, '\"');
                        if (!aspasFim) break;

                        int tamanhoLink = aspasFim - b;

                        if (tamanhoLink > 5 && tamanhoLink < 500) {
                            char linkBruto[512];
                            strncpy(linkBruto, b, tamanhoLink);
                            linkBruto[tamanhoLink] = '\0';

                            // ==========================================================
                            // BUSCA AGRESSIVA: Acha o ID do jogo não importa como o link esteja!
                            // ==========================================================
                            char* tagDetalhes = strstr(linkBruto, "/details/");
                            if (!tagDetalhes) tagDetalhes = strstr(linkBruto, "/title/"); // Tenta /title/ também

                            if (tagDetalhes) {
                                char titleId[64];

                                // Pula a palavra "/details/" (9 letras) ou "/title/" (7 letras)
                                if (strstr(linkBruto, "/details/")) strcpy(titleId, tagDetalhes + 9);
                                else strcpy(titleId, tagDetalhes + 7);

                                // Limpa qualquer lixo que venha depois do ID (barras, interrogações, etc)
                                char* lixo = strpbrk(titleId, "/?#\"\'");
                                if (lixo) *lixo = '\0';

                                // Valida se é um ID real (maior que 3 letras, ex: SKID02048)
                                if (strlen(titleId) >= 4 && strlen(titleId) < 20) {
                                    bool jaExiste = false;
                                    for (int i = 0; i < totalItens; i++) {
                                        if (strcasecmp(nomes[i], titleId) == 0) {
                                            jaExiste = true;
                                            break;
                                        }
                                    }

                                    if (!jaExiste) {
                                        strcpy(nomes[totalItens], titleId);
                                        // MÁGICA: Constrói o link de download direto validado
                                        sprintf(linksAtuais[totalItens], "https://pkg-zone.com/download/ps4/%s/latest", titleId);
                                        totalItens++;
                                    }
                                }
                            }
                            // ==========================================================
                            // MANTER SUPORTE A LINKS DIRETOS .PKG (Ex: Store-R2.pkg)
                            // ==========================================================
                            else if (linkBruto[tamanhoLink - 4] == '.' &&
                                (linkBruto[tamanhoLink - 3] == 'p' || linkBruto[tamanhoLink - 3] == 'P') &&
                                (linkBruto[tamanhoLink - 2] == 'k' || linkBruto[tamanhoLink - 2] == 'K') &&
                                (linkBruto[tamanhoLink - 1] == 'g' || linkBruto[tamanhoLink - 1] == 'G')) {

                                char nomeTela[128];
                                char* ultimaBarra = strrchr(linkBruto, '/');
                                if (ultimaBarra) strcpy(nomeTela, ultimaBarra + 1);
                                else strcpy(nomeTela, linkBruto);

                                char* ext = strstr(nomeTela, ".pkg");
                                if (!ext) ext = strstr(nomeTela, ".PKG");
                                if (ext) *ext = '\0';

                                bool jaExiste = false;
                                for (int i = 0; i < totalItens; i++) {
                                    if (strcasecmp(nomes[i], nomeTela) == 0) {
                                        jaExiste = true;
                                        break;
                                    }
                                }

                                if (!jaExiste && strlen(nomeTela) > 0) {
                                    strcpy(nomes[totalItens], nomeTela);

                                    if (strncmp(linkBruto, "http", 4) == 0) strcpy(linksAtuais[totalItens], linkBruto);
                                    else if (linkBruto[0] == '/') sprintf(linksAtuais[totalItens], "https://pkg-zone.com%s", linkBruto);
                                    else sprintf(linksAtuais[totalItens], "https://pkg-zone.com/%s", linkBruto);

                                    totalItens++;
                                }
                            }
                        }
                        b = aspasFim + 1;
                    }
                    free(h);
                }
                fclose(f2);

                if (totalItens > 0) sprintf(msgStatus, "LOJA CARREGADA COM %d APPS!", totalItens);
                else { strcpy(nomes[0], "Nenhum App encontrado na pagina principal"); totalItens = 1; }
            }
        }
    }
    else {
        sprintf(msgStatus, "ERRO AO ACESSAR A LOJA");
        strcpy(nomes[0], "Verifique a sua internet");
        totalItens = 1;
    }

    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl);

    atualizarBarra(1.0f); msgTimer = 180;

    menuAtual = MENU_BAIXAR_DROPBOX_LISTA;
    sel = 0; off = 0;
}