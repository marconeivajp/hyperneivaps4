# Escritorio 3D - roda quando um chat do Claude Code abre
# Liga o servidor se precisar, avisa o escritorio e abre a pagina na primeira vez do dia.
. (Join-Path $PSScriptRoot 'comum.ps1')
$ligou = Esc-Garante
Esc-Repassa
if ($ligou) { Esc-Abre }
exit 0
