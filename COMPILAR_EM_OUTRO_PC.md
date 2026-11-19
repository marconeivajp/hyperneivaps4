# Compilar o Hyper Neiva em outro PC

Tudo que o projeto usa está no GitHub: o código neste repositório (branch `telegram`) e as
dependências grandes, já compiladas para o PS4, na release
[`dependencias-ps4`](https://github.com/marconeivajp/hyperneivaps4/releases/tag/dependencias-ps4).

## 1. Programas no Windows

- LLVM/Clang em `C:\Program Files\LLVM` (o build usa `clang++.exe` e `ld.lld`).
- MSYS2 em `C:\msys64` (bash, `readelf`, `g++` para o Hyper Neiva de PC).
- Python 3 e Git.

## 2. Dependências (release `dependencias-ps4`)

Baixe e extraia (`tar -xzf arquivo.tar.gz`) mantendo estes lugares:

| Arquivo | Extrair em | O que é |
|---|---|---|
| `OpenOrbis.tar.gz` | `C:\` (vira `C:\OpenOrbis`) | Toolchain OpenOrbis do PS4 (headers, libs, `create-fself`, `PkgTool`) |
| `td_ps4.tar.gz` | `C:\Users\<voce>\source\repos\teste3\` | TDLib (Telegram) compilado para o PS4, com fontes e objetos |
| `lt_ps4.tar.gz` | idem | libtorrent + Boost + OpenSSL para o PS4 |
| `nucleos_ps4.tar.gz` | idem | Núcleos de emulador em `.prx` e os scripts de montagem |
| `nucleos_fonte.tar.gz` | idem | Fontes dos núcleos (libretro) |
| `vulkan_ps4.tar.gz` | idem | Testes do Vulkan para o PS4 |
| `ffmpeg_ps4.tar.gz` | idem | FFmpeg 5.0 recompilado para o PS4 com os codificadores do editor de vídeo (MPEG-4, AAC, MP4). Receita em `ferramentas/ffmpeg_ps4` |

Este repositório fica em `...\teste3\HyperNeiva-Telegram`.

O TDLib tem um ajuste nosso em `ferramentas/tdlib_patches` (já aplicado no `td_ps4` da release).

## 3. Skills do Claude Code

Copie `ferramentas/claude_skills/*` para `%USERPROFILE%\.claude\skills\`. Elas têm a receita de
build (`hyperneiva-ps4-build/scripts/build_pkg.ps1`) e o simulador do player de vídeo.

## 4. Compilar e empacotar

```powershell
$f='C:/Users/Marco/source/repos/teste3/ffmpeg_ps4/instalado/lib'
$e=@('--wrap=pthread_create','--wrap=pthread_join','--wrap=pthread_detach','--wrap=pthread_cancel',"$f/libavformat.a","$f/libavfilter.a","$f/libavcodec.a","$f/libswscale.a","$f/libswresample.a","$f/libavutil.a",'-LC:/Users/Marco/source/repos/teste3/lt_ps4/lib','-LC:/Users/Marco/source/repos/teste3/td_ps4/lib','-lhntorrent','-ltdjsonapi','-ltdcore','-ltdapi','-ltde2e','-ltddb','-ltdsqlite','-ltdnet','-ltdactor','-ltdutils','-lssl','-lcrypto','C:/OpenOrbis/lib/libz.a')
& "$env:USERPROFILE\.claude\skills\hyperneiva-ps4-build\scripts\build_pkg.ps1" -Versao 4.21 -NomePkg HyperNeiva_teste.pkg -Projeto <pasta deste repo> -LinkExtra $e -SemEnvio
```

Troque `C:/Users/Marco` pelo seu usuário. O PKG sai em `%TEMP%\opencode\pkg_<nome>\`.

- As bibliotecas do FFmpeg de `ffmpeg_ps4` vêm antes de tudo: elas substituem as do OpenOrbis
  (que não têm os codificadores do editor de vídeo).
- Os quatro `--wrap=pthread_*` são obrigatórios desde a 4.21: o app reaproveita as threads
  (`__wrap_pthread_create` e afins no começo de `video.cpp`). Sem eles o link falha por falta de
  `__real_pthread_create`.

- `build.bat` é só referência: não execute.
- `sce_sys/param.sfo` do repositório é o oficial (MARC00001). Para testar ao lado do oficial,
  use uma cópia com MARC00002 e não faça commit dela.
- Os `pkg_*.gp4` listam os arquivos do PKG; o script parte do de número maior.

## 5. Hyper Neiva de PC

```sh
sh ferramentas/hyperneiva_pc/compilar.sh
```
