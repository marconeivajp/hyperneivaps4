# Escritório 3D para o Claude Code

Um escritório em 3D, no navegador, onde você vê seus agentes do Claude Code trabalhando ao vivo. Cada chat aberto no Claude Code vira uma tarefa, e o agente certo vai para a mesa dele.

Tem também:

- **Chat com o supervisor**, direto da tela
- **Conversa com cada agente**, clicando nele
- **Brainstorm por voz** com vários agentes falando
- **Lista de tarefas** ao vivo
- **Música ambiente**, ou um link do YouTube
- **Visitar o escritório de amigos**, até 4 pessoas juntas

Tudo roda no seu computador. Para ver amigos, só o encontro (quem está online) passa pelo servidor grátis da Cloudflare.

## Precisa ter

- Windows 10 ou 11
- O app do Claude com o Claude Code (aba Code), já usado pelo menos uma vez
- Chrome ou Edge

## Como instalar

1. Baixe este repositório (botão verde **Code** e depois **Download ZIP**) e extraia.
2. (Opcional) Ponha seu logo na pasta com os nomes `logo-branco.png` (logo inteiro, branco) e `logo-h-branco.png` (logo deitado, branco). Sem logo, aparece o texto "ESCRITÓRIO 3D".
3. Dê dois cliques em `INSTALAR.bat`.
4. Abra `Documentos\Meu Claude Code\CLAUDE.md` e troque o que está entre [colchetes].
5. No app do Claude, aba Code, escolha a pasta **Meu Claude Code**. Cada chat que você abrir ali aparece no escritório.

O escritório abre sozinho quando o Windows liga. Para abrir na mão, use o atalho **Escritório 3D** da área de trabalho, ou acesse `http://127.0.0.1:4747`.

## Ver amigos no escritório

Até 4 pessoas podem ficar no mesmo escritório. Se o dono puser senha, só entra quem souber.

- **No mesmo PC (para testar):** rode `node servidor-rede.js` na pasta do projeto. O `config-rede.js` já aponta para `ws://127.0.0.1:4748`. Abra duas janelas do navegador com endereços diferentes:
  - `http://127.0.0.1:4747/?eu=00000001&nome=Ana`
  - `http://127.0.0.1:4747/?eu=00000002&nome=Bia`
- **Na internet:** siga o `worker/LEIA-ME.md` (é grátis) e troque a `url` do `config-rede.js` pelo endereço que a Cloudflare mostrar.

Clique em **Amigos**, escolha seu apelido e veja quem está online. Clique na pessoa para visitar o escritório dela. O dono pode expulsar visitantes.

No painel **Amigos** também tem **Microfone** e **Câmera**. Quem está no mesmo escritório escuta a sua voz, mais alto quando está perto de você. A câmera aparece acima da cabeça do seu boneco. O navegador vai pedir permissão na primeira vez.

No botão **Meu** você escolhe a cor da roupa, do cabelo e da pele do seu boneco, liga enfeites (planta, aquário, ventilador, globo e luminária) e troca o terreno (jardim, praia, neve ou campo). Quem visita o seu escritório vê tudo igual. Suas escolhas ficam salvas neste navegador.

Voz e câmera precisam de conexão direta entre os computadores. Em redes muito fechadas (empresa, por exemplo) pode não funcionar. Não há servidor de vídeo no meio.

## O que o instalador faz

- Copia os arquivos para `%LOCALAPPDATA%\Escritorio3D`
- Liga os avisos (hooks) no `~/.claude/settings.json`, guardando uma cópia do arquivo antigo
- Cria a pasta de trabalho `Documentos\Meu Claude Code` com as regras
- Cria o atalho na área de trabalho e liga o servidor junto com o Windows

Para tirar tudo: `DESINSTALAR.bat`.

## Trocar os agentes

Os agentes ficam na lista `AG` dentro do `index.html` (nome, sala, cor da roupa, cabelo e pele). O `id` de cada um tem que ser igual ao nome da skill do Claude Code que ele representa. Quando o Claude chama a skill, o boneco certo vai trabalhar.

As salas ficam na lista `ROOMS`, logo acima. O jeito de cada agente no brainstorm fica em `ideias.txt`.

Dica: peça ao próprio Claude Code. Ex.: "troca os agentes do escritório pelos meus: ...".

## Arquivos

| Arquivo | Para que serve |
|---|---|
| `index.html` | O escritório 3D (three.js) |
| `servidor.ps1` | Servidor local na porta 4747, recebe os avisos e conversa com o Claude Code |
| `comum.ps1`, `inicio.ps1`, `aviso.ps1`, `abrir.ps1` | Ligam o servidor e repassam os avisos |
| `rede.js`, `config-rede.js` | Amigos: presença, visita, posições, voz, câmera, perfil, enfeites e terreno |
| `servidor-rede.js` | Relay de teste na porta 4748 (Node) |
| `worker/` | Relay da internet na Cloudflare (veja `worker/LEIA-ME.md`) |
| `vendor/` | three.js copiado para rodar sem internet |
| `ideias.txt` | Instruções do brainstorm |
| `CLAUDE.md` | Modelo de regras para a pasta de trabalho |
| `instalar.ps1`, `desinstalar.ps1` | Instalam e removem |
