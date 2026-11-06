# Recuperar o build em um PC novo

Passo a passo para gerar o PKG do Hyper Neiva depois de formatar.

## 1. Instalar

- Git, Python 3, MSYS2 (`pacman -S gperf`)
- LLVM em `C:\Program Files\LLVM`, a mesma versão guardada em https://github.com/marconeivajp/LLVM

## 2. Baixar o SDK

Clone https://github.com/marconeivajp/openorbs em `C:\OpenOrbis` e extraia `OpenOrbis.part01.rar` nessa mesma pasta. O arquivo tem o SDK inteiro, incluindo `lib` e os arquivos gerados do TDLib.

## 3. Baixar o projeto

Clone https://github.com/marconeivajp/hyperneivaps4.

## 4. Bibliotecas do TDLib

Baixe `tdlib_ps4_lib.rar` na release mais recente e extraia em uma pasta, por exemplo `C:\td_ps4\lib`. Para recompilar do zero, siga `tdlib_ps4/LEIA-ME.md`.

## 5. Gerar o PKG

Não use o `build.bat`: ele não inclui o TDLib. Rode no PowerShell, de dentro da pasta do projeto:

```powershell
$e = @('-LC:/td_ps4/lib','-ltdjsonapi','-ltdcore','-ltdapi','-ltde2e','-ltddb','-ltdsqlite','-ltdnet','-ltdactor','-ltdutils','-lssl','-lcrypto','C:/OpenOrbis/lib/libz.a')
.\ferramentas\build_pkg.ps1 -Versao 2.52 -NomePkg HyperNeiva_v2.52.pkg -Projeto (Get-Location).Path -LinkExtra $e -SemEnvio
```

O PKG sai em `%TEMP%\opencode\pkg_<nome>\`. Sem `-SemEnvio`, o script envia ao PS4; use `-Ps4 IP:2121` se o IP mudou.

## 6. Chave do Telegram

Crie a chave em https://my.telegram.org e grave `/data/HyperNeivaTelegram/api.txt` no PS4: `api_id` na primeira linha e `api_hash` na segunda. Essa chave não fica no repositório.

## 7. Claude Code

Copie `ferramentas/skills/*` para `%USERPROFILE%\.claude\skills\`.
