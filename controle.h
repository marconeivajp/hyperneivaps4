#ifndef CONTROLE_H
#define CONTROLE_H

#include <stdint.h>

// --- PROTÓTIPOS DAS FUNÇÕES ---

/**
 * @brief Processa todas as entradas do comando DualShock 4.
 * * Esta função centraliza a lógica de navegação, confirmação e cancelamento,
 * agindo de forma diferente dependendo do menu atual definido no sistema.
 * * @param buttons Máscara de bits contendo os botões pressionados (pData.buttons).
 * @param userId ID do utilizador ativo para operações de sistema como o teclado.
 */
void processarComando(uint32_t buttons, int32_t userId);

#endif