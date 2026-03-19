#include "criar_pastas.h"
#include <stdio.h>
#include <orbis/libkernel.h>

void inicializarPastas() {
    // Pastas originais
    sceKernelMkdir("/data/HyperNeiva", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);
    sceKernelMkdir("/data/HyperNeiva/configuracao/xml", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado", 0777);
    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);

    // 1. Criar as novas pastas solicitadas para o repositório
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorio", 0777);
    sceKernelMkdir("/data/HyperNeiva/baixado/repositorio/games", 0777);

    // --- NOVAS PASTAS DE MÍDIA DO HYPER NEIVA ---

    // Pasta Mídia raiz
    sceKernelMkdir("/data/HyperNeiva/midia", 0777);

    // Subpastas principais de mídia
    sceKernelMkdir("/data/HyperNeiva/midia/musicas", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/imagens", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/arquivos", 0777);

    // Categorias dentro de vídeos
    sceKernelMkdir("/data/HyperNeiva/midia/videos/video", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/filmes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/series", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/animes", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/videos/outros", 0777);

    // Categorias dentro de leitura
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/livros", 0777);
    sceKernelMkdir("/data/HyperNeiva/midia/leitura/mangas", 0777);

    // ---------------------------------------------

    // 2. Verificar se o XML já existe no destino
    FILE* dstCheck = fopen("/data/HyperNeiva/baixado/repositorio/games/Sega_Master_System.xml", "rb");

    if (dstCheck) {
        // Se abriu, o ficheiro já existe. Não fazemos nada.
        fclose(dstCheck);
    }
    else {
        // 3. Se não existe, copiamos do pacote original da app (/app0/assets/)
        FILE* src = fopen("/app0/assets/Sega_Master_System.xml", "rb");

        if (src) {
            FILE* dst = fopen("/data/HyperNeiva/baixado/repositorio/games/Sega_Master_System.xml", "wb");

            if (dst) {
                char buffer[4096];
                size_t bytesLidos;

                // Copia o ficheiro em blocos de 4KB para ser instantâneo
                while ((bytesLidos = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                    fwrite(buffer, 1, bytesLidos, dst);
                }
                fclose(dst);
            }
            fclose(src);
        }
    }
}