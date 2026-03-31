#include "informacao.h"
#include "menu.h"
#include "graphics.h"
#include <string.h>

extern MenuLevel menuAtual;
extern int totalItens;
extern int sel;

void preencherMenuInformacao() {
    menuAtual = MENU_INFORMACAO;
    totalItens = 0;
    sel = 0;
}

void renderizarInformacao(uint32_t* p) {
    for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF111111;

    desenharTexto(p, "SISTEMA HYPER NEIVA (PS4)", 50, 100, 100, 0xFF00AAFF);
    desenharTexto(p, "================================================", 30, 100, 150, 0xFFFFFFFF);
    desenharTexto(p, "Versao Atual: 1.0.0 PRO", 35, 100, 250, 0xFFFFFFFF);
    desenharTexto(p, "Desenvolvedor Original: Marcao / Hyper Neiva", 35, 100, 320, 0xFFFFFFFF);

    desenharTexto(p, "Recursos Inclusos:", 35, 100, 420, 0xFF00FF00);
    desenharTexto(p, "- Explorador de Arquivos Local com Painel Duplo", 30, 150, 480, 0xFFDDDDDD);
    desenharTexto(p, "- FTP Inteligente Avancado (Upload/Download)", 30, 150, 530, 0xFFDDDDDD);
    desenharTexto(p, "- Leitor Multimidia (Audio, Imagem, Arquivos de Texto)", 30, 150, 580, 0xFFDDDDDD);
    desenharTexto(p, "- Instalador Nativo de PKG", 30, 150, 630, 0xFFDDDDDD);
    desenharTexto(p, "- Personalizacao de Interface em Tempo Real", 30, 150, 680, 0xFFDDDDDD);

    desenharTexto(p, "Obrigado por utilizar o Hyper Neiva!", 35, 100, 850, 0xFF00AAFF);
    desenharTexto(p, "[O] Voltar para a Tela Inicial", 30, 100, 1000, 0xFFAAAAAA);
}