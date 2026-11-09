# Escritório 3D - instalador
# 1) copia os arquivos para %LOCALAPPDATA%\Escritorio3D
# 2) liga os avisos (ganchos) no Claude Code deste computador
# 3) cria a pasta de trabalho com as regras
# 4) cria o atalho na área de trabalho e liga o escritório junto com o Windows

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$origem = $PSScriptRoot
$dest   = Join-Path $env:LOCALAPPDATA 'Escritorio3D'
$utf8   = New-Object System.Text.UTF8Encoding($false)

function Passo([string]$t) { Write-Host ''; Write-Host ('  > ' + $t) -ForegroundColor Cyan }
function Ok([string]$t) { Write-Host ('    ok: ' + $t) -ForegroundColor Green }

Write-Host ''
Write-Host '  ESCRITÓRIO 3D · instalação' -ForegroundColor White
Write-Host '  ------------------------------'

$arquivos = @('servidor.ps1', 'comum.ps1', 'inicio.ps1', 'aviso.ps1', 'abrir.ps1', 'desinstalar.ps1', 'index.html', 'CLAUDE.md', 'ideias.txt')
foreach ($a in $arquivos) {
  if (-not (Test-Path (Join-Path $origem $a))) {
    Write-Host ''
    Write-Host '  Faltou o arquivo' $a -ForegroundColor Red
    Write-Host '  Clique com o botão direito no .zip, escolha "Extrair tudo" e rode o INSTALAR.bat de dentro da pasta extraída.' -ForegroundColor Yellow
    exit 1
  }
}

# Desliga um servidor antigo, se houver
. (Join-Path $origem 'comum.ps1')
[void](Esc-Pede 'GET' "$EscBase/parar" $null 1500)
Start-Sleep -Milliseconds 400

Passo 'Copiando os arquivos'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
foreach ($a in $arquivos) { Copy-Item (Join-Path $origem $a) (Join-Path $dest $a) -Force }
# Logo opcional: ponha seus arquivos logo-branco.png e logo-h-branco.png na pasta antes de instalar
Get-ChildItem $origem -Filter 'logo-*.png' | ForEach-Object { Copy-Item $_.FullName (Join-Path $dest $_.Name) -Force }
Get-ChildItem $dest | ForEach-Object { try { Unblock-File $_.FullName } catch {} }
$tokenArq = Join-Path $dest 'token.txt'
if (-not (Test-Path $tokenArq)) { [System.IO.File]::WriteAllText($tokenArq, [Guid]::NewGuid().ToString('N'), $utf8) }
$token = ([System.IO.File]::ReadAllText($tokenArq)).Trim()
Ok $dest

Passo 'Ligando os avisos no Claude Code'
function H-Http {
  return [pscustomobject][ordered]@{ type = 'http'; url = 'http://127.0.0.1:4747/evento'; headers = [pscustomobject]@{ 'X-Esc' = $token }; timeout = 3 }
}
function H-Cmd([string]$script, [int]$tempo, [bool]$fundo) {
  $o = [ordered]@{
    type    = 'command'
    command = 'powershell.exe'
    args    = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', (Join-Path $dest $script))
    timeout = $tempo
  }
  if ($fundo) { $o['async'] = $true }
  return [pscustomobject]$o
}
$novos = [ordered]@{
  SessionStart     = @([pscustomobject]@{ hooks = @(H-Cmd 'inicio.ps1' 20 $false) })
  UserPromptSubmit = @([pscustomobject]@{ hooks = @(H-Http) })
  PreToolUse       = @([pscustomobject][ordered]@{ matcher = 'Skill'; hooks = @(H-Http) })
  PostToolUse      = @([pscustomobject][ordered]@{ matcher = '*'; hooks = @(H-Http) })
  Notification     = @([pscustomobject]@{ hooks = @(H-Cmd 'aviso.ps1' 10 $true) })
  Stop             = @([pscustomobject]@{ hooks = @(H-Cmd 'aviso.ps1' 10 $true) })
  SessionEnd       = @([pscustomobject]@{ hooks = @(H-Cmd 'aviso.ps1' 5 $false) })
}

$pastaClaude = Join-Path $env:USERPROFILE '.claude'
$cfgArq = Join-Path $pastaClaude 'settings.json'
New-Item -ItemType Directory -Force -Path $pastaClaude | Out-Null
$cfg = $null
if (Test-Path $cfgArq) {
  $raw = [System.IO.File]::ReadAllText($cfgArq)
  Copy-Item $cfgArq ($cfgArq + '.antes-do-escritorio-3d') -Force
  if ($raw.Trim()) {
    try { $cfg = $raw | ConvertFrom-Json }
    catch {
      Write-Host '    O arquivo de configuração do Claude Code está com defeito. Não mexi nele.' -ForegroundColor Red
      Write-Host ('    Arquivo: ' + $cfgArq) -ForegroundColor Yellow
      exit 1
    }
  }
}
if ($null -eq $cfg) { $cfg = New-Object PSObject }
if (-not ($cfg.PSObject.Properties.Name -contains 'hooks') -or $null -eq $cfg.hooks) {
  $cfg | Add-Member -NotePropertyName 'hooks' -NotePropertyValue (New-Object PSObject) -Force
}
foreach ($ev in $novos.Keys) {
  $atual = @()
  if ($cfg.hooks.PSObject.Properties.Name -contains $ev) {
    $atual = @($cfg.hooks.$ev | Where-Object { $null -ne $_ -and -not ((ConvertTo-Json -InputObject $_ -Depth 20 -Compress) -match '127\.0\.0\.1:4747|Escritorio3D') })
  }
  $lista = @($atual) + @($novos[$ev])
  $cfg.hooks | Add-Member -NotePropertyName $ev -NotePropertyValue $lista -Force
}
[System.IO.File]::WriteAllText($cfgArq, (ConvertTo-Json -InputObject $cfg -Depth 30), $utf8)
Ok $cfgArq

Passo 'Criando a pasta de trabalho'
$docs = [Environment]::GetFolderPath('MyDocuments')
$trab = Join-Path $docs 'Meu Claude Code'
New-Item -ItemType Directory -Force -Path $trab | Out-Null
$regras = Join-Path $trab 'CLAUDE.md'
if (Test-Path $regras) {
  Copy-Item (Join-Path $origem 'CLAUDE.md') (Join-Path $trab 'CLAUDE.novo.md') -Force
  Ok ($trab + '  (já tinha um CLAUDE.md, a versão nova ficou como CLAUDE.novo.md)')
} else {
  Copy-Item (Join-Path $origem 'CLAUDE.md') $regras -Force
  Ok $trab
}

Passo 'Criando os atalhos'
$ps = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
try {
  $ws = New-Object -ComObject WScript.Shell
  $lnk = $ws.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Escritório 3D.lnk'))
  $lnk.TargetPath = $ps
  $lnk.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + (Join-Path $dest 'abrir.ps1') + '"'
  $lnk.WindowStyle = 7
  $lnk.IconLocation = (Join-Path $env:WINDIR 'System32\imageres.dll') + ',1'
  $lnk.Description = 'Abre o Escritório 3D'
  $lnk.Save()
  Ok 'Atalho "Escritório 3D" na área de trabalho'

  $ini = $ws.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Startup')) 'Escritorio 3D (servidor).lnk'))
  $ini.TargetPath = $ps
  $ini.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + (Join-Path $dest 'servidor.ps1') + '"'
  $ini.WindowStyle = 7
  $ini.Save()
  Ok 'O escritório liga sozinho quando o Windows abre'
} catch {
  Write-Host '    Não consegui criar os atalhos. O resto funciona.' -ForegroundColor Yellow
}

Passo 'Ligando o escritório'
& (Join-Path $dest 'abrir.ps1')
Start-Sleep -Milliseconds 300
if (Esc-Vivo) { Ok 'Escritório no ar em http://127.0.0.1:4747' }
else { Write-Host '    O escritório não respondeu. Clique no atalho "Escritório 3D" da área de trabalho.' -ForegroundColor Yellow }

Write-Host ''
Write-Host '  PRONTO.' -ForegroundColor Green
Write-Host '  Agora abra o app do Claude, vá na aba Code, escolha "Local" e a pasta "Meu Claude Code" em Documentos.'
Write-Host '  Cada chat que você abrir ali aparece no escritório.'
Write-Host ''
