// --- INÍCIO DO ARQUIVO controle_root.cpp ---
#include <stdio.h>
#include <stdlib.h> 
#include <stdarg.h>
#include <string.h>
#include <dirent.h>

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

#include "controle_root.h"
#include "menu.h"
#include "stb_image.h"
#include "audio.h"
#include "bloco_de_notas.h"

bool visualizandoMidiaImagem = false;
unsigned char* imgMidia = NULL;
int wM = 0, hM = 0, cM = 0;
float zoomMidia = 1.0f;
bool fullscreenMidia = false;

bool visualizandoMidiaTexto = false;
char* textoMidiaBuffer = NULL;
char* linhasTexto[5000];
int totalLinhasTexto = 0;
int textoMidiaScroll = 0;

extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern char nomes[3000][64];
extern char ultimoJogoCarregado[64];
extern unsigned char* imgPreview;
extern char bufferTecladoC[128];

extern char caminhoXMLAtual[256];
extern char caminhoMidiaAtual[512];
extern char msgStatus[128];
extern int msgTimer;

extern void carregarXML(const char* path);
extern void preencherMenuBaixar();
extern void preencherMenuEditar();
extern void preencherExplorerHome();
extern void preencherMenuMusicas();
extern void preencherRoot();
extern void preencherMenuMidia();
extern void abrirPastaMidia(const char* caminho);

void acaoCross_Root() {
    if (visualizandoMidiaImagem) {
        fullscreenMidia = !fullscreenMidia;
        if (!fullscreenMidia) zoomMidia = 1.0f;
        return;
    }

    if (visualizandoMidiaTexto) return;

    if (menuAtual == ROOT) {
        if (sel == 0) carregarXML("/app0/assets/lista.xml");
        else if (sel == 1) { preencherMenuMidia(); sel = 0; off = 0; }
        else if (sel == 2) { preencherMenuBaixar(); sel = 0; off = 0; }
        else if (sel == 3) { preencherMenuEditar(); sel = 0; off = 0; }
        else if (sel == 4) { preencherExplorerHome(); sel = 0; off = 0; }
        else if (sel == 5) {
            if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
            strcpy(ultimoJogoCarregado, ""); preencherMenuMusicas(); sel = 0; off = 0;
        }
        // O sel == 6 ("CRIAR") foi completamente removido daqui
    }
    else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) {
        carregarXML("/app0/assets/sp.xml");
    }
    else if (menuAtual == MENU_MIDIA) {
        if (strcmp(nomes[sel], "Pasta vazia") == 0) return;

        char novoCaminho[512];
        sprintf(novoCaminho, "%s/%s", caminhoMidiaAtual, nomes[sel]);

        DIR* chk = opendir(novoCaminho);
        if (chk) {
            closedir(chk);
            abrirPastaMidia(novoCaminho);
        }
        else {
            int len = strlen(nomes[sel]);

            if (len > 4 && (strcasecmp(&nomes[sel][len - 4], ".mp3") == 0 || strcasecmp(&nomes[sel][len - 4], ".wav") == 0)) {
                tocarMusicaNova(novoCaminho);
                sprintf(msgStatus, "TOCANDO: %s", nomes[sel]);
                msgTimer = 180;
            }
            else if ((len > 4 && (strcasecmp(&nomes[sel][len - 4], ".png") == 0 || strcasecmp(&nomes[sel][len - 4], ".jpg") == 0)) ||
                (len > 5 && strcasecmp(&nomes[sel][len - 5], ".jpeg") == 0)) {

                if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
                imgMidia = stbi_load(novoCaminho, &wM, &hM, &cM, 4);
                if (imgMidia) {
                    visualizandoMidiaImagem = true;
                    zoomMidia = 1.0f;
                    fullscreenMidia = false;
                }
                else {
                    strcpy(msgStatus, "ERRO AO CARREGAR IMAGEM");
                    msgTimer = 120;
                }
            }
            else if ((len > 4 && (strcasecmp(&nomes[sel][len - 4], ".txt") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".xml") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".ini") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".cpp") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".doc") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".bin") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".log") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".csv") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".bat") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".css") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".php") == 0 ||
                strcasecmp(&nomes[sel][len - 4], ".pdf") == 0)) ||
                (len > 5 && (strcasecmp(&nomes[sel][len - 5], ".json") == 0 ||
                    strcasecmp(&nomes[sel][len - 5], ".docx") == 0 ||
                    strcasecmp(&nomes[sel][len - 5], ".html") == 0 ||
                    strcasecmp(&nomes[sel][len - 5], ".yaml") == 0)) ||
                (len > 3 && (strcasecmp(&nomes[sel][len - 3], ".md") == 0 ||
                    strcasecmp(&nomes[sel][len - 3], ".js") == 0 ||
                    strcasecmp(&nomes[sel][len - 3], ".py") == 0 ||
                    strcasecmp(&nomes[sel][len - 3], ".sh") == 0)) ||
                (len > 2 && (strcasecmp(&nomes[sel][len - 2], ".h") == 0 ||
                    strcasecmp(&nomes[sel][len - 2], ".c") == 0))) {

                FILE* f = fopen(novoCaminho, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long fsize = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    if (textoMidiaBuffer) free(textoMidiaBuffer);
                    textoMidiaBuffer = (char*)malloc(fsize + 1);

                    if (textoMidiaBuffer) {
                        fread(textoMidiaBuffer, 1, fsize, f);
                        textoMidiaBuffer[fsize] = '\0';
                        fclose(f);

                        abrirTextoNoNotepad(textoMidiaBuffer);
                        menuAtual = MENU_NOTEPAD;

                        free(textoMidiaBuffer);
                        textoMidiaBuffer = NULL;
                    }
                    else {
                        fclose(f);
                        strcpy(msgStatus, "ARQUIVO GIGANTE DEMAIS!");
                        msgTimer = 120;
                    }
                }
                else {
                    strcpy(msgStatus, "ERRO AO LER ARQUIVO");
                    msgTimer = 120;
                }
            }
            else {
                strcpy(msgStatus, "ARQUIVO NAO SUPORTADO");
                msgTimer = 120;
            }
        }
    }
}

void acaoCircle_Root() {
    if (visualizandoMidiaImagem) {
        visualizandoMidiaImagem = false;
        fullscreenMidia = false;
        zoomMidia = 1.0f;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        return;
    }

    if (visualizandoMidiaTexto) {
        visualizandoMidiaTexto = false;
        if (textoMidiaBuffer) { free(textoMidiaBuffer); textoMidiaBuffer = NULL; }
        return;
    }

    if (menuAtual == JOGAR_XML) {
        if (strstr(caminhoXMLAtual, "sp.xml")) carregarXML("/app0/assets/lista.xml");
        else preencherRoot();
    }
    else if (menuAtual == MENU_MIDIA) {
        if (strcmp(caminhoMidiaAtual, "/data/HyperNeiva/midia") == 0) {
            preencherRoot();
        }
        else {
            char* ultimaBarra = strrchr(caminhoMidiaAtual, '/');
            if (ultimaBarra != NULL) {
                *ultimaBarra = '\0';
            }
            abrirPastaMidia(caminhoMidiaAtual);
        }
    }
}
// --- FIM DO ARQUIVO controle_root.cpp ---