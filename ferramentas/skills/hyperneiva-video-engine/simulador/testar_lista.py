"""Extrai a escolha de icones real de menu_grafico_layout.cpp e renderiza uma lista de amostra."""
import os
import re
import subprocess

P = r"C:\Users\Marco\source\repos\teste3\teste3"
os.environ["PATH"] = r"C:\msys64\mingw64\bin;" + os.environ["PATH"]
t = open(os.path.join(P, "menu_grafico_layout.cpp"), "rb").read().decode("utf-8", "surrogateescape").replace("\r\n", "\n")
ini = t.index("static int iconeArquivo(const char* extensao) {")
fim = t.index("static void drawSimpleIcon(")
codigo = t[ini:fim]
v = open(os.path.join(P, "menu_video.cpp"), "rb").read().decode("utf-8", "surrogateescape").replace("\r\n", "\n")
ini = v.index("struct IconeCarregado {")
fim = v.index("// =========================================================================\n// CONTROLES DO PLAYER")
icones = v[ini:fim]

teste = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "icones_embutidos.h"
enum MenuLevel { ROOT, MENU_MIDIA, MENU_EXPLORAR, MENU_RADIO_LISTA, MENU_RADIO_FAVORITOS, MENU_RADIO_CATEGORIA, MENU_EDITAR, MENU_EDIT_TARGET, MENU_JOGAR_PS4, JOGAR_XML, SCRAPER_LIST, MENU_BAIXAR };
''' + icones + codigo + r'''
struct Item { const char* n; MenuLevel m; };
static Item itens[] = {
 {"JOGAR",ROOT},{"MIDIA",ROOT},{"BAIXAR",ROOT},{"EDITAR",ROOT},{"EXPLORAR",ROOT},{"EXTRA",ROOT},{"INFORMACOES",ROOT},
 {"PS4 NATIVO",ROOT},{"JOGOS XML",ROOT},{"Radio & Podcast",MENU_MIDIA},{"arquivos",MENU_MIDIA},{"audios",MENU_MIDIA},{"imagens",MENU_MIDIA},{"leitura",MENU_MIDIA},{"musicas",MENU_MIDIA},{"videos",MENU_MIDIA},
 {"animes",MENU_MIDIA},{"series",MENU_MIDIA},{"outros",MENU_MIDIA},{"Pasta vazia",MENU_MIDIA},
 {"[Minha Pasta]",MENU_EXPLORAR},{"episodio.mp4",MENU_EXPLORAR},{"filme.mkv",MENU_EXPLORAR},{"musica.mp3",MENU_EXPLORAR},{"foto.JPG",MENU_EXPLORAR},{"livro.pdf",MENU_EXPLORAR},{"manga.cbz",MENU_EXPLORAR},
 {"jogo.pkg",MENU_EXPLORAR},{"backup.7z",MENU_EXPLORAR},{"rom.gba",MENU_EXPLORAR},{"notas.txt",MENU_EXPLORAR},{"estranho.xyz",MENU_EXPLORAR},
 {"Hyper Neiva",MENU_EXPLORAR},{"Raiz",MENU_EXPLORAR},{"USB0",MENU_EXPLORAR},{"Dropbox",MENU_BAIXAR},{"FTP (PC / Android)",MENU_BAIXAR},{"Download (Do PC pro PS4)",MENU_BAIXAR},{"Upload (Do PS4 pro PC)",MENU_BAIXAR},
 {"Lojas",MENU_BAIXAR},{"Repositorios",MENU_BAIXAR},{"LINK DIRETO",MENU_BAIXAR},{"Menu Backup",MENU_BAIXAR},{"[+] Adicionar Novo Servidor",MENU_BAIXAR},{"[ SALVAR SERVIDOR ]",MENU_BAIXAR},
 {"Teste de Controle (Analise de Drift e Botoes)",ROOT},{"Piano Virtual (Instrumento)",ROOT},{"COR DA FONTE (MENU)",MENU_EDITAR},{"Kiss FM",MENU_RADIO_LISTA},{"Erro na API",MENU_BAIXAR}
};
static stbtt_fontinfo fonte;
static void texto(uint32_t* p, const char* s, int tam, int x, int y) {
    float e = stbtt_ScaleForPixelHeight(&fonte, (float)tam); int asc; stbtt_GetFontVMetrics(&fonte, &asc, 0, 0); asc = (int)(asc * e); int cx0 = x;
    for (int i = 0; s[i]; i++) { int adv, lsb, x0, y0, x1, y1; stbtt_GetCodepointHMetrics(&fonte, s[i], &adv, &lsb); stbtt_GetCodepointBitmapBox(&fonte, s[i], e, e, &x0, &y0, &x1, &y1);
        int w = x1 - x0, h = y1 - y0; if (w > 0 && h > 0) { std::vector<unsigned char> b(w * h); stbtt_MakeCodepointBitmap(&fonte, b.data(), w, h, w, e, e, s[i]);
            for (int a = 0; a < h; a++) for (int c = 0; c < w; c++) { int X = cx0 + x0 + c, Y = y + asc + y0 + a; if (X >= 0 && X < 1920 && Y >= 0 && Y < 1080 && b[a * w + c] > 60) p[Y * 1920 + X] = 0xFFFFFFFF; } }
        cx0 += (int)(adv * e); }
}
int main() {
    FILE* f = fopen("C:/Users/Marco/source/repos/teste3/teste3/assets/fonts/font.ttf", "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); std::vector<unsigned char> d(n); fread(d.data(), 1, n, f); fclose(f); stbtt_InitFont(&fonte, d.data(), 0);
    static uint32_t tela[1920 * 1080]; for (int i = 0; i < 1920 * 1080; i++) tela[i] = 0xFF1B2230;
    int total = sizeof(itens) / sizeof(itens[0]), falhas = 0;
    for (int i = 0; i < total; i++) {
        int id = iconeDoItem(itens[i].n, itens[i].m); int col = i / 17, lin = i % 17;
        printf("%-46s -> %s\n", itens[i].n, id >= 0 ? iconesEmbutidos[id].nome : "NENHUM"); if (id < 0) falhas++;
        desenharIcone(tela, id, 40, 30 + col * 630, 20 + lin * 62); texto(tela, itens[i].n, 24, 82 + col * 630, 28 + lin * 62);
    }
    FILE* o = fopen("out/lista.ppm", "wb"); fprintf(o, "P6\n1920 1080\n255\n"); for (int i = 0; i < 1920 * 1080; i++) { unsigned char c[3] = { (unsigned char)(tela[i] >> 16), (unsigned char)(tela[i] >> 8), (unsigned char)tela[i] }; fwrite(c, 1, 3, o); } fclose(o);
    printf("sem icone: %d\n", falhas); return falhas;
}
'''
open("teste_lista.cpp", "w", encoding="utf-8", errors="surrogateescape").write(teste)
r = subprocess.run([r"C:\msys64\mingw64\bin\g++.exe", "-std=gnu++17", "-O1", "-I" + P, "teste_lista.cpp", "-o", "teste_lista.exe"], capture_output=True, text=True)
print(r.stderr[-1500:])
r = subprocess.run(["teste_lista.exe"], shell=True, capture_output=True, text=True)
print(r.stdout)
subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", "out/lista.ppm", "-vf", "scale=1600:-1", "out/lista.png"])
