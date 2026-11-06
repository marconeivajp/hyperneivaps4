"""Extrai a parte de JSON do bloco e testa no PC com respostas no formato do TDLib."""
import os
import subprocess

A = os.path.dirname(os.path.abspath(__file__))
d = open(os.path.join(A, "tg_nucleo.cpp"), encoding="utf-8").read()
j = d[d.index("// TG_JSON_INICIO"):d.index("// TG_JSON_FIM")]
c = r'''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
enum TgMidia { TG_M_NADA, TG_M_FOTO, TG_M_VIDEO, TG_M_AUDIO };
struct TgMensagem { long long id, autor, data; bool minha, figurinha; int arquivo, imgL, imgA, grande, midia, totalLinks, alturaCache[2]; char rotulo[32]; char resumo[48]; char arquivoNome[100]; char links[4][200]; char texto[1024]; };
struct TgPasta { int id; char nome[64]; };
''' + j + r'''
#define IGUAL(a, b) do { if (strcmp((a), (b))) { printf("FALHOU [%s] [%s] linha %d\n", (a), (b), __LINE__); erros++; } } while (0)
#define CERTO(x) do { if (!(x)) { printf("FALHOU %s linha %d\n", #x, __LINE__); erros++; } } while (0)
int main() {
    int erros = 0;
    TgMensagem m; long long c; char t[256]; long long n; char r[300];
    const char* canal = "{\"@type\":\"message\",\"id\":1048576,\"sender_id\":{\"@type\":\"messageSenderChat\",\"chat_id\":-1001234},\"chat_id\":-1005678,\"is_outgoing\":false,\"date\":1793943661,\"content\":{\"@type\":\"messageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"Ol\xc3\xa1 \\\"mundo\\\"\\nnova \xf0\x9f\x98\x80 fim\",\"entities\":[]}}}";
    CERTO(tgLerMensagem(canal, NULL, &m, &c));
    CERTO(m.id == 1048576); CERTO(m.autor == -1001234); CERTO(c == -1005678); CERTO(!m.minha); CERTO(m.data == 1793943661);
    IGUAL(m.texto, "Ol\xc3\xa1 \"mundo\"\nnova \xf0\x9f\x98\x80 fim");
    tgResumir(&m, r, sizeof(r)); IGUAL(r, "Ol\xc3\xa1 \"mundo\" nova \xf0\x9f\x98\x80 fim");

    const char* foto = "{\"@type\":\"message\",\"id\":7,\"sender_id\":{\"@type\":\"messageSenderUser\",\"user_id\":42},\"chat_id\":42,\"is_outgoing\":true,\"reply_to\":{\"@type\":\"messageReplyToMessage\",\"content\":{\"@type\":\"messageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"velha\"}}},\"content\":{\"@type\":\"messagePhoto\",\"photo\":{\"@type\":\"photo\",\"sizes\":[{\"@type\":\"photoSize\",\"type\":\"m\",\"photo\":{\"@type\":\"file\",\"id\":11,\"size\":9},\"width\":320,\"height\":240,\"progressive_sizes\":[]},{\"@type\":\"photoSize\",\"type\":\"x\",\"photo\":{\"@type\":\"file\",\"id\":12,\"size\":9},\"width\":800,\"height\":600,\"progressive_sizes\":[]},{\"@type\":\"photoSize\",\"type\":\"y\",\"photo\":{\"@type\":\"file\",\"id\":13,\"size\":9},\"width\":1280,\"height\":960,\"progressive_sizes\":[]}]},\"caption\":{\"@type\":\"formattedText\",\"text\":\"Almo\xc3\xa7o\",\"entities\":[]}}}";
    CERTO(tgLerMensagem(foto, NULL, &m, &c));
    CERTO(m.autor == 42); CERTO(c == 42); CERTO(m.minha); IGUAL(m.texto, "Almo\xc3\xa7o"); CERTO(m.arquivo == 12); CERTO(m.imgL == 800); CERTO(m.imgA == 600); CERTO(m.grande == 13); CERTO(m.midia == TG_M_FOTO);
    tgResumir(&m, r, sizeof(r)); IGUAL(r, "Voc\xc3\xaa: Almo\xc3\xa7o");

    const char* grande = "{\"@type\":\"message\",\"id\":70,\"chat_id\":1,\"content\":{\"@type\":\"messagePhoto\",\"photo\":{\"sizes\":[{\"@type\":\"photoSize\",\"type\":\"y\",\"photo\":{\"@type\":\"file\",\"id\":21},\"width\":1280,\"height\":960}]},\"caption\":{\"@type\":\"formattedText\",\"text\":\"\"}}}";
    tgLerMensagem(grande, NULL, &m, &c); CERTO(m.arquivo == 21); tgResumir(&m, r, sizeof(r)); CERTO(strstr(r, "Foto") != NULL);

    const char* video = "{\"@type\":\"message\",\"id\":8,\"chat_id\":1,\"content\":{\"@type\":\"messageVideo\",\"video\":{\"@type\":\"video\",\"duration\":84,\"width\":1280,\"height\":720,\"thumbnail\":{\"@type\":\"thumbnail\",\"format\":{\"@type\":\"thumbnailFormatJpeg\"},\"width\":320,\"height\":180,\"file\":{\"@type\":\"file\",\"id\":31}},\"video\":{\"@type\":\"file\",\"id\":32}},\"caption\":{\"@type\":\"formattedText\",\"text\":\"\"}}}";
    tgLerMensagem(video, NULL, &m, &c); CERTO(m.arquivo == 31); CERTO(m.imgL == 320); CERTO(m.grande == 32); CERTO(m.midia == TG_M_VIDEO); IGUAL(m.rotulo, "V\xc3\xad" "deo 1:24");

    const char* fig = "{\"@type\":\"message\",\"id\":9,\"chat_id\":1,\"content\":{\"@type\":\"messageSticker\",\"sticker\":{\"@type\":\"sticker\",\"id\":\"5\",\"set_id\":\"6\",\"width\":512,\"height\":500,\"emoji\":\"\xf0\x9f\x98\x80\",\"format\":{\"@type\":\"stickerFormatWebp\"},\"full_type\":{},\"thumbnail\":{\"@type\":\"thumbnail\",\"format\":{\"@type\":\"thumbnailFormatWebp\"},\"width\":128,\"height\":125,\"file\":{\"@type\":\"file\",\"id\":41}},\"sticker\":{\"@type\":\"file\",\"id\":42}},\"is_premium\":false}}";
    tgLerMensagem(fig, NULL, &m, &c); CERTO(m.arquivo == 42); CERTO(m.figurinha); CERTO(m.imgL == 512); CERTO(m.imgA == 500); IGUAL(m.resumo, "\xf0\x9f\x98\x80 Figurinha");
    const char* tgs = "{\"@type\":\"message\",\"id\":10,\"chat_id\":1,\"content\":{\"@type\":\"messageSticker\",\"sticker\":{\"@type\":\"sticker\",\"id\":\"5\",\"width\":512,\"height\":512,\"emoji\":\"x\",\"format\":{\"@type\":\"stickerFormatTgs\"},\"thumbnail\":{\"@type\":\"thumbnail\",\"format\":{\"@type\":\"thumbnailFormatWebp\"},\"width\":128,\"height\":128,\"file\":{\"@type\":\"file\",\"id\":51}},\"sticker\":{\"@type\":\"file\",\"id\":52}}}}";
    tgLerMensagem(tgs, NULL, &m, &c); CERTO(m.arquivo == 51); CERTO(m.figurinha); CERTO(m.imgL == 512);

    const char* voz = "{\"@type\":\"message\",\"id\":11,\"chat_id\":1,\"content\":{\"@type\":\"messageVoiceNote\",\"voice_note\":{\"@type\":\"voiceNote\",\"duration\":12,\"voice\":{\"@type\":\"file\",\"id\":61}}}}";
    tgLerMensagem(voz, NULL, &m, &c); CERTO(strstr(m.texto, "0:12") != NULL); CERTO(m.grande == 61); CERTO(m.midia == TG_M_AUDIO);
    const char* doc = "{\"@type\":\"message\",\"id\":12,\"chat_id\":1,\"content\":{\"@type\":\"messageDocument\",\"document\":{\"@type\":\"document\",\"file_name\":\"nota.pdf\",\"mime_type\":\"application/pdf\",\"document\":{\"@type\":\"file\",\"id\":71}}}}";
    tgLerMensagem(doc, NULL, &m, &c); CERTO(strstr(m.texto, "nota.pdf") != NULL); CERTO(m.midia == TG_M_NADA); CERTO(m.grande == 71); IGUAL(m.arquivoNome, "nota.pdf");
    const char* docimg = "{\"@type\":\"message\",\"id\":14,\"chat_id\":1,\"content\":{\"@type\":\"messageDocument\",\"document\":{\"@type\":\"document\",\"file_name\":\"a.png\",\"mime_type\":\"image/png\",\"document\":{\"@type\":\"file\",\"id\":72}}}}";
    tgLerMensagem(docimg, NULL, &m, &c); CERTO(m.grande == 72); CERTO(m.midia == TG_M_FOTO);
    CERTO(m.grande == 72); IGUAL(m.arquivoNome, "a.png");
    const char* so = "{\"@type\":\"message\",\"id\":20,\"chat_id\":1,\"content\":{\"@type\":\"messageAnimatedEmoji\",\"animated_emoji\":{\"@type\":\"animatedEmoji\",\"sticker\":{\"@type\":\"sticker\",\"emoji\":\"\xf0\x9f\x98\x82\"}},\"emoji\":\"\xf0\x9f\x98\x82\"}}";
    tgLerMensagem(so, NULL, &m, &c); IGUAL(m.texto, "\xf0\x9f\x98\x82");
    const char* lk = "{\"@type\":\"message\",\"id\":21,\"chat_id\":1,\"content\":{\"@type\":\"messageText\",\"text\":{\"@type\":\"formattedText\",\"text\":\"Veja (https://t.me/canal/12), t.me/outro, @durov e @ab fim\",\"entities\":[{\"@type\":\"textEntity\",\"offset\":0,\"length\":4,\"type\":{\"@type\":\"textEntityTypeTextUrl\",\"url\":\"https://exemplo.com/a\"}}]}},\"reply_markup\":{\"@type\":\"replyMarkupInlineKeyboard\",\"rows\":[[{\"@type\":\"inlineKeyboardButton\",\"text\":\"Abrir\",\"type\":{\"@type\":\"inlineKeyboardButtonTypeUrl\",\"url\":\"https://t.me/botao\"}}]]}}";
    tgLerMensagem(lk, NULL, &m, &c); CERTO(m.totalLinks == 4);
    IGUAL(m.links[0], "https://exemplo.com/a"); IGUAL(m.links[1], "https://t.me/canal/12"); IGUAL(m.links[2], "https://t.me/outro"); IGUAL(m.links[3], "https://t.me/durov");
    CERTO(tgELink("https://x.y")); CERTO(tgELink("@durov")); CERTO(!tgELink("@ab")); CERTO(!tgELink("email@x.com"));
    const char* novo = "{\"@type\":\"message\",\"id\":13,\"chat_id\":1,\"content\":{\"@type\":\"messageGiftNovo\"}}";
    tgLerMensagem(novo, NULL, &m, &c); CERTO(strstr(m.texto, "suportada") != NULL);

    const char* pos = "{\"@type\":\"updateChatPosition\",\"chat_id\":-100,\"position\":{\"@type\":\"chatPosition\",\"list\":{\"@type\":\"chatListMain\"},\"order\":\"7234567890123456789\",\"is_pinned\":false}}";
    CERTO(tgTipo(pos, "updateChatPosition")); CERTO(!tgTipo(pos, "updateChat"));
    int tipo, pasta; const char* q;
    q = tgProximaPosicao(pos, NULL, &tipo, &pasta, &n); CERTO(q && tipo == 0 && n == 7234567890123456789LL);
    CERTO(!tgProximaPosicao(q, NULL, &tipo, &pasta, &n));
    const char* varias = "{\"positions\":[{\"@type\":\"chatPosition\",\"list\":{\"@type\":\"chatListArchive\"},\"order\":\"5\"},{\"@type\":\"chatPosition\",\"list\":{\"@type\":\"chatListFolder\",\"chat_folder_id\":3},\"order\":\"9\"}]}";
    q = tgProximaPosicao(varias, NULL, &tipo, &pasta, &n); CERTO(q && tipo == 1 && n == 5);
    q = tgProximaPosicao(q, NULL, &tipo, &pasta, &n); CERTO(q && tipo == 2 && pasta == 3 && n == 9);
    TgPasta pastas[4];
    const char* fold = "{\"@type\":\"updateChatFolders\",\"chat_folders\":[{\"@type\":\"chatFolderInfo\",\"id\":2,\"name\":{\"@type\":\"chatFolderName\",\"text\":{\"@type\":\"formattedText\",\"text\":\"Fam\xc3\xadlia\",\"entities\":[]},\"animate_custom_emoji\":false},\"icon\":{}},{\"@type\":\"chatFolderInfo\",\"id\":7,\"name\":{\"@type\":\"chatFolderName\",\"text\":{\"@type\":\"formattedText\",\"text\":\"Jogos\",\"entities\":[]}}}],\"main_chat_list_position\":0}";
    CERTO(tgLerPastas(fold, pastas, 4) == 2); CERTO(pastas[0].id == 2); IGUAL(pastas[0].nome, "Fam\xc3\xadlia"); CERTO(pastas[1].id == 7); IGUAL(pastas[1].nome, "Jogos");
    long long ids[8];
    CERTO(tgLerIds("{\"@type\":\"chats\",\"total_count\":3,\"chat_ids\":[12,-100345,7],\"@extra\":\"busca\"}", "\"chat_ids\":[", ids, 8) == 3); CERTO(ids[1] == -100345);
    CERTO(tgLerIds("{\"chat_ids\":[]}", "\"chat_ids\":[", ids, 8) == 0);

    const char* titulo = "{\"@type\":\"updateNewChat\",\"chat\":{\"@type\":\"chat\",\"id\":-55,\"type\":{\"@type\":\"chatTypeSupergroup\"},\"title\":\"Fam\xc3\xadlia \\u00c7\\ud83d\\ude00!\",\"photo\":{\"@type\":\"chatPhotoInfo\",\"small\":{\"@type\":\"file\",\"id\":77}}}}";
    CERTO(tgNumero(titulo, NULL, "\"chat\":{\"@type\":\"chat\",\"id\":", &n)); CERTO(n == -55);
    CERTO(tgNumero(titulo, NULL, "\"small\":{\"@type\":\"file\",\"id\":", &n)); CERTO(n == 77);
    tgTexto(titulo, NULL, "\"title\":\"", t, sizeof(t)); IGUAL(t, "Fam\xc3\xadlia \xc3\x87\xf0\x9f\x98\x80!");
    char pequeno[5]; tgTexto(titulo, NULL, "\"title\":\"", pequeno, sizeof(pequeno)); IGUAL(pequeno, "Fam");

    const uint16_t e[] = { 'O', 'l', 0xE1, ' ', '"', '\\', 0xD83D, 0xDE00, 0 };
    char s[80]; tgEscapar(e, s, sizeof(s)); IGUAL(s, "Ol\\u00e1 \\\"\\\\\\ud83d\\ude00");
    uint16_t longo[600]; for (int i = 0; i < 599; i++) longo[i] = 0xE1; longo[599] = 0;
    static char curto[64]; tgEscapar(longo, curto, sizeof(curto)); CERTO(strlen(curto) < 64);
    printf(erros ? "%d ERROS\n" : "TUDO CERTO\n", erros);
    return erros;
}
'''
f = os.path.join(A, "tg_teste.cpp")
open(f, "w", encoding="utf-8", newline="\n").write(c)
e = os.path.join(A, "tg_teste.exe")
r = subprocess.run([r"C:\msys64\mingw64\bin\g++.exe", "-std=c++17", "-Wall", "-fsanitize=undefined" if False else "-O1", f, "-o", e], capture_output=True, text=True, env={**os.environ, "PATH": r"C:\msys64\mingw64\bin;" + os.environ["PATH"]})
print(r.stderr[-1500:])
if r.returncode == 0:
    x = subprocess.run([e], capture_output=True, text=True, env={**os.environ, "PATH": r"C:\msys64\mingw64\bin;" + os.environ["PATH"]})
    print(x.stdout, "codigo", x.returncode)
