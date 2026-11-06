# TDLib no PS4

O menu Extra > Telegram usa o TDLib compilado para o alvo do OpenOrbis.

## Bibliotecas

Em uma pasta fora do repositório, clone `tdlib/td` em `src` no commit `8ff05a0e7e` e copie estes scripts para ela.

```powershell
python preparar.py
python ajustar_ps4.py
python compilar.py -j 2 todas
```

A compilação leva cerca de 1h30 e gera as bibliotecas em `lib`.

## Link

O `build.bat` não inclui o TDLib. O link precisa destas bibliotecas antes de `-lc`, dentro do grupo da receita:

```text
-L<pasta>/lib -ltdjsonapi -ltdcore -ltdapi -ltde2e -ltddb -ltdsqlite -ltdnet -ltdactor -ltdutils -lssl -lcrypto C:/OpenOrbis/lib/libz.a
```

O zlib precisa do caminho do `.a`, porque `-lz` pega um `.so` que quebra o FSELF.

## Chave da API

Cada usuário cria a própria chave em https://my.telegram.org e grava `/data/HyperNeivaTelegram/api.txt`: `api_id` na primeira linha e `api_hash` na segunda. Essa chave nunca entra no repositório nem no PKG.

## Emojis

`assets/emoji.pack` reúne as imagens 72x72 do Twemoji v17.0.3 (https://github.com/jdecked/twemoji), licença CC-BY 4.0. O app mostra o crédito na lista de conversas.
