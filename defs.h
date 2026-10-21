#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define AF_UNIX 1
#define SOCK_STREAM 1

#ifndef _WIN32
int socketpair(int domain, int type, int protocol, int* out);
ssize_t read(int fd, void* dst, size_t sz);
ssize_t write(int fd, const void* dst, size_t sz);
int close(int fd);
#else
#include <io.h>
#define socketpair(domain, type, protocol, out) (-1)
#endif