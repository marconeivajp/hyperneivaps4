#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include "explorar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h> 
#include <orbis/libkernel.h>
#include <orbis/ImeDialog.h>
#include <orbis/CommonDialog.h>
#include "bloco_de_notas.h" 
#include <errno.h> 

#include "miniz.h" 
#include "stb_image.h" 
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifndef IME_STATUS_RUNNING
#define IME_STATUS_RUNNING 1
#endif

extern void atualizarBarra(float progresso);

extern bool visualizandoMidiaImagem;
char caminhoImagemAberta[512] = "";
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;

char pathExplorar[256] = "";
char baseRaiz[256] = "";
bool marcados[3000];

bool painelDuplo = false;
int painelAtivo = 1;
char pathExplorarEsq[256] = "";
char nomesEsq[3000][64];
bool marcadosEsq[3000];
int totalItensEsq = 0;
int selEsq = 0;
MenuLevel menuAtualEsq = MENU_EXPLORAR_HOME;

char clipboardPaths[100][256];
int clipboardCount = 0;
bool clipboardIsCut = false;
bool showOpcoes = false;
int selOpcao = 0;

const char* listaOpcoes[150] = { "" };
int mapOpcoes[150] = { -1 };
int totalOpcoes = 0;

char caminhoPerfilAlvo[150][64];
char menuStrDinamico[150][128];
int acaoJogoEscolhida = 0;

bool esperandoNomePasta = false;
bool esperandoRenomear = false;
wchar_t textoTeclado[256] = L"";
char oldPathParaRenomear[512] = "";
char oldExtParaRenomear[64] = "";
bool ehPastaParaRenomear = false;

// ====================================================================
// LEITOR DE PARAM.SFO (Extrai o Nome Real do Jogo)
// ====================================================================
void obterNomeJogoSFO(const char* cusa, char* nomeSaida) {
    strcpy(nomeSaida, "Desconhecido");
    char pathSfo[512];
    sprintf(pathSfo, "/user/appmeta/%s/param.sfo", cusa);
    FILE* f = fopen(pathSfo, "rb");
    if (!f) return;

    unsigned int magic; fread(&magic, 4, 1, f);
    if (magic != 0x46535000) { fclose(f); return; }

    fseek(f, 0x08, SEEK_SET);
    unsigned int keyOffset = 0, dataOffset = 0, entries = 0;
    fread(&keyOffset, 4, 1, f); fread(&dataOffset, 4, 1, f); fread(&entries, 4, 1, f);

    for (unsigned int i = 0; i < entries; i++) {
        fseek(f, 0x14 + (i * 16), SEEK_SET);
        unsigned short kOff; fread(&kOff, 2, 1, f);
        fseek(f, 2, SEEK_CUR);
        unsigned int dLen, dMax, dOff;
        fread(&dLen, 4, 1, f); fread(&dMax, 4, 1, f); fread(&dOff, 4, 1, f);

        fseek(f, keyOffset + kOff, SEEK_SET);
        char key[64]; memset(key, 0, 64); fread(key, 1, 63, f);

        if (strcmp(key, "TITLE") == 0) {
            fseek(f, dataOffset + dOff, SEEK_SET);
            int realLen = dLen < 63 ? dLen : 63;
            fread(nomeSaida, 1, realLen, f);
            nomeSaida[realLen] = '\0';
            for (int c = 0; c < realLen; c++) { if (nomeSaida[c] == '\n' || nomeSaida[c] == '\r') nomeSaida[c] = '\0'; }
            break;
        }
    } fclose(f);
}

// ====================================================================
// COMPRESSOR DXT1 AVANÇADO (Obrigatório para Avatares do PS4)
// ====================================================================
void redimensionarImagem(unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int srcX = (x * sw) / dw;
            int srcY = (y * sh) / dh;
            int srcIndex = (srcY * sw + srcX) * 4;
            int dstIndex = (y * dw + x) * 4;
            dst[dstIndex] = src[srcIndex];
            dst[dstIndex + 1] = src[srcIndex + 1];
            dst[dstIndex + 2] = src[srcIndex + 2];
            dst[dstIndex + 3] = src[srcIndex + 3]; // Mantém o Alpha no PNG
        }
    }
}

unsigned short colorTo565(int r, int g, int b) {
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int colorDistance(int r1, int g1, int b1, int r2, int g2, int b2) {
    return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

void salvarComoDDS(const char* filepath, unsigned char* img, int w, int h) {
    FILE* f = fopen(filepath, "wb");
    if (!f) return;

    unsigned int header[32];
    memset(header, 0, sizeof(header));
    header[0] = 0x20534444; // Magia "DDS "
    header[1] = 124;        // Tamanho do Header
    header[2] = 0x00081007; // Flags
    header[3] = h;          // Altura
    header[4] = w;          // Largura
    header[5] = (w > 4 ? w : 4) * (h > 4 ? h : 4) / 2; // LinearSize para DXT1
    header[19] = 32;        // Tamanho do PixelFormat
    header[20] = 0x00000004; // DDPF_FOURCC
    header[21] = 0x31545844; // Assinatura "DXT1" (Requisito estrito da PS4)
    header[27] = 0x1000;    // Caps

    fwrite(header, 1, 128, f);

    // Compressão de Blocos DXT1
    for (int by = 0; by < h / 4; by++) {
        for (int bx = 0; bx < w / 4; bx++) {
            int minR = 255, minG = 255, minB = 255;
            int maxR = 0, maxG = 0, maxB = 0;
            unsigned char block[16][3];

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = bx * 4 + x;
                    int py = by * 4 + y;
                    int idx = (py * w + px) * 4;
                    unsigned char r = img[idx];
                    unsigned char g = img[idx + 1];
                    unsigned char b = img[idx + 2];
                    block[y * 4 + x][0] = r;
                    block[y * 4 + x][1] = g;
                    block[y * 4 + x][2] = b;

                    if (r < minR) minR = r; if (g < minG) minG = g; if (b < minB) minB = b;
                    if (r > maxR) maxR = r; if (g > maxG) maxG = g; if (b > maxB) maxB = b;
                }
            }

            unsigned short c0 = colorTo565(maxR, maxG, maxB);
            unsigned short c1 = colorTo565(minR, minG, minB);

            // Força o modo OPACO no DXT1
            if (c0 < c1) {
                unsigned short temp = c0; c0 = c1; c1 = temp;
                int tr = maxR, tg = maxG, tb = maxB;
                maxR = minR; maxG = minG; maxB = minB;
                minR = tr; minG = tg; minB = tb;
            }
            else if (c0 == c1) {
                if (c0 > 0) c1 = c0 - 1; else c0 = 1;
            }

            unsigned int indices = 0;
            for (int i = 0; i < 16; i++) {
                int d0 = colorDistance(block[i][0], block[i][1], block[i][2], maxR, maxG, maxB);
                int d1 = colorDistance(block[i][0], block[i][1], block[i][2], minR, minG, minB);
                int d2 = colorDistance(block[i][0], block[i][1], block[i][2], (2 * maxR + minR) / 3, (2 * maxG + minG) / 3, (2 * maxB + minB) / 3);
                int d3 = colorDistance(block[i][0], block[i][1], block[i][2], (maxR + 2 * minR) / 3, (maxG + 2 * minG) / 3, (maxB + 2 * minB) / 3);

                unsigned int idx = 0; int minD = d0;
                if (d1 < minD) { minD = d1; idx = 1; }
                if (d2 < minD) { minD = d2; idx = 2; }
                if (d3 < minD) { minD = d3; idx = 3; }
                indices |= (idx << (i * 2));
            }

            fwrite(&c0, 2, 1, f);
            fwrite(&c1, 2, 1, f);
            fwrite(&indices, 4, 1, f);
        }
    }
    fclose(f);
}
// ====================================================================

void preencherOpcoesContexto(const char* nomeArquivo) {
    for (int i = 0; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
    totalOpcoes = 0;

    if (visualizandoMidiaImagem) {
        listaOpcoes[0] = "personalizar jogos"; mapOpcoes[0] = 30;
        listaOpcoes[1] = "usar no perfil ps4"; mapOpcoes[1] = 12;
        listaOpcoes[2] = "plano de fundo do ps4"; mapOpcoes[2] = 11;
        listaOpcoes[3] = "plano de fundo hyper neiva"; mapOpcoes[3] = 10;
        totalOpcoes = 4; return;
    }

    listaOpcoes[0] = "nova pasta"; mapOpcoes[0] = 0; listaOpcoes[1] = "novo arquivo"; mapOpcoes[1] = 1;
    listaOpcoes[2] = "copiar"; mapOpcoes[2] = 2; listaOpcoes[3] = "recortar"; mapOpcoes[3] = 3;
    listaOpcoes[4] = "colar"; mapOpcoes[4] = 4; listaOpcoes[5] = "renomear"; mapOpcoes[5] = 5;
    listaOpcoes[6] = "deletar"; mapOpcoes[6] = 6; listaOpcoes[7] = "selecionar"; mapOpcoes[7] = 8;
    listaOpcoes[8] = "selecionar tudo"; mapOpcoes[8] = 9;

    bool isZip = false;
    bool isPdfOuManga = false;

    if (nomeArquivo) {
        char temp[256]; strcpy(temp, nomeArquivo);
        for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
        if (strstr(temp, ".zip") != NULL || strstr(temp, ".xavatar") != NULL) isZip = true;
        if (strstr(temp, ".pdf") != NULL || strstr(temp, ".cbz") != NULL) isPdfOuManga = true;
    }

    if (isZip) { listaOpcoes[9] = "extrair zip / avatar"; mapOpcoes[9] = 7; totalOpcoes = 10; }
    else { totalOpcoes = 9; }

    if (isPdfOuManga) {
        listaOpcoes[totalOpcoes] = "ler documento";
        mapOpcoes[totalOpcoes] = 40;
        totalOpcoes++;
    }
}

void copiarArquivoReal(const char* origem, const char* destino) {
    FILE* src = fopen(origem, "rb"); if (!src) return;
    FILE* dst = fopen(destino, "wb"); if (!dst) { fclose(src); return; }
    char buffer[65536]; size_t n; while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    fclose(src); fclose(dst);
}

void deletarPastaRecursivamente(const char* path) {
    DIR* d = opendir(path);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                char fullPath[1024]; snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);
                struct stat st;
                if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) deletarPastaRecursivamente(fullPath);
                else unlink(fullPath);
            }
        } closedir(d);
    } rmdir(path);
}

void criarCaminho(const char* filepath) {
    char tmp[1024]; strncpy(tmp, filepath, sizeof(tmp));
    for (char* p = strchr(tmp + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0'; sceKernelMkdir(tmp, 0777); *p = '/';
    }
}

void extrairZip(const char* zipPath, const char* outPath) {
    mz_zip_archive zip_archive; memset(&zip_archive, 0, sizeof(zip_archive));
    sprintf(msgStatus, "LENDO O ARQUIVO ZIP..."); atualizarBarra(0.01f);
    if (!mz_zip_reader_init_file(&zip_archive, zipPath, 0)) { sprintf(msgStatus, "ERRO: ARQUIVO ZIP INVALIDO OU CORROMPIDO!"); msgTimer = 180; return; }
    int totalArquivos = (int)mz_zip_reader_get_num_files(&zip_archive);
    for (int i = 0; i < totalArquivos; i++) {
        mz_zip_archive_file_stat file_stat; if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
        char out_file[1024]; snprintf(out_file, sizeof(out_file), "%s/%s", outPath, file_stat.m_filename);
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) { criarCaminho(out_file); sceKernelMkdir(out_file, 0777); }
        else { criarCaminho(out_file); mz_zip_reader_extract_to_file(&zip_archive, i, out_file, 0); }
        float prog = (float)(i + 1) / (float)totalArquivos; sprintf(msgStatus, "EXTRAINDO: %d%%", (int)(prog * 100)); atualizarBarra(prog);
    }
    mz_zip_reader_end(&zip_archive); sprintf(msgStatus, "EXTRAIDO COM SUCESSO!"); msgTimer = 180;
}

void listarDiretorio(const char* path) {
    DIR* d = opendir(path);
    if (!d) {
        if (errno == EACCES) sprintf(msgStatus, "ERRO 13: SEM PERMISSAO DE ROOT");
        else if (errno == ENOENT) sprintf(msgStatus, "ERRO 2: INVISIVEL (SANDBOX) - %s", path);
        else sprintf(msgStatus, "ERRO %d AO ABRIR USB", errno); msgTimer = 240; return;
    }
    memset(marcados, 0, sizeof(marcados)); memset(nomes, 0, sizeof(nomes));
    ItemLista temp[3000]; int count = 0; struct dirent* dir;

    while ((dir = readdir(d)) != NULL && count < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char cleanName[128]; strcpy(cleanName, dir->d_name); int len = strlen(cleanName);
        while (len > 0 && (cleanName[len - 1] == ' ' || cleanName[len - 1] == '\r' || cleanName[len - 1] == '\n')) { cleanName[len - 1] = '\0'; len--; }
        strncpy(temp[count].nome, cleanName, 63); char fullPath[1024]; snprintf(fullPath, sizeof(fullPath), "%s/%s", path, cleanName); struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) temp[count].ehPasta = true;
        else temp[count].ehPasta = false; count++;
    } closedir(d);

    for (int i = 0; i < count - 1; i++) for (int j = 0; j < count - i - 1; j++) {
        bool trocar = false;
        if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
        else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
        if (trocar) { ItemLista aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
    }
    for (int i = 0; i < count; i++) { if (temp[i].ehPasta) sprintf(nomes[i], "[%s]", temp[i].nome); else strcpy(nomes[i], temp[i].nome); }
    totalItens = count; strcpy(pathExplorar, path); menuAtual = MENU_EXPLORAR; sel = 0;
}

void listarDiretorioEsq(const char* path) {
    DIR* d = opendir(path);
    if (!d) {
        if (errno == EACCES) sprintf(msgStatus, "ERRO 13: SEM PERMISSAO DE ROOT");
        else if (errno == ENOENT) sprintf(msgStatus, "ERRO 2: INVISIVEL (SANDBOX) - %s", path);
        else sprintf(msgStatus, "ERRO %d AO ABRIR USB", errno); msgTimer = 240; return;
    }
    memset(marcadosEsq, 0, sizeof(marcadosEsq)); memset(nomesEsq, 0, sizeof(nomesEsq));
    ItemLista temp[3000]; int count = 0; struct dirent* dir;

    while ((dir = readdir(d)) != NULL && count < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char cleanName[128]; strcpy(cleanName, dir->d_name); int len = strlen(cleanName);
        while (len > 0 && (cleanName[len - 1] == ' ' || cleanName[len - 1] == '\r' || cleanName[len - 1] == '\n')) { cleanName[len - 1] = '\0'; len--; }
        strncpy(temp[count].nome, cleanName, 63); char fullPath[1024]; snprintf(fullPath, sizeof(fullPath), "%s/%s", path, cleanName); struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) temp[count].ehPasta = true;
        else temp[count].ehPasta = false; count++;
    } closedir(d);

    for (int i = 0; i < count - 1; i++) for (int j = 0; j < count - i - 1; j++) {
        bool trocar = false;
        if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
        else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
        if (trocar) { ItemLista aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
    }
    for (int i = 0; i < count; i++) { if (temp[i].ehPasta) sprintf(nomesEsq[i], "[%s]", temp[i].nome); else strcpy(nomesEsq[i], temp[i].nome); }
    totalItensEsq = count; strcpy(pathExplorarEsq, path); menuAtualEsq = MENU_EXPLORAR; selEsq = 0;
}

void acaoArquivo(int idxOpcao) {
    if (idxOpcao < 0 || idxOpcao >= 150) return;
    int op = mapOpcoes[idxOpcao]; if (op == -1) return;

    bool ehEsq = (painelDuplo && painelAtivo == 0);
    int tItens = ehEsq ? totalItensEsq : totalItens; bool* mItems = ehEsq ? marcadosEsq : marcados;
    char (*nItems)[64] = ehEsq ? nomesEsq : nomes; char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;
    int sAtual = ehEsq ? selEsq : sel;

    bool temMarcado = false;
    for (int i = 0; i < tItens; i++) if (mItems[i]) { temMarcado = true; break; }

    switch (op) {
    case 0: {
        OrbisImeDialogSetting param; memset(&param, 0, sizeof(param)); memset(textoTeclado, 0, sizeof(textoTeclado));
        param.maxTextLength = 255; param.inputTextBuffer = textoTeclado; param.title = L"Nome da Nova Pasta"; param.type = (OrbisImeType)0;
        if (sceImeDialogInit(&param, NULL) >= 0) { esperandoNomePasta = true; showOpcoes = false; } break;
    }
    case 1: {
        inicializarNotepad(); strcpy(pastaDestinoFinal, pExplorar);
        if (ehEsq) menuAtualEsq = MENU_NOTEPAD; else menuAtual = MENU_NOTEPAD; showOpcoes = false; break;
    }
    case 2: case 3: {
        clipboardCount = 0; clipboardIsCut = (op == 3);
        for (int i = 0; i < tItens; i++) {
            if (temMarcado ? mItems[i] : (i == sAtual)) {
                char* l = (nItems[i][0] == '[') ? &nItems[i][1] : nItems[i];
                sprintf(clipboardPaths[clipboardCount], "%s/%s", pExplorar, l);
                if (nItems[i][0] == '[') clipboardPaths[clipboardCount][strlen(clipboardPaths[clipboardCount]) - 1] = '\0';
                clipboardCount++;
            }
        } sprintf(msgStatus, op == 2 ? "ARQUIVO(S) COPIADO(S)" : "ARQUIVO(S) RECORTADO(S)"); msgTimer = 90; break;
    }
    case 4: {
        for (int i = 0; i < clipboardCount; i++) {
            char* baseName = strrchr(clipboardPaths[i], '/'); if (baseName) baseName++; else baseName = clipboardPaths[i];
            char dest[512]; sprintf(dest, "%s/%s", pExplorar, baseName); copiarArquivoReal(clipboardPaths[i], dest);
            if (clipboardIsCut) {
                struct stat path_stat; stat(clipboardPaths[i], &path_stat);
                if (S_ISDIR(path_stat.st_mode)) deletarPastaRecursivamente(clipboardPaths[i]); else unlink(clipboardPaths[i]);
            }
        }
        if (ehEsq) listarDiretorioEsq(pathExplorarEsq); else listarDiretorio(pathExplorar);
        if (clipboardIsCut) { clipboardCount = 0; clipboardIsCut = false; }
        sprintf(msgStatus, "ARQUIVO(S) COLADO(S)"); msgTimer = 90; break;
    }
    case 5: {
        int alvo = sAtual; for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }
        char* nomeReal = nItems[alvo]; ehPastaParaRenomear = (nomeReal[0] == '[') ? true : false;
        char nomeLimpo[256];
        if (ehPastaParaRenomear) { strncpy(nomeLimpo, &nomeReal[1], strlen(nomeReal) - 2); nomeLimpo[strlen(nomeReal) - 2] = '\0'; }
        else { strcpy(nomeLimpo, nomeReal); }
        sprintf(oldPathParaRenomear, "%s/%s", pExplorar, nomeLimpo); memset(oldExtParaRenomear, 0, sizeof(oldExtParaRenomear));
        if (!ehPastaParaRenomear) { char* dot = strrchr(nomeLimpo, '.'); if (dot) strcpy(oldExtParaRenomear, dot); }

        OrbisImeDialogSetting param; memset(&param, 0, sizeof(param)); memset(textoTeclado, 0, sizeof(textoTeclado));
        uint16_t* bufWrite = (uint16_t*)textoTeclado;
        for (int i = 0; nomeLimpo[i] != '\0' && i < 255; i++) bufWrite[i] = (uint16_t)nomeLimpo[i];
        param.maxTextLength = 255; param.inputTextBuffer = textoTeclado; param.title = L"Renomear Arquivo/Pasta"; param.type = (OrbisImeType)0;
        if (sceImeDialogInit(&param, NULL) >= 0) { esperandoRenomear = true; showOpcoes = false; } break;
    }
    case 6: {
        for (int i = 0; i < tItens; i++) {
            if (temMarcado ? mItems[i] : (i == sAtual)) {
                char f[512]; char* l = (nItems[i][0] == '[') ? &nItems[i][1] : nItems[i];
                sprintf(f, "%s/%s", pExplorar, l);
                if (nItems[i][0] == '[') { f[strlen(f) - 1] = '\0'; deletarPastaRecursivamente(f); }
                else { unlink(f); }
            }
        } if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar); break;
    }
    case 7: {
        int alvo = sAtual; for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }
        char* nomeReal = nItems[alvo]; if (nomeReal[0] == '[') { sprintf(msgStatus, "SELECIONE UM ARQUIVO .ZIP"); msgTimer = 120; break; }
        char pathFinal[512]; sprintf(pathFinal, "%s/%s", pExplorar, nomeReal);
        char temp[512]; strcpy(temp, pathFinal); for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);

        if (strstr(temp, ".zip") != NULL || strstr(temp, ".xavatar") != NULL) extrairZip(pathFinal, pExplorar);
        else { sprintf(msgStatus, "SOMENTE ARQUIVOS .ZIP OU .XAVATAR SAO SUPORTADOS"); msgTimer = 120; }
        if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar); break;
    }
    case 8: { mItems[sAtual] = !mItems[sAtual]; break; }
    case 9: { bool ligar = false; for (int i = 0; i < tItens; i++) if (!mItems[i]) ligar = true; for (int i = 0; i < tItens; i++) mItems[i] = ligar; break; }

    case 10: {
        remove("/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.png"); remove("/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.jpg");
        char caminhoCopia[512]; char* extCheck = strrchr(caminhoImagemAberta, '.');
        if (extCheck && strcasecmp(extCheck, ".xavatar") == 0) strcpy(caminhoCopia, "/data/HyperNeiva/configuracao/temporario/temp_avatar.png");
        else strcpy(caminhoCopia, caminhoImagemAberta);

        FILE* src = fopen(caminhoCopia, "rb");
        if (src) {
            char dest[512];
            if (extCheck && (strcasecmp(extCheck, ".jpg") == 0 || strcasecmp(extCheck, ".jpeg") == 0)) strcpy(dest, "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.jpg");
            else strcpy(dest, "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.png");
            FILE* dst = fopen(dest, "wb");
            if (dst) {
                char buf[8192]; size_t bytes;
                while ((bytes = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, bytes, dst);
                fclose(dst); sprintf(msgStatus, "PLANO DE FUNDO APLICADO COM SUCESSO!");
            }
            else { sprintf(msgStatus, "ERRO AO SALVAR PLANO DE FUNDO"); }
            fclose(src); msgTimer = 180;
        } visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } break;
    }
    case 11: {
        DIR* d = opendir("/system_data/priv/home");
        if (d) {
            int pCount = 0; struct dirent* dir;
            while ((dir = readdir(d)) != NULL && pCount < 150) {
                if (dir->d_name[0] == '1' || dir->d_name[0] == '0') {
                    sprintf(menuStrDinamico[pCount], "Aplicar a: Conta ID %s", dir->d_name);
                    strcpy(caminhoPerfilAlvo[pCount], dir->d_name);
                    listaOpcoes[pCount] = menuStrDinamico[pCount];
                    mapOpcoes[pCount] = 200 + pCount;
                    pCount++;
                }
            } closedir(d);

            if (pCount > 0) {
                for (int k = pCount; k < 150; k++) { listaOpcoes[k] = ""; mapOpcoes[k] = -1; }
                totalOpcoes = pCount; showOpcoes = true; selOpcao = 0; return;
            }
            else { sprintf(msgStatus, "NENHUMA CONTA ENCONTRADA!"); msgTimer = 180; }
        }
        else { sprintf(msgStatus, "ERRO AO LER PASTAS DAS CONTAS!"); msgTimer = 180; }

        visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        break;
    }

    case 30: {
        listaOpcoes[0] = "icone do jogo (icon0)"; mapOpcoes[0] = 31;
        listaOpcoes[1] = "fundo do jogo (pic1)"; mapOpcoes[1] = 32;
        listaOpcoes[2] = "logo do jogo (pic0)"; mapOpcoes[2] = 33;
        listaOpcoes[3] = "tela loading (splash)"; mapOpcoes[3] = 34;
        listaOpcoes[4] = "icone do save (icon0)"; mapOpcoes[4] = 35;
        for (int i = 5; i < 150; i++) { listaOpcoes[i] = ""; mapOpcoes[i] = -1; }
        totalOpcoes = 5; showOpcoes = true; selOpcao = 0; return;
    }

    case 31: case 32: case 33: case 34: case 35: {
        acaoJogoEscolhida = op;
        int pCount = 0;
        DIR* d = opendir("/user/appmeta");
        if (d) {
            struct dirent* dir;
            while ((dir = readdir(d)) != NULL && pCount < 150) {
                if (dir->d_name[0] == 'C' && dir->d_name[1] == 'U' && dir->d_name[2] == 'S' && dir->d_name[3] == 'A') {
                    char gameName[100];
                    obterNomeJogoSFO(dir->d_name, gameName);
                    sprintf(menuStrDinamico[pCount], "%s (%s)", gameName, dir->d_name);
                    strcpy(caminhoPerfilAlvo[pCount], dir->d_name);
                    listaOpcoes[pCount] = menuStrDinamico[pCount];
                    mapOpcoes[pCount] = 400 + pCount;
                    pCount++;
                }
            } closedir(d);
        }
        if (pCount > 0) {
            for (int k = pCount; k < 150; k++) { listaOpcoes[k] = ""; mapOpcoes[k] = -1; }
            totalOpcoes = pCount; showOpcoes = true; selOpcao = 0; return;
        }
        else { sprintf(msgStatus, "NENHUM JOGO ENCONTRADO!"); msgTimer = 180; }
        visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } break;
    }

    case 12: {
        DIR* d = opendir("/system_data/priv/cache/profile");
        if (d) {
            int pCount = 0; struct dirent* dir;
            while ((dir = readdir(d)) != NULL && pCount < 150) {
                if (dir->d_name[0] == '0' && dir->d_name[1] == 'x') {
                    char onlineJsonPath[512]; sprintf(onlineJsonPath, "/system_data/priv/cache/profile/%s/online.json", dir->d_name);
                    char profileName[64] = "Desconhecido"; FILE* f = fopen(onlineJsonPath, "r");
                    if (f) {
                        char buf[1024]; size_t bytes = fread(buf, 1, sizeof(buf) - 1, f); buf[bytes] = '\0'; fclose(f);
                        char* ptr = strstr(buf, "\"firstName\":\"");
                        if (ptr) {
                            ptr += 13; char* end = strchr(ptr, '\"');
                            if (end && (end - ptr) < 63) { strncpy(profileName, ptr, end - ptr); profileName[end - ptr] = '\0'; }
                        }
                    }
                    sprintf(menuStrDinamico[pCount], "Aplicar a: %s (%s)", profileName, dir->d_name);
                    strcpy(caminhoPerfilAlvo[pCount], dir->d_name);
                    listaOpcoes[pCount] = menuStrDinamico[pCount]; mapOpcoes[pCount] = 20 + pCount; pCount++;
                }
            } closedir(d);

            if (pCount > 0) {
                for (int k = pCount; k < 150; k++) { listaOpcoes[k] = ""; mapOpcoes[k] = -1; }
                totalOpcoes = pCount; showOpcoes = true; selOpcao = 0; return;
            }
            else { sprintf(msgStatus, "NENHUM PERFIL ENCONTRADO!"); msgTimer = 180; }
        }
        else { sprintf(msgStatus, "ERRO AO LER PASTA DE PERFIS!"); msgTimer = 180; }
        visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } break;
    }

    case 13: {
        int alvo = sAtual; for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }
        char* nomeReal = nItems[alvo]; if (nomeReal[0] == '[') break;
        char caminhoArquivo[512]; sprintf(caminhoArquivo, "%s/%s", pExplorar, nomeReal);

        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        mz_zip_archive zip_archive; memset(&zip_archive, 0, sizeof(zip_archive));
        if (mz_zip_reader_init_file(&zip_archive, caminhoArquivo, 0)) {
            const char* tempPath = "/data/HyperNeiva/configuracao/temporario/temp_avatar.png"; bool extraiu = false;
            mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
            for (mz_uint i = 0; i < num_files; i++) {
                mz_zip_archive_file_stat file_stat;
                if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
                if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;
                char tempExtZip[256]; strcpy(tempExtZip, file_stat.m_filename);
                for (int k = 0; tempExtZip[k]; k++) tempExtZip[k] = tolower(tempExtZip[k]);
                if (strstr(tempExtZip, ".png") || strstr(tempExtZip, ".jpg") || strstr(tempExtZip, ".jpeg")) {
                    if (mz_zip_reader_extract_to_file(&zip_archive, i, tempPath, 0)) { extraiu = true; break; }
                }
            }
            if (extraiu) {
                int canais = 0; imgMidia = stbi_load(tempPath, &wM, &hM, &canais, 4);
                if (imgMidia) { visualizandoMidiaImagem = true; zoomMidia = 1.0f; fullscreenMidia = false; strcpy(caminhoImagemAberta, tempPath); }
                else { sprintf(msgStatus, "ERRO AO PROCESSAR A IMAGEM"); msgTimer = 120; }
            }
            else { sprintf(msgStatus, "AVATAR INVALIDO"); msgTimer = 120; }
            mz_zip_reader_end(&zip_archive);
        }
        else { sprintf(msgStatus, "FALHA AO ABRIR O ARQUIVO .xavatar"); msgTimer = 120; } break;
    }

    case 14: {
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        int canais = 0; imgMidia = stbi_load(caminhoImagemAberta, &wM, &hM, &canais, 4);
        if (imgMidia) { visualizandoMidiaImagem = true; zoomMidia = 1.0f; fullscreenMidia = false; }
        else { sprintf(msgStatus, "ERRO AO CARREGAR IMAGEM"); msgTimer = 90; } break;
    }

    case 40: {
        int alvo = sAtual; for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }
        char* nomeReal = nItems[alvo]; if (nomeReal[0] == '[') break; // Ignora se for pasta
        char caminhoArquivo[512]; sprintf(caminhoArquivo, "%s/%s", pExplorar, nomeReal);

        extern void abrirLeitor(const char* caminho);
        abrirLeitor(caminhoArquivo);
        showOpcoes = false;
        break;
    }

    default: {
        if (op >= 20 && op < 170) {
            int pIdx = op - 20; char profileDir[512]; sprintf(profileDir, "/system_data/priv/cache/profile/%s", caminhoPerfilAlvo[pIdx]);

            DIR* d = opendir(profileDir);
            if (d) {
                struct dirent* dir;
                while ((dir = readdir(d)) != NULL) {
                    if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                        char delPath[1024]; sprintf(delPath, "%s/%s", profileDir, dir->d_name); unlink(delPath);
                    }
                } closedir(d);
            }

            char* extCheck = strrchr(caminhoImagemAberta, '.');
            if (extCheck && strcasecmp(extCheck, ".xavatar") == 0) {
                mz_zip_archive zip_archive; memset(&zip_archive, 0, sizeof(zip_archive));
                if (mz_zip_reader_init_file(&zip_archive, caminhoImagemAberta, 0)) {
                    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
                    for (mz_uint i = 0; i < num_files; i++) {
                        mz_zip_archive_file_stat file_stat;
                        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
                        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;
                        char extPath[1024]; sprintf(extPath, "%s/%s", profileDir, file_stat.m_filename);
                        mz_zip_reader_extract_to_file(&zip_archive, i, extPath, 0);
                    }
                    mz_zip_reader_end(&zip_archive);
                }
            }
            else {
                int tw = 0, th = 0, tc = 0;
                unsigned char* tempImg = stbi_load(caminhoImagemAberta, &tw, &th, &tc, 4);
                if (tempImg) {
                    char destPng[1024]; char ddsPath[1024];
                    sprintf(destPng, "%s/avatar.png", profileDir);

                    // SALVA O PNG GIGANTE
                    unsigned char* img440 = (unsigned char*)malloc(440 * 440 * 4);
                    if (img440) {
                        redimensionarImagem(tempImg, tw, th, img440, 440, 440);
                        stbi_write_png(destPng, 440, 440, 4, img440, 440 * 4);

                        // AGORA APLICA DXT1 REAL NOS AVATARES PARA O PS4 LER DIREITO!
                        sprintf(ddsPath, "%s/avatar440.dds", profileDir); salvarComoDDS(ddsPath, img440, 440, 440);
                        free(img440);

                        unsigned char* img64 = (unsigned char*)malloc(64 * 64 * 4);
                        redimensionarImagem(tempImg, tw, th, img64, 64, 64);
                        sprintf(ddsPath, "%s/avatar64.dds", profileDir); salvarComoDDS(ddsPath, img64, 64, 64);
                        free(img64);

                        unsigned char* img128 = (unsigned char*)malloc(128 * 128 * 4);
                        redimensionarImagem(tempImg, tw, th, img128, 128, 128);
                        sprintf(ddsPath, "%s/avatar128.dds", profileDir); salvarComoDDS(ddsPath, img128, 128, 128);
                        free(img128);

                        unsigned char* img260 = (unsigned char*)malloc(260 * 260 * 4);
                        redimensionarImagem(tempImg, tw, th, img260, 260, 260);
                        sprintf(ddsPath, "%s/avatar.dds", profileDir); salvarComoDDS(ddsPath, img260, 260, 260);
                        sprintf(ddsPath, "%s/avatar260.dds", profileDir); salvarComoDDS(ddsPath, img260, 260, 260);
                        free(img260);
                    }
                    stbi_image_free(tempImg);
                }

                char vJson[1024]; sprintf(vJson, "%s/version.json", profileDir);
                FILE* fV = fopen(vJson, "w");
                if (fV) { fprintf(fV, "{\"cacheVersion\":\"5\"}"); fclose(fV); }

                char oJson[1024]; sprintf(oJson, "%s/online.json", profileDir);
                FILE* fO = fopen(oJson, "w");
                if (fO) {
                    fprintf(fO, "{\"avatarUrl\":\"http:\\/\\/static-resource.np.community.playstation.net\\/avatar_xl\\/WWS_E\\/E2096_xl.png\",\"firstName\":\"vamosjogarr\",\"lastName\":\"agora\",\"trophySummary\":\"{\\\"level\\\":1,\\\"progress\\\":25,\\\"earnedTrophies\\\":{\\\"platinum\\\":0,\\\"gold\\\":0,\\\"silver\\\":0,\\\"bronze\\\":1}}\",\"isOfficiallyVerified\":\"false\"}");
                    fclose(fO);
                }
            }
            sprintf(msgStatus, "PERFIL APLICADO! REINICIE O PS4."); msgTimer = 240;
            visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } break;
        }

        else if (op >= 200 && op < 350) {
            int pIdx = op - 200;
            char contaHomeDir[512];
            sprintf(contaHomeDir, "/system_data/priv/home/%s", caminhoPerfilAlvo[pIdx]);

            DIR* dh = opendir(contaHomeDir);
            if (dh) {
                closedir(dh);
                char themeDir[512];
                sprintf(themeDir, "%s/theme", contaHomeDir);
                sceKernelMkdir(themeDir, 0777);

                int tw = 0, th = 0, tc = 0;
                unsigned char* tempImg = stbi_load(caminhoImagemAberta, &tw, &th, &tc, 4);
                if (tempImg) {
                    char finalWallPath[1024];
                    sprintf(finalWallPath, "%s/wallpaper.png", themeDir);

                    unsigned char* img1080p = (unsigned char*)malloc(1920 * 1080 * 4);
                    if (img1080p) {
                        redimensionarImagem(tempImg, tw, th, img1080p, 1920, 1080);
                        stbi_write_png(finalWallPath, 1920, 1080, 4, img1080p, 1920 * 4);
                        sprintf(msgStatus, "FUNDO DO PS4 APLICADO! (CONTA %s)", caminhoPerfilAlvo[pIdx]);
                        free(img1080p);
                    }
                    else {
                        sprintf(msgStatus, "ERRO DE MEMORIA AO REDIMENSIONAR!");
                    }
                    stbi_image_free(tempImg);
                }
                else {
                    sprintf(msgStatus, "ERRO AO LER IMAGEM ORIGEM!");
                }
            }
            else {
                sprintf(msgStatus, "ERRO: CONTA INACESSIVEL!");
            }
            msgTimer = 240;
            visualizandoMidiaImagem = false;
            if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
            break;
        }

        else if (op >= 400 && op < 550) {
            int pIdx = op - 400;
            char cusaAlvo[64]; strcpy(cusaAlvo, caminhoPerfilAlvo[pIdx]);
            char destPath[1024];

            if (acaoJogoEscolhida == 31) sprintf(destPath, "/user/appmeta/%s/icon0.png", cusaAlvo);
            else if (acaoJogoEscolhida == 32) sprintf(destPath, "/user/appmeta/%s/pic1.png", cusaAlvo);
            else if (acaoJogoEscolhida == 33) sprintf(destPath, "/user/appmeta/%s/pic0.png", cusaAlvo);
            else if (acaoJogoEscolhida == 34) sprintf(destPath, "/user/appmeta/%s/splash.png", cusaAlvo);
            else if (acaoJogoEscolhida == 35) {
                DIR* dh = opendir("/user/home");
                bool foundSave = false;
                if (dh) {
                    struct dirent* dh_ent;
                    while ((dh_ent = readdir(dh)) != NULL) {
                        if (dh_ent->d_name[0] != '.') {
                            char checkPath[1024];
                            sprintf(checkPath, "/user/home/%s/savedata/%s/sce_sys", dh_ent->d_name, cusaAlvo);
                            DIR* dchk = opendir(checkPath);
                            if (dchk) {
                                closedir(dchk);
                                sprintf(destPath, "%s/icon0.png", checkPath);
                                copiarArquivoReal(caminhoImagemAberta, destPath);
                                foundSave = true;
                            }
                        }
                    } closedir(dh);
                }
                if (!foundSave) { sprintf(msgStatus, "SAVE DO JOGO NAO ENCONTRADO!"); msgTimer = 180; }
                else { sprintf(msgStatus, "ICONE DE SAVE APLICADO!"); msgTimer = 180; }
                visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } break;
            }

            if (acaoJogoEscolhida != 35) {
                copiarArquivoReal(caminhoImagemAberta, destPath);
                sprintf(msgStatus, "ASSET DO JOGO SUBSTITUIDO COM SUCESSO!"); msgTimer = 180;
            }
            visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } break;
        }
        break;
    }
    }

    if (!esperandoNomePasta && !esperandoRenomear && menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD) {
        showOpcoes = false; msgTimer = 120;
    }
}

void atualizarImePasta() {
    if (!esperandoNomePasta && !esperandoRenomear) return;

    int status = (int)sceImeDialogGetStatus();
    if (status != IME_STATUS_RUNNING && status != 0) {
        OrbisDialogResult res; memset(&res, 0, sizeof(res)); sceImeDialogGetResult(&res);
        int32_t buttonId = *(int32_t*)&res;

        if (buttonId == 0) {
            char nomeFinal[256]; memset(nomeFinal, 0, sizeof(nomeFinal));
            uint16_t* bufRead = (uint16_t*)textoTeclado; int len = 0;
            for (int i = 0; i < 255; i++) { if (bufRead[i] == 0x0000) break; nomeFinal[len] = (char)bufRead[i]; len++; }
            nomeFinal[len] = '\0';
            bool ehEsq = (painelDuplo && painelAtivo == 0); char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

            if (strlen(nomeFinal) > 0) {
                char nPath[512];
                if (esperandoNomePasta) {
                    sprintf(nPath, "%s/%s", pExplorar, nomeFinal); sceKernelMkdir(nPath, 0777); sprintf(msgStatus, "PASTA CRIADA!");
                }
                else if (esperandoRenomear) {
                    if (!ehPastaParaRenomear && strlen(oldExtParaRenomear) > 0) { if (strrchr(nomeFinal, '.') == NULL) strcat(nomeFinal, oldExtParaRenomear); }
                    sprintf(nPath, "%s/%s", pExplorar, nomeFinal); rename(oldPathParaRenomear, nPath); sprintf(msgStatus, "RENOMEADO COM SUCESSO!");
                }
                if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar);
            }
        }
        sceImeDialogTerm(); esperandoNomePasta = false; esperandoRenomear = false; msgTimer = 120;
    }
}