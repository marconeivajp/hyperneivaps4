# libtorrent para o PS4

O torrent do Hyper Neiva usa a libtorrent 2.1.2 com a Boost 1.92, compiladas com o OpenOrbis. O app fala com ela só pela interface em C de `compat/hn_torrent_api.cpp`.

A biblioteca pronta, `libhntorrent.a`, fica em `hntorrent_ps4_lib.rar` na release. Só recompile se for mudar a libtorrent.

## Recompilar do zero

Precisa do `td_ps4` ao lado desta pasta, com os cabeçalhos do OpenSSL em `deps/include` e os ajustes em `compat`.

1. Copie esta pasta para `lt_ps4`, ao lado do `td_ps4`.
2. Extraia em `lt_ps4\src`:
   - `libtorrent-rasterbar-2.1.2.tar.gz`, da release v2.1.2 de `arvidn/libtorrent`
   - `boost-1.92.0-b2-nodocs.7z`, da release boost-1.92.0 de `boostorg/boost`
3. Rode `python ajustar.py`.
4. Rode `python compilar.py -j 4`. Leva cerca de 40 minutos e gera `lib\libhntorrent.a`.

`compilar.py` só olha a data das fontes. Ao mudar `compat\ps4_lt.h` ou um cabeçalho da Boost, apague `obj\*.o` antes.

## O que muda no PS4

- `ps4_lt.h` troca as constantes do SDK que vêm do Linux pelas do kernel do PS4, que é FreeBSD.
- `pthread_condattr_t` do SDK tem 4 bytes e o sistema grava 8: sem o ajuste em `posix_event.ipp`, o app fecha ao criar a sessão.
- DNS usa `sceNetResolver`, porque o console não tem `/etc/resolv.conf`.
- Sem kqueue, epoll, eventfd e mmap de disco: rede por `select`, disco por `posix_disk_io`.
- O sistema não exporta `__cxa_thread_atexit_impl`: `hn_ps4_net.cpp` traz uma versão vazia.

O log fica em `/data/HyperNeiva/torrent/torrent_log.txt` no console.
