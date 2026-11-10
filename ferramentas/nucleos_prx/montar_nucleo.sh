#!/bin/bash
# Uso: montar_nucleo.sh nome arquivo.a [extras...]  -> nome_libretro.prx
B="/c/Program Files/LLVM/bin"; N="$1"; A="$2"; shift 2
U=""; for n in init deinit api_version get_system_info get_system_av_info set_environment set_video_refresh set_audio_sample set_audio_sample_batch set_input_poll set_input_state set_controller_port_device reset run serialize_size serialize unserialize cheat_reset cheat_set load_game unload_game get_region get_memory_data get_memory_size; do U="$U --undefined=retro_$n"; done
rm -f "${N}_libretro.elf" "${N}_libretro.oelf" "${N}_libretro.prx"
"$B/ld.lld.exe" -m elf_x86_64 -pie --script C:/OpenOrbis/link.x --eh-frame-hdr -e module_start --allow-multiple-definition "--export-dynamic-symbol=retro_*" --export-dynamic-symbol=hn_definir_alocador -o "${N}_libretro.elf" -L C:/OpenOrbis/lib crt_nucleo.o crt_alocador.o crt_locale.o $U "$A" "$@" -lc -lkernel -lc++ 2>&1 | grep -v "entry symbol" | head -8
[ -f "${N}_libretro.elf" ] || { echo "FALHOU $N"; exit 1; }
DEF=$("$B/llvm-objdump" -T "${N}_libretro.elf" | grep " retro_" | grep -vc "\*UND\*")
TR=$("$B/llvm-objdump" -p "${N}_libretro.elf" | grep -c TEXTREL)
NE=$("$B/llvm-objdump" -p "${N}_libretro.elf" | grep NEEDED | awk '{print $2}' | tr '\n' ',')
D=$(cygpath -m "$PWD"); /c/OpenOrbis/bin/windows/create-fself.exe -in "$D/${N}_libretro.elf" --out "$D/${N}_libretro.oelf" --lib "$D/${N}_libretro.prx" --paid 0x3800000000000011 > "fself_${N}.txt" 2>&1
echo "$N retro=$DEF textrel=$TR needed=$NE prx=$(stat -c%s ${N}_libretro.prx 2>/dev/null)"
