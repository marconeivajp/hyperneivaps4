// DNS e endereço local no PS4. O getaddrinfo do libc do SDK lê /etc/resolv.conf, que o console não tem.
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <orbis/Net.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int hnPoolDns() {
    static int pool = -1;
    if (pool < 0) pool = sceNetPoolCreate("hnltdns", 16384, 0);
    return pool;
}

static int resolver(const char* host, uint32_t* ip) {
    int id = sceNetResolverCreate("hnlt", hnPoolDns(), 0);
    if (id < 0) return -1;
    OrbisNetInAddr saida;
    memset(&saida, 0, sizeof(saida));
    int r = sceNetResolverStartNtoa(id, host, &saida, 0, 0, 0);
    sceNetResolverDestroy(id);
    if (r < 0) return -1;
    memcpy(ip, &saida, sizeof(*ip));
    return 0;
}

extern "C" int hn_getaddrinfo(const char* host, const char* servico, const struct addrinfo* dica, struct addrinfo** saida) {
    int familia = dica ? dica->ai_family : AF_UNSPEC;
    int flags = dica ? dica->ai_flags : 0;
    if (familia == AF_INET6) return EAI_NONAME;

    uint32_t ip = 0;
    if (!host || !*host) ip = htonl((flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK);
    else if (inet_pton(AF_INET, host, &ip) != 1) {
        if ((flags & AI_NUMERICHOST) || strchr(host, ':')) return EAI_NONAME;
        if (resolver(host, &ip) != 0) return EAI_AGAIN;
    }

    struct Bloco {
        struct addrinfo info;
        struct sockaddr_in endereco;
    };
    Bloco* b = (Bloco*)calloc(1, sizeof(Bloco));
    if (!b) return EAI_MEMORY;
    b->endereco.sin_len = sizeof(b->endereco);
    b->endereco.sin_family = AF_INET;
    b->endereco.sin_port = htons(servico ? (uint16_t)strtoul(servico, NULL, 10) : 0);
    b->endereco.sin_addr.s_addr = ip;
    b->info.ai_family = AF_INET;
    b->info.ai_socktype = (dica && dica->ai_socktype) ? dica->ai_socktype : SOCK_STREAM;
    b->info.ai_protocol = dica ? dica->ai_protocol : 0;
    b->info.ai_addrlen = sizeof(b->endereco);
    b->info.ai_addr = (struct sockaddr*)&b->endereco;
    *saida = &b->info;
    return 0;
}

extern "C" void hn_freeaddrinfo(struct addrinfo* lista) { free(lista); }

// Conectar UDP não envia nada: só pede ao kernel qual IP local sai para a internet.
extern "C" unsigned int hn_ip_local(void) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return 0;
    struct sockaddr_in destino;
    memset(&destino, 0, sizeof(destino));
    destino.sin_len = sizeof(destino);
    destino.sin_family = AF_INET;
    destino.sin_port = htons(53);
    destino.sin_addr.s_addr = htonl(0x08080808);
    unsigned int ip = 0;
    if (connect(s, (struct sockaddr*)&destino, sizeof(destino)) == 0) {
        struct sockaddr_in local;
        socklen_t tamanho = sizeof(local);
        if (getsockname(s, (struct sockaddr*)&local, &tamanho) == 0) ip = ntohl(local.sin_addr.s_addr);
    }
    close(s);
    return ip;
}

// O sistema do PS4 não exporta este símbolo; o destrutor de variável thread_local só deixa de rodar quando a thread acaba.
extern "C" int __cxa_thread_atexit_impl(void (*)(void*), void*, void*) { return 0; }

// A pilha padrão de thread no PS4 é pequena para a libtorrent.
extern "C" int hn_pthread_create(pthread_t* id, const pthread_attr_t* atributos, void* (*funcao)(void*), void* dado) {
    if (atributos) return pthread_create(id, atributos, funcao, dado);
    pthread_attr_t novo;
    pthread_attr_init(&novo);
    pthread_attr_setstacksize(&novo, 2 * 1024 * 1024);
    int r = pthread_create(id, &novo, funcao, dado);
    pthread_attr_destroy(&novo);
    return r;
}
