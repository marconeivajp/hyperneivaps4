// Incluído à força em toda a libtorrent. O SDK do OpenOrbis mistura valores do Linux com o kernel do PS4,
// que é FreeBSD: aqui ficam os valores que o kernel entende.
#pragma once
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define HN_PS4 1

#ifdef __cplusplus
#include <cstdlib>
// O libc++ do SDK não publica estas duas em std.
namespace std { using ::strtoll; using ::strtoull; }
#endif

#undef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x20000
#undef MSG_DONTWAIT
#define MSG_DONTWAIT 0x80
#undef MSG_EOR
#define MSG_EOR 0x8
#undef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0x20000000
#undef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0x10000000
#undef IP_TOS
#define IP_TOS 3
#undef IP_TTL
#define IP_TTL 4
#undef IP_MULTICAST_IF
#define IP_MULTICAST_IF 9
#undef IP_MULTICAST_TTL
#define IP_MULTICAST_TTL 10
#undef IP_MULTICAST_LOOP
#define IP_MULTICAST_LOOP 11
#undef IP_ADD_MEMBERSHIP
#define IP_ADD_MEMBERSHIP 12
#undef IP_DROP_MEMBERSHIP
#define IP_DROP_MEMBERSHIP 13
#undef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#undef FIONBIO
#define FIONBIO 0x8004667e
#undef FIONREAD
#define FIONREAD 0x4004667f
#ifndef SO_NOSIGPIPE
#define SO_NOSIGPIPE 0x0800
#endif

// O sockaddr_in6 do SDK não tem sin6_len: o byte baixo é o tamanho e o alto é a família.
#define HN_SIN6_FAMILY ((AF_INET6 << 8) | 28)

#ifdef __cplusplus
extern "C" {
#endif
int hn_getaddrinfo(const char* host, const char* servico, const struct addrinfo* dica, struct addrinfo** saida);
void hn_freeaddrinfo(struct addrinfo* lista);
unsigned int hn_ip_local(void);
void hn_marca(const char* texto);
int hn_pthread_create(pthread_t* id, const pthread_attr_t* atributos, void* (*funcao)(void*), void* dado);
#ifdef __cplusplus
}
#endif
#define getaddrinfo hn_getaddrinfo
#define freeaddrinfo hn_freeaddrinfo
#define pthread_create hn_pthread_create
