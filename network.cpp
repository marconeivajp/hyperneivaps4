#include "network.h"
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <stddef.h>

int netPoolId = -1;
int httpCtxId = -1;
int sslCtxId = -1;

// Ignora erros de verificação de certificado SSL (necessário para alguns scrapers)
int skipSslCallback(int libsslId, unsigned int verifyErr, void* const sslCert[], int certNum, void* userArg) {
    return 1;
}

// Inicializa todos os módulos de rede do OrbisOS
void initNetwork() {
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);

    sceNetInit();
    netPoolId = sceNetPoolCreate("netPool", 16384, 0);
    sslCtxId = sceSslInit(256 * 1024);
    httpCtxId = sceHttpInit(netPoolId, sslCtxId, 128 * 1024);
}