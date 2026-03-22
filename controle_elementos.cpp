#include "controle_elementos.h"
#include "editar.h"
#include <orbis/Pad.h>
#include <orbis/UserService.h>

static int padHandleElemento = -1;

void atualizarControleElementos() {
    if (!ctrl1On) return;

    if (padHandleElemento < 0) {
        int32_t uId;
        // Pega o utilizador atual e abre uma porta de leitura contínua
        if (sceUserServiceGetInitialUser(&uId) == 0) {
            padHandleElemento = scePadOpen(uId, 0, 0, NULL);
        }
    }

    if (padHandleElemento >= 0) {
        OrbisPadData pData;
        if (scePadReadState(padHandleElemento, &pData) == 0) {
            int valX = pData.rightStick.x;
            int valY = pData.rightStick.y;

            // Centro é ~128. Se sair da zona morta (105 a 150), ele move!
            // Quanto mais você empurra o analógico, mais rápido ele vai.
            if (valX > 150 || valX < 105) {
                ctrl1X += (valX - 128) / 3;
            }
            if (valY > 150 || valY < 105) {
                ctrl1Y += (valY - 128) / 3;
            }

            // Travas invisíveis para não perder a imagem fora da TV
            if (ctrl1X < -500) ctrl1X = -500;
            if (ctrl1X > 2400) ctrl1X = 2400;
            if (ctrl1Y < -500) ctrl1Y = -500;
            if (ctrl1Y > 1500) ctrl1Y = 1500;
        }
    }
}