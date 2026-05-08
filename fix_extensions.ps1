$path = "C:\Users\Marco\source\repos\teste3\teste3\controle_explorar.cpp"
$content = [System.IO.File]::ReadAllText($path)

# Ensure externs for diagnostic messages
$searchExtern = "extern void iniciarEmulador(const char* romPath);"
if ($content -contains $searchExtern -and -not ($content -contains "extern char msgStatus")) {
    $replaceExtern = "extern void iniciarEmulador(const char* romPath);`r`nextern char msgStatus[128];`r`nextern int msgTimer;"
    $content = $content.Replace($searchExtern, $replaceExtern)
}

# Add logging before launching emulator
$searchExec = "iniciarEmulador(caminhoArquivo);"
if ($content -contains $searchExec -and -not ($content -contains "ROM Detectada")) {
    $replaceExec = "snprintf(msgStatus, sizeof(msgStatus), `"ROM Detectada!`"); msgTimer = 120;`r`n                iniciarEmulador(caminhoArquivo);"
    $content = $content.Replace($searchExec, $replaceExec)
}

[System.IO.File]::WriteAllText($path, $content)
