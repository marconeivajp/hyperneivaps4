// --- INÍCIO DO ARQUIVO menu_audio.h ---
#pragma once
#ifndef MENU_AUDIO_H
#define MENU_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

// A palavra 'extern' impede a duplicação!
extern const char* listaOpcoesAudio[11];
extern int selAudioOpcao;

void acaoTriangle_Musicas();
void tratarSelecaoAudio(int op);

#endif
// --- FIM DO ARQUIVO menu_audio.h ---