// Locale "C" para os núcleos .prx, sem tocar em %fs.
// A musl estática lê o locale da thread em __pthread_self() (= %fs:0). No PS4, %fs:0 é a
// thread do sistema, não a da musl: a leitura dá lixo (crash no SNES/Atari) e uselocale
// escreve por cima da thread do sistema (o DS chegou a desligar o console).
// Estas versões ficam antes da libc.a no link e substituem as da musl.
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

#define HN_EXPORTA __attribute__((used))

static locale_t localeAtual = LC_GLOBAL_LOCALE;

HN_EXPORTA size_t __ctype_get_mb_cur_max(void) { return 1; }

HN_EXPORTA locale_t uselocale(locale_t novo) {
    locale_t antigo = localeAtual;
    if (novo) localeAtual = novo;
    return antigo;
}

// No locale "C" cada byte é um caractere.
HN_EXPORTA size_t mbrtowc(wchar_t* restrict wc, const char* restrict s, size_t n, mbstate_t* restrict st) {
    (void)st;
    if (!s) return 0;
    if (!n) return (size_t)-2;
    unsigned char c = (unsigned char)*s;
    if (wc) *wc = c;
    return c ? 1 : 0;
}

HN_EXPORTA int mbtowc(wchar_t* restrict wc, const char* restrict s, size_t n) {
    if (!s) return 0;
    if (!n) return -1;
    unsigned char c = (unsigned char)*s;
    if (wc) *wc = c;
    return c ? 1 : 0;
}

HN_EXPORTA size_t mbsrtowcs(wchar_t* restrict ws, const char** restrict src, size_t n, mbstate_t* restrict st) {
    (void)st;
    const unsigned char* s = (const unsigned char*)*src;
    if (!ws) return strlen((const char*)s);
    size_t i = 0;
    for (; i < n; i++) {
        ws[i] = s[i];
        if (!s[i]) { *src = NULL; return i; }
    }
    *src = (const char*)(s + i);
    return i;
}

HN_EXPORTA size_t wcrtomb(char* restrict s, wchar_t wc, mbstate_t* restrict st) {
    (void)st;
    if (!s) return 1;
    if ((unsigned)wc > 255) { errno = EILSEQ; return (size_t)-1; }
    *s = (char)wc;
    return 1;
}

HN_EXPORTA wint_t btowc(int c) { return c == EOF ? WEOF : (wint_t)(unsigned char)c; }
HN_EXPORTA int wctob(wint_t wc) { return wc < 256 ? (int)wc : EOF; }

HN_EXPORTA int strcoll(const char* a, const char* b) { return strcmp(a, b); }
HN_EXPORTA int wcscoll(const wchar_t* a, const wchar_t* b) { return wcscmp(a, b); }

HN_EXPORTA size_t strxfrm(char* restrict d, const char* restrict s, size_t n) {
    size_t t = strlen(s);
    if (n > t) memcpy(d, s, t + 1);
    return t;
}

HN_EXPORTA size_t wcsxfrm(wchar_t* restrict d, const wchar_t* restrict s, size_t n) {
    size_t t = wcslen(s);
    if (n > t) wmemcpy(d, s, t + 1);
    return t;
}

HN_EXPORTA char* nl_langinfo(nl_item item) {
    if (item == CODESET) return (char*)"ASCII";
    if (item == RADIXCHAR) return (char*)".";
    return (char*)"";
}

HN_EXPORTA char* strerror(int e) {
    static char texto[32];
    snprintf(texto, sizeof(texto), "erro %d", e);
    return texto;
}

// strftime só com o essencial (números), sem nomes de mês ou dia, que dependem de locale.
HN_EXPORTA size_t strftime(char* restrict s, size_t n, const char* restrict f, const struct tm* restrict tm) {
    size_t k = 0;
    for (; *f && k + 1 < n; f++) {
        if (*f != '%' || !f[1]) { s[k++] = *f; continue; }
        f++;
        char tmp[24];
        switch (*f) {
            case 'Y': snprintf(tmp, sizeof(tmp), "%d", tm->tm_year + 1900); break;
            case 'm': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mon + 1); break;
            case 'd': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mday); break;
            case 'H': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_hour); break;
            case 'M': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_min); break;
            case 'S': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_sec); break;
            case '%': tmp[0] = '%'; tmp[1] = 0; break;
            default: tmp[0] = 0; break;
        }
        for (char* t = tmp; *t && k + 1 < n; t++) s[k++] = *t;
    }
    if (!n) return 0;
    s[k] = 0;
    return *f ? 0 : k;
}
