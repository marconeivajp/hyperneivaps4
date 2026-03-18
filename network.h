#ifndef NETWORK_H
#define NETWORK_H

// IDs globais dos contextos de rede
extern int netPoolId;
extern int httpCtxId;
extern int sslCtxId;

// Funções de inicialização e callback
void initNetwork();
int skipSslCallback(int libsslId, unsigned int verifyErr, void* const sslCert[], int certNum, void* userArg);

#endif