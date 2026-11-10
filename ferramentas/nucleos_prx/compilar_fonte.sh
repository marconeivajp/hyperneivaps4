#!/bin/bash
# Compila os fontes de um núcleo libretro para PS4 com as mesmas flags do app (build.bat),
# sem rodar o Makefile do projeto: só lê a lista de fontes do Makefile.common.
# Uso: compilar_fonte.sh nome pasta_fonte "defines extras" "includes extras"
# Saída: nucleos_ps4/<nome>_fonte_ps4.a
set -e
N="$1"; F="$2"; EXTRA_DEF="$3"; EXTRA_INC="$4"
B="/c/Program Files/LLVM/bin"
AQUI="$(cd "$(dirname "$0")" && pwd)"
OBJ="$AQUI/obj_$N"
rm -rf "$OBJ"; mkdir -p "$OBJ"
cd "$F"
BASE="--target=x86_64-pc-freebsd12-elf -fPIC -O2 -march=btver2 -funwind-tables -fno-strict-aliasing -fomit-frame-pointer -Wno-everything -D__PS4__ -D__LIBRETRO__ $EXTRA_DEF -isystem C:/OpenOrbis/include/c++/v1 -isystem C:/OpenOrbis/include $EXTRA_INC"
export BASE OBJ B
compilar() {
    f="$1"; o="$OBJ/$(echo "$f" | sed 's|^\./||; s|/|_|g; s|\.[a-z]*$|.o|')"
    case "$f" in
        *.c) "$B/clang.exe" $BASE -std=gnu11 -c "$f" -o "$o" || echo "ERRO $f" ;;
        *) "$B/clang++.exe" $BASE -std=gnu++14 -c "$f" -o "$o" || echo "ERRO $f" ;;
    esac
}
export -f compilar
cat "$AQUI/lista_$N.txt" | xargs -P 6 -I{} bash -c 'compilar "$@"' _ {} 2>&1 | grep -E "ERRO|error" | head -20
rm -f "$AQUI/${N}_fonte_ps4.a"
"$B/llvm-ar" rcs "$AQUI/${N}_fonte_ps4.a" "$OBJ"/*.o
echo "objetos: $(ls "$OBJ"/*.o | wc -l) de $(wc -l < "$AQUI/lista_$N.txt")"
