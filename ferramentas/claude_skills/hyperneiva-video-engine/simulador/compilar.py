"""Compila o simulador usando o video.cpp real do projeto, sem nenhuma alteracao de logica."""
import os
import shutil
import subprocess
import sys

PROJETO = r"C:\Users\Marco\source\repos\teste3\teste3"
GPP = r"C:\msys64\mingw64\bin\g++.exe"
os.environ["PATH"] = r"C:\msys64\mingw64\bin;" + os.environ["PATH"]

fonte = open(os.path.join(PROJETO, "video.cpp"), encoding="utf-8").read()
# Unica troca: o log vai para uma pasta local em vez de /data/HyperNeiva.
fonte = fonte.replace('"/data/HyperNeiva/video_debug_log.txt"', '"out/video_debug_log.txt"')
fonte = fonte.replace('"/data/HyperNeiva/video_debug_log.old.txt"', '"out/video_debug_log.old.txt"')
open("video_sut.cpp", "w", encoding="utf-8").write(fonte)
shutil.copy(os.path.join(PROJETO, "video.h"), "video.h")
shutil.copy(os.path.join(PROJETO, "menu_video.cpp"), "menu_video_sut.cpp")

opcoes = sys.argv[1:] or ["-O2"]
cmd = [GPP, "-std=gnu++17", *opcoes, "-g", "-msse4.1", "-mavx", "-ffast-math", "-Istubs", "-I.", "-I" + PROJETO, "-IC:/msys64/mingw64/include",
       "video_sut.cpp", "menu_video_sut.cpp", "test_motor.cpp", "-o", "simulador.exe", "-LC:/msys64/mingw64/lib",
       "-lavformat", "-lavcodec", "-lswscale", "-lswresample", "-lavutil", "-lpthread", "-lwinmm", "-lpsapi"]
r = subprocess.run(cmd, capture_output=True, text=True)
print(r.stdout[-3000:])
print(r.stderr[-5000:])
print("compilou" if r.returncode == 0 else "FALHOU")
sys.exit(r.returncode)
