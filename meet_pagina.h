// Página do Meet do Hyper Neiva (servida pelo próprio PS4 em https://<ip>:8443/meet).
// Chat, áudio e câmera. Mídia: WebRTC direto entre os navegadores quando dá; senão (e sempre
// para o PS4) pelo PS4, em WebSocket: quadros JPEG e áudio PCM 16 kHz.
// Salas permanentes, entrada livre ou com aprovação do dono (PS4 ou quem criou pelo navegador).
#pragma once
static const char MEET_PAGINA[] = R"HNMEET(<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hyper Neiva Meet</title>
<style>
:root{--f:#0f1216;--p:#1b2027;--b:#2a313a;--t:#e8ecf1;--s:#8a96a3;--a:#29b6f6;--r:#e53935}
*{box-sizing:border-box}body{margin:0;background:var(--f);color:var(--t);font:15px system-ui,sans-serif;height:100vh;display:flex;flex-direction:column}
header{display:flex;align-items:center;gap:10px;padding:10px 14px;background:var(--p);border-bottom:1px solid var(--b)}
header b{font-size:17px}header .s{color:var(--s);font-size:13px;margin-left:auto}header .tg{font-size:12px;color:var(--s);background:#11161c;padding:3px 8px;border-radius:20px}
main{flex:1;display:flex;min-height:0}
#grade{flex:1;display:grid;gap:8px;padding:8px;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));align-content:start;overflow:auto}
.tile{position:relative;background:#000;border-radius:10px;aspect-ratio:4/3;overflow:hidden;border:2px solid transparent}
.tile.fala{border-color:var(--a)}.tile img,.tile video{width:100%;height:100%;object-fit:cover;display:block}
.tile .n{position:absolute;left:8px;bottom:8px;background:#000a;padding:3px 8px;border-radius:6px;font-size:13px}
.tile .av{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:48px;color:#fff;background:#24303c}
.tile.semcam img,.tile.semcam video{visibility:hidden}
.tile .m{position:absolute;right:8px;top:8px;background:#000b;width:32px;height:32px;border-radius:50%;display:flex;align-items:center;justify-content:center}
.tile .m svg{width:18px;height:18px;fill:#ff6b6b}
aside{width:320px;display:flex;flex-direction:column;background:var(--p);border-left:1px solid var(--b)}
#pedidos:empty{display:none}#pedidos{padding:8px;border-bottom:1px solid var(--b);background:#2a2110}
#pedidos div{display:flex;align-items:center;gap:6px;margin:4px 0}#pedidos span{flex:1}
#pedidos button{padding:6px 10px;font-size:13px}#pedidos .ok{background:#2e7d32;border-color:#2e7d32}#pedidos .nao{background:var(--r);border-color:var(--r)}
#msgs{flex:1;overflow:auto;padding:10px}#msgs div{margin:0 0 8px}#msgs i{color:var(--s);font-style:normal;font-size:12px}
#msgs b{color:var(--a)}form.c{display:flex;gap:6px;padding:8px;border-top:1px solid var(--b)}
input,select,button{font:inherit;border-radius:8px;border:1px solid var(--b);background:#11161c;color:var(--t);padding:9px 11px}
input{flex:1;min-width:0}button{cursor:pointer;background:#24303c}
footer{display:flex;gap:14px;justify-content:center;align-items:center;padding:12px;background:var(--p);border-top:1px solid var(--b)}
.ic{width:54px;height:54px;border-radius:50%;padding:0;display:flex;align-items:center;justify-content:center;background:#2b3542;border:0;position:relative}
.ic svg{width:26px;height:26px;fill:#fff}.ic:hover{filter:brightness(1.15)}
.ic.off{background:var(--r)}.ic.sair{background:var(--r);width:64px;border-radius:28px}
.ic .dica{position:absolute;bottom:62px;left:50%;transform:translateX(-50%);background:#000c;color:#fff;font-size:12px;padding:3px 8px;border-radius:6px;white-space:nowrap;opacity:0;pointer-events:none;transition:opacity .15s}
.ic:hover .dica{opacity:1}
.cx{max-width:460px;margin:8vh auto;padding:24px;background:var(--p);border-radius:14px;border:1px solid var(--b)}
.cx h1{margin:0 0 6px;font-size:22px}.cx p{color:var(--s)}.cx label{display:block;margin:14px 0 6px;color:var(--s);font-size:13px}
.cx input,.cx select{width:100%}.cx button{width:100%;margin-top:16px;background:var(--a);color:#06131c;border:0;padding:12px}
.cx .link{word-break:break-all;background:#11161c;padding:10px;border-radius:8px;color:var(--a);font-size:13px}
.lista a{display:block;padding:10px;margin:6px 0;background:#11161c;border-radius:8px;color:var(--t);text-decoration:none}
.lista a i{color:var(--s);font-style:normal;font-size:13px}
.giro{width:38px;height:38px;border:4px solid #2a313a;border-top-color:var(--a);border-radius:50%;animation:g 1s linear infinite;margin:18px auto}
@keyframes g{to{transform:rotate(360deg)}}
@media(max-width:760px){main{flex-direction:column}aside{width:auto;height:38vh;border-left:0;border-top:1px solid var(--b)}.ic{width:48px;height:48px}}
</style></head><body>
<div id="raiz"></div>
<script>
const $=s=>document.querySelector(s), raiz=$('#raiz');
const url=new URL(location.href), partes=location.pathname.split('/').filter(Boolean);
const sala=partes[1]||'', teste=url.searchParams.has('teste');
let chave=url.searchParams.get('k')||'';
// código de dono: chega no link de quem criou a sala e fica guardado só neste navegador
let dono=url.searchParams.get('d')||'';
try{if(dono)localStorage.setItem('hnDono:'+sala,dono);else dono=localStorage.getItem('hnDono:'+sala)||''}catch(e){}
if(url.searchParams.has('d')){url.searchParams.delete('d');history.replaceState(null,'',url.pathname+url.search)}
function esc(t){return String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
const IC={
 mic:'<svg viewBox="0 0 24 24"><path d="M12 14a3 3 0 0 0 3-3V5a3 3 0 0 0-6 0v6a3 3 0 0 0 3 3zm5-3a5 5 0 0 1-10 0H5a7 7 0 0 0 6 6.92V21h2v-3.08A7 7 0 0 0 19 11h-2z"/></svg>',
 micOff:'<svg viewBox="0 0 24 24"><path d="M19 11h-2c0 .74-.16 1.43-.43 2.05l1.47 1.47A6.96 6.96 0 0 0 19 11zM15 11.16V5a3 3 0 0 0-5.94-.6L15 10.34zM4.27 3 3 4.27l6 6V11a3 3 0 0 0 4.47 2.61l1.66 1.66A4.98 4.98 0 0 1 7 11H5a7 7 0 0 0 6 6.92V21h2v-3.08c.9-.13 1.75-.43 2.52-.86L19.73 21 21 19.73 4.27 3z"/></svg>',
 cam:'<svg viewBox="0 0 24 24"><path d="M17 10.5V7a1 1 0 0 0-1-1H4a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-3.5l4 4v-11l-4 4z"/></svg>',
 camOff:'<svg viewBox="0 0 24 24"><path d="M21 6.5l-4 4V7a1 1 0 0 0-1-1H9.82L21 17.18V6.5zM3.27 2 2 3.27 4.73 6H4a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h12c.21 0 .39-.08.55-.18L19.73 21 21 19.73 3.27 2z"/></svg>',
 link:'<svg viewBox="0 0 24 24"><path d="M3.9 12a3.1 3.1 0 0 1 3.1-3.1h4V7H7a5 5 0 0 0 0 10h4v-1.9H7A3.1 3.1 0 0 1 3.9 12zM8 13h8v-2H8v2zm9-6h-4v1.9h4a3.1 3.1 0 0 1 0 6.2h-4V17h4a5 5 0 0 0 0-10z"/></svg>',
 sair:'<svg viewBox="0 0 24 24"><path d="M12 9c-1.6 0-3.15.25-4.6.72v3.1c0 .39-.23.74-.56.9-.98.49-1.87 1.12-2.66 1.85a.99.99 0 0 1-1.41-.02L.29 13.08a1 1 0 0 1 0-1.41C3.34 8.78 7.46 7 12 7s8.66 1.78 11.71 4.67a1 1 0 0 1 0 1.41l-2.48 2.47a1 1 0 0 1-1.41.02c-.79-.73-1.68-1.36-2.66-1.85a1 1 0 0 1-.56-.9v-3.1A15 15 0 0 0 12 9z"/></svg>'};
function linkConvite(){return location.origin+'/meet/'+sala+(chave?'?k='+chave:'')}

// ---------- entrada (lista de salas abertas e criar sala)
async function inicio(){
  let salas=[];try{salas=await (await fetch('/meet/salas')).json()}catch(e){}
  raiz.innerHTML=`<div class="cx"><h1>Hyper Neiva Meet</h1><p>Chamada de vídeo servida pelo PS4. Sem servidor no meio.</p>
  <div class="lista">${salas.length?salas.map(s=>`<a href="/meet/${encodeURIComponent(s.nome)}">${esc(s.nome)} <i>${s.pessoas?'('+s.pessoas+' na chamada)':'(vazia)'}${s.aprovacao?' · pede aprovação':''}</i></a>`).join(''):'<p>Nenhuma sala aberta.</p>'}</div>
  <label>Nome da sala nova</label><input id="ns" maxlength="24" placeholder="ex.: familia">
  <label>Tipo</label><select id="tp"><option value="0">Aberta (quem tem o link entra)</option><option value="1">Privada (link com código)</option></select>
  <label>Entrada</label><select id="ap"><option value="0">Livre (entra direto)</option><option value="1">Pedir aprovação (você aceita cada pessoa)</option></select>
  <p style="font-size:13px">O link da sala é permanente: vale sempre que o Hyper Neiva estiver aberto no PS4.</p>
  <button id="cr">Criar sala</button></div>`;
  $('#cr').onclick=async()=>{const n=$('#ns').value.trim();if(!n)return;
    const r=await (await fetch('/meet/criar?nome='+encodeURIComponent(n)+'&privada='+$('#tp').value+'&aprovacao='+$('#ap').value,{method:'POST'})).json();
    if(r.erro){alert(r.erro);return}
    try{localStorage.setItem('hnDono:'+r.sala,r.d)}catch(e){}
    location.href='/meet/'+encodeURIComponent(r.sala)+(r.k?'?k='+r.k:'')};
}

// ---------- sala
let ws,meuId=-1,meuNome='',stream=null,micOn=true,camOn=true,actx=null,souDono=false,entrou=false;
const peers={};   // id -> {nome, tile, img, video, pc, p2p, t}
function tileDe(id,nome){
  let p=peers[id];if(!p){p=peers[id]={nome,p2p:false,t:0}}
  if(!p.tile){p.tile=document.createElement('div');p.tile.className='tile';
    p.tile.innerHTML=`<div class="av">${esc((nome||'?')[0].toUpperCase())}</div><img hidden><video autoplay playsinline hidden></video><span class="n">${esc(nome)}</span><span class="m" hidden>${IC.micOff}</span>`;
    p.img=p.tile.querySelector('img');p.video=p.tile.querySelector('video');$('#grade').appendChild(p.tile)}
  return p}
function estadoTile(p,mic,cam){if(!p||!p.tile)return;p.tile.querySelector('.m').hidden=mic!==false;p.tile.classList.toggle('semcam',cam===false)}
const pend=[];function chat(quem,txt,sis){if(!$('#msgs')){pend.push([quem,txt,sis]);return}const d=document.createElement('div');d.innerHTML=sis?`<i>${esc(txt)}</i>`:`<b>${esc(quem)}:</b> ${esc(txt)}`;$('#msgs').appendChild(d);$('#msgs').scrollTop=1e9}
function botao(id,icone,dica,cls){return `<button class="ic ${cls||''}" id="${id}" aria-label="${dica}">${icone}<span class="dica">${dica}</span></button>`}
function atualizarBotoes(){
  $('#bm').className='ic'+(micOn?'':' off');$('#bm').innerHTML=(micOn?IC.mic:IC.micOff)+`<span class="dica">${micOn?'Desligar microfone':'Ligar microfone'}</span>`;
  $('#bc').className='ic'+(camOn?'':' off');$('#bc').innerHTML=(camOn?IC.cam:IC.camOff)+`<span class="dica">${camOn?'Desligar câmera':'Ligar câmera'}</span>`;
  estadoTile(peers.eu,micOn,camOn&&!!(stream&&stream.getVideoTracks().length));
  if(ws&&ws.readyState==1&&entrou)ws.send(JSON.stringify({t:'estado',mic:micOn,cam:camOn&&!!(stream&&stream.getVideoTracks().length)}));
}

async function midiaLocal(){
  if(teste){ // mídia de teste (sem câmera): quadro animado e um tom
    const c=document.createElement('canvas');c.width=320;c.height=240;const g=c.getContext('2d');let k=0;
    setInterval(()=>{g.fillStyle=`hsl(${k++*3%360},70%,40%)`;g.fillRect(0,0,320,240);g.fillStyle='#fff';g.font='28px sans-serif';g.fillText(meuNome+' '+k,20,130)},100);
    const s=c.captureStream(10);const a=new AudioContext();const o=a.createOscillator();o.frequency.value=220+Math.random()*200;const d=a.createMediaStreamDestination();
    const gn=a.createGain();gn.gain.value=0.05;o.connect(gn);gn.connect(d);o.start();s.addTrack(d.stream.getAudioTracks()[0]);return s}
  try{return await navigator.mediaDevices.getUserMedia({video:{width:320,height:240,frameRate:15},audio:{echoCancellation:true,noiseSuppression:true,autoGainControl:true}})}
  catch(e){try{return await navigator.mediaDevices.getUserMedia({audio:true})}catch(e2){chat('','Sem câmera e microfone (permita no navegador). Só chat.',1);return null}}
}

async function entrarSala(){
  try{meuNome=localStorage.getItem('hnNome')||''}catch(e){}
  raiz.innerHTML=`<div class="cx"><h1>Sala ${esc(decodeURIComponent(sala))}</h1><label>Seu nome</label><input id="nm" maxlength="20" value="${esc(meuNome)}"><button id="ok">Entrar</button></div>`;
  await new Promise(r=>{$('#ok').onclick=r;$('#nm').onkeydown=e=>{if(e.key=='Enter')r()}});
  meuNome=($('#nm').value.trim()||'Convidado').slice(0,20);try{localStorage.setItem('hnNome',meuNome)}catch(e){}
  actx=new (window.AudioContext||window.webkitAudioContext)();actx.resume();
  raiz.innerHTML='<div class="cx"><div class="giro"></div><p style="text-align:center">Abrindo câmera e microfone...</p></div>';
  stream=await midiaLocal();
  conectar();
}

function montarSala(){
  raiz.innerHTML=`<header><b>${esc(decodeURIComponent(sala))}</b><span class="tg" id="tipo"></span><span class="s" id="st">na sala</span></header>
  <main><div id="grade"></div><aside><div id="pedidos"></div><div id="msgs"></div><form class="c" id="fc"><input id="tx" placeholder="Mensagem" maxlength="500"><button>Enviar</button></form></aside></main>
  <footer>${botao('bm',IC.mic,'Desligar microfone')}${botao('bc',IC.cam,'Desligar câmera')}${botao('bl',IC.link,'Copiar link de convite')}${botao('bs',IC.sair,'Sair da chamada','sair')}</footer>`;
  document.addEventListener('click',()=>actx&&actx.resume());
  $('#fc').onsubmit=e=>{e.preventDefault();const t=$('#tx').value.trim();if(t&&ws.readyState==1){ws.send(JSON.stringify({t:'chat',m:t}));$('#tx').value=''}};
  $('#bm').onclick=()=>{micOn=!micOn;stream&&stream.getAudioTracks().forEach(t=>t.enabled=micOn);atualizarBotoes()};
  $('#bc').onclick=()=>{camOn=!camOn;stream&&stream.getVideoTracks().forEach(t=>t.enabled=camOn);atualizarBotoes()};
  $('#bl').onclick=()=>{const l=linkConvite();(navigator.clipboard?navigator.clipboard.writeText(l):Promise.reject()).then(()=>chat('','Link de convite copiado: '+l,1),()=>chat('','Link de convite: '+l,1))};
  $('#bs').onclick=()=>{ws&&ws.close();location.href='/meet'};
}

function aguardando(m){
  raiz.innerHTML=`<div class="cx"><h1>Sala ${esc(decodeURIComponent(sala))}</h1><div class="giro"></div><p style="text-align:center">${esc(m)}</p><button id="cancela" style="background:#24303c;color:#e8ecf1">Desistir</button></div>`;
  $('#cancela').onclick=()=>{ws&&ws.close();location.href='/meet'};
}
function erroTela(m){if(entrou){chat('',m,1);$('#st').textContent=m;return}
  raiz.innerHTML=`<div class="cx"><h1>Sala ${esc(decodeURIComponent(sala))}</h1><p>${esc(m)}</p><button onclick="location.href='/meet'">Voltar</button></div>`}

function pedido(id,nome){if(!$('#pedidos')||$('#pd'+id))return;
  const d=document.createElement('div');d.id='pd'+id;d.innerHTML=`<span><b>${esc(nome)}</b> quer entrar</span><button class="ok">Aceitar</button><button class="nao">Recusar</button>`;
  d.querySelector('.ok').onclick=()=>{ws.send(JSON.stringify({t:'aprovar',id,ok:true}));d.remove()};
  d.querySelector('.nao').onclick=()=>{ws.send(JSON.stringify({t:'aprovar',id,ok:false}));d.remove()};
  $('#pedidos').appendChild(d);try{new Notification('Hyper Neiva Meet',{body:nome+' quer entrar'})}catch(e){}}

function comecar(m){
  entrou=true;meuId=m.id;souDono=m.dono;if(m.k)chave=m.k;
  montarSala();
  $('#tipo').textContent=(m.privada?'privada':'aberta')+(m.aprovacao?' · com aprovação':'')+(souDono?' · você é o dono':'');
  pend.splice(0).forEach(a=>chat(...a));
  const eu=tileDe('eu',meuNome+' (você)');if(stream&&stream.getVideoTracks().length){eu.video.srcObject=stream;eu.video.muted=true;eu.video.hidden=false}
  m.pessoas.forEach(p=>{const t=tileDe(p.id,p.nome);t.ps4=!!p.ps4;estadoTile(t,p.mic,p.cam);if(!p.ps4&&meuId>p.id)ligarRtc(p.id,true)});
  (m.pedidos||[]).forEach(p=>pedido(p.id,p.nome));
  if(souDono&&m.aprovacao)chat('','Você é o dono: os pedidos de entrada aparecem aqui em cima.',1);
  atualizarBotoes();enviarMidia();
}

function conectar(){
  ws=new WebSocket(`wss://${location.host}/ws/${sala}?k=${encodeURIComponent(chave)}&d=${encodeURIComponent(dono)}&nome=${encodeURIComponent(meuNome)}`);ws.binaryType='arraybuffer';
  ws.onclose=()=>{if(entrou){$('#st').textContent='desconectado';for(const id in peers)if(id!='eu'){peers[id].pc&&peers[id].pc.close()}}};
  ws.onmessage=e=>{if(typeof e.data!='string'){recebeMidia(new Uint8Array(e.data));return}
    const m=JSON.parse(e.data);
    if(m.t=='bemvindo')comecar(m)
    else if(m.t=='aguardando')aguardando(m.m)
    else if(!entrou){if(m.t=='erro')erroTela(m.m);return}
    else if(m.t=='entrou'){const t=tileDe(m.id,m.nome);if(m.ps4)t.ps4=true;chat('',m.nome+' entrou',1)}
    else if(m.t=='saiu'){const p=peers[m.id];if(p){p.pc&&p.pc.close();p.tile&&p.tile.remove();delete peers[m.id];chat('',m.nome+' saiu',1)}}
    else if(m.t=='estado')estadoTile(peers[m.id],m.mic,m.cam)
    else if(m.t=='pedido')pedido(m.id,m.nome)
    else if(m.t=='pedido_fim'){const d=$('#pd'+m.id);d&&d.remove()}
    else if(m.t=='chat')chat(m.nome,m.m)
    else if(m.t=='rtc')rtcSinal(m)
    else if(m.t=='erro')erroTela(m.m)
    else if(m.t=='expulso'){chat('','Você foi removido da sala.',1);ws.close()}};
}

// ---------- mídia pelo PS4 (quadros JPEG e áudio PCM 16 kHz)
let ultimaFala={};
function recebeMidia(b){const tipo=b[0],de=b[1],dados=b.subarray(2),p=peers[de];if(!p||!p.tile)return;
  if(tipo==1){if(p.p2p)return;const url=URL.createObjectURL(new Blob([dados],{type:'image/jpeg'}));const ant=p.img.src;p.img.onload=()=>{ant&&URL.revokeObjectURL(ant)};p.img.src=url;p.img.hidden=false}
  else if(tipo==2){if(p.p2p&&!p.ps4)return;tocar(p,new Int16Array(dados.buffer,dados.byteOffset,dados.byteLength>>1));p.tile.classList.add('fala');clearTimeout(ultimaFala[de]);ultimaFala[de]=setTimeout(()=>p.tile.classList.remove('fala'),400)}}
function tocar(p,pcm){if(!actx)return;const buf=actx.createBuffer(1,pcm.length,16000),c=buf.getChannelData(0);for(let i=0;i<pcm.length;i++)c[i]=pcm[i]/32768;
  const s=actx.createBufferSource();s.buffer=buf;s.connect(actx.destination);const agora=actx.currentTime;if(p.t<agora+0.05||p.t>agora+0.6)p.t=agora+0.08;s.start(p.t);p.t+=buf.duration}
function enviarMidia(){
  if(!stream)return;
  // vídeo: 320x240 JPEG ~8 fps
  const v=document.createElement('video');v.srcObject=stream;v.muted=true;v.playsInline=true;v.play();const c=document.createElement('canvas');c.width=320;c.height=240;const g=c.getContext('2d');
  setInterval(()=>{if(!camOn||ws.readyState!=1||ws.bufferedAmount>200000||!stream.getVideoTracks().length)return;g.drawImage(v,0,0,320,240);
    c.toBlob(bl=>bl&&bl.arrayBuffer().then(a=>{const o=new Uint8Array(a.byteLength+2);o[0]=1;o.set(new Uint8Array(a),2);ws.send(o)}),'image/jpeg',0.6)},125);
  // áudio: 16 kHz mono, blocos de ~64 ms
  if(!stream.getAudioTracks().length)return;
  const src=actx.createMediaStreamSource(stream),proc=actx.createScriptProcessor(2048,1,1),taxa=actx.sampleRate/16000;
  src.connect(proc);const mudo=actx.createGain();mudo.gain.value=0;proc.connect(mudo);mudo.connect(actx.destination);
  proc.onaudioprocess=e=>{if(!micOn||ws.readyState!=1)return;const i=e.inputBuffer.getChannelData(0),n=Math.floor(i.length/taxa),o=new Uint8Array(2+n*2),pc=new Int16Array(o.buffer,2,n);
    o[0]=2;for(let k=0;k<n;k++){const x=i[Math.floor(k*taxa)];pc[k]=Math.max(-1,Math.min(1,x))*32767}ws.send(o)};
}

// ---------- WebRTC direto entre navegadores (sem STUN externo: rede local / IPs diretos)
function ligarRtc(id,oferecer){const p=peers[id];if(!p||p.pc||!window.RTCPeerConnection)return;
  const pc=p.pc=new RTCPeerConnection({iceServers:[]});
  stream&&stream.getTracks().forEach(t=>pc.addTrack(t,stream));
  pc.onicecandidate=e=>e.candidate&&ws.send(JSON.stringify({t:'rtc',para:id,ice:e.candidate}));
  pc.ontrack=e=>{p.video.srcObject=e.streams[0];p.video.hidden=false;p.img.hidden=true};
  pc.onconnectionstatechange=()=>{const ok=pc.connectionState=='connected';if(ok!=p.p2p){p.p2p=ok;ws.send(JSON.stringify({t:'p2p',com:id,ok}));if(!ok){p.video.hidden=true}}};
  if(oferecer)pc.createOffer().then(o=>pc.setLocalDescription(o)).then(()=>ws.send(JSON.stringify({t:'rtc',para:id,sdp:pc.localDescription})));
  setTimeout(()=>{if(!p.p2p&&p.pc){p.pc.close();p.pc=null}},8000);
}
async function rtcSinal(m){const id=m.de;if(!peers[id])return;if(m.sdp&&m.sdp.type=='offer'&&!peers[id].pc)ligarRtc(id,false);const pc=peers[id].pc;if(!pc)return;
  try{if(m.sdp){await pc.setRemoteDescription(m.sdp);if(m.sdp.type=='offer'){const r=await pc.createAnswer();await pc.setLocalDescription(r);ws.send(JSON.stringify({t:'rtc',para:id,sdp:pc.localDescription}))}}
  else if(m.ice)await pc.addIceCandidate(m.ice)}catch(e){}}

if(!window.isSecureContext&&!teste)raiz.innerHTML='<div class="cx"><h1>Abra pelo link https</h1><p>A câmera e o microfone só funcionam em https.</p></div>';
else if(sala)entrarSala();else inicio();
</script></body></html>)HNMEET";
