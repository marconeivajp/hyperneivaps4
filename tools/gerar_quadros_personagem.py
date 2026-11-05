"""Recorta os quadros do personagem controlavel da folha e gera personagem_quadros.h.

Os quadros da folha nao seguem uma grade, entao as posicoes sao medidas pelos pixels.
Rode depois de trocar a folha:  python tools/gerar_quadros_personagem.py
"""
import os
import subprocess
import sys

BIN = r"C:\msys64\mingw64\bin"
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLHA = os.path.join(RAIZ, "assets", "images", "elementos_animados_sprite_sheet.png")
LARGURA, ALTURA = 1490, 4504

# nome: (y inicial, y final, primeiro quadro, ultimo quadro) dentro da faixa da folha
FAIXAS = {
    "parado": (100, 152, 0, 6),
    "andar": (160, 228, 2, 15),
    "atacar": (236, 300, 0, 12),
    "pular": (688, 786, 9, 21),
    "abaixar": (1142, 1192, 0, 2),
}
LARGURA_MAXIMA = {"andar": 75}   # dois quadros colados pelo cabelo


def carregar():
    r = subprocess.run([os.path.join(BIN, "ffmpeg.exe"), "-hide_banner", "-loglevel", "error", "-i", FOLHA,
                        "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], capture_output=True, check=True)
    return r.stdout


def main():
    px = carregar()
    assert len(px) == LARGURA * ALTURA * 3

    def cor(x, y):
        i = (y * LARGURA + x) * 3
        return px[i], px[i + 1], px[i + 2]

    # As duas cores mais comuns da folha: fundo azul escuro e bloco azul claro atras dos quadros.
    contagem = {}
    for i in range(0, len(px), 3 * 7):
        c = (px[i], px[i + 1], px[i + 2])
        contagem[c] = contagem.get(c, 0) + 1
    fundo, azul = sorted(contagem, key=contagem.get, reverse=True)[:2]
    print("fundo", fundo, "tile", azul)
    vazio = {fundo, azul}
    branco = (255, 255, 255)

    saida = {}
    for nome, (y0, y1, primeiro, ultimo) in FAIXAS.items():
        colunas = [any(cor(x, y) not in vazio and cor(x, y) != branco for y in range(y0, y1)) for x in range(LARGURA)]
        quadros, x = [], 0
        while x < LARGURA:
            if not colunas[x]:
                x += 1
                continue
            inicio = x
            while x < LARGURA and (colunas[x] or any(colunas[x:x + 2])):
                x += 1
            if x - inicio >= 12:
                quadros.append((inicio, x))
        limite = LARGURA_MAXIMA.get(nome)
        if limite:
            separados = []
            for a, b in quadros:
                if b - a <= limite:
                    separados.append((a, b))
                    continue
                meio = range(a + (b - a) // 3, b - (b - a) // 3)
                corte = min(meio, key=lambda c: sum(cor(c, y) not in vazio for y in range(y0, y1)))
                separados += [(a, corte), (corte, b)]
            quadros = separados
        print(nome, "achou", len(quadros), "quadros")

        lista = []
        for a, b in quadros[primeiro:ultimo + 1]:
            ys = [y for y in range(y0, y1) for xx in range(a, b) if cor(xx, y) not in vazio]
            # Fica so o bloco de linhas mais alto: os rotulos da folha ficam soltos embaixo dos pes.
            linhas_ok, blocos = sorted(set(ys)), []
            for y in linhas_ok:
                if blocos and y == blocos[-1][1]:
                    blocos[-1][1] = y + 1
                else:
                    blocos.append([y, y + 1])
            topo, base = max(blocos, key=lambda b: b[1] - b[0])
            # Centro do corpo pela armadura vermelha: cabelo e sabre mudariam o ponto a cada quadro.
            vermelhos = [xx for xx in range(a, b) for y in range(topo, base) if cor(xx, y)[0] > 150 and cor(xx, y)[1] < 80 and cor(xx, y)[2] < 90]
            centro = sorted(vermelhos)[len(vermelhos) // 2] if vermelhos else (a + b) // 2
            lista.append((a, topo, b - a, base - topo, centro - a))
        saida[nome] = lista

    linhas = ["// Gerado por tools/gerar_quadros_personagem.py. Nao edite a mao.", "#pragma once", "",
              "struct QuadroPersonagem { short x, y, largura, altura, centroX; };",
              f"#define PERSONAGEM_COR_FUNDO 0x{fundo[0]:02X}{fundo[1]:02X}{fundo[2]:02X}",
              f"#define PERSONAGEM_COR_TILE 0x{azul[0]:02X}{azul[1]:02X}{azul[2]:02X}", ""]
    for nome, lista in saida.items():
        corpo = ", ".join("{ %d, %d, %d, %d, %d }" % q for q in lista)
        linhas.append(f"static const QuadroPersonagem quadros_{nome}[] = {{ {corpo} }};")
    destino = os.path.join(RAIZ, "personagem_quadros.h")
    open(destino, "w", encoding="ascii", newline="\n").write("\n".join(linhas) + "\n")

    if len(sys.argv) > 1:
        pasta = sys.argv[1]
        os.makedirs(pasta, exist_ok=True)
        W = 1600
        linhas_img = []
        for nome, lista in saida.items():
            linhas_img.append(lista)
        H = 110 * len(linhas_img)
        tela = bytearray(bytes((40, 44, 52)) * (W * H))
        for i, lista in enumerate(linhas_img):
            cx = 50
            for (qx, qy, ql, qa, qc) in lista:
                for y in range(qa):
                    for x in range(ql):
                        c = cor(qx + x, qy + y)
                        if c in vazio:
                            continue
                        dx, dy = cx - qc + x, i * 110 + 100 - qa + y
                        if 0 <= dx < W and 0 <= dy < H:
                            o = (dy * W + dx) * 3
                            tela[o:o + 3] = bytes(c)
                for y in range(i * 110 + 100, i * 110 + 104):
                    o = (y * W + cx) * 3
                    tela[o:o + 3] = bytes((0, 255, 0))
                cx += 105
        caminho = os.path.join(pasta, "quadros.ppm")
        with open(caminho, "wb") as f:
            f.write(b"P6\n%d %d\n255\n" % (W, H))
            f.write(tela)
    print("gerado", destino)


main()
