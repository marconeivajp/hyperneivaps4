// Escritório 3D - servidor de encontro LOCAL (para testar no mesmo PC)
// Sem dependências. Rode com:  node servidor-rede.js
// Fala WebSocket com o rede.js do escritório. Não fala com a internet.
//
// Para dois computadores de verdade, publique o Worker (pasta worker/) e
// troque a url em config-rede.js.

const http = require("http");
const crypto = require("crypto");

const PORTA = 4748;
const GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

const clientes = new Map(); // id -> conexão

function str(v, n) { return typeof v === "string" ? v.slice(0, n) : ""; }
function num(v) { const n = +v; return Number.isFinite(n) ? n : 0; }
function clamp(v, a, b) { return Math.max(a, Math.min(b, v)); }
function sanPerfil(p) {
  if (!p || typeof p !== "object") return null;
  return { shirt: str(p.shirt, 20), hair: str(p.hair, 20), skin: str(p.skin, 20) };
}
function sanLista(v) {
  return Array.isArray(v) ? v.slice(0, 24).map(x => str(x, 24)).filter(Boolean) : [];
}
function sanTerreno(v) {
  const s = str(v, 20);
  return ["jardim", "praia", "neve", "campo"].includes(s) ? s : "jardim";
}
function sanMidia(m) { return { mic: !!(m && m.mic), cam: !!(m && m.cam) }; }

/* ---------- envio de quadros WebSocket ---------- */
function enviaFrame(cli, op, payload) {
  if (!cli || cli.fechado || !cli.socket.writable) return;
  const len = payload.length;
  let cab;
  if (len < 126) { cab = Buffer.from([0x80 | op, len]); }
  else if (len < 65536) { cab = Buffer.alloc(4); cab[0] = 0x80 | op; cab[1] = 126; cab.writeUInt16BE(len, 2); }
  else { cab = Buffer.alloc(10); cab[0] = 0x80 | op; cab[1] = 127; cab.writeBigUInt64BE(BigInt(len), 2); }
  try { cli.socket.write(Buffer.concat([cab, payload])); } catch (e) { fecha(cli); }
}
function envia(cli, obj) {
  enviaFrame(cli, 0x1, Buffer.from(JSON.stringify(obj), "utf8"));
}

/* ---------- estado ---------- */
function listaPresenca() {
  const out = [];
  for (const c of clientes.values()) {
    out.push({
      id: c.id, nome: c.nome, perfil: c.perfil,
      livre: !!c.escritorio.livre, temSenha: !!c.escritorio.senhaHash,
      mic: !!(c.midia && c.midia.mic), cam: !!(c.midia && c.midia.cam),
      disponivel: c.sala === c.id, sala: c.sala
    });
  }
  return out;
}
function broadcast(obj, exceto) {
  for (const c of clientes.values()) if (c !== exceto) envia(c, obj);
}
function broadcastPresenca() { broadcast({ t: "presenca", lista: listaPresenca() }); }

function salaDe(salaId) {
  const out = [];
  for (const c of clientes.values()) if (c.sala === salaId) out.push(c);
  return out;
}
function enviaSala(cli, salaId) {
  const partes = salaDe(salaId).map(c => ({ id: c.id, nome: c.nome, perfil: c.perfil, midia: c.midia || { mic: false, cam: false }, anfitriao: c.id === salaId }));
  const host = clientes.get(salaId);
  envia(cli, {
    t: "sala", anfitriao: salaId, participantes: partes,
    decor: host ? host.decor : [], terreno: host ? host.terreno : "jardim"
  });
}
function avisaSaida(cli, salaAntiga) {
  for (const c of clientes.values()) if (c !== cli && c.sala === salaAntiga) envia(c, { t: "saiu", id: cli.id });
}

/* ---------- mensagens ---------- */
function trata(cli, texto) {
  let o;
  try { o = JSON.parse(texto); } catch (e) { return; }
  if (!o || typeof o !== "object") return;

  switch (o.t) {
    case "ola": {
      const id = str(o.id, 64);
      if (!/^[0-9a-f]{8,64}$/.test(id)) return;
      const velho = clientes.get(id);
      if (velho && velho !== cli) { clientes.delete(id); fecha(velho); }
      const perfil = sanPerfil(o.perfil);
      const esc = o.escritorio && typeof o.escritorio === "object" ? o.escritorio : {};
      cli.id = id;
      cli.nome = str(o.nome, 24) || "Visitante";
      cli.perfil = perfil;
      cli.escritorio = { livre: !!esc.livre, senhaHash: esc.senhaHash ? str(esc.senhaHash, 100) : null };
      cli.decor = sanLista(o.decor);
      cli.terreno = sanTerreno(o.terreno);
      cli.midia = { mic: false, cam: false };
      cli.sala = id;
      clientes.set(id, cli);
      envia(cli, { t: "ola_ok", id });
      enviaSala(cli, id);
      broadcastPresenca();
      break;
    }

    case "perfil": {
      if (!cli.id) return;
      cli.perfil = sanPerfil(o.perfil);
      broadcastPresenca();
      for (const c of clientes.values()) {
        if (c !== cli && c.sala === cli.sala) envia(c, { t: "perfil", id: cli.id, perfil: cli.perfil });
      }
      break;
    }

    case "midia": {
      if (!cli.id) return;
      cli.midia = sanMidia(o);
      broadcastPresenca();
      for (const c of clientes.values()) {
        if (c !== cli && c.sala === cli.sala) envia(c, { t: "midia", id: cli.id, mic: cli.midia.mic, cam: cli.midia.cam });
      }
      break;
    }

    case "decor": {
      if (!cli.id || cli.sala !== cli.id) return;
      cli.decor = sanLista(o.lista);
      for (const c of clientes.values()) {
        if (c !== cli && c.sala === cli.id) envia(c, { t: "decor", lista: cli.decor });
      }
      break;
    }

    case "terreno": {
      if (!cli.id || cli.sala !== cli.id) return;
      cli.terreno = sanTerreno(o.nome);
      for (const c of clientes.values()) {
        if (c !== cli && c.sala === cli.id) envia(c, { t: "terreno", nome: cli.terreno });
      }
      break;
    }

    case "sinal": {
      if (!cli.id) return;
      const para = str(o.para, 64);
      const alvo = clientes.get(para);
      if (!alvo || alvo.sala !== cli.sala) return;
      let dados;
      try { dados = JSON.stringify(o.dados); } catch (e) { return; }
      if (!dados || dados.length > 40000) return;
      envia(alvo, { t: "sinal", de: cli.id, dados: o.dados });
      break;
    }

    case "escritorio": {
      if (!cli.id) return;
      cli.escritorio = {
        livre: !!o.livre,
        senhaHash: o.senhaHash ? str(o.senhaHash, 100) : null
      };
      broadcastPresenca();
      break;
    }

    case "entrar": {
      if (!cli.id) return;
      const alvo = str(o.alvo, 64);
      const host = clientes.get(alvo);
      if (!host) { envia(cli, { t: "recusado", motivo: "offline" }); return; }
      if (host === cli) return;
      if (host.sala !== host.id) { envia(cli, { t: "recusado", motivo: "fora" }); return; }
      const senhaOk = !host.escritorio.livre && host.escritorio.senhaHash && str(o.senhaHash, 100) === host.escritorio.senhaHash;
      if (!host.escritorio.livre && !senhaOk) {
        envia(cli, { t: "recusado", motivo: host.escritorio.senhaHash ? "senha" : "fechado" });
        return;
      }
      if (cli.sala !== alvo && salaDe(alvo).length >= 4) { envia(cli, { t: "recusado", motivo: "cheio" }); return; }
      const antiga = cli.sala;
      if (antiga && antiga !== cli.id) avisaSaida(cli, antiga);
      // se ele era o dono e tinha visitantes, todos voltam para casa
      if (antiga === cli.id) {
        for (const c of salaDe(antiga)) {
          if (c === cli) continue;
          c.sala = c.id;
          envia(c, { t: "aviso", msg: "O dono saiu do escritório." });
          enviaSala(c, c.id);
        }
      }
      cli.sala = alvo;
      enviaSala(cli, alvo);
      for (const c of salaDe(alvo)) if (c !== cli) envia(c, { t: "novo", id: cli.id, nome: cli.nome, perfil: cli.perfil, midia: cli.midia || { mic: false, cam: false }, anfitriao: false });
      broadcastPresenca();
      break;
    }

    case "sair": {
      if (!cli.id) return;
      const antiga = cli.sala;
      cli.sala = cli.id;
      if (antiga && antiga !== cli.id) avisaSaida(cli, antiga);
      enviaSala(cli, cli.id);
      broadcastPresenca();
      break;
    }

    case "expulsar": {
      if (!cli.id || cli.sala !== cli.id) return;
      const alvo = str(o.alvo, 64);
      const t = clientes.get(alvo);
      if (!t || t.sala !== cli.id) return;
      const antiga = cli.id;
      t.sala = t.id;
      envia(t, { t: "expulso" });
      enviaSala(t, t.id);
      avisaSaida(t, antiga);
      broadcastPresenca();
      break;
    }

    case "pos": {
      if (!cli.id || !cli.sala) return;
      const msg = {
        t: "pos", id: cli.id,
        x: clamp(num(o.x), -200, 200), z: clamp(num(o.z), -200, 200),
        yaw: num(o.yaw), pose: o.pose === "sit" ? "sit" : "stand"
      };
      for (const c of clientes.values()) if (c !== cli && c.sala === cli.sala) envia(c, msg);
      break;
    }

    case "ping": envia(cli, { t: "pong" }); break;
  }
}

/* ---------- ciclo de vida ---------- */
function fecha(cli) {
  if (!cli || cli.fechado) return;
  cli.fechado = true;
  try { cli.socket.destroy(); } catch (e) {}
  if (cli.id && clientes.get(cli.id) === cli) {
    clientes.delete(cli.id);
    avisaSaida(cli, cli.sala);
    // quem estava visitando ele volta para o próprio escritório
    for (const c of clientes.values()) {
      if (c.sala === cli.id) { c.sala = c.id; enviaSala(c, c.id); }
    }
    broadcastPresenca();
  }
}

/* ---------- leitura de quadros ---------- */
function processaBuffer(cli) {
  for (;;) {
    const b = cli.buffer;
    if (b.length < 2) return;
    const fin = (b[0] & 0x80) !== 0;
    const op = b[0] & 0x0f;
    const mascarado = (b[1] & 0x80) !== 0;
    let len = b[1] & 0x7f;
    let off = 2;
    if (len === 126) { if (b.length < 4) return; len = b.readUInt16BE(2); off = 4; }
    else if (len === 127) { if (b.length < 10) return; len = Number(b.readBigUInt64BE(2)); off = 10; }
    if (len > 8 * 1024 * 1024) { fecha(cli); return; }
    const mascaraLen = mascarado ? 4 : 0;
    if (b.length < off + mascaraLen + len) return;
    let payload = Buffer.from(b.subarray(off + mascaraLen, off + mascaraLen + len));
    if (mascarado) {
      const mask = b.subarray(off, off + 4);
      for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
    }
    cli.buffer = b.subarray(off + mascaraLen + len);

    if (op === 0x8) { fecha(cli); return; }
    if (op === 0x9) { enviaFrame(cli, 0xA, payload); continue; }
    if (op === 0xA) { continue; }
    if (op === 0x1 || op === 0x0) {
      if (op === 0x1) cli.frag = payload;
      else if (cli.frag) cli.frag = Buffer.concat([cli.frag, payload]);
      else cli.frag = payload;
      if (fin) {
        const texto = cli.frag.toString("utf8");
        cli.frag = null;
        trata(cli, texto);
      }
    }
  }
}

/* ---------- servidor ---------- */
const srv = http.createServer((req, res) => {
  res.writeHead(200, { "Content-Type": "text/plain; charset=utf-8" });
  res.end("Escritorio 3D - servidor de encontro local (porta " + PORTA + ")\n");
});

srv.on("upgrade", (req, socket) => {
  const key = req.headers["sec-websocket-key"];
  if (!key) { socket.destroy(); return; }
  const accept = crypto.createHash("sha1").update(key + GUID).digest("base64");
  socket.write(
    "HTTP/1.1 101 Switching Protocols\r\n" +
    "Upgrade: websocket\r\n" +
    "Connection: Upgrade\r\n" +
    "Sec-WebSocket-Accept: " + accept + "\r\n\r\n"
  );
  socket.setNoDelay(true);
  const cli = {
    socket, buffer: Buffer.alloc(0), frag: null, fechado: false,
    id: null, nome: "", perfil: null,
    escritorio: { livre: true, senhaHash: null }, sala: null,
    midia: { mic: false, cam: false }, decor: [], terreno: "jardim"
  };
  socket.on("data", d => {
    if (cli.fechado) return;
    cli.buffer = Buffer.concat([cli.buffer, d]);
    try { processaBuffer(cli); } catch (e) { fecha(cli); }
  });
  socket.on("close", () => fecha(cli));
  socket.on("error", () => fecha(cli));
});

srv.listen(PORTA, "127.0.0.1", () => {
  console.log("Servidor de encontro do Escritorio 3D em ws://127.0.0.1:" + PORTA);
});

// Encerra limpo com Ctrl+C
process.on("SIGINT", () => { try { srv.close(); } catch (e) {} process.exit(0); });
