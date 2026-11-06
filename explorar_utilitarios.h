#ifndef EXPLORAR_UTILITARIOS_H
#define EXPLORAR_UTILITARIOS_H

void copiarArquivoReal(const char* origem, const char* destino);
void deletarPastaRecursivamente(const char* path);
void criarCaminho(const char* filepath);
void extrairZip(const char* zipPath, const char* outPath);
void hnEnfileirarExtracao(const char* arquivo, const char* destino);
void hnEnfileirarColar(const char* origem, const char* destino, bool recortar);
void hnAtualizarExtracao();
bool hnNaThreadExtracao();
bool hnEhParteCompactada(const char* nomeMinusculo);
void hnTituloPkg(const char* caminho, char* saida, int tamanho);
long long hnBytesFaltandoPkg(const char* caminho);
void hnProgressoExtracao(float progresso);
void extrair7z(const char* zipPath, const char* outPath);
void extrairRar(const char* rarPath, const char* outPath);

#endif