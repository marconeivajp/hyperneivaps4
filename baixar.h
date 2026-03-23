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

void atualizarBarra(float progresso);
void atualizarBarra(float progresso, int arquivoAtual, int totalArquivos);
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

void preencherMenuBackup();
void executarBackupTodos();

extern char caminhoXMLAtual[256];
extern char currentDropboxPath[512];
extern char currentUploadPath[512];

extern char linksAtuais[3000][1024]; // DECLARAÇÃO RESTAURADA

// ==========================================
// NOVO SISTEMA DE FILA DE DOWNLOADS NO TXT
// ==========================================
bool adicionarLinkFila(const char* link);
bool obterPrimeiroLinkFila(char* linkSaida);
void removerPrimeiroLinkFila();
int contarLinksFila();
void processarFilaDownloads();

#endif