#pragma once
#include <stdint.h>

extern bool ftpSelecionandoUpload;

void preencherMenuFTPServidores(bool isUpload);
void setFtpConfigFromLink(const char* ipPort);
void abrirTecladoNovoFTP(int32_t uId);

void acessarFTP(const char* path);
void iniciarDownloadFTP(const char* remotePath);
void preencherMenuFTPUploadRaizes();
void listarArquivosUploadFTP(const char* dirPath);
void fazerUploadFTP(const char* localPath);