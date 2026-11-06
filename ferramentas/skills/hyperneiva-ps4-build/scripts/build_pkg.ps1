# Compila, linka, empacota, valida e envia o Hyper Neiva ao PS4.
# A receita (flags, modulos, ordem do link) e lida do build.bat, que NUNCA e executado.
param(
    [Parameter(Mandatory = $true)][string]$Versao,
    [Parameter(Mandatory = $true)][string]$NomePkg,
    [string[]]$Modulos = @(),
    [string[]]$LinkExtra = @(),
    [switch]$SemEnvio,
    [string]$Projeto = 'C:\Users\Marco\source\repos\teste3\teste3',
    [string]$Ps4 = '192.168.1.101:2121'
)

$ErrorActionPreference = 'Stop'
$PkgTool = 'C:\OpenOrbis\bin\windows\PkgTool.Core.exe'
$Fself = 'C:\OpenOrbis\bin\windows\create-fself.exe'
$Strip = 'C:\Program Files\LLVM\bin\llvm-strip.exe'
$Modulos = @($Modulos | ForEach-Object { $_ -split ',' } | Where-Object { $_ })

function Tokens([string]$Linha) {
    [regex]::Matches($Linha, '(?:[^\s"]+|"[^"]*")+') | ForEach-Object { $_.Value.Replace('"', '') }
}

function Exigir([string]$Etapa) {
    if ($LASTEXITCODE -ne 0) { throw "$Etapa falhou (codigo $LASTEXITCODE)" }
}

# ---- 1. Receita ----
$bat = Get-Content "$Projeto\build.bat"
$cFiles = (($bat | Where-Object { $_ -match '^set "c_files=' }) -replace '^set "c_files=(.*)"$', '$1') -split ' '
$cppFiles = (($bat | Where-Object { $_ -match '^set "cpp_files=' }) -replace '^set "cpp_files=(.*)"$', '$1') -split ' '
$linhaC = ($bat | Where-Object { $_ -match 'clang\.exe" ' -and $_ -match '%%f\.c ' } | Select-Object -First 1).Trim()
$linhaCpp = ($bat | Where-Object { $_ -match 'clang\+\+\.exe" ' -and $_ -match '%%f\.cpp ' } | Select-Object -First 1).Trim()
$linhaLink = ($bat | Where-Object { $_ -match 'ld\.lld\.exe" ' } | Select-Object -First 1).Trim()
if (-not $linhaC -or -not $linhaCpp -or -not $linhaLink -or $cppFiles.Count -lt 10) { throw 'Nao consegui ler a receita do build.bat' }

# ---- 2. Backup e compilacao do que mudou ----
Set-Location $Projeto
$carimbo = Get-Date -Format 'yyyyMMdd_HHmmss'
$backup = "$env:TEMP\opencode\backup_build_$carimbo"
New-Item -ItemType Directory -Force $backup | Out-Null
foreach ($f in 'teste3.elf', 'eboot.bin') { if (Test-Path $f) { Copy-Item $f $backup -Force } }
Write-Output "Backup: $backup"

$fontes = @($cFiles | ForEach-Object { [pscustomobject]@{ Nome = $_; Arquivo = "$_.c"; Linha = $linhaC } })
$fontes += @($cppFiles | ForEach-Object { [pscustomobject]@{ Nome = $_; Arquivo = "$_.cpp"; Linha = $linhaCpp } })
foreach ($m in $fontes) {
    $obj = "$($m.Nome).o"
    $precisa = ($Modulos -contains $m.Nome) -or -not (Test-Path $obj) -or ((Get-Item $m.Arquivo).LastWriteTime -gt (Get-Item $obj).LastWriteTime)
    if (-not $precisa) { continue }
    $t = @(Tokens ($m.Linha -replace '%%f', $m.Nome))
    & $t[0] $t[1..($t.Count - 1)]
    Exigir "Compilacao de $($m.Arquivo)"
    Write-Output "Compilado: $($m.Arquivo)"
}

# ---- 3. Link, strip e FSELF ----
$t = @(Tokens $linhaLink)
# Bibliotecas extras entram antes de -lc, para o libc++ resolver o que elas pedem.
if ($LinkExtra.Count -gt 0) {
    $posicao = [array]::IndexOf($t, '-lc')
    $t = @($t[0..($posicao - 1)]) + $LinkExtra + @($t[$posicao..($t.Count - 1)])
}
& $t[0] $t[1..($t.Count - 1)]
Exigir 'Link'
& $Strip --strip-debug "$Projeto\teste3.elf"
Exigir 'Strip'
$argsFself = @("-in=$Projeto\teste3.elf", "-out=$Projeto\teste3.oelf", "-eboot=$Projeto\eboot.bin", '-paid=0x3800000000000011')
& $Fself @argsFself
Exigir 'FSELF'
Write-Output "eboot.bin: $((Get-Item eboot.bin).Length) bytes"

# ---- 4. GP4 e PKG, com o SFO local sempre restaurado ----
$gp4Nome = 'pkg_' + $Versao.Replace('.', '_') + '.gp4'
if (-not (Test-Path $gp4Nome)) {
    $base = Get-ChildItem -Filter 'pkg_2_*.gp4' | Sort-Object { [int]($_.BaseName -replace '^pkg_2_', '') } | Select-Object -Last 1
    $texto = [IO.File]::ReadAllText($base.FullName)
    $texto = [regex]::Replace($texto, '<volume_ts>[^<]*</volume_ts>', "<volume_ts>$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')</volume_ts>")
    [IO.File]::WriteAllText("$Projeto\$gp4Nome", $texto, (New-Object Text.UTF8Encoding($false)))
    Write-Output "GP4 criado a partir de $($base.Name)"
}

$saida = "$env:TEMP\opencode\pkg_" + [IO.Path]::GetFileNameWithoutExtension($NomePkg)
New-Item -ItemType Directory -Force $saida | Out-Null
$sfo = "$Projeto\sce_sys\param.sfo"
Copy-Item $sfo "$saida\param.sfo.original" -Force
try {
    & $PkgTool sfo_setentry $sfo APP_VER --type Utf8 --maxsize 8 --value $Versao | Out-Null
    Exigir 'SFO APP_VER'
    & $PkgTool sfo_setentry $sfo VERSION --type Utf8 --maxsize 8 --value $Versao | Out-Null
    Exigir 'SFO VERSION'
    # Bit 0x2: nos dialogos do sistema, como o teclado, X confirma e Bolinha cancela.
    & $PkgTool sfo_setentry $sfo ATTRIBUTE --type Integer --maxsize 4 --value 2 | Out-Null
    Exigir 'SFO ATTRIBUTE'
    & $PkgTool pkg_build ".\$gp4Nome" $saida | Select-Object -Last 2
    Exigir 'pkg_build'
}
finally {
    Copy-Item "$saida\param.sfo.original" $sfo -Force
    Write-Output 'SFO local restaurado'
}

$gerado = Get-ChildItem $saida -Filter '*.pkg' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $gerado) { throw 'PKG nao foi gerado' }
$pkg = "$saida\$NomePkg"
if ($gerado.FullName -ne $pkg) { Move-Item $gerado.FullName $pkg -Force }

# ---- 5. Validacao ----
$validacao = & $PkgTool pkg_validate $pkg 2>&1
Exigir 'pkg_validate'
$ruins = @($validacao | Where-Object { "$_" -match '^\[' -and "$_" -notmatch '^\[OK\]' })
if ($ruins.Count -gt 0) { $ruins; throw 'PKG com itens invalidos' }
& $PkgTool pkg_extractentry $pkg 9 "$saida\param_sfo_interno.bin" | Out-Null
$interno = & $PkgTool sfo_listentries "$saida\param_sfo_interno.bin" | Where-Object { "$_" -match '^APP_VER' }
if ("$interno" -notmatch [regex]::Escape($Versao)) { throw "SFO interno errado: $interno" }
$tamanho = (Get-Item $pkg).Length
$hash = (Get-FileHash $pkg -Algorithm SHA256).Hash
Write-Output "PKG validado: $pkg"
Write-Output "Tamanho: $tamanho | SHA256: $hash | $interno"
Write-Output "SHA256 do eboot: $((Get-FileHash "$Projeto\eboot.bin" -Algorithm SHA256).Hash)"

if ($SemEnvio) { Write-Output 'SemEnvio: parou antes da Area de Trabalho e do FTP.'; return }

# ---- 6. Area de Trabalho e PS4 ----
Copy-Item $pkg "$env:USERPROFILE\Desktop\$NomePkg" -Force
curl.exe --ftp-pasv --connect-timeout 10 --silent --show-error -u anonymous:anonymous -T $pkg "ftp://$Ps4/data/pkg/"
Exigir 'Envio FTP'
$remoto = curl.exe --ftp-pasv --connect-timeout 10 --silent -u anonymous:anonymous -I "ftp://$Ps4/data/pkg/$NomePkg"
$linha = $remoto | Where-Object { "$_" -match 'Content-Length' }
if ("$linha" -notmatch "\b$tamanho\b") { throw "Tamanho remoto diferente: $linha" }
Write-Output "Enviado ao PS4: /data/pkg/$NomePkg ($tamanho bytes)"
Write-Output 'Lembrete: enviado nao e instalado nem testado.'
