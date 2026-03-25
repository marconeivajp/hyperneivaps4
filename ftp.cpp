#include "ftp.h"
#include "menu.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <orbis/libkernel.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

#include "ImeDialog.h"
#include "CommonDialog.h"
#include "stb_image.h"

extern char nomes[3000][64];
extern char linksAtuais[3000][1024];
extern int totalItens;
extern MenuLevel menuAtual;
extern int sel;
extern int off;
extern bool marcados[3000];

extern char currentUploadPath[512];
extern char msgStatus[128];
extern int msgTimer;
extern void atualizarBarra(float progresso);
extern void instalarPkgLocal(const char* caminhoAbsoluto);

// VARIAVEIS DO EXPLORAR / OPCOES
extern const char* listaOpcoes[10];
extern int totalOpcoes;
extern bool showOpcoes;
extern int selOpcao;

// VARIAVEIS DO VISUALIZADOR DE MÍDIA
extern bool visualizandoMidiaImagem;
extern unsigned char* imgMidia;
extern int wM, hM;
extern float zoomMidia;
extern bool fullscreenMidia;
extern bool visualizandoMidiaTexto;
extern char* textoMidiaBuffer;
extern char* linhasTexto[5000];
extern int totalLinhasTexto;
extern int textoMidiaScroll;

FtpServer listaServidoresFTP[100];
int totalServidoresFTP = 0;
int servidorAtualFTPIndex = 0;
bool ftpSelecionandoUpload = false;
char currentFtpPath[1024] = "/";
char pathSendoRenomeado[1024] = "";

// ==============================================================
// BASE DE REDE E CONEXÃO
// ==============================================================
int send_ftp_cmd(int sock, const char* cmd, char* response) {
    if (cmd) { send(sock, cmd, strlen(cmd), 0); send(sock, "\r\n", 2, 0); }
    memset(response, 0, 2048); int total = 0;
    while (total < 2047) {
        char line[512]; memset(line, 0, 512); int idx = 0;
        while (idx < 511) { char c; int n = recv(sock, &c, 1, 0); if (n <= 0) break; line[idx++] = c; if (c == '\n') break; }
        strcat(response, line); total += idx;
        if (idx >= 4 && line[3] == ' ' && isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2])) break;
    }
    return total;
}

int ftp_connect_control(int srvIdx) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_port = htons(listaServidoresFTP[srvIdx].port);
    inet_pton(AF_INET, listaServidoresFTP[srvIdx].ip, &addr.sin_addr);
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (res < 0) {
        if (errno == EINPROGRESS) {
            fd_set set; FD_ZERO(&set); FD_SET(sock, &set); struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
            res = select(sock + 1, NULL, &set, NULL, &tv); if (res <= 0) { close(sock); return -1; }
            int error = 0; socklen_t len = sizeof(error); getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
            if (error != 0) { close(sock); return -1; }
        }
        else { close(sock); return -1; }
    }
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    struct timeval tv2; tv2.tv_sec = 5; tv2.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv2, sizeof tv2);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv2, sizeof tv2);
    char resp[2048]; send_ftp_cmd(sock, NULL, resp);
    return sock;
}

int ftp_enter_pasv(int ctrl_sock) {
    char resp[2048]; send_ftp_cmd(ctrl_sock, "PASV", resp);
    int h1, h2, h3, h4, p1, p2; char* start = strchr(resp, '('); if (!start) return -1;
    sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    int data_port = (p1 << 8) | p2; char ip[32]; sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    int data_sock = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in addr;
    addr.sin_family = AF_INET; addr.sin_port = htons(data_port); inet_pton(AF_INET, ip, &addr.sin_addr);
    struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0; setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    if (connect(data_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(data_sock); return -1; }
    return data_sock;
}

// COMANDOS RÁPIDOS DE CONTROLE (DELETAR, CRIAR PASTA)
int ftp_comando_simples(int srvIdx, const char* cmd) {
    int ctrl_sock = ftp_connect_control(srvIdx); if (ctrl_sock < 0) return -1;
    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", listaServidoresFTP[srvIdx].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(listaServidoresFTP[srvIdx].pass) > 0) sprintf(cmdPass, "PASS %s", listaServidoresFTP[srvIdx].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com"); send_ftp_cmd(ctrl_sock, cmdPass, resp);
    int res = send_ftp_cmd(ctrl_sock, cmd, resp);
    send_ftp_cmd(ctrl_sock, "QUIT", resp); close(ctrl_sock); return res;
}

void ftp_renomear(int srvIdx, const char* oldPath, const char* newPath) {
    int ctrl_sock = ftp_connect_control(srvIdx); if (ctrl_sock < 0) return;
    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", listaServidoresFTP[srvIdx].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(listaServidoresFTP[srvIdx].pass) > 0) sprintf(cmdPass, "PASS %s", listaServidoresFTP[srvIdx].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com"); send_ftp_cmd(ctrl_sock, cmdPass, resp);
    char cmd1[512], cmd2[512];
    sprintf(cmd1, "RNFR %s", oldPath); send_ftp_cmd(ctrl_sock, cmd1, resp);
    sprintf(cmd2, "RNTO %s", newPath); send_ftp_cmd(ctrl_sock, cmd2, resp);
    send_ftp_cmd(ctrl_sock, "QUIT", resp); close(ctrl_sock);
}

bool parse_ftp_line(char* line, char* nameOut, bool* isDirOut) {
    *isDirOut = false; nameOut[0] = '\0';
    if (line[0] == 'd' || line[0] == '-') {
        *isDirOut = (line[0] == 'd'); char* p = line; int spaces = 0;
        while (*p && spaces < 8) { if (*p == ' ') { spaces++; while (*(p + 1) == ' ') p++; } p++; }
        if (*p) { strcpy(nameOut, p); return true; }
    }
    else if (isdigit(line[0])) {
        if (strstr(line, "<DIR>")) *isDirOut = true; char* p = line; int spaces = 0;
        while (*p && spaces < 3) { if (*p == ' ') { spaces++; while (*(p + 1) == ' ') p++; } p++; }
        if (*p) { strcpy(nameOut, p); return true; }
    }
    return false;
}

// ==============================================================
// SISTEMA DE SERVIDORES, EDIÇÃO E TECLADO
// ==============================================================
void salvarServidoresFTP() {
    FILE* f = fopen("/data/HyperNeiva/configuracao/ftp_servers_v2.txt", "w");
    if (f) {
        for (int i = 0; i < totalServidoresFTP; i++) {
            fprintf(f, "%s|%s|%d|%s|%s\n", listaServidoresFTP[i].name, listaServidoresFTP[i].ip, listaServidoresFTP[i].port, listaServidoresFTP[i].user, listaServidoresFTP[i].pass);
        }
        fclose(f);
    }
}

void carregarServidoresFTP() {
    sceKernelMkdir("/data/HyperNeiva", 0777); sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    totalServidoresFTP = 0;
    FILE* f = fopen("/data/HyperNeiva/configuracao/ftp_servers_v2.txt", "r");
    if (f) {
        char linha[512];
        while (fgets(linha, sizeof(linha), f) && totalServidoresFTP < 100) {
            linha[strcspn(linha, "\r\n")] = 0; if (strlen(linha) < 5) continue;
            char* p1 = strchr(linha, '|'); if (!p1) continue; *p1 = '\0';
            char* p2 = strchr(p1 + 1, '|'); if (!p2) continue; *p2 = '\0';
            char* p3 = strchr(p2 + 1, '|'); if (!p3) continue; *p3 = '\0';
            char* p4 = strchr(p3 + 1, '|'); if (p4) *p4 = '\0';
            strcpy(listaServidoresFTP[totalServidoresFTP].name, linha); strcpy(listaServidoresFTP[totalServidoresFTP].ip, p1 + 1);
            listaServidoresFTP[totalServidoresFTP].port = atoi(p2 + 1); strcpy(listaServidoresFTP[totalServidoresFTP].user, p3 + 1);
            if (p4) strcpy(listaServidoresFTP[totalServidoresFTP].pass, p4 + 1); else strcpy(listaServidoresFTP[totalServidoresFTP].pass, "");
            totalServidoresFTP++;
        } fclose(f);
    }
    if (totalServidoresFTP == 0) {
        strcpy(listaServidoresFTP[0].name, "PC do Marcao"); strcpy(listaServidoresFTP[0].ip, "192.168.0.5"); listaServidoresFTP[0].port = 21;
        strcpy(listaServidoresFTP[0].user, "anonymous"); strcpy(listaServidoresFTP[0].pass, "");
        totalServidoresFTP = 1; salvarServidoresFTP();
    }
}

void preencherMenuFTPServidores(bool isUpload) {
    ftpSelecionandoUpload = isUpload; carregarServidoresFTP();
    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais));
    strcpy(nomes[0], "[+] Adicionar Novo Servidor"); totalItens = 1;
    for (int i = 0; i < totalServidoresFTP; i++) {
        sprintf(nomes[totalItens], "%s (%s:%d)", listaServidoresFTP[i].name, listaServidoresFTP[i].ip, listaServidoresFTP[i].port); totalItens++;
    }
    menuAtual = MENU_BAIXAR_FTP_SERVIDORES; sel = 0; off = 0;
}

void preencherMenuEditarServidor(int index) {
    memset(nomes, 0, sizeof(nomes));
    sprintf(nomes[0], "Nome: %s", listaServidoresFTP[index].name); sprintf(nomes[1], "IP: %s", listaServidoresFTP[index].ip);
    sprintf(nomes[2], "Porta: %d", listaServidoresFTP[index].port); sprintf(nomes[3], "Usuario: %s", listaServidoresFTP[index].user);
    sprintf(nomes[4], "Senha: %s", strlen(listaServidoresFTP[index].pass) > 0 ? "****" : "(vazio)");
    strcpy(nomes[5], "[ SALVAR SERVIDOR ]"); strcpy(nomes[6], "[ DELETAR SERVIDOR ]");
    totalItens = 7; menuAtual = MENU_BAIXAR_FTP_EDITAR_SERVIDOR; sel = 0; off = 0;
}

volatile bool esperandoInputFTP = false;
wchar_t textoTecladoFTP[256] = L"";
int campoSendoEditado = -1;

void* threadPollerIMEFTP(void* arg) {
    while (esperandoInputFTP) {
        int status = (int)sceImeDialogGetStatus();
        if (status != 1 && status != 0) {
            OrbisDialogResult res; memset(&res, 0, sizeof(res)); sceImeDialogGetResult(&res);
            int32_t buttonId = *(int32_t*)&res;

            if (buttonId == 0) {
                char txtFinal[256]; memset(txtFinal, 0, sizeof(txtFinal));
                uint16_t* bufRead = (uint16_t*)textoTecladoFTP; int len = 0;
                for (int i = 0; i < 255; i++) { if (bufRead[i] == 0x0000) break; txtFinal[len++] = (char)bufRead[i]; } txtFinal[len] = '\0';

                // Lógica Dinâmica baseada no "Campo" aberto
                if (campoSendoEditado == 0) strcpy(listaServidoresFTP[servidorAtualFTPIndex].name, txtFinal);
                else if (campoSendoEditado == 1) strcpy(listaServidoresFTP[servidorAtualFTPIndex].ip, txtFinal);
                else if (campoSendoEditado == 2) listaServidoresFTP[servidorAtualFTPIndex].port = atoi(txtFinal);
                else if (campoSendoEditado == 3) strcpy(listaServidoresFTP[servidorAtualFTPIndex].user, txtFinal);
                else if (campoSendoEditado == 4) strcpy(listaServidoresFTP[servidorAtualFTPIndex].pass, txtFinal);
                else if (campoSendoEditado == 10) { // NOVA PASTA FTP
                    char cmd[1024]; sprintf(cmd, "MKD %s%s%s", currentFtpPath, strcmp(currentFtpPath, "/") == 0 ? "" : "/", txtFinal);
                    ftp_comando_simples(servidorAtualFTPIndex, cmd); acessarFTP(servidorAtualFTPIndex, currentFtpPath);
                    sprintf(msgStatus, "PASTA CRIADA NO PC!"); msgTimer = 180;
                }
                else if (campoSendoEditado == 11) { // RENOMEAR FTP
                    char newP[1024]; sprintf(newP, "%s%s%s", currentFtpPath, strcmp(currentFtpPath, "/") == 0 ? "" : "/", txtFinal);
                    ftp_renomear(servidorAtualFTPIndex, pathSendoRenomeado, newP); acessarFTP(servidorAtualFTPIndex, currentFtpPath);
                    sprintf(msgStatus, "RENOMEADO COM SUCESSO!"); msgTimer = 180;
                }
                if (campoSendoEditado <= 4) preencherMenuEditarServidor(servidorAtualFTPIndex);
            }
            sceImeDialogTerm(); esperandoInputFTP = false; break;
        } sceKernelUsleep(100 * 1000);
    } return NULL;
}

void abrirTecladoEdicaoFTP(int32_t uId, int campo) {
    campoSendoEditado = campo;
    OrbisImeDialogSetting param; memset(&param, 0, sizeof(param)); memset(textoTecladoFTP, 0, sizeof(textoTecladoFTP));
    char atualText[256] = "";
    if (campo == 0) strcpy(atualText, listaServidoresFTP[servidorAtualFTPIndex].name); else if (campo == 1) strcpy(atualText, listaServidoresFTP[servidorAtualFTPIndex].ip);
    else if (campo == 2) sprintf(atualText, "%d", listaServidoresFTP[servidorAtualFTPIndex].port); else if (campo == 3) strcpy(atualText, listaServidoresFTP[servidorAtualFTPIndex].user);
    else if (campo == 4) strcpy(atualText, listaServidoresFTP[servidorAtualFTPIndex].pass);

    // Se for renomear, preenche o nome atual (sem a barra do diretório)
    if (campo == 11) {
        char* pt = strrchr(pathSendoRenomeado, '/');
        if (pt) strcpy(atualText, pt + 1); else strcpy(atualText, pathSendoRenomeado);
    }

    uint16_t* wPtr = (uint16_t*)textoTecladoFTP; for (int i = 0; atualText[i]; i++) wPtr[i] = atualText[i];
    param.userId = uId; param.maxTextLength = 255; param.inputTextBuffer = textoTecladoFTP;

    if (campo == 0) param.title = L"Editar Nome do Servidor"; else if (campo == 1) param.title = L"Editar IP do Computador";
    else if (campo == 2) param.title = L"Editar Porta (Normalmente 21)"; else if (campo == 3) param.title = L"Editar Usuario";
    else if (campo == 4) param.title = L"Editar Senha"; else if (campo == 10) param.title = L"Nome da Nova Pasta";
    else if (campo == 11) param.title = L"Renomear Arquivo/Pasta";
    param.type = (OrbisImeType)0;

    if (sceImeDialogInit(&param, NULL) >= 0) { esperandoInputFTP = true; pthread_t t; pthread_create(&t, NULL, threadPollerIMEFTP, NULL); pthread_detach(t); }
}

// ==============================================================
// MENUS DE CONTEXTO E FORMATOS (LOGICA DO EXPLORAR)
// ==============================================================
void preencherOpcoesFTP() {
    listaOpcoes[0] = "baixar selecionados";
    listaOpcoes[1] = "nova pasta";
    listaOpcoes[2] = "renomear";
    listaOpcoes[3] = "deletar";
    listaOpcoes[4] = "selecionar";
    listaOpcoes[5] = "selecionar tudo";
    totalOpcoes = 6;
    showOpcoes = true;
    selOpcao = 0;
}

void deletar_selecionados_ftp() {
    bool temMarcado = false;
    for (int i = 0; i < totalItens; i++) if (marcados[i]) { temMarcado = true; break; }

    for (int i = 0; i < totalItens; i++) {
        if (marcados[i] || (!temMarcado && i == sel)) {
            char cmd[1024]; char urlSel[1024]; strcpy(urlSel, linksAtuais[i]); int tam = strlen(urlSel);
            if (tam > 0 && urlSel[tam - 1] == '/') {
                urlSel[tam - 1] = '\0'; sprintf(cmd, "RMD %s", urlSel); // Deleta diretório
            }
            else {
                sprintf(cmd, "DELE %s", urlSel); // Deleta arquivo
            }
            ftp_comando_simples(servidorAtualFTPIndex, cmd);
        }
    }
    acessarFTP(servidorAtualFTPIndex, currentFtpPath); // Recarrega
    sprintf(msgStatus, "ARQUIVOS DELETADOS!"); msgTimer = 180;
}

void acaoOpcaoFTP(int idxOpcao, int32_t uId) {
    if (idxOpcao == 0) { // Baixar Lote
        bool temMarcado = false;
        for (int i = 0; i < totalItens; i++) if (marcados[i]) { temMarcado = true; break; }
        for (int i = 0; i < totalItens; i++) {
            if (marcados[i] || (!temMarcado && i == sel)) {
                if (nomes[i][0] != '[') iniciarDownloadFTP(linksAtuais[i]); // Adiciona arquivos na fila
            }
        }
        showOpcoes = false;
    }
    else if (idxOpcao == 1) { // Nova Pasta
        abrirTecladoEdicaoFTP(uId, 10); showOpcoes = false;
    }
    else if (idxOpcao == 2) { // Renomear
        bool temMarcado = false; int alvo = sel;
        for (int i = 0; i < totalItens; i++) if (marcados[i]) { temMarcado = true; alvo = i; break; }
        strcpy(pathSendoRenomeado, linksAtuais[alvo]);
        int tam = strlen(pathSendoRenomeado); if (tam > 0 && pathSendoRenomeado[tam - 1] == '/') pathSendoRenomeado[tam - 1] = '\0';
        abrirTecladoEdicaoFTP(uId, 11); showOpcoes = false;
    }
    else if (idxOpcao == 3) { // Deletar
        deletar_selecionados_ftp(); showOpcoes = false;
    }
    else if (idxOpcao == 4) { // Selecionar
        marcados[sel] = !marcados[sel];
    }
    else if (idxOpcao == 5) { // Selecionar Tudo
        bool ligar = false; for (int i = 0; i < totalItens; i++) if (!marcados[i]) ligar = true;
        for (int i = 0; i < totalItens; i++) marcados[i] = ligar;
    }
}

// ==============================================================
// VISUALIZADOR DE MÍDIAS (IMAGEM E TEXTO) VIA FTP
// ==============================================================
void* threadPreviewFTP(void* arg) {
    char* remotePath = (char*)arg; char nomeArquivo[256];
    char* ref = strrchr(remotePath, '/'); if (ref) strcpy(nomeArquivo, ref + 1); else strcpy(nomeArquivo, remotePath);

    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
    char localPath[512] = "/data/HyperNeiva/configuracao/temporario/preview_ftp.tmp";

    // Faz o download invisível pro HD Temp do PS4
    int ctrl_sock = ftp_connect_control(servidorAtualFTPIndex);
    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", listaServidoresFTP[servidorAtualFTPIndex].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(listaServidoresFTP[servidorAtualFTPIndex].pass) > 0) sprintf(cmdPass, "PASS %s", listaServidoresFTP[servidorAtualFTPIndex].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com"); send_ftp_cmd(ctrl_sock, cmdPass, resp);
    send_ftp_cmd(ctrl_sock, "TYPE I", resp);

    char cmdSize[512]; sprintf(cmdSize, "SIZE %s", remotePath); send_ftp_cmd(ctrl_sock, cmdSize, resp);
    uint64_t totalSize = 0; if (strncmp(resp, "213", 3) == 0) sscanf(resp + 4, "%lu", &totalSize);

    int data_sock = ftp_enter_pasv(ctrl_sock);
    char cmdRetr[512]; sprintf(cmdRetr, "RETR %s", remotePath); send_ftp_cmd(ctrl_sock, cmdRetr, resp);

    FILE* f = fopen(localPath, "wb");
    if (f) {
        unsigned char buf[65536]; int n; uint64_t baixado = 0;
        while ((n = recv(data_sock, buf, sizeof(buf), 0)) > 0) {
            fwrite(buf, 1, n, f); baixado += n;
            if (totalSize > 0) { float prog = (float)baixado / (float)totalSize; sprintf(msgStatus, "ABRINDO: %d%%", (int)(prog * 100)); atualizarBarra(prog); }
        } fclose(f);
    } close(data_sock); send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);

    // O Módulo Identifica a Extensão e Abre a Tela Correta!
    char* ext = strrchr(nomeArquivo, '.');
    if (ext && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)) {
        if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; }
        int canais; imgMidia = stbi_load(localPath, &wM, &hM, &canais, 4);
        if (imgMidia) { visualizandoMidiaImagem = true; zoomMidia = 1.0f; fullscreenMidia = false; msgTimer = 0; }
        else { sprintf(msgStatus, "ERRO AO ABRIR IMAGEM!"); msgTimer = 120; }
    }
    else if (ext && (strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".ini") == 0 || strcasecmp(ext, ".xml") == 0)) {
        FILE* fTxt = fopen(localPath, "rb");
        if (fTxt) {
            fseek(fTxt, 0, SEEK_END); long sz = ftell(fTxt); fseek(fTxt, 0, SEEK_SET);
            if (textoMidiaBuffer) free(textoMidiaBuffer); textoMidiaBuffer = (char*)malloc(sz + 1);
            fread(textoMidiaBuffer, 1, sz, fTxt); textoMidiaBuffer[sz] = '\0'; fclose(fTxt);
            totalLinhasTexto = 0; char* line = strtok(textoMidiaBuffer, "\n");
            while (line && totalLinhasTexto < 5000) { linhasTexto[totalLinhasTexto++] = line; line = strtok(NULL, "\n"); }
            textoMidiaScroll = 0; visualizandoMidiaTexto = true; msgTimer = 0;
        }
    }
    free(arg); return NULL;
}

void prepararPreviewFTP(const char* remotePath) {
    sprintf(msgStatus, "PREPARANDO VISUALIZACAO..."); atualizarBarra(0.1f); msgTimer = 180;
    char* p = strdup(remotePath); pthread_t tId; pthread_create(&tId, NULL, threadPreviewFTP, p); pthread_detach(tId);
}

// ==============================================================
// NAVEGAÇÃO FTP COMUM (LIST / DOWNLOAD / UPLOAD)
// ==============================================================
void acessarFTP(int index, const char* path) {
    servidorAtualFTPIndex = index;
    sprintf(msgStatus, "CONECTANDO AO PC: %s:%d...", listaServidoresFTP[index].ip, listaServidoresFTP[index].port);
    atualizarBarra(0.1f);
    int ctrl_sock = ftp_connect_control(index);
    if (ctrl_sock < 0) { sprintf(msgStatus, "PC NAO ENCONTRADO!"); atualizarBarra(0.0f); msgTimer = 400; return; }

    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", listaServidoresFTP[index].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(listaServidoresFTP[index].pass) > 0) sprintf(cmdPass, "PASS %s", listaServidoresFTP[index].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com"); send_ftp_cmd(ctrl_sock, cmdPass, resp);

    char cmdCwd[1024]; sprintf(cmdCwd, "CWD %s", path); send_ftp_cmd(ctrl_sock, cmdCwd, resp);
    strcpy(currentFtpPath, path);

    int data_sock = ftp_enter_pasv(ctrl_sock);
    if (data_sock < 0) { sprintf(msgStatus, "ERRO PORTA PASV!"); close(ctrl_sock); atualizarBarra(0.0f); msgTimer = 240; return; }

    send_ftp_cmd(ctrl_sock, "LIST", resp);
    char* list_data = (char*)malloc(1024 * 1024); int list_len = 0; int n;
    while ((n = recv(data_sock, list_data + list_len, 65536, 0)) > 0) { list_len += n; if (list_len >= 1024 * 1024 - 65536) break; }
    list_data[list_len] = '\0'; close(data_sock); send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);

    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;
    char* line = strtok(list_data, "\r\n");
    while (line && totalItens < 2900) {
        bool isDir = false; char filename[256];
        if (parse_ftp_line(line, filename, &isDir)) {
            if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
                if (isDir) { sprintf(nomes[totalItens], "[%s]", filename); sprintf(linksAtuais[totalItens], "%s%s%s/", currentFtpPath, strcmp(currentFtpPath, "/") == 0 ? "" : "/", filename); }
                else { strcpy(nomes[totalItens], filename); sprintf(linksAtuais[totalItens], "%s%s%s", currentFtpPath, strcmp(currentFtpPath, "/") == 0 ? "" : "/", filename); }
                totalItens++;
            }
        } line = strtok(NULL, "\r\n");
    } free(list_data);
    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_FTP_LISTA; sel = 0; off = 0; sprintf(msgStatus, "PASTA DO COMPUTADOR CARREGADA!"); atualizarBarra(1.0f); msgTimer = 180;
}

void* threadDownloadFTP(void* arg) {
    char* remotePath = (char*)arg; char nomeArquivo[256];
    char* ref = strrchr(remotePath, '/'); if (ref) strcpy(nomeArquivo, ref + 1); else strcpy(nomeArquivo, remotePath);

    char localPath[512]; sceKernelMkdir("/data/HyperNeiva/baixado", 0777); sceKernelMkdir("/data/HyperNeiva/baixado/ftp", 0777);
    sprintf(localPath, "/data/HyperNeiva/baixado/ftp/%s", nomeArquivo);

    int ctrl_sock = ftp_connect_control(servidorAtualFTPIndex);
    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", listaServidoresFTP[servidorAtualFTPIndex].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(listaServidoresFTP[servidorAtualFTPIndex].pass) > 0) sprintf(cmdPass, "PASS %s", listaServidoresFTP[servidorAtualFTPIndex].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com"); send_ftp_cmd(ctrl_sock, cmdPass, resp);
    send_ftp_cmd(ctrl_sock, "TYPE I", resp);

    char cmdSize[512]; sprintf(cmdSize, "SIZE %s", remotePath); send_ftp_cmd(ctrl_sock, cmdSize, resp);
    uint64_t totalSize = 0; if (strncmp(resp, "213", 3) == 0) sscanf(resp + 4, "%lu", &totalSize);

    int data_sock = ftp_enter_pasv(ctrl_sock);
    char cmdRetr[512]; sprintf(cmdRetr, "RETR %s", remotePath); send_ftp_cmd(ctrl_sock, cmdRetr, resp);

    FILE* f = fopen(localPath, "wb");
    if (f) {
        unsigned char buf[65536]; int n; uint64_t baixado = 0;
        while ((n = recv(data_sock, buf, sizeof(buf), 0)) > 0) {
            fwrite(buf, 1, n, f); baixado += n;
            if (totalSize > 0) { float prog = (float)baixado / (float)totalSize; sprintf(msgStatus, "BAIXANDO DO PC: %d%%", (int)(prog * 100)); atualizarBarra(prog); }
        } fclose(f);
    } close(data_sock); send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);

    char* ext = strrchr(nomeArquivo, '.');
    if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) {
        sceKernelMkdir("/data/pkg", 0777); char destino[512]; sprintf(destino, "/data/pkg/%s", nomeArquivo);
        rename(localPath, destino); sprintf(msgStatus, "INSTALANDO NO PS4..."); msgTimer = 300; instalarPkgLocal(destino);
    }
    else { sprintf(msgStatus, "ARQUIVO BAIXADO COM SUCESSO!"); msgTimer = 240; }
    free(arg); return NULL;
}

void iniciarDownloadFTP(const char* remotePath) {
    sprintf(msgStatus, "INICIANDO DOWNLOAD VIA FTP..."); atualizarBarra(0.1f); msgTimer = 180;
    char* p = strdup(remotePath); pthread_t tId; pthread_create(&tId, NULL, threadDownloadFTP, p); pthread_detach(tId);
}

void preencherMenuFTPUploadRaizes() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva"); strcpy(nomes[1], "Raiz"); strcpy(nomes[2], "USB0"); strcpy(nomes[3], "USB1");
    totalItens = 4; menuAtual = MENU_BAIXAR_FTP_UPLOAD_RAIZES; sel = 0; off = 0;
}

void listarArquivosUploadFTP(const char* dirPath) {
    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;
    strcpy(currentUploadPath, dirPath); DIR* d = opendir(dirPath);
    if (d) { struct dirent* dir; while ((dir = readdir(d)) != NULL) { if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue; strcpy(nomes[totalItens], dir->d_name); sprintf(linksAtuais[totalItens], "%s/%s", dirPath, dir->d_name); if (dir->d_type == DT_DIR) { strcat(nomes[totalItens], " (Pasta)"); strcat(linksAtuais[totalItens], "/"); } totalItens++; if (totalItens >= 2900) break; } closedir(d); }
    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia / Acesso Negado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_FTP_UPLOAD; sel = 0; off = 0; sprintf(msgStatus, "SELECIONE UM ARQUIVO PARA ENVIAR AO PC");
}

void* threadUploadFTP(void* arg) {
    char* localPath = (char*)arg; char nomeArquivo[256];
    char* ref = strrchr(localPath, '/'); if (ref) strcpy(nomeArquivo, ref + 1); else strcpy(nomeArquivo, localPath);
    int ctrl_sock = ftp_connect_control(servidorAtualFTPIndex);
    if (ctrl_sock < 0) { sprintf(msgStatus, "PC NAO ENCONTRADO PARA UPLOAD!"); msgTimer = 240; free(arg); return NULL; }
    char resp[2048]; char cmdUser[128], cmdPass[128];
    sprintf(cmdUser, "USER %s", listaServidoresFTP[servidorAtualFTPIndex].user); send_ftp_cmd(ctrl_sock, cmdUser, resp);
    if (strlen(listaServidoresFTP[servidorAtualFTPIndex].pass) > 0) sprintf(cmdPass, "PASS %s", listaServidoresFTP[servidorAtualFTPIndex].pass);
    else sprintf(cmdPass, "PASS hyperneiva@ps4.com"); send_ftp_cmd(ctrl_sock, cmdPass, resp);
    send_ftp_cmd(ctrl_sock, "TYPE I", resp);
    char cmdCwd[1024]; sprintf(cmdCwd, "CWD %s", currentFtpPath); send_ftp_cmd(ctrl_sock, cmdCwd, resp);
    int data_sock = ftp_enter_pasv(ctrl_sock);
    char cmdStor[512]; sprintf(cmdStor, "STOR %s", nomeArquivo); send_ftp_cmd(ctrl_sock, cmdStor, resp);
    FILE* f = fopen(localPath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); uint64_t totalSize = ftell(f); fseek(f, 0, SEEK_SET); unsigned char buf[65536]; int n; uint64_t enviado = 0;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) { send(data_sock, buf, n, 0); enviado += n; if (totalSize > 0) { float prog = (float)enviado / (float)totalSize; sprintf(msgStatus, "ENVIANDO AO PC: %d%%", (int)(prog * 100)); atualizarBarra(prog); } } fclose(f);
    } close(data_sock); send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);
    sprintf(msgStatus, "ENVIO PARA O COMPUTADOR CONCLUIDO!"); msgTimer = 240; atualizarBarra(1.0f); free(arg); return NULL;
}

void fazerUploadFTP(const char* localPath) {
    sprintf(msgStatus, "PREPARANDO ENVIO AO PC..."); atualizarBarra(0.1f); msgTimer = 180;
    char* p = strdup(localPath); pthread_t tId; pthread_create(&tId, NULL, threadUploadFTP, p); pthread_detach(tId);
}