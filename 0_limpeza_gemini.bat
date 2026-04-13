@echo off
setlocal EnableDelayedExpansion

:: Define que o script vai operar a partir da pasta onde ele mesmo esta salvo
cd /d "%~dp0"

echo.
echo [1/3] Limpando arquivos antigos locais...
del *.o 2>nul
del teste3.elf 2>nul
del teste3.oelf 2>nul
del eboot.bin 2>nul
del *.pkg 2>nul

:: Define o caminho de destino
set "DEST_DIR=C:\Users\Marco\source\repos\0 recortado"

echo.
echo [2/3] Limpando a pasta de destino...
if exist "%DEST_DIR%" (
    rmdir /s /q "%DEST_DIR%"
)
mkdir "%DEST_DIR%"

echo.
echo [3/3] Copiando e dividindo arquivos (Max 100MB por pasta)...
:: 100 MB em bytes = 104857600
set "MAX_BYTES=104857600"
set "CURRENT_BYTES=0"
set "FOLDER_INDEX=1"
set "CURRENT_DEST=%DEST_DIR%\Parte_!FOLDER_INDEX!"

mkdir "!CURRENT_DEST!"

:: O loop 'for' com (*) pega APENAS arquivos soltos na raiz onde o .bat esta, ignorando pastas
for %%F in (*) do (
    :: Ignora o proprio arquivo .bat para ele nao se copiar
    if "%%~nxF" NEQ "%~nx0" (
        set "FILE_SIZE=%%~zF"
        set /a "TEST_SIZE=CURRENT_BYTES + FILE_SIZE"
        
        :: Verifica se adicionar este arquivo vai ultrapassar o limite de 100MB
        if !TEST_SIZE! GTR !MAX_BYTES! (
            :: So cria nova pasta se a atual nao estiver vazia (evita travar se um unico arquivo tiver mais de 100MB)
            if !CURRENT_BYTES! GTR 0 (
                set /a FOLDER_INDEX+=1
                set "CURRENT_DEST=%DEST_DIR%\Parte_!FOLDER_INDEX!"
                mkdir "!CURRENT_DEST!"
                set "CURRENT_BYTES=0"
            )
        )
        
        echo Copiando: %%~nxF -^> Parte_!FOLDER_INDEX!
        copy "%%F" "!CURRENT_DEST!\" >nul
        set /a "CURRENT_BYTES+=FILE_SIZE"
    )
)

echo.
echo Operacao concluida com sucesso! Todos os arquivos foram organizados.
pause