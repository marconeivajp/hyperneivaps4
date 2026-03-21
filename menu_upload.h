#pragma once
#pragma once
#ifndef MENU_UPLOAD_H
#define MENU_UPLOAD_H

#include <stdint.h>

extern bool showUploadOpcoes;
extern int selUploadOpcao;

void desenharMenuUpload(uint32_t* p);
void acaoCross_MenuUpload();
void acaoTriangle_MenuUpload();
void acaoCircle_MenuUpload();

#endif