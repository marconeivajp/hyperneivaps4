# Mudanças nos fontes dos núcleos

Os fontes ficam fora do repositório, em `C:\Users\Marco\source\repos\teste3\nucleos_fonte\`.
São clones dos repositórios oficiais do libretro, nas versões abaixo. Para refazer:
clone, faça checkout da versão, aplique o patch (`git apply`) e compile com `../compilar_fonte.sh`.

| Núcleo | Repositório | Versão | Patch |
|---|---|---|---|
| snes9x | libretro/snes9x | fae2fea | `snes9x.patch` (só marcas `HN` no log) |
| stella2014 | libretro/stella2014-libretro | 7d1361e | sem mudança |
| parallel-n64 | libretro/parallel-n64 | f80b843 | `parallel-n64.patch` |
| pcsx_rearmed | libretro/pcsx_rearmed | 7b0e8f8 | `pcsx_rearmed.patch` (Lightrec com buffer executável); crie `include/revision.h` com `#define REV "7b0e8f8"`. Compile com `-DLIGHTREC -DLIGHTREC_STATIC` no lugar de `-DDRC_DISABLE` |
| yabause (Saturn) | libretro/yabause | 8926b0c | `yabause.patch` |

No parallel-n64, o que faz o N64 rodar no PS4:
- `Graphics/plugins.c`: vídeo padrão angrylion (sem opções do frontend ficava em Glide64, que precisa de OpenGL).
- `libretro/libretro.c`: CPU padrão no interpretador puro (o cached interpreter travava na primeira escrita do boot, em 0xA4000064).
- `parallel_al.c`: angrylion com até 4 threads (a espera usa pthread via rthreads/eventcount com `ORBIS`).
- O resto do patch são marcas `HN` de diagnóstico no `nucleo_log.txt`, com limite de linhas.

No yabause, o que faz o Saturn rodar no PS4:
- `libretro.c`: 4 threads de desenho por padrão (`numthreads = 4`) e frameskip desligado (fazia os sprites piscarem).
- `libretro.c`: `YuiSwapBuffers` não envia quadro do VIDDummy.
- `rthreads.c`: pilha de 1 MB nas threads com `ORBIS`.
- Compilar com `-O3 -flto -DHAVE_THREADS=1 -DORBIS -D__ORBIS__`; o `vfs_implementation.c` vai sem `ORBIS` (pede `sys/dirent.h`).
- A ponte mantém o módulo carregado entre jogos (as threads dele nunca terminam).

Velocidade (todos os núcleos compilados do fonte): use `-O3 -flto` nos defines do `compilar_fonte.sh`.
No parallel-n64 passe também `-DORBIS -D__ORBIS__ -include libretro.h` para todas as fontes, e compile
o `libretro/libretro.c` à parte com `-DHAVE_OPENGLES2`.
