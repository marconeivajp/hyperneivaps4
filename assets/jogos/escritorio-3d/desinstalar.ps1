# Escritório 3D - desinstalar
# Tira os avisos do Claude Code, apaga os atalhos e a pasta do programa.
# A pasta "Meu Claude Code" em Documentos fica (é sua).
$ErrorActionPreference = 'Continue'
$ProgressPreference = 'SilentlyContinue'
$dest = Join-Path $env:LOCALAPPDATA 'Escritorio3D'
$utf8 = New-Object System.Text.UTF8Encoding($false)

$comum = Join-Path $PSScriptRoot 'comum.ps1'
if (Test-Path $comum) { . $comum; [void](Esc-Pede 'GET' "$EscBase/parar" $null 1500) }

$cfgArq = Join-Path (Join-Path $env:USERPROFILE '.claude') 'settings.json'
if (Test-Path $cfgArq) {
  try {
    $cfg = [System.IO.File]::ReadAllText($cfgArq) | ConvertFrom-Json
    if ($cfg.hooks) {
      foreach ($ev in @($cfg.hooks.PSObject.Properties.Name)) {
        $resto = @($cfg.hooks.$ev | Where-Object { $null -ne $_ -and -not ((ConvertTo-Json -InputObject $_ -Depth 20 -Compress) -match '127\.0\.0\.1:4747|Escritorio3D') })
        if ($resto.Count -eq 0) { $cfg.hooks.PSObject.Properties.Remove($ev) }
        else { $cfg.hooks | Add-Member -NotePropertyName $ev -NotePropertyValue $resto -Force }
      }
      if (@($cfg.hooks.PSObject.Properties).Count -eq 0) { $cfg.PSObject.Properties.Remove('hooks') }
    }
    [System.IO.File]::WriteAllText($cfgArq, (ConvertTo-Json -InputObject $cfg -Depth 30), $utf8)
    Write-Host '  ok: avisos retirados do Claude Code' -ForegroundColor Green
  } catch { Write-Host '  Não consegui ler a configuração do Claude Code. Nada foi mudado nela.' -ForegroundColor Yellow }
}

Remove-Item (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Escritório 3D.lnk') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path ([Environment]::GetFolderPath('Startup')) 'Escritorio 3D (servidor).lnk') -Force -ErrorAction SilentlyContinue
Write-Host '  ok: atalhos apagados' -ForegroundColor Green

Start-Sleep -Milliseconds 500
Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
Write-Host '  ok: escritório removido deste computador' -ForegroundColor Green
