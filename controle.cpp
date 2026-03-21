#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "controle.h"
#include "controle_musicas.h"
#include "controle_virtual.h"
#include "controle_explorar.h"
#include "controle_editar.h"
#include "controle_baixar.h"
#include "controle_root.h"
#include "menu_upload.h"

#include "stb_image.h"
#include "menu.h"
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"
#include "bloco_de_notas.h"

int cd = 0;
bool pCross = false;
bool pCircle = false;
bool pTri = false;
bool pSquare = false;
bool pL1 = false;
bool pR1 = false;
bool pL2 = false; // NOVA: Variável para o botão L2

extern int selAudioOpcao;
extern int selOpcao;
extern int sel;
extern int off;
extern int totalItens;
extern bool showOpcoes;
extern bool editMode;
extern bool marcados[3000];

// VARIÁVEIS DO PAINEL DUPLO IMPORTADAS
extern bool painelDuplo;
extern int painelAtivo;
extern int selEsq;
extern int totalItensEsq;
extern int offEsq; // CORRIGIDO: Agora é extern (pois já foi criado no controle_direcional.cpp)

extern void acaoCross_Notepad(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle, const char* textoInicial);
extern void acaoCross_Baixar(int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle);
extern void acaoCircle_Baixar();
extern void acaoTriangle_Baixar();
extern void acaoL2_Explorar(); // DECLARAÇÃO DA FUNÇÃO DO L2

void processarNavegacaoDPad(uint32_t botoes) {
    // LÓGICA DO PAINEL DUPLO: Alternar entre Esq/Dir
    if (painelDuplo && !showOpcoes && (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME)) {
        if (botoes & (ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)) {
            if (cd <= 0) {
                painelAtivo = (painelAtivo == 0) ? 1 : 0;
                cd = 10;
            }
            return; // Sai para não acionar as setas para cima/baixo simultaneamente
        }
    }

    if (botoes & (ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_UP)) {
        if (cd <= 0) {
            if (menuAtual == MENU_NOTEPAD) {
                if (estadoNotepad == 0) {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN) {
                        if (linhaSelecionada < MAX_LINHAS - 1) {
                            linhaSelecionada++;
                            if (linhaSelecionada >= totalLinhasNotepad && !notepadSomenteLeitura) totalLinhasNotepad = linhaSelecionada + 1;
                        }
                    }
                    else if (botoes & ORBIS_PAD_BUTTON_UP && linhaSelecionada > 0) {
                        linhaSelecionada--;
                    }
                }
            }
            else if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) {
                if (botoes & ORBIS_PAD_BUTTON_DOWN && selUploadOpcao < 2) selUploadOpcao++;
                else if (botoes & ORBIS_PAD_BUTTON_UP && selUploadOpcao > 0) selUploadOpcao--;
            }
            else if (showOpcoes) {
                if (menuAtual == MENU_AUDIO_OPCOES) {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && selAudioOpcao < 10) selAudioOpcao++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && selAudioOpcao > 0) selAudioOpcao--;
                }
                else {
                    if (botoes & ORBIS_PAD_BUTTON_DOWN && selOpcao < 9) selOpcao++;
                    else if (botoes & ORBIS_PAD_BUTTON_UP && selOpcao > 0) selOpcao--;
                }
            }
            else {
                // Navegação Clássica com Suporte ao Painel Esquerdo
                bool ehEsq = (painelDuplo && painelAtivo == 0 && (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME));
                int* sAtual = ehEsq ? &selEsq : &sel;
                int* oAtual = ehEsq ? &offEsq : &off;
                int tItens = ehEsq ? totalItensEsq : totalItens;

                if (botoes & ORBIS_PAD_BUTTON_DOWN && *sAtual < (tItens - 1)) {
                    (*sAtual)++;
                    if (*sAtual >= (*oAtual + 6)) (*oAtual)++;
                }
                else if (botoes & ORBIS_PAD_BUTTON_UP && *sAtual > 0) {
                    (*sAtual)--;
                    if (*sAtual < *oAtual) (*oAtual)--;
                }
            }
            cd = 10;
        }
    }
    else cd = 0;
    if (cd > 0) cd--;
}

void processarControles(uint32_t botoes, int32_t uId, OrbisImeDialogSetting* imeSetting, uint16_t* imeTitle) {
    if (editMode && menuAtual != MENU_NOTEPAD) { processarControlesEdicao(botoes); return; }

    processarNavegacaoDPad(botoes);

    // --- NOVA LEITURA DO L2 ---
    if (botoes & ORBIS_PAD_BUTTON_L2) {
        if (!pL2) {
            if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoL2_Explorar();
            pL2 = true;
        }
    }
    else pL2 = false;
    // --------------------------

    if (botoes & ORBIS_PAD_BUTTON_L1) {
        if (!pL1) {
            if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) marcados[sel] = !marcados[sel];
            pL1 = true;
        }
    }
    else pL1 = false;

    if (botoes & ORBIS_PAD_BUTTON_R1) {
        if (!pR1) {
            if (menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD) marcados[sel] = !marcados[sel];
            else if (menuAtual == MENU_EXPLORAR) acaoR1_Explorar();
            pR1 = true;
        }
    }
    else pR1 = false;

    if (botoes & ORBIS_PAD_BUTTON_CROSS) {
        if (!pCross) {
            if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) acaoCross_MenuUpload();
            else if (menuAtual == ROOT || menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCross_Root();
            else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCross_Musicas();
            else if (menuAtual == MENU_NOTEPAD) {
                if (!notepadSomenteLeitura) {
                    if (estadoNotepad == 0) acaoCross_Notepad(uId, imeSetting, imeTitle, linhasNotepad[linhaSelecionada]);
                    else if (estadoNotepad == 1) acaoCross_Notepad(uId, imeSetting, imeTitle, nomeArquivo);
                }
            }
            else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCross_Explorar();
            else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCross_Editar();
            else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCross_Baixar(uId, imeSetting, imeTitle);

            pCross = true;
        }
    }
    else pCross = false;

    if (botoes & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!pCircle) {
            if (showUploadOpcoes && (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA)) acaoCircle_MenuUpload();
            else if (showOpcoes && menuAtual != MENU_AUDIO_OPCOES) showOpcoes = false;
            else {
                if (menuAtual == MENU_NOTEPAD) {
                    if (notepadSomenteLeitura) {
                        menuAtual = MENU_MIDIA;
                    }
                    else {
                        if (estadoNotepad == 1) estadoNotepad = 0; // Volta do nomear pro editar
                        else {
                            menuAtual = MENU_EXPLORAR; // Volta pro explorador
                            // Atualizado para recarregar o painel correto
                            if (painelDuplo && painelAtivo == 0) listarDiretorioEsq(pathExplorarEsq);
                            else listarDiretorio(pathExplorar);
                        }
                    }
                }
                else if (menuAtual == JOGAR_XML || menuAtual == MENU_MIDIA) acaoCircle_Root();
                else if (menuAtual == MENU_MUSICAS || menuAtual == MENU_AUDIO_OPCOES) acaoCircle_Musicas();
                else if (menuAtual == MENU_EXPLORAR || menuAtual == MENU_EXPLORAR_HOME) acaoCircle_Explorar();
                else if (menuAtual == MENU_EDITAR || menuAtual == MENU_EDIT_TARGET) acaoCircle_Editar();
                else if (menuAtual == MENU_BAIXAR || menuAtual == MENU_BAIXAR_REPOS || menuAtual == MENU_BAIXAR_GAMES_XMLS || menuAtual == MENU_BAIXAR_GAMES_LIST || menuAtual == MENU_BAIXAR_LINKS || menuAtual == MENU_BAIXAR_LINK_DIRETO || menuAtual == MENU_BAIXAR_DROPBOX_LISTA || menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_BACKUP || menuAtual == MENU_CAPAS || menuAtual == MENU_CONSOLES || menuAtual == SCRAPER_LIST) acaoCircle_Baixar();
            }
            pCircle = true;
        }
    }
    else pCircle = false;

    if (botoes & ORBIS_PAD_BUTTON_TRIANGLE) {
        if (!pTri) {
            if (menuAtual == MENU_MUSICAS) acaoTriangle_Musicas();
            else if (menuAtual == MENU_EXPLORAR) acaoTriangle_Explorar();
            else if (menuAtual == MENU_BAIXAR_DROPBOX_UPLOAD || menuAtual == MENU_BAIXAR_DROPBOX_LISTA) acaoTriangle_MenuUpload();

            pTri = true;
        }
    }
    else pTri = false;

    if (botoes & ORBIS_PAD_BUTTON_SQUARE) {
        if (!pSquare) {
            if (menuAtual == MENU_NOTEPAD) {
                if (!notepadSomenteLeitura) {
                    if (estadoNotepad == 0) estadoNotepad = 1; // Pula de editar pro nome!
                    else if (estadoNotepad == 1) {
                        if (strlen(nomeArquivo) > 0) salvarArquivoNotepad();
                        else { snprintf(msgStatus, sizeof(msgStatus), "O nome do arquivo nao pode ser vazio!"); msgTimer = 120; }
                    }
                }
            }
            pSquare = true;
        }
    }
    else pSquare = false;
}