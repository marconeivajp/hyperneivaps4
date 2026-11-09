// Escritório 3D - servidor de encontro na internet (Cloudflare Worker)
// Um único Durable Object "global" guarda quem está online, o escritório de
// cada um e repassa as posições. É o mesmo protocolo do servidor-rede.js.

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === "/ws") {
      const id = env.SALA.idFromName("global");
      return env.SALA.get(id).fetch(request);
    }
    return new Response("Escritorio 3D - servidor de encontro\n", {
      status: 200,
      headers: { "content-type": "text/plain; charset=utf-8" }
    });
  }
};

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
function vazio(id) {
  return {
    id: id || null, nome: "", perfil: null,
    escritorio: { livre: true, senhaHash: null }, sala: id || null,
    midia: { mic: false, cam: false }, decor: [], terreno: "jardim"
  };
}

export class Sala {
  constructor(state) { this.state = state; }

  async fetch(request) {
    if (request.headers.get("Upgrade") !== "websocket") {
      return new Response("esperando websocket", { status: 426 });
    }
    const par = new WebSocketPair();
    const [cliente, servidor] = Object.values(par);
    this.state.acceptWebSocket(servidor);
    servidor.serializeAttachment(vazio(null));
    return new Response(null, { status: 101, webSocket: cliente });
  }

  /* ---------- utilidades ---------- */
  todos() {
    return this.state.getWebSockets().map(ws => ({ ws, d: ws.deserializeAttachment() || vazio(null) }));
  }
  acha(id) { return this.todos().find(x => x.d.id === id) || null; }
  envia(ws, o) { try { ws.send(JSON.stringify(o)); } catch (e) {} }
  salva(ws, d) { try { ws.serializeAttachment(d); } catch (e) {} }

  presenca() {
    return this.todos().filter(x => x.d.id).map(x => ({
      id: x.d.id, nome: x.d.nome, perfil: x.d.perfil,
      livre: !!(x.d.escritorio && x.d.escritorio.livre),
      temSenha: !!(x.d.escritorio && x.d.escritorio.senhaHash),
      mic: !!(x.d.midia && x.d.midia.mic), cam: !!(x.d.midia && x.d.midia.cam),
      disponivel: x.d.sala === x.d.id, sala: x.d.sala
    }));
  }
  broadcast(o, exceto) { for (const x of this.todos()) if (x.ws !== exceto) this.envia(x.ws, o); }
  broadcastPresenca() { this.broadcast({ t: "presenca", lista: this.presenca() }); }
  salaDe(salaId) { return this.todos().filter(x => x.d.id && x.d.sala === salaId); }

  enviaSala(ws, salaId) {
    const partes = this.salaDe(salaId).map(x => ({ id: x.d.id, nome: x.d.nome, perfil: x.d.perfil, midia: x.d.midia || { mic: false, cam: false }, anfitriao: x.d.id === salaId }));
    const host = this.acha(salaId);
    this.envia(ws, {
      t: "sala", anfitriao: salaId, participantes: partes,
      decor: host ? host.d.decor : [], terreno: host ? host.d.terreno : "jardim"
    });
  }
  avisaSaida(id, salaAntiga) {
    for (const x of this.todos()) if (x.d.id && x.d.id !== id && x.d.sala === salaAntiga) this.envia(x.ws, { t: "saiu", id });
  }

  /* ---------- mensagens ---------- */
  async webSocketMessage(ws, mensagem) {
    let o;
    try { o = JSON.parse(mensagem); } catch (e) { return; }
    if (!o || typeof o !== "object") return;
    let d = ws.deserializeAttachment() || vazio(null);

    switch (o.t) {
      case "ola": {
        const id = str(o.id, 64);
        if (!/^[0-9a-f]{8,64}$/.test(id)) return;
        for (const x of this.todos()) {
          if (x.ws !== ws && x.d.id === id) { try { x.ws.close(4001, "outro igual"); } catch (e) {} }
        }
        const esc = o.escritorio && typeof o.escritorio === "object" ? o.escritorio : {};
        d = {
          id, nome: str(o.nome, 24) || "Visitante", perfil: sanPerfil(o.perfil),
          escritorio: { livre: !!esc.livre, senhaHash: esc.senhaHash ? str(esc.senhaHash, 100) : null },
          sala: id,
          midia: { mic: false, cam: false }, decor: sanLista(o.decor), terreno: sanTerreno(o.terreno)
        };
        this.salva(ws, d);
        this.envia(ws, { t: "ola_ok", id });
        this.enviaSala(ws, id);
        this.broadcastPresenca();
        break;
      }

      case "perfil": {
        if (!d.id) return;
        d.perfil = sanPerfil(o.perfil);
        this.salva(ws, d);
        this.broadcastPresenca();
        for (const x of this.todos()) {
          if (x.ws !== ws && x.d.sala === d.sala) this.envia(x.ws, { t: "perfil", id: d.id, perfil: d.perfil });
        }
        break;
      }

      case "midia": {
        if (!d.id) return;
        d.midia = sanMidia(o);
        this.salva(ws, d);
        this.broadcastPresenca();
        for (const x of this.todos()) {
          if (x.ws !== ws && x.d.sala === d.sala) this.envia(x.ws, { t: "midia", id: d.id, mic: d.midia.mic, cam: d.midia.cam });
        }
        break;
      }

      case "decor": {
        if (!d.id || d.sala !== d.id) return;
        d.decor = sanLista(o.lista);
        this.salva(ws, d);
        for (const x of this.todos()) {
          if (x.ws !== ws && x.d.sala === d.id) this.envia(x.ws, { t: "decor", lista: d.decor });
        }
        break;
      }

      case "terreno": {
        if (!d.id || d.sala !== d.id) return;
        d.terreno = sanTerreno(o.nome);
        this.salva(ws, d);
        for (const x of this.todos()) {
          if (x.ws !== ws && x.d.sala === d.id) this.envia(x.ws, { t: "terreno", nome: d.terreno });
        }
        break;
      }

      case "sinal": {
        if (!d.id) return;
        const alvo = this.acha(str(o.para, 64));
        if (!alvo || alvo.d.sala !== d.sala) return;
        let dados;
        try { dados = JSON.stringify(o.dados); } catch (e) { return; }
        if (!dados || dados.length > 40000) return;
        this.envia(alvo.ws, { t: "sinal", de: d.id, dados: o.dados });
        break;
      }

      case "escritorio": {
        if (!d.id) return;
        d.escritorio = { livre: !!o.livre, senhaHash: o.senhaHash ? str(o.senhaHash, 100) : null };
        this.salva(ws, d);
        this.broadcastPresenca();
        break;
      }

      case "entrar": {
        if (!d.id) return;
        const alvo = str(o.alvo, 64);
        const host = this.acha(alvo);
        if (!host) { this.envia(ws, { t: "recusado", motivo: "offline" }); return; }
        if (host.ws === ws) return;
        if (host.d.sala !== host.d.id) { this.envia(ws, { t: "recusado", motivo: "fora" }); return; }
        const senhaOk = !host.d.escritorio.livre && host.d.escritorio.senhaHash && str(o.senhaHash, 100) === host.d.escritorio.senhaHash;
        if (!host.d.escritorio.livre && !senhaOk) {
          this.envia(ws, { t: "recusado", motivo: host.d.escritorio.senhaHash ? "senha" : "fechado" });
          return;
        }
        if (d.sala !== alvo && this.salaDe(alvo).length >= 4) { this.envia(ws, { t: "recusado", motivo: "cheio" }); return; }
        const antiga = d.sala;
        if (antiga && antiga !== d.id) this.avisaSaida(d.id, antiga);
        // se ele era o dono e tinha visitantes, todos voltam para casa
        if (antiga === d.id) {
          for (const x of this.salaDe(antiga)) {
            if (x.ws === ws) continue;
            x.d.sala = x.d.id;
            this.salva(x.ws, x.d);
            this.envia(x.ws, { t: "aviso", msg: "O dono saiu do escritório." });
            this.enviaSala(x.ws, x.d.id);
          }
        }
        d.sala = alvo;
        this.salva(ws, d);
        this.enviaSala(ws, alvo);
        for (const x of this.salaDe(alvo)) if (x.ws !== ws) this.envia(x.ws, { t: "novo", id: d.id, nome: d.nome, perfil: d.perfil, midia: d.midia || { mic: false, cam: false }, anfitriao: false });
        this.broadcastPresenca();
        break;
      }

      case "sair": {
        if (!d.id) return;
        const antiga = d.sala;
        d.sala = d.id;
        this.salva(ws, d);
        if (antiga && antiga !== d.id) this.avisaSaida(d.id, antiga);
        this.enviaSala(ws, d.id);
        this.broadcastPresenca();
        break;
      }

      case "expulsar": {
        if (!d.id || d.sala !== d.id) return;
        const t = this.acha(str(o.alvo, 64));
        if (!t || t.d.sala !== d.id) return;
        const antiga = d.id;
        t.d.sala = t.d.id;
        this.salva(t.ws, t.d);
        this.envia(t.ws, { t: "expulso" });
        this.enviaSala(t.ws, t.d.id);
        this.avisaSaida(t.d.id, antiga);
        this.broadcastPresenca();
        break;
      }

      case "pos": {
        if (!d.id || !d.sala) return;
        const msg = {
          t: "pos", id: d.id,
          x: clamp(num(o.x), -200, 200), z: clamp(num(o.z), -200, 200),
          yaw: num(o.yaw), pose: o.pose === "sit" ? "sit" : "stand"
        };
        for (const x of this.todos()) if (x.ws !== ws && x.d.sala === d.sala) this.envia(x.ws, msg);
        break;
      }

      case "ping": this.envia(ws, { t: "pong" }); break;
    }
  }

  async webSocketClose(ws) {
    const d = ws.deserializeAttachment() || vazio(null);
    if (d.id) {
      const ainda = this.todos().some(x => x.ws !== ws && x.d.id === d.id);
      if (!ainda) {
        this.avisaSaida(d.id, d.sala);
        for (const x of this.todos()) {
          if (x.ws !== ws && x.d.id && x.d.sala === d.id) {
            x.d.sala = x.d.id;
            this.salva(x.ws, x.d);
            this.enviaSala(x.ws, x.d.id);
          }
        }
        this.broadcastPresenca();
      }
    }
    try { ws.close(); } catch (e) {}
  }

  async webSocketError(ws) { return this.webSocketClose(ws); }
}
