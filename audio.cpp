#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <time.h> 

#ifdef __INTELLISENSE__
#ifndef __builtin_va_list
#define __builtin_va_list va_list
#endif
#endif

#ifndef _WIN32
#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#else
#include <SDL2/SDL.h>
#endif
#include "audio.h"
#include "audio_radio.h"
#include "audio_musica.h"
#include "audio_emulador.h"
#include "explorar.h" 
#include "elementos_sonoros.h" 
#include "instrumentos.h"
#include "video.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

static int audioPort = -1;
static volatile bool audioRodando = false;
static pthread_t audioThreadId;
static bool sistemaAudioIniciado = false;

static volatile bool comandoTrocar = false;
volatile bool comandoPausar = false;

volatile int modoReproducao = 0;

volatile int comandoBuscarSegundos = 0;
int volumeGeral = 100;
char musicaAtual[256] = "PARADO";
char ultimaMusicaTocada[256] = "";

char caminhosMusicasMenu[3000][256];
char caminhoNavegacaoMusicas[512] = "/data/HyperNeiva/Musicas";

volatile float audioTempoAtual = 0.0f;

// Emulator audio logic moved to audio_emu.cpp

static AudioType currentAudioType = AUDIO_NONE;
static bool radioPausadoPeloModoEspecial = false;
// O vídeo e o emulador pausam a música sem mudar a escolha do usuário.
static volatile bool musicaPausadaPeloVideo = false;

void definirAudioModoEspecial(bool ativo) {
    if (ativo) {
        if (!radioPausadoPeloModoEspecial && isRadioRodando() && !comandoPausar) {
            radioPausadoPeloModoEspecial = true;
            comandoPausar = true;
        }
        return;
    }

    if (radioPausadoPeloModoEspecial) {
        radioPausadoPeloModoEspecial = false;
        comandoPausar = false;
    }
}

void adiantarAudio() { comandoBuscarSegundos = 10; }
void retrocederAudio() { comandoBuscarSegundos = -10; }

void aumentarVolume() {
    volumeGeral += 10;
    if (volumeGeral > 100) volumeGeral = 100;
    salvarConfiguracaoAudio();
}

void diminuirVolume() {
    volumeGeral -= 10;
    if (volumeGeral < 0) volumeGeral = 0;
    salvarConfiguracaoAudio();
}

// Grava em outro arquivo e troca no fim: abrir o original com "wb" zerava a configuração
// se o app fechasse antes de o sistema gravar, e a música voltava ligada.
void salvarConfiguracaoAudio() {
    const char* destino = "/data/HyperNeiva/configuracao/audio_settings.bin";
    const char* temporario = "/data/HyperNeiva/configuracao/audio_settings.tmp";
    FILE* f = fopen(temporario, "wb");
    if (f) {
        fwrite(musicaAtual, 1, sizeof(musicaAtual), f);
        fwrite(&volumeGeral, 1, sizeof(int), f);
        int modoSalvar = (int)modoReproducao;
        fwrite(&modoSalvar, 1, sizeof(int), f);
        fwrite(ultimaMusicaTocada, 1, sizeof(ultimaMusicaTocada), f);

        int pausado = (comandoPausar && !musicaPausadaPeloVideo && !radioPausadoPeloModoEspecial) ? 1 : 0;
        fwrite(&pausado, 1, sizeof(int), f);

        fflush(f);
#ifndef _WIN32
        fsync(fileno(f));
#endif
        fclose(f);
#ifdef _WIN32
        remove(destino);
#endif
        rename(temporario, destino);
    }
}

#define MUSICA_PADRAO "/app0/assets/audio/bgm.mp3"
#define VOLUME_PADRAO 20
#define MARCA_MUSICA_PADRAO "/data/HyperNeiva/configuracao/bgm_padrao_v1.txt"
#define MARCA_PAUSA_LIMPA "/data/HyperNeiva/configuracao/bgm_pausa_v2.txt"

// Mesma ordem de salvarConfiguracaoAudio.
void carregarConfiguracaoAudio() {
    strcpy(musicaAtual, MUSICA_PADRAO);
    volumeGeral = VOLUME_PADRAO;
    modoReproducao = (decltype(modoReproducao))0;
    strcpy(ultimaMusicaTocada, "");
    comandoPausar = false;

    FILE* marca = fopen(MARCA_MUSICA_PADRAO, "r");
    if (!marca) {
        // Primeira vez com a música nova: vale para quem já tinha configuração salva.
        marca = fopen(MARCA_MUSICA_PADRAO, "w");
        if (marca) fclose(marca);
        salvarConfiguracaoAudio();
        return;
    }
    fclose(marca);

    FILE* f = fopen("/data/HyperNeiva/configuracao/audio_settings.bin", "rb");
    if (!f) return;
    char salva[sizeof(musicaAtual)] = "";
    int volume = VOLUME_PADRAO, modo = 0, pausado = 0;
    bool limparPausa = false;
    bool leu = fread(salva, 1, sizeof(salva), f) == sizeof(salva) && fread(&volume, 1, sizeof(int), f) == sizeof(int) && fread(&modo, 1, sizeof(int), f) == sizeof(int);
    if (leu) {
        fread(ultimaMusicaTocada, 1, sizeof(ultimaMusicaTocada), f);
        ultimaMusicaTocada[sizeof(ultimaMusicaTocada) - 1] = '\0';
        if (fread(&pausado, 1, sizeof(int), f) != sizeof(int)) pausado = 0;
        salva[sizeof(salva) - 1] = '\0';
        // Rádio não reconecta sozinho ao abrir o app.
        if (salva[0] && strncmp(salva, "http", 4) != 0) strcpy(musicaAtual, salva);
        if (volume >= 0 && volume <= 100) volumeGeral = volume;
        modoReproducao = (decltype(modoReproducao))modo;
        // Até a 2.60 religar a música gravava "pausado": limpa uma vez para ela voltar a tocar.
        FILE* limpa = fopen(MARCA_PAUSA_LIMPA, "r");
        if (limpa) fclose(limpa);
        else {
            limparPausa = true;
            pausado = 0;
            limpa = fopen(MARCA_PAUSA_LIMPA, "w");
            if (limpa) fclose(limpa);
        }
        comandoPausar = pausado == 1;
    }
    fclose(f);
    if (limparPausa) salvarConfiguracaoAudio();
}

// Pausa do vídeo ou do emulador não conta como desligada: a música volta quando eles fecham.
bool musicaFundoLigada() {
    return strcmp(musicaAtual, "PARADO") != 0 && (!comandoPausar || musicaPausadaPeloVideo || radioPausadoPeloModoEspecial);
}

void tocarMusicaPadrao() { tocarMusicaNova(MUSICA_PADRAO); }

// Ao religar, volta a música que estava tocando.
void alternarMusicaFundo() {
    if (musicaFundoLigada()) {
        strcpy(ultimaMusicaTocada, musicaAtual);
        tocarMusicaNova("PARADO");
        return;
    }
    if (strcmp(musicaAtual, "PARADO") != 0) tocarMusicaNova(musicaAtual);
    else tocarMusicaNova(ultimaMusicaTocada[0] ? ultimaMusicaTocada : MUSICA_PADRAO);
}

void pausarMusicaParaVideo() {
    if (!comandoPausar && strcmp(musicaAtual, "PARADO") != 0) {
        comandoPausar = true;
        musicaPausadaPeloVideo = true;
    }
}

void retomarMusicaAposVideo() {
    if (musicaPausadaPeloVideo) comandoPausar = false;
    musicaPausadaPeloVideo = false;
}

// HN_MP3_INICIO
// A saída de som é fixa em 48 kHz estéreo. Sem esta conversão, MP3 em 44,1 kHz tocava agudo e
// WAV de voz (16 kHz, mono) tocava três vezes mais rápido. Serve aos dois formatos.
// 'posicao' é o quadro de origem em que o player está: se não bate com o que o conversor
// entregou (música nova ou busca), ele joga fora o que tinha guardado.
static size_t lerEm48k(drwav* wav, drmp3* mp3, uint64_t posicao, int16_t* saida, size_t frames, size_t* consumidos) {
    static int16_t bruto[256 * 8], bloco[256 * 2];
    static size_t total = 0, pos = 0;
    static double fracao = 0.0;
    static int16_t antesE = 0, antesD = 0;
    static uint64_t entregue = (uint64_t)-1;
    if (posicao != entregue) { total = 0; pos = 0; fracao = 0.0; antesE = antesD = 0; }
    uint32_t taxa = wav ? wav->sampleRate : mp3->sampleRate;
    uint32_t canais = wav ? wav->channels : mp3->channels;
    if (taxa == 0) taxa = 48000;
    if (canais < 1) canais = 1;
    double passo = (double)taxa / 48000.0;
    size_t feitos = 0;
    *consumidos = 0;
    while (feitos < frames) {
        bool acabou = false;
        while (!acabou && (fracao >= 1.0 || pos >= total)) {
            if (pos >= total) {
                size_t pedir = canais <= 8 ? 256 : 0;
                size_t n = 0;
                if (pedir) n = wav ? (size_t)drwav_read_pcm_frames_s16(wav, pedir, bruto) : (size_t)drmp3_read_pcm_frames_s16(mp3, pedir, bruto);
                if (n == 0) { acabou = true; break; }
                for (size_t i = 0; i < n; i++) {
                    bloco[i * 2] = bruto[i * canais];
                    bloco[i * 2 + 1] = bruto[i * canais + (canais > 1 ? 1 : 0)];
                }
                total = n;
                pos = 0;
            }
            if (fracao >= 1.0) {
                antesE = bloco[pos * 2];
                antesD = bloco[pos * 2 + 1];
                pos++;
                fracao -= 1.0;
                (*consumidos)++;
            }
        }
        if (acabou) break;
        saida[feitos * 2] = (int16_t)(antesE + (bloco[pos * 2] - antesE) * fracao);
        saida[feitos * 2 + 1] = (int16_t)(antesD + (bloco[pos * 2 + 1] - antesD) * fracao);
        feitos++;
        fracao += passo;
    }
    entregue = posicao + *consumidos;
    return feitos;
}
// HN_MP3_FIM

// Playlist and file selection logic moved to audio_playlist.cpp

// Funções de rádio movidas para audio_radio.cpp

#ifdef _WIN32
SDL_AudioDeviceID winAudioDevice = 0;
#endif

void misturarFalaIA(int16_t* saida, int amostras);   // network.cpp

static void* audioThreadFunc(void* argp) {
#ifndef _WIN32
    // Cada bloco de som dura 5 ms: com as threads do vídeo ocupando os núcleos, esta thread
    // precisa de prioridade para não falhar (256 = máxima, 700 = padrão).
    scePthreadSetprio(scePthreadSelf(), 300);
    if (!sistemaAudioIniciado) { sceAudioOutInit(); sistemaAudioIniciado = true; }
    int32_t userId;
    if (sceUserServiceGetInitialUser(&userId) < 0) userId = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
    audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (audioPort < 0) { audioRodando = false; return NULL; }
#else
    SDL_AudioSpec wanted_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = 48000;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 512;
    wanted_spec.callback = NULL;

    winAudioDevice = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, NULL, 0);
    if (winAudioDevice > 0) {
        SDL_PauseAudioDevice(winAudioDevice, 0);
    }
#endif

    drwav wav; drmp3 mp3;
    int16_t pSampleData[256 * 2];
    char caminhoAudio[256];
    uint64_t currentFrame = 0;

    if (!comandoPausar && prepararArquivoAudio(caminhoAudio)) {
        if (strncmp(caminhoAudio, "http", 4) == 0) {
            if (iniciarRadio(caminhoAudio)) {
                currentAudioType = AUDIO_STREAM;
            }
            else {
                extern char msgStatus[128]; extern int msgTimer; extern unsigned int msgStatusColor;
                snprintf(msgStatus, sizeof(msgStatus), "Erro ao conectar no servidor da Radio.");
                msgTimer = 400; msgStatusColor = 0xFF0000FF;
            }
        }
        else if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
            if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
        }
        else {
            if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
        }
    }

    while (audioRodando) {
#ifndef _WIN32
        if (audioPort < 0) {
            audioPort = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        }
#endif

        // TELEMETRIA: Observa a Net a baixar e o FFmpeg a "comer"
        if (currentAudioType == AUDIO_STREAM && isRadioRodando() &&
            !comandoPausar && strcmp(musicaAtual, "PARADO") != 0) {
            extern char msgStatus[128];
            extern int msgTimer;
            extern unsigned int msgStatusColor;

            obterTelemetriaRadio(msgStatus, sizeof(msgStatus));
            msgTimer = 100;
            msgStatusColor = 0xFF00FF00;
        }

        if (comandoBuscarSegundos != 0) {
            if (currentAudioType != AUDIO_NONE) {
                int sampleRate = (currentAudioType == AUDIO_WAV) ? wav.sampleRate : mp3.sampleRate;
                int64_t frameOffset = (int64_t)comandoBuscarSegundos * sampleRate;
                int64_t targetFrame = (int64_t)currentFrame + frameOffset;

                if (targetFrame < 0) targetFrame = 0;

                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, (uint64_t)targetFrame);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, (uint64_t)targetFrame);
                currentFrame = (uint64_t)targetFrame;

                if (currentAudioType == AUDIO_WAV) audioTempoAtual = (float)currentFrame / wav.sampleRate;
                else if (currentAudioType == AUDIO_MP3) audioTempoAtual = (float)currentFrame / mp3.sampleRate;
            }
            comandoBuscarSegundos = 0;
        }

        if (comandoTrocar) {
            comandoTrocar = false;
            if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
            else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
            else if (currentAudioType == AUDIO_STREAM) pararRadioStreaming();
            currentAudioType = AUDIO_NONE;

            if (prepararArquivoAudio(caminhoAudio)) {
                if (strncmp(caminhoAudio, "http", 4) == 0) {
                    if (iniciarRadio(caminhoAudio)) {
                        currentAudioType = AUDIO_STREAM;
                    }
                    else {
                        strcpy(musicaAtual, "PARADO");
                        comandoPausar = true;
                        pararRadioStreaming();
                    }
                }
                else if (strstr(caminhoAudio, ".mp3") || strstr(caminhoAudio, ".MP3")) {
                    if (drmp3_init_file(&mp3, caminhoAudio, NULL)) currentAudioType = AUDIO_MP3;
                }
                else {
                    if (drwav_init_file(&wav, caminhoAudio, NULL)) currentAudioType = AUDIO_WAV;
                }
            }
            currentFrame = 0;
            audioTempoAtual = 0.0f;
            FILE* registro = fopen("/data/HyperNeiva/zip_debug_log.txt", "a");
            if (registro) { fprintf(registro, "MUSICA tipo=%d pausa=%d volume=%d %s\n", (int)currentAudioType, comandoPausar ? 1 : 0, volumeGeral, musicaAtual); fclose(registro); }
        }

        if (comandoPausar || currentAudioType == AUDIO_NONE) {
            for (int i = 0; i < 256 * 2; i++) pSampleData[i] = 0;
            misturarEfeitosSonoros(pSampleData, 256);
            misturarAudioPiano(pSampleData, 256);
            misturarAudioVideo(pSampleData, 256);
            misturarAudioEmulador(pSampleData, 256);
            misturarFalaIA(pSampleData, 256);   // voz da IA (network.cpp)
#ifdef _WIN32
            if (winAudioDevice > 0) {
                SDL_QueueAudio(winAudioDevice, pSampleData, 256 * 2 * sizeof(int16_t));
                while (SDL_GetQueuedAudioSize(winAudioDevice) > 48000 * 2 * sizeof(int16_t) * 0.15) {
                    SDL_Delay(5);
                }
            }
#else
            if (audioPort >= 0) sceAudioOutOutput(audioPort, pSampleData);
#endif
            continue;
        }

        size_t framesLidos = 0, framesConsumidos = 0;
        uint32_t currentChannels = 2;

        if (currentAudioType == AUDIO_WAV) {
            framesLidos = lerEm48k(&wav, NULL, currentFrame, pSampleData, 256, &framesConsumidos);
            currentChannels = 2;
        }
        else if (currentAudioType == AUDIO_MP3) {
            framesLidos = lerEm48k(NULL, &mp3, currentFrame, pSampleData, 256, &framesConsumidos);
            currentChannels = 2;
        }
        else if (currentAudioType == AUDIO_STREAM) {
            framesLidos = lerFrameRadio(pSampleData, 256);
            if (framesLidos == 0 && !isRadioRodando()) {
                // Erro fatal real (EOF) ou desconexão
                strcpy(musicaAtual, "PARADO");
                comandoPausar = true;
            }
            if (framesLidos == 0 && isRadioRodando()) {
                // Keep audio clock moving through temporary network gaps.
                memset(pSampleData, 0, 256 * 2 * sizeof(int16_t));
                framesLidos = 256;
            }
            currentChannels = 2;
        }

        if (framesLidos > 0 && currentChannels == 1) {
            for (int i = framesLidos - 1; i >= 0; i--) {
                pSampleData[i * 2] = pSampleData[i];
                pSampleData[i * 2 + 1] = pSampleData[i];
            }
        }

        currentFrame += (currentAudioType == AUDIO_MP3 || currentAudioType == AUDIO_WAV) ? framesConsumidos : framesLidos;

        if (currentAudioType == AUDIO_WAV) {
            audioTempoAtual = (float)currentFrame / wav.sampleRate;
        }
        else if (currentAudioType == AUDIO_MP3) {
            audioTempoAtual = (float)currentFrame / mp3.sampleRate;
        }

        if (framesLidos == 0) {
            if (modoReproducao == 1 || strcmp(musicaAtual, MUSICA_PADRAO) == 0) {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
                audioTempoAtual = 0.0f;
            }
            else if (strstr(musicaAtual, "/data/HyperNeiva/Musicas/") != NULL) {
                char proxima[256];
                if (obterProximaMusica(proxima)) {
                    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
                    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
                    currentAudioType = AUDIO_NONE;

                    strcpy(musicaAtual, proxima);
                    salvarConfiguracaoAudio();

                    bool sucesso = false;
                    if (strstr(musicaAtual, ".mp3") || strstr(musicaAtual, ".MP3")) {
                        if (drmp3_init_file(&mp3, musicaAtual, NULL)) { currentAudioType = AUDIO_MP3; sucesso = true; }
                    }
                    else {
                        if (drwav_init_file(&wav, musicaAtual, NULL)) { currentAudioType = AUDIO_WAV; sucesso = true; }
                    }
                    if (sucesso) {
                        currentFrame = 0;
                        audioTempoAtual = 0.0f;
                        continue;
                    }
                }
            }
            else {
                if (currentAudioType == AUDIO_WAV) drwav_seek_to_pcm_frame(&wav, 0);
                else if (currentAudioType == AUDIO_MP3) drmp3_seek_to_pcm_frame(&mp3, 0);
                currentFrame = 0;
                audioTempoAtual = 0.0f;
            }
            continue;
        }

        if (framesLidos < 256) {
            for (size_t i = framesLidos * 2; i < 256 * 2; i++) pSampleData[i] = 0;
        }

        if (volumeGeral < 100) {
            float fatorVolume = volumeGeral / 100.0f;
            for (int i = 0; i < 256 * 2; i++) {
                pSampleData[i] = (int16_t)(pSampleData[i] * fatorVolume);
            }
        }

        misturarEfeitosSonoros(pSampleData, 256);
        misturarAudioPiano(pSampleData, 256);
        misturarAudioVideo(pSampleData, 256);
        misturarAudioEmulador(pSampleData, 256);
        misturarFalaIA(pSampleData, 256);   // voz da IA (network.cpp)
#ifdef _WIN32
        if (winAudioDevice > 0) {
            SDL_QueueAudio(winAudioDevice, pSampleData, 256 * 2 * sizeof(int16_t));
            while (SDL_GetQueuedAudioSize(winAudioDevice) > 48000 * 2 * sizeof(int16_t) * 0.15) {
                SDL_Delay(5);
            }
        }
#else
        if (audioPort >= 0) sceAudioOutOutput(audioPort, pSampleData);
#endif
    }

    if (currentAudioType == AUDIO_WAV) drwav_uninit(&wav);
    else if (currentAudioType == AUDIO_MP3) drmp3_uninit(&mp3);
    else if (currentAudioType == AUDIO_STREAM) pararRadioStreaming();
    currentAudioType = AUDIO_NONE;
#ifdef _WIN32
    if (winAudioDevice > 0) SDL_CloseAudioDevice(winAudioDevice);
#else
    sceAudioOutClose(audioPort);
#endif
    return NULL;
}

void inicializarAudio() {
    if (audioRodando) return;
    srand(time(NULL));
    // Note: avformat_network_init calls moved to audio_radio or handled globally

    sceKernelMkdir("/data/HyperNeiva/Musicas", 0777);
    carregarConfiguracaoAudio();
    audioRodando = true;

    if (strcmp(musicaAtual, "PARADO") == 0) comandoPausar = true;

    comandoTrocar = false;
    if (pthread_create(&audioThreadId, NULL, audioThreadFunc, NULL) != 0) audioRodando = false;
}

void pararAudio() {
    if (!audioRodando) return;
    audioRodando = false;
    pthread_join(audioThreadId, NULL);
}

void tocarMusicaNova(const char* path) {
    if (strcmp(path, "PARADO") == 0) {
        radioPausadoPeloModoEspecial = false;
        musicaPausadaPeloVideo = false;
        strcpy(musicaAtual, "PARADO");
        comandoPausar = true;
        comandoTrocar = false;
        salvarConfiguracaoAudio();
        return;
    }
    if (strncmp(path, "http", 4) == 0) {
        extern char msgStatus[128];
        extern int msgTimer;
        extern unsigned int msgStatusColor;
        strcpy(msgStatus, "Conectando ao Servidor...");
        msgTimer = 200;
        msgStatusColor = 0xFF00FF00;
    }
    // path pode ser a própria musicaAtual ou ultimaMusicaTocada.
    char destino[sizeof(musicaAtual)];
    snprintf(destino, sizeof(destino), "%s", path);
    strcpy(musicaAtual, destino);
    strcpy(ultimaMusicaTocada, destino);
    radioPausadoPeloModoEspecial = false;
    musicaPausadaPeloVideo = false;
    comandoPausar = false;
    salvarConfiguracaoAudio();
    comandoTrocar = true;
}

void tocarProximaMusica() {
    bool estavaParado = false;
    if (strcmp(musicaAtual, "PARADO") == 0) {
        if (strlen(ultimaMusicaTocada) == 0) return;
        strcpy(musicaAtual, ultimaMusicaTocada);
        estavaParado = true;
    }

    char proxima[256];
    if (obterProximaMusica(proxima)) {
        tocarMusicaNova(proxima);
    }
    else if (estavaParado) {
        strcpy(musicaAtual, "PARADO");
    }
}

void tocarMusicaAnterior() {
    bool estavaParado = false;
    if (strcmp(musicaAtual, "PARADO") == 0) {
        if (strlen(ultimaMusicaTocada) == 0) return;
        strcpy(musicaAtual, ultimaMusicaTocada);
        estavaParado = true;
    }

    char anterior[256];
    if (obterMusicaAnterior(anterior)) {
        tocarMusicaNova(anterior);
    }
    else if (estavaParado) {
        strcpy(musicaAtual, "PARADO");
    }
}