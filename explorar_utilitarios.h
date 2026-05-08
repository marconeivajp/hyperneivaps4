#ifndef EXPLORAR_UTILITARIOS_H
#define EXPLORAR_UTILITARIOS_H

void copiarArquivoReal(const char* origem, const char* destino);
void deletarPastaRecursivamente(const char* path);
void criarCaminho(const char* filepath);
void extrairZip(const char* zipPath, const char* outPath);

#endif