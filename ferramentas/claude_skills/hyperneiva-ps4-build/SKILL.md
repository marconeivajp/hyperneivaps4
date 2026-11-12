---
name: hyperneiva-ps4-build
description: Compila, empacota, valida e envia o app homebrew Hyper Neiva para o PS4 (OpenOrbis/Clang, PKG, FTP do GoldHEN). Use sempre que for corrigir bug, gerar eboot/PKG, subir versão, mandar PKG ao console, baixar logs do PS4 ou mexer em qualquer arquivo de C:\Users\Marco\source\repos\teste3\teste3, mesmo que o pedido só diga "arruma isso" ou "manda pro PS4".
---

# Build e envio do Hyper Neiva (PS4)

Projeto: `C:\Users\Marco\source\repos\teste3\teste3` (branch `master`, remote `origin`, marconeivajp/hyperneivaps4).

## Regras que protegem o projeto

- **`build.bat` só pode ser lido.** Executar apaga `*.o` e `*.pkg` e usa IP antigo. Não execute, edite nem renomeie. O script desta skill lê a receita dele e reproduz os comandos fora dele.
- **A lista de módulos vive no `build.bat`.** Como ele não pode mudar, código novo entra em arquivos que já existem. Um `.cpp` novo não seria linkado.
- **Backup com data em `%TEMP%\opencode\`** antes de editar. Preserve alterações Git já existentes.
- **Bug corrigido = compilar, empacotar e enviar sem perguntar.** Commit, push, tag e release só com pedido explícito.
- **Nunca monte o link com glob de `*.o` ou `*.a`.** Um PKG montado assim já foi rejeitado: a lista e a ordem das bibliotecas importam.
- **Relate cada etapa separada:** compilado, empacotado, validado, enviado, instalado, testado. Enviar não instala, e PKG válido não prova que o app abre. Só diga "corrigido" depois do teste no PS4.

## Fluxo normal

Use o script, que recompila o que mudou, linka, gera eboot e PKG, valida e envia:

```powershell
& "$env:USERPROFILE\.claude\skills\hyperneiva-ps4-build\scripts\build_pkg.ps1" -Versao 2.25 -NomePkg HyperNeiva_descricao_AAAAMMDD.pkg
```

- `-Modulos video,main`: força recompilar. **Use quando mudar um `.h`**, porque a detecção automática só compara `.cpp` com `.o`. Ao regerar `icones_embutidos.h`, inclua `menu_video`.
- `-SemEnvio`: para antes da Área de Trabalho e do FTP.
- O link leva cerca de 1 minuto, o PKG cerca de 45 s e o envio por Wi-Fi 3 a 4 minutos. Passe `timeout` alto na ferramenta.
- O SFO local volta sozinho para 1.00, mesmo se o empacotamento falhar.
- Versão atual e histórico: veja o maior `pkg_2_*.gp4` no projeto.

## Conferir o binário antes de enviar

Quando mexer em threads, link ou bibliotecas, compare com o `teste3.elf` do último build que abriu no console. O `llvm-readelf` não existe nesta instalação; use `C:\msys64\mingw64\bin\readelf.exe`.

- `-h`: tipo DYN, entry point `0x20`.
- `-d`: mesma lista de NEEDED (20 bibliotecas), sem caminho do Windows.
- `--dyn-syms -W`: importados novos devem ser funções reais do sistema.
- Tamanho saudável: eboot perto de 71,6 MB e PKG perto de 118 MB. Um PKG de cerca de 80 MB indica build mínimo errado.

## PS4 e FTP

- PS4: `192.168.1.101:2121`, usuário `anonymous`. O IP já mudou antes; se falhar, confirme com o usuário.
- PC: `192.168.1.102`. FTP do PC em `C:\Users\Marco\HyperNeivaFTP\HyperNeivaFtp.exe`, porta 21, passivo 50000-50100.
- PKGs ficam em `/data/pkg/`, com nome único por versão. Logs e dados em `/data/HyperNeiva/`.
- Sempre `curl.exe` (no PowerShell, `curl` é alias) com `--ftp-pasv`.

```powershell
curl.exe --ftp-pasv --connect-timeout 10 -s -u anonymous:anonymous -l "ftp://192.168.1.101:2121/data/pkg/"
curl.exe --ftp-pasv --connect-timeout 10 -s -u anonymous:anonymous -o log.txt "ftp://192.168.1.101:2121/data/HyperNeiva/video_debug_log.txt"
```

Logs do console valem mais que suposições: baixe antes de diagnosticar. Leitura pelo FTP é segura.

- Desde novembro de 2026 o PS4 está em `192.168.1.102` (confirme se falhar).
- O FTP do GoldHEN decifra SELF/PRX (`.prx`, `.sprx`, eboot) ao baixar e ao responder ao SIZE. O arquivo devolvido é um ELF maior e o SHA256 nunca bate. Para esses arquivos, confira os bytes enviados com `curl.exe -T arq -w '%{size_upload}' url`. Para PKG, o SHA256 baixado continua valendo.
- No `curl.exe`, `-B` é modo ASCII, não binário: nunca use no envio.
- Núcleos de emulador separados vão para `/data/HyperNeiva/nucleos/` (ex.: `mgba_libretro.prx`).

## Armadilhas conhecidas

- `create-fself.exe` exige caminhos absolutos em `-in`, `-out` e `-eboot`.
- `pkg_build` resolve caminhos a partir da pasta do GP4.
- Em GP4 feito à mão, espaços ou quebras de linha no fim da lista corrompem o último asset.
- `main.cpp` precisa manter a inicialização completa do PS4. Um stub de Windows no lugar dele causa CE-34878-0 logo depois da splash.
- `video_pplay.cpp` e os `pkg_*.gp4` não estão no Git, mas o primeiro faz parte do build: inclua no próximo commit pedido.
- O `orbis/`, `sys/` e `netinet/` locais são stubs do build Windows. O build do PS4 não os usa.
- Os headers do OpenOrbis têm muitos protótipos falsos (`void func();`) sem argumentos nem structs. O stub existir não quer dizer que a API esteja documentada.
- Variável global lida em outra thread precisa de leitura volátil. Com `-flto`, o otimizador pode congelar o valor dentro do laço.
- Botão usado por uma tela em `main.cpp`: use `cross_consumido`, `circle_consumido` ou limpe `pData->buttons`. Ele fica em `botoesConsumidos` até ser solto. Guardar o estado já limpo como "botão anterior" causava repetição a cada quadro e Bolinha voltando dois menus.
- Navegação: entrar em pasta de Mídia não muda `menuAtual` nem `pathExplorar`, então não entra na pilha. Mídia e Músicas sobem de pasta por conta própria, e a Bolinha só chama `voltarNavegacao()` na raiz.
- Antes de commitar ou publicar, rode `git status` e `git log` e veja as releases: o usuário também usa o `opencode`, que pode já ter feito o trabalho.
- FTP recortar só apaga itens com `concluido`, de trás para a frente. Mantenha essa ordem, porque é ela que preserva pastas com arquivo que falhou.

## Menu GitHub em Baixar (2.27 e 2.28)

- Código em `baixar_repositorio.cpp`, sem bibliotecas: leitor de JSON próprio porque `name` aparece em vários níveis. API sem token: 60 consultas por hora e 10 pesquisas por minuto.
- Favoritos em `/data/HyperNeiva/configuracao/github_favoritos.txt`. Cada linha aceita `dono/repo` ou link completo; `#` é comentário. Triângulo abre o menu com adicionar ou remover. Sem o arquivo, ele nasce com os repositórios sugeridos. `marconeivajp/hyperneivaps4` é fixo no topo, fora do txt.
- Download usa a URL `api.github.com/.../assets/ID` com `Accept: application/octet-stream`, e o nome real viaja no campo livre da fila. O redirecionamento automático falhou no console (URL de 907 letras): `abrirDownloadWeb` segue o salto à mão e só depois tenta o automático.
- Etapas de download ficam em `/data/HyperNeiva/download_debug_log.txt`. Baixe esse log antes de diagnosticar.
- O script de build grava `ATTRIBUTE=2` no SFO do PKG: é o que faz X confirmar no teclado do sistema. O `build.bat` gravaria 0.
- Releases e pesquisa ficam em memória, então a Bolinha volta sem usar a rede. `.pkg` vai para `/data/pkg`, o resto para `/data/HyperNeiva/baixado/github`.
- Assets do GitHub usam sempre o downloader interno, nunca BGFT, por causa do redirecionamento 302.
- `sceHttpSetAutoRedirect` tem protótipo falso no OpenOrbis: chamar sem argumentos passa lixo. Use `ligarRedirecionamento`. `seguirRedirecionamentos` é o plano B.
- Downloads com HTTP 300 ou mais não geram arquivo, e tamanho incompleto apaga o arquivo antes de instalar.
- Para testar a lógica contra a API real: `simulador/testar_github.py` da skill de vídeo e depois `teste_github.exe` (com um argumento, roda só os favoritos, sem rede).

## Personagem controlável e tempo do menu (2.30)

- A lógica fica em `elementos.cpp` (`atualizarPersonagem`, `quadroPersonagem`), com quadros em `personagem_quadros.h`, gerado por `tools/gerar_quadros_personagem.py`. Ao mudar o header, force `-Modulos elementos`.
- A folha desenha o personagem olhando para a esquerda. Use `personagemEspelhado()`, senão ele anda de costas.
- `ctrl1H` define o tamanho e `ctrl1Y` o chão. `ctrl1W` não muda o desenho.
- `cd`, `msgTimer` e `anim_timer` agora contam por tempo (passo de 50 ms). Não volte a contar por volta do loop: o fundo em cache acelerou o menu.
- `criar_pastas.cpp` só copia imagem que não existe. Para trocar um asset padrão, trate a cópia antiga do PS4, como o ponteiro faz pelo tamanho 267176.

## Compactados e menu de imagem (2.32)

- `extrairZip` tenta ZIP e, se falhar, olha o começo do arquivo: passa 7z e RAR adiante e avisa quando é página HTML. `cod 7` do miniz é "diretório central não encontrado".
- RAR 1.5 a 5 vem do libarchive em `hn_rar/`, incluído no fim de `kernelrw.c`, porque em `miniz.c` os tipos colidem com o SDK do 7z. Ao mudar `hn_rar/`, force `-Modulos kernelrw`.
- O script de build para em qualquer aviso do clang no stderr. O `hn_rar_impl.h` silencia os avisos com `#pragma clang diagnostic`.
- `abrirOpcoesImagem` (`explorar.cpp`) monta o menu do X em imagem e `.xavatar` para Explorar, FTP e Mídia. `.xavatar` não oferece plano de fundo, porque não é imagem.
- Modo do personagem fica em `configuracao/personagem_modo.txt`. Não acrescente campo em `LayoutConfig`: as leituras do `settings.bin` dependem de `sizeof`.

## Armadilhas de download e controle (2.33 e 2.34)

- O `Location` do Dropbox termina em `#`. `curl` e navegadores tiram isso sozinhos, o `sceHttp` não, e o servidor responde 400. Link que funciona com `curl` no PC não prova nada: compare o pedido cru.
- Todo menu com lista própria precisa de ramo em `voltarNavegacao`. Sem isso, `nomes` continua com a lista do menu seguinte e a Bolinha parece não funcionar.
- Analógico vai de 0 a 255, e 0 é fim do curso. Não use `> 0` para filtrar leitura vazia.

## Favoritos em pastas e 7z em fluxo (2.35)

- `github_favoritos.txt`: `[Pasta/Subpasta]` abre uma pasta e cada linha é `dono/repo` ou `dono/repo | Nome`. Sem nenhuma pasta, `favCarregar` aplica `favPadroes`. A lógica fica entre `// FAVORITOS-INICIO` e `// FAVORITOS-FIM` e roda no PC.
- `hn7z_ler` lê do disco e decodifica LZMA e LZMA2 em fluxo, com CRC por arquivo. Blocos com mais de um coder usam o caminho em memória do SDK. `malloc` de 351 MB falha no PS4 (`-1004`).
- `CrcGenerateTable` precisa rodar antes de abrir 7z. Sem ela, todo arquivo falha com código 3.
- Links dos XMLs podem vir com `&amp;`. `mostrarLinksJogo` decodifica.
- Link do Google Drive termina em `uc`: o nome real vem em `Content-Disposition`.

## Release no GitHub (só quando pedido)

`gh` não está instalado. Use a API com token vindo de `git credential fill` (`protocol=https`, `host=github.com`), sem nunca imprimir o token. Confirme tamanho e digest do asset enviado.

## Antes de compilar: a pasta certa e a versão certa (3.00)

- **Confira se a pasta está no HEAD.** Rode `git status --short` e `git log -1`. Em novembro de 2026 outro agente trabalhou numa cópia em `Desktop\HyperNeiva_v2.71_fonte` e commitou na branch `telegram`, porque o `.git` da cópia aponta para o mesmo worktree. A pasta `HyperNeiva-Telegram` ficou com arquivos antigos e o `git status` mostrou muitos `M` e `D`. Faça backup e `git checkout HEAD -- <arquivos>` antes de mexer, e nunca trabalhe em duas pastas ao mesmo tempo.
- **O GP4 nasce do `pkg_2_*.gp4` de número maior.** Uma pasta sem os GP4 novos monta o PKG com a lista de arquivos antiga. Copie os GP4 junto com a pasta.
- **`sce_sys/param.sfo` da cópia de teste é MARC00002 e nunca vai para o Git.** Se for commitado, o PKG oficial gerado do repositório instala como app de teste.
- **O IP do PS4 muda.** Já foi 1.101, 1.103 e 1.102. Se o FTP der tempo esgotado, pergunte o IP antes de concluir que o console está desligado.
- **Abertura lenta:** desde a 3.02, `/data/HyperNeiva/inicio_log.txt` grava o tempo de cada etapa da abertura e cada quadro que passou de meio segundo, com o menu. Baixe esse arquivo antes de chutar a causa.
- **Heredoc do Bash troca `\n` por quebra de linha real** em código C gerado por script. Para texto com barra invertida, use Write ou Edit.

## Núcleos de emulador em .prx (3.11 a 3.16)

- Ficam em `C:\Users\Marco\source\repos\teste3\nucleos_ps4\`, montados com `montar_nucleo.sh nome arquivo.a [extras]`, e vão para `/data/HyperNeiva/nucleos/<nome>_libretro.prx`. A ponte (`libretro_bridge.cpp`, `bridge_nucleo_para_rom`) escolhe pelo tipo de arquivo e pela pasta.
- Não use o `crtlib.o` do OpenOrbis: ele deixa TEXTREL (o PS4 recusa com `0x80020008`) e chama endereço 0 no `module_start`. Use `crt_nucleo.o`, `crt_alocador.o` e `crt_locale.o`, nessa ordem, antes do `.a`, com `--allow-multiple-definition`.
- `crt_alocador.c`: o núcleo usa o malloc do app, entregue pela ponte via `hn_definir_alocador`.
- `crt_locale.c`: a musl do OpenOrbis guarda o locale em `%fs:0-8` (TLS local-exec). Num módulo, esse endereço é do eboot. Sem a substituição, os núcleos em C++ (snes9x, desmume, stella) caíam no `retro_init`, e o DS chegou a desligar o PS4. Confira com `llvm-objdump -d` se `uselocale` e `mbrtowc` não usam `%fs`.
- O `create-fself.exe` não grava os arquivos se a saída for para `/dev/null`, e não aceita símbolo fraco sem definição (ex.: `ZSTD_trace_*`).
- O `.a` do gambatte, do mupen64plus, do pcsx_rearmed e do desmume (sem 2015) está incompleto: precisa ser compilado de novo a partir do código-fonte.
- O log de etapas (`ETAPA ...` em `zip_debug_log.txt`) e o `nucleo_log.txt` mostram onde o núcleo parou.
- **C++ para PS4 precisa de `-D__PS4__`.** O `__config` do libc++ do OpenOrbis só desliga o `__FreeBSD__` e liga as adaptações da musl e das threads do PS4 com essa definição. Sem ela, os cabeçalhos seguem o caminho do FreeBSD e o código não bate com o `libc++.a`. Os `.a` antigos de snes9x, stella e desmume caíam por isso. O `build.bat` do app também não passa `-D__PS4__`: até hoje não deu erro, mas é um risco no código C++ do app.
- Para recompilar um núcleo a partir do fonte (pedir autorização antes de baixar), use `nucleos_ps4/compilar_fonte.sh nome pasta "defines" "includes"`. Ele lê a lista de fontes de `lista_<nome>.txt` (tirada do `Makefile.common`), não roda o Makefile e usa `-std=gnu11` e `-std=gnu++14`, porque o modo estrito esconde funções POSIX da musl. Os fontes ficam em `C:\Users\Marco\source\repos\teste3\nucleos_fonte\`.
- **Construtores globais:** o PS4 não chama o `module_start` nem roda o `.init_array` de um `.prx` carregado pelo app. Por isso a ponte chama `hn_rodar_construtores` (via `hn_definir_alocador`). Sem isso, objetos globais do C++ (`std::map`, `std::set`) ficam zerados. Não teste isso com um `__attribute__((constructor))` que só grava uma variável: o clang resolve na compilação, e a marca sai falsa.
- **libretro-common precisa de `-DORBIS -D__ORBIS__`** em `rthreads`, `retro_eventcount`, `features_cpu`, `rtime` e `vfs_implementation`. Sem isso o `retro_eventcount` cai no caminho FreeBSD com semáforos da musl, que não funcionam no PS4: o núcleo trava sem crash (foi o caso do angrylion no parallel-n64). Confira com `llvm-objdump -T` que não sobrou `sem_wait`.
- **N64:** use o `parallel-n64` em software (angrylion, `HAVE_THR_AL`, interpretador, sem GL). A lista de fontes está em `lista_parallel_n64.txt`. O `libretro.c` precisa de `-DHAVE_OPENGLES2` só para os tipos do GL, e as 11 fontes de `Graphics/` usam `$(ROOT_DIR)`.
- **Cabeçalho incluído não dispara recompilação.** O `build_pkg.ps1` só recompila um módulo quando o `.c`/`.cpp` dele muda. Mudou um `.h` incluído (ex.: `hn_7z/hn_7z_impl.h`, incluído pelo `miniz.c`)? Passe `-Modulos miniz` (ou o módulo que inclui o cabeçalho). As 3.23 e 3.24 saíram sem a mudança por causa disso. Confira com `grep -c "texto novo" teste3.elf`.
- **JIT/dynarec no PS4 (GoldHEN):** memória no `.bss` do próprio módulo (perto do código, porque o código gerado usa deslocamentos de 32 bits) vira RWX com `sceKernelMprotect(addr, len, 7)`, em páginas de 16 KB. No parallel-n64: `-DDYNAREC -D_M_X64 -DNEW_DYNAREC=2 -DARCH_MIN_SSE2`, `new_dynarec.c`, `linkage_x64.asm` montado com `C:\msys64\usr\bin\nasm.exe -f elf64` (sem `LEADING_UNDERSCORE`) e `asm_defines_nasm.h` gerado das strings `@ASM_DEFINE` do `asm_defines.o`. Referência de como o EmuSuNi faz (GPL, só como ideia): github.com/arturofuentes1988/EmuSuNi.
- Não rode no PC executáveis compilados de código de terceiros: o bloqueio de permissões barra, e a regra do projeto proíbe.
