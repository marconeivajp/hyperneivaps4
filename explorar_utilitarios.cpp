#include "explorar_utilitarios.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include "miniz.h"

extern void atualizarBarra(float progresso);
extern char msgStatus[128];
extern int msgTimer;

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
    if (!mz_zip_reader_init_file(&zip_archive, zipPath, 0)) {
        sprintf(msgStatus, "ERRO: ARQUIVO ZIP INVALIDO OU CORROMPIDO! (%s)", zipPath);
        msgTimer = 240; return;
    }
    int totalArquivos = (int)mz_zip_reader_get_num_files(&zip_archive);
    if (totalArquivos <= 0) {
        mz_zip_reader_end(&zip_archive);
        sprintf(msgStatus, "ERRO: ZIP VAZIO OU SEM ARQUIVOS!"); msgTimer = 240; return;
    }
    int extraidos = 0, falhas = 0;
    for (int i = 0; i < totalArquivos; i++) {
        mz_zip_archive_file_stat file_stat; if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) { falhas++; continue; }
        const char* nome = file_stat.m_filename;
        if (!nome || nome[0] == '\0') { falhas++; continue; }
        if (nome[0] == '/' || strstr(nome, "..")) { falhas++; continue; }
        char out_file[1024];
        snprintf(out_file, sizeof(out_file), "%s/%s", outPath, nome);
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            criarCaminho(out_file); sceKernelMkdir(out_file, 0777); extraidos++;
        } else {
            criarCaminho(out_file);
            if (mz_zip_reader_extract_to_file(&zip_archive, i, out_file, 0)) extraidos++;
            else falhas++;
        }
        float prog = (float)(i + 1) / (float)totalArquivos;
        sprintf(msgStatus, "EXTRAINDO: %d%%", (int)(prog * 100)); atualizarBarra(prog);
    }
    mz_zip_reader_end(&zip_archive);
    if (falhas > 0 && extraidos == 0) { sprintf(msgStatus, "ERRO AO EXTRAIR: NENHUM ARQUIVO (%d falhas)", falhas); msgTimer = 240; }
    else if (falhas > 0) { sprintf(msgStatus, "EXTRAIDO: %d OK, %d falhas", extraidos, falhas); msgTimer = 240; }
    else { sprintf(msgStatus, "EXTRAIDO COM SUCESSO! (%d arquivos)", extraidos); msgTimer = 180; }
}