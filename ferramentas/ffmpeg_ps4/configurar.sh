#!/bin/bash
# FFmpeg 5.0 para o PS4: a MESMA configuração do pacote do OpenOrbis (pacbrew), que o player usa,
# com exportação ligada para o editor de vídeo (codificadores MPEG-4/AAC/MJPEG/PNG/PCM e saídas
# MP4/MOV/MKV/ADTS/WAV). Rodar no bash do MSYS2: bash configurar.sh
set -e
R=/c/Users/Marco/source/repos/teste3/ffmpeg_ps4
export PATH="/usr/bin:/c/Program Files/LLVM/bin:$PATH"
export PKG_CONFIG_LIBDIR=$R/pkgconfig
unset PKG_CONFIG_PATH PKG_CONFIG_SYSTEM_INCLUDE_PATH PKG_CONFIG_SYSTEM_LIBRARY_PATH
export PKG_CONFIG_PATH=   # só os .pc do PS4 (o MSYS2 punha os de /mingw64)
mkdir -p $R/build
cd $R/build
../src/ffmpeg-5.0/configure \
  --prefix=$R/instalado \
  --enable-cross-compile --arch=x86_64 --target-os=freebsd \
  --cc=$R/ferramentas/ps4cc --ld=$R/ferramentas/ps4cc \
  --nm=llvm-nm --ar=llvm-ar --ranlib=llvm-ranlib --strip=llvm-strip \
  --pkg-config=pkg-config \
  --disable-shared --enable-static --enable-pic \
  --disable-programs --disable-debug --disable-doc --disable-avdevice --disable-hwaccels \
  --disable-encoders --enable-encoder=mpeg4,aac,mjpeg,png,pcm_s16le \
  --disable-muxers --enable-muxer=mp4,mov,matroska,ipod,adts,wav \
  --enable-swscale --enable-swresample --enable-network \
  --disable-protocols --enable-protocol=file,http,ftp,tcp,udp,rtmp \
  --enable-zlib --enable-bzlib --enable-libass --enable-libfreetype --enable-libfribidi \
  --x86asmexe=nasm
