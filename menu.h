#ifndef MENU_H
#define MENU_H

enum MenuLevel {
    ROOT,
    MENU_TIPO_JOGO,
    MENU_JOGAR_PS4,
    SCRAPER_LIST,
    JOGAR_XML,
    MENU_MUSICAS,
    MENU_EXPLORAR_HOME,
    MENU_EXPLORAR,
    MENU_BAIXAR,
    MENU_EDITAR,
    MENU_EDIT_TARGET,
    MENU_AUDIO_OPCOES,
    MENU_NOTEPAD,
    MENU_LOJAS,
    MENU_CAPAS, // <--- AQUI ESTÁ ELE DE VOLTA!
    MENU_CONSOLES,
    MENU_BAIXAR_DROPBOX_LISTA,
    MENU_BAIXAR_DROPBOX_UPLOAD,
    MENU_BAIXAR_DROPBOX_BACKUP,
    MENU_BAIXAR_FTP_SERVIDORES,
    MENU_BAIXAR_FTP_EDITAR_SERVIDOR,
    MENU_BAIXAR_FTP_LISTA,
    MENU_BAIXAR_FTP_UPLOAD_RAIZES,
    MENU_BAIXAR_FTP_UPLOAD,
    MENU_BAIXAR_REPOS,
    MENU_BAIXAR_GAMES_XMLS,
    MENU_BAIXAR_GAMES_LIST,
    MENU_BAIXAR_LINKS,
    MENU_BAIXAR_LINK_DIRETO,
    MENU_MIDIA,
    MENU_EXTRA,
    MENU_INFORMACAO,
    MENU_CONTROLE_TESTE,
    MENU_INSTRUMENTOS
};

extern MenuLevel menuAtual;
extern char nomes[3000][64];
extern int totalItens;
extern int sel;
extern int off;
extern int offEsq;
extern char msgStatus[128];
extern int msgTimer;
extern char caminhoMidiaAtual[512];

void preencherRoot();
void preencherExplorerHome();
void abrirPastaMidia(const char* caminho);
void preencherMenuMidia();

#endif