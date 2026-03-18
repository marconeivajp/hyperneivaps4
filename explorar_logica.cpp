#include "globals.h"

// Corrigido para os caminhos reais encontrados na sua pasta assets/xml
void abrirListaPrincipal() {
    carregarXML("/app0/assets/xml/lista.xml");
}

void abrirSubListaSP() {
    carregarXML("/app0/assets/xml/sp.xml");
}

void gerenciarVoltaJogar() {
    if (strstr(xmlCaminhoAtual, "sp.xml")) {
        abrirListaPrincipal();
    }
    else {
        preencherRoot();
    }
}