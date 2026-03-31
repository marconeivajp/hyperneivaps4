@echo off
SETLOCAL EnableDelayedExpansion

echo ==========================================
echo        BUILD HYPER NEIVA (PS4)
echo ==========================================
echo.
echo [1/7] Limpando arquivos antigos...
del *.o 2>nul
del teste3.elf 2>nul
del teste3.oelf 2>nul
del eboot.bin 2>nul
del *.pkg 2>nul

echo.
echo [2/7] Compilando TODOS os modulos C++
"C:\Program Files\LLVM\bin\clang.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c kernelrw.c -o kernelrw.o
"C:\Program Files\LLVM\bin\clang.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c jailbreak.c -o jailbreak.o
"C:\Program Files\LLVM\bin\clang.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c miniz.c -o miniz.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c main.cpp -o main.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c explorar.cpp -o explorar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c editar.cpp -o editar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c network.cpp -o network.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c baixar.cpp -o baixar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c graphics.cpp -o graphics.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c jogar.cpp -o jogar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c audio.cpp -o audio.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle.cpp -o controle.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c menu.cpp -o menu.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c menu_audio.cpp -o menu_audio.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c menu_imagens.cpp -o menu_imagens.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c menu_video.cpp -o menu_video.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c menu_grafico.cpp -o menu_grafico.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_virtual.cpp -o controle_virtual.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c pesquisar.cpp -o pesquisar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c bloco_de_notas.cpp -o bloco_de_notas.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c video.cpp -o video.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c teclado.cpp -o teclado.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c criar_pastas.cpp -o criar_pastas.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_musicas.cpp -o controle_musicas.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_explorar.cpp -o controle_explorar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_editar.cpp -o controle_editar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_baixar.cpp -o controle_baixar.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_root.cpp -o controle_root.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c baixar_repositorio.cpp -o baixar_repositorio.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c baixar_dropbox_download.cpp -o baixar_dropbox_download.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c baixar_lojas.cpp -o baixar_lojas.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c dowload_sistema.cpp -o dowload_sistema.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c menu_upload.cpp -o menu_upload.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c elementos.cpp -o elementos.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c controle_elementos.cpp -o controle_elementos.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c elementos_sonoros.cpp -o elementos_sonoros.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c ftp.cpp -o ftp.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c elementos_animados_sprite_sheet.cpp -o elementos_animados_sprite_sheet.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c extra.cpp -o extra.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c informacao.cpp -o informacao.o
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c instrumentos.cpp -o instrumentos.o

echo.
echo [3/7] Linkando...
"C:\Program Files\LLVM\bin\ld.lld.exe" -m elf_x86_64 -pie --script "C:\OpenOrbis\link.x" --eh-frame-hdr -o teste3.elf "-LC:\OpenOrbis\lib" -lc -lm -lkernel -lc++ -lSceVideoOut -lSceAudioOut -lSceUserService -lSceSysmodule -lSceSysUtil -lScePad -lSceNet -lSceHttp -lSceSsl -lSceImeDialog -lSceCommonDialog -lSceBgft -lSceAppInstUtil "C:\OpenOrbis\lib\crt1.o" kernelrw.o jailbreak.o miniz.o main.o explorar.o editar.o network.o baixar.o graphics.o jogar.o audio.o controle.o menu.o menu_audio.o menu_imagens.o menu_video.o menu_grafico.o controle_virtual.o pesquisar.o bloco_de_notas.o video.o teclado.o criar_pastas.o controle_musicas.o controle_explorar.o controle_editar.o controle_baixar.o controle_root.o baixar_repositorio.o baixar_dropbox_download.o baixar_lojas.o dowload_sistema.o menu_upload.o elementos.o controle_elementos.o elementos_sonoros.o ftp.o elementos_animados_sprite_sheet.o extra.o informacao.o instrumentos.o

:: ==========================================
:: TRAVA DE SEGURANCA ADICIONADA AQUI!
:: ==========================================
if not exist teste3.elf (
    echo.
    echo [ERRO FATAL] A compilacao C++ falhou! Verifique os erros acima.
    echo O pacote PKG nao sera gerado nem enviado.
    pause
    exit /b 1
)

echo.
echo [4/7] Criando FSELF (Com flag de memoria do PS4)...
"C:\OpenOrbis\bin\windows\create-fself.exe" -in=teste3.elf -out=teste3.oelf --eboot=eboot.bin --paid 0x3800000000000011

echo.
echo [5/7] Gerando o SFO do Hyper Neiva...
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_new sce_sys/param.sfo
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value "01.00"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "gd"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value "UP0001-MARC00001_00-0000000000000000"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value "Hyper Neiva"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value "MARC00001"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value "01.00"

echo.
echo [6/7] Coletando Assets (Imagens e Fontes)...
set asset_images_files=
for %%f in (assets\images\*) do set asset_images_files=!asset_images_files! assets/images/%%~nxf

set asset_fonts_files=
for %%f in (assets\fonts\*) do set asset_fonts_files=!asset_fonts_files! assets/fonts/%%~nxf

set asset_audio_files=
for %%f in (assets\audio\*) do set asset_audio_files=!asset_audio_files! assets/audio/%%~nxf

echo.
echo [7/7] Criacao do GP4 e Build do PKG...
"C:\OpenOrbis\bin\windows\create-gp4.exe" -out pkg.gp4 --content-id=UP0001-MARC00001_00-0000000000000000 --files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_module/libc.prx sce_module/libSceFios2.prx assets/lista.xml assets/sp.xml assets/Sega_Master_System.xml assets/dropbox_token.txt !asset_images_files! !asset_fonts_files! !asset_audio_files!"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" pkg_build pkg.gp4 .

echo.
echo [Final] Renomeando e Copiando...
if exist "UP0001-MARC00001_00-0000000000000000.pkg" (
    ren "UP0001-MARC00001_00-0000000000000000.pkg" "Hyper Neiva.pkg"
)

if exist "E:\" (
    echo Copiando para o Pendrive E:\ ...
    copy /y "Hyper Neiva.pkg" "E:\Hyper Neiva.pkg"
)

echo.
echo Tentando enviar para o PS4 via FTP...
curl -T "Hyper Neiva.pkg" ftp://192.168.0.4:2121/data/pkg/ --connect-timeout 3
if %errorlevel% equ 0 (
    echo Envio via FTP concluido com sucesso!
    powershell -c "[console]::beep(400,300)"
) else (
    echo PS4 offline ou sem conexao FTP no momento.
    powershell -c "[console]::beep(300,150); Start-Sleep -Milliseconds 100; [console]::beep(250,400)"
)

cmd /c exit 0

echo.
echo ==========================================
echo         COMPILADO COM SUCESSO!
echo ==========================================
pause
exit /b 0