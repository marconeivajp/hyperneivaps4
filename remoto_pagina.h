// Gerado por ferramentas/remoto/gerar_pagina.py a partir de pagina.html. Não edite à mão.
#pragma once
static const char REMOTO_PAGINA[] =
R"HNPAG(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#101418">
<title>Hyper Neiva Remoto</title>
<style>
:root{--fundo:#101418;--painel:#1b2128;--borda:#2c343d;--texto:#e8edf2;--fraco:#8a96a3;--azul:#3d8bfd;--x:#7aa7ff;--o:#ff6b6b;--q:#e88ee8;--t:#4fd1a5}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%;background:var(--fundo);color:var(--texto);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
body{display:flex;flex-direction:column;overflow:hidden}
header{display:flex;align-items:center;gap:8px;padding:8px 12px;padding-top:max(8px,env(safe-area-inset-top))}
header b{font-size:15px;flex:1}
#estado{font-size:12px;color:var(--fraco)}
#estado.on{color:var(--t)}
nav{display:flex;gap:6px;padding:0 12px 8px}
nav{overflow-x:auto;scrollbar-width:none}
nav button{flex:1 0 auto;padding:10px 10px;border:1px solid var(--borda);background:var(--painel);color:var(--texto);border-radius:10px;font-size:14px}
nav button.ativo{background:var(--azul);border-color:var(--azul);color:#fff}
main{flex:1;min-height:0;position:relative}
section{position:absolute;inset:0;display:none;overflow:auto;padding:0 12px 12px}
section.ativo{display:block}
#pControle{overflow:hidden;touch-action:none;user-select:none;-webkit-user-select:none}
.pad{--e:1;position:absolute;inset:0;display:grid;grid-template-columns:1fr auto 1fr;grid-template-rows:auto 1fr;gap:8px;padding:0 10px 10px}
.gatilhos{grid-column:1/4;display:flex;justify-content:space-between;gap:8px}
.gatilhos div{display:flex;gap:8px}
.lado{display:flex;flex-direction:row;align-items:center;justify-content:space-evenly;gap:12px;min-width:0}
.lado.dir{flex-direction:row-reverse}
.meio{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:10px}
.b{border:1px solid var(--borda);background:var(--painel);color:var(--texto);border-radius:12px;display:flex;align-items:center;justify-content:center;font-weight:600;font-size:15px;touch-action:none}
.b.on{background:var(--azul);border-color:var(--azul);color:#fff}
.g{width:calc(64px*var(--e));height:calc(38px*var(--e))}
.cruz,.face{display:grid;grid-template-columns:repeat(3,var(--t1));grid-template-rows:repeat(3,var(--t1));gap:4px;--t1:calc(56px*var(--e))}
.face .b{border-radius:50%;font-size:22px}
.fx{color:var(--x)}.fo{color:var(--o)}.fq{color:var(--q)}.ft{color:var(--t)}
.c2{grid-column:2}.r1{grid-row:1}.r2{grid-row:2}.r3{grid-row:3}.c1{grid-column:1}.c3{grid-column:3}
.peq{width:calc(84px*var(--e));height:calc(34px*var(--e));font-size:12px}
.stick{width:calc(112px*var(--e));height:calc(112px*var(--e));border-radius:50%;border:1px solid var(--borda);background:radial-gradient(circle,#232a32,#171c22);position:relative;touch-action:none}
.stick i{position:absolute;left:50%;top:50%;width:44%;height:44%;margin:-22% 0 0 -22%;border-radius:50%;background:#3a4450;border:1px solid #4b5866}
.rot{font-size:11px;color:var(--fraco)}
.cartao{background:var(--painel);border:1px solid var(--borda);border-radius:12px;padding:12px;margin-bottom:12px}
.cartao h3{margin:0 0 8px;font-size:15px}
.cartao p{margin:4px 0;color:var(--fraco);font-size:13px}
.btn{display:block;width:100%;padding:14px;border:0;border-radius:10px;background:var(--azul);color:#fff;font-size:16px;font-weight:600;text-align:center}
.btn2{background:#2a323b;color:var(--texto)}
label.btn{cursor:pointer}
input[type=file]{display:none}
textarea{width:100%;min-height:110px;background:#0c0f12;color:var(--texto);border:1px solid var(--borda);border-radius:10px;padding:10px;font-size:16px;resize:vertical}
.barra{height:10px;background:#0c0f12;border-radius:5px;overflow:hidden;margin:8px 0}
.barra div{height:100%;width:0;background:var(--t);transition:width .2s}
#msgEnvio,#msgTexto{font-size:13px;color:var(--fraco);min-height:18px}
ul{list-style:none;margin:0;padding:0}
li{display:flex;align-items:center;gap:8px;padding:8px 0;border-top:1px solid var(--borda);font-size:13px}
li span{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
li button{border:1px solid var(--borda);background:#2a323b;color:var(--texto);border-radius:8px;padding:6px 10px}
.linha{display:flex;gap:8px;margin-top:8px}.linha .btn{flex:1}

/* Chaves */
.chave{display:grid;grid-template-columns:1fr auto;gap:4px 10px;padding:10px 0;border-top:1px solid var(--borda)}
.chave b{font-size:14px}.chave .uso{grid-column:1/3;font-size:12px;color:var(--fraco)}
.chave .est{font-size:12px;color:var(--fraco);text-align:right}.chave .est.ok{color:var(--t)}
.chave .linha{grid-column:1/3;display:flex;gap:6px}
.chave input,.chNova input{flex:1;min-width:0;background:#0c0f12;color:var(--texto);border:1px solid var(--borda);border-radius:8px;padding:8px;font-size:14px}
.chave button,.chNova button,#chImportar button{border:1px solid var(--borda);background:#2a323b;color:var(--texto);border-radius:8px;padding:8px 10px;font-size:13px}
.chave button.salvar{background:var(--azul);border-color:var(--azul);color:#fff}
.chNova{display:flex;gap:6px;flex-wrap:wrap}
#chImportar label{display:flex;gap:8px;align-items:center;font-size:13px;padding:5px 0}
#chMsg{font-size:13px;min-height:18px;color:var(--fraco)}
details.cat{border-top:1px solid var(--borda);padding:6px 0}
details.cat>summary{cursor:pointer;font-weight:600;font-size:15px;padding:6px 0;list-style:none;display:flex;gap:8px;align-items:center}
details.cat>summary::before{content:"▸";color:var(--fraco)}details.cat[open]>summary::before{content:"▾"}
details.cat>summary span{font-weight:400;font-size:12px;color:var(--fraco)}
details.serv{background:#151a20;border:1px solid var(--borda);border-radius:10px;margin:6px 0 6px 14px;padding:4px 10px}
details.serv>summary{cursor:pointer;display:flex;justify-content:space-between;gap:8px;padding:6px 0;font-size:14px;list-style:none}
details.serv>summary span{font-size:12px;color:var(--fraco)}details.serv>summary span.ok{color:var(--t)}
.campo{display:flex;flex-direction:column;gap:4px;padding:6px 0;border-top:1px solid #20262d}
.campo label{font-size:12px;color:var(--fraco)}
.campo .linha{margin:0}
.campo input,.campo select{flex:1;min-width:0;background:#0c0f12;color:var(--texto);border:1px solid var(--borda);border-radius:8px;padding:8px;font-size:14px}
.campo button,.serv .acoes button{border:1px solid var(--borda);background:#2a323b;color:var(--texto);border-radius:8px;padding:8px 10px;font-size:13px}
.campo button.salvar,.serv .acoes button.salvar{background:var(--azul);border-color:var(--azul);color:#fff}
.serv .acoes{display:flex;gap:6px;flex-wrap:wrap;padding:8px 0}
.emuso{background:var(--t);color:#062;border-radius:6px;padding:1px 6px;font-size:11px;margin-left:6px}
/* IA */
#pIa{display:none;flex-direction:column;gap:8px;overflow:hidden}
#pIa.ativo{display:flex}
#iaMsgs{flex:1;min-height:0;overflow:auto;background:var(--painel);border:1px solid var(--borda);border-radius:12px;padding:10px;display:flex;flex-direction:column;gap:8px}
.bolha{max-width:85%;padding:9px 12px;border-radius:14px;font-size:14px;line-height:1.4;white-space:pre-wrap;word-wrap:break-word}
.bolha.u{align-self:flex-end;background:#25466e}
.bolha.m{align-self:flex-start;background:#22302a;border:1px solid #2d4a3d}
.bolha img{display:block;max-width:100%;max-height:50vh;margin-top:6px;border-radius:8px}
.bolha small{display:block;color:var(--fraco);font-size:11px;margin-bottom:3px}
#iaStatus{font-size:13px;min-height:18px;color:var(--fraco)}
#iaStatus.ocupado{color:#ffe08a}#iaStatus.erro{color:var(--o)}
.iaEntrada{display:flex;gap:8px;align-items:flex-end}
.iaEntrada textarea{min-height:46px;height:46px;flex:1}
.iaBotoes{display:flex;gap:6px;flex-wrap:wrap}
.iaBotoes button,.iaAjustes select,.iaAjustes button{border:1px solid var(--borda);background:#2a323b;color:var(--texto);border-radius:8px;padding:8px 10px;font-size:13px}
.iaBotoes button.acao{background:var(--azul);border-color:var(--azul);color:#fff}
.iaAjustes{display:flex;gap:6px;flex-wrap:wrap;align-items:center;font-size:12px;color:var(--fraco)}
.iaAjustes input[type=password]{background:#0c0f12;color:var(--texto);border:1px solid var(--borda);border-radius:8px;padding:7px;width:220px}
details.iaMais summary{cursor:pointer;color:var(--fraco);font-size:13px}
/* Touchpad */
.linhaB{display:flex;gap:6px}
.stick{flex-shrink:0}
.touch{width:calc(210px*var(--e));height:calc(100px*var(--e));border:1px solid var(--borda);border-radius:14px;background:repeating-linear-gradient(45deg,#1b2128,#1b2128 6px,#1e252d 6px,#1e252d 12px);position:relative;touch-action:none;display:flex;align-items:center;justify-content:center;color:var(--fraco);font-size:12px;user-select:none}
.touch.on{border-color:var(--azul)}
.touch i{position:absolute;width:26px;height:26px;margin:-13px 0 0 -13px;border-radius:50%;background:rgba(61,139,253,.55);pointer-events:none}
#modoTouch{width:auto;padding:0 8px;white-space:nowrap}
/* Tela */
#pTela{display:none;flex-direction:column;gap:8px;overflow:hidden}
#pTela.ativo{display:flex}
.telabar{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.telabar button,.telabar select{border:1px solid var(--borda);background:#2a323b;color:var(--texto);border-radius:8px;padding:8px 10px;font-size:13px}
.telabar button.acao{background:var(--azul);border-color:var(--azul);color:#fff}
.telabar span{font-size:12px;color:var(--fraco)}
.telaarea{flex:1;min-height:0;background:#000;border-radius:12px;display:flex;align-items:center;justify-content:center;overflow:hidden}
.telaarea img{width:100%;height:100%;object-fit:contain}
.telaarea .vazio{color:var(--fraco)}
.mini{display:flex;gap:6px;flex-wrap:wrap;justify-content:center;touch-action:none;user-select:none}
.mini .b{width:52px;height:40px}
.ferr .ag{display:none}
#pcBanner{padding:10px;font-size:13px;color:var(--texto);background:#1f2a36;border-bottom:1px solid var(--borda);display:none}
#pcBanner a,#pcBanner button{display:inline-block;margin:6px 6px 0 0;border:0;border-radius:8px;padding:7px 10px;background:var(--azul);color:#fff;text-decoration:none;font-size:13px}
#pcBanner button{background:#2a323b}
/* Arquivos: PC à esquerda, PS4 à direita */
#pArquivos{display:none;flex-direction:column;gap:8px;overflow:hidden}
#pArquivos.ativo{display:flex}
.vista{display:flex;gap:6px;align-items:center;flex-wrap:wrap}
.vista button,.ferr button,.ferr select{border:1px solid var(--borda);background:#2a323b;color:var(--texto);border-radius:8px;padding:7px 10px;font-size:13px}
.vista button.ativo{background:var(--azul);border-color:var(--azul);color:#fff}
.vista .dica{font-size:12px;color:var(--fraco);flex:1;min-width:200px}
.paineis{flex:1;min-height:0;display:grid;grid-template-columns:1fr 1fr;gap:8px}
.paineis.so-pc .pps4,.paineis.so-ps4 .ppc{display:none}
.paineis.so-pc,.paineis.so-ps4{grid-template-columns:1fr}
.painel{display:flex;flex-direction:column;min-height:0;background:var(--painel);border:1px solid var(--borda);border-radius:12px;overflow:hidden}
.painel.alvo{border-color:var(--t);box-shadow:0 0 0 2px var(--t) inset}
.ptit{display:flex;align-items:center;gap:8px;padding:8px 10px;border-bottom:1px solid var(--borda)}
.ptit b{font-size:14px}
.caminho{cursor:pointer;flex:1;font-size:12px;color:var(--fraco);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;direction:rtl;text-align:left}
.ferr{display:flex;gap:6px;flex-wrap:wrap;padding:6px 10px;border-bottom:1px solid var(--borda)}
.ferr .acao{background:var(--azul);border-color:var(--azul);color:#fff}
.lista{flex:1;overflow:auto;min-height:0}
.item{display:flex;align-items:center;gap:8px;padding:7px 10px;border-bottom:1px solid #20262d;font-size:13px;cursor:default;user-select:none}
.item:hover{background:#20272f}
.item.sel{background:#203247}
.item .ic{width:18px;text-align:center}
.item .nm{flex:1;overf)HNPAG"
R"HNPAG(low:hidden;text-overflow:ellipsis;white-space:nowrap}
.item .tm{color:var(--fraco);font-size:12px;white-space:nowrap}
.vazio{padding:14px 10px;color:var(--fraco);font-size:13px}
#fila{max-height:30%;overflow:auto;background:var(--painel);border:1px solid var(--borda);border-radius:12px;padding:6px 10px;display:none}
#fila.tem{display:block}
.tar{font-size:12px;padding:4px 0}
.tar .barra{height:6px;margin:3px 0}
.tar.ok{color:var(--t)}.tar.erro{color:var(--o)}
@media (max-width:700px){.paineis{grid-template-columns:1fr}.paineis:not(.so-pc):not(.so-ps4) .ppc{display:none}}
@media (orientation:portrait){
 .pad{grid-template-columns:1fr 1fr;grid-template-rows:auto 1fr auto}
 .gatilhos{grid-column:1/3}
 .meio{grid-column:1/3;flex-direction:row;flex-wrap:wrap}
 .lado,.lado.dir{flex-direction:column;justify-content:space-evenly;grid-row:2;grid-column:1}
 .lado.dir{grid-column:2}
 .meio{grid-row:3}
 .cruz,.face{--t1:calc(56px*var(--e))}
 .stick{width:calc(150px*var(--e));height:calc(150px*var(--e))}
 .touch{width:100%;height:calc(130px*var(--e))}
 .meio{flex-direction:column}
}
</style>
</head>
<body>
<header><b>Hyper Neiva Remoto</b><small style="color:var(--fraco)">v3.51</small><span id="estado">conectando...</span></header>
<nav>
<button data-p="pControle" class="ativo">Controle</button>
<button data-p="pEnviar">Enviar</button>
<button data-p="pArquivos">Arquivos</button>
<button data-p="pTela">Tela</button>
<button data-p="pIa">IA</button>
<button data-p="pChaves">Chaves</button>
<button data-p="pTexto">Teclado</button>
</nav>
<main>
<section id="pControle" class="ativo">
<div class="pad">
 <div class="gatilhos">
  <div><div class="b g" data-b="256">L2</div><div class="b g" data-b="1024">L1</div></div>
  <div><div class="b g" data-b="2048">R1</div><div class="b g" data-b="512">R2</div></div>
 </div>
 <div class="lado">
  <div class="cruz">
   <div class="b c2 r1" data-b="16">&#9650;</div>
   <div class="b c1 r2" data-b="128">&#9664;</div>
   <div class="b c3 r2" data-b="32">&#9654;</div>
   <div class="b c2 r3" data-b="64">&#9660;</div>
  </div>
  <div class="stick" data-s="0"><i></i></div>
 </div>
 <div class="meio">
  <div class="touch" id="touch"><span>Touchpad: deslize ou toque</span></div>
  <div class="linhaB"><div class="b peq" id="modoTouch">Deslizar: touchpad</div><div class="b peq" data-b="8">Options</div></div>
  <div class="linhaB"><div class="b peq" data-b="2">L3</div><div class="b peq" data-b="4">R3</div></div>
 </div>
 <div class="lado dir">
  <div class="face">
   <div class="b c2 r1 ft" data-b="4096">&#9651;</div>
   <div class="b c1 r2 fq" data-b="32768">&#9633;</div>
   <div class="b c3 r2 fo" data-b="8192">&#9675;</div>
   <div class="b c2 r3 fx" data-b="16384">&#10005;</div>
  </div>
  <div class="stick" data-s="1"><i></i></div>
 </div>
</div>
</section>

<section id="pEnviar">
<div class="cartao">
 <h3>Mostrar no PS4</h3>
 <p>Escolha uma foto ou um vídeo. A foto abre em tela cheia e o vídeo toca no player do Hyper Neiva assim que o envio termina.</p>
 <div class="linha">
  <label class="btn">Foto<input id="arqFoto" type="file" accept="image/*"></label>
  <label class="btn">Vídeo: assistir já<input id="arqFluxo" type="file" accept="video/*"></label>
 </div>
 <div class="linha">
  <label class="btn btn2">Vídeo: copiar e tocar<input id="arqVideo" type="file" accept="video/*"></label>
  <label class="btn btn2">Outro arquivo<input id="arqOutro" type="file"></label>
 </div>
 <p>"Assistir já" toca no PS4 enquanto o vídeo chega, sem copiar. Deixe esta página aberta e a tela do celular ligada até o fim.</p>
 <div class="barra"><div id="barra"></div></div>
 <div id="msgEnvio"></div>
</div>
<div class="cartao">
 <h3>Já enviados</h3>
 <p>Ficam em /data/HyperNeiva/transmitidos no PS4.</p>
 <ul id="lista"></ul>
</div>
</section>

<section id="pArquivos">
<div class="vista">
 <button data-v="pc">PC</button><button data-v="ambos" class="ativo">PC | PS4</button><button data-v="ps4">PS4</button>
 <span class="dica" id="dicaPc"></span>
</div>
<div class="paineis" id="paineis">
 <div class="painel ppc" id="painelPc">
  <div class="ptit"><b id="titPc">PC</b><span class="caminho" id="camPc"></span></div>
  <div id="pcBanner">Para ver o disco do PC (C:, outras unidades, Área de Trabalho, Downloads...) abra o programa <b>Hyper Neiva PC</b> neste computador. Ele roda só aqui e faz as cópias direto com o PS4.<br>
   <a href="/HyperNeivaPC.exe" download>Baixar HyperNeivaPC.exe</a><button id="pcProcurar">Já abri o programa</button></div>
  <div class="ferr">
   <button id="pcSubir" title="Pasta de cima">&#8593;</button>
   <button id="pcAtualizar" title="Atualizar">&#8635;</button>
   <button id="pcEscolher">Escolher pasta</button>
   <button id="pcArquivos">Arquivos</button>
   <button id="pcTudo" title="Marcar tudo (Ctrl+A)">Tudo</button>
   <button class="ag" id="pcNova">Nova pasta</button>
   <button class="ag" id="pcRenomear">Renomear</button>
   <button class="ag" id="pcApagar">Apagar (Lixeira)</button>
   <button id="pcTirar" title="Tira os arquivos escolhidos desta lista (não apaga do PC)">Tirar da lista</button>
   <button class="ag" id="pcAbrir">Abrir no PC</button>
   <button class="acao" id="pcEnviar">Enviar ao PS4 &#8594;</button>
  </div>
  <div class="lista" id="listaPc"></div>
  <input type="file" id="pcInArq" multiple>
  <input type="file" id="pcInPasta" webkitdirectory multiple>
 </div>
 <div class="painel pps4" id="painelPs4">
  <div class="ptit"><b>PS4</b><span class="caminho" id="camPs4"></span></div>
  <div class="ferr">
   <button id="psSubir" title="Pasta de cima">&#8593;</button>
   <select id="psAtalho"><option value="">Ir para...</option><option>/data</option><option>/data/HyperNeiva</option><option>/data/pkg</option><option>/mnt/usb0</option><option>/mnt/usb1</option><option>/user</option><option>/</option></select>
   <button id="psAtualizar" title="Atualizar">&#8635;</button>
   <button class="acao" id="psBaixar">&#8592; Baixar no PC</button>
   <button id="psAbrir">Abrir no PS4</button>
   <button id="psVer">Ver aqui</button>
   <button id="psLer">Ler em voz</button>
   <button id="psPerguntar">Perguntar à IA</button>
   <button id="psTudo" title="Marcar tudo (Ctrl+A)">Tudo</button>
   <button id="psNova">Nova pasta</button>
   <button id="psRenomear">Renomear</button>
   <button id="psApagar">Apagar</button>
  </div>
  <div class="lista" id="listaPs4"></div>
 </div>
</div>
<div id="fila"></div>
</section>

<section id="pTela">
<div class="telabar">
 <button class="acao" id="telaVer">Ver tela do PS4</button>
 <button id="telaCheia">Tela cheia</button>
 <select id="telaQual" title="Qualidade da tela"><option value="rapida">Rápida</option><option value="nitida">Nítida</option></select>
 <button id="telaSom">&#128263; Ativar som</button>
 <span id="telaInfo">Setas, Enter, Esc, Q, W, Tab, 1-4 do teclado controlam o PS4.</span>
</div>
<div class="telaarea" id="telaArea"><div class="vazio">Toque em "Ver tela do PS4".</div></div>
<div class="mini" id="miniCtrl">
 <div class="b" data-b="1024">L1</div><div class="b" data-b="128">&#9664;</div><div class="b" data-b="16">&#9650;</div><div class="b" data-b="64">&#9660;</div><div class="b" data-b="32">&#9654;</div>
 <div class="b fq" data-b="32768">&#9633;</div><div class="b ft" data-b="4096">&#9651;</div><div class="b fo" data-b="8192">&#9675;</div><div class="b fx" data-b="16384">&#10005;</div><div class="b" data-b="2048">R1</div><div class="b" data-b="8">Opt</div>
</div>
<div class="telabar">
 <b style="font-size:13px">Tela do PC no PS4</b>
 <select id="espQual"><option value="1280,12,70">Normal (1280, 12 fps)</option><option value="1920,8,75">Nítida (1920, 8 fps)</option><option value="960,20,60">Rápida (960, 20 fps)</option></select>
 <button class="acao" id="espIniciar">Mostrar tela do PC no PS4</button>
 <button id="espParar">Parar</button>
 <span id="espInfo"></span>
</div>
</section>

<section id="pIa">
<div class="iaAjustes"><b style="font-size:13px;color:var(--texto)">Conversar com</b> <select id="iaConta"></select> <span id="iaContaInfo"></span></div>
<div id="iaMsgs"><div class="vazio" style="color:var(--fraco);font-size:13px">Converse com a IA do Hyper Neiva. A resposta aparece aqui e na tela do PS4 (EXTRA &gt; Inteligência Artificial).</div></div>
<div id="iaStatus"></div>
<div class="iaEntrada"><textarea id="iaTexto" placeholder="Pergunte ou descreva uma imagem (Enter envia)"></textarea><button class="btn" id="iaPerguntar" style="width:auto;padding:12px 16px">Enviar</button></div>
<div class="iaBotoes">
 <button id="iaTela">Perguntar sobre a tela do PS4</button>
 <button id="iaImagem">Gerar imagem</button>
 <button id="iaFalar">Ler em voz no PS4</button>
 <button id="iaParar">Parar a voz</button>
 <button id="iaOuvir">Ouvir aqui</button>
 <button id="iaLimpar">Limpar</button>
</div>
<details class="iaMais"><summary>Ajustes (modelo, voz, imagem)</summary>
<div class="iaAjustes" style="margin-top:6px">
 Modelo <input id="iaModelo" list="iaModelos" style="background:#0c0f12;color:var(--texto);border:1px solid var(--borda);border-radius:8px;padding:7px;width:230px"><datalist id="iaModelos"></datalist>
 Voz <select id="iaVoz"></select>
 Imagem (Gemini) <select id="iaModeloImg"></select>
 Formato <select id="iaAspecto"></select>
 <label><input type="checkbox" id="iaSempreVoz"> responder sempre em voz</label>
</div>
<p style="font-size:12px;color:var(--fraco);margin:6px 0 0">Contas e chaves ficam na aba Chaves &gt; Inteligência Artificial. O Groq não desenha nem fala: imagem e voz usam outra conta (Gemini ou ChatGPT).</p>
</details>
<audio id="iaAudio" style="display:none"></audio>
</section>

<section id="pChaves">
<div class="cartao">
 <h3>Chaves de API</h3>
 <p>Toque num serviço para configurar. Cada chave vai para o arquivo que o recurso já usa no PS4; aqui aparecem só os 4 últimos caracteres.</p>
 <div id="chMsg"></div>
 <div id="chCategorias"></div>
</div>
<div class="cartao">
 <h3>Importar do Hyper Neiva de PC</h3>
 <p>Com o Hyper Neiva PC aberto neste computador, ele lê as chaves do Hyper Neiva de PC (Configurações &gt; Chaves API) e você escolhe quais mandar para o PS4.</p>
 <div class="linha"><button class="btn" id="chBuscarPc">Ler chaves do PC</button></div>
 <div id="chImportar"></div>
</div>
</section>

<section id="pTexto">
<div class="cartao">
 <h3>Digitar no PS4</h3>
 <p>Abra um campo de texto no Hyper Neiva (o teclado do PS4 aparece), escreva aqui e aperte Enter ou toque em Enviar (Shift+Enter quebra a linha). O texto entra no campo e o teclado fecha confirmando.</p>
 <p id="teclado">Teclado do PS4: fechado</p>
 <textarea id="texto" placeholder="Digite aqui"></textarea>
 <div class="linha"><button class="btn" id="enviarTexto">Enviar ao PS4</button></div>
 <div id="msgTexto"></div>
</div>
<div class="cartao">
 <h3>Teclado do computador</h3>
 <p>Na aba Controle: setas = direcional, Enter = X, Esc ou Backspace = Bolinha, Q = Quadrado, W = Triângulo, Tab = Options, 1/2 = L1/R1, 3/4 = L2/R2.</p>
</div>
</section>
</main>
<script>
(function(){
"use strict";
var $=function(i){return document.getElementById(i)};
// Abas
var abas=document.querySelectorAll("nav button");
for(var i=0;i<abas.length;i++)abas[i].onclick=function(){
 for(var j=0;j<abas.length;j++){abas[j].classList.remove("ativo");$(abas[j].dataset.p).classList.remove("ativo")}
 this.classList.add("ativo");$(this.dataset.p).classList.add("ativo");
 if(this.dataset.p==="pEnviar")carregarLista();
};

// Fila em ordem: um pedido por vez, sempre com o estado mais novo.
function Fila(montar){this.montar=montar;this.ocupado=false;this.sujo=false}
Fila.prototype.marcar=function(){this.sujo=true;this.rodar()};
Fila.prototype.rodar=function(){
 var f=this;if(f.ocupado||!f.sujo)return;
 f.sujo=false;f.ocupado=true;
 fetch(f.montar(),{method:"POST",cache:"no-store"}).then(function(r){online(r.ok)}).catch(function(){online(false)}).then(function(){f.ocupado=false;f.rodar()});
};

var seguro=0,tocados=0;
var filaBotoes=new Fila(function(){var u="/b)HNPAG"
R"HNPAG(?m="+seguro+"&t="+tocados;tocados=0;return u});
function apertar(b){if(seguro&b)return;seguro|=b;tocados|=b;filaBotoes.marcar();if(navigator.vibrate)navigator.vibrate(12)}
function soltar(b){if(!(seguro&b))return;seguro&=~b;filaBotoes.marcar()}
setInterval(function(){if(seguro)filaBotoes.marcar()},400);

// Botões da tela: cada dedo segura o botão em que encostou.
var dedos={};
function botaoEm(x,y){var e=document.elementFromPoint(x,y);while(e&&!(e.dataset&&e.dataset.b))e=e.parentElement;return e}
function atualizarVisual(){
 var bs=document.querySelectorAll("[data-b]");
 for(var i=0;i<bs.length;i++)bs[i].classList.toggle("on",!!(seguro&+bs[i].dataset.b));
}
function inicio(id,x,y){var e=botaoEm(x,y);if(!e)return;dedos[id]=+e.dataset.b;apertar(dedos[id]);atualizarVisual()}
function fim(id){var b=dedos[id];if(b===undefined)return;delete dedos[id];
 var outro=false;for(var k in dedos)if(dedos[k]===b)outro=true;
 if(!outro)soltar(b);atualizarVisual()}
// Os mesmos botões funcionam na aba Controle e na barra da aba Tela.
function ligarControles(area){
 area.addEventListener("touchstart",function(ev){for(var i=0;i<ev.changedTouches.length;i++){var t=ev.changedTouches[i];
  if(touchpad.toque(t,true)||sticks.toque(t,true))continue;inicio("t"+t.identifier,t.clientX,t.clientY)}ev.preventDefault()},{passive:false});
 area.addEventListener("touchmove",function(ev){for(var i=0;i<ev.changedTouches.length;i++){var t=ev.changedTouches[i];if(!touchpad.toque(t,false))sticks.toque(t,false)}ev.preventDefault()},{passive:false});
 function fimToque(ev){for(var i=0;i<ev.changedTouches.length;i++){var t=ev.changedTouches[i];touchpad.soltar(t.identifier);sticks.soltar(t.identifier);fim("t"+t.identifier)}ev.preventDefault()}
 area.addEventListener("touchend",fimToque,{passive:false});
 area.addEventListener("touchcancel",fimToque,{passive:false});
 area.addEventListener("mousedown",function(ev){var t={identifier:"m",clientX:ev.clientX,clientY:ev.clientY};if(!touchpad.toque(t,true)&&!sticks.toque(t,true))inicio("m",ev.clientX,ev.clientY)});
}
window.addEventListener("mousemove",function(ev){if(ev.buttons&1){var t={identifier:"m",clientX:ev.clientX,clientY:ev.clientY};if(!touchpad.toque(t,false))sticks.toque(t,false)}});
window.addEventListener("mouseup",function(){touchpad.soltar("m");sticks.soltar("m");fim("m")});
window.addEventListener("blur",function(){dedos={};seguro=0;filaBotoes.marcar();sticks.zerar();touchpad.zerar();atualizarVisual()});

// Analógicos: 0 a 255, 128 no centro.
var valores=[128,128,128,128];
var filaAnalog=new Fila(function(){return "/a?v="+valores.join(",")});
var sticks=(function(){
 var els=document.querySelectorAll(".stick"),dono=[null,null];
 function mover(s,x,y){
  var r=els[s].getBoundingClientRect(),raio=r.width/2;
  var dx=(x-(r.left+raio))/raio,dy=(y-(r.top+raio))/raio,d=Math.sqrt(dx*dx+dy*dy);
  if(d>1){dx/=d;dy/=d}
  valores[s*2]=Math.round(128+dx*127);valores[s*2+1]=Math.round(128+dy*127);
  els[s].firstChild.style.transform="translate("+(dx*raio*0.6)+"px,"+(dy*raio*0.6)+"px)";
  filaAnalog.marcar();
 }
 function centro(s){valores[s*2]=128;valores[s*2+1]=128;els[s].firstChild.style.transform="";filaAnalog.marcar()}
 return{
  toque:function(t,novo){
   for(var s=0;s<2;s++){
    if(dono[s]===t.identifier){mover(s,t.clientX,t.clientY);return true}
    if(novo&&dono[s]===null){var r=els[s].getBoundingClientRect();
     if(t.clientX>=r.left&&t.clientX<=r.right&&t.clientY>=r.top&&t.clientY<=r.bottom){dono[s]=t.identifier;mover(s,t.clientX,t.clientY);return true}}
   }
   return false},
  soltar:function(id){for(var s=0;s<2;s++)if(dono[s]===id){dono[s]=null;centro(s)}},
  zerar:function(){for(var s=0;s<2;s++){dono[s]=null;centro(s)}}
 };
})();
setInterval(function(){if(valores.join()!=="128,128,128,128")filaAnalog.marcar()},250);

// Touchpad: posição dos dedos (0..1919 x 0..941, como o do DS4). Toque rápido = clique do touchpad.
// No modo "setas", deslizar vira direcional, para navegar nos menus como num controle de TV.
var touchpad=(function(){
 var el=$("touch"),dedosTp={},ordem=[],modoSetas=false;
 var filaTp=new Fila(function(){var d=[ordem.length];for(var i=0;i<2;i++){var f=dedosTp[ordem[i]];d.push(f?f.x:0,f?f.y:0)}return "/tp?d="+d.join(",")});
 function pos(t){var r=el.getBoundingClientRect();
  return{x:Math.max(0,Math.min(1919,Math.round((t.clientX-r.left)/r.width*1919))),y:Math.max(0,Math.min(941,Math.round((t.clientY-r.top)/r.height*941)))}}
 function desenhar(){var is=el.querySelectorAll("i");for(var i=0;i<is.length;i++)is[i].remove();
  ordem.forEach(function(id){var f=dedosTp[id],m=document.createElement("i");m.style.left=(f.x/19.19)+"%";m.style.top=(f.y/9.41)+"%";el.appendChild(m)});
  el.classList.toggle("on",ordem.length>0)}
 function seta(b){tocados|=b;filaBotoes.marcar();if(navigator.vibrate)navigator.vibrate(8)}
 $("modoTouch").addEventListener("click",function(){});
 return{
  toque:function(t,novo){
   var f=dedosTp[t.identifier];
   if(!f){if(!novo)return false;
    var alvo=document.elementFromPoint(t.clientX,t.clientY);
    if(alvo&&alvo.id==="modoTouch"){modoSetas=!modoSetas;alvo.textContent=modoSetas?"Deslizar: setas":"Deslizar: touchpad";return true}
    if(!alvo||!el.contains(alvo))return false;
    if(ordem.length>=2)return true;
    var p=pos(t);f=dedosTp[t.identifier]={x:p.x,y:p.y,x0:p.x,y0:p.y,ax:p.x,ay:p.y,t0:Date.now(),longe:false};ordem.push(t.identifier);
   }else{var q=pos(t);f.x=q.x;f.y=q.y}
   if(Math.abs(f.x-f.x0)>60||Math.abs(f.y-f.y0)>60)f.longe=true;
   if(modoSetas){var dx=f.x-f.ax,dy=f.y-f.ay,passo=260;
    if(Math.abs(dx)>=passo&&Math.abs(dx)>Math.abs(dy)){seta(dx>0?32:128);f.ax=f.x;f.ay=f.y}
    else if(Math.abs(dy)>=passo*0.6){seta(dy>0?64:16);f.ax=f.x;f.ay=f.y}}
   else filaTp.marcar();
   desenhar();return true},
  soltar:function(id){var f=dedosTp[id];if(!f)return;delete dedosTp[id];ordem.splice(ordem.indexOf(id),1);
   if(!f.longe&&Date.now()-f.t0<300)seta(modoSetas?16384:1048576);
   if(!modoSetas)filaTp.marcar();desenhar()},
  zerar:function(){dedosTp={};ordem=[];filaTp.marcar();desenhar()},
  ativo:function(){return ordem.length>0&&!modoSetas},
  manter:function(){if(ordem.length&&!modoSetas)filaTp.marcar()}
 };
})();
setInterval(function(){touchpad.manter()},250);

ligarControles($("pControle"));
ligarControles($("miniCtrl"));

// Teclado do computador.
var teclas={ArrowUp:16,ArrowDown:64,ArrowLeft:128,ArrowRight:32,Enter:16384,Escape:8192,Backspace:8192,q:32768,w:4096,Tab:8,"1":1024,"2":2048,"3":256,"4":512};
function tecla(ev,desce){
 if(!$("pControle").classList.contains("ativo")&&!$("pTela").classList.contains("ativo"))return;
 var b=teclas[ev.key.length===1?ev.key.toLowerCase():ev.key];if(!b)return;
 ev.preventDefault();if(desce){if(!ev.repeat)apertar(b)}else soltar(b);atualizarVisual();
}
window.addEventListener("keydown",function(e){tecla(e,true)});
window.addEventListener("keyup",function(e){tecla(e,false)});

// Estado do PS4.
var ultimoOk=0;
function online(ok){if(ok)ultimoOk=Date.now();var e=$("estado");var on=Date.now()-ultimoOk<5000;e.textContent=on?"conectado":"sem conexão";e.className=on?"on":""}
function consultar(){
 fetch("/estado",{cache:"no-store"}).then(function(r){return r.json()}).then(function(j){
  online(true);$("teclado").textContent="Teclado do PS4: "+(j.teclado?"aberto, pode enviar":"fechado");
 }).catch(function(){online(false)});
}
setInterval(consultar,2000);consultar();

// Texto.
$("enviarTexto").onclick=function(){
 var t=$("texto").value;if(!t){$("msgTexto").textContent="Escreva alguma coisa primeiro.";return}
 fetch("/t",{method:"POST",body:t,cache:"no-store"}).then(function(r){return r.json()}).then(function(j){
  if(j.ok){$("msgTexto").textContent="Enviado.";$("texto").value=""}
  else $("msgTexto").textContent="O teclado do PS4 está fechado. Abra um campo de texto no Hyper Neiva e envie de novo.";
 }).catch(function(){$("msgTexto").textContent="Falhou: sem conexão com o PS4."});
};

// Envio de arquivos.
function msg(t){$("msgEnvio").textContent=t}
function barra(p){$("barra").style.width=p+"%"}
function nomeJpg(n){var p=n.lastIndexOf(".");return(p>0?n.slice(0,p):n)+".jpg"}
// Foto grande ou HEIC vira JPEG de até 3840 px: o PS4 abre direto e não falta memória.
function prepararFoto(arq){
 return new Promise(function(ok){
  var tipo=(arq.type||"").toLowerCase();
  if(tipo==="image/gif"){ok({blob:arq,nome:arq.name});return}
  var url=URL.createObjectURL(arq),img=new Image();
  img.onload=function(){
   var w=img.naturalWidth,h=img.naturalHeight,m=3840/Math.max(w,h);
   var simples=(tipo==="image/jpeg"||tipo==="image/png")&&m>=1&&arq.size<15e6;
   if(simples){URL.revokeObjectURL(url);ok({blob:arq,nome:arq.name});return}
   if(m>1)m=1;var c=document.createElement("canvas");c.width=Math.round(w*m);c.height=Math.round(h*m);
   c.getContext("2d").drawImage(img,0,0,c.width,c.height);URL.revokeObjectURL(url);
   c.toBlob(function(b){ok(b?{blob:b,nome:nomeJpg(arq.name)}:{blob:arq,nome:arq.name})},"image/jpeg",0.92);
  };
  img.onerror=function(){URL.revokeObjectURL(url);ok({blob:arq,nome:arq.name})};
  img.src=url;
 });
}
var enviando=false;
function enviar(blob,nome){
 if(enviando){msg("Espere o envio atual terminar.");return}
 enviando=true;barra(0);msg("Enviando "+nome+"...");
 var x=new XMLHttpRequest(),t0=Date.now();
 x.open("POST","/enviar?n="+encodeURIComponent(nome));
 x.upload.onprogress=function(e){if(e.lengthComputable){var p=e.loaded/e.total*100;barra(p);
  var s=(Date.now()-t0)/1000,v=s>0?e.loaded/s/1048576:0;msg("Enviando "+nome+": "+p.toFixed(0)+"% ("+v.toFixed(1)+" MB/s)")}};
 x.onload=function(){enviando=false;var j={};try{j=JSON.parse(x.responseText)}catch(e){}
  if(x.status===200&&j.ok){barra(100);msg(j.tipo==="outro"?"Salvo no PS4 (este tipo não abre sozinho).":"Pronto: aberto no PS4.");carregarLista()}
  else msg("Falhou: "+(j.erro||("HTTP "+x.status)))};
 x.onerror=function(){enviando=false;msg("Falhou: a conexão caiu.")};
 x.send(blob);
}
function escolher(id,foto){$(id).onchange=function(){var a=this.files[0];this.value="";if(!a)return;
 if(foto){msg("Preparando a foto...");prepararFoto(a).then(function(r){enviar(r.blob,r.nome)})}else enviar(a,a.name)}}
escolher("arqFoto",true);escolher("arqVideo",false);escolher("arqOutro",false);

function tamanho(b){return b>1073741824?(b/1073741824).toFixed(2)+" GB":b>1048576?(b/1048576).toFixed(1)+" MB":(b/1024).toFixed(0)+" KB"}
function carregarLista(){
 fetch("/lista",{cache:"no-store"}).then(function(r){return r.json()}).then(function(l){
  var ul=$("lista");ul.innerHTML="";
  if(!l.length){ul.innerHTML="<li><span>Nada enviado ainda.</span></li>";return}
  l.forEach(function(a){
   var li=document.createElement("li"),s=document.createElement("span");s.textContent=a.n+" ("+tamanho(a.t)+")";li.appendChild(s);
   var ab=document.createElement("button");ab.textContent="Abrir";ab.onclick=function(){fetch("/abrir?n="+encodeURIComponent(a.n),{method:"POST"}).then(function(){msg("Abrindo "+a.n+" no PS4.")})};li.appendChild(ab);
   var ap=document.createElement("button");ap.textContent="Apagar";ap.onclick=function(){if(confirm("Apagar "+a.n+" do PS4?"))fetch("/apagar?n="+encodeURIComponent(a.n),{method:"POST"}).then(carregarLista)};li.appendChild(ap);
   ul.appendChild(li);
  });
 }).catch(function(){});
}

// =================== Vídeo tocando enquanto chega ===================
function assistirJa(arq){
 if(enviando){msg("Espere o envio atual terminar.");return}
 enviando=true;barra(0);msg("Abrindo "+arq.name+" no PS4...");
 var id=0,fila=[],naFila={},enviados=0,ativos=0,acabou=false,t0=Date.now(),bytes=0;
 function fim(texto){if(acabou)return;acabou=true;enviando=false;msg(texto)}
 function mandar(){
  while(ativos<2&&fila.length){
   (function(b,tam){ativos++;var ini=b*tam,blob=arq.slice(ini,Math.min(arq.size,ini+tam));
    fetch("/fluxo/bloco?id="+id+"&b="+b,{method:"POST",body:blob,cache:"no-store"}).then(function(r){
     delete naFila[b];enviados++;bytes+=blob.size;var s=(Date.now()-t0)/1000;
     barra(Math.min()HNPAG"
R"HNPAG(100,bytes/arq.size*100));
     msg("Tocando no PS4 ("+(bytes/s/1048576).toFixed(1)+" MB/s, "+(bytes/1048576).toFixed(0)+" MB lidos). Mantenha esta página aberta.");
    }).catch(function(){delete naFila[b]}).then(function(){ativos--;mandar()});
   })(fila.shift(),tamBloco);
  }
 }
 var tamBloco=524288;
 function perguntar(){
  if(acabou)return;
  fetch("/fluxo/pedido?id="+id,{method:"POST",cache:"no-store"}).then(function(r){return r.json()}).then(function(j){
   if(j.fim){fim("Vídeo fechado no PS4.");return}
   if(j.tam)tamBloco=j.tam;
   (j.b||[]).forEach(function(b){if(!naFila[b]){naFila[b]=1;fila.push(b)}});
   mandar();perguntar();
  }).catch(function(){setTimeout(perguntar,1000)});
 }
 fetch("/fluxo/abrir?n="+encodeURIComponent(arq.name)+"&t="+arq.size,{method:"POST",cache:"no-store"}).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){fim("O PS4 recusou o vídeo.");return}
  id=j.id;perguntar();
 }).catch(function(){fim("Falhou: sem conexão com o PS4.")});
}
$("arqFluxo").onchange=function(){var a=this.files[0];this.value="";if(a)assistirJa(a)};

// =================== Arquivos: PC | PS4 ===================
var temFs=!!window.showDirectoryPicker;
var pc={modo:temFs?"fs":"lista",pilha:[],itens:[],sel:{},virtual:[],pasta:""};
var ps={caminho:"/data",itens:[],sel:{}};
function juntar(a,b){return a==="/"?"/"+b:a+"/"+b}
function pai(c){var i=c.lastIndexOf("/");return i<=0?"/":c.slice(0,i)}
function esc(t){return String(t).replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}

(function(){
 var bs=document.querySelectorAll(".vista button");
 for(var i=0;i<bs.length;i++)bs[i].onclick=function(){
  for(var j=0;j<bs.length;j++)bs[j].classList.remove("ativo");this.classList.add("ativo");
  var v=this.dataset.v;$("paineis").className="paineis"+(v==="pc"?" so-pc":v==="ps4"?" so-ps4":"");
 };
 var celular=/Android|iPhone|iPad/i.test(navigator.userAgent);
 $("titPc").textContent=celular?"Este aparelho":"PC";
 $("dicaPc").textContent=temFs?"Clique em \"Escolher pasta\" para abrir uma pasta do PC. Arraste arquivos do Windows para o painel do PS4.":
  (celular?"Escolha arquivos do aparelho para mandar ao PS4. O que vier do PS4 vai para Downloads.":
  "Modo simples: escolha arquivos ou pastas para mandar; o que vier do PS4 vai para Downloads. Para abrir pastas do PC direto, no Chrome/Edge ative chrome://flags/#unsafely-treat-insecure-origin-as-secure com "+location.origin+".");
 if(!temFs)$("pcEscolher").textContent="Escolher pasta";
})();

// ---- Lista desenhada (os dois painéis usam a mesma) ----
var painelArq="ps4",ultimoClique={el:null,i:-1};
function desenhar(el,itens,sel,abrir){
 el._redesenhar=function(){desenhar(el,itens,sel,abrir)};el._sel=sel;el._itens=itens;
 el.innerHTML="";
 if(!itens.length){el.innerHTML='<div class="vazio">Pasta vazia</div>';return}
 itens.forEach(function(it,i){
  var d=document.createElement("div");d.className="item"+(sel[i]?" sel":"");
  d.innerHTML='<span class="ic">'+(it.ic||(it.d?"&#128193;":"&#128196;"))+'</span><span class="nm">'+esc(it.n)+'</span><span class="tm">'+(it.info!==undefined?it.info:(it.d?"":tamanho(it.t||0)))+'</span>';
  d.onclick=function(ev){
   painelArq=el.id==="listaPc"?"pc":"ps4";
   if(ev.shiftKey&&ultimoClique.el===el&&ultimoClique.i>=0){
    // Shift: marca do último clicado até este.
    if(!(ev.ctrlKey||ev.metaKey)){for(var k in sel)delete sel[k]}
    var a=Math.min(ultimoClique.i,i),b=Math.max(ultimoClique.i,i);for(var j=a;j<=b;j++)sel[j]=1;
   }else{
    if(!(ev.ctrlKey||ev.metaKey)){var so=sel[i]&&Object.keys(sel).length===1;for(var k2 in sel)delete sel[k2];if(!so)sel[i]=1}
    else if(sel[i])delete sel[i];else sel[i]=1;
    ultimoClique={el:el,i:i};
   }
   desenhar(el,itens,sel,abrir);
  };
  d.ondblclick=function(){abrir(it)};
  // Toque duplo no celular abre a pasta.
  var ultimo=0;d.addEventListener("touchend",function(){var a=Date.now();if(a-ultimo<350)abrir(it);ultimo=a});
  el.appendChild(d);
 });
}
function marcarTudo(qual){
 var el=$(qual==="pc"?"listaPc":"listaPs4");if(!el._sel)return;
 var tudo=true;for(var i=0;i<el._itens.length;i++)if(!el._sel[i])tudo=false;
 for(var k in el._sel)delete el._sel[k];
 // Segundo Ctrl+A (ou "Tudo") com tudo marcado desmarca.
 if(!tudo)for(var j=0;j<el._itens.length;j++)if(!el._itens[j].p)el._sel[j]=1;
 painelArq=qual;el._redesenhar();
}
$("pcTudo").onclick=function(){marcarTudo("pc")};
$("psTudo").onclick=function(){marcarTudo("ps4")};
function painelVisivel(){var c=$("paineis").className;return c.indexOf("so-pc")>=0?"pc":c.indexOf("so-ps4")>=0?"ps4":painelArq}
window.addEventListener("keydown",function(e){
 if(!$("pArquivos").classList.contains("ativo"))return;
 var t=e.target&&e.target.tagName;if(t==="INPUT"||t==="TEXTAREA"||t==="SELECT")return;
 if((e.ctrlKey||e.metaKey)&&(e.key==="a"||e.key==="A")){e.preventDefault();marcarTudo(painelVisivel());return}
 if(e.key==="Delete"){e.preventDefault();$(painelVisivel()==="pc"?"pcApagar":"psApagar").click()}
});
function ordenar(l){return l.sort(function(a,b){return(b.d-a.d)||a.n.localeCompare(b.n,"pt",{numeric:true,sensitivity:"base"})})}
function selecionados(p){var r=[];for(var k in p.sel)if(p.itens[k])r.push(p.itens[k]);return r}

// ---- Painel do PS4 ----
function listarPs4(c){
 c=c||ps.caminho;
 fetch("/fs/lista?p="+encodeURIComponent(c),{cache:"no-store"}).then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(l){
  ps.caminho=c;ps.itens=ordenar(l);ps.sel={};$("camPs4").textContent=c;
  desenhar($("listaPs4"),ps.itens,ps.sel,function(it){if(it.d)listarPs4(juntar(ps.caminho,it.n));else verNoNavegador(juntar(ps.caminho,it.n))});
 }).catch(function(){$("listaPs4").innerHTML='<div class="vazio">Não abriu '+esc(c)+'</div>'});
}
$("psSubir").onclick=function(){listarPs4(pai(ps.caminho))};
$("camPs4").onclick=function(){var c=prompt("Ir para a pasta do PS4:",ps.caminho);if(c)listarPs4(c.replace(/\/+$/,"")||"/")};
$("psAtualizar").onclick=function(){listarPs4()};
$("psAtalho").onchange=function(){if(this.value)listarPs4(this.value);this.value=""};
function verNoNavegador(c){window.open("/fs/baixar?p="+encodeURIComponent(c),"_blank")}
$("psLer").onclick=function(){var it=selecionados(ps)[0];if(!it||it.d)return alert("Escolha um arquivo (PDF, TXT, DOCX, ODT, EPUB...).");
 iaPost("/ia/arquivo/ler?p="+encodeURIComponent(juntar(ps.caminho,it.n))).then(function(j){alert(j.ok?"Lendo \""+it.n+"\" em voz alta no PS4. Para parar: aba IA > Parar a voz.":j.erro)})};
$("psPerguntar").onclick=function(){var it=selecionados(ps)[0];if(!it||it.d)return alert("Escolha um arquivo (PDF, TXT, DOCX, ODT, EPUB...).");
 var q=prompt("O que quer saber sobre \""+it.n+"\"? (vazio = resumo)","");if(q===null)return;
 iaPost("/ia/arquivo/perguntar?p="+encodeURIComponent(juntar(ps.caminho,it.n)),q).then(function(j){if(j.ok){alert("Pergunta enviada. A resposta aparece na aba IA.");document.querySelector('nav button[data-p="pIa"]').click()}else alert(j.erro)})};
$("psVer").onclick=function(){selecionados(ps).forEach(function(it){if(!it.d)verNoNavegador(juntar(ps.caminho,it.n))})};
$("psAbrir").onclick=function(){var it=selecionados(ps)[0];if(!it||it.d)return;
 fetch("/fs/abrir?p="+encodeURIComponent(juntar(ps.caminho,it.n)),{method:"POST"}).then(function(r){return r.json()}).then(function(j){if(!j.ok)alert("Este tipo de arquivo não abre no PS4.")})};
$("psNova").onclick=function(){var n=prompt("Nome da pasta nova:");if(!n)return;
 fetch("/fs/pasta?p="+encodeURIComponent(juntar(ps.caminho,n)),{method:"POST"}).then(function(){listarPs4()})};
$("psRenomear").onclick=function(){var it=selecionados(ps)[0];if(!it)return;var n=prompt("Novo nome:",it.n);if(!n||n===it.n)return;
 fetch("/fs/renomear?p="+encodeURIComponent(juntar(ps.caminho,it.n))+"&para="+encodeURIComponent(juntar(ps.caminho,n)),{method:"POST"}).then(function(){listarPs4()})};
$("psApagar").onclick=function(){var l=selecionados(ps);if(!l.length)return alert("Marque o que quer apagar no painel do PS4 (clique, Ctrl/Shift+clique ou \"Tudo\").");
 if(!confirm("Apagar do PS4 "+(l.length===1?'"'+l[0].n+'"':l.length+" itens")+"? Pastas são apagadas com tudo dentro."))return;
 var falhas=[];
 // Um de cada vez: o PS4 atende melhor que dezenas de pedidos juntos.
 l.reduce(function(fila,it){return fila.then(function(){
  return fetch("/fs/apagar?p="+encodeURIComponent(juntar(ps.caminho,it.n)),{method:"POST"}).then(function(r){return r.json()}).then(function(j){if(!j.ok)falhas.push(it.n+(j.erro?" ("+j.erro+")":""))}).catch(function(){falhas.push(it.n+" (sem resposta)")});
 })},Promise.resolve()).then(function(){listarPs4();if(falhas.length)alert("Não apagou "+falhas.length+" de "+l.length+":\n"+falhas.slice(0,15).join("\n"))})};

// ---- Fila de transferências (uma por vez) ----
var tarefas=[],rodando=false;
function novaTarefa(nome,fn){
 var el=document.createElement("div");el.className="tar";el.innerHTML='<span></span><div class="barra"><div></div></div>';
 el.firstChild.textContent=nome+": na fila";$("fila").appendChild(el);$("fila").classList.add("tem");
 tarefas.push({nome:nome,fn:fn,el:el});proxima();
}
function proxima(){
 if(rodando||!tarefas.length)return;rodando=true;
 var t=tarefas.shift(),txt=t.el.firstChild,bar=t.el.lastChild.firstChild,t0=Date.now();
 txt.textContent=t.nome+": começando";
 t.fn(function(feito,total){bar.style.width=(total?feito/total*100:0)+"%";var s=(Date.now()-t0)/1000;
  txt.textContent=t.nome+": "+tamanho(feito)+(total?" de "+tamanho(total):"")+(s>0.5?" ("+(feito/s/1048576).toFixed(1)+" MB/s)":"")})
 .then(function(){t.el.className="tar ok";txt.textContent=t.nome+": pronto";bar.style.width="100%"},
  function(e){t.el.className="tar erro";txt.textContent=t.nome+": falhou ("+(e&&e.message||e||"erro")+")"})
 .then(function(){rodando=false;if(!tarefas.length){listarPs4();if(pc.modo==="fs"&&pc.pilha.length)listarPc()}proxima()});
}
function enviarArquivoPs4(file,destino,prog){
 return new Promise(function(ok,falha){
  var x=new XMLHttpRequest();x.open("POST","/fs/enviar?p="+encodeURIComponent(destino));
  x.upload.onprogress=function(e){prog(e.loaded,e.total||file.size)};
  x.onload=function(){var j={};try{j=JSON.parse(x.responseText)}catch(e){}if(x.status===200&&j.ok)ok();else falha(j.erro||"HTTP "+x.status)};
  x.onerror=function(){falha("conexão caiu")};x.send(file);
 });
}
function criarPastaPs4(c){return fetch("/fs/pasta?p="+encodeURIComponent(c),{method:"POST"})}

// Envia uma lista {caminho relativo, file} para a pasta atual do PS4, criando as subpastas.
function enviarListaPs4(lista,base){
 var pastas={};
 lista.forEach(function(e){var p=e.rel.split("/");p.pop();var acc=base;p.forEach(function(x){acc=juntar(acc,x);pastas[acc]=1})});
 var ps_=Object.keys(pastas).sort(function(a,b){return a.length-b.length});
 var cria=ps_.reduce(function(pr,c){return pr.then(function(){return criarPastaPs4(c)})},Promise.resolve());
 cria.then(function(){lista.forEach(function(e){
  novaTarefa("→ PS4 "+e.rel,function(prog){return enviarArquivoPs4(e.file,juntar(base,e.rel),prog)});
 })});
}

// ---- Painel do PC ----
function listarPc(){
 if(pc.modo==="fs"){
  var dir=pc.pilha[pc.pilha.length-1];
  if(!dir){$("listaPc").innerHTML='<div class="vazio">Clique em "Escolher pasta" para abrir uma pasta do PC.</div>';$("camPc").textContent="";return}
  $("camPc").textContent=pc.pilha.map(function(d){return d.name}).join("/");
  var l=[];
  (async function(){
   for await(var par of dir.entries()){var h=par[1];var it={n:h.name,d:h.kind==="directory"?1:0,h:h,t:0};
    if(!it.d){try{it.t=(await h.getFile()).size}catch(e){}}l.push(it)}
   pc.itens=ordenar(l);pc.sel={};
   desenhar($("listaPc"),pc.itens,pc.sel,function(it){if(it.d){pc.pilha.push(it.h);listarPc()}});
  })().catch(function(e){$("listaPc").innerHTML='<div class="vazio">Não abriu: '+esc(e.message||e)+'</div>'});
  return;
 }
 // Modo simples: mostra o que foi escolhido, navegando pelas pastas.
 var pref=pc.pasta?pc.pasta+"/":"",vistos={},l=[];
 pc.virtual.forEach(function(e){
  if(e)HNPAG"
R"HNPAG(.rel.indexOf(pref)!==0)return;var resto=e.rel.slice(pref.length),i=resto.indexOf("/");
  if(i<0)l.push({n:resto,d:0,t:e.file.size,e:e});
  else{var n=resto.slice(0,i);if(!vistos[n]){vistos[n]=1;l.push({n:n,d:1})}}
 });
 $("camPc").textContent=pc.pasta||(pc.virtual.length?"escolhidos":"");
 pc.itens=ordenar(l);pc.sel={};
 if(!pc.virtual.length){$("listaPc").innerHTML='<div class="vazio">Use "Escolher pasta" ou "Arquivos", ou arraste arquivos para o painel do PS4.</div>';return}
 desenhar($("listaPc"),pc.itens,pc.sel,function(it){if(it.d){pc.pasta=pref+it.n;listarPc()}});
}
$("pcSubir").onclick=function(){
 if(pc.modo==="fs"){if(pc.pilha.length>1){pc.pilha.pop();listarPc()}}
 else{var i=pc.pasta.lastIndexOf("/");pc.pasta=i<0?"":pc.pasta.slice(0,i);listarPc()}
};
$("pcEscolher").onclick=function(){
 if(pc.modo==="fs"){window.showDirectoryPicker({mode:"readwrite"}).then(function(d){pc.pilha=[d];listarPc()}).catch(function(){})}
 else $("pcInPasta").click();
};
$("pcArquivos").onclick=function(){$("pcInArq").click()};
function usarEscolhidos(files){
 pc.modo="lista";pc.pasta="";
 pc.virtual=Array.prototype.map.call(files,function(f){return{rel:f.webkitRelativePath||f.name,file:f}});
 listarPc();
}
$("pcInArq").onchange=function(){usarEscolhidos(this.files);this.value=""};
$("pcInPasta").onchange=function(){usarEscolhidos(this.files);this.value=""};

// Junta arquivos (com pastas) de um item do PC para mandar.
async function coletarFs(h,rel,saida){
 if(h.kind==="file"){saida.push({rel:rel,file:await h.getFile()});return}
 for await(var par of h.entries())await coletarFs(par[1],rel+"/"+par[1].name,saida);
}
$("pcEnviar").onclick=async function(){
 var l=selecionados(pc);if(!l.length){alert("Selecione arquivos ou pastas no painel do PC.");return}
 var lista=[];
 if(pc.modo==="fs"){for(var i=0;i<l.length;i++)await coletarFs(l[i].h,l[i].n,lista)}
 else{var pref=pc.pasta?pc.pasta+"/":"";l.forEach(function(it){
  var alvo=pref+it.n;pc.virtual.forEach(function(e){if(e.rel===alvo||e.rel.indexOf(alvo+"/")===0)lista.push({rel:e.rel.slice(pref.length),file:e.file})})})}
 enviarListaPs4(lista,ps.caminho);
};

// ---- PS4 para o PC ----
async function listarRecursivoPs4(c,rel,saida){
 var l=await (await fetch("/fs/lista?p="+encodeURIComponent(c),{cache:"no-store"})).json();
 for(var i=0;i<l.length;i++){var it=l[i],cc=juntar(c,it.n),rr=rel+"/"+it.n;
  if(it.d){saida.push({rel:rr,d:1});await listarRecursivoPs4(cc,rr,saida)}else saida.push({rel:rr,c:cc,t:it.t})}
}
async function baixarParaPasta(dir,item,prog){
 var partes=item.rel.split("/").filter(Boolean),nome=partes.pop(),d=dir;
 for(var i=0;i<partes.length;i++)d=await d.getDirectoryHandle(partes[i],{create:true});
 var fh=await d.getFileHandle(nome,{create:true}),w=await fh.createWritable();
 var r=await fetch("/fs/baixar?p="+encodeURIComponent(item.c),{cache:"no-store"});
 if(!r.ok)throw new Error("HTTP "+r.status);
 var leitor=r.body.getReader(),feito=0;
 for(;;){var p=await leitor.read();if(p.done)break;await w.write(p.value);feito+=p.value.length;prog(feito,item.t)}
 await w.close();
}
function baixarNavegador(item){
 return new Promise(function(ok){var a=document.createElement("a");a.href="/fs/baixar?p="+encodeURIComponent(item.c)+"&dl=1";
  a.download=item.rel.split("/").pop();document.body.appendChild(a);a.click();a.remove();setTimeout(ok,700)});
}
$("psBaixar").onclick=async function(){
 var l=selecionados(ps);if(!l.length){alert("Selecione arquivos ou pastas no painel do PS4.");return}
 var lista=[];
 for(var i=0;i<l.length;i++){var it=l[i],c=juntar(ps.caminho,it.n);
  if(it.d){lista.push({rel:it.n,d:1});await listarRecursivoPs4(c,it.n,lista)}else lista.push({rel:it.n,c:c,t:it.t})}
 var dir=pc.modo==="fs"?pc.pilha[pc.pilha.length-1]:null;
 if(pc.modo==="fs"&&!dir){alert("Escolha antes a pasta do PC que vai receber.");return}
 lista.forEach(function(item){
  if(item.d){if(dir)novaTarefa("← PC "+item.rel+"/",function(){return item.rel.split("/").reduce(function(pr,x){return pr.then(function(d){return d.getDirectoryHandle(x,{create:true})})},Promise.resolve(dir))});return}
  novaTarefa("← PC "+item.rel,function(prog){return dir?baixarParaPasta(dir,item,prog):baixarNavegador(item)});
 });
};

// Arrastar do Windows (ou do celular) para o painel do PS4.
(function(){
 var alvo=$("painelPs4");
 alvo.addEventListener("dragover",function(e){e.preventDefault();alvo.classList.add("alvo")});
 alvo.addEventListener("dragleave",function(){alvo.classList.remove("alvo")});
 alvo.addEventListener("drop",function(e){
  e.preventDefault();alvo.classList.remove("alvo");
  var entradas=[];for(var i=0;i<e.dataTransfer.items.length;i++){var en=e.dataTransfer.items[i].webkitGetAsEntry&&e.dataTransfer.items[i].webkitGetAsEntry();if(en)entradas.push(en)}
  var lista=[];
  function ler(en,rel){return new Promise(function(ok){
   if(en.isFile)en.file(function(f){lista.push({rel:rel,file:f});ok()},function(){ok()});
   else{var r=en.createReader(),todos=[];(function mais(){r.readEntries(function(l){if(!l.length){Promise.all(todos.map(function(x){return ler(x,rel+"/"+x.name)})).then(ok);return}todos=todos.concat(l);mais()},function(){ok()})})()}
  })}
  if(entradas.length)Promise.all(entradas.map(function(en){return ler(en,en.name)})).then(function(){enviarListaPs4(lista,ps.caminho)});
  else enviarListaPs4(Array.prototype.map.call(e.dataTransfer.files,function(f){return{rel:f.name,file:f}}),ps.caminho);
 });
})();

var arquivosAbertos=false;
document.querySelector('nav button[data-p="pArquivos"]').addEventListener("click",function(){if(!arquivosAbertos){arquivosAbertos=true;listarPs4();listarPc()}});

// =================== Tela do PS4 no navegador ===================
var vendoTela=false;
function telaLigar(ligar){
 vendoTela=ligar;var area=$("telaArea"),velha=area.querySelector("img");
 // Trocar o src corta o MJPEG na hora; só tirar o <img> deixa a conexão aberta.
 if(velha){velha.onerror=null;velha.src="data:image/gif;base64,R0lGODlhAQABAAAAACw=";}
 area.innerHTML="";
 if(ligar){var img=document.createElement("img");img.alt="Tela do PS4";img.src="/tela.mjpg?q="+$("telaQual").value+"&t="+Date.now();
  img.onerror=function(){if(vendoTela)area.innerHTML='<div class="vazio">A tela não chegou. Toque em "Ver tela do PS4" de novo.</div>';vendoTela=false;$("telaVer").textContent="Ver tela do PS4"};
  area.appendChild(img)}
 else area.innerHTML='<div class="vazio">Toque em "Ver tela do PS4".</div>';
 $("telaVer").textContent=ligar?"Parar de ver":"Ver tela do PS4";
}
$("telaVer").onclick=function(){telaLigar(!vendoTela)};
$("telaQual").onchange=function(){if(vendoTela)telaLigar(true)};
$("telaCheia").onclick=function(){var a=$("telaArea");if(a.requestFullscreen)a.requestFullscreen();else if(a.webkitRequestFullscreen)a.webkitRequestFullscreen()};
// Som do PS4: PCM 16 bits estéreo 24 kHz sem fim em /tela.pcm, tocado em pedaços pelo WebAudio.
var somCtx=null,somParar=null,somFim=0;
function somLigar(ligar){
 if(somParar){somParar.abort();somParar=null}
 if(!ligar){if(somCtx){somCtx.close();somCtx=null}$("telaSom").innerHTML="&#128263; Ativar som";return}
 var AC=window.AudioContext||window.webkitAudioContext;
 if(!AC||!window.fetch||!window.AbortController){alert("Este navegador não toca o som do PS4.");return}
 somCtx=new AC();somFim=0;somCtx.resume();
 var ctrl=new AbortController();somParar=ctrl;$("telaSom").innerHTML="&#128266; Desligar som";
 var sobra=null;
 fetch("/tela.pcm?"+Date.now(),{signal:ctrl.signal,cache:"no-store"}).then(function(r){
  if(!r.ok||!r.body)throw new Error("sem som");
  var leitor=r.body.getReader();
  function ler(){return leitor.read().then(function(x){
   if(x.done||!somCtx)return;
   var v=x.value;
   if(sobra){var j=new Uint8Array(sobra.length+v.length);j.set(sobra);j.set(v,sobra.length);v=j;sobra=null}
   var n=Math.floor(v.length/4);if(v.length>n*4)sobra=v.slice(n*4);
   if(n>0){
    var dv=new DataView(v.buffer,v.byteOffset,n*4),buf=somCtx.createBuffer(2,n,24000),e=buf.getChannelData(0),d=buf.getChannelData(1);
    for(var i=0;i<n;i++){e[i]=dv.getInt16(i*4,true)/32768;d[i]=dv.getInt16(i*4+2,true)/32768}
    var agora=somCtx.currentTime;
    // Atraso entre 0,15 e 0,6 s: se acumular mais, pula para o agora.
    if(somFim<agora+0.05||somFim>agora+0.6)somFim=agora+0.15;
    var f=somCtx.createBufferSource();f.buffer=buf;f.connect(somCtx.destination);f.start(somFim);somFim+=buf.duration;
   }
   return ler()})}
  return ler();
 }).catch(function(){}).then(function(){if(somParar===ctrl)somLigar(false)});
}
$("telaSom").onclick=function(){somLigar(!somParar)};
// Sair da aba fecha a imagem: o PS4 para de capturar quando ninguém está vendo.
(function(){var bs=document.querySelectorAll("nav button");for(var i=0;i<bs.length;i++)bs[i].addEventListener("click",function(){if(this.dataset.p!=="pTela"&&vendoTela)telaLigar(false)})})();

// =================== Programa do PC (HyperNeivaPC.exe) ===================
var AG="http://127.0.0.1:8001",agente=false;
function ag(caminho,post){return fetch(AG+caminho,{method:post?"POST":"GET",cache:"no-store"}).then(function(r){return r.json()})}
function procurarAgente(){
 return ag("/oi").then(function(j){if(j.ok){agente=true;pc.modo="agente";pc.caminho="";prepararPainelPc();listarPc();acompanharTarefas()}}).catch(function(){agente=false;prepararPainelPc()});
}
function prepararPainelPc(){
 $("pcBanner").style.display=agente?"none":"block";
 var ags=document.querySelectorAll(".ag");for(var i=0;i<ags.length;i++)ags[i].style.display=agente?"inline-block":"none";
 $("pcEscolher").style.display=agente?"none":"";$("pcArquivos").style.display=agente?"none":"";$("pcTirar").style.display=agente?"none":"";
 if(agente){$("titPc").textContent="PC";$("dicaPc").textContent="Hyper Neiva PC conectado: unidades e pastas do computador à esquerda."}
}
$("pcProcurar").onclick=function(){procurarAgente().then(function(){if(!agente)alert("Não achei o programa. Abra o HyperNeivaPC.exe e deixe a janela aberta. Se o navegador perguntar, permita o acesso aos apps deste dispositivo.")})};
$("pcAtualizar").onclick=function(){listarPc()};

function juntarPc(a,b){return /\/$/.test(a)?a+b:a+"/"+b}
function paiPc(c){if(/^[A-Za-z]:\/?$/.test(c))return "";var i=c.replace(/\/$/,"").lastIndexOf("/");return i<=2?c.slice(0,3):c.slice(0,i)}

var listarPcSimples=listarPc;
listarPc=function(){
 if(pc.modo!=="agente")return listarPcSimples();
 var c=pc.caminho;$("camPc").textContent=c||"Este computador";
 var pedido=c?ag("/fs/lista?p="+encodeURIComponent(c)):ag("/fs/raizes");
 pedido.then(function(l){
  if(!Array.isArray(l))throw 0;
  pc.itens=c?ordenar(l):l.map(function(r){return{n:r.n,d:1,p:r.p,ic:r.tipo==="disco"?"&#128190;":"&#11088;",t:r.livre,info:r.tipo==="disco"?tamanho(r.livre)+" livres":""}});
  pc.sel={};
  desenhar($("listaPc"),pc.itens,pc.sel,function(it){if(it.d){pc.caminho=it.p||juntarPc(pc.caminho,it.n);listarPc()}});
 }).catch(function(){$("listaPc").innerHTML='<div class="vazio">Não abriu '+esc(c)+'</div>'});
};
var subirAntigo=$("pcSubir").onclick;
$("pcSubir").onclick=function(){if(pc.modo==="agente"){pc.caminho=paiPc(pc.caminho);listarPc()}else subirAntigo()};
function caminhoPc(it){return it.p||juntarPc(pc.caminho,it.n)}
$("pcNova").onclick=function(){if(!pc.caminho)return alert("Abra antes uma unidade ou pasta.");var n=prompt("Nome da pasta nova:");if(!n)return;ag("/fs/pasta?p="+encodeURIComponent(juntarPc(pc.caminho,n)),1).then(listarPc)};
$("pcRenomear").onclick=function(){var it=selecionados(pc)[0];if(!it||it.p)return;var n=prompt("Novo nome:",it.n);if(!n||n===it.n)return;
 ag("/fs/renomear?p="+encodeURIComponent(caminhoPc(it))+"&para="+encodeURIComponent(juntarPc(pc.caminho,n)),1).then(listarPc)};
// Sem o HyperNeivaPC.exe o navegador não pode apagar nada do PC: só tira os escolhidos da lista.
$("pcTirar").onclick=function(){var l=selecionados(pc);if(!l.length)return alert("Marque na lista do PC o que quer tirar (clique, Ctrl/Shift+clique ou Tudo).");
 var pref=pc.pasta?pc.pasta+"/":"";
 l.forEach(function(it){var al)HNPAG"
R"HNPAG(vo=pref+it.n;pc.virtual=pc.virtual.filter(function(e){return e.rel!==alvo&&e.rel.indexOf(alvo+"/")!==0})});
 listarPc()};
$("pcApagar").onclick=function(){
 if(!agente){if(pc.modo!=="fs")return $("pcTirar").click();return alert("Para apagar arquivos do PC, abra o HyperNeivaPC.exe (botão \"Baixar HyperNeivaPC.exe\" acima) e deixe a janela aberta.")}
 var l=selecionados(pc).filter(function(it){return!it.p});if(!l.length)return alert("Marque o que quer apagar no painel do PC (clique, Ctrl/Shift+clique ou Tudo).");
 if(!confirm("Mandar para a Lixeira do PC "+(l.length===1?'"'+l[0].n+'"':l.length+" itens")+"?"))return;
 var falhas=[];
 Promise.all(l.map(function(it){return ag("/fs/apagar?p="+encodeURIComponent(caminhoPc(it)),1).then(function(j){if(!j.ok)falhas.push(it.n)}).catch(function(){falhas.push(it.n)})})).then(function(){listarPc();if(falhas.length)alert("Não apagou "+falhas.length+" de "+l.length+": "+falhas.slice(0,15).join(", "))})};
$("pcAbrir").onclick=function(){selecionados(pc).forEach(function(it){ag("/fs/abrir?p="+encodeURIComponent(caminhoPc(it)),1)})};

// Cópias pelo programa: direto entre o PC e o PS4.
var enviarAntigo=$("pcEnviar").onclick,baixarAntigo=$("psBaixar").onclick;
$("pcEnviar").onclick=function(){
 if(pc.modo!=="agente")return enviarAntigo();
 var l=selecionados(pc).filter(function(it){return!it.p});if(!l.length)return alert("Selecione arquivos ou pastas no painel do PC.");
 l.forEach(function(it){ag("/copiar?dir=pcps4&ps4="+encodeURIComponent(location.host)+"&de="+encodeURIComponent(caminhoPc(it))+"&para="+encodeURIComponent(ps.caminho),1)});
 acompanharTarefas();
};
$("psBaixar").onclick=function(){
 if(pc.modo!=="agente")return baixarAntigo();
 var l=selecionados(ps);if(!l.length)return alert("Selecione arquivos ou pastas no painel do PS4.");
 if(!pc.caminho)return alert("Abra à esquerda a pasta do PC que vai receber.");
 l.forEach(function(it){ag("/copiar?dir=ps4pc&ps4="+encodeURIComponent(location.host)+"&de="+encodeURIComponent(juntar(ps.caminho,it.n))+"&para="+encodeURIComponent(pc.caminho),1)});
 acompanharTarefas();
};
var acompanhando=null,vistasOk={};
function acompanharTarefas(){
 if(acompanhando)return;
 acompanhando=setInterval(function(){
  ag("/tarefas").then(function(l){
   var f=$("fila"),algumaAtiva=false;
   l.forEach(function(t){
    var el=document.getElementById("ag"+t.id);
    if(!el){el=document.createElement("div");el.id="ag"+t.id;el.className="tar";el.innerHTML='<span></span><div class="barra"><div></div></div>';f.appendChild(el);f.classList.add("tem")}
    var pct=t.total?t.feito/t.total*100:(t.estado==="ok"?100:0);
    el.lastChild.firstChild.style.width=pct+"%";
    el.className="tar"+(t.estado==="ok"?" ok":t.estado==="erro"||t.estado==="cancelada"?" erro":"");
    el.firstChild.textContent=t.nome+": "+(t.estado==="fila"?"na fila":t.estado==="rodando"?tamanho(t.feito)+" de "+tamanho(t.total)+" ("+t.prontos+"/"+t.arquivos+" arquivos)"+(t.atual?" — "+t.atual:""):
     t.estado==="ok"?"pronto ("+t.arquivos+" arquivos)":t.estado==="cancelada"?"cancelada":"falhou: "+t.erro);
    if(t.estado==="fila"||t.estado==="rodando")algumaAtiva=true;
    if(t.estado==="ok"&&!vistasOk[t.id]){vistasOk[t.id]=1;listarPs4();listarPc()}
   });
   if(!algumaAtiva){clearInterval(acompanhando);acompanhando=null}
  }).catch(function(){});
 },700);
}

// Tela do PC no PS4 (o programa captura e manda).
var espVerificar=null;
$("espIniciar").onclick=function(){
 if(!agente){procurarAgente().then(function(){if(!agente)alert("Para mostrar a tela do PC no PS4, abra o HyperNeivaPC.exe neste computador (aba Arquivos tem o link).");else $("espIniciar").onclick()});return}
 var v=$("espQual").value.split(",");
 ag("/espelho/iniciar?ps4="+encodeURIComponent(location.host)+"&larg="+v[0]+"&fps="+v[1]+"&q="+v[2],1).then(function(){
  $("espInfo").textContent="Mandando a tela do PC...";
  if(!espVerificar)espVerificar=setInterval(function(){ag("/espelho/estado").then(function(e){
   $("espInfo").textContent=e.ativo?("No PS4: "+e.fps.toFixed(1)+" fps, "+(e.kbps/1000).toFixed(1)+" Mbit/s"+(e.erro?" — "+e.erro:"")):("Parado"+(e.erro?" ("+e.erro+")":""));
   if(!e.ativo){clearInterval(espVerificar);espVerificar=null}})},1000);
 });
};
$("espParar").onclick=function(){if(agente)ag("/espelho/parar",1)};

// Procura o programa quando abre Arquivos ou Tela.
var jaProcurou=false;
(function(){var bs=document.querySelectorAll("nav button");for(var i=0;i<bs.length;i++)bs[i].addEventListener("click",function(){
 if((this.dataset.p==="pArquivos"||this.dataset.p==="pTela")&&!jaProcurou){jaProcurou=true;procurarAgente()}})})();

// Controle sem sobreposição: começa grande e diminui até nenhum botão encostar em outro
// nem sair da tela. Roda ao abrir, ao girar o aparelho e ao mudar o tamanho da janela.
function controleSobrepoe(pad){
 var area=pad.getBoundingClientRect(),els=pad.querySelectorAll(".b,.stick,.touch"),r=[];
 for(var i=0;i<els.length;i++){var q=els[i].getBoundingClientRect();if(!q.width)continue;
  if(q.left<area.left-1||q.right>area.right+1||q.top<area.top-1||q.bottom>area.bottom+1)return true;r.push(q)}
 for(var a=0;a<r.length;a++)for(var b=a+1;b<r.length;b++){var x=r[a],y=r[b];
  if(x.left<y.right+2&&y.left<x.right+2&&x.top<y.bottom+2&&y.top<x.bottom+2)return true}
 return false;
}
function ajustarControle(){
 if(!$("pControle").classList.contains("ativo"))return;
 var pad=document.querySelector("#pControle .pad");
 for(var e=1.8;e>=0.4;e-=0.05){pad.style.setProperty("--e",e.toFixed(2));if(!controleSobrepoe(pad))break}
}
window.addEventListener("resize",function(){clearTimeout(window._aj);window._aj=setTimeout(ajustarControle,80)});
window.addEventListener("orientationchange",function(){setTimeout(ajustarControle,300)});
document.querySelector('nav button[data-p="pControle"]').addEventListener("click",function(){setTimeout(ajustarControle,0)});
ajustarControle();

// Teclado: Enter manda o texto (Shift+Enter quebra a linha).
$("texto").addEventListener("keydown",function(e){if(e.key==="Enter"&&!e.shiftKey&&!e.isComposing){e.preventDefault();$("enviarTexto").click()}});

// =================== IA (Gemini, ChatGPT, Groq) ===================
var iaVersao=-1,iaTimer=null,iaOpcoes=null,iaEstado=null;
function iaPost(rota,corpo){return fetch(rota,{method:"POST",body:corpo||"",cache:"no-store"}).then(function(r){return r.json()})}
function iaStatus(t,cls){var e=$("iaStatus");e.textContent=t||"";e.className=cls||""}
function iaLerOpcoes(){return iaOpcoes?Promise.resolve(iaOpcoes):fetch("/ia/opcoes").then(function(r){return r.json()}).then(function(o){iaOpcoes=o;return o})}
function iaProv(id){return iaOpcoes.provedores.filter(function(p){return p.id===id})[0]}
function iaPreencherSelect(id,lista,valor,usarTexto){var s=$(id);s.innerHTML="";lista.forEach(function(n,i){var o=document.createElement("option");o.value=usarTexto?n:i;o.textContent=n;s.appendChild(o)});s.value=valor}
function iaAtualizar(){
 Promise.all([iaLerOpcoes(),fetch("/ia/estado",{cache:"no-store"}).then(function(r){return r.json()})]).then(function(x){
  var o=x[0],e=x[1];iaEstado=e;
  if(e.ocupado||e.lendo)iaStatus(e.tarefa||"Trabalhando...","ocupado");else if(e.erro)iaStatus(e.erro,"erro");else iaStatus("");
  // Contas.
  var sc=$("iaConta"),atual=e.contas.map(function(c){return c.id+c.nome+c.modeloUsado}).join("|");
  if(sc.dataset.lista!==atual){sc.dataset.lista=atual;sc.innerHTML="";
   if(!e.contas.length){var op=document.createElement("option");op.textContent="nenhuma conta: cadastre na aba Chaves";sc.appendChild(op)}
   e.contas.forEach(function(c){var op=document.createElement("option");op.value=c.id;op.textContent=c.nome+" ("+c.provNome+")";sc.appendChild(op)})}
  if(e.contas.length)sc.value=e.contaAtiva;
  var ativa=e.contas[e.contaAtiva];
  $("iaContaInfo").textContent=ativa?ativa.modeloUsado+(iaProv(ativa.prov).imagem?"":" · sem imagem/voz: usa outra conta"):"";
  // Ajustes do provedor da conta escolhida.
  if(ativa){var pv=iaProv(ativa.prov);var dl=$("iaModelos");dl.innerHTML="";pv.modelos.forEach(function(m){var op=document.createElement("option");op.value=m;dl.appendChild(op)});
   if(document.activeElement!==$("iaModelo"))$("iaModelo").value=ativa.modeloUsado;
   var pvVoz=pv.voz?pv:o.provedores.filter(function(p){return p.voz&&e.contas.some(function(c){return c.prov===p.id})})[0];
   if(pvVoz)iaPreencherSelect("iaVoz",pvVoz.vozes,pvVoz.id===e.prov?e.voz:pvVoz.vozes[0],true)}
  iaPreencherSelect("iaModeloImg",o.modelosImg,e.modeloImg);iaPreencherSelect("iaAspecto",o.aspectos,e.aspecto);
  $("iaSempreVoz").checked=e.falar;
  if(e.versao===iaVersao)return;iaVersao=e.versao;
  var box=$("iaMsgs"),noFim=box.scrollTop+box.clientHeight>=box.scrollHeight-30;
  box.innerHTML="";
  if(!e.msgs.length)box.innerHTML='<div class="vazio" style="color:var(--fraco);font-size:13px">'+(e.contas.length?"Converse com a IA do Hyper Neiva. A resposta aparece aqui e na tela do PS4 (EXTRA &gt; Inteligência Artificial).":"Cadastre uma conta de IA na aba Chaves (Gemini, ChatGPT ou Groq).")+'</div>';
  e.msgs.forEach(function(m){
   var b=document.createElement("div");b.className="bolha "+m.q;
   if(m.q==="u"&&m.img==="tela"){var sm=document.createElement("small");sm.textContent="sobre a tela do PS4";b.appendChild(sm)}
   b.appendChild(document.createTextNode(m.t));
   if(m.img&&m.img!=="tela"){var im=document.createElement("img");im.src="/fs/baixar?p="+encodeURIComponent(m.img);im.alt="imagem gerada";im.onclick=function(){window.open(im.src,"_blank")};b.appendChild(im)}
   box.appendChild(b);
  });
  if(noFim)box.scrollTop=box.scrollHeight;
 }).catch(function(){iaStatus("Sem conexão com o PS4.","erro")});
}
function iaEnviar(rota){
 var t=$("iaTexto").value.trim();if(!t){iaStatus("Escreva alguma coisa primeiro.","erro");return}
 iaPost(rota,t).then(function(j){if(j.ok){$("iaTexto").value="";iaStatus("Enviado.","ocupado");iaAtualizar()}else iaStatus(j.erro||"Não foi.","erro")}).catch(function(){iaStatus("Sem conexão com o PS4.","erro")});
}
$("iaPerguntar").onclick=function(){iaEnviar("/ia/perguntar")};
$("iaTela").onclick=function(){iaEnviar("/ia/perguntar?tela=1")};
$("iaImagem").onclick=function(){iaEnviar("/ia/imagem")};
$("iaTexto").addEventListener("keydown",function(e){if(e.key==="Enter"&&!e.shiftKey&&!e.isComposing){e.preventDefault();iaEnviar("/ia/perguntar")}});
$("iaFalar").onclick=function(){iaPost("/ia/falar").then(function(j){if(!j.ok)iaStatus(j.erro,"erro")})};
$("iaParar").onclick=function(){iaPost("/ia/parar")};
$("iaOuvir").onclick=function(){var a=$("iaAudio");a.src="/fs/baixar?p="+encodeURIComponent("/data/HyperNeiva/ia/resposta.wav")+"&t="+Date.now();
 a.play().catch(function(){iaStatus("Ainda não há voz gerada. Use \"Ler em voz no PS4\" antes.","erro")})};
$("iaLimpar").onclick=function(){if(confirm("Apagar a conversa?"))iaPost("/ia/limpar").then(iaAtualizar)};
$("iaConta").onchange=function(){if(this.value!=="")iaPost("/ia/conta/usar?id="+this.value).then(iaAtualizar)};
$("iaModelo").onchange=function(){iaPost("/ia/config?modelo="+encodeURIComponent(this.value.trim())).then(iaAtualizar)};
$("iaVoz").onchange=function(){iaPost("/ia/config?voz="+encodeURIComponent(this.value))};
$("iaModeloImg").onchange=function(){iaPost("/ia/config?modeloImg="+this.value)};
$("iaAspecto").onchange=function(){iaPost("/ia/config?aspecto="+this.value)};
$("iaSempreVoz").onchange=function(){iaPost("/ia/config?falar="+(this.checked?1:0))};
(function(){var bs=document.querySelectorAll("nav button");for(var i=0;i<bs.length;i++)bs[i].addEventListener("click",function(){
 if(this.dataset.p==="pIa"){iaAtualizar();if(!iaTimer)iaTimer=setInterval(iaAtualizar,1000)}else if(iaTimer){clearInterval(iaTimer);iaTimer=null}})})();

// =================== Chaves de API (por categoria) ===================
function chMsg(t,erro){var e=$("chMsg");e.textContent=t||"";e.style.color=erro?"var(--o)":"var(--fraco)"}
function chSalvar(id,valor){return iaPost("/chaves/salvar?id="+encodeURIComponent(id),valor)}
var chAbertos={};
)HNPAG"
R"HNPAG(function el(tag,cls,txt){var e=document.createElement(tag);if(cls)e.className=cls;if(txt!==undefined)e.textContent=txt;return e}
function chDetalhes(cls,id,titulo,dir,dirOk){
 var d=el("details",cls);d.dataset.id=id;if(chAbertos[id])d.open=true;d.addEventListener("toggle",function(){chAbertos[id]=d.open});
 var sm=el("summary");var t=el("b",null,titulo);sm.appendChild(t);if(dir!==undefined){var sp=el("span",dirOk?"ok":"",dir);sm.appendChild(sp)}d.appendChild(sm);return d}
function chCampo(rotulo,input,botoes){var c=el("div","campo");c.appendChild(el("label",null,rotulo));var l=el("div","linha");l.appendChild(input);botoes.forEach(function(b){l.appendChild(b)});c.appendChild(l);return c}
function chBotao(txt,cls,fn){var b=el("button",cls,txt);b.onclick=fn;return b}
function chSenha(ph){var i=el("input");i.type="password";i.placeholder=ph;i.autocomplete="off";return i}

// Conta de IA (nova quando c é null).
function chFormConta(c,o){
 var box=el("div");
 var prov=el("select");o.provedores.forEach(function(p){var op=el("option",null,p.nome);op.value=p.id;prov.appendChild(op)});prov.value=c?c.prov:"gemini";
 var nome=el("input");nome.placeholder="nome (ex.: Gemini Trabalho)";nome.value=c?c.nome:"";
 var modelo=el("input");modelo.setAttribute("list","chModelos"+(c?c.id:"n"));modelo.placeholder="modelo (vazio = padrão)";modelo.value=c?c.modelo:"";
 var dl=el("datalist");dl.id="chModelos"+(c?c.id:"n");
 function sugestoes(){dl.innerHTML="";iaProvOp(prov.value).modelos.forEach(function(m){var op=el("option");op.value=m;dl.appendChild(op)});site.textContent="Chave em "+iaProvOp(prov.value).site+(iaProvOp(prov.value).imagem?"":" · o Groq não desenha nem fala")}
 var site=el("label");
 prov.onchange=function(){modelo.value="";sugestoes()};
 var chave=chSenha(c?"nova chave (vazio mantém ...)"+(c.fim?"":""):"cole a chave aqui");
 if(c&&c.fim)chave.placeholder="vazio mantém a atual (..."+c.fim+")";
 box.appendChild(chCampo("Provedor",prov,[]));box.appendChild(site);
 box.appendChild(chCampo("Nome",nome,[]));
 var mc=chCampo("Modelo",modelo,[]);mc.appendChild(dl);box.appendChild(mc);
 box.appendChild(chCampo("Chave da API",chave,[]));
 var acoes=el("div","acoes");
 acoes.appendChild(chBotao(c?"Salvar":"Adicionar conta","salvar",function(){
  var q="/ia/conta/salvar?id="+(c?c.id:-1)+"&prov="+prov.value+"&nome="+encodeURIComponent(nome.value.trim())+"&modelo="+encodeURIComponent(modelo.value.trim());
  iaPost(q,chave.value.trim()).then(function(j){chMsg(j.ok?(c?"Conta salva.":"Conta adicionada."):"Não salvei: "+j.erro,!j.ok);if(j.ok){chave.value="";chListar()}})}));
 if(c){
  if(!c.ativa)acoes.appendChild(chBotao("Usar esta na IA","",function(){iaPost("/ia/conta/usar?id="+c.id).then(function(){chMsg("A IA agora usa "+c.nome+".");chListar()})}));
  acoes.appendChild(chBotao("Apagar","",function(){if(confirm("Apagar a conta "+c.nome+" do PS4?"))iaPost("/ia/conta/apagar?id="+c.id).then(function(){chMsg("Conta apagada.");chListar()})}));
 }
 box.appendChild(acoes);sugestoes();
 return box;
}
function iaProvOp(id){return iaOpcoes.provedores.filter(function(p){return p.id===id})[0]}

function chListar(){
 Promise.all([iaLerOpcoes(),fetch("/chaves/lista",{cache:"no-store"}).then(function(r){return r.json()})]).then(function(x){
  var o=x[0],l=x[1],box=$("chCategorias");box.innerHTML="";
  l.categorias.forEach(function(cat){
   var resumo;
   if(cat.id==="ia")resumo=l.contas.length+" conta(s)";
   else{var cfg=cat.servicos.filter(function(sv){return sv.campos.every(function(f){return f.tem||/opcional/.test(f.dica)})}).length;resumo=cfg+" de "+cat.servicos.length+" configurado(s)"}
   var dc=chDetalhes("cat","cat-"+cat.id,cat.nome,resumo);
   if(cat.id==="ia"){
    l.contas.forEach(function(c){
     var ds=chDetalhes("serv","conta-"+c.id+"-"+c.nome,c.nome+" · "+c.provNome,"..."+c.fim,true);
     if(c.ativa){ds.querySelector("summary b").appendChild(el("span","emuso","em uso"))}
     ds.appendChild(chFormConta(c,o));dc.appendChild(ds);
    });
    var dn=chDetalhes("serv","conta-nova","+ Adicionar conta","Gemini, ChatGPT ou Groq");dn.appendChild(chFormConta(null,o));dc.appendChild(dn);
   }
   else{
    cat.servicos.forEach(function(sv){
     var n=sv.campos.filter(function(f){return f.tem}).length;
     var ds=chDetalhes("serv","serv-"+sv.id,sv.nome,n===sv.campos.length?"configurado":(n?n+" de "+sv.campos.length:"vazio"),n===sv.campos.length);
     ds.appendChild(el("div","campo",sv.uso));
     sv.campos.forEach(function(f){
      var inp=chSenha(f.tem?"vazio mantém a atual (..."+f.fim+")":"cole aqui");
      var bts=[chBotao("Salvar","salvar",function(){if(!inp.value.trim())return;chSalvar(f.id,inp.value).then(function(j){chMsg(j.ok?sv.nome+": "+f.nome+" salvo.":"Não salvei: "+j.erro,!j.ok);if(j.ok)chListar()})})];
      if(f.tem)bts.push(chBotao("Apagar","",function(){if(confirm("Apagar "+sv.nome+" · "+f.nome+"?"))chSalvar(f.id,"").then(function(){chMsg("Apagado.");chListar()})}));
      ds.appendChild(chCampo(f.nome+(f.dica?" ("+f.dica+")":"")+(f.tem?" — configurado":""),inp,bts));
     });
     dc.appendChild(ds);
    });
    if(cat.id==="outras"){
     var dn2=chDetalhes("serv","outra-nova","+ Adicionar chave","");
     var nome=el("input");nome.placeholder="nome (ex.: openweather)";var val=chSenha("chave");
     dn2.appendChild(chCampo("Nome",nome,[]));dn2.appendChild(chCampo("Chave",val,[chBotao("Adicionar","salvar",function(){var n=nome.value.trim();if(!n||!val.value.trim())return;
      chSalvar(n,val.value).then(function(j){chMsg(j.ok?n+" adicionada.":"Não adicionei: "+j.erro,!j.ok);if(j.ok)chListar()})})]));
     dc.appendChild(dn2);
    }
   }
   box.appendChild(dc);
  });
 }).catch(function(){chMsg("Sem conexão com o PS4.",true)});
}

// Importar do Hyper Neiva de PC: IA vira conta, o resto vai para o serviço certo.
function chDestino(nome){var n=nome.toLowerCase().replace(/[^a-z0-9_.-]/g,"");
 if(n.indexOf("gemini")>=0)return{conta:"gemini"};if(n.indexOf("chatgpt")>=0||n.indexOf("openai")>=0)return{conta:"openai"};if(n.indexOf("groq")>=0)return{conta:"groq"};
 if(n.indexOf("dropbox")>=0)return{campo:"dropbox_token"};if(n==="google")return{campo:"google"};if(n.indexOf("tmdb")>=0)return{campo:"tmdb"};
 if(n.indexOf("steamgrid")>=0)return{campo:"steamgriddb"};return n?{campo:n}:null}
$("chBuscarPc").onclick=function(){
 var box=$("chImportar");box.textContent="Lendo...";
 (agente?Promise.resolve():procurarAgente()).then(function(){
  if(!agente){box.textContent="Abra o HyperNeivaPC.exe neste computador (link na aba Arquivos) e tente de novo.";return}
  return fetch(AG+"/chaves/pc",{method:"POST",cache:"no-store"}).then(function(r){return r.json()}).then(function(j){
   if(!j.ok){box.textContent="Não achei as chaves do Hyper Neiva de PC ("+j.erro+").";return}
   box.innerHTML="";var itens=[];
   j.chaves.forEach(function(k){
    var alvo=chDestino(k.nome);if(!k.chave||!alvo)return;
    var lb=el("label"),cb=el("input");cb.type="checkbox";cb.checked=true;lb.appendChild(cb);
    lb.appendChild(document.createTextNode(k.nome+" (..."+k.chave.slice(-4)+") → "+(alvo.conta?"conta de IA "+alvo.conta:alvo.campo)));box.appendChild(lb);
    itens.push({cb:cb,alvo:alvo,nome:k.nome,chave:k.chave});
    if(alvo.campo==="dropbox_token"&&k.app_id){var lb2=el("label"),cb2=el("input");cb2.type="checkbox";cb2.checked=true;lb2.appendChild(cb2);
     lb2.appendChild(document.createTextNode("Dropbox App key (..."+k.app_id.slice(-4)+") → dropbox_appkey"));box.appendChild(lb2);itens.push({cb:cb2,alvo:{campo:"dropbox_appkey"},chave:k.app_id})}
   });
   if(!itens.length){box.textContent="O Hyper Neiva de PC não tem chaves preenchidas.";return}
   box.appendChild(chBotao("Importar as marcadas","salvar",function(){var ok=0,falhas=[];
    itens.filter(function(i){return i.cb.checked}).reduce(function(pr,i){return pr.then(function(){
     var pedido=i.alvo.conta?iaPost("/ia/conta/salvar?id=-1&prov="+i.alvo.conta+"&nome="+encodeURIComponent(i.nome+" (PC)"),i.chave):chSalvar(i.alvo.campo,i.chave);
     return pedido.then(function(r){if(r.ok)ok++;else falhas.push((i.alvo.conta||i.alvo.campo)+": "+r.erro)})})},Promise.resolve())
    .then(function(){itens.forEach(function(i){i.chave=""});box.textContent="";chMsg(ok+" chave(s) importada(s)."+(falhas.length?" Falharam: "+falhas.join("; "):""),falhas.length>0);chListar()})}));
  });
 }).catch(function(){box.textContent="Não consegui falar com o Hyper Neiva PC."});
};
(function(){var bs=document.querySelectorAll("nav button");for(var i=0;i<bs.length;i++)bs[i].addEventListener("click",function(){if(this.dataset.p==="pChaves")chListar()})})();
})();
</script>
</body>
</html>
)HNPAG"
;
