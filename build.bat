@echo off
SETLOCAL EnableDelayedExpansion

:: Limpeza total de arquivos temporarios
if exist main.o del main.o
if exist explorar.o del explorar.o
if exist editar.o del editar.o
if exist network.o del network.o
if exist baixar.o del baixar.o
if exist graphics.o del graphics.o
if exist jogar.o del jogar.o
if exist audio.o del audio.o
if exist controle.o del controle.o
if exist menu.o del menu.o
if exist menu_audio.o del menu_audio.o
if exist menu_imagens.o del menu_imagens.o
if exist menu_video.o del menu_video.o
if exist menu_grafico.o del menu_grafico.o
if exist controle_virtual.o del controle_virtual.o
if exist pesquisar.o del pesquisar.o
if exist bloco_de_notas.o del bloco_de_notas.o
if exist video.o del video.o
if exist teclado.o del teclado.o
if exist criar_pastas.o del criar_pastas.o
if exist teste3.elf del teste3.elf
if exist teste3.oelf del teste3.oelf
if exist eboot.bin del eboot.bin

:: 1. COMPILACAO DOS MODULOS
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

:: 2. LINKAGEM
"C:\Program Files\LLVM\bin\ld.lld.exe" -m elf_x86_64 -pie --script "C:\OpenOrbis\link.x" --eh-frame-hdr -o teste3.elf "-LC:\OpenOrbis\lib" -lc -lm -lkernel -lc++ -lSceVideoOut -lSceAudioOut -lSceUserService -lSceSysmodule -lSceSysUtil -lScePad -lSceNet -lSceHttp -lSceSsl -lSceImeDialog -lSceCommonDialog "C:\OpenOrbis\lib\crt1.o" main.o explorar.o editar.o network.o baixar.o graphics.o jogar.o audio.o controle.o menu.o menu_audio.o menu_imagens.o menu_video.o menu_grafico.o controle_virtual.o pesquisar.o bloco_de_notas.o video.o teclado.o criar_pastas.o

:: 3. CRIACAO DO BINARIO PS4
"C:\OpenOrbis\bin\windows\create-fself.exe" -in=teste3.elf -out=teste3.oelf --eboot=eboot.bin --paid 0x3800000000000011

:: 4. GERACAO DO SFO
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_new sce_sys/param.sfo
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value "01.00"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "gd"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value "UP0001-TEST00021_00-0000000000000000"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value "Hyper Neiva"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value "TEST00021"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value "01.00"

:: 5. COLETANDO ASSETS
set asset_images_files=
for %%f in (assets\images\*) do set asset_images_files=!asset_images_files! assets/images/%%~nxf

set asset_fonts_files=
for %%f in (assets\fonts\*) do set asset_fonts_files=!asset_fonts_files! assets/fonts/%%~nxf

:: 6. CRIACAO DO GP4 E BUILD DO PKG
"C:\OpenOrbis\bin\windows\create-gp4.exe" -out pkg.gp4 --content-id=UP0001-TEST00021_00-0000000000000000 --files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_module/libc.prx sce_module/libSceFios2.prx assets/lista.xml assets/sp.xml assets/Sega_Master_System.xml !asset_images_files! !asset_fonts_files!"

"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" pkg_build pkg.gp4 .

:: --- COPIA PARA O PENDRIVE E: COM O NOME DESEJADO ---
if exist E:\ (
    echo Copiando para o pendrive E: como Hyper Neiva.pkg...
    copy /y "UP0001-TEST00021_00-0000000000000000.pkg" "E:\Hyper Neiva.pkg"
)

echo COMPILADO COM SUCESSO!
pause