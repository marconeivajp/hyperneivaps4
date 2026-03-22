#include "elementos.h"
#include "graphics.h"
#include "stb_image.h"
#include "editar.h"

unsigned char* imgElem1 = NULL; int e1W, e1H, e1C;
unsigned char* imgCtrl1 = NULL; int c1W, c1H, c1C;
unsigned char* imgPont1 = NULL; int p1W, p1H, p1C;
bool elemsIniciados = false;

void inicializarElementos() {
    if (elemsIniciados) return;
    imgElem1 = stbi_load("/app0/assets/images/0_Defalt_elemento1.png", &e1W, &e1H, &e1C, 4);
    imgCtrl1 = stbi_load("/app0/assets/images/0_Defalt_elemento_controlavel1.png", &c1W, &c1H, &c1C, 4);
    imgPont1 = stbi_load("/app0/assets/images/0_Defalt_ponteiro1.png", &p1W, &p1H, &p1C, 4);
    elemsIniciados = true;
}

void desenharElementos(uint32_t* p, int selX, int selY, int selW, int selH) {
    inicializarElementos();

    if (elem1On && imgElem1) {
        desenharRedimensionado(p, imgElem1, e1W, e1H, elem1W, elem1H, elem1X, elem1Y);
    }
    if (ctrl1On && imgCtrl1) {
        desenharRedimensionado(p, imgCtrl1, c1W, c1H, ctrl1W, ctrl1H, ctrl1X, ctrl1Y);
    }
    if (pont1On && imgPont1) {
        int drawX = pont1X;
        int drawY = pont1Y;
        if (pont1Modo == 0) { // Acompanha a Seleção
            if (pont1Lado == 0) { // Esquerda
                drawX += selX - pont1W;
                drawY += selY + (selH / 2) - (pont1H / 2);
            }
            else if (pont1Lado == 1) { // Direita
                drawX += selX + selW;
                drawY += selY + (selH / 2) - (pont1H / 2);
            }
            else if (pont1Lado == 2) { // Cima
                drawX += selX + (selW / 2) - (pont1W / 2);
                drawY += selY - pont1H;
            }
            else if (pont1Lado == 3) { // Baixo
                drawX += selX + (selW / 2) - (pont1W / 2);
                drawY += selY + selH;
            }
        }
        desenharRedimensionado(p, imgPont1, p1W, p1H, pont1W, pont1H, drawX, drawY);
    }
}