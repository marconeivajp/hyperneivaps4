import os
import subprocess

FF = r"C:\msys64\mingw64\bin\ffmpeg.exe"
os.environ["PATH"] = r"C:\msys64\mingw64\bin;" + os.environ["PATH"]
os.makedirs("media", exist_ok=True)


def video(tamanho, fps, duracao):
    # Flash branco de 0,1 s a cada segundo inteiro: casa com o bipe do audio para medir sincronia.
    return (
        f"color=c=0x202020:s={tamanho}:r={fps}:d={duracao},"
        "drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='lt(mod(t,1),0.1)',"
        "drawgrid=w=iw/8:h=ih/8:t=2:c=0x808080,"
        "drawbox=x=0:y=0:w=iw/8:h=ih/8:color=red:t=fill,"
        "drawbox=x=iw*7/8:y=0:w=iw/8:h=ih/8:color=green:t=fill,"
        "drawbox=x=0:y=ih*7/8:w=iw/8:h=ih/8:color=blue:t=fill"
    )


def audio(duracao):
    return f"aevalsrc='if(lt(mod(t,1),0.1),0.8*sin(2*PI*1000*t),0)':s=48000:d={duracao}"


def gerar(nome, tamanho, fps, duracao, *opcoes):
    cmd = [FF, "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", video(tamanho, fps, duracao),
           "-f", "lavfi", "-i", audio(duracao), *opcoes, f"media/{nome}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    print(nome, "ok" if r.returncode == 0 else "ERRO " + r.stderr[-400:])


x264 = ["-c:v", "libx264", "-preset", "veryfast"]
gerar("sync720.mp4", "1280x720", "24000/1001", 60, *x264, "-bf", "3", "-g", "48", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "128k")
gerar("sync1080p60.mp4", "1920x1080", "60", 12, *x264, "-bf", "2", "-g", "60", "-pix_fmt", "yuv420p", "-c:a", "aac")
gerar("sync4k.mp4", "3840x2160", "30", 16, "-c:v", "libx264", "-preset", "ultrafast", "-pix_fmt", "yuv420p", "-c:a", "aac")
gerar("sd43.mp4", "640x480", "25", 8, *x264, "-pix_fmt", "yuv420p", "-c:a", "aac")
gerar("impar.mp4", "854x478", "24", 8, *x264, "-pix_fmt", "yuv420p", "-c:a", "aac")
gerar("largo.mp4", "1920x800", "24", 8, *x264, "-pix_fmt", "yuv420p", "-c:a", "aac")
gerar("fullrange.mp4", "1280x720", "24", 8, *x264, "-pix_fmt", "yuvj420p", "-c:a", "aac")
gerar("hevc10.mkv", "1280x720", "24", 8, "-c:v", "libx265", "-preset", "ultrafast", "-x265-params", "log-level=error", "-pix_fmt", "yuv420p10le", "-c:a", "aac")
gerar("mpeg4.avi", "720x576", "25", 8, "-c:v", "mpeg4", "-q:v", "4", "-pix_fmt", "yuv420p", "-c:a", "libmp3lame")
r = subprocess.run([FF, "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", "testsrc2=s=1280x720:r=24:d=8",
                    "-c:v", "libx264", "-preset", "veryfast", "-pix_fmt", "yuv420p", "media/semaudio.mp4"], capture_output=True, text=True)
print("semaudio.mp4", "ok" if r.returncode == 0 else "ERRO " + r.stderr[-400:])

r = subprocess.run([FF, "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", "testsrc2=s=1920x1080:r=24:d=20,noise=alls=25:allf=t",
                    "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:d=20", "-c:v", "libx264", "-preset", "veryfast", "-b:v", "8M",
                    "-maxrate", "10M", "-bufsize", "16M", "-bf", "3", "-pix_fmt", "yuv420p", "-c:a", "aac", "media/pesado1080.mp4"], capture_output=True, text=True)
print("pesado1080.mp4", "ok" if r.returncode == 0 else "ERRO " + r.stderr[-400:])