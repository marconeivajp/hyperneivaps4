#pragma once
#ifndef BAIXAR_REPOSITORIO_H
#define BAIXAR_REPOSITORIO_H

#include <stdint.h>

void preencherMenuRepositorios();
void listarXMLsRepositorio();
void abrirXMLRepositorio(const char* xmlFile);
void mostrarLinksJogo(int gameIndex);

void preencherMenuGithub();
void mostrarGithubRepos();
void mostrarGithubReleases();
void githubAbrirReleases(const char* repo);
void githubAbrirRelease(int indice);
void abrirTecladoGithub(int32_t uId);
void acaoTriangle_Github();
void acaoOpcaoGithub(int indice);
void githubBaixarArquivo(int indice);
void atualizarImeGithub();

#endif