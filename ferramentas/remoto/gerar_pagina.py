# Gera remoto_pagina.h a partir de pagina.html (página do controle remoto e do envio de mídia).
# Uso: python ferramentas/remoto/gerar_pagina.py   (depois recompile com -Modulos network)
import os

aqui = os.path.dirname(os.path.abspath(__file__))
raiz = os.path.dirname(os.path.dirname(aqui))
with open(os.path.join(aqui, "pagina.html"), "r", encoding="utf-8") as f:
    html = f.read()
if ')HNPAG"' in html:
    raise SystemExit("pagina.html contem o delimitador )HNPAG\"")

# O clang aceita literais longos, mas o MSVC não passa de 16 KB por pedaço: divide em blocos.
blocos = [html[i:i + 12000] for i in range(0, len(html), 12000)]
with open(os.path.join(raiz, "remoto_pagina.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write("// Gerado por ferramentas/remoto/gerar_pagina.py a partir de pagina.html. Não edite à mão.\n")
    f.write("#pragma once\n")
    f.write("static const char REMOTO_PAGINA[] =\n")
    for b in blocos:
        f.write('R"HNPAG(' + b + ')HNPAG"\n')
    f.write(";\n")
print("remoto_pagina.h:", len(html.encode("utf-8")), "bytes")
