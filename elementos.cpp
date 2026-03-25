#include "elementos.h"
#include "graphics.h"
#include "stb_image.h"
#include "editar.h"

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
    if (ctrl1On && imgCtrl1 && ctrl1W > 0 && ctrl1H > 0) {
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
        animPontX += (targetX - animPontX) * fluidez;
        animPontY += (targetY - animPontY) * fluidez;

        // O "+ 0.5f" garante que o gráfico "encaixe" perfeitamente na grelha de píxeis
        desenharRedimensionado(p, imgPont1, p1W, p1H, pont1W, pont1H, (int)(animPontX + 0.5f), (int)(animPontY + 0.5f));
    }
}