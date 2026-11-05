"""Gera icones_embutidos.h com os icones do menu e do player dentro do executavel.

Rode depois de mudar um icone:  python tools/gerar_icones.py
Precisa de rsvg-convert e ffmpeg do MSYS2 (C:\\msys64\\mingw64\\bin).
Ficar dentro do eboot evita depender de arquivos do PKG e de caminhos em /app0.
"""
import glob
import os
import subprocess
import tempfile

BIN = r"C:\msys64\mingw64\bin"
RSVG, FFMPEG = os.path.join(BIN, "rsvg-convert.exe"), os.path.join(BIN, "ffmpeg.exe")
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ["PATH"] = BIN + os.pathsep + os.environ["PATH"]

# Desenhos originais em caixa 24x24, usando branco. "t" = traço, sem preenchimento.
T = 'fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
PLAYER = {
    "play": '<path fill="#fff" d="M8 4.5v15l12-7.5z"/>',
    "pause": '<path fill="#fff" d="M6.5 5h4v14h-4zM13.5 5h4v14h-4z"/>',
    "anterior": '<path fill="#fff" d="M5 5h2.5v14H5zM20 5v14L9 12z"/>',
    "proximo": '<path fill="#fff" d="M16.5 5H19v14h-2.5zM4 5v14l11-7z"/>',
    "voltar": '<path fill="#fff" d="M12 5v14L2.5 12zM21.5 5v14L12 12z"/>',
    "avancar": '<path fill="#fff" d="M12 5v14l9.5-7zM2.5 5v14L12 12z"/>',
    "volume": f'<path fill="#fff" d="M3 9v6h4l5.5 5V4L7 9z"/><path {T} d="M15.5 8.5a5 5 0 0 1 0 7M18 6a8.5 8.5 0 0 1 0 12"/>',
    "mudo": f'<path fill="#fff" d="M3 9v6h4l5.5 5V4L7 9z"/><path {T} d="M16 9.5l5 5M21 9.5l-5 5"/>',
    "pip": f'<rect {T} x="2.5" y="4.5" width="19" height="15" rx="2"/><rect fill="#fff" x="12" y="11" width="7" height="6" rx="1"/>',
    "tres_pontos": '<circle fill="#fff" cx="5" cy="12" r="2.3"/><circle fill="#fff" cx="12" cy="12" r="2.3"/><circle fill="#fff" cx="19" cy="12" r="2.3"/>',
}
MENU = {
    "jogar": ("#2e7d32", f'<path {T} d="M7 8h10a4.5 4.5 0 0 1 4.4 5.4l-.6 3a2.4 2.4 0 0 1-4.2 1L15 16H9l-1.6 1.4a2.4 2.4 0 0 1-4.2-1l-.6-3A4.5 4.5 0 0 1 7 8zM8 11v3M6.5 12.5h3"/><circle fill="#fff" cx="15.5" cy="11.5" r="1"/><circle fill="#fff" cx="17.5" cy="13.5" r="1"/>'),
    "midia": ("#8e24aa", f'<rect {T} x="3" y="5" width="18" height="14" rx="2"/><path fill="#fff" d="M10 9v6l5-3z"/>'),
    "baixar": ("#1565c0", f'<path {T} d="M12 4v11M7 10.5l5 5 5-5M5 19.5h14"/>'),
    "enviar": ("#00838f", f'<path {T} d="M12 16V5M7 9.5l5-5 5 5M5 19.5h14"/>'),
    "editar": ("#ef6c00", f'<path {T} d="M4 20l1-4.5L16 4.5a2 2 0 0 1 3 0l.5.5a2 2 0 0 1 0 3L8.5 19zM14 6.5l3.5 3.5"/>'),
    "explorar": ("#00695c", f'<circle {T} cx="12" cy="12" r="9"/><path fill="#fff" d="M16 8l-2.5 5.5L8 16l2.5-5.5z"/>'),
    "extra": ("#f9a825", '<path fill="#fff" d="M12 3l2.7 5.8 6.3.8-4.6 4.3 1.2 6.2L12 17l-5.6 3.1 1.2-6.2L3 9.6l6.3-.8z"/>'),
    "info": ("#0277bd", f'<circle {T} cx="12" cy="12" r="9"/><path {T} d="M12 11v5.5"/><circle fill="#fff" cx="12" cy="7.5" r="1.3"/>'),
    "radio": ("#c62828", f'<rect {T} x="3" y="9" width="18" height="11" rx="2"/><path {T} d="M6 9l11-5"/><circle {T} cx="8.5" cy="14.5" r="2.5"/><path {T} d="M14 13h4M14 16h4"/>'),
    "casa": ("#37474f", f'<path {T} d="M3.5 11.5L12 4l8.5 7.5M6 10v9.5h4.5V14h3v5.5H18V10"/>'),
    "disco": ("#455a64", f'<rect {T} x="3" y="6" width="18" height="12" rx="2"/><path {T} d="M3.5 13.5h17"/><circle fill="#fff" cx="17" cy="15.8" r="1"/>'),
    "usb": ("#546e7a", f'<path {T} d="M12 21V5M12 3l-2 3h4zM12 16l-5-3v-3M12 13.5l5-3V8"/><circle fill="#fff" cx="12" cy="20" r="1.8"/><rect fill="#fff" x="5.5" y="7.5" width="3" height="3"/><circle fill="#fff" cx="17" cy="7" r="1.6"/>'),
    "video": ("#6a1b9a", f'<rect {T} x="3" y="6" width="18" height="12" rx="2"/><path {T} d="M7 6v12M17 6v12M3 10h4M3 14h4M17 10h4M17 14h4"/>'),
    "nota": ("#ad1457", f'<path {T} d="M9 18V6l11-2v12"/><circle {T} cx="6.5" cy="18" r="2.5"/><circle {T} cx="17.5" cy="16" r="2.5"/>'),
    "imagem": ("#00897b", f'<rect {T} x="3" y="5" width="18" height="14" rx="2"/><circle fill="#fff" cx="8.5" cy="10" r="1.6"/><path {T} d="M4 17l5-4.5 3.5 3 3-2.5 4.5 4"/>'),
    "livro": ("#5d4037", f'<path {T} d="M12 7c-2-2-5-2.5-8.5-2v13.5c3.5-.5 6.5 0 8.5 2 2-2 5-2.5 8.5-2V5C17 4.5 14 5 12 7zM12 7v13.5"/>'),
    "arquivos": ("#3949ab", f'<path {T} d="M7 3.5h8l4 4V17a1.5 1.5 0 0 1-1.5 1.5h-10A1.5 1.5 0 0 1 6 17V5a1.5 1.5 0 0 1 1-1.5zM15 3.5v4h4M3.5 8v11.5A1.5 1.5 0 0 0 5 21h9"/>'),
    "nuvem": ("#0288d1", f'<path {T} d="M7 18.5a4.5 4.5 0 0 1-.7-8.9A6 6 0 0 1 18 10.5a4 4 0 0 1-.5 8z"/>'),
    "servidor": ("#283593", f'<rect {T} x="3.5" y="4" width="17" height="7" rx="1.5"/><rect {T} x="3.5" y="13" width="17" height="7" rx="1.5"/><circle fill="#fff" cx="7.5" cy="7.5" r="1.1"/><circle fill="#fff" cx="7.5" cy="16.5" r="1.1"/>'),
    "loja": ("#d84315", f'<path {T} d="M5 8h14l1.2 11.5a1.5 1.5 0 0 1-1.5 1.5H5.3a1.5 1.5 0 0 1-1.5-1.5zM8.5 10.5V7a3.5 3.5 0 0 1 7 0v3.5"/>'),
    "caixa": ("#6d4c41", f'<path {T} d="M12 3l8.5 4.5v9L12 21l-8.5-4.5v-9zM3.5 7.5L12 12l8.5-4.5M12 12v9"/>'),
    "link": ("#00796b", f'<path {T} d="M10 14a4 4 0 0 0 6 .4l3-3a4.2 4.2 0 0 0-6-6l-1.5 1.5M14 10a4 4 0 0 0-6-.4l-3 3a4.2 4.2 0 0 0 6 6l1.5-1.5"/>'),
    "lista": ("#1b5e20", f'<path {T} d="M8.5 6.5H20M8.5 12H20M8.5 17.5H20"/><circle fill="#fff" cx="4.5" cy="6.5" r="1.4"/><circle fill="#fff" cx="4.5" cy="12" r="1.4"/><circle fill="#fff" cx="4.5" cy="17.5" r="1.4"/>'),
    "lupa": ("#4527a0", f'<circle {T} cx="10.5" cy="10.5" r="6.5"/><path {T} d="M15.5 15.5l5 5"/>'),
    "engrenagem": ("#546e7a", f'<circle {T} cx="12" cy="12" r="3"/><path {T} d="M12 3v3M12 18v3M3 12h3M18 12h3M5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1M18.4 5.6l-2.1 2.1M7.7 16.3l-2.1 2.1"/>'),
    "mais": ("#2e7d32", f'<path {T} d="M12 5v14M5 12h14"/>'),
    "aviso": ("#b71c1c", f'<path {T} d="M12 4l9 16H3zM12 10v4.5"/><circle fill="#fff" cx="12" cy="17" r="1.1"/>'),
    "item": ("#607d8b", f'<path {T} d="M9.5 6l6 6-6 6"/>'),
}


def executar(*cmd, **extra):
    subprocess.run(cmd, check=True, capture_output=True, **extra)


def svg_para_png(svg, pasta, nome, tamanho):
    caminho = os.path.join(pasta, nome + ".svg")
    saida = os.path.join(pasta, nome + ".png")
    open(caminho, "w", encoding="utf-8").write(svg)
    executar(RSVG, "-w", str(tamanho), "-h", str(tamanho), caminho, "-o", saida)
    return open(saida, "rb").read()


def main():
    icones = []
    with tempfile.TemporaryDirectory() as pasta:
        for nome, desenho in PLAYER.items():
            svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">{desenho}</svg>'
            icones.append(("p_" + nome, svg_para_png(svg, pasta, "p_" + nome, 64)))
        for nome, (cor, desenho) in MENU.items():
            svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="7" fill="{cor}"/>'
                   f'<rect width="32" height="16" rx="7" fill="#fff" opacity=".10"/><g transform="translate(4 4)">{desenho}</g></svg>')
            icones.append(("m_" + nome, svg_para_png(svg, pasta, "m_" + nome, 64)))

        origem = os.path.join(RAIZ, "assets", "icons")
        pasta_svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">'
                     '<path fill="#c98a1b" d="M3 8a2.5 2.5 0 0 1 2.5-2.5h7l3 3.5h11A2.5 2.5 0 0 1 29 11.5V25a2.5 2.5 0 0 1-2.5 2.5h-21A2.5 2.5 0 0 1 3 25z"/>'
                     '<path fill="#f6c343" d="M3 13.5A2.5 2.5 0 0 1 5.5 11h21a2.5 2.5 0 0 1 2.5 2.5V25a2.5 2.5 0 0 1-2.5 2.5h-21A2.5 2.5 0 0 1 3 25z"/>'
                     '<path fill="#fff" opacity=".25" d="M3 13.5A2.5 2.5 0 0 1 5.5 11h21a2.5 2.5 0 0 1 2.5 2.5V15H3z"/></svg>')
        icones.append(("f_pasta", svg_para_png(pasta_svg, pasta, "f_pasta", 64)))
        for caminho in sorted(glob.glob(os.path.join(origem, "*.png"))):
            nome = os.path.splitext(os.path.basename(caminho))[0]
            if nome in ("folder", "_blank"):
                continue
            icones.append(("f_" + nome.lstrip("_"), open(caminho, "rb").read()))

    linhas = ["// Gerado por tools/gerar_icones.py. Nao edite a mao.", "#pragma once", "#include <stddef.h>", ""]
    for nome, dados in icones:
        corpo = ",".join(str(b) for b in dados)
        linhas.append(f"static const unsigned char icone_{nome}[] = {{{corpo}}};")
    linhas += ["", "struct IconeEmbutido { const char* nome; const unsigned char* png; int tamanho; };",
               "static const IconeEmbutido iconesEmbutidos[] = {"]
    linhas += [f'    {{ "{nome}", icone_{nome}, {len(dados)} }},' for nome, dados in icones]
    linhas += ["};", "#define TOTAL_ICONES_EMBUTIDOS ((int)(sizeof(iconesEmbutidos) / sizeof(iconesEmbutidos[0])))", ""]
    destino = os.path.join(RAIZ, "icones_embutidos.h")
    open(destino, "w", encoding="ascii", newline="\n").write("\n".join(linhas))
    print(len(icones), "icones,", sum(len(d) for _, d in icones) // 1024, "KB de PNG,", os.path.getsize(destino) // 1024, "KB no header")


main()
