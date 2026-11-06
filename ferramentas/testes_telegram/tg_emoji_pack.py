"""Junta os PNGs do Twemoji em um arquivo: total, índice ordenado e os dados."""
import os
import struct
import sys

P = r"C:\Users\Marco\source\repos\teste3\td_ps4\emoji_twemoji\assets\72x72"
S = sys.argv[1]
nomes = sorted(n[:-4] for n in os.listdir(P) if n.endswith(".png"))
nomes.sort(key=lambda n: n.encode())
indice, dados = [], bytearray()
for n in nomes:
    assert len(n) < 72, n
    b = open(os.path.join(P, n + ".png"), "rb").read()
    indice.append(struct.pack("<72sII", n.encode(), len(dados), len(b)))
    dados += b
with open(S, "wb") as f:
    f.write(struct.pack("<I", len(nomes)))
    f.write(b"".join(indice))
    f.write(dados)
print(len(nomes), "emojis", os.path.getsize(S) >> 10, "KB")
