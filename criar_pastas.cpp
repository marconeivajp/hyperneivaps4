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

void inicializarPastas() {
    sceKernelMkdir("/data/HyperNeiva", 0777);

    // CONFIGURAÇÃO
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/temporario", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/imagens", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/audios", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/jogar", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/repositorios", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/settings", 0777);

    // BAIXADO E SUBPASTAS
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/linkdireto", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/dropbox", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/capas", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorios", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorios/games", 0777);

    // MÍDIA E SUBPASTAS
    sceKernelMkdir("/data/HyperNeiva/midia", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/musicas", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/audios", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/arquivos", 0777);

    // Subpastas de Vídeos
    sceKernelMkdir("/data/HyperNeiva/midia/videos/video", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/filmes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/series", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/animes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/outros", 0777);

    // Subpastas de Imagens (Games)
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games/Artwork1", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games/Artwork2", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games/Background", 0777);

    // Subpastas de Leitura
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/livros", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/mangas", 0777);

    // Extra (Musicas Root)
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);

    // =========================================================
    // ARQUIVOS INICIAIS DE SISTEMA (XMLS)
    // =========================================================

    // Copia para a raiz do "configuracao" (Onde o jogar.cpp vai procurar)
    copiarArquivoSeNaoExistir("/app0/assets/system.xml", "/data/HyperNeiva/configuracao/system.xml");
    copiarArquivoSeNaoExistir("/app0/assets/lista.xml", "/data/HyperNeiva/configuracao/lista.xml");
    copiarArquivoSeNaoExistir("/app0/assets/sp.xml", "/data/HyperNeiva/configuracao/sp.xml");
    copiarArquivoSeNaoExistir("/app0/assets/Sega_Master_System.xml", "/data/HyperNeiva/configuracao/Sega_Master_System.xml");

    // Copia para a pasta "repositorios" (Para o menu de Baixar Repos achar)
    copiarArquivoSeNaoExistir("/app0/assets/system.xml", "/data/HyperNeiva/configuracao/repositorios/system.xml");
    copiarArquivoSeNaoExistir("/app0/assets/lista.xml", "/data/HyperNeiva/configuracao/repositorios/lista.xml");
    copiarArquivoSeNaoExistir("/app0/assets/sp.xml", "/data/HyperNeiva/configuracao/repositorios/sp.xml");
    copiarArquivoSeNaoExistir("/app0/assets/Sega_Master_System.xml", "/data/HyperNeiva/configuracao/repositorios/Sega_Master_System.xml");


    // O TOKEN DO DROPBOX É COPIADO AQUI (Garantindo que a pasta configuracao já existe!)
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
}