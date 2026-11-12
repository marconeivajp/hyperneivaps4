// HyperNeivaPC: programa do PC que completa a página do Hyper Neiva (http://<ps4>:8000).
// - Mostra as unidades e as pastas do PC (o navegador sozinho não enxerga o disco).
// - Copia arquivos e pastas entre o PC e o PS4, direto, sem passar pelo navegador.
// - Manda a tela do PC para o PS4 (JPEG em sequência para /espelho/quadro).
// Escuta só em 127.0.0.1:8001 e só atende a página do Hyper Neiva (Origin numa rede local,
// porta 8000). Compilar: ferramentas/hyperneiva_pc/compilar.sh
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../../stb_image_write.h"
#include "../../remoto_qr.h"

#define PORTA 8001
#define VERSAO 4

static CRITICAL_SECTION trava;
struct Trava { Trava() { EnterCriticalSection(&trava); } ~Trava() { LeaveCriticalSection(&trava); } };

static uint64_t agoraMs() { return GetTickCount64(); }

// ---------------------------------------------------------------- texto
static std::wstring largo(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}
static std::string jsonTexto(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { r += '\\'; r += (char)c; }
        else if (c < 32) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); r += b; }
        else r += (char)c;
    }
    return r;
}
static int hex(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }
static std::string param(const std::string& q, const char* nome) {
    std::string chave = std::string(nome) + "=";
    size_t i = 0;
    while (i <= q.size()) {
        size_t fim = q.find('&', i); if (fim == std::string::npos) fim = q.size();
        if (q.compare(i, chave.size(), chave) == 0) {
            std::string r;
            for (size_t k = i + chave.size(); k < fim; k++) {
                if (q[k] == '%' && k + 2 < fim && hex(q[k + 1]) >= 0 && hex(q[k + 2]) >= 0) { r += (char)(hex(q[k + 1]) * 16 + hex(q[k + 2])); k += 2; }
                else if (q[k] == '+') r += ' ';
                else r += q[k];
            }
            return r;
        }
        i = fim + 1;
    }
    return "";
}
static std::string urlCodificar(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (isalnum(c) || strchr("-_.~/", c)) r += (char)c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); r += b; }
    }
    return r;
}
// Caminho do PC sempre com "/" na página; o Windows aceita as duas barras.
static std::string barras(std::string s) { for (auto& c : s) if (c == '\\') c = '/'; return s; }
static std::string cabValor(const std::string& cab, const char* nome) {
    size_t ln = strlen(nome);
    size_t i = 0;
    while ((i = cab.find('\n', i)) != std::string::npos) {
        i++;
        if (cab.size() - i > ln && _strnicmp(cab.c_str() + i, nome, ln) == 0 && cab[i + ln] == ':') {
            size_t a = i + ln + 1; while (a < cab.size() && (cab[a] == ' ' || cab[a] == '\t')) a++;
            size_t b = cab.find_first_of("\r\n", a);
            return cab.substr(a, b == std::string::npos ? std::string::npos : b - a);
        }
    }
    return "";
}

// ---------------------------------------------------------------- sockets
static bool enviarTudo(SOCKET s, const char* d, size_t n) {
    while (n > 0) { int r = send(s, d, (int)(n > 1 << 20 ? 1 << 20 : n), 0); if (r <= 0) return false; d += r; n -= r; }
    return true;
}
static void prazo(SOCKET s, int ms) {
    DWORD t = ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof(t));
}

struct Conexao {
    SOCKET s = INVALID_SOCKET;
    std::string buf;   // bytes recebidos e ainda não usados
    bool lerCabecalho(std::string& cab) {
        for (;;) {
            size_t f = buf.find("\r\n\r\n");
            if (f != std::string::npos) { cab = buf.substr(0, f + 4); buf.erase(0, f + 4); return true; }
            if (buf.size() > 65536) return false;
            char t[16384]; int n = recv(s, t, sizeof(t), 0);
            if (n <= 0) return false;
            buf.append(t, n);
        }
    }
    // Lê até n bytes (usa primeiro o que sobrou do cabeçalho).
    int ler(char* d, int n) {
        if (!buf.empty()) { int k = (int)std::min((size_t)n, buf.size()); memcpy(d, buf.data(), k); buf.erase(0, k); return k; }
        return recv(s, d, n, 0);
    }
    bool lerN(std::string& saida, long long n) {
        saida.clear();
        char t[65536];
        while ((long long)saida.size() < n) { int r = ler(t, (int)std::min<long long>(sizeof(t), n - saida.size())); if (r <= 0) return false; saida.append(t, r); }
        return true;
    }
};

static SOCKET conectar(const std::string& hostPorta, int msPrazo) {
    std::string host = hostPorta; int porta = 8000;
    size_t p = host.rfind(':'); if (p != std::string::npos) { porta = atoi(host.c_str() + p + 1); host = host.substr(0, p); }
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons((u_short)porta);
    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1) return INVALID_SOCKET;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return s;
    prazo(s, msPrazo);
    int sim = 1; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&sim, sizeof(sim));
    if (connect(s, (sockaddr*)&a, sizeof(a)) != 0) { closesocket(s); return INVALID_SOCKET; }
    return s;
}

// GET simples no PS4: devolve o corpo inteiro (para listas).
static bool ps4Get(const std::string& ps4, const std::string& caminho, std::string& corpo, int* status) {
    Conexao c; c.s = conectar(ps4, 15000);
    if (c.s == INVALID_SOCKET) return false;
    std::string ped = "GET " + caminho + " HTTP/1.1\r\nHost: " + ps4 + "\r\nConnection: close\r\n\r\n";
    std::string cab;
    bool ok = enviarTudo(c.s, ped.data(), ped.size()) && c.lerCabecalho(cab);
    if (ok) {
        *status = atoi(cab.c_str() + 9);
        std::string tl = cabValor(cab, "Content-Length");
        ok = c.lerN(corpo, tl.empty() ? 0 : _atoi64(tl.c_str()));
    }
    closesocket(c.s);
    return ok;
}

// ---------------------------------------------------------------- tarefas de cópia
struct Tarefa {
    int id; std::string nome, atual, erro, estado;   // fila, rodando, ok, erro, cancelada
    long long feito = 0, total = 0; int arquivos = 0, prontos = 0;
    std::string dir, ps4, de, para;
};
static std::deque<Tarefa> tarefas;
static int proximaTarefa = 1;
static volatile bool cancelarTudo = false;
static HANDLE avisoTarefa;

static Tarefa* acharTarefa(int id) { for (auto& t : tarefas) if (t.id == id) return &t; return NULL; }

// Lista do PS4 no formato da página: [{"n":"..","d":1,"t":123,"m":..},...]
struct ItemPs4 { std::string n; bool d; long long t; };
static bool lerListaPs4(const std::string& json, std::vector<ItemPs4>& saida) {
    size_t i = 0;
    while ((i = json.find("{\"n\":\"", i)) != std::string::npos) {
        i += 6;
        ItemPs4 it; it.d = false; it.t = 0;
        while (i < json.size() && json[i] != '"') {
            if (json[i] == '\\' && i + 1 < json.size()) {
                char e = json[i + 1];
                if (e == 'u' && i + 5 < json.size()) { it.n += (char)strtol(json.substr(i + 2, 4).c_str(), NULL, 16); i += 6; continue; }
                it.n += e; i += 2; continue;
            }
            it.n += json[i++];
        }
        size_t d = json.find("\"d\":", i), t = json.find("\"t\":", i);
        if (d != std::string::npos) it.d = json[d + 4] == '1';
        if (t != std::string::npos) it.t = _atoi64(json.c_str() + t + 4);
        saida.push_back(it);
    }
    return true;
}

static std::string juntarPs4(const std::string& a, const std::string& b) { return a == "/" ? "/" + b : a + "/" + b; }
static std::string juntarPc(const std::string& a, const std::string& b) { return (!a.empty() && (a.back() == '/' || a.back() == '\\')) ? a + b : a + "/" + b; }
static std::string nomeDe(const std::string& c) { std::string s = barras(c); while (s.size() > 1 && s.back() == '/') s.pop_back(); size_t p = s.rfind('/'); return p == std::string::npos ? s : s.substr(p + 1); }

static void progresso(int id, long long somar, const std::string* atual = NULL) {
    Trava t; Tarefa* x = acharTarefa(id); if (!x) return;
    x->feito += somar; if (atual) x->atual = *atual;
}

// PS4 -> PC: um arquivo.
static bool baixarArquivo(int id, const std::string& ps4, const std::string& origem, const std::string& destino, std::string& erro) {
    Conexao c; c.s = conectar(ps4, 30000);
    if (c.s == INVALID_SOCKET) { erro = "sem conexao com o PS4"; return false; }
    std::string ped = "GET /fs/baixar?p=" + urlCodificar(origem) + " HTTP/1.1\r\nHost: " + ps4 + "\r\nConnection: close\r\n\r\n";
    std::string cab;
    if (!enviarTudo(c.s, ped.data(), ped.size()) || !c.lerCabecalho(cab)) { closesocket(c.s); erro = "PS4 nao respondeu"; return false; }
    int status = atoi(cab.c_str() + 9);
    long long tam = _atoi64(cabValor(cab, "Content-Length").c_str());
    if (status != 200) { closesocket(c.s); erro = "PS4 respondeu " + std::to_string(status); return false; }
    std::wstring w = largo(destino + ".parte");
    HANDLE f = CreateFileW(w.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { closesocket(c.s); erro = "nao criou " + destino; return false; }
    std::vector<char> b(1 << 20);
    long long recebido = 0; bool ok = true;
    while (recebido < tam) {
        if (cancelarTudo) { ok = false; erro = "cancelada"; break; }
        int n = c.ler(b.data(), (int)std::min<long long>(b.size(), tam - recebido));
        if (n <= 0) { ok = false; erro = "conexao caiu"; break; }
        DWORD escrito = 0;
        if (!WriteFile(f, b.data(), n, &escrito, NULL) || (int)escrito != n) { ok = false; erro = "disco cheio?"; break; }
        recebido += n; progresso(id, n);
    }
    CloseHandle(f); closesocket(c.s);
    std::wstring final_ = largo(destino);
    if (ok) { DeleteFileW(final_.c_str()); ok = MoveFileW(w.c_str(), final_.c_str()) != 0; if (!ok) erro = "nao renomeou"; }
    if (!ok) DeleteFileW(w.c_str());
    return ok;
}

// PC -> PS4: um arquivo.
static bool enviarArquivo(int id, const std::string& ps4, const std::string& origem, const std::string& destino, std::string& erro) {
    std::wstring w = largo(origem);
    HANDLE f = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) { erro = "nao abriu " + origem; return false; }
    LARGE_INTEGER tam; GetFileSizeEx(f, &tam);
    Conexao c; c.s = conectar(ps4, 30000);
    if (c.s == INVALID_SOCKET) { CloseHandle(f); erro = "sem conexao com o PS4"; return false; }
    std::string ped = "POST /fs/enviar?p=" + urlCodificar(destino) + " HTTP/1.1\r\nHost: " + ps4 + "\r\nContent-Length: " + std::to_string(tam.QuadPart) + "\r\nConnection: close\r\n\r\n";
    bool ok = enviarTudo(c.s, ped.data(), ped.size());
    std::vector<char> b(1 << 20);
    long long falta = tam.QuadPart;
    while (ok && falta > 0) {
        if (cancelarTudo) { ok = false; erro = "cancelada"; break; }
        DWORD lido = 0;
        if (!ReadFile(f, b.data(), (DWORD)std::min<long long>(b.size(), falta), &lido, NULL) || lido == 0) { ok = false; erro = "erro lendo o arquivo"; break; }
        if (!enviarTudo(c.s, b.data(), lido)) { ok = false; erro = "conexao caiu"; break; }
        falta -= lido; progresso(id, lido);
    }
    CloseHandle(f);
    if (ok) {
        std::string cab, corpo;
        ok = c.lerCabecalho(cab) && atoi(cab.c_str() + 9) == 200;
        if (!ok) erro = "PS4 recusou (" + (cab.size() > 12 ? cab.substr(9, 3) : std::string("?")) + ")";
    }
    closesocket(c.s);
    return ok;
}

static void criarPastaPs4(const std::string& ps4, const std::string& c) {
    Conexao x; x.s = conectar(ps4, 15000);
    if (x.s == INVALID_SOCKET) return;
    std::string ped = "POST /fs/pasta?p=" + urlCodificar(c) + " HTTP/1.1\r\nHost: " + ps4 + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    std::string cab;
    if (enviarTudo(x.s, ped.data(), ped.size())) x.lerCabecalho(cab);
    closesocket(x.s);
}

// Monta a lista de arquivos (origem, destino, tamanho) de uma cópia.
struct Copia { std::string de, para; long long t; bool pasta; };
static void listarPc(const std::string& de, const std::string& para, std::vector<Copia>& l) {
    std::wstring w = largo(de);
    DWORD at = GetFileAttributesW(w.c_str());
    if (at == INVALID_FILE_ATTRIBUTES) return;
    if (!(at & FILE_ATTRIBUTE_DIRECTORY)) {
        WIN32_FILE_ATTRIBUTE_DATA d; GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &d);
        l.push_back({ de, para, ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow, false });
        return;
    }
    l.push_back({ de, para, 0, true });
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(largo(juntarPc(de, "*")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        listarPc(juntarPc(de, utf8(n)), juntarPs4(para, utf8(n)), l);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void listarPs4(const std::string& ps4, const std::string& de, const std::string& para, bool pasta, long long t, std::vector<Copia>& l) {
    if (!pasta) { l.push_back({ de, para, t, false }); return; }
    l.push_back({ de, para, 0, true });
    std::string corpo; int st = 0;
    if (!ps4Get(ps4, "/fs/lista?p=" + urlCodificar(de), corpo, &st) || st != 200) return;
    std::vector<ItemPs4> itens; lerListaPs4(corpo, itens);
    for (auto& it : itens) listarPs4(ps4, juntarPs4(de, it.n), juntarPc(para, it.n), it.d, it.t, l);
}

static DWORD WINAPI threadCopias(LPVOID) {
    for (;;) {
        WaitForSingleObject(avisoTarefa, 1000);
        int id = 0; Tarefa t;
        {
            Trava x;
            for (auto& k : tarefas) if (k.estado == "fila") { k.estado = "rodando"; id = k.id; t = k; break; }
        }
        if (!id) continue;
        cancelarTudo = false;
        std::vector<Copia> lista;
        std::string erro;
        if (t.dir == "pcps4") listarPc(t.de, juntarPs4(t.para, nomeDe(t.de)), lista);
        else {
            // Do PS4: descobre se é pasta pela lista da pasta de cima.
            std::string corpo; int st = 0;
            std::string paiPs4 = t.de.substr(0, t.de.rfind('/')); if (paiPs4.empty()) paiPs4 = "/";
            bool pasta = false; long long tam = 0;
            if (ps4Get(t.ps4, "/fs/info?p=" + urlCodificar(t.de), corpo, &st) && st == 200) { pasta = corpo.find("\"d\":1") != std::string::npos; size_t k = corpo.find("\"t\":"); if (k != std::string::npos) tam = _atoi64(corpo.c_str() + k + 4); }
            else erro = "item nao encontrado no PS4";
            if (erro.empty()) listarPs4(t.ps4, t.de, juntarPc(t.para, nomeDe(t.de)), pasta, tam, lista);
        }
        long long total = 0; int arquivos = 0;
        for (auto& c : lista) if (!c.pasta) { total += c.t; arquivos++; }
        { Trava x; Tarefa* k = acharTarefa(id); if (k) { k->total = total; k->arquivos = arquivos; } }
        for (auto& c : lista) {
            if (!erro.empty() || cancelarTudo) break;
            if (c.pasta) {
                if (t.dir == "pcps4") criarPastaPs4(t.ps4, c.para);
                else SHCreateDirectoryExW(NULL, largo(barras(c.para)).c_str(), NULL);
                continue;
            }
            std::string nome = nomeDe(c.de);
            progresso(id, 0, &nome);
            bool ok = t.dir == "pcps4" ? enviarArquivo(id, t.ps4, c.de, c.para, erro) : baixarArquivo(id, t.ps4, c.de, c.para, erro);
            if (ok) { Trava x; Tarefa* k = acharTarefa(id); if (k) k->prontos++; }
        }
        Trava x; Tarefa* k = acharTarefa(id);
        if (k) { k->estado = cancelarTudo ? "cancelada" : (erro.empty() ? "ok" : "erro"); k->erro = erro; k->atual = ""; }
        printf("Copia %s: %s %s\n", k ? k->nome.c_str() : "?", k ? k->estado.c_str() : "", erro.c_str());
    }
    return 0;
}

// ---------------------------------------------------------------- tela do PC para o PS4
static volatile bool espelhoAtivo = false;
static std::string espelhoPs4, espelhoErro;
static int espelhoFps = 12, espelhoLarg = 1280, espelhoQualidade = 70;
static double espelhoFpsReal = 0, espelhoKbps = 0;
static HANDLE espelhoThread = NULL;

static void jpegAcumular(void* ctx, void* d, int n) { ((std::string*)ctx)->append((const char*)d, n); }

// Captura + JPEG em 2 threads (cada uma faz um quadro inteiro); a de envio manda só o mais novo.
static std::string quadroNovo; static unsigned quadroSeq = 0;
static int espelhoW = 0, espelhoH = 0;

static DWORD WINAPI threadCaptura(LPVOID) {
    HDC tela = GetDC(NULL);
    HDC mem = CreateCompatibleDC(tela);
    int largTela = GetSystemMetrics(SM_CXSCREEN), altTela = GetSystemMetrics(SM_CYSCREEN);
    int W = espelhoW, H = espelhoH;
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP bmp = CreateDIBSection(tela, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ antigo = SelectObject(mem, bmp);
    // HALFTONE é bonito mas leva 100 ms numa tela comum; COLORONCOLOR fica em 40 ms.
    SetStretchBltMode(mem, COLORONCOLOR);
    std::vector<uint8_t> rgb(W * H * 3);
    while (espelhoAtivo) {
        uint64_t ini = agoraMs();
        if (W == largTela && H == altTela) BitBlt(mem, 0, 0, W, H, tela, 0, 0, SRCCOPY | CAPTUREBLT);
        else StretchBlt(mem, 0, 0, W, H, tela, 0, 0, largTela, altTela, SRCCOPY | CAPTUREBLT);
        CURSORINFO ci = { sizeof(ci) };
        if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) {
            ICONINFO ii; int hx = 0, hy = 0;
            if (GetIconInfo(ci.hCursor, &ii)) { hx = ii.xHotspot; hy = ii.yHotspot; if (ii.hbmMask) DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor); }
            int x = (int)((long long)(ci.ptScreenPos.x - hx) * W / largTela), y = (int)((long long)(ci.ptScreenPos.y - hy) * H / altTela);
            DrawIconEx(mem, x, y, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
        }
        GdiFlush();
        const uint8_t* p = (const uint8_t*)bits;
        for (int i = 0; i < W * H; i++) { rgb[i * 3] = p[i * 4 + 2]; rgb[i * 3 + 1] = p[i * 4 + 1]; rgb[i * 3 + 2] = p[i * 4]; }
        std::string jpeg;
        stbi_write_jpg_to_func(jpegAcumular, &jpeg, W, H, 3, rgb.data(), espelhoQualidade);
        { Trava x; quadroNovo.swap(jpeg); quadroSeq++; }
        // Duas threads: cada uma espera o dobro do intervalo do fps pedido.
        int alvo = 2000 / (espelhoFps > 0 ? espelhoFps : 1), gasto = (int)(agoraMs() - ini);
        if (gasto < alvo) Sleep(alvo - gasto);
    }
    SelectObject(mem, antigo); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, tela);
    return 0;
}

static DWORD WINAPI threadEspelho(LPVOID) {
    int largTela = GetSystemMetrics(SM_CXSCREEN), altTela = GetSystemMetrics(SM_CYSCREEN);
    espelhoW = std::min(espelhoLarg, largTela) & ~1; espelhoH = (int)((long long)altTela * espelhoW / largTela) & ~1;
    if (espelhoLarg >= largTela) { espelhoW = largTela; espelhoH = altTela; }
    { Trava x; quadroNovo.clear(); }
    HANDLE capt[2];
    for (int i = 0; i < 2; i++) { capt[i] = CreateThread(NULL, 0, threadCaptura, NULL, 0, NULL); Sleep(1000 / (espelhoFps > 0 ? espelhoFps : 1)); }
    Conexao c;
    unsigned enviado = 0;
    int quadros = 0; long long bytes = 0; uint64_t marco = agoraMs();
    printf("Tela do PC -> PS4 %s (%dx%d, %d fps)\n", espelhoPs4.c_str(), espelhoW, espelhoH, espelhoFps);
    while (espelhoAtivo) {
        std::string jpeg;
        { Trava x; if (quadroSeq != enviado) { jpeg = quadroNovo; enviado = quadroSeq; } }
        if (jpeg.empty()) { Sleep(5); continue; }
        if (c.s == INVALID_SOCKET) { c.s = conectar(espelhoPs4, 8000); c.buf.clear(); }
        bool ok = c.s != INVALID_SOCKET;
        std::string cab;
        if (ok) {
            std::string ped = "POST /espelho/quadro HTTP/1.1\r\nHost: " + espelhoPs4 + "\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(jpeg.size()) + "\r\nConnection: keep-alive\r\n\r\n";
            ok = enviarTudo(c.s, ped.data(), ped.size()) && enviarTudo(c.s, jpeg.data(), jpeg.size()) && c.lerCabecalho(cab);
            if (ok) { std::string corpo; c.lerN(corpo, _atoi64(cabValor(cab, "Content-Length").c_str())); }
        }
        if (!ok) {
            if (c.s != INVALID_SOCKET) closesocket(c.s);
            c.s = INVALID_SOCKET;
            { Trava x; espelhoErro = "sem conexao com o PS4"; }
            Sleep(1000);
            continue;
        }
        if (atoi(cab.c_str() + 9) == 410) { Trava x; espelhoErro = "fechado no PS4"; espelhoAtivo = false; break; }
        { Trava x; espelhoErro = ""; }
        quadros++; bytes += jpeg.size();
        uint64_t agora = agoraMs();
        if (agora - marco >= 2000) { espelhoFpsReal = quadros * 1000.0 / (agora - marco); espelhoKbps = bytes * 8.0 / (agora - marco); quadros = 0; bytes = 0; marco = agora; }
    }
    WaitForMultipleObjects(2, capt, TRUE, 3000);
    CloseHandle(capt[0]); CloseHandle(capt[1]);
    if (c.s != INVALID_SOCKET) closesocket(c.s);
    // Avisa o PS4 para fechar a tela na hora.
    Conexao f; f.s = conectar(espelhoPs4, 3000);
    if (f.s != INVALID_SOCKET) {
        std::string ped = "POST /espelho/parar HTTP/1.1\r\nHost: " + espelhoPs4 + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        std::string cab; if (enviarTudo(f.s, ped.data(), ped.size())) f.lerCabecalho(cab);
        closesocket(f.s);
    }
    printf("Tela do PC encerrada\n");
    espelhoFpsReal = 0; espelhoKbps = 0;
    return 0;
}

// ---------------------------------------------------------------- pedidos do PS4
// A tela de FTP do PS4 controla este PC. Como este programa só escuta em 127.0.0.1, é ele que
// busca os pedidos no PS4 (GET /pc/pedidos, que espera até 700 ms) e devolve o resultado.
static std::string ps4Endereco;   // "ip:8000"
static std::string pastaEspecial(REFKNOWNFOLDERID id);

static void pcMouse(DWORD flags, LONG dx, LONG dy, DWORD dados) {
    INPUT i = {}; i.type = INPUT_MOUSE; i.mi.dx = dx; i.mi.dy = dy; i.mi.dwFlags = flags; i.mi.mouseData = dados;
    SendInput(1, &i, sizeof(i));
}
static void pcTecla(WORD vk, bool desce) {
    INPUT i = {}; i.type = INPUT_KEYBOARD; i.ki.wVk = vk;
    i.ki.dwFlags = desce ? 0 : KEYEVENTF_KEYUP;
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN || vk == VK_LWIN) i.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &i, sizeof(i));
}
static void pcTexto(const std::string& t) {
    std::wstring w = largo(t);
    std::vector<INPUT> v;
    for (wchar_t ch : w) {
        INPUT a = {}; a.type = INPUT_KEYBOARD; a.ki.wScan = ch; a.ki.dwFlags = KEYEVENTF_UNICODE;
        v.push_back(a); a.ki.dwFlags |= KEYEVENTF_KEYUP; v.push_back(a);
    }
    if (!v.empty()) SendInput((UINT)v.size(), v.data(), sizeof(INPUT));
}
static std::vector<std::string> separar(const std::string& l, char sep) {
    std::vector<std::string> r; size_t ini = 0;
    for (;;) { size_t p = l.find(sep, ini); if (p == std::string::npos) { r.push_back(l.substr(ini)); break; } r.push_back(l.substr(ini, p - ini)); ini = p + 1; }
    return r;
}
static std::string semTab(std::string s) { for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' '; return s; }

// Lista em linhas para o PS4: "ok\t<pasta>" e depois "d|f\tnome\tcaminho\ttamanho".
static std::string listaParaPs4(const std::string& pasta) {
    std::string r;
    if (pasta.empty()) {
        r = "ok\t\n";
        struct { const char* nome; const KNOWNFOLDERID* id; } atalhos[] = {
            { "Área de Trabalho", &FOLDERID_Desktop }, { "Downloads", &FOLDERID_Downloads }, { "Documentos", &FOLDERID_Documents },
            { "Imagens", &FOLDERID_Pictures }, { "Vídeos", &FOLDERID_Videos }, { "Músicas", &FOLDERID_Music } };
        for (auto& a : atalhos) { std::string p = pastaEspecial(*a.id); if (!p.empty()) r += std::string("d\t") + a.nome + "\t" + barras(p) + "\t0\n"; }
        wchar_t unidades[512]; DWORD n = GetLogicalDriveStringsW(511, unidades);
        for (wchar_t* u = unidades; n && *u; u += wcslen(u) + 1) {
            UINT tipo = GetDriveTypeW(u);
            wchar_t rotulo[128] = L""; GetVolumeInformationW(u, rotulo, 127, NULL, NULL, NULL, NULL, 0);
            std::string letra = utf8(std::wstring(u, 2));
            std::string desc = tipo == DRIVE_REMOVABLE ? "Removível" : tipo == DRIVE_CDROM ? "CD/DVD" : tipo == DRIVE_REMOTE ? "Rede" : "Disco local";
            r += "d\t" + semTab((rotulo[0] ? utf8(rotulo) : desc) + " (" + letra + ")") + "\t" + letra + "/\t0\n";
        }
        return r;
    }
    struct Item { bool d; std::wstring n; long long t; };
    std::vector<Item> itens;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(largo(juntarPc(pasta, "*")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return "erro\tpasta nao abriu\n";
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) && (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) continue;
        bool d = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        itens.push_back({ d, n, d ? 0 : (((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow) });
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(itens.begin(), itens.end(), [](const Item& a, const Item& b) { if (a.d != b.d) return a.d; return _wcsicmp(a.n.c_str(), b.n.c_str()) < 0; });
    r = "ok\t" + pasta + "\n";
    for (auto& it : itens) {
        std::string n = semTab(utf8(it.n));
        r += std::string(it.d ? "d" : "f") + "\t" + n + "\t" + juntarPc(pasta, n) + "\t" + std::to_string(it.t) + "\n";
    }
    return r;
}

static bool ps4Post(const std::string& ps4, const std::string& caminho, const std::string& corpo) {
    Conexao c; c.s = conectar(ps4, 15000);
    if (c.s == INVALID_SOCKET) return false;
    std::string ped = "POST " + caminho + " HTTP/1.1\r\nHost: " + ps4 + "\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: " + std::to_string(corpo.size()) + "\r\nConnection: close\r\n\r\n";
    std::string cab;
    bool ok = enviarTudo(c.s, ped.data(), ped.size()) && enviarTudo(c.s, corpo.data(), corpo.size()) && c.lerCabecalho(cab);
    closesocket(c.s);
    return ok;
}

static void iniciarEspelho(const std::string& ps4, int larg, int fps, int qualidade) {
    if (espelhoAtivo) { espelhoAtivo = false; WaitForSingleObject(espelhoThread, 5000); }
    espelhoPs4 = ps4;
    espelhoFps = fps >= 1 && fps <= 30 ? fps : 12;
    espelhoLarg = larg >= 320 && larg <= 1920 ? larg : 1280;
    espelhoQualidade = qualidade >= 20 && qualidade <= 95 ? qualidade : 70;
    espelhoErro = ""; espelhoAtivo = true;
    if (espelhoThread) CloseHandle(espelhoThread);
    espelhoThread = CreateThread(NULL, 0, threadEspelho, NULL, 0, NULL);
}

static std::string estadoParaPs4() {
    Trava x;
    for (auto& t : tarefas) {
        if (t.estado == "rodando") {
            int pct = t.total > 0 ? (int)(t.feito * 100 / t.total) : 0;
            return t.nome + " " + std::to_string(pct) + "% (" + std::to_string(t.prontos) + "/" + std::to_string(t.arquivos) + ")";
        }
    }
    int fila = 0; for (auto& t : tarefas) if (t.estado == "fila") fila++;
    if (fila) return std::to_string(fila) + " copia(s) na fila";
    if (!tarefas.empty()) { auto& u = tarefas.back(); if (u.estado == "ok") return "copiado: " + u.nome; if (u.estado == "erro") return "erro: " + u.nome + " " + u.erro; }
    return "";
}

static void executarPedido(const std::string& linha) {
    std::vector<std::string> c = separar(linha, '\t');
    const std::string& cmd = c[0];
    if (cmd == "mover" && c.size() >= 3) pcMouse(MOUSEEVENTF_MOVE, atoi(c[1].c_str()), atoi(c[2].c_str()), 0);
    else if (cmd == "rolar" && c.size() >= 2) pcMouse(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(atoi(c[1].c_str()) * WHEEL_DELTA));
    else if (cmd == "botao" && c.size() >= 3) {
        bool desce = c[2] == "1";
        DWORD f = c[1] == "dir" ? (desce ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP) : c[1] == "meio" ? (desce ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP) : (desce ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        pcMouse(f, 0, 0, 0);
    }
    else if (cmd == "tecla" && c.size() >= 3) { int vk = atoi(c[1].c_str()); if (vk > 0 && vk < 256) pcTecla((WORD)vk, c[2] == "1"); }
    else if (cmd == "texto" && c.size() >= 2) pcTexto(linha.substr(6));
    else if (cmd == "espelho" && c.size() >= 2) {
        if (c[1] == "1") { iniciarEspelho(ps4Endereco, c.size() > 2 ? atoi(c[2].c_str()) : 1280, c.size() > 3 ? atoi(c[3].c_str()) : 15, c.size() > 4 ? atoi(c[4].c_str()) : 65); printf("O PS4 pediu a tela do PC\n"); }
        else espelhoAtivo = false;
    }
    else if (cmd == "listar" && c.size() >= 2) {
        std::string pasta = c.size() >= 3 ? barras(c[2]) : "";
        ps4Post(ps4Endereco, "/pc/resposta?id=" + c[1], listaParaPs4(pasta));
    }
    else if (cmd == "copiar" && c.size() >= 3) {
        Tarefa t;
        t.dir = "pcps4"; t.ps4 = ps4Endereco; t.de = barras(c[1]); t.para = c[2];
        t.nome = "→ PS4 " + nomeDe(t.de); t.estado = "fila";
        { Trava x; t.id = proximaTarefa++; tarefas.push_back(t); }
        SetEvent(avisoTarefa);
        printf("O PS4 pediu: copiar %s para %s\n", t.de.c_str(), t.para.c_str());
    }
}

static std::string acharPs4();
static DWORD WINAPI threadPedidosPs4(LPVOID) {
    wchar_t nome[64] = L"PC"; DWORD tn = 63; GetComputerNameW(nome, &tn);
    std::string meuNome = urlCodificar(utf8(nome));
    bool avisado = false;
    int falhas = 0;
    for (;;) {
        std::string corpo; int st = 0;
        std::string caminho = "/pc/pedidos?pc=" + meuNome + "&v=" + std::to_string(VERSAO) + "&estado=" + urlCodificar(estadoParaPs4());
        if (ps4Endereco.empty() || !ps4Get(ps4Endereco, caminho, corpo, &st)) {
            // PS4 desligado ou com IP novo: de tempos em tempos procura de novo pela rede.
            if (++falhas % 5 == 0 || ps4Endereco.empty()) {
                std::string achado = acharPs4();
                if (!achado.empty()) { Trava x; ps4Endereco = achado + ":8000"; }
            }
            Sleep(2000);
            continue;
        }
        falhas = 0;
        if (st == 404) { Sleep(10000); continue; }          // Hyper Neiva antigo no PS4
        if (st != 200) { Sleep(1000); continue; }
        if (!avisado) { printf("Ligado ao PS4: a tela de FTP do PS4 pode controlar este PC.\n"); avisado = true; }
        size_t ini = 0;
        while (ini < corpo.size()) {
            size_t f = corpo.find('\n', ini); if (f == std::string::npos) f = corpo.size();
            std::string l = corpo.substr(ini, f - ini);
            if (!l.empty()) executarPedido(l);
            ini = f + 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------- servidor local
// Só a página do Hyper Neiva: http://<ip de rede local>:8000 (ou o teste em 127.0.0.1).
static bool origemPermitida(const std::string& o) {
    unsigned a, b, c, d, porta;
    if (sscanf(o.c_str(), "http://%u.%u.%u.%u:%u", &a, &b, &c, &d, &porta) != 5 || porta != 8000) return false;
    if (a == 127) return true;
    if (a == 10) return true;
    if (a == 192 && b == 168) return true;
    if (a == 172 && b >= 16 && b <= 31) return true;
    return false;
}

static bool responder(SOCKET s, const char* status, const char* tipo, const std::string& corpo, const std::string& origem) {
    std::string r = std::string("HTTP/1.1 ") + status + "\r\nContent-Type: " + tipo + "\r\nContent-Length: " + std::to_string(corpo.size()) +
        "\r\nCache-Control: no-store\r\nConnection: keep-alive\r\n";
    if (!origem.empty()) r += "Access-Control-Allow-Origin: " + origem + "\r\nVary: Origin\r\nAccess-Control-Allow-Private-Network: true\r\n";
    r += "\r\n";
    return enviarTudo(s, r.data(), r.size()) && enviarTudo(s, corpo.data(), corpo.size());
}
static bool json(SOCKET s, const std::string& corpo, const std::string& origem, const char* status = "200 OK") { return responder(s, status, "application/json; charset=utf-8", corpo, origem); }

static std::string pastaEspecial(REFKNOWNFOLDERID id) {
    PWSTR p = NULL; std::string r;
    if (SHGetKnownFolderPath(id, 0, NULL, &p) == S_OK) r = barras(utf8(p));
    if (p) CoTaskMemFree(p);
    return r;
}

static std::string raizes() {
    std::string j = "[";
    struct { const char* nome; const KNOWNFOLDERID* id; } atalhos[] = {
        { "Área de Trabalho", &FOLDERID_Desktop }, { "Downloads", &FOLDERID_Downloads }, { "Documentos", &FOLDERID_Documents },
        { "Imagens", &FOLDERID_Pictures }, { "Vídeos", &FOLDERID_Videos }, { "Músicas", &FOLDERID_Music }, { "Pasta pessoal", &FOLDERID_Profile } };
    bool primeiro = true;
    for (auto& a : atalhos) {
        std::string p = pastaEspecial(*a.id); if (p.empty()) continue;
        j += std::string(primeiro ? "" : ",") + "{\"n\":\"" + jsonTexto(a.nome) + "\",\"p\":\"" + jsonTexto(p) + "\",\"tipo\":\"atalho\"}";
        primeiro = false;
    }
    wchar_t unidades[512]; DWORD n = GetLogicalDriveStringsW(511, unidades);
    for (wchar_t* u = unidades; n && *u; u += wcslen(u) + 1) {
        UINT tipo = GetDriveTypeW(u);
        wchar_t rotulo[128] = L""; GetVolumeInformationW(u, rotulo, 127, NULL, NULL, NULL, NULL, 0);
        ULARGE_INTEGER livre = {}, total = {}; GetDiskFreeSpaceExW(u, &livre, &total, NULL);
        std::string letra = utf8(std::wstring(u, 2));
        std::string desc = tipo == DRIVE_REMOVABLE ? "Removível" : tipo == DRIVE_CDROM ? "CD/DVD" : tipo == DRIVE_REMOTE ? "Rede" : "Disco local";
        std::string nome = (rotulo[0] ? utf8(rotulo) : desc) + " (" + letra + ")";
        j += std::string(primeiro ? "" : ",") + "{\"n\":\"" + jsonTexto(nome) + "\",\"p\":\"" + jsonTexto(letra + "/") + "\",\"tipo\":\"disco\",\"livre\":" +
            std::to_string(livre.QuadPart) + ",\"total\":" + std::to_string(total.QuadPart) + "}";
        primeiro = false;
    }
    return j + "]";
}

static long long unixDe(const FILETIME& f) { ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime; return (long long)(u.QuadPart / 10000000ULL) - 11644473600LL; }

static bool listar(const std::string& pasta, std::string& j) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(largo(juntarPc(pasta, "*")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    j = "["; bool primeiro = true;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        // Arquivos de sistema escondidos (desktop.ini, $Recycle.Bin...) ficam de fora.
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) && (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) continue;
        bool d = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        long long t = d ? 0 : (((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow);
        j += std::string(primeiro ? "" : ",") + "{\"n\":\"" + jsonTexto(utf8(n)) + "\",\"d\":" + (d ? "1" : "0") + ",\"t\":" + std::to_string(t) + ",\"m\":" + std::to_string(unixDe(fd.ftLastWriteTime)) + "}";
        primeiro = false;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    j += "]";
    return true;
}

// Chaves do Hyper Neiva de PC (data/HyperNeiva/api_keys.dat, cifrado com o DPAPI do Windows).
// Este programa roda com o mesmo usuário, então consegue abrir e mandar para a página importar.
struct ApiKeyPc { char nome[64]; char desc[256]; char chave[512]; char app_id[256]; };
static bool lerArquivo(const std::wstring& c, std::string& saida) {
    HANDLE f = CreateFileW(c.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER t; GetFileSizeEx(f, &t);
    if (t.QuadPart <= 0 || t.QuadPart > 1 << 20) { CloseHandle(f); return false; }
    saida.resize((size_t)t.QuadPart);
    DWORD lido = 0; bool ok = ReadFile(f, &saida[0], (DWORD)t.QuadPart, &lido, NULL) && lido == t.QuadPart;
    CloseHandle(f);
    return ok;
}
static std::string chavesDoPc(const std::string& pastaPedida, std::string& achado) {
    std::vector<std::string> lugares;
    if (!pastaPedida.empty()) { lugares.push_back(juntarPc(pastaPedida, "api_keys.dat")); lugares.push_back(juntarPc(pastaPedida, "data/HyperNeiva/api_keys.dat")); }
    std::string mesa = pastaEspecial(FOLDERID_Desktop), perfil = pastaEspecial(FOLDERID_Profile);
    lugares.push_back(mesa + "/player/data/HyperNeiva/api_keys.dat");
    lugares.push_back(mesa + "/player winds/data/HyperNeiva/api_keys.dat");
    lugares.push_back(perfil + "/source/repos/hyperneivawindows/data/HyperNeiva/api_keys.dat");
    // Qualquer pasta da Área de Trabalho que tenha data/HyperNeiva/api_keys.dat.
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(largo(mesa + "/*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') lugares.push_back(mesa + "/" + utf8(fd.cFileName) + "/data/HyperNeiva/api_keys.dat"); } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    for (auto& l : lugares) {
        std::string bruto;
        if (!lerArquivo(largo(l), bruto)) continue;
        DATA_BLOB entrada = { (DWORD)bruto.size(), (BYTE*)&bruto[0] }, saida = { 0, NULL };
        std::string claro;
        if (CryptUnprotectData(&entrada, NULL, NULL, NULL, NULL, 0, &saida)) { claro.assign((const char*)saida.pbData, saida.cbData); SecureZeroMemory(saida.pbData, saida.cbData); LocalFree(saida.pbData); }
        else claro = bruto;   // o PC grava sem cifrar se o DPAPI falhar
        if (claro.size() < sizeof(ApiKeyPc)) continue;
        achado = l;
        std::string j = "[";
        int n = (int)(claro.size() / sizeof(ApiKeyPc));
        for (int i = 0; i < n && i < 16; i++) {
            ApiKeyPc k; memcpy(&k, claro.data() + i * sizeof(ApiKeyPc), sizeof(k));
            k.nome[63] = k.desc[255] = k.chave[511] = k.app_id[255] = 0;
            j += std::string(i ? "," : "") + "{\"nome\":\"" + jsonTexto(k.nome) + "\",\"desc\":\"" + jsonTexto(k.desc) + "\",\"chave\":\"" + jsonTexto(k.chave) + "\",\"app_id\":\"" + jsonTexto(k.app_id) + "\"}";
            SecureZeroMemory(&k, sizeof(k));
        }
        SecureZeroMemory(&claro[0], claro.size());
        return j + "]";
    }
    return "";
}

static std::string listaTarefas() {
    Trava x;
    std::string j = "["; bool primeiro = true;
    for (auto& t : tarefas) {
        j += std::string(primeiro ? "" : ",") + "{\"id\":" + std::to_string(t.id) + ",\"nome\":\"" + jsonTexto(t.nome) + "\",\"atual\":\"" + jsonTexto(t.atual) +
            "\",\"estado\":\"" + t.estado + "\",\"erro\":\"" + jsonTexto(t.erro) + "\",\"feito\":" + std::to_string(t.feito) + ",\"total\":" + std::to_string(t.total) +
            ",\"arquivos\":" + std::to_string(t.arquivos) + ",\"prontos\":" + std::to_string(t.prontos) + ",\"dir\":\"" + t.dir + "\"}";
        primeiro = false;
    }
    return j + "]";
}

static bool tratar(SOCKET s, const std::string& metodo, const std::string& alvo, const std::string& cab, const std::string& corpo) {
    std::string caminho = alvo, q;
    size_t i = alvo.find('?'); if (i != std::string::npos) { caminho = alvo.substr(0, i); q = alvo.substr(i + 1); }
    std::string origem = cabValor(cab, "Origin");
    std::string host = cabValor(cab, "Host");
    if (host != "127.0.0.1:8001" && host != "localhost:8001") return json(s, "{\"ok\":false}", "", "403 Forbidden");
    if (caminho == "/") return responder(s, "200 OK", "text/plain; charset=utf-8", "Hyper Neiva PC rodando. Abra a pagina do PS4 (http://<ip-do-ps4>:8000) e use a aba Arquivos.", "");
    if (!origemPermitida(origem)) return json(s, "{\"ok\":false,\"erro\":\"origem\"}", "", "403 Forbidden");
    if (metodo == "OPTIONS") {
        std::string r = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: " + origem + "\r\nAccess-Control-Allow-Methods: GET, POST\r\n"
            "Access-Control-Allow-Headers: *\r\nAccess-Control-Allow-Private-Network: true\r\nAccess-Control-Max-Age: 600\r\nVary: Origin\r\nContent-Length: 0\r\n\r\n";
        return enviarTudo(s, r.data(), r.size());
    }
    // O que muda alguma coisa só por POST (um <img> de outro site não consegue).
    bool post = metodo == "POST";
    std::string p = barras(param(q, "p"));
    if (caminho == "/oi") {
        wchar_t nome[64] = L"PC"; DWORD tn = 63; GetComputerNameW(nome, &tn);
        return json(s, "{\"ok\":true,\"app\":\"HyperNeivaPC\",\"versao\":" + std::to_string(VERSAO) + ",\"pc\":\"" + jsonTexto(utf8(nome)) + "\"}", origem);
    }
    if (caminho == "/fs/raizes") return json(s, raizes(), origem);
    if (caminho == "/fs/lista") { std::string j; if (listar(p, j)) return json(s, j, origem); return json(s, "{\"erro\":\"pasta nao abriu\"}", origem, "404 Not Found"); }
    if (caminho == "/tarefas") return json(s, listaTarefas(), origem);
    if (caminho == "/espelho/estado") {
        Trava x; char b[256];
        snprintf(b, sizeof(b), "{\"ativo\":%s,\"fps\":%.1f,\"kbps\":%.0f,\"erro\":\"%s\"}", espelhoAtivo ? "true" : "false", espelhoFpsReal, espelhoKbps, jsonTexto(espelhoErro).c_str());
        return json(s, b, origem);
    }
    if (!post) return json(s, "{\"ok\":false,\"erro\":\"use POST\"}", origem, "405 Method Not Allowed");
    if (caminho == "/fs/pasta") { int r = SHCreateDirectoryExW(NULL, largo(p).c_str(), NULL); return json(s, (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS) ? "{\"ok\":true}" : "{\"ok\":false}", origem); }
    if (caminho == "/fs/renomear") { std::string para = barras(param(q, "para")); bool ok = MoveFileExW(largo(p).c_str(), largo(para).c_str(), 0) != 0; return json(s, ok ? "{\"ok\":true}" : "{\"ok\":false}", origem); }
    if (caminho == "/fs/apagar") {
        // Vai para a Lixeira, nunca apaga de vez.
        if (p.size() <= 3) return json(s, "{\"ok\":false,\"erro\":\"protegido\"}", origem);
        std::wstring w = largo(p); for (auto& c : w) if (c == L'/') c = L'\\';
        w.push_back(0); w.push_back(0);
        SHFILEOPSTRUCTW op = {}; op.wFunc = FO_DELETE; op.pFrom = w.c_str(); op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        bool ok = SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
        return json(s, ok ? "{\"ok\":true}" : "{\"ok\":false}", origem);
    }
    if (caminho == "/fs/abrir") { std::wstring w = largo(p); for (auto& c : w) if (c == L'/') c = L'\\'; ShellExecuteW(NULL, L"open", w.c_str(), NULL, NULL, SW_SHOWNORMAL); return json(s, "{\"ok\":true}", origem); }
    if (caminho == "/copiar") {
        Tarefa t;
        t.dir = param(q, "dir"); t.ps4 = param(q, "ps4"); t.de = param(q, "de"); t.para = param(q, "para");
        if ((t.dir != "pcps4" && t.dir != "ps4pc") || t.ps4.empty() || t.de.empty() || t.para.empty()) return json(s, "{\"ok\":false}", origem, "400 Bad Request");
        if (t.dir == "pcps4") t.de = barras(t.de); else t.para = barras(t.para);
        t.nome = (t.dir == "pcps4" ? "→ PS4 " : "← PC ") + nomeDe(t.de);
        t.estado = "fila";
        { Trava x; t.id = proximaTarefa++; tarefas.push_back(t); while (tarefas.size() > 100 && tarefas.front().estado != "fila" && tarefas.front().estado != "rodando") tarefas.pop_front(); }
        SetEvent(avisoTarefa);
        return json(s, "{\"ok\":true,\"id\":" + std::to_string(t.id) + "}", origem);
    }
    if (caminho == "/chaves/pc") {
        std::string achado, lista = chavesDoPc(barras(param(q, "pasta")), achado);
        if (lista.empty()) return json(s, "{\"ok\":false,\"erro\":\"nao achei data/HyperNeiva/api_keys.dat do Hyper Neiva de PC\"}", origem);
        printf("Chaves do Hyper Neiva de PC lidas de %s\n", achado.c_str());
        return json(s, "{\"ok\":true,\"arquivo\":\"" + jsonTexto(achado) + "\",\"chaves\":" + lista + "}", origem);
    }
    if (caminho == "/tarefas/limpar") { Trava x; std::deque<Tarefa> resto; for (auto& t : tarefas) if (t.estado == "fila" || t.estado == "rodando") resto.push_back(t); tarefas = resto; return json(s, "{\"ok\":true}", origem); }
    if (caminho == "/tarefas/cancelar") { Trava x; for (auto& t : tarefas) if (t.estado == "fila") t.estado = "cancelada"; cancelarTudo = true; return json(s, "{\"ok\":true}", origem); }
    if (caminho == "/espelho/iniciar") {
        std::string ps4 = param(q, "ps4");
        if (ps4.empty()) return json(s, "{\"ok\":false}", origem, "400 Bad Request");
        if (espelhoAtivo) { espelhoAtivo = false; WaitForSingleObject(espelhoThread, 5000); }
        espelhoPs4 = ps4;
        int f = atoi(param(q, "fps").c_str()); espelhoFps = f >= 1 && f <= 30 ? f : 12;
        int l = atoi(param(q, "larg").c_str()); espelhoLarg = l >= 320 && l <= 1920 ? l : 1280;
        int k = atoi(param(q, "q").c_str()); espelhoQualidade = k >= 20 && k <= 95 ? k : 70;
        espelhoErro = ""; espelhoAtivo = true;
        if (espelhoThread) CloseHandle(espelhoThread);
        espelhoThread = CreateThread(NULL, 0, threadEspelho, NULL, 0, NULL);
        return json(s, "{\"ok\":true}", origem);
    }
    if (caminho == "/espelho/parar") { espelhoAtivo = false; return json(s, "{\"ok\":true}", origem); }
    return json(s, "{\"ok\":false}", origem, "404 Not Found");
}

static DWORD WINAPI threadConexao(LPVOID arg) {
    Conexao c; c.s = (SOCKET)(uintptr_t)arg;
    prazo(c.s, 60000);
    for (;;) {
        std::string cab;
        if (!c.lerCabecalho(cab)) break;
        char metodo[16] = "", alvo[4096] = "";
        sscanf(cab.c_str(), "%15s %4095s", metodo, alvo);
        std::string corpo;
        std::string tl = cabValor(cab, "Content-Length");
        long long tam = tl.empty() ? 0 : _atoi64(tl.c_str());
        if (tam < 0 || tam > 65536 || !c.lerN(corpo, tam)) break;
        if (!tratar(c.s, metodo, alvo, cab, corpo)) break;
    }
    closesocket(c.s);
    return 0;
}

// Acha o PS4 na rede pelo anúncio do Hyper Neiva (SSDP) para abrir a página.
static std::string acharPs4() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return "";
    prazo(s, 700);
    sockaddr_in m = {}; m.sin_family = AF_INET; m.sin_port = htons(1900); inet_pton(AF_INET, "239.255.255.250", &m.sin_addr);
    const char* busca = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n\r\n";
    std::string achado;
    for (int tentativa = 0; tentativa < 3 && achado.empty(); tentativa++) {
        sendto(s, busca, (int)strlen(busca), 0, (sockaddr*)&m, sizeof(m));
        uint64_t fim = agoraMs() + 1500;
        while (agoraMs() < fim && achado.empty()) {
            char b[2048]; sockaddr_in de; int ld = sizeof(de);
            int n = recvfrom(s, b, sizeof(b) - 1, 0, (sockaddr*)&de, &ld);
            if (n <= 0) continue;
            b[n] = 0;
            if (strstr(b, "HyperNeiva")) { char ip[32]; inet_ntop(AF_INET, &de.sin_addr, ip, sizeof(ip)); achado = ip; }
        }
    }
    closesocket(s);
    return achado;
}

// ---------------------------------------------------------------- iniciar com o Windows
// Como o AnyDesk: fica rodando escondido desde que o Windows liga, e o PS4 controla o PC sem
// ninguém abrir o programa. Fica na pasta do usuário e na chave Run do usuário (sem admin).
static const wchar_t* CHAVE_RUN = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static std::wstring pastaInstalada() {
    wchar_t local[MAX_PATH] = L"";
    GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    return std::wstring(local) + L"\\HyperNeivaPC";
}
static void fecharOutrasCopias() {
    DWORD eu = GetCurrentProcessId();
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W e = {}; e.dwSize = sizeof(e);
    for (BOOL ok = Process32FirstW(h, &e); ok; ok = Process32NextW(h, &e)) {
        if (e.th32ProcessID == eu || _wcsicmp(e.szExeFile, L"HyperNeivaPC.exe") != 0) continue;
        HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, e.th32ProcessID);
        if (p) { TerminateProcess(p, 0); CloseHandle(p); }
    }
    CloseHandle(h);
}
static int instalar(const std::string& ps4) {
    wchar_t meu[MAX_PATH]; GetModuleFileNameW(NULL, meu, MAX_PATH);
    std::wstring pasta = pastaInstalada(), destino = pasta + L"\\HyperNeivaPC.exe";
    CreateDirectoryW(pasta.c_str(), NULL);
    fecharOutrasCopias();
    Sleep(500);
    if (_wcsicmp(meu, destino.c_str()) != 0 && !CopyFileW(meu, destino.c_str(), FALSE)) { printf("Nao consegui copiar para %s\n", utf8(destino).c_str()); return 1; }
    std::wstring argumentos = L"--oculto" + (ps4.empty() ? std::wstring() : L" " + largo(ps4));
    std::wstring comando = L"\"" + destino + L"\" " + argumentos;
    LONG r = RegSetKeyValueW(HKEY_CURRENT_USER, CHAVE_RUN, L"HyperNeivaPC", REG_SZ, comando.c_str(), (DWORD)((comando.size() + 1) * sizeof(wchar_t)));
    if (r != ERROR_SUCCESS) { printf("Nao consegui registrar no inicio do Windows (erro %ld)\n", r); return 1; }
    ShellExecuteW(NULL, L"open", destino.c_str(), argumentos.c_str(), pasta.c_str(), SW_HIDE);
    printf("Pronto: o Hyper Neiva PC agora inicia com o Windows, sem janela.\n"
           "A tela de FTP do PS4 ja pode ver e controlar este PC com o programa fechado.\n"
           "Para desfazer: HyperNeivaPC.exe --desinstalar\n");
    return 0;
}
static int desinstalar() {
    RegDeleteKeyValueW(HKEY_CURRENT_USER, CHAVE_RUN, L"HyperNeivaPC");
    fecharOutrasCopias();
    printf("O Hyper Neiva PC nao inicia mais com o Windows, e a copia escondida foi fechada.\n");
    return 0;
}

// ---------------------------------------------------------------- tokens permanentes
// HyperNeivaPC.exe --token dropbox   (ou google): abre a autorização no navegador, troca o
// código pelo refresh token (não expira) e grava direto em Chaves de API no PS4.
static int httpsPost(const wchar_t* host, const wchar_t* caminho, const std::string& corpo, std::string& resp) {
    resp.clear();
    HINTERNET ses = WinHttpOpen(L"HyperNeivaPC", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return -1;
    HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"POST", caminho, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
    DWORD st = 0, tam = sizeof(st);
    if (req && WinHttpSendRequest(req, L"Content-Type: application/x-www-form-urlencoded\r\n", (DWORD)-1L, (LPVOID)corpo.data(), (DWORD)corpo.size(), (DWORD)corpo.size(), 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &st, &tam, WINHTTP_NO_HEADER_INDEX);
        char b[8192]; DWORD n = 0;
        while (WinHttpReadData(req, b, sizeof(b), &n) && n) resp.append(b, n);
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return (int)st;
}
static std::string jsonValor(const std::string& j, const char* chave) {
    size_t i = j.find(std::string("\"") + chave + "\"");
    if (i == std::string::npos) return "";
    i = j.find(':', i); if (i == std::string::npos) return "";
    i = j.find('"', i); if (i == std::string::npos) return "";
    size_t f = j.find('"', i + 1);
    return f == std::string::npos ? "" : j.substr(i + 1, f - i - 1);
}
static std::string perguntar(const char* texto) {
    printf("%s", texto); fflush(stdout);
    char l[1024] = ""; if (!fgets(l, sizeof(l), stdin)) return "";
    std::string r = l;
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r' || r.back() == ' ')) r.pop_back();
    while (!r.empty() && r[0] == ' ') r.erase(0, 1);
    return r;
}
// Google com cliente "App para computador": o código volta sozinho para 127.0.0.1.
static std::string esperarCodigoGoogle(SOCKET srv) {
    for (;;) {
        SOCKET c = accept(srv, NULL, NULL);
        if (c == INVALID_SOCKET) return "";
        char b[4096]; int n = recv(c, b, sizeof(b) - 1, 0);
        std::string ped = n > 0 ? std::string(b, n) : "";
        size_t i = ped.find("code=");
        std::string codigo;
        if (i != std::string::npos) { size_t f = ped.find_first_of("& ", i); codigo = ped.substr(i + 5, f - i - 5); }
        const char* html = codigo.empty() ? "Nao veio o codigo. Volte para a janela do Hyper Neiva PC." : "Pronto! Pode fechar esta aba e voltar para a janela do Hyper Neiva PC.";
        std::string r = std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\nContent-Length: ") + std::to_string(strlen(html)) + "\r\n\r\n" + html;
        send(c, r.data(), (int)r.size(), 0);
        closesocket(c);
        if (!codigo.empty()) {
            std::string d; for (size_t k = 0; k < codigo.size(); k++) { if (codigo[k] == '%' && k + 2 < codigo.size()) { d += (char)strtol(codigo.substr(k + 1, 2).c_str(), NULL, 16); k += 2; } else d += codigo[k]; }
            return d;
        }
    }
}
static int gerarToken(const std::string& servico, std::string ps4) {
    printf("\n== Token permanente: %s ==\n", servico.c_str());
    std::string refresh, idCampo, segredoCampo, refreshCampo, id, segredo;
    if (servico == "dropbox") {
        printf("1. Em dropbox.com/developers/apps abra o seu app (Scoped access, Full Dropbox,\n   permissoes files.content.read e files.content.write marcadas).\n");
        id = perguntar("2. Cole o App key: ");
        segredo = perguntar("3. Cole o App secret: ");
        if (id.empty() || segredo.empty()) return 1;
        std::string url = "https://www.dropbox.com/oauth2/authorize?client_id=" + urlCodificar(id) + "&token_access_type=offline&response_type=code";
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        printf("4. Abri o navegador. Clique em Continuar e Permitir.\n   (Se nao abriu: %s )\n", url.c_str());
        std::string codigo = perguntar("5. Cole aqui o codigo que o Dropbox mostrou: ");
        std::string resp;
        int st = httpsPost(L"api.dropboxapi.com", L"/oauth2/token", "code=" + urlCodificar(codigo) + "&grant_type=authorization_code&client_id=" + urlCodificar(id) + "&client_secret=" + urlCodificar(segredo), resp);
        refresh = jsonValor(resp, "refresh_token");
        if (refresh.empty()) { printf("O Dropbox recusou (HTTP %d): %s\n", st, resp.c_str()); return 1; }
        idCampo = "dropbox_appkey"; segredoCampo = "dropbox_secret"; refreshCampo = "dropbox_token";
    }
    else if (servico == "google") {
        printf("1. No console.cloud.google.com: crie um projeto, ative a Google Drive API e, em\n   Credenciais, crie um ID do cliente OAuth do tipo \"App para computador\".\n"
               "   Na tela de consentimento, publique o app (em producao) para o token nao vencer em 7 dias.\n");
        id = perguntar("2. Cole o ID do cliente: ");
        segredo = perguntar("3. Cole a Chave secreta do cliente: ");
        if (id.empty() || segredo.empty()) return 1;
        SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(srv, (sockaddr*)&a, sizeof(a)) != 0 || listen(srv, 4) != 0) { printf("Nao consegui abrir uma porta local.\n"); return 1; }
        int tl = sizeof(a); getsockname(srv, (sockaddr*)&a, &tl);
        std::string volta = "http://127.0.0.1:" + std::to_string(ntohs(a.sin_port));
        std::string url = "https://accounts.google.com/o/oauth2/v2/auth?client_id=" + urlCodificar(id) + "&redirect_uri=" + urlCodificar(volta) +
            "&response_type=code&scope=" + urlCodificar("https://www.googleapis.com/auth/drive") + "&access_type=offline&prompt=consent";
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        printf("4. Abri o navegador: entre na sua conta e permita o acesso (em app de teste, clique em Continuar).\n   Esperando a resposta do Google...\n");
        std::string codigo = esperarCodigoGoogle(srv);
        closesocket(srv);
        std::string resp;
        int st = httpsPost(L"oauth2.googleapis.com", L"/token", "code=" + urlCodificar(codigo) + "&client_id=" + urlCodificar(id) + "&client_secret=" + urlCodificar(segredo) +
            "&redirect_uri=" + urlCodificar(volta) + "&grant_type=authorization_code", resp);
        refresh = jsonValor(resp, "refresh_token");
        if (refresh.empty()) { printf("O Google recusou (HTTP %d): %s\n", st, resp.c_str()); return 1; }
        idCampo = "gdrive_id"; segredoCampo = "gdrive_secret"; refreshCampo = "gdrive_refresh";
    }
    else { printf("Use: HyperNeivaPC.exe --token dropbox   ou   --token google\n"); return 1; }

    printf("Token permanente gerado.\n");
    if (ps4.empty()) { printf("Procurando o PS4 na rede...\n"); ps4 = acharPs4(); }
    if (ps4.empty()) ps4 = perguntar("IP do PS4 (com o Hyper Neiva aberto): ");
    std::string end = ps4.find(':') == std::string::npos ? ps4 + ":8000" : ps4;
    bool ok = ps4Post(end, "/chaves/salvar?id=" + idCampo, id) && ps4Post(end, "/chaves/salvar?id=" + segredoCampo, segredo) && ps4Post(end, "/chaves/salvar?id=" + refreshCampo, refresh);
    if (ok) printf("Gravado no PS4 (Extra > Chaves de API). Nao precisa fazer mais nada.\n");
    else {
        // Sem PS4: grava num txt para importar depois.
        FILE* f = fopen("tokens_hyperneiva.txt", "a");
        if (f) { fprintf(f, "%s=%s\n%s=%s\n%s=%s\n", idCampo.c_str(), id.c_str(), segredoCampo.c_str(), segredo.c_str(), refreshCampo.c_str(), refresh.c_str()); fclose(f); }
        printf("Nao achei o PS4. Salvei em tokens_hyperneiva.txt nesta pasta: copie os valores para Extra > Chaves de API.\n");
    }
    perguntar("Aperte Enter para sair.");
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    SetProcessDPIAware();
    setvbuf(stdout, NULL, _IONBF, 0);
    SetConsoleOutputCP(CP_UTF8);
    InitializeCriticalSection(&trava);
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    avisoTarefa = CreateEvent(NULL, FALSE, FALSE, NULL);
    printf("Hyper Neiva PC %d\n", VERSAO);

    bool semBusca = false, semNavegador = false, oculto = false, querInstalar = false, querDesinstalar = false; std::string ps4, tokenServico;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sem-busca")) semBusca = true;
        else if (!strcmp(argv[i], "--sem-navegador")) semNavegador = true;
        else if (!strcmp(argv[i], "--oculto")) oculto = true;
        else if (!strcmp(argv[i], "--instalar")) querInstalar = true;
        else if (!strcmp(argv[i], "--desinstalar")) querDesinstalar = true;
        else if (!strcmp(argv[i], "--token") && i + 1 < argc) tokenServico = argv[++i];
        else ps4 = argv[i];
    }
    if (querInstalar) return instalar(ps4);
    if (querDesinstalar) return desinstalar();
    if (!tokenServico.empty()) return gerarToken(tokenServico, ps4);
    if (oculto) {
        // Sem janela e sem perguntas: acha o PS4 sozinho (e de novo, se o IP mudar).
        FreeConsole();
        semNavegador = true;
    }

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons(PORTA); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    BOOL exclusivo = TRUE; setsockopt(srv, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusivo, sizeof(exclusivo));
    bool ouvindo = bind(srv, (sockaddr*)&a, sizeof(a)) == 0 && listen(srv, 16) == 0;
    if (!ouvindo) printf("A porta %d ja esta em uso: o Hyper Neiva PC ja deve estar aberto.\n", PORTA);
    else {
        CreateThread(NULL, 0, threadCopias, NULL, 0, NULL);
        printf("Pronto. Deixe esta janela aberta enquanto usa a pagina do PS4.\n");
    }

    if (oculto) {
        if (ps4.empty()) ps4 = acharPs4();
    }
    else if (!semBusca) {
        if (ps4.empty()) { printf("Procurando o PS4 na rede...\n"); ps4 = acharPs4(); if (!ps4.empty()) printf("PS4 encontrado.\n"); }
        if (ps4.empty()) {
            // Quase sempre é 192.168.1.101: Enter aceita, ou digita só o final (ex.: 102).
            printf("Nao achei o PS4 sozinho. IP do PS4 [192.168.1.101]: ");
            char linha[64] = "";
            if (!fgets(linha, sizeof(linha), stdin)) linha[0] = 0;
            std::string r = linha;
            while (!r.empty() && (r.back() == '\n' || r.back() == '\r' || r.back() == ' ')) r.pop_back();
            while (!r.empty() && r[0] == ' ') r.erase(0, 1);
            if (r.empty()) ps4 = "192.168.1.101";
            else if (r.find('.') == std::string::npos) ps4 = "192.168.1." + r;
            else ps4 = r;
        }
        std::string url = "http://" + ps4 + (ps4.find(':') == std::string::npos ? ":8000" : "");
        printf("\nPagina do PS4: %s\n", url.c_str());
        if (!semNavegador) ShellExecuteA(NULL, "open", (url + "/").c_str(), NULL, NULL, SW_SHOWNORMAL);
        // QR para abrir no celular: quadrados claros sobre o fundo escuro do console.
        uint8_t m[QR_MAX_LADO * QR_MAX_LADO];
        int lado = qrGerar(url.c_str(), m);
        if (lado) {
            printf("Aponte a camera do celular:\n\n");
            auto claro = [&](int x, int y) { return !(x >= 0 && y >= 0 && x < lado && y < lado && m[y * lado + x]); };
            for (int y = -2; y < lado + 2; y += 2) {
                std::string linha2 = "  ";
                for (int x = -2; x < lado + 2; x++) {
                    bool a = claro(x, y), b = y + 1 < lado + 2 ? claro(x, y + 1) : false;
                    linha2 += a && b ? "\xE2\x96\x88" : a ? "\xE2\x96\x80" : b ? "\xE2\x96\x84" : " ";
                }
                printf("%s\n", linha2.c_str());
            }
            printf("\n");
        }
    }
    if (!ouvindo) { Sleep(5000); return 1; }
    if (!ps4.empty() || oculto) {
        if (!ps4.empty()) ps4Endereco = ps4.find(':') == std::string::npos ? ps4 + ":8000" : ps4;
        CreateThread(NULL, 0, threadPedidosPs4, NULL, 0, NULL);
    }
    if (!oculto) printf("Dica: para o PS4 controlar este PC mesmo com esta janela fechada, rode HyperNeivaPC.exe --instalar\n");

    for (;;) {
        SOCKET c = accept(srv, NULL, NULL);
        if (c == INVALID_SOCKET) { Sleep(100); continue; }
        HANDLE t = CreateThread(NULL, 0, threadConexao, (LPVOID)(uintptr_t)c, 0, NULL);
        if (t) CloseHandle(t); else closesocket(c);
    }
}
