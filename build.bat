@echo off
SETLOCAL EnableDelayedExpansion

:: RESOLVE PROBLEMA DE DIRETORIO NO VS CODE
cd /d "%~dp0"

echo ==========================================
echo         BUILD HYPER NEIVA (PS4)
echo ==========================================
echo.
echo [1/7] Limpando arquivos antigos e processos...
taskkill /F /IM ld.lld.exe /T 2>nul
taskkill /F /IM create-fself.exe /T 2>nul
del *.o 2>nul
del teste3.elf 2>nul
del teste3.oelf 2>nul
del eboot.bin 2>nul
del *.pkg 2>nul
del fix_threads.cpp 2>nul

echo.
echo [1.5/7] Criando Vacina de Simbolos (fix_threads)...
echo extern "C" int __cxa_thread_atexit_impl(void (*func)(void*), void* arg, void* dso_handle) { return 0; } > fix_threads.cpp
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -flto -O3 -march=btver2 -c fix_threads.cpp -o fix_threads.o

echo.
echo [2/7] Compilando modulos C e C++...
set "c_files=kernelrw jailbreak miniz"
set "cpp_files=main explorar explorar_conversor explorar_utilitarios editar network baixar menu_emulador controle_emulador graphics jogar audio controle menu menu_audio menu_imagens menu_video menu_grafico menu_grafico_cache_grafico menu_grafico_render_texto menu_grafico_visualizadores menu_grafico_layout controle_virtual pesquisar bloco_de_notas video teclado criar_pastas controle_musicas controle_explorar controle_editar controle_baixar controle_root baixar_repositorio baixar_dropbox_download baixar_lojas dowload_sistema menu_upload elementos controle_elementos elementos_sonoros ftp elementos_animados_sprite_sheet extra informacao instrumentos pdf libretro_bridge radio audio_radio audio_musica audio_emulador video_pplay torrent_engine"

set total=0
for %%f in (%c_files%) do set /a total+=1
for %%f in (%cpp_files%) do set /a total+=1
set current=0

for %%f in (%c_files%) do (
    set /a current+=1
    set /a percent=current*100/total
    <nul set /p "=!percent!%% [Compilando %%f.c] "
    "C:\Program Files\LLVM\bin\clang.exe" --target=x86_64-pc-freebsd12-elf -fPIC -flto -O3 -march=btver2 -ffast-math -funwind-tables -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c %%f.c -o %%f.o
    if !errorlevel! neq 0 goto :erro
    echo [OK]
)

for %%f in (%cpp_files%) do (
    set /a current+=1
    set /a percent=current*100/total
    <nul set /p "=!percent!%% [Compilando %%f.cpp] "
    "C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -flto -O3 -march=btver2 -ffast-math -funwind-tables -DBOOST_ASIO_DISABLE_KQUEUE -D__stddef_max_align_t_defined -D__CLANG_MAX_ALIGN_T_DEFINED -Wno-macro-redefined -I"C:\Program Files\LLVM\bin\clang\include" -I"C:\OpenOrbis\include" -I"C:\OpenOrbis\include\c++\v1" -I"C:\OpenOrbis\include\orbis" -c %%f.cpp -o %%f.o
    if !errorlevel! neq 0 goto :erro
    echo [OK]
)

echo.
echo [3/7] Linkando... [AGUARDE LTO]
"C:\Program Files\LLVM\bin\ld.lld.exe" -m elf_x86_64 -pie --script "C:\OpenOrbis\link.x" --eh-frame-hdr --gc-sections --allow-multiple-definition -o teste3.elf "-LC:\OpenOrbis\lib" "C:\OpenOrbis\lib\crt1.o" kernelrw.o jailbreak.o miniz.o main.o menu_emulador.o controle_emulador.o explorar.o explorar_conversor.o explorar_utilitarios.o editar.o network.o baixar.o graphics.o jogar.o audio.o controle.o menu.o menu_audio.o menu_imagens.o menu_video.o menu_grafico.o menu_grafico_cache_grafico.o menu_grafico_render_texto.o menu_grafico_visualizadores.o menu_grafico_layout.o controle_virtual.o pesquisar.o bloco_de_notas.o video.o teclado.o criar_pastas.o controle_musicas.o controle_explorar.o controle_editar.o controle_baixar.o controle_root.o baixar_repositorio.o baixar_dropbox_download.o baixar_lojas.o dowload_sistema.o menu_upload.o elementos.o controle_elementos.o elementos_sonoros.o ftp.o elementos_animados_sprite_sheet.o extra.o informacao.o instrumentos.o pdf.o libretro_bridge.o radio.o audio_radio.o audio_musica.o audio_emulador.o video_pplay.o torrent_engine.o fix_threads.o mgba_libretro_ps4.a --start-group -ltorrent-rasterbar -lc -lm -lkernel -lc++ -lmupdf -lmupdf-third -lmpv -lass -lSDL2 -lsamplerate -lSceSystemService -lScePigletv2VSH -lSceFios2 -lavformat -lavfilter -lavcodec -lswscale -lswresample -lavutil -lSceVideoOut -lSceAudioOut -lSceAudioIn -lSceUserService -lSceSysmodule -lSceSysUtil -lScePad -lSceNet -lSceHttp -lSceSsl -lSceImeDialog -lSceCommonDialog -lSceBgft -lSceAppInstUtil -ljpeg -lpng16 -lbz2 -lopus -lfreetype -lfribidi -lSceZlib -lScePosix --end-group

:: ==========================================
:: TRAVA DE SEGURANCA COM 2 BIPES DE ERRO
:: ==========================================
if not exist teste3.elf (
    goto :erro
)

echo.
echo [3.5/7] Otimizando ELF (Strip Seguro)...
"C:\Program Files\LLVM\bin\llvm-strip.exe" --strip-debug teste3.elf

echo.
echo [4/7] Criando FSELF (Com flag de memoria do PS4)...
"C:\OpenOrbis\bin\windows\create-fself.exe" -in=teste3.elf -out=teste3.oelf --eboot=eboot.bin --paid 0x3800000000000011

if not exist eboot.bin (
    echo.
    echo [ERRO FATAL] O Passo 4 Falhou. O eboot.bin nao foi gerado!
    powershell -NoProfile -Command "[System.Console]::Beep(1500,400); Start-Sleep -Milliseconds 150; [System.Console]::Beep(1500,400)"
    exit /b 1
)

echo.
echo [5/7] Gerando o SFO do Hyper Neiva...
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_new sce_sys/param.sfo
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value "1.00"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "gd"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo MEMSIZE --type Integer --maxsize 4 --value 512
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value "UP0001-MARC00001_00-0000000000000000"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value "Hyper Neiva"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value "MARC00001"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value "1.00"

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
"C:\OpenOrbis\bin\windows\create-gp4.exe" -out pkg.gp4 --content-id=UP0001-MARC00001_00-0000000000000000 --files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic1.png sce_module/libc.prx sce_module/libSceFios2.prx assets/system.xml assets/sp.xml assets/Sega_Master_System.xml assets/systemas+zipados.xml assets/xavatar.xml assets/xml.xml assets/retrocast_brasil.xml assets/dropbox_token.txt !asset_images_files! !asset_fonts_files! !asset_audio_files!"
"C:\OpenOrbis\bin\windows\PkgTool.Core.exe" pkg_build pkg.gp4 .

echo.
echo [Final] Renomeando e Copiando...
powershell -NoProfile -Command "Start-Sleep -Seconds 3"

if exist "UP0001-MARC00001_00-0000000000000000.pkg" (
    ren "UP0001-MARC00001_00-0000000000000000.pkg" "Hyper Neiva.pkg"
)

if exist "E:\" (
    echo Garantindo limpeza do Pendrive E:\ ...
    if exist "E:\Hyper Neiva.pkg" del /f /q "E:\Hyper Neiva.pkg"
    echo Copiando para o Pendrive E:\ ...
    copy /y "Hyper Neiva.pkg" "E:\Hyper Neiva.pkg"
)

echo.
echo Tentando enviar para o PS4 via FTP (Timeout 10s)...
curl -T "Hyper Neiva.pkg" ftp://192.168.0.4:2121/data/pkg/ --connect-timeout 10
if %errorlevel% equ 0 (
    echo Envio via FTP concluido com sucesso!
    :: 1 BIPE PARA SUCESSO
    powershell -NoProfile -Command "[System.Console]::Beep(1200,500)"
) else (
    echo PS4 offline ou sem conexao FTP no momento.
    :: 2 BIPES PARA ERRO
    powershell -NoProfile -Command "[System.Console]::Beep(1500,400); Start-Sleep -Milliseconds 150; [System.Console]::Beep(1500,400)"
)

echo.
echo ==========================================
echo          COMPILADO COM SUCESSO!
echo ==========================================
exit /b 0

:erro
echo.
echo [ERRO FATAL] A compilacao ou Linker falhou!
echo O pacote PKG nao sera gerado nem enviado.
powershell -NoProfile -Command "[System.Console]::Beep(1500,400); Start-Sleep -Milliseconds 150; [System.Console]::Beep(1500,400)"
exit /b 1