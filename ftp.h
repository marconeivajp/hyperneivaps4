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

extern int ftpL2State;
extern char currentFtpPathEsq[1024];
extern char linksAtuaisEsq[3000][1024];

extern char ftpClipPaths[100][1024];
extern bool ftpClipIsDir[100];
extern int ftpClipCount;
extern bool ftpClipIsCut;
extern int ftpClipSource;

void carregarServidoresFTP();
void salvarServidoresFTP();
void preencherMenuFTPServidores(bool isUpload);
void preencherMenuEditarServidor(int index);
void abrirTecladoEdicaoFTP(int32_t uId, int campo);

void acessarFTP(int index, const char* path);
void acessarFTPEsq(int index, const char* path);

// O NOVO MOTOR DE FILA INTELIGENTE (COPIA PASTAS E ARQUIVOS)
void adicionarFilaFTP(const char* sourcePath, const char* destPath, bool isUpload, bool isDir);
void iniciarProcessamentoFilaFTP();

void acaoL2_FTP();
void preencherOpcoesFTP();
void acaoOpcaoFTP(int idxOpcao, int32_t uId);
void prepararPreviewFTP(const char* remotePath);