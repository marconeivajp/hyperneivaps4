// --- INÍCIO DO ARQUIVO controle_root.cpp ---
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h> // ADICIONADO PARA VERIFICAR DIRETÓRIOS

#ifdef __INTELLISENSE__
#define __builtin_va_list void*
#endif

#include "controle_root.h"
#include "menu.h"
#include "stb_image.h"
#include "audio.h" // IMPORTADO PARA TOCAR A MÚSICA

// --- NOVAS VARIÁVEIS GLOBAIS PARA O VISUALIZADOR DE IMAGENS ---
bool visualizandoMidiaImagem = false;
unsigned char* imgMidia = NULL;
int wM = 0, hM = 0, cM = 0;

extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern char nomes[3000][64];
extern char ultimoJogoCarregado[64];
extern unsigned char* imgPreview;
extern char bufferTecladoC[128];

// Variáveis de estado e notificação
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
    // Se estiver visualizando uma imagem em tela cheia, ignora o botão X
    if (visualizandoMidiaImagem) return;

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
        else if (sel == 6) {
            menuAtual = MENU_NOTEPAD;
            memset(bufferTecladoC, 0, sizeof(bufferTecladoC));
        }
    }
    else if (menuAtual == JOGAR_XML && strcasecmp(nomes[sel], "sp") == 0) {
        carregarXML("/app0/assets/sp.xml");
    }
    else if (menuAtual == MENU_MIDIA) { // <-- DENTRO DA ABA MÍDIA
        if (strcmp(nomes[sel], "Pasta vazia") == 0) return;

        char novoCaminho[512];
        sprintf(novoCaminho, "%s/%s", caminhoMidiaAtual, nomes[sel]);

        DIR* chk = opendir(novoCaminho);
        if (chk) {
            // É UMA PASTA: Entra nela
            closedir(chk);
            abrirPastaMidia(novoCaminho);
        }
        else {
            // É UM ARQUIVO: Verifica o formato
            int len = strlen(nomes[sel]);

            // 1. VERIFICA SE É ÁUDIO (MP3 ou WAV)
            if (len > 4 && (strcasecmp(&nomes[sel][len - 4], ".mp3") == 0 || strcasecmp(&nomes[sel][len - 4], ".wav") == 0)) {
                tocarMusicaNova(novoCaminho);
                sprintf(msgStatus, "TOCANDO: %s", nomes[sel]);
                msgTimer = 180;
            }
            // 2. VERIFICA SE É IMAGEM (PNG, JPG, JPEG)
            else if ((len > 4 && (strcasecmp(&nomes[sel][len - 4], ".png") == 0 || strcasecmp(&nomes[sel][len - 4], ".jpg") == 0)) ||
                (len > 5 && strcasecmp(&nomes[sel][len - 5], ".jpeg") == 0)) {

                // Limpa imagem anterior, caso exista na memória
                if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }

                // Carrega a nova imagem
                imgMidia = stbi_load(novoCaminho, &wM, &hM, &cM, 4);
                if (imgMidia) {
                    visualizandoMidiaImagem = true; // Ativa o modo tela cheia
                }
                else {
                    strcpy(msgStatus, "ERRO AO CARREGAR IMAGEM");
                    msgTimer = 120;
                }
            }
            // 3. OUTROS ARQUIVOS NÃO SUPORTADOS
            else {
                strcpy(msgStatus, "ARQUIVO NAO SUPORTADO");
                msgTimer = 120;
            }
        }
    }
}

void acaoCircle_Root() {
    // Se a imagem estiver aberta, o Bolinha apenas fecha a imagem e não sai da pasta
    if (visualizandoMidiaImagem) {
        visualizandoMidiaImagem = false;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
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