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
#include <sys/stat.h>

// Jogos de CD (Saturn, PS1...) dentro de .zip/.7z não cabem na memória e o .cue precisa
// dos .bin ao lado: o compactado é extraído uma vez para uma pasta com o mesmo nome.
static bool hnCompactadoDeCd(const char* arquivo) {
    char minusculo[600];
    snprintf(minusculo, sizeof(minusculo), "%s", arquivo);
    for (int i = 0; minusculo[i]; i++) if (minusculo[i] >= 'A' && minusculo[i] <= 'Z') minusculo[i] += 32;
    if (strstr(minusculo, "saturn") || strstr(minusculo, "/ps1") || strstr(minusculo, "playstation") ||
        strstr(minusculo, "dreamcast") || strstr(minusculo, "/psp") || strstr(minusculo, "sega cd") || strstr(minusculo, "mega cd"))
        return true;
    struct stat info;
    return stat(arquivo, &info) == 0 && info.st_size > 64LL * 1024 * 1024;
}

// Imagem CloneCD só com .ccd: o Yabause caía lendo o .ccd. Gera um .cue equivalente
// (mesmo .img, faixas e início de cada uma) e abre por ele.
static bool hnCcdParaCue(const char* ccd, char* cue, size_t tamanho) {
    FILE* f = fopen(ccd, "r");
    if (!f) return false;
    int pontos[100], plba[100], modos[100], total = 0, ponto = -1, faixa = -1, controle = 4;
    int controles[100];
    for (int i = 0; i < 100; i++) modos[i] = 1;
    char linha[160];
    while (fgets(linha, sizeof(linha), f)) {
        int valor;
        if (strncmp(linha, "[Entry", 6) == 0) { ponto = -1; controle = 4; }
        else if (sscanf(linha, "Control=0x%x", &valor) == 1) controle = valor;
        else if (sscanf(linha, "Point=0x%x", &valor) == 1) ponto = valor;
        else if (sscanf(linha, "PLBA=%d", &valor) == 1 && ponto >= 1 && ponto <= 99 && total < 100) { pontos[total] = ponto; plba[total] = valor; controles[total] = controle; total++; }
        else if (sscanf(linha, "[TRACK %d]", &valor) == 1) faixa = valor;
        else if (sscanf(linha, "MODE=%d", &valor) == 1 && faixa >= 1 && faixa <= 99) modos[faixa] = valor;
    }
    fclose(f);
    if (total == 0) return false;
    const char* base = strrchr(ccd, '/');
    base = base ? base + 1 : ccd;
    snprintf(cue, tamanho, "%.*s.cue", (int)(strrchr(ccd, '.') - ccd), ccd);
    FILE* s = fopen(cue, "w");
    if (!s) return false;
    fprintf(s, "FILE \"%.*s.img\" BINARY\n", (int)(strrchr(base, '.') - base), base);
    for (int i = 0; i < total; i++) {
        int n = pontos[i], lba = plba[i];
        // Control 0x04 marca faixa de dados; sem ele é áudio. O MODE de [TRACK] (quando existe) define 1 ou 2.
        const char* tipo = !(controles[i] & 0x04) ? "AUDIO" : (modos[n] == 2 ? "MODE2/2352" : "MODE1/2352");
        fprintf(s, "  TRACK %02d %s\n    INDEX 01 %02d:%02d:%02d\n", n, tipo, lba / 4500, (lba / 75) % 60, lba % 75);
    }
    fclose(s);
    FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
    if (registro) { fprintf(registro, "CD cue gerado do ccd (%d faixas): %s\n", total, cue); fclose(registro); }
    return true;
}

// Procura o disco dentro da pasta extraída (até um nível de subpasta). Ordem: m3u, cue, chd, iso, ccd, mds.
static bool hnAcharDisco(const char* pasta, char* saida, size_t tamanho, int nivel) {
    static const char* ordem[] = { ".m3u", ".cue", ".chd", ".iso", ".ccd", ".mds" };
    for (int o = 0; o < 6; o++) {
        DIR* d = opendir(pasta);
        if (!d) return false;
        struct dirent* item;
        while ((item = readdir(d)) != NULL) {
            if (item->d_name[0] == '.') continue;
            const char* ponto = strrchr(item->d_name, '.');
            if (item->d_type == DT_REG && ponto && strcasecmp(ponto, ordem[o]) == 0) {
                snprintf(saida, tamanho, "%s/%s", pasta, item->d_name);
                closedir(d);
                if (strcasecmp(ordem[o], ".ccd") == 0) {
                    char cue[800];
                    if (hnCcdParaCue(saida, cue, sizeof(cue))) snprintf(saida, tamanho, "%s", cue);
                }
                return true;
            }
        }
        closedir(d);
    }
    if (nivel > 0) return false;
    DIR* d = opendir(pasta);
    if (!d) return false;
    struct dirent* item;
    bool achou = false;
    while (!achou && (item = readdir(d)) != NULL) {
        if (item->d_type != DT_DIR || item->d_name[0] == '.') continue;
        char sub[700];
        snprintf(sub, sizeof(sub), "%s/%s", pasta, item->d_name);
        achou = hnAcharDisco(sub, saida, tamanho, nivel + 1);
    }
    closedir(d);
    return achou;
}

// DECLARAÇÃO DOS NOVOS MENUS
extern void preencherMenuExtra();
extern void preencherMenuInformacao();
extern void preencherMenuEmulador();
extern void preencherMenuGBA(); // Novo V26
extern void abrirEscritorio3D();

bool visualizandoMidiaImagem = false;
unsigned char* imgMidia = NULL;
int wM = 0, hM = 0, cM = 0;
float zoomMidia = 1.0f;
bool fullscreenMidia = false;
int panMidiaX = 0, panMidiaY = 0;

bool visualizandoMidiaTexto = false;
char* textoMidiaBuffer = NULL;
char* linhasTexto[5000];
int totalLinhasTexto = 0;
int textoMidiaScroll = 0;

extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern char nomes[3000][256];
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
extern void iniciarEmulador(const char* romPath);

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
    extern bool showOpcoes; extern int selOpcao; extern void acaoArquivo(int);
    if (menuAtual == MENU_MIDIA && showOpcoes) { acaoArquivo(selOpcao); return; }
    if (visualizandoMidiaImagem) { fullscreenMidia = !fullscreenMidia; if (!fullscreenMidia) { zoomMidia = 1.0f; panMidiaX = 0; panMidiaY = 0; } return; }
    if (visualizandoMidiaTexto) return;

    if (menuAtual == MENU_ERRO_CRITICO) {
        msgTimer = 0;
        msgStatus[0] = '\0';
        // extern void listarCoresDisponiveis();
        // listarCoresDisponiveis();
        return;
    }

    if (menuAtual == ROOT) {
        if (sel == 0) { // JOGAR
            extern void preencherMenuJogar();
            preencherMenuJogar();
            sel = 0; off = 0;
        }
        else if (sel == 1) { preencherMenuMidia(); sel = 0; off = 0; }
        else if (sel == 2) { preencherMenuBaixar(); sel = 0; off = 0; }
        else if (sel == 3) { preencherMenuEditar(); sel = 0; off = 0; }
        else if (sel == 4) { preencherExplorerHome(); sel = 0; off = 0; }
        else if (sel == 5) { preencherMenuExtra(); sel = 0; off = 0; }
        else if (sel == 6) { preencherMenuInformacao(); sel = 0; off = 0; }
        else if (sel == 7) { extern void abrirConquistas(); abrirConquistas(); }
        else if (sel == 8) { abrirEscritorio3D(); }
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
            extern void bibliotecaMostrarSistemas();
            bibliotecaMostrarSistemas();
            if (false) carregarXML("system.xml");
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

    else if (menuAtual == MENU_SISTEMAS) {
        extern void bibliotecaCruzSistemas();
        bibliotecaCruzSistemas();
    }
    else if (menuAtual == MENU_EMULADOR) {
        extern bool bibliotecaCruz();
        if (bibliotecaCruz()) return;
        if (totalItens > 0 && strcmp(nomes[sel], "Nenhuma ROM encontrada") != 0) {
            extern char romParaCarregar[512];
            extern char caminhoROMsAtual[512];
            sprintf(romParaCarregar, "%s/%s", caminhoROMsAtual, nomes[sel]);
            
            // NOVO: Se for GBA, não precisa escolher núcleo (é estático!)
            const char* ext = strrchr(romParaCarregar, '.');
            if (ext && (strcasecmp(ext, ".gba") == 0 || strcasecmp(ext, ".gb") == 0 || strcasecmp(ext, ".gbc") == 0)) {
                extern void iniciarEmulador(const char* romPath);
                iniciarEmulador(romParaCarregar);
            } else if (ext && (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".7z") == 0) && hnCompactadoDeCd(romParaCarregar)) {
                // Jogo de CD: extrai uma vez para a pasta de mesmo nome e abre o disco dela.
                static char pastaJogo[600], disco[800];
                snprintf(pastaJogo, sizeof(pastaJogo), "%.*s", (int)(ext - romParaCarregar), romParaCarregar);
                FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
                if (hnAcharDisco(pastaJogo, disco, sizeof(disco), 0)) {
                    if (registro) { fprintf(registro, "CD extraido, abrindo %s\n", disco); fclose(registro); }
                    extern void iniciarEmulador(const char* romPath);
                    iniciarEmulador(disco);
                } else {
                    if (registro) { fprintf(registro, "CD extraindo %s para %s\n", romParaCarregar, pastaJogo); fclose(registro); }
                    mkdir(pastaJogo, 0777);
                    extern void hnEnfileirarExtracao(const char* arquivo, const char* destino);
                    hnEnfileirarExtracao(romParaCarregar, pastaJogo);
                    snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO O JOGO DE CD. QUANDO A BARRA TERMINAR, APERTE X DE NOVO");
                    msgTimer = 600;
                }
            } else if (ext && (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".7z") == 0)) {
                extern void iniciarEmulador(const char* romPath);
                extern bool hnCarregarRomGba(const char* arquivo, void** dados, size_t* tamanho, char* nome, int tamanhoNome);
                extern void bridge_rom_em_memoria(void* dados, size_t tamanho);
                void* dados = NULL;
                size_t tamanho = 0;
                char nome[256];
                FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
                if (registro) { fprintf(registro, "ZIP abrindo %s\n", romParaCarregar); fclose(registro); }
                bool achou = hnCarregarRomGba(romParaCarregar, &dados, &tamanho, nome, sizeof(nome));
                registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
                if (registro) { fprintf(registro, "ZIP %s %zu %s\n", achou ? "rom" : "sem rom", tamanho, achou ? nome : ""); fclose(registro); }
                if (achou) {
                    // O núcleo recebe os dados; o nome .gba só indica o tipo do jogo.
                    static char romVirtual[800];
                    snprintf(romVirtual, sizeof(romVirtual), "%s/%s", caminhoROMsAtual, nome);
                    bridge_rom_em_memoria(dados, tamanho);
                    iniciarEmulador(romVirtual);
                }
                else { snprintf(msgStatus, sizeof(msgStatus), "NENHUMA ROM COM EMULADOR DENTRO DESTE COMPACTADO"); msgTimer = 300; }
            } else {
                // Outros sistemas: abre quando há núcleo separado para a extensão.
                extern const char* bridge_nucleo_para_rom(const char* rom);
                extern void iniciarEmulador(const char* romPath);
                if (bridge_nucleo_para_rom(romParaCarregar)) iniciarEmulador(romParaCarregar);
                else { snprintf(msgStatus, sizeof(msgStatus), "ESTE ARQUIVO AINDA NAO TEM EMULADOR"); msgTimer = 240; }
            }
        }
    }

    else if (menuAtual == JOGAR_XML) {
        extern int modoLauncher;
        if (modoLauncher == 1) { // PASTA EMULADOR: um item por sistema
            extern void bibliotecaAbrirSistema(int sistema);
            bibliotecaAbrirSistema(sel);
            modoLauncher = 0; // Reseta para XML ao entrar
            sel = 0; off = 0;
        }
        else { // MODO XML NORMAL
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
    }

    else if (menuAtual == MENU_MIDIA) {
        if (strcmp(nomes[sel], "Radio & Podcast") == 0) {
            extern void preencherMenuRadioCategorias();
            preencherMenuRadioCategorias();
            return;
        }
        if (strcmp(nomes[sel], "Pasta vazia") == 0) return;
        char novoCaminho[512]; sprintf(novoCaminho, "%s/%s", caminhoMidiaAtual, nomes[sel]);

        DIR* chk = opendir(novoCaminho);
        if (chk) {
            closedir(chk);
            if (strcasecmp(nomes[sel], "musicas") == 0 && strcmp(caminhoMidiaAtual, "/data/HyperNeiva/midia") == 0) {
                menuAtual = MENU_MUSICAS; if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
                strcpy(ultimoJogoCarregado, ""); strcpy(caminhoNavegacaoMusicas, "/data/HyperNeiva/Musicas");
                { extern char caminhoBaseMusicas[512]; strcpy(caminhoBaseMusicas, caminhoNavegacaoMusicas); }
                preencherMenuMusicas(); sel = 0; off = 0;
            }
            else { abrirPastaMidia(novoCaminho); }
        }
        else {
            int len = strlen(nomes[sel]);
            extern bool ehArquivoVideo(const char* nome);
            if (ehArquivoVideo(nomes[sel])) {
                extern bool videoRodando; extern char ultimo_video_tocado[512];
                extern void iniciarVideoMP4(const char* caminho);
                if (!(videoRodando && strcmp(novoCaminho, ultimo_video_tocado) == 0)) iniciarVideoMP4(novoCaminho);
            }
            else if (len > 4 && (strcasecmp(&nomes[sel][len - 4], ".pdf") == 0 || strcasecmp(&nomes[sel][len - 4], ".cbz") == 0)) {
                extern void abrirLeitor(const char* caminho);
                abrirLeitor(novoCaminho);
            }
            else if (len > 4 && (strcasecmp(&nomes[sel][len - 4], ".mp3") == 0 || strcasecmp(&nomes[sel][len - 4], ".wav") == 0)) { tocarMusicaNova(novoCaminho); sprintf(msgStatus, "TOCANDO: %s", nomes[sel]); msgTimer = 180; }
            else if ((len > 4 && (strcasecmp(&nomes[sel][len - 4], ".png") == 0 || strcasecmp(&nomes[sel][len - 4], ".jpg") == 0 || strcasecmp(&nomes[sel][len - 4], ".bmp") == 0)) || (len > 5 && strcasecmp(&nomes[sel][len - 5], ".jpeg") == 0) || (len > 8 && strcasecmp(&nomes[sel][len - 8], ".xavatar") == 0)) {
                extern void abrirOpcoesImagem(const char*, bool);
                abrirOpcoesImagem(novoCaminho, false);
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
    if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; fullscreenMidia = false; zoomMidia = 1.0f; panMidiaX = 0; panMidiaY = 0; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } return; }
    if (visualizandoMidiaTexto) { visualizandoMidiaTexto = false; if (textoMidiaBuffer) { free(textoMidiaBuffer); textoMidiaBuffer = NULL; } return; }
    
    // A navegação agora é tratada centralmente no controle.cpp
}