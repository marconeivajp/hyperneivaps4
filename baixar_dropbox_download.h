#pragma once
#ifndef BAIXAR_DROPBOX_DOWNLOAD_H
#define BAIXAR_DROPBOX_DOWNLOAD_H

void preencherMenuBackup();
void acessarDropbox(const char* path);
void iniciarDownload(const char* url);
void listarArquivosUpload(const char* dirPath);
void fazerUploadDropbox(const char* localPath);

void criarPastaDropbox(const char* remotePath);
void fazerUploadArquivoParaNuvem(const char* localPath, const char* remoteFilePath);
void processarPastaRecursiva(const char* localRoot, const char* currentLocal);
void fazerUploadPastaRecursivo(const char* dirPath);

void fazerDownloadPastaRecursivo(const char* remotePath, const char* folderName);
void processarDownloadPastaRecursiva(const char* remotePath, const char* localPath);
void fazerDownloadArquivoDaNuvem(const char* remotePath, const char* localPath);

// Novas Funções de Seleção Múltipla
void fazerUploadSelecionados();
void fazerDownloadSelecionados();

void executarBackupTodos();

#endif