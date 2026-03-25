#include "explorar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h> 
#include <orbis/libkernel.h>
#include "ImeDialog.h"
#include "CommonDialog.h"
#include "bloco_de_notas.h" 
#include <errno.h> 

#include "miniz.h" 
#include "stb_image.h" 

#define IME_STATUS_RUNNING 1
#define IME_STATUS_FINISHED 2

extern void atualizarBarra(float progresso);

extern bool visualizandoMidiaImagem;
extern char caminhoImagemAberta[512];
extern unsigned char* imgMidia;

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

const char* listaOpcoes[10] = { "", "", "", "", "", "", "", "", "", "" };
int mapOpcoes[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int totalOpcoes = 0;

bool esperandoNomePasta = false;
bool esperandoRenomear = false;
wchar_t textoTeclado[256] = L"";
char oldPathParaRenomear[512] = "";
char oldExtParaRenomear[64] = "";
bool ehPastaParaRenomear = false;

void preencherOpcoesContexto(const char* nomeArquivo) {
    for (int i = 0; i < 10; i++) {
        listaOpcoes[i] = "";
        mapOpcoes[i] = -1;
    }
    totalOpcoes = 0;

    // Menu de Fundo para Imagens Abertas
    if (visualizandoMidiaImagem) {
        listaOpcoes[0] = "definir fundo hyper neiva"; mapOpcoes[0] = 10;
        listaOpcoes[1] = "definir fundo ps4 (em breve)"; mapOpcoes[1] = 11;
        listaOpcoes[2] = "definir avatar ps4 (em breve)"; mapOpcoes[2] = 12;
        totalOpcoes = 3;
        return;
    }

    // ========================================================
    // MENU COMPLETO NORMAL (Mostra sempre no Triângulo!)
    // ========================================================
    listaOpcoes[0] = "nova pasta"; mapOpcoes[0] = 0;
    listaOpcoes[1] = "novo arquivo"; mapOpcoes[1] = 1;
    listaOpcoes[2] = "copiar"; mapOpcoes[2] = 2;
    listaOpcoes[3] = "recortar"; mapOpcoes[3] = 3;
    listaOpcoes[4] = "colar"; mapOpcoes[4] = 4;
    listaOpcoes[5] = "renomear"; mapOpcoes[5] = 5;
    listaOpcoes[6] = "deletar"; mapOpcoes[6] = 6;
    listaOpcoes[7] = "selecionar"; mapOpcoes[7] = 8;
    listaOpcoes[8] = "selecionar tudo"; mapOpcoes[8] = 9;

    // Se for ZIP, adiciona o Extrair no final da lista do Triângulo!
    bool isZip = false;
    if (nomeArquivo) {
        char temp[256];
        strcpy(temp, nomeArquivo);
        for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
        if (strstr(temp, ".zip") != NULL || strstr(temp, ".xavatar") != NULL) {
            isZip = true;
        }
    }

    if (isZip) {
        listaOpcoes[9] = "extrair zip / avatar"; mapOpcoes[9] = 7;
        totalOpcoes = 10;
    }
    else {
        totalOpcoes = 9;
    }
}

void copiarArquivoReal(const char* origem, const char* destino) {
    FILE* src = fopen(origem, "rb"); if (!src) return;
    FILE* dst = fopen(destino, "wb"); if (!dst) { fclose(src); return; }
    char buffer[65536]; size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    fclose(src); fclose(dst);
}

void deletarPastaRecursivamente(const char* path) {
    DIR* d = opendir(path);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                char fullPath[1024];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);

                struct stat st;
                if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
                    deletarPastaRecursivamente(fullPath);
                }
                else {
                    unlink(fullPath);
                }
            }
        }
        closedir(d);
    }
    rmdir(path);
}

void criarCaminho(const char* filepath) {
    char tmp[1024];
    strncpy(tmp, filepath, sizeof(tmp));
    for (char* p = strchr(tmp + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        sceKernelMkdir(tmp, 0777);
        *p = '/';
    }
}

void extrairZip(const char* zipPath, const char* outPath) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    sprintf(msgStatus, "LENDO O ARQUIVO ZIP...");
    atualizarBarra(0.01f);

    if (!mz_zip_reader_init_file(&zip_archive, zipPath, 0)) {
        sprintf(msgStatus, "ERRO: ARQUIVO ZIP INVALIDO OU CORROMPIDO!");
        msgTimer = 180;
        return;
    }

    int totalArquivos = (int)mz_zip_reader_get_num_files(&zip_archive);

    for (int i = 0; i < totalArquivos; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;

        char out_file[1024];
        snprintf(out_file, sizeof(out_file), "%s/%s", outPath, file_stat.m_filename);

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            criarCaminho(out_file);
            sceKernelMkdir(out_file, 0777);
        }
        else {
            criarCaminho(out_file);
            mz_zip_reader_extract_to_file(&zip_archive, i, out_file, 0);
        }

        float prog = (float)(i + 1) / (float)totalArquivos;
        sprintf(msgStatus, "EXTRAINDO: %d%%", (int)(prog * 100));
        atualizarBarra(prog);
    }

    mz_zip_reader_end(&zip_archive);
    sprintf(msgStatus, "EXTRAIDO COM SUCESSO!");
    msgTimer = 180;
}

void listarDiretorio(const char* path) {
    DIR* d = opendir(path);
    if (!d) {
        if (errno == EACCES) sprintf(msgStatus, "ERRO 13: SEM PERMISSAO DE ROOT");
        else if (errno == ENOENT) sprintf(msgStatus, "ERRO 2: INVISIVEL (SANDBOX) - %s", path);
        else sprintf(msgStatus, "ERRO %d AO ABRIR USB", errno);
        msgTimer = 240;
        return;
    }

    memset(marcados, 0, sizeof(marcados));
    memset(nomes, 0, sizeof(nomes));
    ItemLista temp[3000]; int count = 0; struct dirent* dir;

    while ((dir = readdir(d)) != NULL && count < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char cleanName[128];
        strcpy(cleanName, dir->d_name);
        int len = strlen(cleanName);
        while (len > 0 && (cleanName[len - 1] == ' ' || cleanName[len - 1] == '\r' || cleanName[len - 1] == '\n')) {
            cleanName[len - 1] = '\0';
            len--;
        }

        strncpy(temp[count].nome, cleanName, 63);

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, cleanName);
        struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
            temp[count].ehPasta = true;
        }
        else {
            temp[count].ehPasta = false;
        }
        count++;
    }
    closedir(d);

    for (int i = 0; i < count - 1; i++) for (int j = 0; j < count - i - 1; j++) {
        bool trocar = false;
        if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
        else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
        if (trocar) { ItemLista aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
    }
    for (int i = 0; i < count; i++) {
        if (temp[i].ehPasta) sprintf(nomes[i], "[%s]", temp[i].nome);
        else strcpy(nomes[i], temp[i].nome);
    }
    totalItens = count; strcpy(pathExplorar, path); menuAtual = MENU_EXPLORAR;
    sel = 0;
}

void listarDiretorioEsq(const char* path) {
    DIR* d = opendir(path);
    if (!d) {
        if (errno == EACCES) sprintf(msgStatus, "ERRO 13: SEM PERMISSAO DE ROOT");
        else if (errno == ENOENT) sprintf(msgStatus, "ERRO 2: INVISIVEL (SANDBOX) - %s", path);
        else sprintf(msgStatus, "ERRO %d AO ABRIR USB", errno);
        msgTimer = 240;
        return;
    }

    memset(marcadosEsq, 0, sizeof(marcadosEsq));
    memset(nomesEsq, 0, sizeof(nomesEsq));
    ItemLista temp[3000]; int count = 0; struct dirent* dir;

    while ((dir = readdir(d)) != NULL && count < 3000) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char cleanName[128];
        strcpy(cleanName, dir->d_name);
        int len = strlen(cleanName);
        while (len > 0 && (cleanName[len - 1] == ' ' || cleanName[len - 1] == '\r' || cleanName[len - 1] == '\n')) {
            cleanName[len - 1] = '\0';
            len--;
        }

        strncpy(temp[count].nome, cleanName, 63);

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, cleanName);
        struct stat st;
        if (dir->d_type == DT_DIR || (dir->d_type == DT_UNKNOWN && stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode))) {
            temp[count].ehPasta = true;
        }
        else {
            temp[count].ehPasta = false;
        }
        count++;
    }
    closedir(d);

    for (int i = 0; i < count - 1; i++) for (int j = 0; j < count - i - 1; j++) {
        bool trocar = false;
        if (!temp[j].ehPasta && temp[j + 1].ehPasta) trocar = true;
        else if (temp[j].ehPasta == temp[j + 1].ehPasta && strcasecmp(temp[j].nome, temp[j + 1].nome) > 0) trocar = true;
        if (trocar) { ItemLista aux = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = aux; }
    }
    for (int i = 0; i < count; i++) {
        if (temp[i].ehPasta) sprintf(nomesEsq[i], "[%s]", temp[i].nome);
        else strcpy(nomesEsq[i], temp[i].nome);
    }
    totalItensEsq = count; strcpy(pathExplorarEsq, path); menuAtualEsq = MENU_EXPLORAR;
    selEsq = 0;
}

void acaoArquivo(int idxOpcao) {
    if (idxOpcao < 0 || idxOpcao >= 10) return;
    int op = mapOpcoes[idxOpcao];
    if (op == -1) return;

    bool ehEsq = (painelDuplo && painelAtivo == 0);
    int tItens = ehEsq ? totalItensEsq : totalItens;
    bool* mItems = ehEsq ? marcadosEsq : marcados;
    char (*nItems)[64] = ehEsq ? nomesEsq : nomes;
    char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;
    int sAtual = ehEsq ? selEsq : sel;

    bool temMarcado = false;
    for (int i = 0; i < tItens; i++) if (mItems[i]) { temMarcado = true; break; }

    switch (op) {
    case 0: { // Nova Pasta
        OrbisImeDialogSetting param;
        memset(&param, 0, sizeof(param));
        memset(textoTeclado, 0, sizeof(textoTeclado));

        param.maxTextLength = 255;
        param.inputTextBuffer = textoTeclado;
        param.title = L"Nome da Nova Pasta";
        param.type = (OrbisImeType)0;

        if (sceImeDialogInit(&param, NULL) >= 0) {
            esperandoNomePasta = true;
            showOpcoes = false;
        }
        break;
    }
    case 1: { // NOVO ARQUIVO
        inicializarNotepad();
        strcpy(pastaDestinoFinal, pExplorar);
        if (ehEsq) menuAtualEsq = MENU_NOTEPAD; else menuAtual = MENU_NOTEPAD;
        showOpcoes = false;
        break;
    }
    case 2: // Copiar
    case 3: { // Recortar
        clipboardCount = 0;
        clipboardIsCut = (op == 3);
        for (int i = 0; i < tItens; i++) {
            if (temMarcado ? mItems[i] : (i == sAtual)) {
                char* l = (nItems[i][0] == '[') ? &nItems[i][1] : nItems[i];
                sprintf(clipboardPaths[clipboardCount], "%s/%s", pExplorar, l);
                if (nItems[i][0] == '[') clipboardPaths[clipboardCount][strlen(clipboardPaths[clipboardCount]) - 1] = '\0';
                clipboardCount++;
            }
        }
        sprintf(msgStatus, op == 2 ? "ARQUIVO(S) COPIADO(S)" : "ARQUIVO(S) RECORTADO(S)");
        msgTimer = 90;
        break;
    }
    case 4: { // Colar
        for (int i = 0; i < clipboardCount; i++) {
            char* baseName = strrchr(clipboardPaths[i], '/');
            if (baseName) baseName++; else baseName = clipboardPaths[i];
            char dest[512];
            sprintf(dest, "%s/%s", pExplorar, baseName);

            copiarArquivoReal(clipboardPaths[i], dest);

            if (clipboardIsCut) {
                struct stat path_stat;
                stat(clipboardPaths[i], &path_stat);
                if (S_ISDIR(path_stat.st_mode)) {
                    deletarPastaRecursivamente(clipboardPaths[i]);
                }
                else {
                    unlink(clipboardPaths[i]);
                }
            }
        }
        if (ehEsq) listarDiretorioEsq(pathExplorarEsq); else listarDiretorio(pathExplorar);
        if (clipboardIsCut) { clipboardCount = 0; clipboardIsCut = false; }

        sprintf(msgStatus, "ARQUIVO(S) COLADO(S)");
        msgTimer = 90;
        break;
    }
    case 5: { // Renomear
        int alvo = sAtual;
        for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }

        char* nomeReal = nItems[alvo];
        ehPastaParaRenomear = (nomeReal[0] == '[') ? true : false;

        char nomeLimpo[256];
        if (ehPastaParaRenomear) {
            strncpy(nomeLimpo, &nomeReal[1], strlen(nomeReal) - 2);
            nomeLimpo[strlen(nomeReal) - 2] = '\0';
        }
        else {
            strcpy(nomeLimpo, nomeReal);
        }

        sprintf(oldPathParaRenomear, "%s/%s", pExplorar, nomeLimpo);

        memset(oldExtParaRenomear, 0, sizeof(oldExtParaRenomear));
        if (!ehPastaParaRenomear) {
            char* dot = strrchr(nomeLimpo, '.');
            if (dot) strcpy(oldExtParaRenomear, dot);
        }

        OrbisImeDialogSetting param;
        memset(&param, 0, sizeof(param));
        memset(textoTeclado, 0, sizeof(textoTeclado));

        uint16_t* bufWrite = (uint16_t*)textoTeclado;
        for (int i = 0; nomeLimpo[i] != '\0' && i < 255; i++) {
            bufWrite[i] = (uint16_t)nomeLimpo[i];
        }

        param.maxTextLength = 255;
        param.inputTextBuffer = textoTeclado;
        param.title = L"Renomear Arquivo/Pasta";
        param.type = (OrbisImeType)0;

        if (sceImeDialogInit(&param, NULL) >= 0) {
            esperandoRenomear = true;
            showOpcoes = false;
        }
        break;
    }
    case 6: { // Deletar
        for (int i = 0; i < tItens; i++) {
            if (temMarcado ? mItems[i] : (i == sAtual)) {
                char f[512]; char* l = (nItems[i][0] == '[') ? &nItems[i][1] : nItems[i];
                sprintf(f, "%s/%s", pExplorar, l);

                if (nItems[i][0] == '[') {
                    f[strlen(f) - 1] = '\0';
                    deletarPastaRecursivamente(f);
                }
                else {
                    unlink(f);
                }
            }
        }
        if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar);
        break;
    }
    case 7: { // EXTRAIR ZIP
        int alvo = sAtual;
        for (int i = 0; i < tItens; i++) if (mItems[i]) { alvo = i; break; }

        char* nomeReal = nItems[alvo];
        if (nomeReal[0] == '[') {
            sprintf(msgStatus, "SELECIONE UM ARQUIVO .ZIP");
            msgTimer = 120;
            break;
        }

        char pathFinal[512];
        sprintf(pathFinal, "%s/%s", pExplorar, nomeReal);

        char temp[512]; strcpy(temp, pathFinal);
        for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);

        if (strstr(temp, ".zip") != NULL || strstr(temp, ".xavatar") != NULL) {
            extrairZip(pathFinal, pExplorar);
        }
        else {
            sprintf(msgStatus, "SOMENTE ARQUIVOS .ZIP SAO SUPORTADOS");
            msgTimer = 120;
        }

        if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar);
        break;
    }
    case 8: { // Selecionar
        mItems[sAtual] = !mItems[sAtual];
        break;
    }
    case 9: { // Selecionar tudo
        bool ligar = false; for (int i = 0; i < tItens; i++) if (!mItems[i]) ligar = true;
        for (int i = 0; i < tItens; i++) mItems[i] = ligar;
        break;
    }

          // =========================================================
          // AÇÕES DO FUNDO DE TELA
          // =========================================================
    case 10: {
        remove("/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.png");
        remove("/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.jpg");

        FILE* src = fopen(caminhoImagemAberta, "rb");
        if (src) {
            char* ext = strrchr(caminhoImagemAberta, '.');
            char dest[512];
            if (ext && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)) {
                strcpy(dest, "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.jpg");
            }
            else {
                strcpy(dest, "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.png");
            }
            FILE* dst = fopen(dest, "wb");
            if (dst) {
                char buf[8192]; size_t bytes;
                while ((bytes = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, bytes, dst);
                fclose(dst);
                sprintf(msgStatus, "PLANO DE FUNDO APLICADO COM SUCESSO!");
            }
            else {
                sprintf(msgStatus, "ERRO AO SALVAR PLANO DE FUNDO");
            }
            fclose(src);
            msgTimer = 180;
        }
        visualizandoMidiaImagem = false;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        break;
    }
    case 11: {
        sprintf(msgStatus, "Funcao Fundo PS4 em breve!"); msgTimer = 120;
        visualizandoMidiaImagem = false;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        break;
    }
    case 12: {
        sprintf(msgStatus, "Funcao Avatar PS4 em breve!"); msgTimer = 120;
        visualizandoMidiaImagem = false;
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        break;
    }
    }

    if (!esperandoNomePasta && !esperandoRenomear && menuAtual != MENU_NOTEPAD && menuAtualEsq != MENU_NOTEPAD) {
        showOpcoes = false;
        msgTimer = 120;
    }
}

void atualizarImePasta() {
    if (!esperandoNomePasta && !esperandoRenomear) return;

    int status = (int)sceImeDialogGetStatus();
    if (status != IME_STATUS_RUNNING && status != 0) {
        OrbisDialogResult res;
        memset(&res, 0, sizeof(res));
        sceImeDialogGetResult(&res);

        int32_t buttonId = *(int32_t*)&res;

        if (buttonId == 0) {
            char nomeFinal[256];
            memset(nomeFinal, 0, sizeof(nomeFinal));

            uint16_t* bufRead = (uint16_t*)textoTeclado;
            int len = 0;

            for (int i = 0; i < 255; i++) {
                if (bufRead[i] == 0x0000) {
                    break;
                }
                nomeFinal[len] = (char)bufRead[i];
                len++;
            }
            nomeFinal[len] = '\0';

            bool ehEsq = (painelDuplo && painelAtivo == 0);
            char* pExplorar = ehEsq ? pathExplorarEsq : pathExplorar;

            if (strlen(nomeFinal) > 0) {
                char nPath[512];

                if (esperandoNomePasta) {
                    sprintf(nPath, "%s/%s", pExplorar, nomeFinal);
                    sceKernelMkdir(nPath, 0777);
                    sprintf(msgStatus, "PASTA CRIADA!");
                }
                else if (esperandoRenomear) {
                    if (!ehPastaParaRenomear && strlen(oldExtParaRenomear) > 0) {
                        if (strrchr(nomeFinal, '.') == NULL) {
                            strcat(nomeFinal, oldExtParaRenomear);
                        }
                    }
                    sprintf(nPath, "%s/%s", pExplorar, nomeFinal);
                    rename(oldPathParaRenomear, nPath);
                    sprintf(msgStatus, "RENOMEADO COM SUCESSO!");
                }

                if (ehEsq) listarDiretorioEsq(pExplorar); else listarDiretorio(pExplorar);
            }
        }

        sceImeDialogTerm();
        esperandoNomePasta = false;
        esperandoRenomear = false;
        msgTimer = 120;
    }
}