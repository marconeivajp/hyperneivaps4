@echo off
echo ==========================================
echo      FORJA DE PRX - TESTE NINTENDO DS 2015
echo ==========================================
echo.

mkdir sce_module 2>nul

echo [1/3] Compilando o Adaptador do PS4 (prx_stub.o)...
"C:\Program Files\LLVM\bin\clang++.exe" --target=x86_64-pc-freebsd12-elf -fPIC -O3 -I"C:\OpenOrbis\include" -c prx_stub.cpp -o prx_stub.o

echo.
echo [2/3] Linkando o DeSmuME 2015 com o Adaptador...
"C:\Program Files\LLVM\bin\ld.lld.exe" -m elf_x86_64 -shared -z max-page-size=0x4000 --script "C:\OpenOrbis\link.x" -o ds2015.so "C:\OpenOrbis\lib\crti.o" "C:\OpenOrbis\lib\crt_dyn.o" prx_stub.o --whole-archive desmume2015_libretro_ps4.a --no-whole-archive "C:\OpenOrbis\lib\crtn.o" "-LC:\OpenOrbis\lib" -lc -lm -lkernel -lc++

echo.
echo [3/3] Convertendo para PRX Oficial do PS4...
"C:\OpenOrbis\bin\windows\create-fself.exe" -in=ds2015.so -lib=sce_module/ds2015_core.prx -ptype=npdrm_dynlib

echo.
if exist sce_module\ds2015_core.prx (
    echo SUCESSO ABSOLUTO! O arquivo ds2015_core.prx foi gerado na pasta sce_module.
    del ds2015.so
    del prx_stub.o
) else (
    echo ERRO! O fantasma do -fPIC ou a versao do LLVM atacou novamente.
)

pause
