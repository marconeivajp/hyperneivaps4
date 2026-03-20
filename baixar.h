#ifndef BAIXAR_H
#define BAIXAR_H

#include <stdbool.h>

struct Console {
    const char* nome;
    const char* pathServidor;
};

extern Console listaConsoles[5];
extern int consoleAtual;

extern unsigned char* imgPreview;
extern int wP, hP, cP;
extern char ultimoJogoCarregado[64];

void acaoRede(const char* jogo, bool buscarLista, bool salvarNoHD);

void preencherMenuBaixar();
void preencherMenuRepositorios();
void listarXMLsRepositorio();
void abrirXMLRepositorio(const char* xmlFile);
void mostrarLinksJogo(int gameIndex);

void acessarDropbox(const char* url);
void iniciarDownload(const char* url);

void listarArquivosUpload(const char* dirPath);
void fazerUploadDropbox(const char* localPath);

// NOVAS FUNÇÕES DO MENU DE BACKUP
void preencherMenuBackup();
void executarBackupTodos();

extern char linksAtuais[3000][1024];
extern char caminhoXMLAtual[256];

extern char currentDropboxPath[512];
extern char currentUploadPath[512];

#endif