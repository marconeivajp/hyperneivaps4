"""Roda o codigo real do menu GitHub contra a API de verdade; so a rede do PS4 vira curl."""
import os
import subprocess

P = r"C:\Users\Marco\source\repos\teste3\teste3"
os.environ["PATH"] = r"C:\msys64\mingw64\bin;" + os.environ["PATH"]
t = open(os.path.join(P, "baixar_repositorio.cpp"), "rb").read().decode("utf-8", "surrogateescape").replace("\r\n", "\n")
ini = t.index("#define GITHUB_RESPOSTA_MAX")
fim = t.index("static volatile bool githubEsperandoTeclado")
codigo = t[ini:fim].replace("/data/HyperNeiva/configuracao/github_favoritos", "out/github_favoritos").replace('sceKernelMkdir("/data/HyperNeiva/configuracao", 0777);', "")
a = codigo.index("static char* githubBaixarJson(")
b = codigo.index("static void githubAvisarErro(")
codigo = codigo[:a] + r'''
static char* githubBaixarJson(const char* url, int* status) {
    char cmd[1200]; snprintf(cmd, sizeof(cmd), "curl.exe -s -A HyperNeiva/1.0 -H \"Accept: application/vnd.github+json\" -w \"\\n@@%%{http_code}\" \"%s\"", url);
    FILE* f = popen(cmd, "rb"); if (!f) { *status = -1; return NULL; }
    size_t cap = 4 * 1024 * 1024, n = 0; char* d = (char*)malloc(cap + 1); size_t r;
    while ((r = fread(d + n, 1, cap - n, f)) > 0) n += r;
    pclose(f); d[n] = 0; char* m = strstr(d, "\n@@"); *status = m ? atoi(m + 3) : -1; if (m) *m = 0; return d;
}
''' + codigo[b:]

teste = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <strings.h>
enum MenuLevel { ROOT, MENU_BAIXAR_GITHUB, MENU_BAIXAR_GITHUB_REPOS, MENU_BAIXAR_GITHUB_RELEASES, MENU_BAIXAR_GITHUB_ARQUIVOS };
char nomes[3000][256]; char linksAtuais[3000][1024]; int totalItens, sel, off; MenuLevel menuAtual; char msgStatus[128]; int msgTimer;
void atualizarBarra(float) {}
const char* listaOpcoes[150]; int mapOpcoes[150], totalOpcoes, selOpcao; bool showOpcoes;
void iniciarDownload(const char* u) { printf("    baixar: %s\n", u); }
''' + codigo + r'''
static const char* menus[] = { "ROOT", "GITHUB", "REPOS", "RELEASES", "ARQUIVOS" };
static void mostrar(int limite) { printf("  [%s] %d itens | msg: %s\n", menus[menuAtual], totalItens, msgStatus); for (int i = 0; i < totalItens && i < limite; i++) printf("    %s\n", nomes[i]); msgStatus[0] = 0; }
static int ok = 0, ruim = 0; static void checa(const char* n, bool c) { printf("[%s] %s\n", c ? " OK " : "FALHA", n); if (c) ok++; else ruim++; }
static bool tem(const char* r) { for (int i = 1; i < totalItens; i++) if (!strcmp(nomes[i], r)) return true; return false; }
static void favTri() { acaoTriangle_Github(); if (showOpcoes) acaoOpcaoGithub(0); }
static int favoritos() {
    remove("out/github_favoritos.txt");
    preencherMenuGithub(); checa("primeira vez cria com os 8 e Hyper Neiva em primeiro", totalItens == 10 && tem("iHaiDeeZ/M33") && !strcmp(nomes[1], "marconeivajp/hyperneivaps4"));
    FILE* a = fopen("out/github_favoritos.txt", "r"); checa("arquivo txt criado", a != NULL); if (a) fclose(a);
    menuAtual = MENU_BAIXAR_GITHUB_REPOS; strcpy(linksAtuais[sel = 0], "dono/novo-repo"); favTri(); checa("triangulo salva", !strcmp(msgStatus, "SALVO NOS FAVORITOS"));
    preencherMenuGithub(); checa("aparece na lista", tem("dono/novo-repo") && totalItens == 11);
    menuAtual = MENU_BAIXAR_GITHUB_RELEASES; strcpy(githubRepoAtual, "outro/repo"); favTri(); preencherMenuGithub(); checa("salva pela tela de releases", tem("outro/repo"));
    sel = 1; favTri(); checa("Hyper Neiva nao sai da lista", tem("marconeivajp/hyperneivaps4")); sel = 2; favTri(); checa("triangulo remove da lista", !tem("iHaiDeeZ/M33") && menuAtual == MENU_BAIXAR_GITHUB && sel == 2);
    a = fopen("out/github_favoritos.txt", "a"); fprintf(a, "\n# comentario\nhttps://github.com/iHaiDeeZ/M33/releases/tag/1.04\r\nhttps://github.com/iHaiDeeZ/Crema\nlinha invalida\nPhoenixx1202/PS4SX2\n"); fclose(a);
    preencherMenuGithub(); checa("aceita link editado no txt", tem("iHaiDeeZ/M33"));
    int repetidos = 0; for (int i = 1; i < totalItens; i++) if (!strcmp(nomes[i], "iHaiDeeZ/Crema")) repetidos++;
    checa("nao repete nem le lixo", repetidos == 1 && !tem("linha invalida") && !tem("# comentario"));
    for (int i = totalItens - 1; i >= 2; i--) { sel = 2; favTri(); }
    preencherMenuGithub(); checa("sem favoritos sobra o Hyper Neiva", totalItens == 2);
    char r[160]; checa("palavra nao vira repo", !githubNormalizarRepo("dolphin ps4", r, 160)); checa("dono/repo vale", githubNormalizarRepo("a/b-c", r, 160) && !strcmp(r, "a/b-c"));
    githubAbrirReleases("iHaiDeeZ/M33"); githubAbrirRelease(0); githubBaixarArquivo(0); checa("usa API e guarda o nome real", strstr(linksAtuais[0], "api.github.com/repos/iHaiDeeZ/M33/releases/assets/") && !strcmp(githubNomeDownload, "M33-v1.04.pkg"));
    githubAbrirReleases("marconeivajp/hyperneivaps4"); printf("    Hyper Neiva: %d releases, primeira: %s\n", totalItens, nomes[0]); checa("releases do Hyper Neiva abrem", menuAtual == MENU_BAIXAR_GITHUB_RELEASES && totalItens >= 4);
    preencherMenuGithub(); menuAtual = MENU_BAIXAR_GITHUB_REPOS; strcpy(linksAtuais[sel = 0], "teste/menu"); acaoTriangle_Github();
    checa("triangulo abre opcao adicionar", showOpcoes && totalOpcoes == 2 && !strcmp(listaOpcoes[0], "adicionar aos favoritos"));
    acaoOpcaoGithub(1); preencherMenuGithub(); checa("cancelar nao salva", !showOpcoes && !tem("teste/menu"));
    menuAtual = MENU_BAIXAR_GITHUB_REPOS; strcpy(linksAtuais[sel = 0], "teste/menu"); acaoTriangle_Github(); acaoOpcaoGithub(0); menuAtual = MENU_BAIXAR_GITHUB_REPOS; strcpy(linksAtuais[sel = 0], "teste/menu"); acaoTriangle_Github();
    checa("depois vira remover", !strcmp(listaOpcoes[0], "remover dos favoritos")); showOpcoes = false;
    printf("favoritos: %d ok, %d falhas\n", ok, ruim); return ruim;
}
int main(int argc, char** argv) {
    if (argc > 1) return favoritos();
    preencherMenuGithub(); mostrar(12);
    const char* repos[] = { "iHaiDeeZ/M33", "iHaiDeeZ/Crema", "iHaiDeeZ/Resonance", "iHaiDeeZ/DolphinPS4", "arturofuentes1988/EmuSuNi", "Phoenixx1202/PS4SX2", "RoberixStudios/rpcs3-port-for-ps4", "alechurri/nx-on-orbis" };
    for (int i = 0; i < 8; i++) {
        printf("\n== %s\n", repos[i]); githubAbrirReleases(repos[i]); mostrar(2);
        if (menuAtual == MENU_BAIXAR_GITHUB_RELEASES) { githubAbrirRelease(0); mostrar(3); printf("    link: %s\n", linksAtuais[0]); mostrarGithubReleases(); printf("    volta da cache: %d itens\n", totalItens); }
    }
    printf("\n== pesquisa: dolphin ps4\n"); githubPesquisar("dolphin ps4"); mostrar(5);
    printf("\n== pesquisa com acento e simbolo: emulador c++\n"); githubPesquisar("emulador c++"); mostrar(3);
    printf("\n== link colado\n"); githubPesquisar("https://github.com/iHaiDeeZ/M33/releases/tag/1.04"); mostrar(2);
    printf("\n== dono/repo\n"); githubPesquisar("alechurri/nx-on-orbis"); mostrar(2);
    printf("\n== inexistente\n"); menuAtual = MENU_BAIXAR_GITHUB; githubAbrirReleases("usuario-inexistente-xyz/nada"); mostrar(1);
    return 0;
}
'''
open("teste_github.cpp", "w", encoding="utf-8", errors="surrogateescape").write(teste)
r = subprocess.run([r"C:\msys64\mingw64\bin\g++.exe", "-std=gnu++17", "-O1", "-g", "teste_github.cpp", "-o", "teste_github.exe"], capture_output=True, text=True)
print(r.stderr[-1800:])
print("compilou" if r.returncode == 0 else "FALHOU")
