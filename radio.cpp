#include "radio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <orbis/Http.h>
#include <orbis/libkernel.h>
#include "audio.h" 
#include "menu_grafico.h"
#include "network.h"

extern char linksAtuais[3000][1024];
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern int off;
extern MenuLevel menuAtual;

char radioBrowserQuery[128] = "";
char radioFavPath[] = "/data/HyperNeiva/configuracao/radio_favoritos.json";

char debugHttpInfo[256] = "";

void preencherMenuRadioCategorias() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Radios Populares (Top 50)");
    strcpy(nomes[1], "Radios de Sao Paulo (89 FM, Kiss, etc)");
    strcpy(nomes[2], "Podcasts (Tags)");
    strcpy(nomes[3], "Pesquisar (Regiao ou Estilo)");
    strcpy(nomes[4], "Meus Favoritos");
    totalItens = 5; sel = 0; off = 0; menuAtual = MENU_RADIO_CATEGORIA;
}

int baixarArquivoRadio(const char* url, const char* localPath) {
    extern int httpCtxId;
    int ret = 0;

    int tmpl = sceHttpCreateTemplate(httpCtxId, "HyperNeiva/1.0", 1, 1);
    bool ownTmpl = true;
    if (tmpl < 0) {
        tmpl = httpCtxId;
        ownTmpl = false;
    }

    int conn = sceHttpCreateConnectionWithURL(tmpl, url, 0);
    if (conn < 0) {
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return conn;
    }

    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    if (req < 0) {
        sceHttpDeleteConnection(conn);
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return req;
    }

    sceHttpAddRequestHeader(req, "User-Agent", "HyperNeiva/1.0 (PS4)", 0);

    ret = sceHttpSendRequest(req, NULL, 0);
    if (ret < 0) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return ret;
    }

    int statusCode = 0;
    sceHttpGetStatusCode(req, &statusCode);
    if (statusCode != 200) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        if (ownTmpl) sceHttpDeleteTemplate(tmpl);
        return -2;
    }

    FILE* f = fopen(localPath, "wb");
    if (f) {
        unsigned char buf[32768];
        int n;
        while ((n = sceHttpReadData(req, buf, sizeof(buf))) > 0) {
            fwrite(buf, 1, n, f);
        }
        fclose(f);
        if (n < 0) {
            ret = n;
        }
        else {
            ret = 0;
        }
    }
    else {
        ret = -3;
    }

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    if (ownTmpl) sceHttpDeleteTemplate(tmpl);

    return ret;
}

void* threadBuscarRadio(void* arg) {
    char url[512];
    const char* jsonPath = "/data/HyperNeiva/radio_api.json";

    // Servidor global 'all' como primário. Se falhar, usa o 'de1'.
    const char* servidores[] = {
        "all.api.radio-browser.info",
        "de1.api.radio-browser.info"
    };

    int downloadStatus = -1;

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    totalItens = 1;

    // Tenta descarregar o JSON
    for (int i = 0; i < 2; i++) {
        sprintf(nomes[0], "Conectando ao servidor (%d/2)...", i + 1);

        if (strcmp(radioBrowserQuery, "topclick") == 0) {
            sprintf(url, "http://%s/json/stations/topclick/50", servidores[i]);
        }
        else if (strcmp(radioBrowserQuery, "saopaulo") == 0) {
            sprintf(url, "http://%s/json/stations/search?state=Sao%%20Paulo&limit=50&order=clickcount&reverse=true", servidores[i]);
        }
        else if (strlen(radioBrowserQuery) > 0) {
            sprintf(url, "http://%s/json/stations/search?name=%s&limit=50", servidores[i], radioBrowserQuery);
        }
        else {
            sprintf(url, "http://%s/json/stations/topclick/50", servidores[i]);
        }

        downloadStatus = baixarArquivoRadio(url, jsonPath);

        if (downloadStatus == 0) {
            break; // Sucesso!
        }
        else {
            sceKernelUsleep(1000000);
        }
    }

    // Leitura e Preenchimento da Lista
    if (downloadStatus == 0) {
        strcpy(nomes[0], "Lendo estacoes...");

        FILE* f = fopen(jsonPath, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* buffer = (char*)malloc(fsize + 1);
            if (buffer) {
                fread(buffer, 1, fsize, f);
                buffer[fsize] = '\0';

                memset(nomes, 0, sizeof(nomes));
                memset(linksAtuais, 0, sizeof(linksAtuais));
                totalItens = 0;

                char* ptr = buffer;

                // O Parser Blindado (Imune a erros de JSON da API)
                while (totalItens < 50 && ptr != NULL) {
                    char* namePtr = strstr(ptr, "\"name\":\"");
                    if (!namePtr) break;

                    char* streamPtr = strstr(namePtr, "\"url_resolved\":\"");
                    if (!streamPtr) break;

                    namePtr += 8;
                    char* nameEnd = strchr(namePtr, '\"');
                    if (!nameEnd) break;

                    streamPtr += 16;
                    char* streamEnd = strchr(streamPtr, '\"');
                    if (!streamEnd) break;

                    int nameLen = nameEnd - namePtr;
                    if (nameLen > 63) nameLen = 63;
                    strncpy(nomes[totalItens], namePtr, nameLen);
                    nomes[totalItens][nameLen] = '\0';

                    int streamLen = streamEnd - streamPtr;
                    if (streamLen > 1023) streamLen = 1023;
                    strncpy(linksAtuais[totalItens], streamPtr, streamLen);
                    linksAtuais[totalItens][streamLen] = '\0';

                    totalItens++;
                    ptr = streamEnd; // Avança o leitor no texto
                }
                free(buffer);
            }
            fclose(f);
        }
    }
    else {
        strcpy(nomes[0], "Erro de Rede: Nao foi possivel baixar a lista.");
        totalItens = 1;
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhuma estacao encontrada.");
        totalItens = 1;
    }

    return NULL;
}

void buscarEstacoesRadio(const char* query, bool isPodcast) {
    strcpy(radioBrowserQuery, query);
    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));
    strcpy(nomes[0], "Preparando Rede...");
    totalItens = 1; sel = 0; off = 0;
    menuAtual = MENU_RADIO_LISTA;

    pthread_t t;
    pthread_create(&t, NULL, threadBuscarRadio, (void*)(uintptr_t)isPodcast);
    pthread_detach(t);
}

void acaoCross_Radio() {
    if (menuAtual == MENU_RADIO_CATEGORIA) {
        if (sel == 0) buscarEstacoesRadio("topclick", false);
        else if (sel == 1) buscarEstacoesRadio("saopaulo", false);
        else if (sel == 2) buscarEstacoesRadio("podcast", true);
        else if (sel == 3) {
            extern void exibirTecladoVirtual(int tipo);
            exibirTecladoVirtual(10);
        }
        else if (sel == 4) { menuAtual = MENU_RADIO_FAVORITOS; }
    }
    else if (menuAtual == MENU_RADIO_LISTA) {
        if (strlen(linksAtuais[sel]) > 5) {
            char linkConvertido[1024];

            // CONVERSÃO SEGURA: Garante que o FFmpeg consegue tocar (Força HTTP)
            if (strncmp(linksAtuais[sel], "https://", 8) == 0) {
                sprintf(linkConvertido, "http://%s", linksAtuais[sel] + 8);
            }
            else {
                strcpy(linkConvertido, linksAtuais[sel]);
            }

            tocarMusicaNova(linkConvertido);
        }
    }
}

void preencherMenuOpcoesRadio() {
    extern const char* listaOpcoes[150];
    extern int totalOpcoes;
    extern bool showOpcoes;
    extern int mapOpcoes[150];
    extern int selOpcao;
    listaOpcoes[0] = "Pesquisar"; mapOpcoes[0] = 700;
    listaOpcoes[1] = "Adicionar aos Favoritos"; mapOpcoes[1] = 701;
    totalOpcoes = 2; showOpcoes = true; selOpcao = 0;
}

void acaoTriangle_Radio() {
    preencherMenuOpcoesRadio();
}

void acaoCircle_Radio() {
    // Agora o controle central (controle.cpp) cuida de fechar Overlays
    // e da navegação de volta (voltarNavegacao).
    // Esta função pode ser usada para disparar efeitos colaterais específicos se necessário.
}