#ifndef PDF_H
#define PDF_H

extern bool visualizandoPDF;
extern int pdfPaginaAtual;
extern int pdfTotalPaginas;
extern float pdfZoom;
extern int pdfOffsetX;
extern int pdfOffsetY;

// A imagem renderizada da página atual vai ficar salva aqui
extern unsigned char* imgPaginaAtual;
extern int pdfImgW;
extern int pdfImgH;

void abrirLeitor(const char* caminho);
void fecharPDF();

// Coloque esta função dentro do seu controle.cpp para interceptar os botões
bool processarControlesLeitor(unsigned int botoes, unsigned int botoesAntigos);

#endif