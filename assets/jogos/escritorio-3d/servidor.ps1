# Escritorio 3D - servidor local
# Recebe os avisos do Claude Code e entrega a pagina do escritorio em http://127.0.0.1:4747
# Fica so neste computador. Nao fala com a internet.

$ErrorActionPreference = 'Continue'
# Quando o servidor nasce de dentro de um chat do Claude, herda variaveis daquele chat.
# Tira todas para o Claude Code chamado daqui usar o login proprio deste computador.
Get-ChildItem env: | Where-Object { $_.Name -match '^(CLAUDE|ANTHROPIC_)' } | ForEach-Object { [Environment]::SetEnvironmentVariable($_.Name, $null, 'Process') }
$ProgressPreference = 'SilentlyContinue'
$dir      = $PSScriptRoot
$porta    = 4747
$arq      = Join-Path $dir 'eventos.jsonl'
$pagina   = Join-Path $dir 'index.html'
$tokenArq = Join-Path $dir 'token.txt'
$utf8     = New-Object System.Text.UTF8Encoding($false)
$token    = ''
if (Test-Path $tokenArq) { $token = ([System.IO.File]::ReadAllText($tokenArq)).Trim() }

# Chat com o supervisor e brainstorm: roda o Claude Code deste computador (a sua assinatura).
# A pagina recebe uma chave nova a cada vez que o servidor liga; sem ela nada roda.
$chave    = [Guid]::NewGuid().ToString('N')
$trabalho = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Meu Claude Code'
$conversas = Join-Path $dir 'conversas'
$ideiasDir = Join-Path $dir 'ideias'
New-Item -ItemType Directory -Force -Path $conversas, $ideiasDir | Out-Null
Get-ChildItem $conversas -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-2) } | Remove-Item -Force -ErrorAction SilentlyContinue
$jobs = @{}
$claudeExe = $null

function Acha-Claude {
  if ($script:claudeExe -and (Test-Path $script:claudeExe)) { return $script:claudeExe }
  # 1) instalado no computador (instalador oficial ou npm)
  foreach ($n in @('claude.exe', 'claude.cmd')) {
    $c = Get-Command $n -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($c) { $script:claudeExe = $c.Source; return $script:claudeExe }
  }
  foreach ($p in @((Join-Path $env:USERPROFILE '.local\bin\claude.exe'), (Join-Path $env:APPDATA 'npm\claude.cmd'))) {
    if (Test-Path $p) { $script:claudeExe = $p; return $script:claudeExe }
  }
  # 2) o que vem junto com o app do Claude (aba Code); o app mesmo tambem se chama Claude.exe, por isso exige a pasta claude-code
  $lugares = @((Join-Path $env:LOCALAPPDATA 'Claude-Data'), (Join-Path $env:APPDATA 'Claude'), (Join-Path $env:LOCALAPPDATA 'Claude'), (Join-Path $env:LOCALAPPDATA 'AnthropicClaude'))
  $lugares += @(Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Packages') -Directory -Filter '*Claude*' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
  $f = $lugares | Where-Object { Test-Path $_ } | ForEach-Object { Get-ChildItem $_ -Recurse -Filter claude.exe -Depth 8 -ErrorAction SilentlyContinue } |
    Where-Object { $_.FullName -match '\\claude-code\\' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($f) { $script:claudeExe = $f.FullName }
  return $script:claudeExe
}

function Liberado($h) {
  if ($h['x-chave'] -ne $chave) { return $false }
  if ($h.ContainsKey('origin') -and $h['origin'] -notmatch '^http://(127\.0\.0\.1|localhost):4747$') { return $false }
  return $true
}

function Json($o) { return $utf8.GetBytes((ConvertTo-Json -InputObject $o -Compress -Depth 4)) }

function Inicia-Claude($o) {
  $exe = Acha-Claude
  if (-not $exe) { return @{ erro = 'sem-claude' } }
  $texto = [string]$o.texto
  if (-not $texto.Trim()) { return @{ erro = 'vazio' } }
  $id = [Guid]::NewGuid().ToString('N').Substring(0, 12)
  $in = Join-Path $conversas "$id.in.txt"; $out = Join-Path $conversas "$id.out.jsonl"; $err = Join-Path $conversas "$id.err.txt"
  [System.IO.File]::WriteAllText($in, $texto, $utf8)
  $a = @('-p', '--output-format', 'stream-json', '--verbose')
  $sess = [string]$o.sessao
  if ($sess -match '^[0-9a-f\-]{36}$') { $a += @('--resume', $sess) }
  # modelo e esforco escolhidos pela pagina conforme o tamanho do pedido
  if ([string]$o.modelo -match '^(haiku|sonnet|opus)$') { $a += @('--model', [string]$o.modelo) }
  if ([string]$o.esforco -match '^(low|medium|high)$') { $a += @('--effort', [string]$o.esforco) }
  if ([string]$o.modo -eq 'ideias') {
    $pasta = $ideiasDir
    $a += @('--tools', '""', '--strict-mcp-config', '--system-prompt-file', ('"' + (Join-Path $dir 'ideias.txt') + '"'))
  } else {
    $pasta = $trabalho
    New-Item -ItemType Directory -Force -Path $pasta | Out-Null
    $a += @('--permission-mode', 'acceptEdits')
    $lib = @($o.liberar | Where-Object { [string]$_ -match '^[A-Za-z0-9_\-\.:]+$' })
    if ($lib.Count) { $a += @('--allowedTools', ('"' + ($lib -join ',') + '"')) }
  }
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = Join-Path $env:WINDIR 'System32\cmd.exe'
  $psi.Arguments = '/d /s /c ""' + $exe + '" ' + ($a -join ' ') + ' < "' + $in + '" > "' + $out + '" 2> "' + $err + '""'
  $psi.WorkingDirectory = $pasta
  $psi.UseShellExecute = $false
  $psi.CreateNoWindow = $true
  foreach ($k in @('CLAUDECODE', 'CLAUDE_CODE_ENTRYPOINT', 'CLAUDE_CODE_SSE_PORT')) { if ($psi.EnvironmentVariables.ContainsKey($k)) { $psi.EnvironmentVariables.Remove($k) } }
  $p = [System.Diagnostics.Process]::Start($psi)
  $jobs[$id] = @{ p = $p; out = $out; err = $err }
  return @{ id = $id }
}

function Le-Trabalho([string]$id, [int64]$desde) {
  $j = $jobs[$id]
  if (-not $j) { return @{ erro = 'nao-achei'; fim = $true } }
  $fim = $j.p.HasExited
  $txt = ''; $novo = $desde
  if (Test-Path $j.out) {
    $fs = New-Object System.IO.FileStream($j.out, 'Open', 'Read', 'ReadWrite')
    try {
      if ($fs.Length -gt $desde) {
        [void]$fs.Seek($desde, 'Begin')
        $b = New-Object byte[] ($fs.Length - $desde)
        $n = $fs.Read($b, 0, $b.Length)
        $ult = [Array]::LastIndexOf($b, [byte]10, $n - 1)
        if ($fim -and $n -gt 0) { $ult = $n - 1 }
        if ($ult -ge 0) { $txt = $utf8.GetString($b, 0, $ult + 1); $novo = $desde + $ult + 1 }
      }
    } finally { $fs.Close() }
  }
  $r = @{ fim = $fim; desde = $novo; linhas = $txt }
  if ($fim -and (Test-Path $j.err)) { $e = [System.IO.File]::ReadAllText($j.err); if ($e) { $r['erro'] = Corta $e 600 } }
  return $r
}

$srv = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Loopback, $porta)
try { $srv.Start() } catch { exit 0 }   # ja tem um servidor rodando

$boot  = [Guid]::NewGuid().ToString('N').Substring(0, 8)
$lista = New-Object System.Collections.ArrayList
$base  = 0
if (Test-Path $arq) {
  try {
    $linhas = [System.IO.File]::ReadAllLines($arq, $utf8)
    $ini = [Math]::Max(0, $linhas.Length - 2000)
    for ($i = $ini; $i -lt $linhas.Length; $i++) {
      $l = $linhas[$i].Trim()
      if ($l.StartsWith('{"ts"')) { [void]$lista.Add($l) }
    }
  } catch {}
}
$gravadas = $lista.Count

function Corta([string]$s, [int]$n) {
  if ($null -eq $s) { return $null }
  if ($s.Length -gt $n) { return $s.Substring(0, $n) }
  return $s
}

function Enxuga([string]$corpo) {
  $o = $corpo | ConvertFrom-Json
  $d = [ordered]@{}
  $d['evento'] = [string]$o.hook_event_name
  $d['sessao'] = [string]$o.session_id
  if ($o.cwd) { $d['pasta'] = Split-Path -Leaf ([string]$o.cwd) }
  if ($o.source) { $d['origem'] = [string]$o.source }
  if ($o.prompt) { $d['pedido'] = Corta ([string]$o.prompt) 400 }
  if ($o.tool_name) { $d['ferramenta'] = [string]$o.tool_name }
  if ($o.tool_input) { if ($o.tool_input.skill) { $d['skill'] = [string]$o.tool_input.skill } }
  if ($o.notification_type) { $d['tipo'] = [string]$o.notification_type }
  if ($o.message) { $d['mensagem'] = Corta ([string]$o.message) 300 }
  if ($o.last_assistant_message) { $d['resposta'] = Corta ([string]$o.last_assistant_message) 400 }
  if ($o.reason) { $d['motivo'] = Corta ([string]$o.reason) 100 }
  return (ConvertTo-Json -InputObject $d -Compress -Depth 3)
}

# Para aviso grande demais para ler inteiro: pega so o nome do evento e a sessao
function Minimo([string]$corpo) {
  $e = [regex]::Match($corpo, '"hook_event_name"\s*:\s*"([^"]+)"')
  $s = [regex]::Match($corpo, '"session_id"\s*:\s*"([^"]+)"')
  if (-not $e.Success) { return $null }
  $d = [ordered]@{ evento = $e.Groups[1].Value; sessao = $s.Groups[1].Value }
  $k = [regex]::Match($corpo, '"skill"\s*:\s*"([^"]+)"')
  if ($k.Success) { $d['skill'] = $k.Groups[1].Value }
  $f = [regex]::Match($corpo, '"tool_name"\s*:\s*"([^"]+)"')
  if ($f.Success) { $d['ferramenta'] = $f.Groups[1].Value }
  return (ConvertTo-Json -InputObject $d -Compress)
}

function Responde($st, [int]$cod, [string]$tipo, [byte[]]$dados) {
  $nomes = @{ 200 = 'OK'; 400 = 'Bad Request'; 401 = 'Unauthorized'; 404 = 'Not Found'; 500 = 'Error' }
  if ($null -eq $dados) { $dados = New-Object byte[] 0 }
  $cab = "HTTP/1.1 $cod $($nomes[$cod])`r`nContent-Type: $tipo`r`nContent-Length: $($dados.Length)`r`nCache-Control: no-store`r`nConnection: close`r`n`r`n"
  $b = [System.Text.Encoding]::ASCII.GetBytes($cab)
  $st.Write($b, 0, $b.Length)
  if ($dados.Length -gt 0) { $st.Write($dados, 0, $dados.Length) }
  $st.Flush()
}

$vazio = New-Object byte[] 0
$parar = $false

while (-not $parar) {
  $cli = $null
  $st = $null
  try {
    $cli = $srv.AcceptTcpClient()
    $cli.ReceiveTimeout = 3000
    $cli.SendTimeout = 3000
    $st  = $cli.GetStream()
    $ms  = New-Object System.IO.MemoryStream
    $buf = New-Object byte[] 65536
    $fimCab = -1
    $txt = ''
    while ($fimCab -lt 0) {
      $n = $st.Read($buf, 0, $buf.Length)
      if ($n -le 0) { break }
      $ms.Write($buf, 0, $n)
      $txt = [System.Text.Encoding]::ASCII.GetString($ms.ToArray())
      $fimCab = $txt.IndexOf("`r`n`r`n")
      if ($fimCab -lt 0 -and $ms.Length -gt 65536) { break }
    }
    if ($fimCab -lt 0) { continue }

    $linhasCab = $txt.Substring(0, $fimCab) -split "`r`n"
    $partes = $linhasCab[0] -split ' '
    $metodo = $partes[0]
    $alvo = '/'
    if ($partes.Length -gt 1) { $alvo = $partes[1] }
    $h = @{}
    for ($i = 1; $i -lt $linhasCab.Length; $i++) {
      $p = $linhasCab[$i].IndexOf(':')
      if ($p -gt 0) { $h[$linhasCab[$i].Substring(0, $p).Trim().ToLower()] = $linhasCab[$i].Substring($p + 1).Trim() }
    }

    $tam = 0
    if ($h.ContainsKey('content-length')) { $tam = [int]$h['content-length'] }
    if ($tam -gt 20MB) { Responde $st 400 'text/plain' $vazio; continue }
    $corpoBytes = New-Object byte[] $tam
    if ($tam -gt 0) {
      $todos = $ms.ToArray()
      $inicio = $fimCab + 4
      $ja = [Math]::Min($todos.Length - $inicio, $tam)
      if ($ja -gt 0) { [Array]::Copy($todos, $inicio, $corpoBytes, 0, $ja) } else { $ja = 0 }
      while ($ja -lt $tam) {
        $n = $st.Read($corpoBytes, $ja, $tam - $ja)
        if ($n -le 0) { break }
        $ja += $n
      }
    }

    $caminho = $alvo
    $consulta = ''
    $q = $alvo.IndexOf('?')
    if ($q -ge 0) { $caminho = $alvo.Substring(0, $q); $consulta = $alvo.Substring($q + 1) }

    if ($metodo -eq 'GET' -and ($caminho -eq '/' -or $caminho -eq '/index.html')) {
      $html = [System.IO.File]::ReadAllText($pagina, $utf8).Replace('__CHAVE__', $chave)
      Responde $st 200 'text/html; charset=utf-8' ($utf8.GetBytes($html))
    }
    elseif ($metodo -eq 'GET' -and $caminho -match '^/(logo-[a-z\-]+\.png)$' -and (Test-Path (Join-Path $dir $Matches[1]))) {
      Responde $st 200 'image/png' ([System.IO.File]::ReadAllBytes((Join-Path $dir $Matches[1])))
    }
    elseif ($metodo -eq 'POST' -and $caminho -eq '/pedir') {
      if (-not (Liberado $h)) { Responde $st 401 'text/plain' $vazio }
      else { Responde $st 200 'application/json; charset=utf-8' (Json (Inicia-Claude ($utf8.GetString($corpoBytes) | ConvertFrom-Json))) }
    }
    elseif ($metodo -eq 'GET' -and $caminho -eq '/trabalho') {
      if (-not (Liberado $h)) { Responde $st 401 'text/plain' $vazio }
      else {
        $mi = [regex]::Match($consulta, 'id=([0-9a-f]{12})'); $md = [regex]::Match($consulta, 'desde=(\d+)')
        $d = 0; if ($md.Success) { $d = [int64]$md.Groups[1].Value }
        Responde $st 200 'application/json; charset=utf-8' (Json (Le-Trabalho $mi.Groups[1].Value $d))
      }
    }
    elseif ($metodo -eq 'POST' -and $caminho -eq '/cancelar') {
      if (-not (Liberado $h)) { Responde $st 401 'text/plain' $vazio }
      else {
        $o = $utf8.GetString($corpoBytes) | ConvertFrom-Json
        $j = $jobs[[string]$o.id]
        if ($j -and -not $j.p.HasExited) { & taskkill.exe /T /F /PID $j.p.Id | Out-Null }
        Responde $st 200 'application/json' (Json @{ ok = $true })
      }
    }
    elseif ($metodo -eq 'POST' -and $caminho -eq '/login') {
      if (-not (Liberado $h)) { Responde $st 401 'text/plain' $vazio }
      else {
        $exe = Acha-Claude
        if ($exe) {
          New-Item -ItemType Directory -Force -Path $trabalho | Out-Null
          Start-Process -FilePath 'powershell.exe' -WorkingDirectory $trabalho -ArgumentList @('-NoExit', '-Command', ("Write-Host 'Digite /login, entre na sua conta do Claude e depois feche esta janela.' -ForegroundColor Yellow; & '" + $exe + "'"))
        }
        Responde $st 200 'application/json' (Json @{ ok = [bool]$exe })
      }
    }
    elseif ($metodo -eq 'GET' -and $caminho -eq '/saude') {
      Responde $st 200 'text/plain; charset=utf-8' ($utf8.GetBytes('ok'))
    }
    elseif ($metodo -eq 'GET' -and $caminho -eq '/eventos') {
      $desde = 0
      $mq = [regex]::Match($consulta, 'desde=(\d+)')
      if ($mq.Success) { $desde = [int]$mq.Groups[1].Value }
      $ultimo = $base + $lista.Count
      $ini = [Math]::Max(0, $desde - $base)
      $sb = New-Object System.Text.StringBuilder
      [void]$sb.Append('{"boot":"' + $boot + '","ultimo":' + $ultimo + ',"eventos":[')
      for ($i = $ini; $i -lt $lista.Count; $i++) {
        if ($i -gt $ini) { [void]$sb.Append(',') }
        [void]$sb.Append('{"seq":' + ($base + $i + 1) + ',' + ([string]$lista[$i]).Substring(1))
      }
      [void]$sb.Append(']}')
      Responde $st 200 'application/json; charset=utf-8' ($utf8.GetBytes($sb.ToString()))
    }
    elseif ($metodo -eq 'POST' -and $caminho -eq '/evento') {
      if ($token -and ($h['x-esc'] -ne $token)) {
        Responde $st 401 'text/plain' $vazio
      }
      else {
        $corpo = $utf8.GetString($corpoBytes)
        $limpo = $null
        try { $limpo = Enxuga $corpo } catch { try { $limpo = Minimo $corpo } catch { $limpo = $null } }
        if ($limpo) {
          $linha = '{"ts":"' + [DateTime]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") + '","dados":' + $limpo + '}'
          [void]$lista.Add($linha)
          try { [System.IO.File]::AppendAllText($arq, $linha + "`n", $utf8) } catch {}
          $gravadas++
          if ($lista.Count -gt 3000) { $lista.RemoveRange(0, 1000); $base += 1000 }
          if ($gravadas -gt 4000) {
            try { [System.IO.File]::WriteAllLines($arq, [string[]]$lista.ToArray(), $utf8) } catch {}
            $gravadas = $lista.Count
          }
        }
        Responde $st 200 'application/json' $vazio
      }
    }
    elseif ($metodo -eq 'GET' -and $caminho -match '^/([A-Za-z0-9_\-./]+)$') {
      $rel = $Matches[1] -replace '/', '\'
      $completo = [System.IO.Path]::GetFullPath((Join-Path $dir $rel))
      $raiz = [System.IO.Path]::GetFullPath($dir)
      $ext = [System.IO.Path]::GetExtension($completo).ToLower()
      $tipos = @{
        '.html' = 'text/html; charset=utf-8'; '.js' = 'application/javascript; charset=utf-8'; '.mjs' = 'application/javascript; charset=utf-8';
        '.css' = 'text/css; charset=utf-8'; '.json' = 'application/json; charset=utf-8'; '.png' = 'image/png';
        '.jpg' = 'image/jpeg'; '.jpeg' = 'image/jpeg'; '.gif' = 'image/gif'; '.svg' = 'image/svg+xml'; '.webp' = 'image/webp';
        '.ico' = 'image/x-icon'; '.glb' = 'model/gltf-binary'; '.gltf' = 'model/gltf+json'; '.wasm' = 'application/wasm';
        '.mp3' = 'audio/mpeg'; '.ogg' = 'audio/ogg'; '.wav' = 'audio/wav'; '.mp4' = 'video/mp4';
        '.woff2' = 'font/woff2'; '.woff' = 'font/woff'
      }
      if ($tipos.ContainsKey($ext) -and $completo.StartsWith($raiz + '\', [System.StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $completo -PathType Leaf)) {
        Responde $st 200 $tipos[$ext] ([System.IO.File]::ReadAllBytes($completo))
      } else {
        Responde $st 404 'text/plain' $vazio
      }
    }
    else {
      Responde $st 404 'text/plain' $vazio
    }
  }
  catch {
    try { if ($st) { Responde $st 500 'text/plain' $vazio } } catch {}
  }
  finally {
    if ($cli) { try { $cli.Close() } catch {} }
  }
}
$srv.Stop()
