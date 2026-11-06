"""Ajustes do TDLib e do SDK para o alvo do PS4. Pode rodar de novo sem duplicar nada."""
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(AQUI, "src")
COMPAT = os.path.join(AQUI, "compat")
os.makedirs(os.path.join(COMPAT, "sys"), exist_ok=True)


def trocar(rel, antigo, novo):
    c = rel if os.path.isabs(rel) else os.path.join(SRC, rel)
    d = open(c, "rb").read().decode("utf-8", "surrogateescape")
    nl = "\r\n" if "\r\n" in d else "\n"
    a, b = antigo.replace("\n", nl), novo.replace("\n", nl)
    if b in d:
        return
    assert a in d, (rel, antigo[:60])
    open(c, "wb").write(d.replace(a, b, 1).encode("utf-8", "surrogateescape"))


def gravar(rel, texto):
    open(os.path.join(COMPAT, rel), "w", newline="\n").write(texto)


# O PS4 não tem o kqueue do FreeBSD no SDK: o build antigo do TDLib usava poll.
trocar("tdutils/td/utils/port/config.h", "#elif TD_FREEBSD || TD_OPENBSD || TD_NETBSD\n  #define TD_POLL_KQUEUE 1", "#elif TD_FREEBSD || TD_OPENBSD || TD_NETBSD\n  #define TD_POLL_POLL 1")

# signal.h do SDK aponta sa_sigaction para um campo que não existe.
trocar("tdutils/td/utils/port/signals.cpp", '#include "td/utils/port/signals.h"\n', '#include "td/utils/port/signals.h"\n\n#include <signal.h>\n#undef sa_sigaction\n#define sa_sigaction __sa_handler.__sa_sigaction\n')

trocar(os.path.join(AQUI, "deps", "include", "openssl", "opensslconf.h"), "#if OPENSSL_API_COMPAT < 0x10100000L\n", "#define DEPRECATEDIN_1_2_0(f)   DECLARE_DEPRECATED(f)\n#if OPENSSL_API_COMPAT < 0x10100000L\n")

# Afinidade de thread não importa aqui, e o pthread.h do SDK declara um tipo diferente.
trocar("tdutils/td/utils/port/detail/ThreadPthread.cpp", "namespace td {\nnamespace detail {\n", "#define pthread_setaffinity_np(a, b, c) 0\n#define pthread_getaffinity_np(a, b, c) 0\n\nnamespace td {\nnamespace detail {\n")

# O realpath do libc do SDK depende de /proc, que o PS4 não tem, e falha com ENOSYS.
trocar("tdutils/td/utils/port/path.cpp", "Result<string> realpath(CSlice slice, bool ignore_access_denied) {\n  char full_path[PATH_MAX + 1];", """static char *ps4_realpath(const char *entrada, char *saida) {
  string caminho = entrada;
  if (caminho.empty()) {
    errno = ENOENT;
    return nullptr;
  }
  if (caminho[0] != '/') {
    char atual[PATH_MAX + 1];
    if (getcwd(atual, sizeof(atual)) == nullptr) {
      return nullptr;
    }
    caminho = string(atual) + '/' + caminho;
  }
  struct ::stat info;
  if (::stat(caminho.c_str(), &info) != 0) {
    return nullptr;
  }
  vector<string> partes;
  size_t inicio = 0;
  while (inicio <= caminho.size()) {
    size_t fim = caminho.find('/', inicio);
    if (fim == string::npos) {
      fim = caminho.size();
    }
    string parte = caminho.substr(inicio, fim - inicio);
    if (parte == "..") {
      if (!partes.empty()) {
        partes.pop_back();
      }
    } else if (!parte.empty() && parte != ".") {
      partes.push_back(parte);
    }
    inicio = fim + 1;
  }
  string resultado;
  for (auto &parte : partes) {
    resultado += '/';
    resultado += parte;
  }
  if (resultado.empty()) {
    resultado = "/";
  }
  if (resultado.size() > PATH_MAX) {
    errno = ENAMETOOLONG;
    return nullptr;
  }
  memcpy(saida, resultado.c_str(), resultado.size() + 1);
  return saida;
}

Result<string> realpath(CSlice slice, bool ignore_access_denied) {
  char full_path[PATH_MAX + 1];""")
trocar("tdutils/td/utils/port/path.cpp", "return ::realpath(slice.c_str(), full_path);", "return ps4_realpath(slice.c_str(), full_path);")
trocar("tdutils/td/utils/port/path.cpp", '#include "td/utils/port/path.h"\n', '#include "td/utils/port/path.h"\n\n#include <cerrno>\n#include <cstring>\n#include <sys/stat.h>\n#include <unistd.h>\n')

for n in ("mime_type_to_extension", "extension_to_mime_type"):
    c = os.path.join(SRC, "tdutils", "generate", "auto", n + ".cpp")
    d = open(c, encoding="utf-8").read().replace("register ", "")
    open(c, "w", encoding="utf-8", newline="\n").write(d)

gravar("xlocale.h", """#pragma once
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#define strtoll_l(s, e, b, l) strtoll(s, e, b)
#define strtoull_l(s, e, b, l) strtoull(s, e, b)
#define sscanf_l(s, l, ...) sscanf(s, __VA_ARGS__)
#define snprintf_l(s, n, l, ...) snprintf(s, n, __VA_ARGS__)
#define asprintf_l(r, l, ...) asprintf(r, __VA_ARGS__)
""")
gravar("pthread_np.h", "#pragma once\n#include <pthread.h>\n")
gravar(os.path.join("sys", "cpuset.h"), "#pragma once\n#include <sched.h>\ntypedef cpu_set_t cpuset_t;\n")
# fcntl.h do SDK mistura F_SETLK do FreeBSD com struct flock e F_*LCK do Linux: o kernel responde EINVAL.
gravar("ps4_flock.h", """#pragma once
#include <fcntl.h>
#include <sys/types.h>
#undef F_RDLCK
#undef F_UNLCK
#undef F_WRLCK
#define F_RDLCK 1
#define F_UNLCK 2
#define F_WRLCK 3
struct ps4_flock {
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
  short l_type;
  short l_whence;
  int l_sysid;
};
#define flock ps4_flock
""")
trocar("tdutils/td/utils/port/FileFd.cpp", '#include "td/utils/port/FileFd.h"\n', '#include "td/utils/port/FileFd.h"\n\n#include "ps4_flock.h"\n')
trocar("sqlite/sqlite/sqlite3.c", "#include <fcntl.h>\n", '#include <fcntl.h>\n#include "ps4_flock.h"\n')

# Só este app abre a pasta do Telegram: se o sistema de arquivos não tiver trava, a trava local basta.
trocar("tdutils/td/utils/port/FileFd.cpp", "if (fcntl(native_fd, F_SETLK, &lock) == -1) {\n      if (errno == EAGAIN) {", "if (fcntl(native_fd, F_SETLK, &lock) == -1) {\n      if (errno == EINVAL || errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOLCK) {\n        LOG(WARNING) << \"Trava de arquivo indisponivel, errno \" << errno;\n        break;\n      }\n      if (errno == EAGAIN) {")

# MSG_NOSIGNAL do SDK é o valor do Linux; no FreeBSD 0x4000 é outra coisa e o SIGPIPE fecharia o app.
trocar("tdutils/td/utils/port/SocketFd.cpp", '#include "td/utils/port/SocketFd.h"\n', '#include "td/utils/port/SocketFd.h"\n\n#include <sys/socket.h>\n#undef MSG_NOSIGNAL\n#define MSG_NOSIGNAL 0x20000\n')

# sockaddr_in6 do SDK não tem sin6_len: sin6_family cai no byte do tamanho e sa_family fica 0.
c = os.path.join(SRC, "tdutils", "td", "utils", "port", "IPAddress.cpp")
d = open(c, "rb").read().decode("utf-8", "surrogateescape")
d = d.replace("ipv6_addr_.sin6_family = AF_INET6;", "ipv6_addr_.sin6_family = sizeof(ipv6_addr_);\n  reinterpret_cast<sockaddr *>(&ipv6_addr_)->sa_family = AF_INET6;")
open(c, "wb").write(d.encode("utf-8", "surrogateescape"))

print("ajustes ok")

# Gerado pelo CMake no build normal. Com biblioteca estática as macros ficam vazias.
os.makedirs(os.path.join(COMPAT, "td", "telegram"), exist_ok=True)
gravar(os.path.join("td", "telegram", "tdjson_export.h"), "#pragma once\n#define TDJSON_EXPORT\n#define TDJSON_NO_EXPORT\n#define TDJSON_DEPRECATED\n#define TDJSON_DEPRECATED_EXPORT\n#define TDJSON_DEPRECATED_NO_EXPORT\n")
