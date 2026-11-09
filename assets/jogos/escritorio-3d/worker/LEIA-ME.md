# Colocar a rede na internet (grátis)

Isto publica o "porteiro" que deixa dois computadores se encontrarem. Você só
faz uma vez. Depois, é só colar o endereço no `config-rede.js`.

## Passo a passo

1. Crie uma conta grátis em <https://dash.cloudflare.com/sign-up> (só e-mail e senha).
2. Abra o PowerShell nesta pasta `worker` e rode:

   ```
   npx wrangler login
   ```

   Ele abre o navegador. Autorize.

3. Depois, publique:

   ```
   npx wrangler deploy
   ```

4. No fim, ele mostra um endereço parecido com:

   ```
   https://escritorio-rede.SEUNOME.workers.dev
   ```

5. Abra o arquivo `config-rede.js` (na pasta principal) e troque a linha `url`
   por esse endereço, com `wss://` na frente e `/ws` no fim:

   ```js
   window.ESC_REDE = {
     url: "wss://escritorio-rede.SEUNOME.workers.dev/ws"
   };
   ```

6. Pronto. Abra o escritório nos dois computadores (com este mesmo `config-rede.js`).
   Clique em **Amigos**, ponha seu apelido e veja quem está online.

## Testar sem internet

Precisa do Node.js instalado. O `abrir.ps1` liga o relay sozinho (porta 4748).
Se não ligar, rode `node servidor-rede.js` na pasta principal. Deixe
`config-rede.js` com `ws://127.0.0.1:4748` e abra duas janelas com endereços
diferentes (senão as duas usam a mesma pessoa):

- `http://127.0.0.1:4747/?eu=00000001&nome=Ana`
- `http://127.0.0.1:4747/?eu=00000002&nome=Bia`

## Custo

O plano grátis da Cloudflare aguenta folgado o uso de amigos (100 mil pedidos
por dia). Se um dia crescer muito, dá para mudar de plano.

## Segurança

- A senha do escritório nunca vai inteira para o servidor: vai um código
  (SHA-256) dela.
- O escritório só aceita quem tem a senha certa (ou é livre).
- O dono pode expulsar visitantes.
- A voz e a câmera vão direto de um computador para o outro (WebRTC). O
  servidor só ajuda a combinar a ligação, não vê nem guarda o áudio ou o vídeo.
- A câmera e o microfone só ligam quando você clica no botão. O navegador
  pede permissão na primeira vez.
- Para microfone e câmera funcionarem, use `localhost` ou um endereço HTTPS.
  O endereço da Cloudflare já é HTTPS.
