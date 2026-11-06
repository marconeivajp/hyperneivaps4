"""Compila o TDLib para o alvo do PS4, com a lista de fontes dos CMakeLists.

Uso: python compilar.py [-j N] [--parar-em N] parte [parte ...]
Retoma de onde parou: só recompila o que não tem .o mais novo que o fonte.
"""
import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(AQUI, "src")
OBJ = os.path.join(AQUI, "obj")
LIBS = os.path.join(AQUI, "lib")
CC = r"C:\Program Files\LLVM\bin\clang.exe"
CXX = r"C:\Program Files\LLVM\bin\clang++.exe"
AR = r"C:\Program Files\LLVM\bin\llvm-ar.exe"

BASE = ["--target=x86_64-pc-freebsd12-elf", "-fPIC", "-O1", "-march=btver2", "-funwind-tables", "-ffunction-sections", "-fdata-sections", "-w",
        "-DNDEBUG", "-D_GNU_SOURCE", "-D_DEFAULT_SOURCE", "-DTD_ENABLE_LTO=0", "-DTDJSON_STATIC_DEFINE",
        "-isystem", r"C:\OpenOrbis\include\c++\v1", "-isystem", r"C:\OpenOrbis\include", "-I", os.path.join(AQUI, "deps", "include"), "-I", os.path.join(AQUI, "compat")]
INC = [os.path.join(SRC, d) for d in ("", "tdutils", "tdactor", "tdnet", "tddb", "tde2e", "sqlite", "td/generate/auto", "tdutils/generate")]
SQLITE = ["-DOMIT_MEMLOCK", "-DSQLITE_DEFAULT_MEMSTATUS=0", "-DSQLITE_DEFAULT_RECURSIVE_TRIGGERS=1", "-DSQLITE_DEFAULT_SYNCHRONOUS=1", "-DSQLITE_DISABLE_LFS",
          "-DSQLITE_ENABLE_FTS5", "-DSQLITE_HAS_CODEC", "-DSQLITE_OMIT_DECLTYPE", "-DSQLITE_OMIT_DEPRECATED", "-DSQLITE_OMIT_DESERIALIZE",
          "-DSQLITE_OMIT_LOAD_EXTENSION", "-DSQLITE_OMIT_PROGRESS_CALLBACK", "-DSQLITE_TEMP_STORE=2", "-DHAVE_USLEEP"]


def lista(cmake, *variaveis):
    t = open(os.path.join(SRC, cmake), encoding="utf-8").read()
    saida = []
    for v in variaveis:
        m = re.search(r"^\s*set\(" + v + r"\b(.*?)\)", t, re.S | re.M)
        for item in m.group(1).split():
            if item.endswith((".cpp", ".c")):
                saida.append(item)
    return saida


def partes():
    pasta = lambda d, l: [os.path.join(d, x) for x in l]
    principal = lista("CMakeLists.txt", "TD_MTPROTO_SOURCE", "TDLIB_SOURCE_PART1", "TDLIB_SOURCE_PART2")
    principal = [x for x in principal if not x.startswith("$")]
    auto = [f"td/generate/auto/td/telegram/{n}_{i}.cpp" for n in ("td_api", "telegram_api") for i in range(10)]
    auto += ["td/generate/auto/td/mtproto/mtproto_api.cpp", "td/generate/auto/td/telegram/secret_api.cpp", "td/generate/auto/td/telegram/e2e_api.cpp"]
    json = [f"td/generate/auto/td/telegram/td_api_json_{i}.cpp" for i in range(10)]
    return {
        "tdutils": pasta("tdutils", lista("tdutils/CMakeLists.txt", "TDUTILS_SOURCE")) + ["tdutils/generate/auto/mime_type_to_extension.cpp", "tdutils/td/utils/MimeType.cpp", "tdutils/generate/auto/extension_to_mime_type.cpp"],
        "tdactor": pasta("tdactor", lista("tdactor/CMakeLists.txt", "TDACTOR_SOURCE")),
        "tdnet": pasta("tdnet", lista("tdnet/CMakeLists.txt", "TDNET_SOURCE")),
        "tddb": pasta("tddb", lista("tddb/CMakeLists.txt", "TDDB_SOURCE")),
        "tdsqlite": pasta("sqlite", lista("sqlite/CMakeLists.txt", "SQLITE_SOURCE")),
        "tde2e": pasta("tde2e", lista("tde2e/CMakeLists.txt", "TDE2E_SOURCE")),
        "tdapi": auto,
        "tdjsonapi": json + ["td/telegram/GitCommitHash.cpp", "td/telegram/Client.cpp", "td/telegram/ClientJson.cpp", "td/telegram/td_json_client.cpp", "td/telegram/td_log.cpp"],
        "tdcore": [x for x in principal if x not in ("td/telegram/Client.cpp", "td/telegram/ClientJson.cpp")],
    }


def compilar(parte, fonte):
    caminho = os.path.join(SRC, fonte)
    o = os.path.join(OBJ, parte, fonte.replace("/", "_").replace("\\", "_") + ".o")
    if os.path.exists(o) and os.path.getmtime(o) >= os.path.getmtime(caminho):
        return o, 0, "", 0.0
    os.makedirs(os.path.dirname(o), exist_ok=True)
    c = fonte.endswith(".c")
    cmd = [CC if c else CXX] + BASE + [] if c else [CXX] + BASE + ["-std=c++17"]
    if parte == "tdsqlite":
        cmd += SQLITE
    for i in INC:
        cmd += ["-I", i]
    cmd += ["-c", caminho, "-o", o]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return o, r.returncode, r.stderr, time.time() - t


def main():
    a = argparse.ArgumentParser()
    a.add_argument("-j", type=int, default=3)
    a.add_argument("--parar-em", type=int, default=25)
    a.add_argument("nomes", nargs="+")
    x = a.parse_args()
    todas = partes()
    os.makedirs(LIBS, exist_ok=True)
    nomes = list(todas) if x.nomes == ["todas"] else x.nomes
    for parte in nomes:
        fontes = todas[parte]
        print(f"== {parte}: {len(fontes)} fontes", flush=True)
        objs, erros, feitos = [], [], 0
        with concurrent.futures.ThreadPoolExecutor(x.j) as ex:
            futuros = {ex.submit(compilar, parte, f): f for f in fontes}
            for fut in concurrent.futures.as_completed(futuros):
                o, rc, err, dur = fut.result()
                feitos += 1
                if rc:
                    erros.append((futuros[fut], err))
                    print(f"ERRO {futuros[fut]}", flush=True)
                    for l in [l for l in err.splitlines() if " error:" in l][:3]:
                        print("   ", l[-170:], flush=True)
                    if len(erros) >= x.parar_em:
                        for f in futuros:
                            f.cancel()
                        break
                else:
                    objs.append(o)
                    if dur > 60 or feitos % 25 == 0:
                        print(f"  {feitos}/{len(fontes)} {futuros[fut]} {dur:.0f}s", flush=True)
        if erros:
            open(os.path.join(AQUI, f"erros_{parte}.txt"), "w", encoding="utf-8").write("\n\n".join(f"### {f}\n{e}" for f, e in erros))
            print(f"{parte}: {len(erros)} erros", flush=True)
            sys.exit(1)
        lib = os.path.join(LIBS, f"lib{parte}.a")
        if os.path.exists(lib):
            os.remove(lib)
        rsp = os.path.join(LIBS, parte + ".rsp")
        open(rsp, "w").write("\n".join('"' + o.replace("\\", "/") + '"' for o in objs))
        subprocess.run([AR, "qc", lib, "@" + rsp], check=True)
        print(f"{parte}: OK {os.path.getsize(lib) >> 20} MB", flush=True)


main()
