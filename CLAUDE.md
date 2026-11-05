@AGENTS.md

# Hyper Neiva PS4

App homebrew para PS4 destravado (GoldHEN), feito com OpenOrbis/Clang. Responda em português do Brasil.

## Skills

- `hyperneiva-ps4-build`: compilar, empacotar, validar, enviar e baixar logs.
- `hyperneiva-video-engine`: player de vídeo, leitura do log e simulador no PC.

## Regras

- `build.bat` só pode ser lido: nunca execute, edite ou regenere. Reproduza a receita fora dele.
- Código novo entra em arquivos existentes, porque a lista de módulos está no `build.bat`.
- Antes de editar, faça backup com data em `%TEMP%\opencode\` e preserve alterações Git existentes.
- Ao corrigir bug: compile, empacote e envie o PKG ao PS4 sem perguntar.
- Commit, push, tag e release só com pedido explícito.
- Dê nome único a cada PKG, preserve o rollback e confira tamanho remoto e SHA256 depois do envio.
- Informe separadamente: compilado, empacotado, validado, enviado, instalado e testado. Só diga "corrigido" depois do teste no console.
- Use `curl.exe`, nunca o alias `curl`. Não exponha tokens nem senhas.

## Antes de mexer

- Baixe o log do PS4 por FTP e meça antes de diagnosticar: `/data/HyperNeiva/`.
- Teste no PC tudo que der. Cada teste no console custa tempo do usuário.

## Arquivos mais sensíveis

- `video.cpp`: motor de vídeo multi-thread.
- `graphics.cpp`: tela, flips e camadas de interface.
- `main.cpp`: loop principal e inicialização do PS4.
- `ftp.cpp` e `controle_baixar.cpp`: FTP e explorador remoto.
- `menu_grafico.cpp` e `baixar.cpp`: barra de download.
