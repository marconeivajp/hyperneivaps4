#include "criar_pastas.h"
#include <stdio.h>
#include <orbis/libkernel.h>

void copiarArquivoSeNaoExistir(const char* srcPath, const char* dstPath) {
    FILE* check = fopen(dstPath, "rb");
    if (check) { fclose(check); return; }
    FILE* src = fopen(srcPath, "rb");
    if (src) {
        FILE* dst = fopen(dstPath, "wb");
        if (dst) {
            char buffer[4096]; size_t bytesLidos;
            while ((bytesLidos = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, bytesLidos, dst);
            fclose(dst);
        }
        fclose(src);
    }
}

void criarEstruturaSubpastas(const char* consolePath) {
    char buf[512];
    sceKernelMkdir(consolePath, 0777);
    snprintf(buf, sizeof(buf), "%s/artwork1", consolePath); sceKernelMkdir(buf, 0777);
    snprintf(buf, sizeof(buf), "%s/artwork2", consolePath); sceKernelMkdir(buf, 0777);
    snprintf(buf, sizeof(buf), "%s/artwork3", consolePath); sceKernelMkdir(buf, 0777);
    snprintf(buf, sizeof(buf), "%s/background", consolePath); sceKernelMkdir(buf, 0777);
    snprintf(buf, sizeof(buf), "%s/barra_de_load", consolePath); sceKernelMkdir(buf, 0777);
    snprintf(buf, sizeof(buf), "%s/elementos1", consolePath); sceKernelMkdir(buf, 0777);
    snprintf(buf, sizeof(buf), "%s/ponteiros", consolePath); sceKernelMkdir(buf, 0777);
}

void inicializarPastasGamesESistemas(const char* rootMidia) {
    char pathBase[512];
    char pathConsole[512];

    // --- GAMES ---
    snprintf(pathBase, sizeof(pathBase), "%s/Games", rootMidia);
    sceKernelMkdir(pathBase, 0777);

    // Atari
    snprintf(pathConsole, sizeof(pathConsole), "%s/Atari", pathBase); sceKernelMkdir(pathConsole, 0777);
    const char* atari[] = { "Atari - 2600", "Atari - 5200", "Atari - 7800", "Atari - Jaguar", "Atari - Lynx", "Atari - ST", "Atari - XEGS" };
    for (int i = 0; i < 7; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, atari[i]); criarEstruturaSubpastas(p); }

    // Microsoft
    snprintf(pathConsole, sizeof(pathConsole), "%s/Microsoft", pathBase); sceKernelMkdir(pathConsole, 0777);
    const char* ms[] = { "Microsoft - Xbox", "Microsoft - Xbox 360", "Microsoft - Xbox One" };
    for (int i = 0; i < 3; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, ms[i]); criarEstruturaSubpastas(p); }

    // Nintendo
    snprintf(pathConsole, sizeof(pathConsole), "%s/Nintendo", pathBase); sceKernelMkdir(pathConsole, 0777);
    const char* nin[] = { 
        "Nintendo - Game Boy", "Nintendo - Game Boy Advance", "Nintendo - Game Boy Color", 
        "Nintendo - GameCube", "Nintendo - Nintendo 3DS", "Nintendo - Nintendo 64", 
        "Nintendo - Nintendo DS", "Nintendo - Nintendo Entertainment System", 
        "Nintendo - Nintendo Switch", "Nintendo - Super Nintendo Entertainment System", 
        "Nintendo - Virtual Boy", "Nintendo - Wii", "Nintendo - Wii U" 
    };
    for (int i = 0; i < 13; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, nin[i]); criarEstruturaSubpastas(p); }

    // Sega
    snprintf(pathConsole, sizeof(pathConsole), "%s/Sega", pathBase); sceKernelMkdir(pathConsole, 0777);
    const char* sega[] = { 
        "Sega - 32X", "Sega - Dreamcast", "Sega - Game Gear", 
        "Sega - Master System - Mark III", "Sega - Mega Drive - Genesis", 
        "Sega - Sega CD", "Sega - Saturn", "Sega - SG-1000" 
    };
    for (int i = 0; i < 8; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, sega[i]); criarEstruturaSubpastas(p); }

    // Sony
    snprintf(pathConsole, sizeof(pathConsole), "%s/Sony", pathBase); sceKernelMkdir(pathConsole, 0777);
    const char* sony[] = { 
        "Sony - PlayStation", "Sony - PlayStation 2", "Sony - PlayStation 3", 
        "Sony - PlayStation 4", "Sony - PlayStation Portable", "Sony - PlayStation Vita" 
    };
    for (int i = 0; i < 6; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, sony[i]); criarEstruturaSubpastas(p); }

    // --- SISTEMAS ---
    snprintf(pathBase, sizeof(pathBase), "%s/Sistemas", rootMidia);
    sceKernelMkdir(pathBase, 0777);
    const char* sistemas[] = { "Editar", "Midia", "Explorar", "Baixar" };
    for (int i = 0; i < 4; i++) {
        char pS[512]; snprintf(pS, sizeof(pS), "%s/%s", pathBase, sistemas[i]); sceKernelMkdir(pS, 0777);
        char pG[512]; snprintf(pG, sizeof(pG), "%s/Geral", pS); criarEstruturaSubpastas(pG);
    }
}

void inicializarPastas() {
    sceKernelMkdir("/data/HyperNeiva", 0777);

    // CONFIGURAÇÃO E SUBPASTAS
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/imagens", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/imagens/elementos", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/imagens/sistema", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/audios", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/audios/elementos_sonoros", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/jogar", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/settings", 0777);

    // VÍDEO IMAGENS E CATEGORIAS DE SISTEMA (V64)
    sceKernelMkdir("/data/HyperNeiva/configuracao/video_imagens", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/game_e_sistema", 0777);

    // REPOSITÓRIOS E SUAS NOVAS SUBPASTAS (LOJAS)
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios/games", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios/imagens para perfil", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios/xml games", 0777);

    // BAIXADO E SUBPASTAS
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/linkdireto", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/capas", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorios", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorios/games", 0777);

    // MÍDIA E SUBPASTAS (REESTRUTURAÇÃO MASSIVA V65)
    sceKernelMkdir("/data/HyperNeiva/midia", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/musicas", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/audios", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/arquivos", 0777);

    // IMAGENS: GAMES E SISTEMAS
    inicializarPastasGamesESistemas("/data/HyperNeiva/midia/imagens");

    // VÍDEOS: GAMES E SISTEMAS (Dentro de midia/videos/video)
    sceKernelMkdir("/data/HyperNeiva/midia/videos/video", 0777);
    inicializarPastasGamesESistemas("/data/HyperNeiva/midia/videos/video");

    // ÁUDIOS: ELEMENTOS SONOROS (Games e Sistemas)
    sceKernelMkdir("/data/HyperNeiva/midia/audios/elementos sonoros", 0777);
    inicializarPastasGamesESistemas("/data/HyperNeiva/midia/audios/elementos sonoros");

    // Outras subpastas de Vídeos
    sceKernelMkdir("/data/HyperNeiva/midia/videos/filmes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/series", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/animes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/outros", 0777);

    // Leitura e Música
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/livros", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/mangas", 0777);
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);


    // =========================================================
    // ARQUIVOS DO JOGAR (HOMEBREWS/RETRO)
    // =========================================================
    copiarArquivoSeNaoExistir("/app0/assets/system.xml", "/data/HyperNeiva/configuracao/jogar/system.xml");
    copiarArquivoSeNaoExistir("/app0/assets/lista.xml", "/data/HyperNeiva/configuracao/jogar/lista.xml");
    copiarArquivoSeNaoExistir("/app0/assets/sp.xml", "/data/HyperNeiva/configuracao/jogar/sp.xml");
    copiarArquivoSeNaoExistir("/app0/assets/Sega_Master_System.xml", "/data/HyperNeiva/configuracao/jogar/Sega_Master_System.xml");

    // =========================================================
    // ARQUIVOS DOS REPOSITÓRIOS (AS 3 LOJAS)
    // =========================================================
    copiarArquivoSeNaoExistir("/app0/assets/systemas+zipados.xml", "/data/HyperNeiva/configuracao/repositorios/games/systemas+zipados.xml");
    copiarArquivoSeNaoExistir("/app0/assets/Sega_Master_System.xml", "/data/HyperNeiva/configuracao/repositorios/games/Sega_Master_System.xml"); // <-- MASTER SYSTEM AGORA ESTÁ AQUI
    copiarArquivoSeNaoExistir("/app0/assets/retrocast_brasil.xml", "/data/HyperNeiva/configuracao/repositorios/games/retrocast_brasil.xml");
    copiarArquivoSeNaoExistir("/app0/assets/xavatar.xml", "/data/HyperNeiva/configuracao/repositorios/imagens para perfil/xavatar.xml");
    copiarArquivoSeNaoExistir("/app0/assets/xml.xml", "/data/HyperNeiva/configuracao/repositorios/xml games/xml.xml");

    // LIXEIRO: Remove os arquivos antigos que ficaram presos na pasta geral do PS4
    remove("/data/HyperNeiva/configuracao/repositorios/system.xml");
    remove("/data/HyperNeiva/configuracao/repositorios/lista.xml");
    remove("/data/HyperNeiva/configuracao/repositorios/sp.xml");
    remove("/data/HyperNeiva/configuracao/repositorios/Sega_Master_System.xml");


    // O TOKEN DO DROPBOX É COPIADO AQUI
    copiarArquivoSeNaoExistir("/app0/assets/dropbox_token.txt", "/data/HyperNeiva/configuracao/dropbox_token.txt");

    // IMAGENS PADRÃO (Configuração)
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.png", "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.jpg", "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.jpg");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork1.png", "/data/HyperNeiva/configuracao/imagens/0_Defalt_Artwork1.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork2.png", "/data/HyperNeiva/configuracao/imagens/0_Defalt_Artwork2.png");

    // ELEMENTOS E PONTEIRO ADICIONADOS AQUI
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_elemento1.png", "/data/HyperNeiva/configuracao/imagens/0_Defalt_elemento1.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_elemento_controlavel1.png", "/data/HyperNeiva/configuracao/imagens/0_Defalt_elemento_controlavel1.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_ponteiro1.png", "/data/HyperNeiva/configuracao/imagens/0_Defalt_ponteiro1.png");

    // IMAGENS PADRÃO (Mídia Games)
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.png", "/data/HyperNeiva/midia/imagens/Games/Background/0_Defalt_Background.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.jpg", "/data/HyperNeiva/midia/imagens/Games/Background/0_Defalt_Background.jpg");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork1.png", "/data/HyperNeiva/midia/imagens/Games/Artwork1/0_Defalt_Artwork1.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork2.png", "/data/HyperNeiva/midia/imagens/Games/Artwork2/0_Defalt_Artwork2.png");

    // ÁUDIOS PADRÃO
    copiarArquivoSeNaoExistir("/app0/assets/audio/bgm.wav", "/data/HyperNeiva/configuracao/audios/bgm.wav");
    copiarArquivoSeNaoExistir("/app0/assets/audio/0_Defalt_direcinal_cima.wav", "/data/HyperNeiva/configuracao/audios/0_Defalt_direcinal_cima.wav");
    copiarArquivoSeNaoExistir("/app0/assets/audio/0_Defalt_direcional_baixo.wav", "/data/HyperNeiva/configuracao/audios/0_Defalt_direcional_baixo.wav");
    copiarArquivoSeNaoExistir("/app0/assets/audio/0_Defalt_x.wav", "/data/HyperNeiva/configuracao/audios/0_Defalt_x.wav");
    copiarArquivoSeNaoExistir("/app0/assets/audio/0_Defalt_bolinha.wav", "/data/HyperNeiva/configuracao/audios/0_Defalt_bolinha.wav");

    // =========================================================
    // ESTRUTURA RETROARCH (GAMES & THUMBNAILS)
    // =========================================================
    sceKernelMkdir("/data/retroarch", 0777);
    sceKernelMkdir("/data/retroarch/Games", 0777);
    sceKernelMkdir("/data/retroarch/savefiles", 0777);
    sceKernelMkdir("/data/retroarch/savestates", 0777);

    // Nintendo
    sceKernelMkdir("/data/retroarch/Games/Nintendo - Nintendo Entertainment System", 0777);
    sceKernelMkdir("/data/retroarch/Games/Nintendo - Super Nintendo Entertainment System", 0777);
    sceKernelMkdir("/data/retroarch/Games/Nintendo - Game Boy", 0777);
    sceKernelMkdir("/data/retroarch/Games/Nintendo - Game Boy Color", 0777);
    sceKernelMkdir("/data/retroarch/Games/Nintendo - Game Boy Advance", 0777);
    sceKernelMkdir("/data/retroarch/Games/Nintendo - Nintendo 64", 0777);

    // Sega
    sceKernelMkdir("/data/retroarch/Games/Sega - SG-1000", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sega - Master System - Mark III", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sega - Mega Drive - Genesis", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sega - 32X", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sega - Saturn", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sega - Dreamcast", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sega - Game Gear", 0777);

    // Sony
    sceKernelMkdir("/data/retroarch/Games/Sony - PlayStation", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sony - PlayStation 2", 0777);
    sceKernelMkdir("/data/retroarch/Games/Sony - PlayStation Portable", 0777);

    // Outros
    sceKernelMkdir("/data/retroarch/Games/SNK - Neo Geo", 0777);
    sceKernelMkdir("/data/retroarch/Games/Bandai - WonderSwan Color", 0777);
}
