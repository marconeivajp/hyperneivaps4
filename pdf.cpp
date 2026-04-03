#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <orbis/libkernel.h> 
#include <orbis/Pad.h> 

#include "pdf.h"

// ==============================================================
// MOTOR MUPDF 100% NATIVO PARA LER PDFS NO PS4
// ==============================================================
#include <mupdf/fitz.h>

fz_context* ctx_pdf = NULL;
fz_document* doc_pdf = NULL;

extern char msgStatus[128];
extern int msgTimer;

bool visualizandoPDF = false;
char caminhoLeituraAtual[512] = "";

int pdfPaginaAtual = 1;
int pdfTotalPaginas = 1;
float pdfZoom = 1.0f;
int pdfOffsetX = 0;
int pdfOffsetY = 0;

unsigned char* imgPaginaAtual = NULL;
int pdfImgW = 0;
int pdfImgH = 0;

void fecharPDF() {
    visualizandoPDF = false;
    strcpy(caminhoLeituraAtual, "");

    if (imgPaginaAtual) {
        free(imgPaginaAtual);
        imgPaginaAtual = NULL;
    }

    if (doc_pdf) { fz_drop_document(ctx_pdf, doc_pdf); doc_pdf = NULL; }
    if (ctx_pdf) { fz_drop_context(ctx_pdf); ctx_pdf = NULL; }

    sprintf(msgStatus, "LEITOR DE PDF FECHADO");
    msgTimer = 90;
}

void carregarPaginaPDFNativo(int pagina) {
    if (pagina < 1 || pagina > pdfTotalPaginas) return;

    if (imgPaginaAtual) {
        free(imgPaginaAtual);
        imgPaginaAtual = NULL;
    }

    // Carrega a página com resolução dobrada para não serrilhar
    fz_matrix ctm = fz_scale(2.0f, 2.0f);
    fz_page* page = fz_load_page(ctx_pdf, doc_pdf, pagina - 1);

    // Converte a página para uma matriz de pixels RGB
    fz_pixmap* pix = fz_new_pixmap_from_page_number(ctx_pdf, doc_pdf, pagina - 1, ctm, fz_device_rgb(ctx_pdf), 0);

    if (pix) {
        pdfImgW = pix->w;
        pdfImgH = pix->h;
        int numCanais = pix->n;

        imgPaginaAtual = (unsigned char*)malloc(pdfImgW * pdfImgH * 4);
        unsigned char* samples = pix->samples;

        // O PS4 precisa da ordem RGBA para renderizar a imagem na tela
        for (int i = 0; i < pdfImgW * pdfImgH; i++) {
            imgPaginaAtual[i * 4 + 0] = samples[i * numCanais + 0]; // R
            imgPaginaAtual[i * 4 + 1] = samples[i * numCanais + 1]; // G
            imgPaginaAtual[i * 4 + 2] = samples[i * numCanais + 2]; // B
            imgPaginaAtual[i * 4 + 3] = (numCanais == 4) ? samples[i * numCanais + 3] : 255; // A
        }
        fz_drop_pixmap(ctx_pdf, pix);

        pdfPaginaAtual = pagina;

        // CÁLCULO DE ZOOM INTELIGENTE: Faz a página caber exatamente na altura da TV (deixando uma bordinha de 20px)
        pdfZoom = 1040.0f / (float)pdfImgH;
        pdfOffsetX = 0;
        pdfOffsetY = 0;

        sprintf(msgStatus, "PAGINA %d DE %d", pdfPaginaAtual, pdfTotalPaginas);
    }
    else {
        sprintf(msgStatus, "ERRO: RENDERIZACAO MUPDF FALHOU.");
    }
    fz_drop_page(ctx_pdf, page);
    msgTimer = 240;
}

void abrirLeitor(const char* caminho) {
    strcpy(caminhoLeituraAtual, caminho);
    visualizandoPDF = true;

    sprintf(msgStatus, "ABRINDO PDF..."); msgTimer = 180;

    ctx_pdf = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!ctx_pdf) { sprintf(msgStatus, "ERRO: FALHA AO INICIAR CONTEXTO MUPDF"); visualizandoPDF = false; return; }

    fz_register_document_handlers(ctx_pdf);

    fz_try(ctx_pdf) {
        doc_pdf = fz_open_document(ctx_pdf, caminho);
        pdfTotalPaginas = fz_count_pages(ctx_pdf, doc_pdf);
    }
    fz_catch(ctx_pdf) {
        sprintf(msgStatus, "ERRO AO LER O ARQUIVO PDF."); visualizandoPDF = false; return;
    }

    if (pdfTotalPaginas > 0) carregarPaginaPDFNativo(1);
}

// Intercepta os botões do PS4
bool processarControlesLeitor(unsigned int botoes, unsigned int botoesAntigos) {
    if (!visualizandoPDF) return false;

    unsigned int press = botoes & ~botoesAntigos;
    unsigned int hold = botoes;

    // Bolinha para Fechar
    if (press & ORBIS_PAD_BUTTON_CIRCLE) { fecharPDF(); return true; }

    // L1 e R1 para Trocar as Páginas
    if (press & ORBIS_PAD_BUTTON_L1) { if (pdfPaginaAtual > 1) carregarPaginaPDFNativo(pdfPaginaAtual - 1); }
    if (press & ORBIS_PAD_BUTTON_R1) { if (pdfPaginaAtual < pdfTotalPaginas) carregarPaginaPDFNativo(pdfPaginaAtual + 1); }

    // L2 e R2 para Controlar o Zoom
    if (hold & ORBIS_PAD_BUTTON_L2) { pdfZoom -= 0.05f; if (pdfZoom < 0.2f) pdfZoom = 0.2f; }
    if (hold & ORBIS_PAD_BUTTON_R2) { pdfZoom += 0.05f; if (pdfZoom > 5.0f) pdfZoom = 5.0f; }

    // Setinhas do controle para Mover a Página
    int velPan = 30; // Velocidade da câmera
    if (hold & ORBIS_PAD_BUTTON_UP) pdfOffsetY += velPan;
    if (hold & ORBIS_PAD_BUTTON_DOWN) pdfOffsetY -= velPan;
    if (hold & ORBIS_PAD_BUTTON_LEFT) pdfOffsetX += velPan;
    if (hold & ORBIS_PAD_BUTTON_RIGHT) pdfOffsetX -= velPan;

    // ========================================================
    // TRAVA INTELIGENTE DE BORDAS
    // Só deixa mover se a imagem for maior que a tela.
    // ========================================================
    int dW = (int)(pdfImgW * pdfZoom);
    int dH = (int)(pdfImgH * pdfZoom);

    // Calcula até onde a imagem pode ser arrastada sem sair da tela
    int limiteX = (dW > 1920) ? (dW - 1920) / 2 : 0;
    int limiteY = (dH > 1080) ? (dH - 1080) / 2 : 0;

    // Trava o eixo X
    if (pdfOffsetX > limiteX) pdfOffsetX = limiteX;
    if (pdfOffsetX < -limiteX) pdfOffsetX = -limiteX;

    // Trava o eixo Y
    if (pdfOffsetY > limiteY) pdfOffsetY = limiteY;
    if (pdfOffsetY < -limiteY) pdfOffsetY = -limiteY;

    return true;
}