#include "informacao.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>
#include <stdio.h>

extern MenuLevel menuAtual;
extern int totalItens;
extern int sel;
extern void remotoDesenharQrTexto(uint32_t* p, const char* texto, int x, int y, int modulo);

#define LINK_DOACAO "https://nubank.com.br/cobrar/19bpla/6af52563-cda8-41c4-b2b9-e64a7033841c"

void preencherMenuInformacao() {
    menuAtual = MENU_INFORMACAO;
    totalItens = 0;
    sel = 0;
}

static void ret(uint32_t* p, int x, int y, int w, int h, uint32_t cor) {
    for (int yy = y; yy < y + h && yy < 1080; yy++) for (int xx = x; xx < x + w && xx < 1920; xx++) p[yy * 1920 + xx] = cor;
}

void renderizarInformacao(uint32_t* p) {
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF101418;

    desenharTexto(p, "HYPER NEIVA PS4", 54, 60, 40, 0xFF00AAFF);
    desenharTexto(p, "Versão 3.56  -  feito por Marcão (marconeivajp)  -  github.com/marconeivajp/hyperneivaps4", 24, 60, 108, 0xFFB8C2CC);
    ret(p, 60, 146, 1260, 2, 0xFF2A3440);

    desenharTexto(p, "O que tem no app", 30, 60, 166, 0xFF4CD964);
    static const char* recursos[] = {
        "JOGAR: jogos do PS4, listas XML e emuladores (GBA, SNES, PS1, N64, Saturn...)",
        "MÍDIA: player de vídeo com legendas, PiP e capas; músicas com letra e capa",
        "Rádios do mundo inteiro com capas e temas; podcasts",
        "BAIXAR: FTP, Dropbox, Google Drive, GitHub, torrent, links e downloads do PS4",
        "Gerenciador de downloads com a origem de cada arquivo",
        "EXPLORAR: dois painéis, copiar, 7z/zip/rar, PKG, PDF, imagens e modelos 3D",
        "Telegram completo: conversas, grupos, canais, arquivos e áudio",
        "Celular e PC: controle, teclado, tela do PS4 e do PC, arquivos e chaves",
        "IA (Gemini, OpenAI, Groq), transcrição de áudio e tradução",
        "Editor de imagem com camadas, texto e filtros; tempo, piano e conquistas",
    };
    for (int i = 0; i < 10; i++) desenharTexto(p, recursos[i], 22, 80, 210 + i * 34, 0xFFDDE3EA);

    desenharTexto(p, "Créditos: código e bibliotecas de terceiros", 30, 60, 566, 0xFF4CD964);
    static const char* creditos[] = {
        "OpenOrbis Toolchain (SDK aberto do PS4)",
        "GoldHEN (sleirsgoevy, SiSTRo e equipe)",
        "FFmpeg (vídeo e áudio, LGPL)",
        "TDLib (Telegram, Boost)",
        "libtorrent-rasterbar (BSD)",
        "MuPDF (PDF, AGPL)",
        "OpenSSL e zlib",
        "miniz, LZMA SDK (7-Zip) e libarchive (RAR)",
        "stb_image, stb_truetype, stb_image_write",
        "dr_mp3 e dr_wav (mackron)",
        "FreeType, libpng, libjpeg, Opus",
        "Twemoji (emojis, CC-BY 4.0)",
        "libretro / RetroArch: mGBA, snes9x",
        "pcsx_rearmed, parallel-n64 (angrylion)",
        "Yabause (Sega Saturn)",
        "EmuSuNi / DolphinPS4 (referência, GPL-2)",
        "Radio Browser (rádios), iTunes e Deezer",
        "TMDB (capas de filmes), LRCLIB (letras)",
        "Open-Meteo (previsão do tempo)",
        "APIs: Dropbox, Google Drive, GitHub",
        "Gemini, OpenAI e Groq (IA)",
        "Comunidade PS4 homebrew e testadores",
    };
    for (int i = 0; i < 22; i++) {
        int col = i / 11, lin = i % 11;
        desenharTexto(p, creditos[i], 20, 80 + col * 640, 610 + lin * 32, 0xFFAAB4BF);
    }

    // Doação, no canto direito.
    ret(p, 1360, 40, 520, 940, 0xFF182230);
    desenharTexto(p, "Apoie o projeto", 40, 1400, 70, 0xFFFFFFFF);
    desenharTexto(p, "Gostou do Hyper Neiva? Doe quanto", 24, 1400, 130, 0xFFDDE3EA);
    desenharTexto(p, "puder pelo Nubank (Pix ou cartão).", 24, 1400, 162, 0xFFDDE3EA);
    desenharTexto(p, "Toda ajuda mantém o app vivo.", 24, 1400, 194, 0xFFDDE3EA);
    remotoDesenharQrTexto(p, LINK_DOACAO, 1415, 250, 11);
    desenharTexto(p, "Aponte a câmera do celular", 24, 1400, 760, 0xFFFFE08A);
    desenharTexto(p, "nubank.com.br/cobrar/19bpla/", 22, 1400, 810, 0xFF8FC8FF);
    desenharTexto(p, "6af52563-cda8-41c4-b2b9-", 22, 1400, 840, 0xFF8FC8FF);
    desenharTexto(p, "e64a7033841c", 22, 1400, 870, 0xFF8FC8FF);
    desenharTexto(p, "Obrigado!", 30, 1400, 920, 0xFF4CD964);

    desenharTexto(p, "[O] Voltar", 24, 60, 1030, 0xFF8A96A3);
}
