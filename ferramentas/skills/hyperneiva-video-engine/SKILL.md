---
name: hyperneiva-video-engine
description: Conhecimento do player de vídeo do Hyper Neiva no PS4 (video.cpp, pipeline multi-thread com FFmpeg 5.0, apresentador no vsync, PiP, sincronia de áudio) e simulador no PC para testá-lo sem o console. Use sempre que o pedido envolver vídeo travando, FPS, PiP, tela cheia, áudio dessincronizado, pausa, busca, swscale, threads do FFmpeg, video_debug_log.txt, ou qualquer alteração em video.cpp, menu_video.cpp ou na apresentação em graphics.cpp, mesmo sem citar "motor de vídeo".
---

# Motor de vídeo do Hyper Neiva

Para compilar e enviar, use também a skill `hyperneiva-ps4-build`.

## Como funciona (versão 2.24)

Cada etapa tem sua thread, para uma etapa lenta não travar as outras:

```
demux -> filaVideo -> decodificação (4-5 threads FFmpeg) -> filaFrames -> apresentador -> tela
      -> filaAudio -> áudio -> anel -> misturarAudioVideo (thread de som do app)
```

- **Áudio é o relógio mestre.** Vídeo atrasado descarta quadros, nunca vira câmera lenta.
- **Apresentador (`passoApresentador`)**: thread própria, prioridade 400, ritmada pelo fim do flip. Sem nada novo, repete o flip do buffer atual para manter o ritmo do vsync.
- **Tela cheia**: `main.cpp` não desenha o menu; só lê o controle a cada 33 ms. Esse tempo preserva a velocidade dos botões segurados do menu do player.
- **PiP**: a interface vai para uma camada em RAM (triplo buffer em `graphics.cpp`) e o apresentador compõe por cima. A interface pode levar o tempo que for sem atrasar o vídeo.
- **Saída**: dois buffers próprios `WC_GARLIC`, registrados nos índices 2 e 3 do VideoOut. Se falhar, usa os buffers do menu.
- **Conversão em duas etapas**: `sws` em YUV para redimensionar, depois YUV420 para BGRA pelo caminho SIMD, com 3 threads. É bem mais rápido que escalar direto para BGRA.
- **Conversão adiantada**: logo após o flip, o próximo quadro já é convertido em `imagens[1]`, deixando só a cópia para a hora de exibir.
- **Serial**: toda busca ou repetição incrementa `serialAtual`, e tudo com serial antigo é descartado.
- **Qualidade adaptativa (nível 0 a 4)**: só reduz qualidade se atrasar e volta sozinha. Nível 3 corta quadros B e derruba muito o FPS.

## Controles e interface do player (2.26)

Ficam em `menu_video.cpp`. Em tela cheia:

| Entrada | Ação |
|---|---|
| R1 ou L1 duas vezes | Próximo ou anterior, sem dar a volta na lista |
| Segurar R2 | 2x enquanto segurar |
| R2 duas vezes, depois toques seguidos | +10 s por toque |
| L2 duas vezes, depois toques seguidos | -10 s por toque |
| Segurar L2 | Cursor volta 5 s por segundo e busca ao soltar: buscas repetidas não mostrariam imagem |
| X | Pausa. Com foco nos ícones, ativa o ícone |
| Setas ou analógico esquerdo | Mostram a barra. Esquerda e direita só movem o cursor: X confirma, Bolinha cancela. Descer aos ícones ou deixar a barra sumir também cancela |
| Baixo na barra | Foco nos ícones: anterior, -10, play, +10, próximo, volume, mudo, tela pequena, três pontos |
| Bolinha | Cancela o cursor, depois fecha a barra. Sem barra, fecha o vídeo |

- `processarControlesTelaCheia` roda por último em `processarControlesMenuVideo`, senão o X usado pelo menu viraria pausa.
- A barra é montada em RAM por `desenharFaixaControles` e copiada para a tela: ler da tela WC é muito lento. Nunca faça transparência lendo o buffer de saída.
- `reiniciarControlesPlayer` devolve a velocidade se o vídeo fechar com R2 segurado.

## Ícones

- `icones_embutidos.h` guarda PNGs dentro do eboot, gerados por `python tools/gerar_icones.py`. Isso evita depender de `/app0/assets/icons`, que não entra no GP4 (o `build.bat` só lista images, fonts e audio).
- Prefixos: `p_` player, `m_` menu, `f_` tipo de arquivo. `assets/icons/folder.png` é uma imagem de personagem, não uma pasta; a pasta é um SVG do gerador.
- A escolha do ícone é `iconeDoItem` em `menu_grafico_layout.cpp`. Palavras mais específicas vêm primeiro na tabela.
- Só o estilo de lista desenha ícones. Grade e roleta mostram capa ou texto.

## Decisões que não devem ser desfeitas

| Regra | Por quê |
|---|---|
| Sem novo `.cpp` | A lista de módulos está no `build.bat`, que não pode mudar |
| Nada de timeout fixo no fim do arquivo | O demux lê cerca de 40 s adiantado; o limite só conta se a reprodução parar de avançar. Um limite fixo de 6 s fez o vídeo reiniciar antes de acabar |
| Áudio só libera depois do primeiro quadro | Fica alinhado com a imagem; ao abrir ou buscar, o relógio pula direto para o tempo do áudio |
| Áudio não pode travar o demux antes do primeiro quadro | Seria um impasse: pacote de áudio excedente é descartado nessa fase |
| Ao voltar a velocidade para 1x, fazer seek e zerar o nível | Fora de 1x o som fica para trás, e o atraso de antes não diz nada sobre o arquivo |
| Fatia de conversão com altura par (`threadsComFatiaPar`) | Fatia ímpar pega o croma da linha errada no FFmpeg 5.0 |
| `sws_flags = SWS_BILINEAR`, sem `FAST_BILINEAR` | FAST_BILINEAR depende de memória executável e falha com erro, sem alternativa |
| Sem `SWS_ACCURATE_RND` | Ele desliga o caminho SIMD de YUV para RGB |
| Geometria com inteiros | `float` transformava 1920 em 1918 e perdia o caminho sem escala |
| Camadas de UI nunca são liberadas | `atualizarBarra` pode desenhar nelas de threads de download quando o vídeo fecha |
| `verificarTrocaDeVideo()` antes de pegar o buffer | Ela reinicia o motor e trocaria o destino do desenho |
| Leituras de outra thread via `lerCompartilhado` | Com LTO, o otimizador pode fixar o valor no laço |
| Thread de som com prioridade 300 | Blocos de 5 ms falhariam com os núcleos ocupados pelo vídeo |

## Fatos comprovados

- **Multi-thread por quadro funciona no PS4.** Os congelamentos de outubro de 2026 vieram do relógio preso no PTS do áudio, não das threads: congelava com 4 threads e também com 1.
- **`libavcodec.a`** do OpenOrbis é FFmpeg 5.0 (`avcodec 59.18`, `swscale 6.4`), com assembly SSE/AVX, sem hwaccel. A API de canais é a antiga (`channel_layout`).
- **Medido no PS4, 720p 23,98 fps em tela cheia:** 24,0 FPS, Drop 0, Underrun 0, Diff em torno de 0,01 s, Nivel 0, Conv 4,5 ms, Compor 2,3 ms. Antes: 8 a 13 FPS, a um terço da velocidade.
- **`sceAvPlayer`/`sceVideodec2`**: há símbolos no OpenOrbis, mas os headers não têm structs e a busca não achou exemplo público. Se 1080p pesado sofrer no console, tente isso como caminho adicional com volta automática ao software, sem substituir o atual. No PS4 base, 4K em software não é realista.

## Ler o log do console

Baixe `/data/HyperNeiva/video_debug_log.txt` por FTP. O anterior fica em `video_debug_log.old.txt`. A cada 5 s:

```
PLAYBACK: FPS:24.0 | Dec:24.0 | Drop:0 | V_Time | A_Time | Diff:0.003s | Filas V:1021 A:1821 Q:7 | Nivel:0 | Conv:4.5ms | Compor:2.3ms | Underrun:0 | Mult:1.00x
```

| Sintoma | Causa provável |
|---|---|
| `Q` perto de 0 e `Dec` abaixo do FPS do arquivo | Decodificador não dá conta |
| `Q` cheio e FPS baixo | Apresentador lento: veja `Conv` e `Compor` |
| `Conv` acima de 33 ms | Conversão pesada: confira a linha `CONVERSOR` e as threads |
| `Compor` alto | Escrita lenta na tela: buffers WC podem ter falhado |
| `Filas V` e `A` perto de 0 | Leitura lenta (USB) ou fim do arquivo |
| `Underrun` subindo | Thread de som com fome ou áudio atrasado |
| `Nivel` acima de 0 | Qualidade reduzida por atraso |
| `Dec` 0 | Decodificador sem saída, verifique `ERRO` no log |

Compare sempre `V_Time` com o tempo real: FPS alto com `V_Time` lento é câmera lenta.

## Testar antes do console

Cada teste no console custa tempo do usuário, então rode o simulador em toda mudança relevante. Ele compila o `video.cpp` real contra o FFmpeg do MSYS2, com vsync, som e loop principal simulados. Vídeos de teste têm flash e bipe no mesmo instante para medir a sincronia.

```powershell
$sim = "$env:TEMP\opencode\simulador_video"
Copy-Item "$env:USERPROFILE\.claude\skills\hyperneiva-video-engine\simulador" $sim -Recurse -Force
Set-Location $sim
New-Item -ItemType Directory -Force out | Out-Null
python gerar_midia.py    # demora vários minutos; rode em segundo plano
python compilar.py
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
.\simulador.exe completo   # também: player, formatos, estresse, pesado, lento
python testar_lista.py     # escolha de ícones e imagem out/lista.png
```

`player` roda o `menu_video.cpp` real e aperta os botões em sequência, com capturas em `out/player_*.ppm`.

`pesado` precisa de `media/pesado1080.mp4` (ruído em movimento, 1080p, 8 Mbps), que `gerar_midia.py` gera.

Como interpretar:

- `vsyncs{2:72 3:71}` é a cadência perfeita de 24 fps em 60 Hz.
- `A/V` inclui até 41 ms do próprio teste: o flash só aparece no quadro seguinte ao segundo exato.
- Este notebook tem 2 núcleos e o simulador não usa prioridade. Soluços que mudam de lugar a cada execução são ruído; repita o caso isolado antes de concluir.
- `rasgados` só vale em vídeos com flash. Em `semaudio.mp4` e `pesado1080.mp4` é falso positivo, porque os pontos medidos diferem de verdade, e isso conta como `FALHAS: 1` no modo `pesado`.
- Ao adicionar uma função de `graphics` usada por `video.cpp`, atualize `graphics.h` e `test_motor.cpp` do simulador.
- O simulador usa FFmpeg 8. Não prova buffers WC, flip real nem prioridades, e sua velocidade não prevê a do Jaguar. Isso se confirma no log do console.

Use a captura feita no flip (`salvarTela`) e confira imagens de verdade: números não mostram canal de cor trocado ou imagem deslocada.
