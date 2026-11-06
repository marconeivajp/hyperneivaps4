"""Gera no PC imagens das telas do Telegram com dados inventados, usando o mesmo código de desenho."""
import os
import subprocess

A = os.path.dirname(os.path.abspath(__file__))
C = r"C:\Users\Marco\source\repos\teste3\HyperNeiva-Telegram"
v = open(os.path.join(A, "tg_visual.cpp"), encoding="utf-8").read()
fonte = (C + r"\assets\fonts\font.ttf").replace("\\", "/")
pacote = os.path.join(A, "emoji.pack").replace("\\", "/")
saida = A.replace("\\", "/")
c = r'''#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_truetype.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#define TG_COR(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))
enum TgTela { TG_INICIANDO, TG_TELEFONE, TG_CODIGO, TG_SENHA, TG_SEM_SUPORTE, TG_CONVERSAS, TG_MENSAGENS };
#define TG_MAX_PASTAS 14
#define TG_MAX_MENSAGENS 120
#include <pthread.h>
enum TgMidia { TG_M_NADA, TG_M_FOTO, TG_M_VIDEO, TG_M_AUDIO };
enum TgEstadoDownload { TG_D_BAIXANDO, TG_D_COPIAR, TG_D_COPIANDO, TG_D_PRONTO, TG_D_ERRO };
struct TgDownload { int arquivo, estado; bool salvar, pkg; long long total, baixado, mensagem; char nome[120]; };
struct TgConversa { long long id, data, lidoAte; long long ordens[TG_MAX_PASTAS + 2]; int naoLidas, foto; char titulo[160]; char ultima[260]; };
struct TgMensagem { long long id, autor, data; bool minha, figurinha; int arquivo, imgL, imgA, grande, midia, totalLinks, alturaCache[2]; char rotulo[32]; char resumo[48]; char arquivoNome[100]; char links[4][200]; char texto[1024]; };
struct TgPasta { int id; char nome[64]; };
struct TgResultado { long long conversa, mensagem, data; char texto[200]; };
stbtt_fontinfo font;
static TgTela tgTela = TG_CONVERSAS;
static char tgAviso[160] = "";
static TgConversa tgConversas[300]; static int tgTotalConversas = 0, tgIndice[300], tgVisiveis = 0, tgSelecao = 0, tgTopo = 0;
static TgMensagem tgMensagens[120]; static int tgTotalMensagens = 0, tgRolagem = 0; static long long tgAberta = 0, tgSelId = 0, tgAudioMensagem = 0, tgBuscaConversa = 0;
static bool tgFimDaLista[TG_MAX_PASTAS + 2]; static TgPasta tgPastas[TG_MAX_PASTAS]; static int tgTotalPastas = 0, tgAba = 0;
static bool tgEmBusca = false, tgAudioRodando = false; static TgResultado tgResultados[60]; static int tgTotalResultados = 0, tgSelResultado = 0, tgTopoResultado = 0, tgBuscasPendentes = 0; static char tgBuscaTexto[128] = "";
static int tgVisor = 0, tgVisorMini = 0, tgVisorX = 0, tgVisorY = 0; static float tgVisorZoom = 1.0f;
static int tgSlotDaAba(int aba) { return aba == 0 ? 0 : aba <= tgTotalPastas ? aba + 1 : 1; }
static int tgTotalAbas() { return tgTotalPastas + 2; }
static const char* tgNomeDaAba(int aba) { return aba == 0 ? "Todas" : aba <= tgTotalPastas ? tgPastas[aba - 1].nome : "Arquivadas"; }
static void tgBaixarArquivo(int) {}
static bool tgBarraAtiva = false; static float tgBarraPos = 0.3f; static long long tgPrimeiraData = 1700000000;
static pthread_mutex_t tgTrava = PTHREAD_MUTEX_INITIALIZER; static volatile bool tgSujo = false;
static TgDownload tgDownloads[24]; static int tgTotalDownloads = 0, tgFilaSel = 0; static bool tgFilaAberta = false;
static bool tgMenuAberto = false; static int tgMenuSel = 0, tgMenuTotal = 0, tgMenuLinks[6]; static long long tgMenuMensagem = 0;
static bool tgELink(const char* t) { return strncmp(t, "http", 4) == 0 || strncmp(t, "t.me", 4) == 0 || t[0] == '@'; }
static const char* tgNomeAutor(const TgMensagem* m) { return m->minha ? "Voce" : "Maria Souza"; }
''' + v + r'''
static void conversa(long long id, const char* t, const char* u, int n, int foto) {
    TgConversa* c = &tgConversas[tgTotalConversas++]; memset(c, 0, sizeof(*c));
    c->id = id; c->ordens[0] = 1000 - tgTotalConversas; if (tgTotalConversas % 2) c->ordens[2] = 1000 - tgTotalConversas; c->data = 1793943661 - tgTotalConversas * 1700; c->naoLidas = n; c->foto = foto;
    strcpy(c->titulo, t); strcpy(c->ultima, u);
}
static void mensagem(long long autor, bool minha, const char* t, int arq, int w, int h, const char* rotulo, bool fig) {
    TgMensagem* m = &tgMensagens[tgTotalMensagens]; memset(m, 0, sizeof(*m));
    m->id = ++tgTotalMensagens; m->autor = autor; m->minha = minha; m->data = 1793943661 + tgTotalMensagens * 240; m->arquivo = arq; m->imgL = w; m->imgA = h; m->figurinha = fig;
    strcpy(m->texto, t); strcpy(m->rotulo, rotulo);
}
static void imagem(int id, int w, int h, int base) {
    TgArquivo* a = tgArquivo(id); a->pedido = true; a->largura = w; a->altura = h; a->pixels = (uint32_t*)malloc(w * h * 4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) a->pixels[y * w + x] = TG_COR((x * 255 / w + base) & 255, (y * 255 / h) & 255, (base * 3) & 255);
}
static void salvar(uint32_t* p, const char* nome) { char c[400]; snprintf(c, sizeof(c), "SAIDA/%s", nome); stbi_write_png(c, 1920, 1080, 4, p, 1920 * 4); }
int main() {
    FILE* f = fopen("FONTE", "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    unsigned char* b = (unsigned char*)malloc(n); fread(b, 1, n, f); fclose(f);
    stbtt_InitFont(&font, b, 0);
    int acento = stbtt_FindGlyphIndex(&font, 0xE1), cirilico = stbtt_FindGlyphIndex(&font, 0x416), japones = stbtt_FindGlyphIndex(&font, 0x65E5);
    printf("glifos: acento=%d cirilico=%d japones=%d\n", acento, cirilico, japones);
    tgAbrirEmojis("PACOTE");
    printf("emojis %d\n", tgTotalEmojis);
    tgTotalPastas = 3; tgPastas[0].id = 1; strcpy(tgPastas[0].nome, "Família"); tgPastas[1].id = 2; strcpy(tgPastas[1].nome, "Trabalho e estudos"); tgPastas[2].id = 3; strcpy(tgPastas[2].nome, "Jogos 🎮");
    conversa(100, "𝐓𝐞𝐱𝐭𝐨 𝓮𝓼𝓽𝓲𝓵𝓲𝔃𝓪𝓭𝓸 ⓐⓑⓒ ᴘᴇǫᴜᴇɴᴏ", "Ｆｕｌｌ ｗｉｄｔｈ e 🅰🅱 e ★ → ✓ • …", 0, 0);
    conversa(101, "Maria Souza", "Bora jogar hoje à noite? 🎮🔥", 3, 500);
    conversa(-102, "Família ❤️", "João: Foto do almoço de domingo", 0, 0);
    conversa(103, "Ana", "[foto]", 12, 0);
    conversa(-104, "Canal de notícias com um nome bem grande para testar o corte do título", "Atualização importante da semana com bastante texto para cortar", 128, 0);
    conversa(105, "😎 Pedro", "Valeu! 👍🏽", 0, 0);
    conversa(106, "João", "Áudio", 0, 0);
    conversa(107, "Trabalho", "Reunião às 14h", 1, 0);
    conversa(108, "Mãe", "🇧🇷 Ganhamos!", 0, 0);
    conversa(109, "Lucas", "kkkkkkkk", 0, 0);
    conversa(110, "Suporte", "Ticket fechado", 0, 0);
    conversa(111, "Carla", "Até amanhã", 0, 0);
    imagem(500, 96, 96, 40); imagem(501, 560, 420, 120); imagem(502, 256, 256, 200);
    uint32_t* p = (uint32_t*)malloc(1920 * 1080 * 4);
    tgDesenhar(p); salvar(p, "tela_lista.png");

    tgTela = TG_MENSAGENS; tgAberta = -102;
    mensagem(7, false, "Bom dia, família! ☀️", 0, 0, 0, "", false);
    mensagem(7, false, "Olha só o que eu fiz: não é fácil, mas está ótimo. Coração ❤️ e bandeira 🇧🇷 e pele 👍🏽 e família 👨‍👩‍👧‍👦 e 1️⃣", 0, 0, 0, "", false);
    mensagem(9, true, "Ficou muito bom!", 0, 0, 0, "", false);
    mensagem(9, true, "Vou mandar uma mensagem bem longa aqui para ver como a quebra de linha funciona dentro do balão, porque quem escreve no Telegram costuma mandar parágrafos inteiros e o texto precisa respeitar a largura máxima do balão sem sair da tela.", 0, 0, 0, "", false);
    mensagem(8, false, "Foto do almoço de domingo", 501, 560, 420, "", false);
    mensagem(8, false, "", 501, 800, 600, "", false);
    mensagem(8, false, "", 502, 512, 512, "", true);
    mensagem(9, true, "Vídeo da viagem", 501, 640, 360, "Vídeo 1:24", false);
    mensagem(7, false, "Linha um\nLinha dois\n\nLinha quatro", 0, 0, 0, "", false);
    mensagem(7, false, "", 0, 0, 0, "", false); strcpy(tgMensagens[tgTotalMensagens - 1].texto, "🎤 Áudio 0:12");
    tgMensagens[4].midia = TG_M_FOTO; tgMensagens[4].grande = 501; tgMensagens[9].midia = TG_M_AUDIO; tgMensagens[9].grande = 900; tgSelId = 0;
    mensagem(7, false, "😂", 0, 0, 0, "", false);
    mensagem(7, false, "Entra no canal https://t.me/hyperneiva ou fala com @durov agora", 0, 0, 0, "", false);
    { TgMensagem* m = &tgMensagens[tgTotalMensagens - 1]; m->totalLinks = 2; strcpy(m->links[0], "https://t.me/hyperneiva"); strcpy(m->links[1], "https://t.me/durov"); m->grande = 777; }
    tgSelecao = 1;
    tgDesenhar(p); salvar(p, "tela_conversa.png");
    tgMenuAberto = true; tgMenuTotal = 4; tgMenuLinks[0] = -2; tgMenuLinks[1] = 0; tgMenuLinks[2] = 1; tgMenuLinks[3] = -1; tgMenuSel = 0; tgMenuMensagem = tgMensagens[tgTotalMensagens - 1].id;
    tgDesenhar(p); salvar(p, "tela_menu.png"); tgMenuAberto = false;
    tgSelId = 1; tgAudioRodando = true;
    tgDesenhar(p); salvar(p, "tela_rolada.png");
    tgAudioRodando = false; tgSelId = 5; tgBarraAtiva = true;
    tgDesenhar(p); salvar(p, "tela_selecao.png"); tgBarraAtiva = false;
    { char d[16]; tgDataEmTexto(1793943661, d, sizeof(d)); printf("data %s\n", d); tgDataEmTexto(1709251200 + 10800, d, sizeof(d)); printf("bissexto %s\n", d); }

    tgEmBusca = true; strcpy(tgBuscaTexto, "almoço");
    TgResultado* r = &tgResultados[0]; r->conversa = -102; r->mensagem = 5; r->data = 1793943661; strcpy(r->texto, "Foto do almoço de domingo");
    r = &tgResultados[1]; r->conversa = 103; r->mensagem = 0; tgTotalResultados = 2;
    tgDesenhar(p); salvar(p, "tela_busca.png");
    tgEmBusca = false; tgTotalResultados = 0;

    { const char* nomes[4] = { "Filme do fim de semana com um nome bem grande para cortar na fila de downloads.mkv", "Jogo.pkg", "foto.jpg", "erro.zip" };
      int estados[4] = { TG_D_BAIXANDO, TG_D_COPIANDO, TG_D_PRONTO, TG_D_ERRO };
      for (int i = 0; i < 4; i++) { TgDownload* d = &tgDownloads[tgTotalDownloads++]; memset(d, 0, sizeof(*d)); strcpy(d->nome, nomes[i]); d->estado = estados[i]; d->total = 100; d->baixado = 45; d->pkg = i == 1; d->arquivo = i + 1; } }
    tgFilaAberta = true; tgDesenhar(p); salvar(p, "tela_fila.png"); tgFilaAberta = false;
    tgVisor = 501; tgVisorZoom = 1.0f; tgDesenhar(p); salvar(p, "tela_visor.png");
    tgVisorZoom = 3.0f; tgVisorX = 5000; tgDesenhar(p); salvar(p, "tela_visor_zoom.png"); printf("pan limitado %d\n", tgVisorX);
    tgVisor = 0;
    tgTela = TG_CONVERSAS; tgAberta = 0; tgAba = 3; tgSelecao = 0; tgTopo = 0;
    tgDesenhar(p); salvar(p, "tela_aba.png");

    tgTela = TG_TELEFONE; strcpy(tgAviso, "PHONE_NUMBER_INVALID");
    tgDesenhar(p); salvar(p, "tela_entrada.png");
    clock_t t0 = clock(); for (int i = 0; i < 20; i++) { tgTela = TG_MENSAGENS; tgAviso[0] = 0; tgDesenhar(p); }
    printf("redesenho %.1f ms\n", (clock() - t0) * 1000.0 / CLOCKS_PER_SEC / 20);
    return 0;
}
'''.replace("SAIDA", saida).replace("FONTE", fonte).replace("PACOTE", pacote)
f = os.path.join(A, "tg_tela_pc.cpp")
open(f, "w", encoding="utf-8", newline="\n").write(c)
e = os.path.join(A, "tg_tela_pc.exe")
env = {**os.environ, "PATH": r"C:\msys64\mingw64\bin;" + os.environ["PATH"]}
r = subprocess.run([r"C:\msys64\mingw64\bin\g++.exe", "-std=c++17", "-O2", "-w", "-I", C, f, "-o", e], capture_output=True, text=True, env=env)
print(r.stderr[-2500:])
if r.returncode == 0:
    x = subprocess.run([e], capture_output=True, text=True, env=env)
    print(x.stdout, x.stderr[-500:], "codigo", x.returncode)
