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
extern int panMidiaX, panMidiaY;
extern unsigned char* imgMidia;
extern float zoomMidia;
extern bool fullscreenMidia;
extern int wM, hM;

// Puxando a variável e função de inicialização pro atalho funcionar
extern char ultimo_video_tocado[512];
extern void iniciarVideoMP4(const char* caminho);
extern void carregarApenasCaminhoUltimoVideo();

// Declarações Externas para linkar com o menu_video.cpp
extern bool videoRodando, video_minimizado, menu_video_aberto, pip_selecionado, bloqueio_fechar_video;
extern void pararVideo(), verificarTrocaDeVideo();
extern void pausarVideoParaTela(), retomarVideoAposTela();
extern void definirApresentacaoVideo(bool naTela);
extern void processarControlesMenuVideo(unsigned int btn, OrbisPadData* pData, bool& cross_consumido, bool& circle_consumido);

off_t imePh; void* imeVm = NULL;
off_t mPh; void* mVm = NULL; size_t mSz_global = 0;
OrbisImeDialogSetting* imeSetting = NULL;

// Redimensionar o fundo pixel a pixel era a parte mais cara de cada volta do menu. Ele só muda
// quando a imagem muda, então fica pronto em RAM e cada volta faz apenas uma cópia.
static uint32_t* fundoPronto = NULL;
static uint32_t fundoAssinatura = 0;

static uint32_t assinaturaFundo() {
    if (!backImg) return 1;
    uint32_t soma = (uint32_t)wB * 31u + (uint32_t)hB;
    size_t total = (size_t)wB * hB * 4;
    for (size_t i = 0; i < total; i += total / 512 + 1) soma = soma * 33u + backImg[i];
    return soma | 2u;
}

static void desenharFundoMenu(uint32_t* p) {
    if (!fundoPronto) fundoPronto = (uint32_t*)malloc(1920 * 1080 * 4);
    uint32_t* destino = fundoPronto ? fundoPronto : p;
    uint32_t assinatura = assinaturaFundo();
    if (!fundoPronto || assinatura != fundoAssinatura) {
        for (int i = 0; i < 1920 * 1080; i++) destino[i] = 0xFF121212;
        if (backImg) desenharRedimensionado(destino, backImg, wB, hB, 1920, 1080, 0, 0);
        fundoAssinatura = assinatura;
    }
    if (fundoPronto) memcpy(p, fundoPronto, 1920 * 1080 * 4);
}

void desligarInterfaceGrafica() {
    if (backImg) { stbi_image_free(backImg); backImg = NULL; }
    free(fundoPronto); fundoPronto = NULL; fundoAssinatura = 0;
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
            "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.png",
            "/data/HyperNeiva/configuracao/imagens/0_Defalt_Background.jpg",
            "/data/HyperNeiva/configuracao/0_Defalt_Background.png",
            "/data/HyperNeiva/background.png",
            "/app0/assets/images/background.png",
            "/app0/assets/images/0_Defalt_Background.png",
            "/app0/assets/images/bg.png"
        };
        for (int i = 0; i < 7; i++) {
            backImg = stbi_load(caminhos[i], &wB, &hB, &cB, 4);
            if (backImg) { printf("[UI] Fundo carregado de: %s\n", caminhos[i]); break; }
        }
    }
}

// Mede a abertura e os quadros que demoram: o console não dá depurador, e o log mostra onde travou.
static void registrarInicio(const char* etapa, uint64_t desde) {
    FILE* f = fopen("/data/HyperNeiva/inicio_log.txt", "a");
    if (!f) return;
    fprintf(f, "%s %llu ms\n", etapa, (unsigned long long)((sceKernelGetProcessTime() - desde) / 1000));
    fclose(f);
}

int main(void) {
    uint64_t inicioApp = sceKernelGetProcessTime();
    remove("/data/HyperNeiva/inicio_log.txt");
    initNetwork(); registrarInicio("rede", inicioApp);
    inicializarAudio(); registrarInicio("audio", inicioApp);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    sceCommonDialogInitialize();
    sceUserServiceInitialize(NULL);
    sceUserServiceGetInitialUser(&global_uId);
    int32_t uId = global_uId;

    scePadInit(); globalPadHandle = scePadOpen(global_uId, 0, 0, NULL); int pad = globalPadHandle;

    inicializarVideo();
    if (!obterBufferVideo()) return 1;
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

    registrarInicio("video e fonte", inicioApp);
    inicializarPastas(); registrarInicio("pastas", inicioApp);
    carregarConfiguracao(); inicializarElementosSonoros(); registrarInicio("configuracao", inicioApp);

    // Configurações já salvas tinham o elemento desligado: liga uma vez e depois respeita a escolha.
    FILE* marcaPersonagem = fopen("/data/HyperNeiva/configuracao/personagem_padrao_v2.txt", "r");
    if (marcaPersonagem) fclose(marcaPersonagem);
    else {
        extern void salvarConfiguracao();
        extern int ctrl1Livre, pont1On;
        ctrl1On = 1;
        pont1On = 1;
        ctrl1Livre = 1;
        salvarConfiguracao();
        marcaPersonagem = fopen("/data/HyperNeiva/configuracao/personagem_padrao_v2.txt", "w");
        if (marcaPersonagem) fclose(marcaPersonagem);
    }

    backImg = stbi_load("/data/HyperNeiva/configuracao/0_Defalt_Background.png", &wB, &hB, &cB, 4);
    if (!backImg) backImg = stbi_load("/app0/assets/images/0_Defalt_Background.png", &wB, &hB, &cB, 4);
    carregarSpriteSheetAnimada();
    struct jbc_cred cred; jbc_get_cred(&cred); jbc_jailbreak_cred(&cred); jbc_set_cred(&cred);
    sceKernelChmod("/mnt/usb0", 0777); sceKernelChmod("/mnt/usb1", 0777);
    registrarInicio("fundo e personagem", inicioApp);
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
    // Botões já usados por uma tela (vídeo, imagem...): ficam bloqueados até serem soltos.
    // Sem isso, segurar o botão repetia a ação a cada volta do loop.
    static unsigned int botoesConsumidos = 0;

    registrarInicio("menu pronto", inicioApp);
    uint64_t quadroAnterior = sceKernelGetProcessTime();
    int quadrosLentos = 0;
    for (;;) {
        {
            uint64_t agora = sceKernelGetProcessTime();
            if (agora - quadroAnterior > 500000 && quadrosLentos < 200) {
                quadrosLentos++;
                char etapa[64];
                snprintf(etapa, sizeof(etapa), "QUADRO LENTO menu=%d duracao", (int)menuAtual);
                registrarInicio(etapa, quadroAnterior);
            }
            quadroAnterior = agora;
        }
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
                            
                            // O emulador liberou as imagens do menu para ter memória: carrega tudo de novo.
                            extern bool isFirstFrameUI;
                            ligarInterfaceGrafica();
                            isFirstFrameUI = true;

                            snprintf(msgStatus, sizeof(msgStatus), "JOGO ENCERRADO"); msgTimer = 180;
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

            unsigned int botoesFisicos = pData.buttons;
            botoesConsumidos &= botoesFisicos;
            unsigned int btn = botoesFisicos & ~ultimos_botoes;
            pData.buttons &= ~botoesConsumidos;
            bool cross_consumido = false;
            bool circle_consumido = false;

            // NOVO ATALHO GLOBAL: L2 + QUADRADO (Bloqueado em Testes de Hardware)
            if (menuAtual != MENU_CONTROLE_TESTE && menuAtual != MENU_INSTRUMENTOS) {
                if ((pData.buttons & ORBIS_PAD_BUTTON_L2) && (btn & ORBIS_PAD_BUTTON_SQUARE)) {
                    if (videoRodando) {
                        // Ja existe video rodando: so alterna PiP, nunca reinicia
                        video_minimizado = !video_minimizado;
                        pData.buttons &= ~(ORBIS_PAD_BUTTON_L2 | ORBIS_PAD_BUTTON_SQUARE);
                    }
                    else if (strlen(ultimo_video_tocado) > 0) {
                        iniciarVideoMP4(ultimo_video_tocado);
                        video_minimizado = true; // inicia em PiP (pararVideo zera a flag)
                        pData.buttons &= ~(ORBIS_PAD_BUTTON_L2 | ORBIS_PAD_BUTTON_SQUARE);
                    }
                }
            }

            if (btn & ORBIS_PAD_BUTTON_CIRCLE) {
                if (visualizandoPic1) { visualizandoPic1 = false; circle_consumido = true; }
                else if (visualizandoMidiaImagem) { visualizandoMidiaImagem = false; if (imgMidia) { stbi_image_free(imgMidia); imgMidia = NULL; } circle_consumido = true; }
                else if (visualizandoMidiaTexto) { visualizandoMidiaTexto = false; circle_consumido = true; }
                else if (visualizandoPDF) { fecharPDF(); circle_consumido = true; }
                else if (videoRodando && !menu_video_aberto) {
                    extern bool fecharControlesPlayer();
                    // Com a barra do player na tela, a Bolinha só fecha a barra.
                    if (!video_minimizado) {
                        if (!fecharControlesPlayer()) { bloqueio_fechar_video = false; pararVideo(); }
                        circle_consumido = true;
                    }
                    else if (pip_selecionado) { pip_selecionado = false; circle_consumido = true; }
                }
            }

            // Controles do visualizador de imagem (X = tela cheia, Cima/Baixo = zoom)
            if (visualizandoMidiaImagem) {
                static int cdZoomImg = 0;
                if (btn & ORBIS_PAD_BUTTON_CROSS) {
                    fullscreenMidia = !fullscreenMidia;
                    panMidiaX = 0; panMidiaY = 0;
                    cross_consumido = true;
                }
                bool upImg = (pData.buttons & ORBIS_PAD_BUTTON_UP) != 0;
                bool dnImg = (pData.buttons & ORBIS_PAD_BUTTON_DOWN) != 0;
                if ((upImg || dnImg) && cdZoomImg <= 0) {
                    float fit = 1.0f;
                    if (wM > 0 && hM > 0) {
                        float pw = 1920.0f / (float)wM, ph = 1080.0f / (float)hM;
                        fit = (pw < ph) ? pw : ph;
                    }
                    if (fullscreenMidia) { fullscreenMidia = false; zoomMidia = fit; }
                    if (upImg) zoomMidia *= 1.25f;
                    if (dnImg) zoomMidia /= 1.25f;
                    if (zoomMidia > fit * 12.0f) zoomMidia = fit * 12.0f;
                    if (zoomMidia < fit * 0.1f) zoomMidia = fit * 0.1f;
                    cdZoomImg = 6;
                }
                else if (cdZoomImg > 0) cdZoomImg--;
            }

            if (ctrl1On) {
                extern int personagemDirX, personagemDirY; extern bool personagemAtacar;
                int rx = pData.rightStick.x, ry = pData.rightStick.y, lx = pData.leftStick.x, ly = pData.leftStick.y;
                // Fim do curso para a esquerda e para cima vale 0. Só os quatro em 0 quer dizer leitura vazia.
                bool semLeitura = rx == 0 && ry == 0 && lx == 0 && ly == 0;
                personagemDirX = semLeitura ? 0 : (rx > 180 || lx > 180) ? 1 : ((rx < 80 || lx < 80) ? -1 : 0);
                personagemDirY = semLeitura ? 0 : (ry > 180 || ly > 180) ? 1 : ((ry < 80 || ly < 80) ? -1 : 0);
                if (btn & ORBIS_PAD_BUTTON_SQUARE) personagemAtacar = true;
                // Player, leitor e imagem usam o analógico para si.
                if (videoRodando || visualizandoPDF || visualizandoMidiaImagem) { personagemDirX = 0; personagemDirY = 0; personagemAtacar = false; }
            }

            if (videoRodando) {
                processarControlesMenuVideo(btn, &pData, cross_consumido, circle_consumido);
            }
            // Controles do leitor de PDF (L1/R1 paginar, L2/R2 zoom, setas mover)
            if (visualizandoPDF) {
                processarControlesLeitor(pData.buttons, ultimos_botoes);
                circle_consumido = true;  // Bolinha ja tratada no processarControlesLeitor
            }
            // Pan da imagem com o analogico esquerdo (apenas quando ampliada)
            if (visualizandoMidiaImagem && !fullscreenMidia) {
                int lx = (int)pData.leftStick.x - 128, ly = (int)pData.leftStick.y - 128;
                if (lx > 30 || lx < -30) panMidiaX += lx / 12;
                if (ly > 30 || ly < -30) panMidiaY += ly / 12;
            }

            if (cross_consumido) pData.buttons &= ~ORBIS_PAD_BUTTON_CROSS;
            if (circle_consumido) pData.buttons &= ~ORBIS_PAD_BUTTON_CIRCLE;

            if (video_minimizado && !pip_selecionado) { bloqueio_fechar_video = true; }
            else { bloqueio_fechar_video = false; }

            if (!esperandoNomePasta && !esperandoRenomear && !visualizandoMidiaImagem && !visualizandoMidiaTexto && !visualizandoPic1 && !visualizandoPDF) {
                if (!emuladorAtivo && (!videoRodando || (video_minimizado && !menu_video_aberto && !pip_selecionado))) {
                    MenuLevel menuAntesDoControle = menuAtual;
                    int selecaoAntesDoControle = sel;
                    processarControles(pData.buttons, uId, imeSetting, imeTitle);
                    if (menuAtual != menuAntesDoControle &&
                        (menuAtual == ROOT || menuAtual == MENU_TIPO_JOGO || menuAtual == MENU_JOGAR_PS4 ||
                         menuAtual == MENU_EXPLORAR_HOME || menuAtual == MENU_EXPLORAR ||
                         menuAntesDoControle == MENU_CONTROLE_TESTE || menuAntesDoControle == MENU_INSTRUMENTOS)) {
                        FILE* navLog = fopen("/data/HyperNeiva/navigation_debug_log.txt", "a");
                        if (navLog) {
                            fprintf(navLog, "menu:%d->%d sel:%d->%d backImg:%d\n", (int)menuAntesDoControle,
                                (int)menuAtual, selecaoAntesDoControle, sel, backImg != NULL ? 1 : 0);
                            fclose(navLog);
                        }
                    }
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

            if (emuladorAtivo) {
                if (menuAtual == 50) { // MENU_EMU_CONTROLES (controle_emulador.cpp)
                    atualizarMapeamentoControles(&pData, ultimos_botoes_globais);
                    pData.buttons = 0;
                }
                else if (menuAtual == 51 || menuEmuladorAtivo) {
                    // Os botões do menu são tratados só no bloco do gatilho, lá em cima:
                    // dois leitores faziam cada toque andar dois itens.
                    menuEmuladorAtivo = true;
                    bridge_set_pausado(true);
                    pData.buttons = 0;
                }
                else bridge_set_pausado(false);   // o menu só abre com L1 + R1 + Options + Touchpad
            }
            else {
                // OPTIONS nos menus do app não pode abrir o menu de pausa do emulador.
                menuEmuladorAtivo = false;
            }

            botoesConsumidos |= botoesFisicos & ~botoes_preservados;
            ultimos_botoes_globais = botoesFisicos;
            ultimos_botoes = botoesFisicos;
        }

        extern void atenderFimDeVideo(), conquistasAtualizar();
        atenderFimDeVideo();
        conquistasAtualizar();

        bool telaSemVideo = (menuAtual == MENU_INSTRUMENTOS || menuAtual == MENU_CONTROLE_TESTE || (menuAtual == MENU_TELEGRAM && !videoRodando) ||
            menuAtual == MENU_NOTEPAD || menuAtualEsq == MENU_NOTEPAD ||
            visualizandoPDF || visualizandoMidiaTexto || visualizandoMidiaImagem ||
            visualizandoPic1 || emuladorAtivo);
        if (telaSemVideo) pausarVideoParaTela();
        else retomarVideoAposTela();

        // A troca de vídeo precisa vir antes de pegar o buffer: ela reinicia o motor de vídeo.
        bool videoNaTela = videoRodando && !telaSemVideo;
        if (videoNaTela) {
            verificarTrocaDeVideo();
            videoNaTela = videoRodando;
        }
        definirApresentacaoVideo(videoNaTela);

        if (emuladorAtivo) {
            if (menuEmuladorAtivo) {
                // Monta em RAM e copia de uma vez: desenhar na tela aos poucos faz o menu piscar.
                static uint32_t* telaMenuEmulador = (uint32_t*)malloc(1920 * 1080 * 4);
                uint32_t* tela = obterBufferVideo();
                uint32_t* alvo = telaMenuEmulador ? telaMenuEmulador : tela;
                if (menuAtual == 50) desenharAjudaControles(alvo);
                else desenharMenuEmulador(alvo);
                if (telaMenuEmulador) memcpy(tela, telaMenuEmulador, 1920 * 1080 * 4);
            } else {
                bridge_atualizar();
            }
            submeterTelaSemPausa();
            continue; 
        }

        extern void atualizarImePasta_explorar();

        // Tela cheia: o vídeo cobre o menu, então não há por que desenhá-lo. A thread do
        // vídeo cuida da tela, e aqui fica só a leitura do controle (33 ms mantém o ritmo
        // de repetição dos botões do menu do player, que dependem da volta deste loop).
        if (videoNaTela && !video_minimizado) {
            atualizarImePasta(); atualizarImePasta_explorar();
            if (msgTimer > 0) msgTimer--;
            sceKernelUsleep(33000);
            continue;
        }

        // Com o vídeo em PiP este buffer é a camada da interface: o vídeo entra por cima na
        // thread do vídeo, sem depender do tempo que o menu leva para ser desenhado.
        uint32_t* p = obterBufferVideo();
        // Com tema, o fundo vem dele: copiar a tela duas vezes custa um quadro inteiro.
        extern bool temaCobreMenu();
        if (!temaCobreMenu()) desenharFundoMenu(p);

        atualizarImePasta(); atualizarImePasta_explorar();

        desenharInterface(p);
        extern void desenharRelogio(uint32_t* p);
        desenharRelogio(p);
        extern void hnAtualizarExtracao();
        hnAtualizarExtracao();

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

        submeterTela();
    }
}


