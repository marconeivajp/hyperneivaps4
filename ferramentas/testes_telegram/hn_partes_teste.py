"""Testa no PC a procura de partes, o título do PKG e a leitura real de RAR em volumes."""
import hashlib
import os
import shutil
import subprocess
import zipfile

A = os.path.dirname(os.path.abspath(__file__))
C = r"C:\Users\Marco\source\repos\teste3\HyperNeiva-Telegram"
W = os.path.join(A, "partes_teste")
shutil.rmtree(W, ignore_errors=True)
os.makedirs(W)
env = {**os.environ, "PATH": r"C:\msys64\mingw64\bin;" + os.environ["PATH"]}
RAR = r"C:\Program Files\WinRAR\Rar.exe"
Z7 = r"C:\Program Files\7-Zip\7z.exe"

d = open(os.path.join(C, "explorar_utilitarios.cpp"), encoding="utf-8", errors="surrogateescape").read().replace("\r\n", "\n")
s = d[d.index("// HN_PARTES_INICIO"):d.index("// HN_PARTES_FIM")]
c = r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <io.h>
#define fseeko fseeko64
static void hnAvisarCopia(float p) {}
static void sceKernelMkdir(const char* c, int m) { mkdir(c); }
void deletarPastaRecursivamente(const char* c);
#define ftello ftello64
''' + s + r'''
extern "C" { void* hnrar_abrir_lista(const char*, int, int); int hnrar_proximo(void*, char*, int, int*); long hnrar_ler(void*, void*, long); void hnrar_fechar(void*); const char* hnrar_erro(void*); int hnrar_progresso(void*); }
void deletarPastaRecursivamente(const char* c) { char x[600]; snprintf(x, sizeof(x), "rmdir /s /q \"%s\"", c); for (char* q = x; *q; q++) if (*q == '/') *q = '\\'; system(x); }
int main(int argc, char** argv) {
    static char lista[HN_MAX_PARTES * HN_PASSO];
    int total, faltando;
    if (!strcmp(argv[1], "mover")) { bool r = hnCopiarOuMover(argv[2], argv[3], argv[4][0] == '1'); printf("MOVER %d\n", r); return 0; }
    if (!strcmp(argv[1], "titulo")) { char t[128]; hnTituloPkg(argv[2], t, sizeof(t)); printf("TITULO [%s] update=%d falta=%lld\n", t, hnPkgEhAtualizacao(argv[2]), hnBytesFaltandoPkg(argv[2])); return 0; }
    int tipo = hnListarPartes(argv[2], lista, &total, &faltando);
    printf("tipo=%d total=%d faltando=%d primeira=%s\n", tipo, total, faltando, total ? strrchr(lista, '/') + 1 : "-");
    if (!strcmp(argv[1], "rar")) {
        void* h = hnrar_abrir_lista(lista, total, HN_PASSO);
        if (!h) { printf("nao abriu\n"); return 1; }
        char nome[512]; int pasta; static char b[262144];
        int r;
        while ((r = hnrar_proximo(h, nome, sizeof(nome), &pasta)) == 1) {
            if (pasta) continue;
            unsigned long long soma = 0, n2 = 0; long n;
            while ((n = hnrar_ler(h, b, sizeof(b))) > 0) { for (long i = 0; i < n; i++) soma = soma * 131 + (unsigned char)b[i]; n2 += n; }
            printf("%s %llu %llx leitura=%ld\n", nome, n2, soma, n);
        }
        printf("fim=%d progresso=%d erro=[%s]\n", r, hnrar_progresso(h), hnrar_erro(h));
        hnrar_fechar(h);
    }
    return 0;
}
'''
open(os.path.join(W, "t.cpp"), "w", encoding="utf-8", newline="\n").write(c)
# libarchive fica em C: compila o mesmo arquivo do app para rodar no Windows.
open(os.path.join(W, "r.c"), "w", encoding="utf-8", newline="\n").write('#include <sys/types.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <errno.h>\n#include <wchar.h>\n#include <time.h>\n#include <sys/stat.h>\n#include <limits.h>\ntypedef int uid_t;\ntypedef int gid_t;\n#define F_GETFD 1\n#define F_SETFD 2\n#define FD_CLOEXEC 1\nstatic int fcntl(int a, int b, ...) { return 0; }\nstatic struct tm* localtime_r(const time_t* t, struct tm* r) { *r = *localtime(t); return r; }\n#define __CYGWIN__ 1\n#include "hn_rar_impl.h"\n')
inc = os.path.join(C, "hn_rar")
r1 = subprocess.run([r"C:\msys64\mingw64\bin\gcc.exe", "-w", "-O1", "-DHAVE_CONFIG_H", "-I", inc, "-c", os.path.join(W, "r.c"), "-o", os.path.join(W, "r.o")], capture_output=True, text=True, env=env)
print("rar c:", r1.returncode, "\n".join(l[-120:] for l in r1.stderr.splitlines() if " error:" in l)[:1400])
r2 = subprocess.run([r"C:\msys64\mingw64\bin\g++.exe", "-w", "-std=c++17", "-c", os.path.join(W, "t.cpp"), "-o", os.path.join(W, "t.o")], capture_output=True, text=True, env=env)
print("cpp:", r2.returncode, r2.stderr[-900:])
r3 = subprocess.run([r"C:\msys64\mingw64\bin\g++.exe", os.path.join(W, "t.o"), os.path.join(W, "r.o"), "-o", os.path.join(W, "t.exe")], capture_output=True, text=True, env=env)
print("link:", r3.returncode, r3.stderr[-700:])


def rodar(*a):
    x = subprocess.run([os.path.join(W, "t.exe"), *a], capture_output=True, text=True, env=env)
    print(" ", " ".join(os.path.basename(i) for i in a), "=>", x.stdout.strip().replace("\n", " | ")[:300], x.stderr[-200:])


dados = os.path.join(W, "jogo.bin")
with open(dados, "wb") as f:
    f.write(os.urandom(6 << 20))

soma = 0
for b in open(dados, "rb").read():
    soma = (soma * 131 + b) & 0xFFFFFFFFFFFFFFFF
print("esperado", os.path.getsize(dados), hex(soma)[2:])

for pasta, opcoes in (("r5", ["-ma5", "-v2m", "-m1"]), ("r4", ["-ma4", "-v2m", "-m1"]), ("r4antigo", ["-ma4", "-v2m", "-m1", "-vn"])):
    p = os.path.join(W, pasta)
    os.makedirs(p)
    subprocess.run([RAR, "a", "-ep", "-idq", *opcoes, os.path.join(p, "x.rar"), dados], check=True)
    nomes = sorted(os.listdir(p))
    print(pasta, nomes)
    rodar("rar", os.path.join(p, nomes[0]).replace("\\", "/"))
    # Abrir por uma parte do meio precisa voltar à primeira.
    rodar("partes", os.path.join(p, nomes[-1]).replace("\\", "/"))

p = os.path.join(W, "r5")
nomes = sorted(os.listdir(p))
os.remove(os.path.join(p, nomes[1]))
print("sem a parte 2:")
rodar("partes", os.path.join(p, nomes[0]).replace("\\", "/"))

p = os.path.join(W, "sete")
os.makedirs(p)
subprocess.run([Z7, "a", "-bd", "-v2m", "-mx=1", os.path.join(p, "x.7z"), dados], check=True, capture_output=True)
print(sorted(os.listdir(p)))
rodar("partes", os.path.join(p, "x.7z.002").replace("\\", "/"))
os.remove(os.path.join(p, "x.7z.002"))
rodar("partes", os.path.join(p, "x.7z.001").replace("\\", "/"))

p = os.path.join(W, "comum")
os.makedirs(p)
with zipfile.ZipFile(os.path.join(p, "a.zip"), "w") as z:
    z.writestr("a.txt", "oi")
shutil.copy(os.path.join(p, "a.zip"), os.path.join(p, "b.rar"))
open(os.path.join(p, "c.z01"), "wb").close()
shutil.copy(os.path.join(p, "a.zip"), os.path.join(p, "c.zip"))
for n in ("a.zip", "b.rar", "c.zip", "c.z01"):
    rodar("partes", os.path.join(p, n).replace("\\", "/"))

for k in (os.path.join(os.environ["TEMP"], "opencode", "pkg_HyperNeiva_v2.47", "HyperNeiva_v2.47.pkg"),
          os.path.join(os.environ["TEMP"], "opencode", "pkg_HyperNeivaTelegram_stream_barra_posicao_20261106", "HyperNeivaTelegram_stream_barra_posicao_20261106.pkg")):
    rodar("titulo", k.replace("\\", "/"))
rodar("titulo", os.path.join(p, "a.zip").replace("\\", "/"))
cortado = os.path.join(W, "cortado.pkg")
with open(k, "rb") as a, open(cortado, "wb") as b:
    b.write(a.read(50 << 20))
rodar("titulo", cortado.replace("\\", "/"))


def arvore(p):
    return sorted(os.path.relpath(os.path.join(a, f), p).replace("\\", "/") + ":" + str(os.path.getsize(os.path.join(a, f))) for a, _, fs in os.walk(p) for f in fs)


print("--- copiar e mover")
b = os.path.join(W, "mover")
os.makedirs(os.path.join(b, "pasta", "sub", "fundo"))
os.makedirs(os.path.join(b, "alvo"))
for n, t in (("pasta/a.bin", 3000000), ("pasta/sub/b.txt", 12), ("pasta/sub/fundo/c.bin", 2500000), ("solto.bin", 1000)):
    with open(os.path.join(b, n), "wb") as f:
        f.write(os.urandom(t))
antes = arvore(os.path.join(b, "pasta"))
barra = b.replace("\\", "/")
rodar("mover", barra + "/pasta", barra + "/alvo/copia", "0")
print("  pasta copiada igual:", arvore(os.path.join(b, "alvo", "copia")) == antes, "origem preservada:", arvore(os.path.join(b, "pasta")) == antes)
rodar("mover", barra + "/solto.bin", barra + "/solto.bin", "0")
print("  mesma pasta nao zerou:", os.path.getsize(os.path.join(b, "solto.bin")) == 1000)
rodar("mover", barra + "/pasta", barra + "/pasta/sub/dentro", "1")
print("  dentro de si recusado:", arvore(os.path.join(b, "pasta")) == antes)
rodar("mover", barra + "/solto.bin", barra + "/alvo/movido.bin", "1")
print("  recortar:", not os.path.exists(os.path.join(b, "solto.bin")), os.path.getsize(os.path.join(b, "alvo", "movido.bin")) == 1000)
rodar("mover", barra + "/naoexiste.bin", barra + "/alvo/x.bin", "1")
print("  origem inexistente nao cria:", not os.path.exists(os.path.join(b, "alvo", "x.bin")))
