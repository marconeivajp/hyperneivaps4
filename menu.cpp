#include <strings.h>
#include <stdlib.h>
#include <orbis/libkernel.h>
#include "menu.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>

MenuLevel menuAtual = ROOT;
char nomes[3000][256];
int totalItens = 0;
int sel = 0;
int off = 0;
int offEsq = 0;

char msgStatus[128] = "SISTEMA PRONTO";
int msgTimer = 0;

char caminhoMidiaAtual[512] = "/data/HyperNeiva/midia";
char caminhoROMsAtual[512] = ""; // Novo: rastreia a pasta de ROMs ativa

void preencherRoot() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "JOGAR"); // <--- Agora agrupa PS4 Nativo e XML
    strcpy(nomes[1], "MIDIA");
    strcpy(nomes[2], "BAIXAR");
    strcpy(nomes[3], "EDITAR");
    strcpy(nomes[4], "EXPLORAR");
    strcpy(nomes[5], "EXTRA");
    strcpy(nomes[6], "INFORMACOES");
    strcpy(nomes[7], "CONQUISTAS");
    strcpy(nomes[8], "ESCRITORIO 3D");
    totalItens = 9;
    menuAtual = ROOT;
}

void preencherMenuJogar() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "PS4 NATIVO");
    strcpy(nomes[1], "JOGOS XML");
    totalItens = 2;
    menuAtual = MENU_TIPO_JOGO;
}

void preencherExplorerHome() {
    memset(nomes, 0, sizeof(nomes));
    strcpy(nomes[0], "Hyper Neiva");
    strcpy(nomes[1], "Raiz");
    strcpy(nomes[2], "USB0");
    strcpy(nomes[3], "USB1");
    totalItens = 4;
    menuAtual = MENU_EXPLORAR_HOME;
}

bool ehArquivoVideo(const char* nome) {
    static const char* tipos[] = { ".mp4", ".m4v", ".mkv", ".avi", ".mov", ".webm", ".ts", ".flv", ".wmv", ".mpg", ".mpeg", ".3gp",
        ".m2ts", ".mts", ".vob", ".ogv", ".asf", ".f4v", ".3g2", ".divx", ".mpe", ".m2v" };
    const char* ext = strrchr(nome, '.');
    if (!ext) return false;
    for (unsigned int i = 0; i < sizeof(tipos) / sizeof(tipos[0]); i++) {
        if (strcasecmp(ext, tipos[i]) == 0) return true;
    }
    return false;
}

void abrirPastaMidia(const char* caminho) {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    strcpy(caminhoMidiaAtual, caminho);

    DIR* d = opendir(caminhoMidiaAtual);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && totalItens < 2999) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                strcpy(nomes[totalItens], dir->d_name);
                totalItens++;
            }
        }
        closedir(d);
    }

    // Radio & Podcast é uma entrada só da raiz de Midia.
    if (strcmp(caminhoMidiaAtual, "/data/HyperNeiva/midia") == 0) {
        for (int i = totalItens; i > 0; i--) {
            strcpy(nomes[i], nomes[i-1]);
        }
        strcpy(nomes[0], "Radio & Podcast");
        totalItens++;
    }
    else if (totalItens == 0) {
        strcpy(nomes[0], "Pasta vazia");
        totalItens = 1;
    }

    menuAtual = MENU_MIDIA;
    sel = 0;
    off = 0;
}

void preencherMenuGBA() {
    // A lista agora vem do sistema escolhido; o nome ficou por compatibilidade.
    extern void bibliotecaListarJogos();
    bibliotecaListarJogos();
    if (false) {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    // Aceita a pasta com o nome certo ou como ela já existe no console.
    const char* romPath = "/data/retroarch/0 Games/Game Boy Advance";
    DIR* d = opendir(romPath);
    if (!d) {
        romPath = "/data/retroarch/0 Games/Game Boy Advace";
        d = opendir(romPath);
    }
    strcpy(caminhoROMsAtual, romPath); // Registra que estamos no GBA
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && totalItens < 3000) {
            if (dir->d_type == DT_REG) {
                const char* ext = strrchr(dir->d_name, '.');
                if (ext && (strcasecmp(ext, ".gba") == 0 || strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".7z") == 0)) {
                    strncpy(nomes[totalItens], dir->d_name, 255);
                    nomes[totalItens][255] = '\0';
                    totalItens++;
                }
            }
        }
        closedir(d);
        qsort(nomes, totalItens, sizeof(nomes[0]), (int (*)(const void*, const void*))strcasecmp);
    }
    }

    extern void bibliotecaAplicar();
    bibliotecaAplicar();

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhuma ROM encontrada");
        totalItens = 1;
    }

    menuAtual = MENU_EMULADOR; // Reaproveita o menu de lista de ROMs
    sel = 0;
    off = 0;
}

void preencherMenuMidia() {
    abrirPastaMidia("/data/HyperNeiva/midia");
}

void preencherMenuEmulador() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;

    const char* romPath = "/data/retroarch/Games/roms Genesis";
    strcpy(caminhoROMsAtual, romPath); // Registra que estamos no Genesis
    DIR* d = opendir(romPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && totalItens < 3000) {
            if (dir->d_type == DT_REG) {
                const char* ext = strrchr(dir->d_name, '.');
                if (ext && (strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".bin") == 0 || strcasecmp(ext, ".gen") == 0 || strcasecmp(ext, ".smd") == 0 || strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".rar") == 0 || strcasecmp(ext, ".7z") == 0)) {
                    strncpy(nomes[totalItens], dir->d_name, 255);
                    nomes[totalItens][63] = '\0';
                    totalItens++;
                }
            }
        }
        closedir(d);
    }

    if (totalItens == 0) {
        strcpy(nomes[0], "Nenhuma ROM encontrada");
        totalItens = 1;
    }

    menuAtual = MENU_EMULADOR;
    sel = 0;
    off = 0;
}

// ===== CONQUISTAS =====
// Tutorial do app: cada conquista ensina uma função e é ganha ao usá-la de verdade.
#include <sys/stat.h>

extern int medirLarguraTexto(const char* texto, int tam);
extern void desenharTexto(uint32_t* pixels, const char* texto, int tam, int x, int y, uint32_t cor);

struct Conquista { const char* chave; int topico; const char* nome; const char* como; };

static const char* topicosConquista[] = { "JOGAR", "MIDIA", "BAIXAR", "EDITAR", "EXPLORAR", "EXTRA", "INFORMACOES" };
static const char* resumoTopico[] = {
    "Jogos do PS4, jogos em XML e a biblioteca do emulador.",
    "Vídeos, músicas, imagens e rádio.",
    "FTP, Dropbox, GitHub, torrent e outras formas de trazer arquivos para o PS4.",
    "Tema da interface, música de fundo e ajustes da tela.",
    "Os arquivos do console: copiar, extrair e instalar.",
    "Telegram, teste de controle e piano.",
    "Dados do app e do console.",
};
#define TOTAL_TOPICOS 7

static const Conquista conquistas[] = {
    { "jogar", 0, "Hora de jogar", "No menu inicial, escolha JOGAR e aperte X." },
    { "ps4", 0, "Jogos do console", "Em JOGAR, escolha PS4 NATIVO. A lista mostra os jogos instalados no PS4." },
    { "xml", 0, "Jogos em lista", "Em JOGAR, escolha JOGOS XML." },
    { "biblioteca", 0, "Minha biblioteca", "Em JOGOS XML, aperte Quadrado, escolha NINTENDO GAME BOY ADVANCE e aperte X. As ROMs ficam em /data/retroarch/0 Games/Game Boy Advance." },
    { "partida", 0, "Primeira partida", "Na biblioteca, aperte X em um jogo. Para abrir o menu dentro do jogo, use L1 + R1 + Options + Touchpad." },
    { "favorito", 0, "Meu preferido", "Na biblioteca, aperte L2 em um jogo. L1 ou R1 mostra só os favoritos. Triângulo abre as outras opções do jogo." },

    { "midia", 1, "Central de mídia", "No menu inicial, escolha MIDIA e aperte X." },
    { "video", 1, "Sessão de cinema", "Aperte X em um vídeo em MIDIA ou no EXPLORAR. Setas mostram a barra do player." },
    { "pip", 1, "Tela pequena", "Com um vídeo aberto, desça até os ícones do player e escolha a tela pequena. O vídeo continua enquanto você usa o app." },
    { "musica", 1, "Minhas músicas", "Em MIDIA, abra a pasta de músicas. Os arquivos ficam em /data/HyperNeiva/Musicas." },
    { "radio", 1, "No ar", "Em MIDIA, abra Radio & Podcast." },
    { "radio_player", 1, "Locutor", "Toque uma rádio com X. Triângulo abre o player com a capa da estação; nos três pontinhos dá para favoritar, trocar o tema da lista e abrir as opções antigas. L1 e R1 trocam de estação." },

    { "baixar", 2, "Central de downloads", "No menu inicial, escolha BAIXAR e aperte X." },
    { "ftp", 2, "FTP: conhecendo", "Em BAIXAR, escolha FTP (PC / Android). Download traz arquivos do PC ou do celular para o PS4. Upload leva do PS4 para lá. O PC ou celular precisa de um programa servidor de FTP ligado, na mesma rede do PS4. O IP do próprio PS4 aparece embaixo, à direita, em BAIXAR." },
    { "ftp_servidor", 2, "FTP: cadastrar servidor", "Ver o IP: no Windows, abra o Prompt de Comando, digite ipconfig e copie o Endereço IPv4, como 192.168.1.10. No celular, o app de FTP mostra o IP. Porta: use a que aparece no seu servidor, normalmente 21 no PC e 2121 no celular. Usuario e Senha: os mesmos criados no servidor. Depois, em Download, escolha [+] Adicionar Novo Servidor, preencha cada campo e escolha [ SALVAR SERVIDOR ]." },
    { "ftp_lista", 2, "FTP: conectado", "Em FTP, escolha Download e aperte X no servidor salvo. A lista de arquivos do seu PC ou celular aparece na tela." },
    { "dropbox_app", 2, "Dropbox: criar o app no PC", "No PC, abra dropbox.com/developers/apps e clique em Create app. Escolha Scoped access e Full Dropbox e dê um nome. Na aba Permissions, marque files.content.read e files.content.write e clique em Submit. Na aba Settings, copie o App key e o App secret." },
    { "dropbox_chave", 2, "Dropbox: token que não expira", "Jeito fácil: no PC, rode HyperNeivaPC.exe --token dropbox, cole App key e App secret, permita no navegador e cole o código. Ele gera o refresh token e grava sozinho no PS4. À mão: abra dropbox.com/oauth2/authorize?client_id=APPKEY&token_access_type=offline&response_type=code, troque o código pelo refresh token e preencha Extra > Chaves de API > Dropbox." },
    { "dropbox", 2, "Dropbox: abrir", "Em BAIXAR, escolha Dropbox. Download traz arquivos, Upload envia, e Menu Backup guarda seus dados." },
    { "dropbox_lista", 2, "Dropbox: conectado", "Depois de configurar, escolha Dropbox e Download. Seus arquivos da nuvem aparecem na lista." },
    { "github", 2, "Direto do GitHub", "Em BAIXAR, escolha GitHub para ver repositórios e baixar arquivos das releases." },
    { "torrent", 2, "Torrent: abrir", "Em BAIXAR, escolha Torrent. A lista mostra progresso, velocidade e fontes." },
    { "gerenciador", 2, "Tudo num lugar só", "Em BAIXAR, abra Gerenciador de downloads. Ele mostra o que está chegando do Telegram, GitHub, Dropbox, Google Drive, torrent, links, PC e dos downloads do PS4." },
    { "gdrive_app", 2, "Google Drive: credenciais", "No PC, em console.cloud.google.com, crie um projeto e ative a Google Drive API. Em Tela de consentimento OAuth, preencha o nome, adicione seu e-mail como usuário de teste e depois publique o app (em produção), senão o token vence em 7 dias. Em Credenciais, crie um ID do cliente OAuth do tipo Aplicativo da Web com o URI de redirecionamento https://developers.google.com/oauthplayground." },
    { "gdrive_chave", 2, "Google Drive: OAuth Playground", "Abra developers.google.com/oauthplayground, clique na engrenagem e marque Use your own OAuth credentials com seu ID e chave secreta. No passo 1, digite https://www.googleapis.com/auth/drive e clique em Authorize APIs. No passo 2, Exchange authorization code for tokens e copie o Refresh token. Preencha Extra > Chaves de API > Google Drive. Ou rode HyperNeivaPC.exe --token google (cliente do tipo App para computador)." },
    { "gdrive", 2, "Google Drive: abrir", "Em BAIXAR, escolha Google Drive. X abre pastas e baixa arquivos para /data/HyperNeiva/baixado/gdrive. Triângulo atualiza." },
    { "torrent_novo", 2, "Torrent: primeiro download", "Aperte X em um arquivo .torrent no EXPLORAR, ou escolha ADICIONAR LINK MAGNET em Baixar > Torrent. Triângulo remove da lista." },

    { "editar", 3, "Do seu jeito", "No menu inicial, escolha EDITAR e aperte X." },
    { "tema", 3, "Cara nova", "Em EDITAR, abra TEMA DA INTERFACE e aperte X em um tema." },
    { "musica_fundo", 3, "Trilha sonora", "Em EDITAR, abra MUSICA DE FUNDO para ligar, desligar, ajustar o volume e escolher outra música." },

    { "explorar", 4, "Explorador", "No menu inicial, escolha EXPLORAR. Hyper Neiva abre a pasta do app, Raiz abre o console inteiro, USB abre o pendrive." },
    { "opcoes", 4, "Opções do arquivo", "No EXPLORAR, aperte Triângulo em um arquivo para copiar, recortar, apagar e mais." },
    { "painel", 4, "Dois painéis", "No EXPLORAR, aperte L2 para abrir dois painéis lado a lado." },
    { "colar", 4, "Copiar e colar", "Copie um arquivo pelo Triângulo, vá até outra pasta e cole. A cópia roda na fila, com barra de progresso." },
    { "extrair", 4, "Descompactar", "No Triângulo de um .zip, .7z ou .rar, escolha extrair. O arquivo entra na fila." },
    { "pkg", 4, "Instalador", "Aperte X em um arquivo .pkg no EXPLORAR. Os PKGs ficam em /data/pkg." },
    { "modelo3d", 4, "Em três dimensões", "No EXPLORAR, aperte X em um arquivo .obj ou .stl. O analógico gira, L1 e R1 dão zoom e X mostra a malha de arame." },
    { "avatar", 4, "Foto de perfil", "No EXPLORAR, aperte X em uma imagem .png ou .jpg e escolha definir imagem de perfil. A foto entra no seu usuário do PS4." },
    { "fundo_ps4", 4, "Fundo do PS4", "No EXPLORAR, aperte X em uma imagem e escolha plano de fundo do ps4." },
    { "fundo_hn", 4, "Fundo do Hyper Neiva", "No EXPLORAR, aperte X em uma imagem e escolha plano de fundo hyper neiva. Vale para o tema padrão: os outros temas têm fundo próprio." },
    { "xavatar", 4, "Avatar pronto", "Um .xavatar é um pacote pronto de foto de perfil. Para baixar, entre em BAIXAR > Repositorios > Imagens para perfil e escolha xavatar lapys. Depois, no EXPLORAR, extraia o .zip baixado, aperte X em um arquivo .xavatar e escolha definir imagem de perfil." },

    { "extra", 5, "Mais recursos", "No menu inicial, escolha EXTRA e aperte X." },
    { "controle", 5, "Controle em dia", "Em EXTRA, abra o Teste de Controle para ver os botões e o desvio dos analógicos." },
    { "piano", 5, "Músico", "Em EXTRA, abra o Piano Virtual." },
    { "ventoinha", 5, "Ventoinha", "Em EXTRA, abra Ventoinha e temperatura. Esquerda e direita escolhem a temperatura em que a ventoinha acelera, e X aplica. Valor baixo deixa o PS4 mais frio e mais barulhento." },
    { "editor", 5, "Artista", "Em EXTRA, abra Editor de imagem, ou aperte X numa imagem no EXPLORAR e escolha editar imagem. L1 e R1 trocam a ferramenta, o touchpad desenha e Options abre o menu de salvar." },
    { "tempo", 5, "Meteorologista", "Em EXTRA, abra Previsão do tempo. Quadrado adiciona uma cidade pelo nome, Triângulo tira, cima e baixo trocam de cidade." },
    { "tg_chave", 5, "Telegram: criar a chave", "No PC, entre em my.telegram.org, abra API development tools e crie um app. Grave /data/HyperNeivaTelegram/api.txt no PS4 pelo FTP: api_id na primeira linha e api_hash na segunda." },
    { "tg_abrir", 5, "Telegram: abrir", "Em EXTRA, escolha Telegram. Sem o arquivo api.txt, o app avisa que falta a chave." },
    { "tg_login", 5, "Telegram: entrar", "Aperte X, digite o telefone com DDI, como +5511999998888, e depois o código que chega no seu Telegram." },

    { "info", 6, "Conheça o app", "No menu inicial, escolha INFORMACOES." },
};
#define TOTAL_CONQUISTAS ((int)(sizeof(conquistas) / sizeof(conquistas[0])))
#define ARQUIVO_CONQUISTAS "/data/HyperNeiva/configuracao/conquistas.txt"

static bool conquistaFeita[TOTAL_CONQUISTAS];
static volatile bool conquistaPendente[TOTAL_CONQUISTAS];
static bool conquistasLidas = false;
static int conquistaNivel = 0, conquistaTopico = 0, conquistaVersao = 0;
static int conquistaIndice[TOTAL_CONQUISTAS];

static int acharConquista(const char* chave) {
    for (int i = 0; i < TOTAL_CONQUISTAS; i++) if (strcmp(conquistas[i].chave, chave) == 0) return i;
    return -1;
}

static void lerConquistas() {
    conquistasLidas = true;
    FILE* f = fopen(ARQUIVO_CONQUISTAS, "r");
    if (!f) return;
    char linha[64];
    while (fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\r\n")] = '\0';
        int i = acharConquista(linha);
        if (i >= 0) conquistaFeita[i] = true;
    }
    fclose(f);
}

static void gravarConquistas() {
    const char* temporario = ARQUIVO_CONQUISTAS ".tmp";
    FILE* f = fopen(temporario, "w");
    if (!f) return;
    for (int i = 0; i < TOTAL_CONQUISTAS; i++) if (conquistaFeita[i]) fprintf(f, "%s\n", conquistas[i].chave);
    fclose(f);
    rename(temporario, ARQUIVO_CONQUISTAS);
}

// Pode vir de outra thread: só marca, e o laço principal registra e avisa.
void conquistaGanhar(const char* chave) {
    int i = acharConquista(chave);
    if (i >= 0) conquistaPendente[i] = true;
}

static bool conquistaArquivoExiste(const char* caminho) {
    struct stat info;
    return stat(caminho, &info) == 0 && info.st_size > 0;
}

// Roda a cada quadro, inclusive com vídeo aberto.
void conquistasAtualizar() {
    extern bool videoRodando, video_minimizado, emuladorAtivo, emSubmenuFTP, emSubmenuDropbox, showOpcoes, painelDuplo;
    extern int editTarget;
    if (!conquistasLidas) lerConquistas();

    static const struct { MenuLevel menu; const char* chave; } pelaTela[] = {
        { MENU_TIPO_JOGO, "jogar" }, { MENU_JOGAR_PS4, "ps4" }, { JOGAR_XML, "xml" }, { MENU_EMULADOR, "biblioteca" },
        { MENU_MIDIA, "midia" }, { MENU_MUSICAS, "musica" }, { MENU_RADIO_CATEGORIA, "radio" },
        { MENU_BAIXAR, "baixar" }, { MENU_BAIXAR_FTP_EDITAR_SERVIDOR, "ftp_servidor" }, { MENU_BAIXAR_FTP_LISTA, "ftp_lista" },
        { MENU_BAIXAR_DROPBOX_LISTA, "dropbox_lista" }, { MENU_BAIXAR_GITHUB, "github" }, { MENU_BAIXAR_TORRENT, "torrent" },
        { MENU_EDITAR, "editar" }, { MENU_EXPLORAR, "explorar" }, { MENU_EXTRA, "extra" }, { MENU_CONTROLE_TESTE, "controle" },
        { MENU_INSTRUMENTOS, "piano" }, { MENU_TELEGRAM, "tg_abrir" }, { MENU_INFORMACAO, "info" }, { MENU_VENTOINHA, "ventoinha" },
    };
    for (unsigned int i = 0; i < sizeof(pelaTela) / sizeof(pelaTela[0]); i++) if (menuAtual == pelaTela[i].menu) conquistaGanhar(pelaTela[i].chave);
    if (emuladorAtivo) conquistaGanhar("partida");
    if (videoRodando) conquistaGanhar("video");
    if (videoRodando && video_minimizado) conquistaGanhar("pip");
    if (emSubmenuFTP) conquistaGanhar("ftp");
    if (emSubmenuDropbox) conquistaGanhar("dropbox");
    if (menuAtual == MENU_EDIT_TARGET && editTarget == 30) conquistaGanhar("musica_fundo");
    if (menuAtual == MENU_EXPLORAR && showOpcoes) conquistaGanhar("opcoes");
    if (painelDuplo) conquistaGanhar("painel");

    static int espera = 0, arquivos = 0;
    if (++arquivos >= 600) {
        arquivos = 0;
        if (conquistaArquivoExiste("/data/HyperNeiva/configuracao/dropbox_token.txt")) { conquistaGanhar("dropbox_app"); conquistaGanhar("dropbox_chave"); }
        if (conquistaArquivoExiste("/data/HyperNeivaTelegram/api.txt")) conquistaGanhar("tg_chave");
        FILE* ch = fopen("/data/HyperNeiva/configuracao/chaves_api.txt", "r");
        if (ch) {
            char l[512];
            while (fgets(l, sizeof(l), ch)) if (!strncmp(l, "gdrive_refresh_token=", 21) && l[21] > ' ') { conquistaGanhar("gdrive_app"); conquistaGanhar("gdrive_chave"); }
            fclose(ch);
        }
    }
    if (espera > 0) { espera--; return; }
    if (emuladorAtivo || (videoRodando && !video_minimizado)) return;
    for (int i = 0; i < TOTAL_CONQUISTAS; i++) {
        if (!conquistaPendente[i]) continue;
        conquistaPendente[i] = false;
        if (conquistaFeita[i]) continue;
        conquistaFeita[i] = true;
        conquistaVersao++;
        gravarConquistas();
        snprintf(msgStatus, sizeof(msgStatus), "CONQUISTA: %s", conquistas[i].nome);
        msgTimer = 240;
        espera = 200;
        return;
    }
}

static void contarTopico(int topico, int* feitas, int* total) {
    *feitas = 0;
    *total = 0;
    for (int i = 0; i < TOTAL_CONQUISTAS; i++) {
        if (conquistas[i].topico != topico) continue;
        (*total)++;
        if (conquistaFeita[i]) (*feitas)++;
    }
}

void preencherMenuConquistas() {
    memset(nomes, 0, sizeof(nomes));
    totalItens = 0;
    if (conquistaNivel == 0) {
        for (int t = 0; t < TOTAL_TOPICOS; t++) {
            int feitas, total;
            contarTopico(t, &feitas, &total);
            snprintf(nomes[totalItens++], 256, "%s   %d/%d", topicosConquista[t], feitas, total);
        }
    }
    else {
        for (int i = 0; i < TOTAL_CONQUISTAS; i++) {
            if (conquistas[i].topico != conquistaTopico) continue;
            conquistaIndice[totalItens] = i;
            snprintf(nomes[totalItens++], 256, "%s %s", conquistaFeita[i] ? "[X]" : "[  ]", conquistas[i].nome);
        }
    }
    menuAtual = MENU_CONQUISTAS;
}

void abrirConquistas() {
    if (!conquistasLidas) lerConquistas();
    conquistaNivel = 0;
    sel = 0;
    off = 0;
    preencherMenuConquistas();
}

void acaoCross_Conquistas() {
    if (conquistaNivel != 0) return;
    conquistaTopico = sel;
    conquistaNivel = 1;
    sel = 0;
    off = 0;
    preencherMenuConquistas();
}

bool acaoCircle_Conquistas() {
    if (conquistaNivel == 0) return false;
    conquistaNivel = 0;
    sel = conquistaTopico;
    off = 0;
    preencherMenuConquistas();
    return true;
}

static void conquistaCaixa(uint32_t* p, int x, int y, int w, int h, uint32_t cor) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) p[(y + j) * 1920 + x + i] = cor;
}

static void conquistaParagrafo(uint32_t* p, const char* texto, int tam, int x, int y, int largura, uint32_t cor) {
    char linha[300] = "";
    const char* c = texto;
    while (*c) {
        const char* fim = strchr(c, ' ');
        size_t n = fim ? (size_t)(fim - c) : strlen(c);
        char tentativa[300];
        snprintf(tentativa, sizeof(tentativa), "%s%s%.*s", linha, linha[0] ? " " : "", (int)n, c);
        if (linha[0] && medirLarguraTexto(tentativa, tam) > largura) {
            desenharTexto(p, linha, tam, x, y, cor);
            y += tam + 16;
            snprintf(linha, sizeof(linha), "%.*s", (int)n, c);
        }
        else snprintf(linha, sizeof(linha), "%s", tentativa);
        c += n;
        while (*c == ' ') c++;
    }
    if (linha[0]) desenharTexto(p, linha, tam, x, y, cor);
}

// Montada em RAM e refeita só quando algo muda.
void renderizarConquistas(uint32_t* p) {
    static uint32_t* quadro = NULL;
    static int ultimoSel = -1, ultimoNivel = -1, ultimoTopico = -1, ultimaVersao = -1;
    if (!quadro) quadro = (uint32_t*)malloc(1920 * 1080 * 4);
    if (!quadro) return;
    if (conquistaVersao != ultimaVersao) { int s = sel; preencherMenuConquistas(); sel = s; }
    if (sel != ultimoSel || conquistaNivel != ultimoNivel || conquistaTopico != ultimoTopico || conquistaVersao != ultimaVersao) {
        ultimoSel = sel; ultimoNivel = conquistaNivel; ultimoTopico = conquistaTopico; ultimaVersao = conquistaVersao;
        uint32_t branco = 0xFFFFFFFF, cinza = 0xFFB4BCCB, ouro = 0xFFFFC83C, azul = 0xFF2E5FB8;
        for (int i = 0; i < 1920 * 1080; i++) quadro[i] = 0xFF0E1018;
        int feitas = 0;
        for (int i = 0; i < TOTAL_CONQUISTAS; i++) if (conquistaFeita[i]) feitas++;
        char texto[128];
        desenharTexto(quadro, conquistaNivel ? topicosConquista[conquistaTopico] : "CONQUISTAS", 52, 90, 60, branco);
        snprintf(texto, sizeof(texto), "%d de %d conquistas", feitas, TOTAL_CONQUISTAS);
        desenharTexto(quadro, texto, 30, 90, 130, ouro);
        conquistaCaixa(quadro, 90, 180, 1740, 12, 0xFF262B38);
        conquistaCaixa(quadro, 90, 180, 1740 * feitas / TOTAL_CONQUISTAS, 12, ouro);

        int passo = 68, linhas = 11, inicio = sel - linhas / 2;
        if (inicio > totalItens - linhas) inicio = totalItens - linhas;
        if (inicio < 0) inicio = 0;
        for (int n = 0; n < linhas && inicio + n < totalItens; n++) {
            int i = inicio + n, y = 220 + n * passo;
            if (i == sel) conquistaCaixa(quadro, 90, y - 8, 780, passo - 6, azul);
            bool feita = conquistaNivel && conquistaFeita[conquistaIndice[i]];
            desenharTexto(quadro, nomes[i], 32, 112, y + 6, i == sel ? branco : (feita ? ouro : cinza));
        }
        conquistaCaixa(quadro, 910, 220, 2, 730, 0xFF2E3442);
        if (conquistaNivel == 0) {
            int feitasTopico, total;
            contarTopico(sel, &feitasTopico, &total);
            desenharTexto(quadro, topicosConquista[sel], 42, 960, 230, branco);
            snprintf(texto, sizeof(texto), "%d de %d", feitasTopico, total);
            desenharTexto(quadro, texto, 30, 960, 296, ouro);
            conquistaParagrafo(quadro, resumoTopico[sel], 32, 960, 360, 860, cinza);
        }
        else if (totalItens > 0) {
            const Conquista* c = &conquistas[conquistaIndice[sel]];
            bool feita = conquistaFeita[conquistaIndice[sel]];
            conquistaParagrafo(quadro, c->nome, 42, 960, 230, 860, branco);
            desenharTexto(quadro, feita ? "Conquista ganha" : "Ainda não ganha", 30, 960, 310, feita ? ouro : cinza);
            desenharTexto(quadro, "Como fazer", 30, 960, 380, branco);
            conquistaParagrafo(quadro, c->como, 29, 960, 430, 870, cinza);
        }
        desenharTexto(quadro, conquistaNivel ? "Setas  Escolher      O  Voltar" : "X  Abrir      O  Voltar", 28, 90, 1000, cinza);
    }
    memcpy(p, quadro, 1920 * 1080 * 4);
}

// ===== RELÓGIO =====
#include <time.h>
#include <orbis/SystemService.h>

// Hora local do PS4, por cima de todas as telas de menu.
void desenharRelogio(uint32_t* p) {
    static int fuso = 9999;
    if (fuso == 9999) {
        int minutos = 0, verao = 0;
        fuso = 0;
        if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE, &minutos) == 0 && minutos >= -720 && minutos <= 840) fuso = minutos;
        if (sceSystemServiceParamGetInt((OrbisSystemServiceParamId)5, &verao) == 0 && verao == 1) fuso += 60;
    }
    time_t agora = time(NULL) + (time_t)fuso * 60;
    struct tm* t = gmtime(&agora);
    if (!t) return;
    char texto[40];
    snprintf(texto, sizeof(texto), "%02d:%02d   %02d/%02d/%04d", t->tm_hour, t->tm_min, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    int x = 1880 - medirLarguraTexto(texto, 26);
    desenharTexto(p, texto, 26, x + 2, 12, 0xFF000000);
    desenharTexto(p, texto, 26, x, 10, 0xFFFFFFFF);
}
