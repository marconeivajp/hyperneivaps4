/* Escritório 3D - rede de amigos
   Conversa com o servidor de encontro (local ou Cloudflare Worker), mostra
   os amigos online, entrada com senha, os bonecos deles se movendo, voz,
   câmera, perfil, enfeites e terreno do escritório. */

(function () {
  const CFG = window.ESC_REDE || {};
  const URL_REDE = CFG.url || "";
  const LS_ID = "esc.rede.id";
  const LS_NOME = "esc.rede.nome";
  const LS_LIVRE = "esc.rede.livre";
  const LS_PERFIL = "esc.rede.perfil";
  const LS_DECOR = "esc.decor";
  const LS_TERRENO = "esc.terreno";
  const DECOR_IDS = ["planta", "aquario", "ventilador", "globo", "luminaria"];
  const TERRENO_IDS = ["jardim", "praia", "neve", "campo"];
  const STUN = [{ urls: "stun:stun.l.google.com:19302" }];

  const $ = id => document.getElementById(id);
  let Esc = null;
  let ws = null, conectado = false, tentativa = 0, fechando = false;
  let meuId = null, meuNome = "", meuPerfil = null;
  let escritorio = { livre: true, senhaHash: null };
  let sala = null;
  const naSala = new Map();
  let presenca = [];
  let timerPos = null;
  let posRecebidas = 0;
  let saiuRecebidos = 0;
  let decor = [];
  let terreno = "jardim";
  const midiaLocal = { mic: false, cam: false };
  let streamMic = null, streamCam = null;
  const peers = new Map();

  /* ---------- utilidades ---------- */
  function hex(bytes) { let s = ""; for (const b of bytes) s += b.toString(16).padStart(2, "0"); return s; }
  async function hashSenha(txt) {
    if (!txt) return null;
    try {
      const h = await crypto.subtle.digest("SHA-256", new TextEncoder().encode("esc3d:" + txt));
      return hex(new Uint8Array(h));
    } catch (e) { return "x" + txt; }
  }
  function idNovo() { const b = new Uint8Array(12); crypto.getRandomValues(b); return hex(b); }
  function corDeId(id) {
    const roupas = ["#F1521B", "#2F6FD1", "#3DA36B", "#E8B931", "#E9579F", "#7A5CF0", "#3EC1E6", "#C0504D"];
    const cabelos = ["#1A1A1A", "#4A2E1E", "#7A5232", "#C89B6D", "#8C8C8C", "#2E2E2E"];
    const peles = ["#D9A07A", "#B97A56", "#8C5A3C", "#F0C7A0", "#6B4632", "#E6B48C"];
    let n = 0; for (let i = 0; i < id.length; i++) n = (n * 31 + id.charCodeAt(i)) >>> 0;
    return { shirt: roupas[n % roupas.length], hair: cabelos[(n >>> 3) % cabelos.length], skin: peles[(n >>> 6) % peles.length] };
  }
  function corValida(c, padrao) { return typeof c === "string" && /^#[0-9a-fA-F]{6}$/.test(c) ? c : padrao; }
  function status(txt) { const e = $("rede-est"); if (e) e.textContent = txt; }
  function toast(txt) {
    const t = $("toast"); if (!t) return;
    t.hidden = false; t.textContent = txt;
    clearTimeout(toast._t); toast._t = setTimeout(() => { t.hidden = true; }, 2800);
  }
  function nomeDe(id) {
    const p = presenca.find(x => x.id === id);
    if (p && p.nome) return p.nome;
    const s = naSala.get(id);
    return (s && s.nome) || "amigo";
  }

  /* ---------- identidade e preferências ---------- */
  function carregaIdentidade() {
    let ovId = null, ovNome = null;
    try {
      const q = new URLSearchParams(location.search);
      const qi = q.get("eu");
      if (qi && /^[0-9a-f]{8,64}$/.test(qi)) ovId = qi;
      const qn = q.get("nome");
      if (qn) ovNome = qn.slice(0, 24);
    } catch (e) {}
    let id = null;
    try { id = localStorage.getItem(LS_ID); } catch (e) {}
    if (!id || !/^[0-9a-f]{24}$/.test(id)) { id = idNovo(); try { localStorage.setItem(LS_ID, id); } catch (e) {} }
    meuId = ovId || id;
    try { meuNome = localStorage.getItem(LS_NOME) || ""; } catch (e) {}
    if (!meuNome) {
      meuNome = "Visitante " + meuId.slice(-4).toUpperCase();
      try { localStorage.setItem(LS_NOME, meuNome); } catch (e) {}
    }
    if (ovNome) meuNome = ovNome;
    try { const l = localStorage.getItem(LS_LIVRE); escritorio.livre = l === null ? true : l === "1"; } catch (e) {}
    carregaPerfil();
    carregaDecor();
  }
  function carregaPerfil() {
    const padrao = corDeId(meuId);
    let p = null;
    try { p = JSON.parse(localStorage.getItem(LS_PERFIL) || "null"); } catch (e) {}
    if (!p || typeof p !== "object") p = padrao;
    meuPerfil = { shirt: corValida(p.shirt, padrao.shirt), hair: corValida(p.hair, padrao.hair), skin: corValida(p.skin, padrao.skin) };
  }
  function salvaPerfil() { try { localStorage.setItem(LS_PERFIL, JSON.stringify(meuPerfil)); } catch (e) {} }
  function carregaDecor() {
    try {
      const d = JSON.parse(localStorage.getItem(LS_DECOR) || "[]");
      decor = Array.isArray(d) ? DECOR_IDS.filter(id => d.includes(id)) : [];
    } catch (e) { decor = []; }
    try { const t = localStorage.getItem(LS_TERRENO); terreno = TERRENO_IDS.includes(t) ? t : "jardim"; } catch (e) { terreno = "jardim"; }
  }
  function salvaDecor() { try { localStorage.setItem(LS_DECOR, JSON.stringify(decor)); } catch (e) {} }

  /* ---------- interface ---------- */
  function montaUI() {
    const b = $("b-rede");
    if (b) b.addEventListener("click", () => {
      const p = $("rede"); if (!p) return;
      p.hidden = !p.hidden;
      b.setAttribute("aria-pressed", String(!p.hidden));
      const n = $("rede-nome"); if (!p.hidden && n) n.value = meuNome;
    });
    const x = $("rede-x");
    if (x) x.addEventListener("click", () => { $("rede").hidden = true; if (b) b.setAttribute("aria-pressed", "false"); });

    const nome = $("rede-nome");
    if (nome) {
      nome.value = meuNome;
      nome.addEventListener("change", () => {
        const v = nome.value.trim().slice(0, 24);
        meuNome = v || ("Visitante " + meuId.slice(-4).toUpperCase());
        nome.value = meuNome;
        try { localStorage.setItem(LS_NOME, meuNome); } catch (e) {}
        enviarOla(); renderLista();
      });
    }
    const livre = $("rede-livre");
    if (livre) { livre.checked = escritorio.livre; livre.addEventListener("change", aplicaEscritorio); }
    const senha = $("rede-senha");
    if (senha) senha.addEventListener("change", aplicaEscritorio);

    const bMeu = $("b-meu");
    if (bMeu) bMeu.addEventListener("click", () => {
      const p = $("meu"); if (!p) return;
      p.hidden = !p.hidden;
      bMeu.setAttribute("aria-pressed", String(!p.hidden));
    });
    const xMeu = $("meu-x");
    if (xMeu) xMeu.addEventListener("click", () => { $("meu").hidden = true; if (bMeu) bMeu.setAttribute("aria-pressed", "false"); });

    const cores = { shirt: $("per-shirt"), hair: $("per-hair"), skin: $("per-skin") };
    Object.keys(cores).forEach(k => {
      const el = cores[k]; if (!el) return;
      el.value = meuPerfil[k];
      el.addEventListener("change", () => {
        meuPerfil[k] = corValida(el.value, meuPerfil[k]);
        el.value = meuPerfil[k];
        salvaPerfil();
        enviar({ t: "perfil", perfil: meuPerfil });
      });
    });
    const rand = $("per-rand");
    if (rand) rand.addEventListener("click", () => {
      meuPerfil = corDeId(idNovo());
      Object.keys(cores).forEach(k => { if (cores[k]) cores[k].value = meuPerfil[k]; });
      salvaPerfil();
      enviar({ t: "perfil", perfil: meuPerfil });
    });

    const lista = $("decor-lista");
    if (lista && Esc && Esc.DECOR_CATALOGO) {
      lista.innerHTML = "";
      DECOR_IDS.forEach(id => {
        const item = Esc.DECOR_CATALOGO[id]; if (!item) return;
        const lab = document.createElement("label");
        const cb = document.createElement("input"); cb.type = "checkbox"; cb.checked = decor.includes(id);
        cb.addEventListener("change", () => {
          decor = DECOR_IDS.filter(x => (x === id ? cb.checked : decor.includes(x)));
          salvaDecor();
          enviaDecor();
        });
        lab.append(cb, document.createTextNode(item.nome));
        lista.appendChild(lab);
      });
    }

    const sel = $("ter-sel");
    if (sel) {
      sel.value = terreno;
      sel.addEventListener("change", () => {
        terreno = TERRENO_IDS.includes(sel.value) ? sel.value : "jardim";
        try { localStorage.setItem(LS_TERRENO, terreno); } catch (e) {}
        enviaTerreno();
      });
    }

    const mic = $("rede-mic"); if (mic) mic.addEventListener("click", ligaMic);
    const cam = $("rede-cam"); if (cam) cam.addEventListener("click", ligaCam);
    atualizaMidiaTexto();
  }

  async function aplicaEscritorio() {
    const livre = $("rede-livre") ? $("rede-livre").checked : true;
    const senha = $("rede-senha") ? $("rede-senha").value : "";
    escritorio.livre = livre;
    escritorio.senhaHash = livre ? null : await hashSenha(senha);
    try { localStorage.setItem(LS_LIVRE, livre ? "1" : "0"); } catch (e) {}
    enviar({ t: "escritorio", livre: escritorio.livre, senhaHash: escritorio.senhaHash });
    status(livre ? "seu escritório: livre" : (escritorio.senhaHash ? "seu escritório: com senha" : "seu escritório: fechado"));
  }

  function renderLista() {
    const box = $("rede-lista"); if (!box) return;
    box.innerHTML = "";
    const outros = presenca.filter(p => p.id !== meuId);
    if (!outros.length) { box.textContent = conectado ? "Ninguém online ainda." : "Sem conexão com o servidor de encontro."; return; }
    outros.forEach(p => {
      const linha = document.createElement("div");
      linha.style.cssText = "display:flex;align-items:center;gap:8px;padding:6px 0;border-top:1px solid var(--line)";
      const bolinha = document.createElement("i");
      bolinha.style.cssText = "width:8px;height:8px;border-radius:50%;flex:none;background:" + (p.disponivel ? (p.temSenha ? "#E8B931" : "#3DD68C") : "#7D8FAA");
      const nome = document.createElement("span");
      nome.style.cssText = "flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:12px;color:var(--fg)";
      nome.textContent = p.nome + (p.temSenha ? " (senha)" : "") + (p.disponivel ? "" : " · visitando") + (p.mic ? " · mic" : "") + (p.cam ? " · cam" : "");
      const btn = document.createElement("button");
      btn.className = "btn sm"; btn.type = "button"; btn.textContent = "Entrar";
      btn.disabled = !p.disponivel;
      btn.addEventListener("click", () => entrarEm(p.id));
      linha.append(bolinha, nome, btn);
      box.appendChild(linha);
    });
  }

  /* ---------- avatares remotos ---------- */
  function criaRemoto(p) {
    if (!Esc) return;
    const perfil = p.perfil || corDeId(p.id);
    Esc.criarRemoto(p.id, { nome: p.nome || "Amigo", shirt: perfil.shirt, hair: perfil.hair, skin: perfil.skin });
    if (p.midia && p.midia.cam) Esc.mostrarVideoRemoto(p.id, true);
  }
  function limpaRemotos() {
    if (!Esc) return;
    for (const id of naSala.keys()) if (id !== meuId) Esc.removerRemoto(id);
  }
  function limpaSala() { limpaRemotos(); naSala.clear(); sala = null; fechaTodosPeers(); }

  /* ---------- entrada ---------- */
  async function entrarEm(id) {
    if (!id || id === meuId) return;
    const alvo = presenca.find(p => p.id === id);
    if (!alvo) return;
    if (!alvo.disponivel) { toast("Ele está visitando outra pessoa agora."); return; }
    let senhaHash = null;
    if (alvo.temSenha) {
      const s = prompt("Senha do escritório de " + alvo.nome + ":");
      if (s === null) return;
      senhaHash = await hashSenha(s);
    }
    if (sala && sala !== meuId) enviar({ t: "sair" });
    enviar({ t: "entrar", alvo: id, senhaHash });
    status("pedindo para entrar…");
  }

  /* ---------- conexão ---------- */
  function enviar(o) { if (ws && ws.readyState === 1) { try { ws.send(JSON.stringify(o)); } catch (e) {} } }
  function enviarOla() {
    if (!conectado) return;
    enviar({ t: "ola", id: meuId, nome: meuNome, perfil: meuPerfil, escritorio, decor, terreno });
  }
  function conectar() {
    if (!URL_REDE) { status("rede não configurada"); return; }
    status("conectando…");
    try { ws = new WebSocket(URL_REDE); } catch (e) { status("endereço da rede inválido"); return; }
    ws.onopen = () => { conectado = true; tentativa = 0; enviarOla(); status("online"); renderLista(); };
    ws.onmessage = ev => { let o; try { o = JSON.parse(ev.data); } catch (e) { return; } trata(o); };
    ws.onclose = () => {
      conectado = false; limpaSala(); renderLista();
      if (fechando) return;
      status("offline, tentando de novo…");
      tentativa++;
      setTimeout(conectar, Math.min(15000, 1500 + tentativa * 1500));
    };
    ws.onerror = () => { try { ws.close(); } catch (e) {} };
  }

  function trata(o) {
    switch (o.t) {
      case "ola_ok": if (midiaLocal.mic || midiaLocal.cam) enviaMidia(); break;
      case "presenca": presenca = Array.isArray(o.lista) ? o.lista : []; renderLista(); break;
      case "sala": {
        limpaRemotos();
        fechaTodosPeers();
        sala = o.anfitriao || null;
        naSala.clear();
        (o.participantes || []).forEach(p => { naSala.set(p.id, p); if (p.id !== meuId) criaRemoto(p); });
        if (Esc) {
          if (sala === meuId) { Esc.aplicarDecor(decor); Esc.aplicarTerreno(terreno); }
          else { Esc.aplicarDecor(Array.isArray(o.decor) ? o.decor : []); Esc.aplicarTerreno(o.terreno || "jardim"); }
        }
        sincronizaPeers(o.participantes || []);
        status(sala === meuId ? "no seu escritório" : "visitando " + nomeDe(sala));
        renderLista();
        break;
      }
      case "novo": {
        const p = { id: o.id, nome: o.nome, perfil: o.perfil, midia: o.midia || { mic: false, cam: false }, anfitriao: !!o.anfitriao };
        naSala.set(p.id, p);
        if (p.id !== meuId) { criaRemoto(p); toast(p.nome + " entrou no escritório"); }
        sincronizaPeers([...naSala.values()]);
        status(sala === meuId ? "no seu escritório" : "visitando " + nomeDe(sala));
        renderLista();
        break;
      }
      case "saiu": {
        saiuRecebidos++;
        naSala.delete(o.id);
        fechaPeer(o.id);
        if (Esc) Esc.removerRemoto(o.id);
        renderLista();
        break;
      }
      case "perfil": {
        const p = naSala.get(o.id); if (p) p.perfil = o.perfil;
        if (Esc) Esc.atualizarPerfilRemoto(o.id, o.perfil);
        break;
      }
      case "midia": {
        const p = naSala.get(o.id);
        if (p) p.midia = { mic: !!o.mic, cam: !!o.cam };
        if (Esc) Esc.mostrarVideoRemoto(o.id, !!o.cam);
        break;
      }
      case "decor": {
        if (sala && sala !== meuId && Esc) Esc.aplicarDecor(Array.isArray(o.lista) ? o.lista : []);
        break;
      }
      case "terreno": {
        if (sala && sala !== meuId && Esc) Esc.aplicarTerreno(o.nome || "jardim");
        break;
      }
      case "sinal": { trataSinal(o); break; }
      case "pos": {
        if (o.id !== meuId && Esc) { posRecebidas++; Esc.atualizarRemoto(o.id, +o.x, +o.z, +o.yaw, o.pose); }
        break;
      }
      case "recusado": {
        const m = o.motivo === "senha" ? "Senha errada." :
          o.motivo === "fechado" ? "O escritório dele está fechado." :
          o.motivo === "fora" ? "Ele está visitando outra pessoa." :
          o.motivo === "cheio" ? "O escritório está cheio (4 pessoas)." :
          o.motivo === "offline" ? "Ele saiu." : "Não consegui entrar.";
        toast(m); status(m); break;
      }
      case "expulso": { toast("Você foi convidado a sair."); break; }
      case "aviso": { toast(o.msg || "O escritório fechou."); break; }
      case "erro": { status(o.msg || "erro na rede"); break; }
    }
  }

  /* ---------- voz e câmera (WebRTC) ---------- */
  function criaPeer(id) {
    if (peers.has(id)) return peers.get(id);
    if (!window.RTCPeerConnection) return null;
    const pc = new RTCPeerConnection({ iceServers: STUN });
    const peer = { id, pc, polite: String(meuId) > String(id), makingOffer: false, ignoreOffer: false, senders: {}, pendentes: [], audioEl: null, fechado: false };
    const a = pc.addTransceiver("audio", { direction: "sendrecv" });
    const v = pc.addTransceiver("video", { direction: "sendrecv" });
    peer.senders.audio = a.sender;
    peer.senders.video = v.sender;
    if (streamMic) { const t = streamMic.getAudioTracks()[0]; if (t) peer.senders.audio.replaceTrack(t).catch(() => {}); }
    if (streamCam) { const t = streamCam.getVideoTracks()[0]; if (t) peer.senders.video.replaceTrack(t).catch(() => {}); }

    pc.onnegotiationneeded = async () => {
      try {
        peer.makingOffer = true;
        await pc.setLocalDescription();
        enviar({ t: "sinal", para: id, dados: { description: pc.localDescription } });
      } catch (e) {} finally { peer.makingOffer = false; }
    };
    pc.onicecandidate = ev => { if (ev.candidate) enviar({ t: "sinal", para: id, dados: { candidate: ev.candidate } }); };
    pc.ontrack = ev => {
      if (ev.track.kind === "video") {
        if (Esc) Esc.atualizarVideoRemoto(id, ev.streams[0]);
      } else if (ev.streams[0]) {
        ligaAudioRemoto(peer, ev.streams[0]);
      }
    };
    pc.onconnectionstatechange = () => {
      if (peer.fechado) return;
      if (pc.connectionState === "failed") { try { pc.restartIce(); } catch (e) {} }
    };
    peers.set(id, peer);
    return peer;
  }
  function fechaPeer(id) {
    const peer = peers.get(id); if (!peer) return;
    peers.delete(id);
    peer.fechado = true;
    try { peer.pc.close(); } catch (e) {}
    if (peer.audioEl) { try { peer.audioEl.srcObject = null; peer.audioEl.remove(); } catch (e) {} peer.audioEl = null; }
    if (Esc) { Esc.atualizarVideoRemoto(id, null); Esc.mostrarVideoRemoto(id, false); }
  }
  function fechaTodosPeers() { for (const id of [...peers.keys()]) fechaPeer(id); }
  function sincronizaPeers(partes) {
    const ids = new Set((partes || []).filter(p => p && p.id && p.id !== meuId).map(p => p.id));
    for (const id of [...peers.keys()]) if (!ids.has(id)) fechaPeer(id);
    for (const id of ids) if (!peers.has(id)) criaPeer(id);
  }
  function ligaAudioRemoto(peer, stream) {
    if (!peer.audioEl) {
      const el = document.createElement("audio");
      el.autoplay = true; el.playsInline = true; el.style.display = "none";
      document.body.appendChild(el);
      peer.audioEl = el;
    }
    if (peer.audioEl.srcObject !== stream) peer.audioEl.srcObject = stream;
    peer.audioEl.play().catch(() => {
      const tenta = () => { try { peer.audioEl && peer.audioEl.play().catch(() => {}); } catch (e) {} };
      document.addEventListener("click", tenta, { once: true });
      document.addEventListener("keydown", tenta, { once: true });
    });
  }
  async function trataSinal(o) {
    const id = o.de;
    if (!id || id === meuId) return;
    const peer = peers.get(id) || criaPeer(id);
    if (!peer) return;
    const s = o.dados || {};
    try {
      if (s.description) {
        const desc = s.description;
        const oferta = desc.type === "offer";
        const colisao = oferta && (peer.makingOffer || peer.pc.signalingState !== "stable");
        peer.ignoreOffer = !peer.polite && colisao;
        if (peer.ignoreOffer) return;
        if (colisao && peer.polite) await peer.pc.setLocalDescription({ type: "rollback" });
        await peer.pc.setRemoteDescription(desc);
        for (const c of peer.pendentes.splice(0)) { try { await peer.pc.addIceCandidate(c); } catch (e) {} }
        if (oferta) {
          await peer.pc.setLocalDescription();
          enviar({ t: "sinal", para: id, dados: { description: peer.pc.localDescription } });
        }
      } else if (s.candidate) {
        if (peer.pc.remoteDescription) { try { await peer.pc.addIceCandidate(s.candidate); } catch (e) {} }
        else peer.pendentes.push(s.candidate);
      }
    } catch (e) {}
  }
  function enviaMidia() { enviar({ t: "midia", mic: midiaLocal.mic, cam: midiaLocal.cam }); atualizaMidiaTexto(); }
  function atualizaMidiaTexto() {
    const e = $("rede-midia"); if (!e) return;
    const partes = [];
    if (midiaLocal.mic) partes.push("microfone ligado");
    if (midiaLocal.cam) partes.push("câmera ligada");
    e.textContent = partes.length ? partes.join(" · ") : "voz e câmera desligadas";
  }
  async function ligaMic() {
    if (midiaLocal.mic) { desligaMic(); return; }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) { toast("Este navegador não deixa usar o microfone aqui."); return; }
    try {
      streamMic = await navigator.mediaDevices.getUserMedia({ audio: true });
      midiaLocal.mic = true;
      for (const p of peers.values()) { const t = streamMic.getAudioTracks()[0]; if (t && p.senders.audio) p.senders.audio.replaceTrack(t).catch(() => {}); }
      const btn = $("rede-mic"); if (btn) { btn.setAttribute("aria-pressed", "true"); btn.textContent = "Microfone ligado"; }
      enviaMidia();
    } catch (e) { toast("Não consegui ligar o microfone."); }
  }
  function desligaMic() {
    if (streamMic) { streamMic.getTracks().forEach(t => t.stop()); streamMic = null; }
    midiaLocal.mic = false;
    for (const p of peers.values()) { if (p.senders.audio) p.senders.audio.replaceTrack(null).catch(() => {}); }
    const btn = $("rede-mic"); if (btn) { btn.setAttribute("aria-pressed", "false"); btn.textContent = "Microfone"; }
    enviaMidia();
  }
  async function ligaCam() {
    if (midiaLocal.cam) { desligaCam(); return; }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) { toast("Este navegador não deixa usar a câmera aqui."); return; }
    try {
      streamCam = await navigator.mediaDevices.getUserMedia({ video: { width: 320, height: 240 } });
      midiaLocal.cam = true;
      const v = $("rede-eu-cam");
      if (v) { v.srcObject = streamCam; v.hidden = false; v.play().catch(() => {}); }
      for (const p of peers.values()) { const t = streamCam.getVideoTracks()[0]; if (t && p.senders.video) p.senders.video.replaceTrack(t).catch(() => {}); }
      const btn = $("rede-cam"); if (btn) { btn.setAttribute("aria-pressed", "true"); btn.textContent = "Câmera ligada"; }
      enviaMidia();
    } catch (e) { toast("Não consegui ligar a câmera."); }
  }
  function desligaCam() {
    if (streamCam) { streamCam.getTracks().forEach(t => t.stop()); streamCam = null; }
    midiaLocal.cam = false;
    const v = $("rede-eu-cam"); if (v) { v.srcObject = null; v.hidden = true; }
    for (const p of peers.values()) { if (p.senders.video) p.senders.video.replaceTrack(null).catch(() => {}); }
    const btn = $("rede-cam"); if (btn) { btn.setAttribute("aria-pressed", "false"); btn.textContent = "Câmera"; }
    enviaMidia();
  }
  function atualizaVolumes(e) {
    if (!Esc) return;
    for (const [id, peer] of peers) {
      if (!peer.audioEl) continue;
      const o = Esc.avatars[id];
      let v = 1;
      if (o) {
        const d = Math.hypot(o.g.position.x - e.x, o.g.position.z - e.z);
        v = d < 2 ? 1 : Math.max(0, 1 - (d - 2) / 16);
        v = v * v;
      }
      peer.audioEl.volume = v;
    }
  }

  /* ---------- enfeites e terreno ---------- */
  function enviaDecor() {
    enviar({ t: "decor", lista: decor });
    if ((!sala || sala === meuId) && Esc) Esc.aplicarDecor(decor);
  }
  function enviaTerreno() {
    enviar({ t: "terreno", nome: terreno });
    if ((!sala || sala === meuId) && Esc) Esc.aplicarTerreno(terreno);
  }

  /* ---------- envio de posição ---------- */
  function ligaPos() {
    if (timerPos) return;
    timerPos = setInterval(() => {
      if (!conectado || !sala || !Esc) return;
      const e = Esc.estado();
      enviar({ t: "pos", x: +e.x.toFixed(3), z: +e.z.toFixed(3), yaw: +e.yaw.toFixed(3), pose: e.pose });
      atualizaVolumes(e);
    }, 100);
  }

  /* ---------- início ---------- */
  function inicia() {
    Esc = window.Esc3D;
    carregaIdentidade();
    montaUI();
    if (Esc) { Esc.aplicarDecor(decor); Esc.aplicarTerreno(terreno); }
    renderLista();
    ligaPos();
    conectar();
    window.__escRede = {
      estado: () => ({
        conectado, sala, eu: meuId, nome: meuNome, perfil: meuPerfil, naSala: [...naSala.keys()],
        presenca, posRecebidas, saiuRecebidos, decor, terreno, midia: { ...midiaLocal },
        peers: [...peers.keys()]
      }),
      entrarEm, sair: () => enviar({ t: "sair" })
    };
    window.addEventListener("beforeunload", () => {
      fechando = true;
      try { ws && ws.close(); } catch (e) {}
      try { streamMic && streamMic.getTracks().forEach(t => t.stop()); } catch (e) {}
      try { streamCam && streamCam.getTracks().forEach(t => t.stop()); } catch (e) {}
    });
  }
  function espera() {
    if (window.Esc3D) { inicia(); return; }
    let n = 0;
    const t = setInterval(() => {
      if (window.Esc3D) { clearInterval(t); inicia(); }
      else if (++n > 300) clearInterval(t);
    }, 100);
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", espera);
  else espera();
})();
