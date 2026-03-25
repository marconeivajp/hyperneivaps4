#pragma once
#include <stdint.h>
#include <stdbool.h>

struct FtpServer {
    char name[64];
    char ip[64];
    int port;
    char user[64];
    char pass[64];
};

extern FtpServer listaServidoresFTP[100];
extern int totalServidoresFTP;
extern int servidorAtualFTPIndex;
extern bool ftpSelecionandoUpload;

void carregarServidoresFTP();
void salvarServidoresFTP();
void preencherMenuFTPServidores(bool isUpload);
void preencherMenuEditarServidor(int index);
void abrirTecladoEdicaoFTP(int32_t uId, int campo);

void acessarFTP(int index, const char* path);
void iniciarDownloadFTP(const char* remotePath);
void preencherMenuFTPUploadRaizes();
void listarArquivosUploadFTP(const char* dirPath);
void fazerUploadFTP(const char* localPath);

// --- NOVAS FUNCOES DO EXPLORAR REMOTO ---
void preencherOpcoesFTP();
void acaoOpcaoFTP(int idxOpcao, int32_t uId);
void prepararPreviewFTP(const char* remotePath);