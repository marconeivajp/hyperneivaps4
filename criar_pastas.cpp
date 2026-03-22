#include "criar_pastas.h"
#include <stdio.h>
#include <orbis/libkernel.h>

void copiarArquivoSeNaoExistir(const char* srcPath, const char* dstPath) {
    FILE* check = fopen(dstPath, "rb");
    if (check) {
        fclose(check);
        return;
    }
    FILE* src = fopen(srcPath, "rb");
    if (src) {
        FILE* dst = fopen(dstPath, "wb");
        if (dst) {
            char buffer[4096];
            size_t bytesLidos;
            while ((bytesLidos = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                fwrite(buffer, 1, bytesLidos, dst);
            }
            fclose(dst);
        }
        fclose(src);
    }
}

void inicializarPastas() {
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/xml", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorio", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorio/games", 0777);

    sceKernelMkdir("/data/HyperNeiva/midia", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/musicas", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens", 0777);

    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games/Artwork1", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games/Artwork2", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens/Games/Background", 0777);

    sceKernelMkdir("/data/HyperNeiva/midia/leitura", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/arquivos", 0777);

    sceKernelMkdir("/data/HyperNeiva/midia/videos/video", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/filmes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/series", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/animes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/outros", 0777);

    sceKernelMkdir("/data/HyperNeiva/midia/leitura/livros", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/mangas", 0777);

    // XML base e Token Dropbox
    copiarArquivoSeNaoExistir("/app0/assets/Sega_Master_System.xml", "/data/HyperNeiva/baixado/repositorio/games/Sega_Master_System.xml");
    copiarArquivoSeNaoExistir("/app0/assets/dropbox_token.txt", "/data/HyperNeiva/configuracao/dropbox_token.txt");

    // Copia as imagens Default para CONFIGURACAO (Testa PNG e JPG)
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.png", "/data/HyperNeiva/configuracao/0_Defalt_Background.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.jpg", "/data/HyperNeiva/configuracao/0_Defalt_Background.jpg");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork1.png", "/data/HyperNeiva/configuracao/0_Defalt_Artwork1.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork2.png", "/data/HyperNeiva/configuracao/0_Defalt_Artwork2.png");

    // Copia as imagens Default para GAMES (Testa PNG e JPG)
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.png", "/data/HyperNeiva/midia/imagens/Games/Background/0_Defalt_Background.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Background.jpg", "/data/HyperNeiva/midia/imagens/Games/Background/0_Defalt_Background.jpg");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork1.png", "/data/HyperNeiva/midia/imagens/Games/Artwork1/0_Defalt_Artwork1.png");
    copiarArquivoSeNaoExistir("/app0/assets/images/0_Defalt_Artwork2.png", "/data/HyperNeiva/midia/imagens/Games/Artwork2/0_Defalt_Artwork2.png");
}