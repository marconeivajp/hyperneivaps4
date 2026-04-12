$path = "C:\Users\Marco\source\repos\teste3\teste3\controle_explorar.cpp"
$lines = [System.IO.File]::ReadAllLines($path)
$newLines = New-Object System.Collections.Generic.List[string]

$externAdded = $false
$logicAdded = $false

foreach ($line in $lines) {
    # 1. Add Externs
    if (-not $externAdded -and $line -like "*extern void acaoArquivo*") {
        $newLines.Add($line)
        if ($content -notlike "*extern void iniciarEmulador*") {
            $newLines.Add("extern void iniciarEmulador(const char* romPath);")
            $newLines.Add("extern char msgStatus[128];")
            $newLines.Add("extern int msgTimer;")
            $externAdded = $true
        }
    }
    # 2. Add Logic
    elseif (-not $logicAdded -and $line -like "*else if (strstr(nomeBlindado, `".mp4`")) {*") {
        $newLines.Add($line)
        $newLines.Add("                iniciarVideoMP4(caminhoArquivo);")
        $newLines.Add("            }")
        $newLines.Add("            else if (strstr(nomeBlindado, `".bin`") || strstr(nomeBlindado, `".md`") || strstr(nomeBlindado, `".smd`") || strstr(nomeBlindado, `".gen`") || strstr(nomeBlindado, `".zip`") || strstr(nomeBlindado, `".rar`") || strstr(nomeBlindado, `".7z`")) {")
        $newLines.Add("                snprintf(msgStatus, sizeof(msgStatus), `"ROM Detectada!`"); msgTimer = 120;")
        $newLines.Add("                iniciarEmulador(caminhoArquivo);")
        $logicAdded = $true
        # We skip the original "iniciarVideoMP4" and "}" because we already added them
        continue
    }
    else {
        $newLines.Add($line)
    }
}

# Second pass if we skipped the "iniciarVideoMP4" block to avoid duplication
# Actually, the logic above is a bit complex. Let's simplify.

$raw = [System.IO.File]::ReadAllText($path)

$find1 = "extern void acaoArquivo(int idxOpcao);"
$repl1 = "extern void acaoArquivo(int idxOpcao);`r`nextern void iniciarEmulador(const char* romPath);`r`nextern char msgStatus[128];`r`nextern int msgTimer;"

$find2 = 'else if (strstr(nomeBlindado, ".mp4")) {
                iniciarVideoMP4(caminhoArquivo);
            }'

$repl2 = 'else if (strstr(nomeBlindado, ".mp4")) {
                iniciarVideoMP4(caminhoArquivo);
            }
            else if (strstr(nomeBlindado, ".bin") || strstr(nomeBlindado, ".md") || strstr(nomeBlindado, ".smd") || strstr(nomeBlindado, ".gen") || strstr(nomeBlindado, ".zip") || strstr(nomeBlindado, ".rar") || strstr(nomeBlindado, ".7z")) {
                snprintf(msgStatus, sizeof(msgStatus), "ROM Detectada!"); msgTimer = 120;
                iniciarEmulador(caminhoArquivo);
            }'

if ($raw -contains $find1 -and -not ($raw -contains "extern void iniciarEmulador")) {
    $raw = $raw.Replace($find1, $repl1)
}

if ($raw -contains $find2 -and -not ($raw -contains "ROM Detectada")) {
    $raw = $raw.Replace($find2, $repl2)
}

[System.IO.File]::WriteAllText($path, $raw)
