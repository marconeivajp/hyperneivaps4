# Escritorio 3D - funcoes usadas pelos outros scripts
$ProgressPreference = 'SilentlyContinue'
$EscBase = 'http://127.0.0.1:4747'

function Esc-Pede([string]$metodo, [string]$url, [byte[]]$corpo, [int]$ms) {
  try {
    $req = [System.Net.HttpWebRequest]::Create($url)
    $req.Method = $metodo
    $req.Proxy = $null
    $req.Timeout = $ms
    $req.ReadWriteTimeout = $ms
    $req.KeepAlive = $false
    if ($corpo) {
      $tok = ''
      $t = Join-Path $PSScriptRoot 'token.txt'
      if (Test-Path $t) { $tok = ([System.IO.File]::ReadAllText($t)).Trim() }
      $req.ContentType = 'application/json; charset=utf-8'
      $req.Headers.Add('X-Esc', $tok)
      $req.ContentLength = $corpo.Length
      $s = $req.GetRequestStream()
      $s.Write($corpo, 0, $corpo.Length)
      $s.Close()
    }
    $resp = $req.GetResponse()
    $cod = [int]$resp.StatusCode
    $resp.Close()
    return $cod
  } catch { return 0 }
}

# Abre o escritorio no Edge (tem as vozes naturais em portugues). Sem Edge, usa o navegador padrao.
function Esc-Abre {
  try { Start-Process 'msedge.exe' -ArgumentList "$EscBase/" -ErrorAction Stop } catch { Start-Process "$EscBase/" }
}

function Esc-Vivo { return ((Esc-Pede 'GET' "$EscBase/saude" $null 1500) -eq 200) }

# Verdadeiro se o relay de rede (porta 4748) estiver ligado.
function Esc-RedeViva {
  try {
    $c = New-Object System.Net.Sockets.TcpClient
    $r = $c.BeginConnect('127.0.0.1', 4748, $null, $null)
    $ok = $r.AsyncWaitHandle.WaitOne(400)
    if ($ok) { $c.EndConnect($r) }
    $c.Close()
    return $ok
  } catch { return $false }
}

# Liga o relay de rede se estiver desligado. Precisa do Node instalado.
function Esc-GaranteRede {
  if (Esc-RedeViva) { return $false }
  $node = Get-Command node.exe -ErrorAction SilentlyContinue
  if (-not $node) { return $false }
  $arq = Join-Path $PSScriptRoot 'servidor-rede.js'
  if (-not (Test-Path -LiteralPath $arq)) { return $false }
  Start-Process -FilePath $node.Source -WindowStyle Hidden -ArgumentList @(('"' + $arq + '"'))
  for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 200
    if (Esc-RedeViva) { break }
  }
  return $true
}

# Liga o servidor se estiver desligado. Devolve $true se precisou ligar.
function Esc-Garante {
  [void](Esc-GaranteRede)
  if (Esc-Vivo) { return $false }
  $srvArq = Join-Path $PSScriptRoot 'servidor.ps1'
  Start-Process -FilePath 'powershell.exe' -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', ('"' + $srvArq + '"'))
  for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 200
    if (Esc-Vivo) { break }
  }
  return $true
}

# Le o aviso que o Claude Code manda (sem mexer na codificacao) e repassa ao servidor
function Esc-Repassa {
  try {
    $in = [Console]::OpenStandardInput()
    $ms = New-Object System.IO.MemoryStream
    $in.CopyTo($ms)
    $bytes = $ms.ToArray()
    if ($bytes.Length -gt 0) { [void](Esc-Pede 'POST' "$EscBase/evento" $bytes 3000) }
  } catch {}
}
