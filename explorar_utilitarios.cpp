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
    snprintf(msgStatus, sizeof(msgStatus), "LENDO O ARQUIVO ZIP..."); atualizarBarra(0.01f);
    if (!mz_zip_reader_init_file(&zip_archive, zipPath, 0)) {
        int cod = (int)mz_zip_get_last_error(&zip_archive);
        unsigned char hdr[8] = { 0 }; size_t hn = 0; long fsz = -1;
        FILE* fh = fopen(zipPath, "rb");
        if (fh) { fseek(fh, 0, SEEK_END); fsz = ftell(fh); fseek(fh, 0, SEEK_SET); hn = fread(hdr, 1, sizeof(hdr), fh); fclose(fh); }
        if (fsz == 0) { snprintf(msgStatus, sizeof(msgStatus), "ARQUIVO VAZIO (0 bytes)"); }
        else if (hn >= 4 && hdr[0] == 'R' && hdr[1] == 'a' && hdr[2] == 'r' && hdr[3] == '!') { snprintf(msgStatus, sizeof(msgStatus), "ARQUIVO RAR NAO SUPORTADO - CONVERTA PARA ZIP"); }
        else if (hn >= 6 && hdr[0] == 0x37 && hdr[1] == 0x7A && hdr[2] == 0xBC && hdr[3] == 0xAF && hdr[4] == 0x27 && hdr[5] == 0x1C) { snprintf(msgStatus, sizeof(msgStatus), "ARQUIVO 7Z NAO SUPORTADO - CONVERTA PARA ZIP"); }
        else if (hn >= 2 && hdr[0] == 'P' && hdr[1] == 'K') { snprintf(msgStatus, sizeof(msgStatus), "ZIP INCOMPLETO/CORROMPIDO - BAIXE NOVAMENTE (cod %d)", cod); }
        else { snprintf(msgStatus, sizeof(msgStatus), "NAO E UM ZIP VALIDO (cod %d)", cod); }
        FILE* dbg = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
        if (dbg) { fprintf(dbg, "FALHA init cod=%d size=%ld hdr=%02X%02X%02X%02X path=%s\n", cod, fsz, hdr[0], hdr[1], hdr[2], hdr[3], zipPath); fclose(dbg); }
        msgTimer = 360; return;
    }
    int totalArquivos = (int)mz_zip_reader_get_num_files(&zip_archive);
    if (totalArquivos <= 0) {
        mz_zip_reader_end(&zip_archive);
        snprintf(msgStatus, sizeof(msgStatus), "ERRO: ZIP VAZIO OU SEM ARQUIVOS!"); msgTimer = 300; return;
    }
    int extraidos = 0, falhas = 0, semSuporte = 0;
    for (int i = 0; i < totalArquivos; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) { falhas++; continue; }
        const char* nome = file_stat.m_filename;
        if (!nome || nome[0] == '\0') { falhas++; continue; }

        char nomeNorm[1024];
        snprintf(nomeNorm, sizeof(nomeNorm), "%s", nome);
        for (char* q = nomeNorm; *q; q++) { if (*q == '\\') *q = '/'; }
        while (nomeNorm[0] == '/') memmove(nomeNorm, nomeNorm + 1, strlen(nomeNorm));
        while (strncmp(nomeNorm, "./", 2) == 0) memmove(nomeNorm, nomeNorm + 2, strlen(nomeNorm + 2) + 1);

        bool inseguro = false;
        char tmpNome[1024]; snprintf(tmpNome, sizeof(tmpNome), "%s", nomeNorm);
        char* tok = strtok(tmpNome, "/");
        while (tok) { if (strcmp(tok, "..") == 0) { inseguro = true; break; } tok = strtok(NULL, "/"); }
        if (inseguro || nomeNorm[0] == '\0') { falhas++; continue; }

        char out_file[2048];
        snprintf(out_file, sizeof(out_file), "%s/%s", outPath, nomeNorm);
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            criarCaminho(out_file); sceKernelMkdir(out_file, 0777); extraidos++;
        } else if (!mz_zip_reader_is_file_supported(&zip_archive, i)) {
            semSuporte++;
        } else {
            criarCaminho(out_file);
            if (mz_zip_reader_extract_to_file(&zip_archive, i, out_file, 0)) extraidos++;
            else falhas++;
        }
        float prog = (float)(i + 1) / (float)totalArquivos;
        snprintf(msgStatus, sizeof(msgStatus), "EXTRAINDO: %d%%", (int)(prog * 100)); atualizarBarra(prog);
    }
    mz_zip_reader_end(&zip_archive);
    if (semSuporte > 0 && extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ZIP COM COMPRESSAO NAO SUPORTADA (use Normal/Deflate)"); msgTimer = 360; }
    else if (falhas > 0 && extraidos == 0) { snprintf(msgStatus, sizeof(msgStatus), "ERRO AO EXTRAIR: NENHUM ARQUIVO (%d falhas)", falhas); msgTimer = 300; }
    else if (semSuporte > 0) { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO: %d OK, %d sem suporte", extraidos, semSuporte); msgTimer = 300; }
    else if (falhas > 0) { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO: %d OK, %d falhas", extraidos, falhas); msgTimer = 300; }
    else { snprintf(msgStatus, sizeof(msgStatus), "EXTRAIDO COM SUCESSO! (%d arquivos)", extraidos); msgTimer = 180; }
}