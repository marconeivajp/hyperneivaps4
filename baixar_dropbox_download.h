#pragma once
#pragma once
#ifndef BAIXAR_DROPBOX_DOWNLOAD_H
#define BAIXAR_DROPBOX_DOWNLOAD_H

void preencherMenuBackup();
void acessarDropbox(const char* path);
void iniciarDownload(const char* url);
void listarArquivosUpload(const char* dirPath);
void fazerUploadDropbox(const char* localPath);
void executarBackupTodos();

#endif