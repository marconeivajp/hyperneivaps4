#ifndef __builtin_va_list
#define __builtin_va_list void*
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>
#include <orbis/AudioOut.h>

extern "C" {
    struct jbc_cred { uid_t uid; uid_t ruid; uid_t svuid; gid_t rgid; gid_t svgid; uintptr_t prison; uintptr_t cdir; uintptr_t rdir; uintptr_t jdir; uint64_t sceProcType; uint64_t sonyCred; uint64_t sceProcCap; };
    int jbc_get_cred(struct jbc_cred*);
    int jbc_jailbreak_cred(struct jbc_cred*);
    int jbc_set_cred(const struct jbc_cred*);
}
#include "libretro_bridge.h"

extern int gba_to_ps4_map[16][2];
#include "menu.h" 
#include "menu_grafico.h" 
#include "menu_emulador.h"
#include "controle_emulador.h"
#include "explorar.h"
#include "editar.h"
#include "network.h"
#include "baixar.h"
#include "jogar.h"
#include "audio.h"
#include "graphics.h"
#include "controle.h"

extern bool emuladorAtivo;
bool menuEmuladorAtivo = false;
int menuEmuSelecao = 0;
#include "criar_pastas.h"
#include "bloco_de_notas.h" 
#include "controle_elementos.h" 
#include "elementos_sonoros.h" 
#include "elementos_animados_sprite_sheet.h"
#include "pdf.h"   // Leitor de PDF com MuPDF
#include "libretro_bridge.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int selAudioOpcao = 0; bool tecladoAtivo = false; int tecladoTipo = 0; uint16_t* bufferTecladoW = NULL; char bufferTecladoC[128] = "";
int pad = -1; // Usado pelo Bridge para vibracao
extern char msgStatus[128];
extern int msgTimer;
uint32_t msgStatusColor = 0xFFFFFFFF; // Branco por padrao
unsigned char* backImg = NULL, * defaultArtwork1 = NULL, * defaultArtwork2 = NULL;
int wB, hB, cB, wDef1, hDef1, cDef1, wDef2, hDef2, cDef2;

int32_t global_uId = 0;
int globalPadHandle = -1;

// =========================================================================
// VARIÁVEIS CRIADAS AQUI PARA MATAR O ERRO "UNDEFINED SYMBOL"
// =========================================================================
bool visualizandoPic1 = false;
bool menuL2_Aberto = false;
// visualizandoMidiaPDF removida — usa visualizandoPDF de pdf.h

// Adicionadas para o escudo do PiP não travar o compilador!
bool bloco_notas_aberto = false;
bool instrumento_aberto = false;
bool teste_controle_aberto = false;

extern const char* listaOpcoesAudio[11];
extern void abrirMenuAudioOpcoes(), tratarSelecaoAudio(int op);
extern bool visualizandoMidiaImagem, visualizandoMidiaTexto;
extern unsigned char* imgMidia;

// Puxando a variável e função de inicialização pro atalho funcionar
extern char ultimo_video_tocado[512];
extern void iniciarVideoMP4(const char* caminho);
extern void carregarApenasCaminhoUltimoVideo();

// Declarações Externas para linkar com o menu_video.cpp
extern bool videoRodando, video_minimizado, menu_video_aberto, pip_selecionado, bloqueio_fechar_video;
extern void pararVideo(), atualizarVideoFFmpeg(uint32_t* tela), verificarTrocaDeVideo();
extern void processarControlesMenuVideo(unsigned int btn, OrbisPadData* pData, bool& cross_consumido, bool& circle_consumido);
extern void desenharMenuPlayerVideo(uint32_t* p);

off_t imePh; void* imeVm = NULL;
off_t mPh; void* mVm = NULL; size_t mSz_global = 0;
OrbisImeDialogSetting* imeSetting = NULL;

void desligarInterfaceGrafica() {
    if (backImg) { stbi_image_free(backImg); backImg = NULL; }
}

// NOVO: Purga Agressiva para núcleos grandes (>20MB)
extern unsigned char* imgBgDinamico;
extern unsigned char* imgCapaDinamica;
extern unsigned char* imgDiscoDinamico;
extern unsigned char* imgPreview;
extern unsigned char* uiTextures[10];

extern void pararVideo(); // Importado do menu_video.cpp
extern void pararVideo(); // Importado do menu_video.cpp

void limpezaProfundaRAM() {
    desligarInterfaceGrafica();
    
    // PURGA RADICAL V20: Fecha vídeo e descarrega módulos pesados
    pararVideo();
    
    // Desativar IME p/ liberar espaço de execução contíguo (0x80020008)
    sceSysmoduleUnloadModule(ORBIS_SYSMODULE_IME_DIALOG);
    
    // Limpeza de Ponteiros e Buffers Dinâmicos
    if (imgBgDinamico) { stbi_image_free(imgBgDinamico); imgBgDinamico = NULL; }
    if (imgCapaDinamica) { stbi_image_free(imgCapaDinamica); imgCapaDinamica = NULL; }
    if (imgDiscoDinamico) { stbi_image_free(imgDiscoDinamico); imgDiscoDinamico = NULL; }
    if (imgPreview) { stbi_image_free(imgPreview); imgPreview = NULL; }
    for (int i = 0; i < 10; i++) {
        if (uiTextures[i]) { stbi_image_free(uiTextures[i]); uiTextures[i] = NULL; }
    }
}

// FERRAMENTA DE DESCOBERTA V19: Testa especificamente a "Memória Direta"
int obterMemoriaEstimadaLivreMB() {
    int tamanhosTeste[] = { 128, 64, 32, 24, 16, 8, 4, 1 };
    int maiorSucesso = 0;
    
    for (int i = 0; i < 8; i++) {
        size_t bSz = (size_t)tamanhosTeste[i] * 1024 * 1024;
        off_t ph;
        // Tenta alocar na Memória Direta (Type 2 = Direct Memory)
        if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), bSz, 2097152, 2, &ph) == 0) {
            maiorSucesso = tamanhosTeste[i];
            sceKernelReleaseDirectMemory(ph, bSz);
            break; 
        }
    }
    
    return maiorSucesso;
}

extern int selMapeamento;
extern bool esperandoBotao;

void ligarInterfaceGrafica() {
    // V50: Busca exaustiva por plano de fundo
    if (!backImg) {
        printf("[UI] Tentando carregar fundo...\n");
        const char* caminhos[] = {
            "/data/HyperNeiva/configuracao/0_Defalt_Background.png",
            "/data/HyperNeiva/background.png",
            "/app0/assets/images/background.png",
            "/app0/assets/images/0_Defalt_Background.png",
            "/app0/assets/images/bg.png"
        };
        for (int i = 0; i < 5; i++) {
            backImg = stbi_load(caminhos[i], &wB, &hB, &cB, 4);
            if (backImg) { printf("[UI] Fundo carregado de: %s\n", caminhos[i]); break; }
        }
    }
}

int main(void) {
    initNetwork(); inicializarAudio();
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    sceCommonDialogInitialize();
    sceUserServiceInitialize(NULL);
    sceUserServiceGetInitialUser(&global_uId);
    int32_t uId = global_uId;

    scePadInit(); globalPadHandle = scePadOpen(global_uId, 0, 0, NULL); int pad = globalPadHandle;

    inicializarVideo();
    carregarApenasCaminhoUltimoVideo(); // Carrega o vídeo pra memória logo no boot!
    
    // --- RESTAURAÇÃO DA FONTE E TECLADO ---
    if (!imeVm) {
        sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 2097152, 2097152, 2, &imePh);
        sceKernelMapDirectMemory(&imeVm, 2097152, 0x33, 0, imePh, 2097152);
        imeSetting = (OrbisImeDialogSetting*)imeVm;
        bufferTecladoW = (uint16_t*)((uint8_t*)imeVm + 1024);
    }
    if (!temF) {
        int fd = sceKernelOpen("/app0/assets/fonts/font.ttf", 0, 0);
        if (fd >= 0) {
            off_t fs = sceKernelLseek(fd, 0, 2); sceKernelLseek(fd, 0, 0);
            if (mSz_global == 0) mSz_global = (fs + 0x1FFFFF) & ~0x1FFFFF;
            sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), mSz_global, 2097152, 2, &mPh);
            sceKernelMapDirectMemory(&mVm, mSz_global, 0x01 | 0x02, 0, mPh, 2097152);
            sceKernelRead(fd, mVm, fs); sceKernelClose(fd);
            memset(&font, 0, sizeof(font)); 
            temF = stbtt_InitFont(&font, (const unsigned char*)mVm, 0);
        }
    }
    
    ligarInterfaceGrafica(); // Recarrega o fundo
    
    uint16_t* imeTitle = (uint16_t*)((uint8_t*)bufferTecladoW + 1024);
    uint16_t t[] = { 'D','i','g','i','t','e',' ','o',' ','L','i','n','k','\0' }; memcpy(imeTitle, t, sizeof(t));

    inicializarPastas(); carregarConfiguracao(); inicializarElementosSonoros();

    backImg = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Background.png", &wB, &hB, &cB, 4);
    if (!backImg) backImg = stbi_load("/app0/assets/images/0_Defalt_Background.png", &wB, &hB, &cB, 4);
    carregarSpriteSheetAnimada();
    struct jbc_cred cred; jbc_get_cred(&cred); jbc_jailbreak_cred(&cred); jbc_set_cred(&cred);
    sceKernelChmod("/mnt/usb0", 0777); sceKernelChmod("/mnt/usb1", 0777);
    preencherRoot();
    
    // CONFIRMAÇÃO VISUAL V45 - LIMPO
    sprintf(msgStatus, "HYPER NEIVA CARREGADA");
    msgTimer = 120; 

    int libSys = sceKernelLoadStartModule("libSceSystemService.sprx", 0, NULL, 0, NULL, NULL);
    if (libSys >= 0) {
        typedef int (*HideSplashFunc)(); HideSplashFunc hideSplash = NULL;
        sceKernelDlsym(libSys, "sceSystemServiceHideSplashScreen", (void**)&hideSplash);
        if (hideSplash) hideSplash();
        sceKernelStopUnloadModule(libSys, 0, NULL, 0, NULL, NULL);
    }

    static unsigned int ultimos_botoes = 0;

    for (;;) {
        OrbisPadData pData;
        if (scePadReadState(pad, &pData) == 0) {
            // --- LÓGICA DE CONTROLE DO EMULADOR (V33) ---
            if (emuladorAtivo) {
                gEmuPadButtons = pData.buttons; // Centraliza leitura para o nucleo
                
                // Gatilho do Menu: L1 + R1 + OPTIONS + TOUCHPAD
                if ((pData.buttons & ORBIS_PAD_BUTTON_L1) && (pData.buttons & ORBIS_PAD_BUTTON_R1) && 
                    (pData.buttons & ORBIS_PAD_BUTTON_OPTIONS) && (pData.buttons & ORBIS_PAD_BUTTON_TOUCH_PAD)) {
                    if (!menuEmuladorAtivo) {
                        menuEmuladorAtivo = true;
                        extern void capturarUltimoFrame();
                        capturarUltimoFrame(); // V40: Captura única ao pausar
                        menuEmuSelecao = 0;
                        ultimos_botoes = pData.buttons;
                    }
                }

                if (menuEmuladorAtivo) {
                    unsigned int btnM = pData.buttons & ~ultimos_botoes;
                    if (btnM & ORBIS_PAD_BUTTON_UP) { menuEmuSelecao--; if (menuEmuSelecao < 0) menuEmuSelecao = 4; }
                    if (btnM & ORBIS_PAD_BUTTON_DOWN) { menuEmuSelecao++; if (menuEmuSelecao > 4) menuEmuSelecao = 0; }
                    
                    if (btnM & ORBIS_PAD_BUTTON_CROSS) { 
                        if (menuEmuSelecao == 0) menuEmuladorAtivo = false; // Continuar
                        if (menuEmuSelecao == 1) { // Save State
                            char sPath[1024]; snprintf(sPath, sizeof(sPath), "/data/retroarch/savestates/current.state");
                            bridge_salvar_state(sPath);
                            snprintf(msgStatus, sizeof(msgStatus), "STATUS: JOGO SALVO COM SUCESSO!"); msgTimer = 180;
                            menuEmuladorAtivo = false;
                        }
                        if (menuEmuSelecao == 2) { // Load State
                            char sPath[1024]; snprintf(sPath, sizeof(sPath), "/data/retroarch/savestates/current.state");
                            bridge_carregar_state(sPath);
                            snprintf(msgStatus, sizeof(msgStatus), "STATUS: JOGO CARREGADO!"); msgTimer = 180;
                            menuEmuladorAtivo = false;
                        }
                        if (menuEmuSelecao == 3) { // Controles
                            menuAtual = MENU_EMU_CONTROLES; // V45: TRANSIÇÃO REAL PARA TELA DE CONTROLES
                        }
                        if (menuEmuSelecao == 4) { // Sair
                            fecharEmulador();
                            menuEmuladorAtivo = false;
                            
                            // V43: FORÇA O RETORNO PARA A LISTA DE JOGOS GBA
                            extern void preencherMenuGBA();
                            preencherMenuGBA(); 
                            menuAtual = MENU_EMULADOR; // Retorna para a lista de jogos, nao para o explorador vazio
                            
                            ligarInterfaceGrafica();
                            if (backImg) { stbi_image_free(backImg); backImg = NULL; }
                            backImg = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Background.png", &wB, &hB, &cB, 4);
                            if (!backImg) backImg = stbi_load("/app0/assets/images/0_Defalt_Background.png", &wB, &hB, &cB, 4);
                            
                            snprintf(msgStatus, sizeof(msgStatus), "RETORNO GBA OK"); msgTimer = 240;
                        }
                    }
                    if (btnM & ORBIS_PAD_BUTTON_CIRCLE) menuEmuladorAtivo = false; // Atalho para voltar
                    
                    // ultimos_botoes = pData.buttons; // REMOVIDO PARA EVITAR DUPLICIDADE
                }
            } else {
                // Se o emulador NÃO estiver ativo, garantimos que ele não fique preso no estado de execução
                if (menuAtual == MENU_EMULADOR_EXECUCAO) {
                    extern void preencherMenuGBA();
                    preencherMenuGBA();
                    menuAtual = MENU_EMULADOR;
                }
            }

            unsigned int btn = pData.buttons & ~ultimos_botoes;
            bool cross_consumido = false;
            bool circle_consumido = false;

            // NOVO ATALHO GLOBAL: L2 + QUADRADO
            if ((pData.buttons & ORBIS_PAD_BUTTON_L2) && (btn & ORBIS_PAD_BUTTON_SQUARE)) {
                if (strlen(ultimo_video_tocado) > 0) {
                    video_minimizado = true;
                    iniciarVideoMP4(ultimo_video_tocado);
                    pData.buttons &= ~(ORBIS_PAD_BUTTON_L2 | ORBIS_PAD_BUTTON_SQUARE);
                }
            }

            if (btn & ORBIS_PAD_BUTTON_CIRCLE) {
                if (visualizandoPic1) { visualizandoPic1 = false; circle_consumido = true; }
                else if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } circle_consumido = true; }
                else if (visualizandoMidiaTexto) { visualizandoMidiaTexto = false; circle_consumido = true; }
                else if (visualizandoPDF) { fecharPDF(); circle_consumido = true; }
                else if (videoRodando && !menu_video_aberto) {
                    if (!video_minimizado) { bloqueio_fechar_video = false; pararVideo(); circle_consumido = true; }
                    else if (pip_selecionado) { pip_selecionado = false; circle_consumido = true; }
                }
            }

            if (ctrl1On) {
                int rx = pData.rightStick.x, ry = pData.rightStick.y, lx = pData.leftStick.x, ly = pData.leftStick.y;
                if (rx > 180 || lx > 180) ctrl1X += 15; if ((rx < 80 && rx > 0) || (lx < 80 && lx > 0)) ctrl1X -= 15;
                if (ry > 180 || ly > 180) ctrl1Y += 15; if ((ry < 80 && ry > 0) || (ly < 80 && ry > 0)) ctrl1Y -= 15;
            }

            if (videoRodando) {
                processarControlesMenuVideo(btn, &pData, cross_consumido, circle_consumido);
            }
            // Controles do leitor de PDF (L1/R1 paginar, L2/R2 zoom, setas mover)
            if (visualizandoPDF) {
                processarControlesLeitor(pData.buttons, ultimos_botoes);
                circle_consumido = true;  // Bolinha ja tratada no processarControlesLeitor
            }

            if (cross_consumido) pData.buttons &= ~ORBIS_PAD_BUTTON_CROSS;
            if (circle_consumido) pData.buttons &= ~ORBIS_PAD_BUTTON_CIRCLE;

            if (video_minimizado && !pip_selecionado) { bloqueio_fechar_video = true; }
            else { bloqueio_fechar_video = false; }

            if (!esperandoNomePasta && !esperandoRenomear && !visualizandoMidiaImagem && !visualizandoMidiaTexto && !visualizandoPic1 && !visualizandoPDF) {
                if (!emuladorAtivo && (!videoRodando || (video_minimizado && !pip_selecionado))) {
                    processarControles(pData.buttons, uId, imeSetting, imeTitle);
                }
            }
            
            extern uint8_t gEmuAnaLX, gEmuAnaLY, gEmuAnaRX, gEmuAnaRY;
            gEmuAnaLX = pData.leftStick.x;
            gEmuAnaLY = pData.leftStick.y;
            gEmuAnaRX = pData.rightStick.x;
            gEmuAnaRY = pData.rightStick.y;

            // --- LÓGICA MODULAR V61 ---
            static uint32_t ultimos_botoes_globais = 0;
            uint32_t botoes_preservados = pData.buttons;

            if (menuAtual == 50) { // MENU_EMU_CONTROLES (controle_emulador.cpp)
                atualizarMapeamentoControles(&pData, ultimos_botoes_globais);
                pData.buttons = 0; 
            } 
            else if (menuAtual == 51 || menuEmuladorAtivo) { // MENU_PAUSA (menu_emulador.cpp)
                menuEmuladorAtivo = true;
                atualizarMenuEmulador(pData.buttons, ultimos_botoes_globais);
                pData.buttons = 0; 
            }
            else { // MODO DE JOGO NORMAL
                bridge_set_pausado(false);
                if ((pData.buttons & ORBIS_PAD_BUTTON_OPTIONS) && !(ultimos_botoes_globais & ORBIS_PAD_BUTTON_OPTIONS)) {
                    menuEmuladorAtivo = true;
                    bridge_set_pausado(true);
                    capturarUltimoFrame(); 
                    menuEmuSelecao = 0;
                }
            }

            ultimos_botoes_globais = botoes_preservados;
            ultimos_botoes = botoes_preservados; 
        }

        if (emuladorAtivo) {
            if (menuEmuladorAtivo) {
                if (menuAtual == 50) desenharAjudaControles(obterBufferVideo());
                else desenharMenuEmulador(obterBufferVideo());
            } else {
                bridge_atualizar();
            }
            submeterTelaSemPausa();
            continue; 
        }

        uint32_t* p = obterBufferVideo();
        for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF121212;
        if (backImg) desenharRedimensionado(p, backImg, wB, hB, 1920, 1080, 0, 0);

        atualizarImePasta();

        if (videoRodando && video_minimizado) {
            verificarTrocaDeVideo();
            atualizarVideoFFmpeg(p);
        }

        desenharInterface(p);

        if (videoRodando && !video_minimizado) {
            verificarTrocaDeVideo();
            atualizarVideoFFmpeg(p);
        }

        // ===== RENDERIZACAO DO LEITOR DE PDF =====
        if (visualizandoPDF && imgPaginaAtual) {
            // Fundo preto sólido
            for (int i = 0; i < 1920 * 1080; i++) p[i] = 0xFF000000;

            int dW = (int)(pdfImgW * pdfZoom);
            int dH = (int)(pdfImgH * pdfZoom);
            int posX = 960 - dW / 2 + pdfOffsetX;
            int posY = 540 - dH / 2 + pdfOffsetY;

            // Clipa para nao sair da tela
            if (dW > 0 && dH > 0) desenharRedimensionado(p, imgPaginaAtual, pdfImgW, pdfImgH, dW, dH, posX, posY);

            // HUD do leitor
            char hudPdf[128];
            sprintf(hudPdf, "Pag %d/%d  |  Zoom:%.0f%%  |  O=Fechar  L1/R1=Paginar  L2/R2=Zoom  Setas=Mover",
                pdfPaginaAtual, pdfTotalPaginas, pdfZoom * 100.0f);
            // Barra de status na parte de baixo
            for (int hx = 0; hx < 1920; hx++) p[1058 * 1920 + hx] = 0xCC222222;
            for (int hx = 0; hx < 1920; hx++) p[1059 * 1920 + hx] = 0xCC222222;
            for (int hy = 1060; hy < 1080; hy++) for (int hx = 0; hx < 1920; hx++) p[hy * 1920 + hx] = 0xDD111111;
            desenharTexto(p, hudPdf, 17, 20, 1063, 0xFFFFFFFF);
        }
        // ==========================================

        if (videoRodando) {
            desenharMenuPlayerVideo(p);
        }

        submeterTela();
    }
}