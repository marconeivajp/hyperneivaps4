"""Compila a libtorrent para o PS4 com a lista de fontes do CMakeLists. Retoma de onde parou.

Uso: python compilar.py [-j N] [--parar-em N] [--so arquivo.cpp]
"""
import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
LT = os.path.join(AQUI, "src", "libtorrent-rasterbar-2.1.2")
BOOST = os.path.join(AQUI, "src", "boost-1.92.0")
TD = os.path.join(os.path.dirname(AQUI), "td_ps4")
COMPAT = os.path.join(AQUI, "compat")
OBJ = os.path.join(AQUI, "obj")
LIBS = os.path.join(AQUI, "lib")
CC = r"C:\Program Files\LLVM\bin\clang.exe"
CXX = r"C:\Program Files\LLVM\bin\clang++.exe"
AR = r"C:\Program Files\LLVM\bin\llvm-ar.exe"

DEF = """NDEBUG _GNU_SOURCE _DEFAULT_SOURCE _FILE_OFFSET_BITS=64 TORRENT_BUILDING_LIBRARY BOOST_EXCEPTION_DISABLE
BOOST_ASIO_HAS_STD_CHRONO BOOST_ASIO_NO_DEPRECATED BOOST_SYSTEM_USE_UTF8 BOOST_ALL_NO_LIB BOOST_JSON_HEADER_ONLY
BOOST_ASIO_DISABLE_KQUEUE BOOST_ASIO_DISABLE_EPOLL BOOST_ASIO_DISABLE_EVENTFD BOOST_ASIO_DISABLE_SERIAL_PORT
BOOST_ASIO_DISABLE_LOCAL_SOCKETS BOOST_ASIO_DISABLE_SIGNAL TORRENT_USE_LIBCRYPTO TORRENT_USE_OPENSSL
TORRENT_HAVE_MMAP=0 TORRENT_USE_I2P=0 TORRENT_USE_RTC=0""".split()
BASE = ["--target=x86_64-pc-freebsd12-elf", "-fPIC", "-O2", "-march=btver2", "-funwind-tables", "-ffunction-sections", "-fdata-sections", "-w",
        "-isystem", r"C:\OpenOrbis\include\c++\v1", "-isystem", r"C:\OpenOrbis\include", "-isystem", BOOST,
        "-isystem", os.path.join(TD, "deps", "include"), "-I", os.path.join(LT, "include"), "-I", os.path.join(LT, "deps", "try_signal"),
        "-I", COMPAT, "-I", os.path.join(TD, "compat")] + ["-D" + d for d in DEF]


def fontes():
    t = open(os.path.join(LT, "CMakeLists.txt"), encoding="utf-8").read()
    saida = []
    for nome, pasta in (("sources", "src"), ("kademlia_sources", "src/kademlia"), ("ed25519_sources", "src/ed25519")):
        m = re.search(r"^set\(" + nome + r"\b(.*?)\)", t, re.S | re.M)
        saida += [os.path.join(LT, pasta, x) for x in m.group(1).split() if x.endswith((".cpp", ".c"))]
    saida.append(os.path.join(LT, "src", "pe_crypto.cpp"))  # no CMake fica num bloco condicional
    return [f for f in saida if os.path.exists(f)] + [os.path.join(COMPAT, x) for x in os.listdir(COMPAT) if x.endswith(".cpp")]


def compilar(fonte):
    rel = os.path.relpath(fonte, AQUI).replace("\\", "_").replace("/", "_")
    o = os.path.join(OBJ, rel + ".o")
    if os.path.exists(o) and os.path.getmtime(o) >= os.path.getmtime(fonte):
        return o, 0, "", 0.0
    os.makedirs(OBJ, exist_ok=True)
    forcado = [] if os.path.basename(fonte) == "hn_ps4_net.cpp" else ["-include", os.path.join(COMPAT, "ps4_lt.h")]
    cmd = [CXX, "-std=c++17"] + BASE + forcado + ["-c", fonte, "-o", o]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return o, r.returncode, r.stderr, time.time() - t


def main():
    a = argparse.ArgumentParser()
    a.add_argument("-j", type=int, default=4)
    a.add_argument("--parar-em", type=int, default=6)
    a.add_argument("--so")
    x = a.parse_args()
    lista = fontes()
    if x.so:
        lista = [f for f in lista if os.path.basename(f) == x.so]
    print("fontes:", len(lista), flush=True)
    erros = 0
    feitos = 0
    with concurrent.futures.ThreadPoolExecutor(x.j) as p:
        tarefas = [p.submit(compilar, f) for f in lista]
        for t in concurrent.futures.as_completed(tarefas):
            o, codigo, err, seg = t.result()
            feitos += 1
            if codigo:
                erros += 1
                linhas = [l for l in err.splitlines() if "error" in l][:6]
                print("ERRO", os.path.basename(o), *linhas, sep="\n  ", flush=True)
                if erros >= x.parar_em:
                    for r in tarefas:
                        r.cancel()
                    break
            elif seg:
                print(f"{feitos}/{len(lista)} {os.path.basename(o)} {seg:.0f}s", flush=True)
    if erros:
        sys.exit(f"{erros} erros")
    if x.so:
        return
    os.makedirs(LIBS, exist_ok=True)
    alvo = os.path.join(LIBS, "libhntorrent.a")
    if os.path.exists(alvo):
        os.remove(alvo)
    rsp = os.path.join(LIBS, "hntorrent.rsp")
    open(rsp, "w").write("\n".join('"' + os.path.join(OBJ, f).replace("\\", "/") + '"' for f in sorted(os.listdir(OBJ)) if f.endswith(".o")))
    subprocess.check_call([AR, "rcs", alvo, "@" + rsp])
    print("biblioteca", os.path.getsize(alvo), flush=True)


main()
