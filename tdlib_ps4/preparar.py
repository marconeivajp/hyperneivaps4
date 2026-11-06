"""Prepara o TDLib para compilar para PS4 sem rodar nenhum gerador baixado no PC."""
import os
import re
import shutil
import subprocess
import tarfile
import urllib.request

AQUI = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(AQUI, "src")
DEP = os.path.join(AQUI, "deps")
SDK = r"C:\OpenOrbis\include\td\generate\auto"

# Arquivos gerados do SDK, que batem com o commit 8ff05a0e7e.
shutil.copytree(SDK, os.path.join(SRC, "td", "generate", "auto"), dirs_exist_ok=True)

# Mesma lógica de tdutils/generate/generate_mime_types_gperf.cpp.
ger = os.path.join(SRC, "tdutils", "generate")
os.makedirs(os.path.join(ger, "auto"), exist_ok=True)
preferidas = {"image/jpeg": "jpg", "audio/mpeg": "mp3", "audio/midi": "midi", "text/x-pascal": "pas", "text/x-asm": "asm", "video/quicktime": "mov"}
fonte = open(os.path.join(ger, "generate_mime_types_gperf.cpp"), encoding="utf-8").read()
achadas = dict(re.findall(r'\{"([a-z]+/[a-z0-9.+-]+)", "([a-z0-9]+)"\}', fonte))
assert achadas == preferidas, achadas
mime_ext, ext_mime = {}, {}
privado = lambda m: "/x-" in m
for linha in open(os.path.join(ger, "mime_types.txt"), encoding="utf-8"):
    linha = linha.rstrip("\r\n")
    tipo, _, resto = linha.partition("\t")
    resto = resto.lstrip(" \t")
    if not tipo or not resto:
        continue
    exts = resto.split(" ")
    i = exts.index(preferidas[tipo]) if tipo in preferidas else 0
    mime_ext.setdefault(tipo, exts[i])
    for e in exts:
        if e not in ext_mime:
            ext_mime[e] = tipo
        elif privado(ext_mime[e]) and not privado(tipo):
            ext_mime[e] = tipo


def gperf(nome, de, para, mapa):
    t = ["%struct-type", "%ignore-case", "%language=ANSI-C", "%readonly-tables", "%includes", "%enum",
         f"%define slot-name {de}", "%define initializer-suffix ,nullptr", f"%define slot-name {de}",
         f"%define hash-function-name {de}_hash", f"%define lookup-function-name search_{de}",
         f"struct {de}_and_{para} {{", f"  const char *{de};", f"  const char *{para};", "}", "%%"]
    t += [f'"{k}", "{mapa[k]}"' for k in sorted(mapa)]
    t += ["%%", f"const char *{de}_to_{para}(const char *{de}, size_t {de}_len) {{",
          f"  const auto &result = search_{de}({de}, {de}_len);", "  if (result == nullptr) {", "    return nullptr;", "  }", "",
          f"  return result->{para};", "}", ""]
    c = os.path.join(ger, "auto", nome)
    open(c + ".gperf", "w", newline="\n").write("\n".join(t))
    subprocess.run([r"C:\msys64\usr\bin\gperf.exe", "-m100", f"--output-file=auto/{nome}.cpp", f"auto/{nome}.gperf"], cwd=ger, check=True)


gperf("mime_type_to_extension", "mime_type", "extension", mime_ext)
gperf("extension_to_mime_type", "extension", "mime_type", ext_mime)

cfg = open(os.path.join(SRC, "tdutils", "td", "utils", "config.h.in")).read()
valores = {"TD_HAVE_OPENSSL": 1, "TD_HAVE_ZLIB": 1, "TD_HAVE_CRC32C": 0, "TD_HAVE_COROUTINES": 0, "TD_HAVE_ABSL": 0, "TD_FD_DEBUG": 0}
cfg = re.sub(r"#cmakedefine01 (\w+)", lambda m: f"#define {m.group(1)} {valores[m.group(1)]}", cfg)
open(os.path.join(SRC, "tdutils", "td", "utils", "config.h"), "w", newline="\n").write(cfg)

# Cabeçalhos das bibliotecas que já vêm compiladas no SDK: OpenSSL 1.1.1w e zlib 1.3.1.
os.makedirs(DEP, exist_ok=True)
for url, arquivo in (("https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1w/openssl-1.1.1w.tar.gz", "openssl.tar.gz"),
                     ("https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz", "zlib.tar.gz")):
    c = os.path.join(DEP, arquivo)
    if not os.path.exists(c):
        urllib.request.urlretrieve(url, c)
inc = os.path.join(DEP, "include")
os.makedirs(os.path.join(inc, "openssl"), exist_ok=True)
with tarfile.open(os.path.join(DEP, "openssl.tar.gz")) as t:
    for m in t.getmembers():
        if m.isfile() and "/include/openssl/" in m.name and m.name.endswith(".h"):
            open(os.path.join(inc, "openssl", os.path.basename(m.name)), "wb").write(t.extractfile(m).read())
with tarfile.open(os.path.join(DEP, "zlib.tar.gz")) as t:
    for n in ("zlib-1.3.1/zlib.h", "zlib-1.3.1/zconf.h"):
        open(os.path.join(inc, os.path.basename(n)), "wb").write(t.extractfile(n).read())

open(os.path.join(inc, "openssl", "opensslconf.h"), "w", newline="\n").write("""#include <openssl/opensslv.h>
#ifdef  __cplusplus
extern "C" {
#endif
#ifndef OPENSSL_NO_MD2
# define OPENSSL_NO_MD2
#endif
#ifndef OPENSSL_NO_RC5
# define OPENSSL_NO_RC5
#endif
#ifndef OPENSSL_THREADS
# define OPENSSL_THREADS
#endif
#ifndef OPENSSL_RAND_SEED_OS
# define OPENSSL_RAND_SEED_OS
#endif
#ifndef DECLARE_DEPRECATED
# define DECLARE_DEPRECATED(f)   f;
#endif
#ifndef OPENSSL_FILE
# define OPENSSL_FILE __FILE__
# define OPENSSL_LINE __LINE__
#endif
#ifndef OPENSSL_MIN_API
# define OPENSSL_MIN_API 0
#endif
#if !defined(OPENSSL_API_COMPAT) || OPENSSL_API_COMPAT < OPENSSL_MIN_API
# undef OPENSSL_API_COMPAT
# define OPENSSL_API_COMPAT OPENSSL_MIN_API
#endif
#if OPENSSL_API_COMPAT < 0x10100000L
# define DEPRECATEDIN_1_1_0(f)   DECLARE_DEPRECATED(f)
#else
# define DEPRECATEDIN_1_1_0(f)
#endif
#if OPENSSL_API_COMPAT < 0x10000000L
# define DEPRECATEDIN_1_0_0(f)   DECLARE_DEPRECATED(f)
#else
# define DEPRECATEDIN_1_0_0(f)
#endif
#if OPENSSL_API_COMPAT < 0x00908000L
# define DEPRECATEDIN_0_9_8(f)   DECLARE_DEPRECATED(f)
#else
# define DEPRECATEDIN_0_9_8(f)
#endif
#define OPENSSL_UNISTD <unistd.h>
#undef OPENSSL_EXPORT_VAR_AS_FUNCTION
#if !defined(OPENSSL_SYS_UEFI)
# undef BN_LLONG
# define SIXTY_FOUR_BIT_LONG
# undef SIXTY_FOUR_BIT
# undef THIRTY_TWO_BIT
#endif
#define RC4_INT unsigned int
#ifdef  __cplusplus
}
#endif
""")
print("preparado: mime", len(mime_ext), "extensoes", len(ext_mime))
