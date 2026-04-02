#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdio.h>
#include <stdlib.h> 
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <strings.h> 

#include "controle_root.h"
#include "menu.h"
#include "stb_image.h"
#include "audio.h"
#include "bloco_de_notas.h"

// DECLARAÇÃO DOS NOVOS MENUS
extern void preencherMenuExtra();
extern void preencherMenuInformacao();

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
extern char linksAtuais[3000][1024];
extern char ultimoJogoCarregado[64];
extern unsigned char* imgPreview;
extern char bufferTecladoC[128];

extern char caminhoXMLAtual[256];
extern char caminhoMidiaAtual[512];
extern char msgStatus[128];
extern int msgTimer;
extern int timeToLoadCapa;

extern void carregarXML(const char* path);
extern void preencherMenuBaixar();
extern void preencherMenuEditar();
extern void preencherExplorerHome();
extern void preencherMenuMusicas();
extern void preencherRoot();
extern void preencherMenuMidia();
extern void abrirPastaMidia(const char* caminho);
extern void chamarJogo(const char* titleId, const char* romPath);

// =========================================================================
// FILTRO ANTI-FANTASMA (SÓ RODA NO CLIQUE DO X)
// =========================================================================
bool checarJogoInstaladoDeVerdade(const char* cusa) {
    char p[512];
    const char* bases[] = { "/user/app", "/mnt/ext0/user/app", "/mnt/ext1/user/app" };

    for (int i = 0; i < 3; i++) {
        sprintf(p, "%s/%s", bases[i], cusa);
        DIR* d = opendir(p);
        if (d) {
            struct dirent* dir;
            bool temConteudo = false;

            while ((dir = readdir(d)) != NULL) {
                if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                    temConteudo = true;
                    break;
                }
            }
            closedir(d);

            if (temConteudo) return true; // Achou a pasta e tem arquivos dentro! É real!
        }
    }
    return false; // É um fantasma!
}

// =========================================================================
// LEITOR INTELIGENTE
// =========================================================================
void lerNome_PronunciationXML(const char* cusa, char* nomeSaida) {
    strcpy(nomeSaida, "Desconhecido");
    char xmlPath[512];

    // 1º Tenta no appmeta padrão
    sprintf(xmlPath, "/user/appmeta/%s/pronunciation.xml", cusa);
    FILE* f = fopen(xmlPath, "rb");

    // 2º Se não tiver, tenta no cofre de sistema
    if (!f) {
        sprintf(xmlPath, "/system_data/priv/appmeta/%s/pronunciation.xml", cusa);
        f = fopen(xmlPath, "rb");
    }

    // 3º Se não tiver, tenta no diretório do jogo
    if (!f) {
        sprintf(xmlPath, "/user/app/%s/sce_sys/pronunciation.xml", cusa);
        f = fopen(xmlPath, "rb");
    }

    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buffer = (char*)malloc(fsize + 1);
        if (buffer) {
            fread(buffer, 1, fsize, f);
            buffer[fsize] = '\0';

            char tempNome[128] = "Desconhecido";
            char* pBusca = buffer;
            char* pStart = strstr(pBusca, "<title_name>");

            if (pStart) {
                pStart += 12;
                char* pEnd = strstr(pStart, "</title_name>");
                if (pEnd) {
                    int len = pEnd - pStart;
                    if (len > 63) len = 63;
                    strncpy(tempNome, pStart, len);
                    tempNome[len] = '\0';
                }
            }

            bool precisaBuscarText = (strcmp(tempNome, "Desconhecido") == 0);
            for (int i = 0; tempNome[i] != '\0'; i++) {
                if ((unsigned char)tempNome[i] >= 0xE0) { precisaBuscarText = true; break; }
            }

            if (precisaBuscarText) {
                pBusca = buffer;
                while ((pStart = strstr(pBusca, "<text")) != NULL) {
                    char* tagEnd = strchr(pStart, '>');
                    if (tagEnd) {
                        char* textStart = tagEnd + 1;
                        char* pEnd = strstr(textStart, "</text>");
                        if (pEnd) {
                            int len = pEnd - textStart;
                            if (len > 63) len = 63;
                            strncpy(tempNome, textStart, len);
                            tempNome[len] = '\0';

                            bool temQuadradinho = false;
                            for (int i = 0; tempNome[i] != '\0'; i++) {
                                if ((unsigned char)tempNome[i] >= 0xE0) { temQuadradinho = true; break; }
                            }
                            if (!temQuadradinho) break;
                        }
                    }
                    pBusca = pStart + 5;
                }
            }
            if (strcmp(tempNome, "Desconhecido") != 0) strcpy(nomeSaida, tempNome);
            free(buffer);
        }
        fclose(f);
    }

    // PLANO B: LÊ DO SFO
    if (strcmp(nomeSaida, "Desconhecido") == 0) {
        char sfoPath[512];
        sprintf(sfoPath, "/user/appmeta/%s/param.sfo", cusa);
        FILE* sf = fopen(sfoPath, "rb");

        if (!sf) {
            sprintf(sfoPath, "/system_data/priv/appmeta/%s/param.sfo", cusa);
            sf = fopen(sfoPath, "rb");
        }

        if (sf) {
            unsigned int magic; fread(&magic, 4, 1, sf);
            if (magic == 0x46535000) {
                fseek(sf, 0x08, SEEK_SET);
                uint32_t keyOffset, dataOffset, entries;
                fread(&keyOffset, 4, 1, sf); fread(&dataOffset, 4, 1, sf); fread(&entries, 4, 1, sf);
                for (uint32_t i = 0; i < entries; i++) {
                    fseek(sf, 0x14 + (i * 16), SEEK_SET);
                    uint16_t kOff; fread(&kOff, 2, 1, sf);
                    fseek(sf, 2, SEEK_CUR);
                    uint32_t dLen, dMax, dOff;
                    fread(&dLen, 4, 1, sf); fread(&dMax, 4, 1, sf); fread(&dOff, 4, 1, sf);
                    fseek(sf, keyOffset + kOff, SEEK_SET);
                    char key[64]; memset(key, 0, 64);
                    for (int c = 0; c < 63; c++) { fread(&key[c], 1, 1, sf); if (key[c] == '\0') break; }
                    if (strcmp(key, "TITLE") == 0) {
                        fseek(sf, dataOffset + dOff, SEEK_SET);
                        int realLen = dLen < 63 ? dLen : 63;
                        fread(nomeSaida, 1, realLen, sf);
                        nomeSaida[realLen] = '\0';
                        break;
                    }
                }
            }
            fclose(sf);
        }
    }
}

void acaoCross_Root() {
    if (visualizandoMidiaImagem) { fullscreenMidia = !fullscreenMidia; if (!fullscreenMidia) zoomMidia = 1.0f; return; }
    if (visualizandoMidiaTexto) return;

    if (menuAtual == ROOT) {
        if (sel == 0) {
            memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "Jogos de PS4 Nativo"); strcpy(nomes[1], "Listas XML (Retro)");
            totalItens = 2; sel = 0; off = 0; menuAtual = MENU_TIPO_JOGO;
        }
        else if (sel == 1) { preencherMenuMidia(); sel = 0; off = 0; }
        else if (sel == 2) { preencherMenuBaixar(); sel = 0; off = 0; }
        else if (sel == 3) { preencherMenuEditar(); sel = 0; off = 0; }
        else if (sel == 4) { preencherExplorerHome(); sel = 0; off = 0; }
        else if (sel == 5) { preencherMenuExtra(); sel = 0; off = 0; }
        else if (sel == 6) { preencherMenuInformacao(); sel = 0; off = 0; }
    }

    else if (menuAtual == MENU_TIPO_JOGO) {
        if (sel == 0) {
            memset(nomes, 0, sizeof(nomes));
            memset(linksAtuais, 0, sizeof(linksAtuais));
            int cont = 0;

            DIR* d = opendir("/user/appmeta");
            if (!d) d = opendir("/system_data/priv/appmeta");

            if (d) {
                struct dirent* dir;
                while ((dir = readdir(d)) != NULL && cont < 2900) {

                    if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 && strlen(dir->d_name) >= 8) {

                        char idLimpo[16]; memset(idLimpo, 0, sizeof(idLimpo));
                        strncpy(idLimpo, dir->d_name, 9);
                        idLimpo[9] = '\0';

                        char nomeReal[100];
                        lerNome_PronunciationXML(idLimpo, nomeReal);

                        sprintf(nomes[cont], "%s", nomeReal);
                        strcpy(linksAtuais[cont], idLimpo);
                        cont++;
                    }
                }
                closedir(d);
            }

            if (cont > 0) { totalItens = cont; }
            else { strcpy(nomes[0], "Nenhum jogo encontrado no appmeta!"); strcpy(linksAtuais[0], ""); totalItens = 1; }

            sel = 0; off = 0; menuAtual = MENU_JOGAR_PS4; timeToLoadCapa = 5;
        }
        else if (sel == 1) {
            // -----------------------------------------------------
            // AGORA MANDA APENAS O NOME, O MOTOR TRATA DE COPIAR!
            // -----------------------------------------------------
            carregarXML("system.xml");
        }
    }

    else if (menuAtual == MENU_JOGAR_PS4) {
        if (totalItens > 0 && strlen(linksAtuais[sel]) > 0) {
            if (checarJogoInstaladoDeVerdade(linksAtuais[sel])) {
                chamarJogo(linksAtuais[sel], NULL);
            }
            else {
                strcpy(msgStatus, "APP NAO ENCONTRADO! (FANTASMA / DESCONECTADO)");
                msgTimer = 180;
            }
        }
    }

    else if (menuAtual == JOGAR_XML) {
        // Se estamos dentro do system.xml e clicamos num nome
        if (strstr(caminhoXMLAtual, "system.xml") != NULL) {
            char caminhoNovoXML[512];
            sprintf(caminhoNovoXML, "%s.xml", nomes[sel]);
            carregarXML(caminhoNovoXML);
            sel = 0;
            off = 0;
        }
        // Atalho legado para SP, por segurança!
        else if (strcasecmp(nomes[sel], "sp") == 0) {
            carregarXML("sp.xml");
            sel = 0; off = 0;
        }
        else {
            sprintf(msgStatus, "PREPARANDO PARA JOGAR: %s", nomes[sel]);
            msgTimer = 120;
        }
    }

    else if (menuAtual == MENU_MIDIA) {
        if (strcmp(nomes[sel], "Pasta vazia") == 0) return;
        char novoCaminho[512]; sprintf(novoCaminho, "%s/%s", caminhoMidiaAtual, nomes[sel]);

        DIR* chk = opendir(novoCaminho);
        if (chk) {
            closedir(chk);
            if (strcasecmp(nomes[sel], "musicas") == 0 && strcmp(caminhoMidiaAtual, "/data/HyperNeiva/midia") == 0) {
                menuAtual = MENU_MUSICAS; if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
                strcpy(ultimoJogoCarregado, ""); strcpy(caminhoNavegacaoMusicas, "/data/HyperNeiva/Musicas");
                preencherMenuMusicas(); sel = 0; off = 0;
            }
            else { abrirPastaMidia(novoCaminho); }
        }
        else {
            int len = strlen(nomes[sel]);
            if (len > 4 && (strcasecmp(&nomes[sel][len - 4], ".mp3") == 0 || strcasecmp(&nomes[sel][len - 4], ".wav") == 0)) { tocarMusicaNova(novoCaminho); sprintf(msgStatus, "TOCANDO: %s", nomes[sel]); msgTimer = 180; }
            else if ((len > 4 && (strcasecmp(&nomes[sel][len - 4], ".png") == 0 || strcasecmp(&nomes[sel][len - 4], ".jpg") == 0)) || (len > 5 && strcasecmp(&nomes[sel][len - 5], ".jpeg") == 0)) {
                if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } imgMidia = stbi_load(novoCaminho, &wM, &hM, &cM, 4);
                if (imgMidia) { visualizandoMidiaImagem = true; zoomMidia = 1.0f; fullscreenMidia = false; }
                else { strcpy(msgStatus, "ERRO AO CARREGAR IMAGEM"); msgTimer = 120; }
            }
            else if ((len > 4 && (strcasecmp(&nomes[sel][len - 4], ".txt") == 0 || strcasecmp(&nomes[sel][len - 4], ".xml") == 0 || strcasecmp(&nomes[sel][len - 4], ".ini") == 0 || strcasecmp(&nomes[sel][len - 4], ".cpp") == 0 || strcasecmp(&nomes[sel][len - 4], ".doc") == 0 || strcasecmp(&nomes[sel][len - 4], ".bin") == 0 || strcasecmp(&nomes[sel][len - 4], ".log") == 0 || strcasecmp(&nomes[sel][len - 4], ".csv") == 0 || strcasecmp(&nomes[sel][len - 4], ".bat") == 0 || strcasecmp(&nomes[sel][len - 4], ".css") == 0 || strcasecmp(&nomes[sel][len - 4], ".php") == 0 || strcasecmp(&nomes[sel][len - 4], ".pdf") == 0)) || (len > 5 && (strcasecmp(&nomes[sel][len - 5], ".json") == 0 || strcasecmp(&nomes[sel][len - 5], ".docx") == 0 || strcasecmp(&nomes[sel][len - 5], ".html") == 0 || strcasecmp(&nomes[sel][len - 5], ".yaml") == 0)) || (len > 3 && (strcasecmp(&nomes[sel][len - 3], ".md") == 0 || strcasecmp(&nomes[sel][len - 3], ".js") == 0 || strcasecmp(&nomes[sel][len - 3], ".py") == 0 || strcasecmp(&nomes[sel][len - 3], ".sh") == 0)) || (len > 2 && (strcasecmp(&nomes[sel][len - 2], ".h") == 0 || strcasecmp(&nomes[sel][len - 2], ".c") == 0))) {
                FILE* f = fopen(novoCaminho, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
                    if (textoMidiaBuffer) free(textoMidiaBuffer); textoMidiaBuffer = (char*)malloc(fsize + 1);
                    if (textoMidiaBuffer) { fread(textoMidiaBuffer, 1, fsize, f); textoMidiaBuffer[fsize] = '\0'; fclose(f); abrirTextoNoNotepad(textoMidiaBuffer); menuAtual = MENU_NOTEPAD; free(textoMidiaBuffer); textoMidiaBuffer = NULL; }
                    else { fclose(f); strcpy(msgStatus, "ARQUIVO GIGANTE DEMAIS!"); msgTimer = 120; }
                }
                else { strcpy(msgStatus, "ERRO AO LER ARQUIVO"); msgTimer = 120; }
            }
            else { strcpy(msgStatus, "ARQUIVO NAO SUPORTADO"); msgTimer = 120; }
        }
    }
}

void acaoCircle_Root() {
    if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; fullscreenMidia = false; zoomMidia = 1.0f; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } return; }
    if (visualizandoMidiaTexto) { visualizandoMidiaTexto = false; if (textoMidiaBuffer) { free(textoMidiaBuffer); textoMidiaBuffer = NULL; } return; }

    if (menuAtual == MENU_TIPO_JOGO) { preencherRoot(); }
    else if (menuAtual == MENU_JOGAR_PS4) { memset(nomes, 0, sizeof(nomes)); strcpy(nomes[0], "Jogos de PS4 Nativo"); strcpy(nomes[1], "Listas XML (Retro)"); totalItens = 2; sel = 0; off = 0; menuAtual = MENU_TIPO_JOGO; }
    else if (menuAtual == JOGAR_XML) {
        if (strstr(caminhoXMLAtual, "system.xml") == NULL) {
            carregarXML("system.xml");
            sel = 0; off = 0;
        }
        else {
            memset(nomes, 0, sizeof(nomes));
            strcpy(nomes[0], "Jogos de PS4 Nativo");
            strcpy(nomes[1], "Listas XML (Retro)");
            totalItens = 2; sel = 0; off = 0;
            menuAtual = MENU_TIPO_JOGO;
        }
    }
    else if (menuAtual == MENU_MIDIA) { if (strcmp(caminhoMidiaAtual, "/data/HyperNeiva/midia") == 0) { preencherRoot(); } else { char* ultimaBarra = strrchr(caminhoMidiaAtual, '/'); if (ultimaBarra != NULL) { *ultimaBarra = '\0'; } abrirPastaMidia(caminhoMidiaAtual); } }

    else if (menuAtual == MENU_EXTRA || menuAtual == MENU_INFORMACAO) { preencherRoot(); }
}