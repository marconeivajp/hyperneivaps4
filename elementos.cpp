#include "elementos.h"
#include "graphics.h"
#include "stb_image.h"
#include "editar.h"
#include "personagem_quadros.h"
#include <orbis/libkernel.h>

extern int ctrl1Livre;
extern unsigned char* imgSpriteSheet;
extern int wSprite, hSprite;
extern void carregarSpriteSheetAnimada();

unsigned char* imgElem1 = NULL; int e1W, e1H, e1C;
unsigned char* imgCtrl1 = NULL; int c1W, c1H, c1C;
unsigned char* imgPont1 = NULL; int p1W, p1H, p1C;
bool elemsIniciados = false;

// ====================================================
// VARIÁVEIS PARA A MÁGICA DA FLUIDEZ EXTREMA (LERP)
// ====================================================
float animPontX = -1000.0f;
float animPontY = -1000.0f;
float animCtrlX = -1000.0f;
float animCtrlY = -1000.0f;

// Fluidez reduzida para 0.10f (10%). 
// Isto cria uma desaceleração muito mais suave e longa!
float fluidez = 0.10f;


// ====================================================
// PERSONAGEM CONTROLAVEL
// Parado, andando, pulando, abaixado e atacando, com os quadros da folha de sprites.
// O tempo real manda no movimento, entao a velocidade nao depende do FPS do menu.
// ====================================================
int personagemDirX = 0, personagemDirY = 0;
bool personagemAtacar = false;

enum EstadoPersonagem { PERSONAGEM_PARADO, PERSONAGEM_ANDANDO, PERSONAGEM_PULANDO, PERSONAGEM_ABAIXADO, PERSONAGEM_ATACANDO, PERSONAGEM_VOANDO };
static EstadoPersonagem estadoPersonagem = PERSONAGEM_PARADO;
static float personagemX = -1000.0f, personagemY = -1000.0f, alturaPulo = 0.0f, velocidadePulo = 0.0f, tempoEstado = 0.0f, tempoAtaque = -1.0f, duracaoPulo = 1.0f;
static bool personagemEsquerda = false, noAr = false;

#define QUADROS(lista) ((int)(sizeof(lista) / sizeof(lista[0])))

static void atualizarPersonagem(float passou) {
    float tamanho = (float)ctrl1H;
    float gravidade = tamanho * 14.0f;
    float impulso = tamanho * 5.6f;
    float velocidadeAndar = tamanho * 2.2f;

    if (personagemDirX) personagemEsquerda = personagemDirX < 0;

    if (personagemAtacar && tempoAtaque < 0.0f) tempoAtaque = 0.0f;
    personagemAtacar = false;

    if (ctrl1Livre) { noAr = false; alturaPulo = 0.0f; }   // modo livre: sem gravidade

    if (!ctrl1Livre && !noAr && personagemDirY < 0 && tempoAtaque < 0.0f) {
        noAr = true;
        velocidadePulo = -impulso;
        duracaoPulo = 2.0f * impulso / gravidade;
        tempoEstado = 0.0f;
    }

    bool abaixado = !ctrl1Livre && !noAr && personagemDirY > 0 && personagemDirX == 0;
    bool parado = abaixado || (tempoAtaque >= 0.0f && !noAr);
    if (personagemDirX && !parado) personagemX += personagemDirX * velocidadeAndar * passou;
    if (personagemX < 0.0f) personagemX = 0.0f;
    if (personagemX > 1920.0f - tamanho) personagemX = 1920.0f - tamanho;
    ctrl1X = (int)personagemX;

    if (ctrl1Livre && personagemDirY && !parado) {
        personagemY += personagemDirY * velocidadeAndar * passou;
        if (personagemY < 0.0f) personagemY = 0.0f;
        if (personagemY > 1080.0f - tamanho) personagemY = 1080.0f - tamanho;
    }

    if (noAr) {
        alturaPulo += velocidadePulo * passou;
        velocidadePulo += gravidade * passou;
        if (alturaPulo >= 0.0f) { alturaPulo = 0.0f; noAr = false; tempoEstado = 0.0f; }
    }

    EstadoPersonagem novo = PERSONAGEM_PARADO;
    if (tempoAtaque >= 0.0f) novo = PERSONAGEM_ATACANDO;
    else if (noAr) novo = PERSONAGEM_PULANDO;
    else if (ctrl1Livre && (personagemDirY < 0 || (personagemDirY > 0 && !personagemDirX))) novo = PERSONAGEM_VOANDO;
    else if (abaixado) novo = PERSONAGEM_ABAIXADO;
    else if (personagemDirX) novo = PERSONAGEM_ANDANDO;
    if (novo != estadoPersonagem) { estadoPersonagem = novo; if (novo != PERSONAGEM_PULANDO) tempoEstado = 0.0f; }
    tempoEstado += passou;

    if (tempoAtaque >= 0.0f) {
        tempoAtaque += passou;
        if (tempoAtaque >= QUADROS(quadros_atacar) / 24.0f) tempoAtaque = -1.0f;
    }
}

static const QuadroPersonagem* quadroPersonagem() {
    if (estadoPersonagem == PERSONAGEM_ATACANDO) {
        int i = (int)(tempoAtaque * 24.0f);
        return &quadros_atacar[i < QUADROS(quadros_atacar) ? i : QUADROS(quadros_atacar) - 1];
    }
    if (estadoPersonagem == PERSONAGEM_PULANDO) {
        int i = (int)(tempoEstado / duracaoPulo * QUADROS(quadros_pular));
        return &quadros_pular[i < QUADROS(quadros_pular) ? i : QUADROS(quadros_pular) - 1];
    }
    if (estadoPersonagem == PERSONAGEM_VOANDO) {
        // Modo livre: quadros de subida e de queda do pulo, em repetição.
        int i = (int)(tempoEstado * 10.0f);
        return &quadros_pular[personagemDirY < 0 ? 2 + i % 2 : 8 + i % 3];
    }
    if (estadoPersonagem == PERSONAGEM_ABAIXADO) {
        int i = (int)(tempoEstado * 20.0f);
        return &quadros_abaixar[i < QUADROS(quadros_abaixar) ? i : QUADROS(quadros_abaixar) - 1];
    }
    if (estadoPersonagem == PERSONAGEM_ANDANDO) return &quadros_andar[(int)(tempoEstado * 18.0f) % QUADROS(quadros_andar)];
    return &quadros_parado[(int)(tempoEstado * 8.0f) % QUADROS(quadros_parado)];
}

// A folha desenha o personagem olhando para a esquerda.
static bool personagemEspelhado() { return !personagemEsquerda; }

static void desenharQuadroPersonagem(uint32_t* p, const unsigned char* folha, int folhaW, const QuadroPersonagem* q, float escala, int centroX, int baseY, bool espelhar) {
    int largura = (int)(q->largura * escala), altura = (int)(q->altura * escala);
    int centro = (int)(q->centroX * escala);
    int inicioX = centroX - (espelhar ? largura - centro : centro);
    int inicioY = baseY - altura;

    for (int y = 0; y < altura; y++) {
        int py = inicioY + y;
        if (py < 0 || py >= 1080) continue;
        const unsigned char* linha = folha + ((size_t)(q->y + (int)(y / escala)) * folhaW + q->x) * 4;
        for (int x = 0; x < largura; x++) {
            int px = inicioX + x;
            if (px < 0 || px >= 1920) continue;
            int origem = (int)((espelhar ? largura - 1 - x : x) / escala);
            const unsigned char* c = linha + origem * 4;
            uint32_t rgb = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2];
            if (rgb == PERSONAGEM_COR_FUNDO || rgb == PERSONAGEM_COR_TILE || c[3] == 0) continue;
            p[py * 1920 + px] = 0xFF000000 | rgb;
        }
    }
}

// Quanto o ponteiro anda em direção ao alvo nesta volta. Por tempo: a 0,10 por volta ele
// ficava para trás da seleção sempre que o menu rodava mais devagar.
static float fatorPonteiro(float passou) {
    float fator = passou * 25.0f;
    return fator > 1.0f ? 1.0f : fator;
}

static bool desenharPersonagem(uint32_t* p) {
    if (!imgSpriteSheet) carregarSpriteSheetAnimada();
    if (!imgSpriteSheet || wSprite <= 0) return false;

    static uint64_t ultimoUs = 0;
    uint64_t agora = sceKernelGetProcessTime();
    float passou = ultimoUs ? (float)(agora - ultimoUs) / 1000000.0f : 0.0f;
    if (passou > 0.1f) passou = 0.1f;
    ultimoUs = agora;

    if ((int)personagemX != ctrl1X) personagemX = (float)ctrl1X;   // mudou em Editar
    // O chão é o ctrl1Y salvo. O modo livre anda por cima dele, para a troca de modo voltar ao chão.
    static int chaoAnterior = -100000, modoAnterior = -1;
    if (ctrl1Y != chaoAnterior || ctrl1Livre != modoAnterior) { personagemY = (float)ctrl1Y; chaoAnterior = ctrl1Y; modoAnterior = ctrl1Livre; }
    atualizarPersonagem(passou);

    float escala = (float)ctrl1H / (float)quadros_parado[0].altura;
    int centroX = (int)(personagemX + ctrl1H * 0.5f);
    int baseY = (ctrl1Livre ? (int)personagemY : ctrl1Y) + ctrl1H + (int)alturaPulo;
    desenharQuadroPersonagem(p, imgSpriteSheet, wSprite, quadroPersonagem(), escala, centroX, baseY, personagemEspelhado());
    return true;
}

void inicializarElementos() {
    if (elemsIniciados) return;

    imgElem1 = stbi_load("/data/HyperNeiva/configuracao/imagens/0_Defalt_elemento1.png", &e1W, &e1H, &e1C, 4);
    imgCtrl1 = stbi_load("/data/HyperNeiva/configuracao/imagens/0_Defalt_elemento_controlavel1.png", &c1W, &c1H, &c1C, 4);
    imgPont1 = stbi_load("/data/HyperNeiva/configuracao/imagens/0_Defalt_ponteiro1.png", &p1W, &p1H, &p1C, 4);

    elemsIniciados = true;
}

void desenharElementos(uint32_t* p, int selX, int selY, int selW, int selH) {
    inicializarElementos();

    // 1. Elemento Fixo (Sem movimento, desenha direto)
    if (elem1On && imgElem1 && elem1W > 0 && elem1H > 0) {
        desenharRedimensionado(p, imgElem1, e1W, e1H, elem1W, elem1H, elem1X, elem1Y);
    }

    // 2. Elemento Controlável (Com movimento extra fluido)
    if (ctrl1On && ctrl1H > 0 && desenharPersonagem(p)) {
        // A folha de sprites cuidou do desenho.
    }
    else if (ctrl1On && imgCtrl1 && ctrl1W > 0 && ctrl1H > 0) {
        // Inicializa para não vir voando do zero na primeira vez
        if (animCtrlX == -1000.0f) {
            animCtrlX = ctrl1X;
            animCtrlY = ctrl1Y;
        }

        // Matemática da fluidez
        animCtrlX += (ctrl1X - animCtrlX) * fluidez;
        animCtrlY += (ctrl1Y - animCtrlY) * fluidez;

        // O "+ 0.5f" arredonda os píxeis para evitar tremores visuais
        desenharRedimensionado(p, imgCtrl1, c1W, c1H, ctrl1W, ctrl1H, (int)(animCtrlX + 0.5f), (int)(animCtrlY + 0.5f));
    }

    // 3. Ponteiro (Com acompanhamento ultra suave da seleção)
    if (pont1On && imgPont1 && pont1W > 0 && pont1H > 0) {
        float targetX = pont1X;
        float targetY = pont1Y;

        // Calcula o destino exato para onde o ponteiro tem de ir
        if (pont1Modo == 0) { // Acompanha a Seleção
            if (pont1Lado == 0) { // Esquerda
                targetX += selX - pont1W;
                targetY += selY + (selH / 2) - (pont1H / 2);
            }
            else if (pont1Lado == 1) { // Direita
                targetX += selX + selW;
                targetY += selY + (selH / 2) - (pont1H / 2);
            }
            else if (pont1Lado == 2) { // Cima
                targetX += selX + (selW / 2) - (pont1W / 2);
                targetY += selY - pont1H;
            }
            else if (pont1Lado == 3) { // Baixo
                targetX += selX + (selW / 2) - (pont1W / 2);
                targetY += selY + selH;
            }
        }

        // Inicializa na primeira vez
        if (animPontX == -1000.0f && animPontY == -1000.0f) {
            animPontX = targetX;
            animPontY = targetY;
        }

        // Matemática da fluidez (Interpolação Linear)
        static uint64_t ultimoPontUs = 0;
        uint64_t agoraPontUs = sceKernelGetProcessTime();
        float fatorPont = fatorPonteiro(ultimoPontUs ? (float)(agoraPontUs - ultimoPontUs) / 1000000.0f : 1.0f);
        ultimoPontUs = agoraPontUs;
        animPontX += (targetX - animPontX) * fatorPont;
        animPontY += (targetY - animPontY) * fatorPont;

        // O "+ 0.5f" garante que o gráfico "encaixe" perfeitamente na grelha de píxeis
        desenharRedimensionado(p, imgPont1, p1W, p1H, pont1W, pont1H, (int)(animPontX + 0.5f), (int)(animPontY + 0.5f));

        // 4. Ponteiro Duplo (Segunda instância espelhada)
        if (p1Double) {
            float targetX2 = pont1X;
            float targetY2 = pont1Y;
            bool flipH = false; bool flipV = false;

            if (pont1Modo == 0) { // Acompanha a Seleção
                if (pont1Lado == 0) { // Original na Esquerda -> Duplo na Direita
                    targetX2 += selX + selW;
                    targetY2 += selY + (selH / 2) - (pont1H / 2);
                    flipH = true;
                }
                else if (pont1Lado == 1) { // Original na Direita -> Duplo na Esquerda
                    targetX2 += selX - pont1W;
                    targetY2 += selY + (selH / 2) - (pont1H / 2);
                    flipH = true;
                }
                else if (pont1Lado == 2) { // Original em Cima -> Duplo em Baixo
                    targetX2 += selX + (selW / 2) - (pont1W / 2);
                    targetY2 += selY + selH;
                    flipV = true;
                }
                else if (pont1Lado == 3) { // Original em Baixo -> Duplo em Cima
                    targetX2 += selX + (selW / 2) - (pont1W / 2);
                    targetY2 += selY - pont1H;
                    flipV = true;
                }
            }

            // O segundo ponteiro usa a mesma animação/fluidez, mas com o offset espelhado
            // (Para simplicidade, vamos desenhar o segundo na posição alvo espelhada relativa à animação atual ou calcular uma segunda animação)
            // Para um efeito visual limpo, vamos calcular o offset relativo à animação do primeiro
            float relX = animPontX - targetX;
            float relY = animPontY - targetY;
            
            desenharRedimensionadoEspelhado(p, imgPont1, p1W, p1H, pont1W, pont1H, (int)(targetX2 + relX + 0.5f), (int)(targetY2 + relY + 0.5f), flipH, flipV);
        }
    }
}