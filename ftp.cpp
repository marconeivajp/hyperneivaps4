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

// BIBLIOTECAS PARA O TECLADO VIRTUAL FUNCIONAR AQUI
#include "ImeDialog.h"
#include "CommonDialog.h"

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

char ftpConfigIp[64] = "192.168.0.5";
int ftpConfigPort = 21;
char currentFtpPath[1024] = "/";

bool ftpSelecionandoUpload = false;

// ==============================================================
// SISTEMA DE SERVIDORES E TECLADO VIRTUAL (IME)
// ==============================================================
void preencherMenuFTPServidores(bool isUpload) {
    ftpSelecionandoUpload = isUpload;
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);

    // Se o txt não existir, cria o pré-configurado do Marcao
    FILE* fCheck = fopen("/data/HyperNeiva/configuracao/ftp_servers.txt", "r");
    if (!fCheck) {
        FILE* fOut = fopen("/data/HyperNeiva/configuracao/ftp_servers.txt", "w");
        if (fOut) {
            fprintf(fOut, "PC do Marcao|192.168.0.5:21\n");
            fclose(fOut);
        }
    }
    else {
        fclose(fCheck);
    }

    memset(nomes, 0, sizeof(nomes));
    memset(linksAtuais, 0, sizeof(linksAtuais));

    strcpy(nomes[0], "[+] Adicionar Novo Servidor");
    totalItens = 1;

    FILE* f = fopen("/data/HyperNeiva/configuracao/ftp_servers.txt", "r");
    if (f) {
        char linha[256];
        while (fgets(linha, sizeof(linha), f) && totalItens < 2900) {
            linha[strcspn(linha, "\r\n")] = 0;
            if (strlen(linha) < 5) continue;

            char* sep = strchr(linha, '|');
            if (sep) {
                *sep = '\0';
                char* ipPort = sep + 1;
                sprintf(nomes[totalItens], "%s (%s)", linha, ipPort);
                strcpy(linksAtuais[totalItens], ipPort);
            }
            else {
                strcpy(nomes[totalItens], linha);
                strcpy(linksAtuais[totalItens], linha);
            }
            totalItens++;
        }
        fclose(f);
    }

    menuAtual = MENU_BAIXAR_FTP_SERVIDORES;
    sel = 0; off = 0;
}

void setFtpConfigFromLink(const char* ipPort) {
    char temp[128];
    strcpy(temp, ipPort);
    char* p = strchr(temp, ':');
    if (p) {
        *p = '\0';
        strcpy(ftpConfigIp, temp);
        ftpConfigPort = atoi(p + 1);
    }
    else {
        strcpy(ftpConfigIp, temp);
        ftpConfigPort = 21; // Padrão
    }
}

// A THREAD QUE FICA LENDO O TECLADO
volatile bool esperandoIPFTP = false;
wchar_t textoTecladoFTP[256] = L"";

void* threadPollerIME(void* arg) {
    while (esperandoIPFTP) {
        int status = (int)sceImeDialogGetStatus();
        if (status != 1 && status != 0) {
            OrbisDialogResult res;
            memset(&res, 0, sizeof(res));
            sceImeDialogGetResult(&res);

            int32_t buttonId = *(int32_t*)&res;

            if (buttonId == 0) { // Botão OK clicado
                char ipFinal[256];
                memset(ipFinal, 0, sizeof(ipFinal));
                uint16_t* bufRead = (uint16_t*)textoTecladoFTP;
                int len = 0;
                for (int i = 0; i < 255; i++) {
                    if (bufRead[i] == 0x0000) break;
                    ipFinal[len++] = (char)bufRead[i];
                }
                ipFinal[len] = '\0';

                if (strlen(ipFinal) > 5) {
                    FILE* f = fopen("/data/HyperNeiva/configuracao/ftp_servers.txt", "a");
                    if (f) {
                        fprintf(f, "Servidor Custom|%s\n", ipFinal);
                        fclose(f);
                    }
                    // Atualiza a tela automaticamente
                    preencherMenuFTPServidores(ftpSelecionandoUpload);
                    sprintf(msgStatus, "NOVO SERVIDOR SALVO COM SUCESSO!");
                    msgTimer = 180;
                }
            }
            sceImeDialogTerm();
            esperandoIPFTP = false;
            break;
        }
        sceKernelUsleep(100 * 1000);
    }
    return NULL;
}

void abrirTecladoNovoFTP(int32_t uId) {
    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    memset(textoTecladoFTP, 0, sizeof(textoTecladoFTP));

    // Pré-preenche para o usuário saber o formato
    const char* ph = "192.168.0.5:21";
    uint16_t* wPtr = (uint16_t*)textoTecladoFTP;
    for (int i = 0; ph[i]; i++) wPtr[i] = ph[i];

    param.userId = uId;
    param.maxTextLength = 255;
    param.inputTextBuffer = textoTecladoFTP;
    param.title = L"Digite o IP e a Porta do PC";
    param.type = (OrbisImeType)0;

    if (sceImeDialogInit(&param, NULL) >= 0) {
        esperandoIPFTP = true;
        pthread_t t;
        pthread_create(&t, NULL, threadPollerIME, NULL);
        pthread_detach(t);
    }
}

// ==============================================================
// FUNÇÕES BASE DO PROTOCOLO FTP (SOCKETS NATIVOS)
// ==============================================================
int send_ftp_cmd(int sock, const char* cmd, char* response) {
    if (cmd) {
        send(sock, cmd, strlen(cmd), 0);
        send(sock, "\r\n", 2, 0);
    }
    memset(response, 0, 2048);
    int total = 0;
    while (total < 2047) {
        char line[512]; memset(line, 0, 512); int idx = 0;
        while (idx < 511) {
            char c; int n = recv(sock, &c, 1, 0);
            if (n <= 0) break; line[idx++] = c; if (c == '\n') break;
        }
        strcat(response, line); total += idx;
        if (idx >= 4 && line[3] == ' ' && isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2])) break;
    }
    return total;
}

int ftp_connect_control() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ftpConfigPort);
    inet_pton(AF_INET, ftpConfigIp, &addr.sin_addr);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (res < 0) {
        if (errno == EINPROGRESS) {
            fd_set set; FD_ZERO(&set); FD_SET(sock, &set);
            struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
            res = select(sock + 1, NULL, &set, NULL, &tv);
            if (res <= 0) { close(sock); return -1; }
            int error = 0; socklen_t len = sizeof(error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
            if (error != 0) { close(sock); return -1; }
        }
        else { close(sock); return -1; }
    }
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);

    struct timeval tv2; tv2.tv_sec = 5; tv2.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv2, sizeof tv2);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv2, sizeof tv2);

    char resp[2048];
    send_ftp_cmd(sock, NULL, resp);
    return sock;
}

int ftp_enter_pasv(int ctrl_sock) {
    char resp[2048];
    send_ftp_cmd(ctrl_sock, "PASV", resp);

    int h1, h2, h3, h4, p1, p2;
    char* start = strchr(resp, '(');
    if (!start) return -1;

    sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    int data_port = (p1 << 8) | p2;
    char ip[32]; sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);

    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(data_port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
    setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    if (connect(data_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(data_sock); return -1;
    }
    return data_sock;
}

bool parse_ftp_line(char* line, char* nameOut, bool* isDirOut) {
    *isDirOut = false; nameOut[0] = '\0';
    if (line[0] == 'd' || line[0] == '-') {
        *isDirOut = (line[0] == 'd');
        char* p = line; int spaces = 0;
        while (*p && spaces < 8) { if (*p == ' ') { spaces++; while (*(p + 1) == ' ') p++; } p++; }
        if (*p) { strcpy(nameOut, p); return true; }
    }
    else if (isdigit(line[0])) {
        if (strstr(line, "<DIR>")) *isDirOut = true;
        char* p = line; int spaces = 0;
        while (*p && spaces < 3) { if (*p == ' ') { spaces++; while (*(p + 1) == ' ') p++; } p++; }
        if (*p) { strcpy(nameOut, p); return true; }
    }
    return false;
}

// ==============================================================
// EXPLORADOR E NAVEGAÇÃO
// ==============================================================
void acessarFTP(const char* path) {
    sprintf(msgStatus, "CONECTANDO AO PC EM %s:%d...", ftpConfigIp, ftpConfigPort);
    atualizarBarra(0.1f);

    int ctrl_sock = ftp_connect_control();
    if (ctrl_sock < 0) {
        sprintf(msgStatus, "PC NAO ENCONTRADO! Verifique o IP ou o FileZilla.");
        atualizarBarra(0.0f); msgTimer = 400; return;
    }

    char resp[2048];
    send_ftp_cmd(ctrl_sock, "USER anonymous", resp);
    send_ftp_cmd(ctrl_sock, "PASS hyperneiva@ps4.com", resp);

    char cmdCwd[1024]; sprintf(cmdCwd, "CWD %s", path);
    send_ftp_cmd(ctrl_sock, cmdCwd, resp);
    strcpy(currentFtpPath, path);

    int data_sock = ftp_enter_pasv(ctrl_sock);
    if (data_sock < 0) {
        sprintf(msgStatus, "ERRO AO ABRIR PORTA DE DADOS (PASV)!");
        close(ctrl_sock); atualizarBarra(0.0f); msgTimer = 240; return;
    }

    send_ftp_cmd(ctrl_sock, "LIST", resp);

    char* list_data = (char*)malloc(1024 * 1024);
    int list_len = 0; int n;
    while ((n = recv(data_sock, list_data + list_len, 65536, 0)) > 0) {
        list_len += n; if (list_len >= 1024 * 1024 - 65536) break;
    }
    list_data[list_len] = '\0'; close(data_sock);
    send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);

    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;

    char* line = strtok(list_data, "\r\n");
    while (line && totalItens < 2900) {
        bool isDir = false; char filename[256];
        if (parse_ftp_line(line, filename, &isDir)) {
            if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
                if (isDir) {
                    sprintf(nomes[totalItens], "[%s]", filename);
                    sprintf(linksAtuais[totalItens], "%s%s%s/", currentFtpPath, strcmp(currentFtpPath, "/") == 0 ? "" : "/", filename);
                }
                else {
                    strcpy(nomes[totalItens], filename);
                    sprintf(linksAtuais[totalItens], "%s%s%s", currentFtpPath, strcmp(currentFtpPath, "/") == 0 ? "" : "/", filename);
                }
                totalItens++;
            }
        }
        line = strtok(NULL, "\r\n");
    }
    free(list_data);

    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia"); totalItens = 1; }

    menuAtual = MENU_BAIXAR_FTP_LISTA;
    sel = 0; off = 0;
    sprintf(msgStatus, "PASTA DO COMPUTADOR CARREGADA!");
    atualizarBarra(1.0f); msgTimer = 180;
}

// ==============================================================
// DOWNLOAD (COM INSTALAÇÃO DE PKG NATIVA!)
// ==============================================================
void* threadDownloadFTP(void* arg) {
    char* remotePath = (char*)arg;
    char nomeArquivo[256];
    char* ref = strrchr(remotePath, '/');
    if (ref) strcpy(nomeArquivo, ref + 1); else strcpy(nomeArquivo, remotePath);

    char localPath[512];
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/ftp", 0777);
    sprintf(localPath, "/data/HyperNeiva/baixado/ftp/%s", nomeArquivo);

    int ctrl_sock = ftp_connect_control();
    char resp[2048];
    send_ftp_cmd(ctrl_sock, "USER anonymous", resp);
    send_ftp_cmd(ctrl_sock, "PASS anon@anon", resp);
    send_ftp_cmd(ctrl_sock, "TYPE I", resp);

    char cmdSize[512]; sprintf(cmdSize, "SIZE %s", remotePath);
    send_ftp_cmd(ctrl_sock, cmdSize, resp);
    uint64_t totalSize = 0;
    if (strncmp(resp, "213", 3) == 0) sscanf(resp + 4, "%lu", &totalSize);

    int data_sock = ftp_enter_pasv(ctrl_sock);
    char cmdRetr[512]; sprintf(cmdRetr, "RETR %s", remotePath);
    send_ftp_cmd(ctrl_sock, cmdRetr, resp);

    FILE* f = fopen(localPath, "wb");
    if (f) {
        unsigned char buf[65536]; int n; uint64_t baixado = 0;
        while ((n = recv(data_sock, buf, sizeof(buf), 0)) > 0) {
            fwrite(buf, 1, n, f); baixado += n;
            if (totalSize > 0) {
                float prog = (float)baixado / (float)totalSize;
                sprintf(msgStatus, "BAIXANDO DO PC: %d%%", (int)(prog * 100));
                atualizarBarra(prog);
            }
            else {
                sprintf(msgStatus, "BAIXANDO DO PC: %.2f MB", (float)baixado / 1048576.0f);
            }
            sceKernelUsleep(1000);
        }
        fclose(f);
    }
    close(data_sock); send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);

    char* ext = strrchr(nomeArquivo, '.');
    if (ext && (strcasecmp(ext, ".pkg") == 0 || strcasecmp(ext, ".PKG") == 0)) {
        sceKernelMkdir("/data/pkg", 0777);
        char destino[512]; sprintf(destino, "/data/pkg/%s", nomeArquivo);
        rename(localPath, destino);
        sprintf(msgStatus, "DOWNLOAD DO PC CONCLUIDO! INSTALANDO NO PS4...");
        msgTimer = 300;
        instalarPkgLocal(destino);
    }
    else {
        sprintf(msgStatus, "ARQUIVO BAIXADO COM SUCESSO!");
        msgTimer = 240;
    }
    free(arg); return NULL;
}

void iniciarDownloadFTP(const char* remotePath) {
    sprintf(msgStatus, "INICIANDO DOWNLOAD VIA FTP...");
    atualizarBarra(0.1f); msgTimer = 180;
    char* p = strdup(remotePath);
    pthread_t tId; pthread_create(&tId, NULL, threadDownloadFTP, p); pthread_detach(tId);
}

// ==============================================================
// UPLOAD PARA O COMPUTADOR
// ==============================================================
void preencherMenuFTPUploadRaizes() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva");
    strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB0");
    strcpy(nomes[3], "USB1");
    totalItens = 4;
    menuAtual = MENU_BAIXAR_FTP_UPLOAD_RAIZES;
    sel = 0; off = 0;
}

void listarArquivosUploadFTP(const char* dirPath) {
    memset(nomes, 0, sizeof(nomes)); memset(linksAtuais, 0, sizeof(linksAtuais)); memset(marcados, 0, sizeof(marcados)); totalItens = 0;
    strcpy(currentUploadPath, dirPath);

    DIR* d = opendir(dirPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
            strcpy(nomes[totalItens], dir->d_name); sprintf(linksAtuais[totalItens], "%s/%s", dirPath, dir->d_name);
            if (dir->d_type == DT_DIR) { strcat(nomes[totalItens], " (Pasta)"); strcat(linksAtuais[totalItens], "/"); }
            totalItens++; if (totalItens >= 2900) break;
        } closedir(d);
    }
    if (totalItens == 0) { strcpy(nomes[0], "Pasta Vazia / Acesso Negado"); totalItens = 1; }
    menuAtual = MENU_BAIXAR_FTP_UPLOAD;
    sel = 0; off = 0;
    sprintf(msgStatus, "SELECIONE UM ARQUIVO PARA ENVIAR AO PC");
}

void* threadUploadFTP(void* arg) {
    char* localPath = (char*)arg;
    char nomeArquivo[256];
    char* ref = strrchr(localPath, '/');
    if (ref) strcpy(nomeArquivo, ref + 1); else strcpy(nomeArquivo, localPath);

    int ctrl_sock = ftp_connect_control();
    if (ctrl_sock < 0) {
        sprintf(msgStatus, "PC NAO ENCONTRADO PARA UPLOAD!"); msgTimer = 240;
        free(arg); return NULL;
    }

    char resp[2048];
    send_ftp_cmd(ctrl_sock, "USER anonymous", resp);
    send_ftp_cmd(ctrl_sock, "PASS anon@anon", resp);
    send_ftp_cmd(ctrl_sock, "TYPE I", resp);

    char cmdCwd[1024]; sprintf(cmdCwd, "CWD %s", currentFtpPath);
    send_ftp_cmd(ctrl_sock, cmdCwd, resp);

    int data_sock = ftp_enter_pasv(ctrl_sock);

    char cmdStor[512]; sprintf(cmdStor, "STOR %s", nomeArquivo);
    send_ftp_cmd(ctrl_sock, cmdStor, resp);

    FILE* f = fopen(localPath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); uint64_t totalSize = ftell(f); fseek(f, 0, SEEK_SET);

        unsigned char buf[65536]; int n; uint64_t enviado = 0;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            send(data_sock, buf, n, 0);
            enviado += n;
            if (totalSize > 0) {
                float prog = (float)enviado / (float)totalSize;
                sprintf(msgStatus, "ENVIANDO AO PC: %d%%", (int)(prog * 100));
                atualizarBarra(prog);
            }
        }
        fclose(f);
    }
    close(data_sock); send_ftp_cmd(ctrl_sock, NULL, resp); close(ctrl_sock);

    sprintf(msgStatus, "ENVIO PARA O COMPUTADOR CONCLUIDO!");
    msgTimer = 240; atualizarBarra(1.0f);
    free(arg); return NULL;
}

void fazerUploadFTP(const char* localPath) {
    sprintf(msgStatus, "PREPARANDO ENVIO AO PC...");
    atualizarBarra(0.1f); msgTimer = 180;
    char* p = strdup(localPath);
    pthread_t tId; pthread_create(&tId, NULL, threadUploadFTP, p); pthread_detach(tId);
}