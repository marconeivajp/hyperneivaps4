// =========================================================================
// HYPER NEIVA MEET no PS4: tela da chamada, PS4 como participante (microfone e som),
// QR do link e abertura da porta no roteador (UPnP). Incluído pelo network.cpp depois de
// meet_ps4.h; usa as funções do network.cpp (desenho, teclado, rede, som).
// =========================================================================
#pragma once

static void meetLog(const char* formato, ...) {
    char b[400]; va_list a; va_start(a, formato); vsnprintf(b, sizeof(b), formato, a); va_end(a);
    remotoLog("%s", b);
}

// ---------------------------------------------------------------- estado
struct MtPessoa { bool usada; int id; uint8_t* rgba; int w, h; int16_t* anel; int ini, fim; uint64_t falouMs; };
#define MT_ANEL 16000          // 1 s de áudio por pessoa
static MtPessoa mtPessoas[MEET_MAX_PESSOAS];
static pthread_mutex_t mtTrava = PTHREAD_MUTEX_INITIALIZER;
static char mtChat[30][220]; static int mtChatN = 0;
static int mtSala = -1, mtSelMenu = 0, mtSelPessoa = 0, mtImeModo = 0;   // ime: 1 nome da sala, 2 chat, 3 meu nome
static bool mtCriarPrivada = false, mtInternet = false, mtMudo = false, mtConfirmarSair = false, mtAprovacao = false;
static int mtConfirmarApagar = -1;   // sala salva marcada para apagar (Triangulo duas vezes)
static char mtMeuNome[24] = "PS4";
static volatile bool mtSomRodando = false, mtSomParar = false, mtMicRodando = false, mtMicParar = false;
static char mtLinkLocal[200] = "", mtLinkInternet[200] = "", mtAvisoInternet[200] = "", mtAviso[200] = "";
static volatile bool mtUpnpRodando = false;
static char mtUpnpControle[400] = "", mtUpnpServico[120] = "";
static bool mtUpnpAberto = false, mtVerInternet = false;
#define MT_CONFIG REMOTO_RAIZ "/configuracao/meet.txt"
#define MT_ITENS 5   // itens fixos do menu antes das salas salvas

static MtPessoa* mtPessoaDe(int id, bool criar) {
    for (int i = 0; i < MEET_MAX_PESSOAS; i++) if (mtPessoas[i].usada && mtPessoas[i].id == id) return &mtPessoas[i];
    if (!criar) return NULL;
    for (int i = 0; i < MEET_MAX_PESSOAS; i++) if (!mtPessoas[i].usada) {
        memset(&mtPessoas[i], 0, sizeof(MtPessoa));
        mtPessoas[i].usada = true; mtPessoas[i].id = id;
        mtPessoas[i].anel = (int16_t*)calloc(MT_ANEL, 2);
        return &mtPessoas[i];
    }
    return NULL;
}
static void mtLimparPessoas() {
    pthread_mutex_lock(&mtTrava);
    for (int i = 0; i < MEET_MAX_PESSOAS; i++) { if (mtPessoas[i].rgba) stbi_image_free(mtPessoas[i].rgba); free(mtPessoas[i].anel); memset(&mtPessoas[i], 0, sizeof(MtPessoa)); }
    pthread_mutex_unlock(&mtTrava);
}

// ---------------------------------------------------------------- ganchos do servidor
static void meetPs4Midia(int sala, int de, int tipo, const uint8_t* dados, int n) {
    if (sala != mtSala) return;
    if (tipo == 1) {
        int w = 0, h = 0, c = 0;
        uint8_t* img = stbi_load_from_memory(dados, n, &w, &h, &c, 4);
        if (!img) return;
        pthread_mutex_lock(&mtTrava);
        MtPessoa* p = mtPessoaDe(de, true);
        if (p) { if (p->rgba) stbi_image_free(p->rgba); p->rgba = img; p->w = w; p->h = h; img = NULL; }
        pthread_mutex_unlock(&mtTrava);
        if (img) stbi_image_free(img);
    }
    else if (tipo == 2) {
        const int16_t* pcm = (const int16_t*)dados; int amostras = n / 2;
        pthread_mutex_lock(&mtTrava);
        MtPessoa* p = mtPessoaDe(de, true);
        if (p && p->anel) {
            int mx = 0;
            for (int i = 0; i < amostras; i++) {
                int prox = (p->fim + 1) % MT_ANEL;
                if (prox == p->ini) p->ini = (p->ini + 1) % MT_ANEL;   // cheio: descarta o mais velho (latência curta)
                p->anel[p->fim] = pcm[i]; p->fim = prox;
                int v = pcm[i] < 0 ? -pcm[i] : pcm[i]; if (v > mx) mx = v;
            }
            if (mx > 1500) p->falouMs = remotoAgoraMs();
        }
        pthread_mutex_unlock(&mtTrava);
    }
}
static void meetPs4Texto(int sala, const char* quem, const char* texto, bool sistema) {
    if (sala != mtSala) return;
    pthread_mutex_lock(&mtTrava);
    if (mtChatN == 30) { memmove(mtChat[0], mtChat[1], sizeof(mtChat[0]) * 29); mtChatN = 29; }
    snprintf(mtChat[mtChatN++], sizeof(mtChat[0]), sistema ? "* %s %s" : "%s: %s", quem, texto);
    pthread_mutex_unlock(&mtTrava);
}

// ---------------------------------------------------------------- som: mistura as vozes no som do app
static void* mtThreadSom(void*) {
    static int16_t mono[320], estereo[960 * 2];
    while (!mtSomParar) {
        // mantém só ~80 ms no anel do app (latência baixa)
        if (appSomEscrito - appSomLido > 3840) { remotoDormir(5); continue; }
        memset(mono, 0, sizeof(mono));
        int32_t soma[320]; memset(soma, 0, sizeof(soma));
        bool algo = false;
        pthread_mutex_lock(&mtTrava);
        for (int k = 0; k < MEET_MAX_PESSOAS; k++) {
            MtPessoa* p = &mtPessoas[k];
            if (!p->usada || !p->anel) continue;
            int disponivel = (p->fim - p->ini + MT_ANEL) % MT_ANEL;
            if (disponivel > 8000) p->ini = (p->fim - 1600 + MT_ANEL) % MT_ANEL;   // atrasou demais: pula para perto do fim
            for (int i = 0; i < 320 && p->ini != p->fim; i++) { soma[i] += p->anel[p->ini]; p->ini = (p->ini + 1) % MT_ANEL; algo = true; }
        }
        pthread_mutex_unlock(&mtTrava);
        for (int i = 0; i < 320; i++) mono[i] = (int16_t)(soma[i] > 32767 ? 32767 : soma[i] < -32768 ? -32768 : soma[i]);
        // 16 kHz mono -> 48 kHz estéreo
        for (int i = 0; i < 960; i++) { int16_t v = mono[i / 3]; estereo[i * 2] = v; estereo[i * 2 + 1] = v; }
        if (!algo) { remotoDormir(10); if (appSomEscrito - appSomLido > 960) continue; }
        appSomEmpurrar(estereo, 960, &mtSomParar);
    }
    mtSomRodando = false;
    return NULL;
}
// ---------------------------------------------------------------- microfone do PS4 para a sala
static void* mtThreadMic(void*) {
    extern int32_t global_uId;
    typedef int (*AbrirMic)(int32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef int (*LerMic)(int32_t, void*);
    typedef int (*FecharMic)(int32_t);
    int mic = ((AbrirMic)sceAudioInOpen)(global_uId, 0, 0, 256, 16000, 0);
    if (mic < 0) mic = ((AbrirMic)sceAudioInOpen)(255, 0, 0, 256, 16000, 0);
    if (mic < 0) { snprintf(mtAviso, sizeof(mtAviso), "Sem microfone (ligue um fone no controle). Os outros ainda te ouvem pelo chat."); mtMicRodando = false; return NULL; }
    static int16_t lido[256], bloco[1024];
    int n = 0, erros = 0;
    while (!mtMicParar) {
        int r = ((LerMic)sceAudioInInput)(mic, lido);
        if (r < 0) { if (++erros > 300) break; sceKernelUsleep(5000); continue; }
        erros = 0;
        memcpy(bloco + n, lido, 512); n += 256;
        if (n == 1024) {
            if (!mtMudo && mtSala >= 0) meetPs4EnviarMidia(mtSala, 2, (const uint8_t*)bloco, sizeof(bloco));
            n = 0;
        }
    }
    ((FecharMic)sceAudioInClose)(mic);
    mtMicRodando = false;
    return NULL;
}

// ---------------------------------------------------------------- UPnP: abre a porta no roteador
static bool mtXmlTag(const char* xml, const char* tag, char* out, int tam, const char* depois = NULL) {
    out[0] = 0;
    const char* base = depois ? depois : xml;
    char a[64]; snprintf(a, sizeof(a), "<%s>", tag);
    const char* p = strstr(base, a);
    if (!p) { snprintf(a, sizeof(a), ":%s>", tag); p = strstr(base, a); if (!p) return false; }
    p += strlen(a);
    const char* f = strchr(p, '<'); if (!f) return false;
    snprintf(out, tam, "%.*s", (int)(f - p), p);
    return true;
}
static int mtSoap(const char* acao, const char* argumentos, char* resp, int tamResp) {
    char corpo[1500];
    int n = snprintf(corpo, sizeof(corpo), "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>", acao, mtUpnpServico, argumentos, acao);
    char cab[300]; snprintf(cab, sizeof(cab), "SOAPAction: \"%s#%s\"\n", mtUpnpServico, acao);
    char* r = NULL; size_t tr = 0;
    int st = iaHttpPost(mtUpnpControle, cab, "text/xml; charset=\"utf-8\"", corpo, (size_t)n, &r, &tr);
    if (resp) { snprintf(resp, tamResp, "%.*s", (int)(tr < (size_t)tamResp - 1 ? tr : (size_t)tamResp - 1), r ? r : ""); }
    free(r);
    return st;
}
static bool mtIpPrivado(const char* ip) {
    unsigned a = 0, b = 0; sscanf(ip, "%u.%u", &a, &b);
    return a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127) || a == 0;
}
static void* mtThreadUpnp(void*) {
    snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "Procurando o roteador...");
    mtUpnpControle[0] = 0;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv = { 1, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in grupo; memset(&grupo, 0, sizeof(grupo)); grupo.sin_family = AF_INET; grupo.sin_port = htons(1900); inet_pton(AF_INET, "239.255.255.250", &grupo.sin_addr);
    char local[300] = "";
    for (int tentativa = 0; tentativa < 3 && !local[0]; tentativa++) {
        for (int v = 1; v <= 2; v++) {
            char m[300]; int n = snprintf(m, sizeof(m), "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:%d\r\n\r\n", v);
            sendto(s, m, n, 0, (struct sockaddr*)&grupo, sizeof(grupo));
        }
        for (int k = 0; k < 6 && !local[0]; k++) {
            char b[1500]; int n = (int)recv(s, b, sizeof(b) - 1, 0);
            if (n <= 0) continue;
            b[n] = 0;
            for (char* p = b; *p; p++) if (!strncasecmp(p, "\nlocation:", 10)) { p += 10; while (*p == ' ') p++; int j = 0; while (p[j] && p[j] != '\r' && p[j] != '\n' && j < 299) { local[j] = p[j]; j++; } local[j] = 0; break; }
        }
    }
    close(s);
    if (!local[0]) { snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "O roteador nao respondeu ao UPnP. Ligue o UPnP nele ou abra a porta %d para o PS4.", MEET_PORTA); mtUpnpRodando = false; return NULL; }
    char* xml = NULL; size_t tx = 0;
    int st = iaHttpGet(local, &xml, &tx);
    const char* serv = xml ? strstr(xml, "WANIPConnection:") : NULL;
    if (!serv && xml) serv = strstr(xml, "WANPPPConnection:");
    if (st != 200 || !serv) { free(xml); snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "O roteador nao tem o servico de abrir portas (UPnP)."); mtUpnpRodando = false; return NULL; }
    // tipo do serviço (até o '<') e o controlURL do mesmo bloco
    const char* ini = serv; while (ini > xml && *(ini - 1) != '>') ini--;
    snprintf(mtUpnpServico, sizeof(mtUpnpServico), "%.*s", (int)(strchr(ini, '<') - ini), ini);
    char ctrl[300]; mtXmlTag(xml, "controlURL", ctrl, sizeof(ctrl), serv);
    free(xml);
    if (!strncmp(ctrl, "http", 4)) snprintf(mtUpnpControle, sizeof(mtUpnpControle), "%s", ctrl);
    else { const char* h = strstr(local, "://"); const char* barra = h ? strchr(h + 3, '/') : NULL; snprintf(mtUpnpControle, sizeof(mtUpnpControle), "%.*s%s%s", (int)(barra ? barra - local : (long)strlen(local)), local, ctrl[0] == '/' ? "" : "/", ctrl); }
    char ip[64] = ""; remotoMeuIp(ip, sizeof(ip));
    char args[600];
    snprintf(args, sizeof(args), "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>TCP</NewProtocol><NewInternalPort>%d</NewInternalPort>"
        "<NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>Hyper Neiva Meet</NewPortMappingDescription><NewLeaseDuration>0</NewLeaseDuration>", MEET_PORTA, MEET_PORTA, ip);
    int r = mtSoap("AddPortMapping", args, NULL, 0);
    char resp[2000]; char externo[64] = "";
    if (mtSoap("GetExternalIPAddress", "", resp, sizeof(resp)) == 200) mtXmlTag(resp, "NewExternalIPAddress", externo, sizeof(externo));
    meetLog("MEET UPnP: %s porta %d -> %d, IP externo %s", mtUpnpServico, MEET_PORTA, r, externo);
    if (r != 200) snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "O roteador recusou abrir a porta %d (HTTP %d). Abra manualmente para %s.", MEET_PORTA, r, ip);
    else if (!externo[0]) snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "Porta aberta, mas o roteador nao disse o IP publico.");
    else if (mtIpPrivado(externo)) snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "Seu provedor usa CGNAT (IP %s): de fora da sua casa nao da para entrar sem um servidor.", externo);
    else {
        mtUpnpAberto = true;
        char k[40] = ""; if (mtSala >= 0 && meetSalas[mtSala].privada) snprintf(k, sizeof(k), "?k=%s", meetSalas[mtSala].chave);
        snprintf(mtLinkInternet, sizeof(mtLinkInternet), "https://%s:%d/meet/%s%s", externo, MEET_PORTA, mtSala >= 0 ? meetSalas[mtSala].nome : "", k);
        snprintf(mtAvisoInternet, sizeof(mtAvisoInternet), "Aberto para a internet. Quem estiver fora de casa usa este link.");
        mtVerInternet = true;
    }
    mtUpnpRodando = false;
    return NULL;
}
static void mtFecharPortaUpnp() {
    if (!mtUpnpAberto || !mtUpnpControle[0]) return;
    char args[300]; snprintf(args, sizeof(args), "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>TCP</NewProtocol>", MEET_PORTA);
    mtSoap("DeletePortMapping", args, NULL, 0);
    mtUpnpAberto = false;
}

// ---------------------------------------------------------------- abrir / fechar a sala do PS4
static void mtMontarLinks() {
    char ip[64] = ""; remotoMeuIp(ip, sizeof(ip));
    char k[40] = ""; if (mtSala >= 0 && meetSalas[mtSala].privada) snprintf(k, sizeof(k), "?k=%s", meetSalas[mtSala].chave);
    snprintf(mtLinkLocal, sizeof(mtLinkLocal), "https://%s:%d/meet/%s%s", ip, MEET_PORTA, mtSala >= 0 ? meetSalas[mtSala].nome : "", k);
}
static void mtEntrarNaSala(const char* nome, bool privada, bool aprovacao) {
    if (!meetIniciar(REMOTO_RAIZ "/configuracao")) { snprintf(mtAviso, sizeof(mtAviso), "O Meet nao ligou: %s", meetErro); return; }
    mtLimparPessoas();
    mtChatN = 0;
    mtSala = meetPs4AbrirSala(nome, privada, aprovacao, mtMeuNome, !mtMudo);
    if (mtSala < 0) { snprintf(mtAviso, sizeof(mtAviso), "Nao deu para abrir a sala (nome invalido ou limite de salas)."); return; }
    mtMontarLinks();
    mtLinkInternet[0] = 0; mtVerInternet = false; mtConfirmarSair = false; mtAviso[0] = 0;
    mtSomParar = false; mtMicParar = false;
    if (!mtSomRodando && remotoCriarThread(mtThreadSom, NULL)) mtSomRodando = true;
    if (!mtMicRodando && remotoCriarThread(mtThreadMic, NULL)) mtMicRodando = true;
    if (mtInternet && !mtUpnpRodando && remotoCriarThread(mtThreadUpnp, NULL)) mtUpnpRodando = true;
    pthread_mutex_lock(&meetTrava); privada = meetSalas[mtSala].privada; aprovacao = meetSalas[mtSala].aprovacao; pthread_mutex_unlock(&meetTrava);
    meetPs4Texto(mtSala, "Sala", privada ? "privada: so entra quem tem o link com o codigo" : "aberta: quem tiver o link entra", true);
    if (aprovacao) meetPs4Texto(mtSala, "Sala", "com aprovacao: cada pessoa pede para entrar; X aceita, O recusa", true);
    meetPs4Texto(mtSala, "Sala", "link permanente: vale sempre que o app estiver aberto", true);
    conquistaGanhar("meet");
}
static void mtSairDaSala() {
    if (mtSala < 0) return;
    meetPs4FecharSala(mtSala);
    mtSala = -1;
    mtSomParar = true; mtMicParar = true;
    for (int i = 0; i < 100 && (mtSomRodando || mtMicRodando); i++) remotoDormir(10);
    mtFecharPortaUpnp();
    mtLimparPessoas();
}
static void mtSalvarConfig() { FILE* f = fopen(MT_CONFIG, "w"); if (f) { fprintf(f, "%s\n%d\n%d\n", mtMeuNome, mtInternet ? 1 : 0, mtAprovacao ? 1 : 0); fclose(f); } }
static void mtLerConfig() {
    FILE* f = fopen(MT_CONFIG, "r");
    if (!f) return;
    char l[64];
    if (fgets(l, sizeof(l), f)) { l[strcspn(l, "\r\n")] = 0; if (l[0]) snprintf(mtMeuNome, sizeof(mtMeuNome), "%.20s", l); }
    if (fgets(l, sizeof(l), f)) mtInternet = atoi(l) != 0;
    if (fgets(l, sizeof(l), f)) mtAprovacao = atoi(l) != 0;
    fclose(f);
}
// Ao abrir o app: se há salas salvas, liga o servidor para os links permanentes funcionarem.
static void* mtThreadLigarSalvas(void*) { meetIniciar(REMOTO_RAIZ "/configuracao"); return NULL; }
void meetLigarSeTiverSalas() {
    FILE* f = fopen(REMOTO_RAIZ "/configuracao/meet_salas.txt", "r");
    if (!f) return;
    int c = fgetc(f); fclose(f);
    if (c != EOF) remotoCriarThread(mtThreadLigarSalvas, NULL);
}

// ---------------------------------------------------------------- tela
void meetAbrirTela() {
    extern MenuLevel menuAtual;
    mtLerConfig();
    menuAtual = MENU_MEET;
    mtSelMenu = 0; mtAviso[0] = 0;
}
bool meetControles(uint32_t botoes, uint32_t antigos, int32_t usuario) {
    if (mtImeModo) {
        char t[300]; int r = iaTecladoResultado(t, sizeof(t));
        if (r < 0) return false;
        int modo = mtImeModo; mtImeModo = 0;
        if (r == 1) {
            if (modo == 1) mtEntrarNaSala(t, mtCriarPrivada, mtAprovacao);
            else if (modo == 2 && mtSala >= 0) meetPs4EnviarChat(mtSala, t);
            else if (modo == 3) { snprintf(mtMeuNome, sizeof(mtMeuNome), "%.20s", t); mtSalvarConfig(); }
        }
        return false;
    }
    uint32_t n = botoes & ~antigos;
    if (mtSala < 0) {
        // menu: criar sala, entrada, nome, internet, e as salas salvas (links permanentes)
        int extras = 0; int salasExtras[MEET_MAX_SALAS];
        pthread_mutex_lock(&meetTrava);
        for (int i = 0; i < MEET_MAX_SALAS; i++) if (meetSalas[i].usada) salasExtras[extras++] = i;
        pthread_mutex_unlock(&meetTrava);
        int total = MT_ITENS + extras;
        if (mtSelMenu >= total) mtSelMenu = total - 1;
        if (n & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_CIRCLE)) mtConfirmarApagar = -1;
        if (n & ORBIS_PAD_BUTTON_UP) mtSelMenu = (mtSelMenu + total - 1) % total;
        if (n & ORBIS_PAD_BUTTON_DOWN) mtSelMenu = (mtSelMenu + 1) % total;
        if (n & ORBIS_PAD_BUTTON_CIRCLE) return true;
        if (n & ORBIS_PAD_BUTTON_CROSS) {
            if (mtSelMenu == 0 || mtSelMenu == 1) { mtCriarPrivada = mtSelMenu == 1; if (iaAbrirTecladoBase(usuario, "Nome da sala (letras e numeros)", 24, "familia")) mtImeModo = 1; }
            else if (mtSelMenu == 2) { mtAprovacao = !mtAprovacao; mtSalvarConfig(); }
            else if (mtSelMenu == 3) { if (iaAbrirTecladoBase(usuario, "Seu nome no Meet", 20, mtMeuNome)) mtImeModo = 3; }
            else if (mtSelMenu == 4) { mtInternet = !mtInternet; mtSalvarConfig(); }
            else { int s = salasExtras[mtSelMenu - MT_ITENS]; char nome[32]; snprintf(nome, sizeof(nome), "%s", meetSalas[s].nome); mtEntrarNaSala(nome, meetSalas[s].privada, meetSalas[s].aprovacao); }
        }
        if ((n & ORBIS_PAD_BUTTON_TRIANGLE) && mtSelMenu >= MT_ITENS) {
            int s = salasExtras[mtSelMenu - MT_ITENS];
            if (mtConfirmarApagar != s) { mtConfirmarApagar = s; snprintf(mtAviso, sizeof(mtAviso), "Triangulo de novo apaga a sala %s (o link deixa de valer).", meetSalas[s].nome); }
            else { meetApagarSala(s); mtConfirmarApagar = -1; snprintf(mtAviso, sizeof(mtAviso), "Sala apagada."); }
        }
        return false;
    }
    // na sala: pedido de entrada esperando? X aceita, O recusa
    char quem[24]; int pedido = meetPrimeiroPedido(mtSala, quem, sizeof(quem));
    if (pedido >= 0 && (n & (ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_CIRCLE))) {
        bool ok = (n & ORBIS_PAD_BUTTON_CROSS) != 0;
        if (meetDecidir(mtSala, pedido, ok)) meetPs4Texto(mtSala, quem, ok ? "foi aceito" : "foi recusado", true);
        mtConfirmarSair = false;
        return false;
    }
    if (n & ORBIS_PAD_BUTTON_CIRCLE) {
        if (!mtConfirmarSair) { mtConfirmarSair = true; snprintf(mtAviso, sizeof(mtAviso), "O de novo encerra a chamada para todos (a sala e o link continuam salvos)."); return false; }
        mtSairDaSala();
        return false;
    }
    if (n & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT | ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1)) mtConfirmarSair = false;
    if (n & ORBIS_PAD_BUTTON_SQUARE) { if (iaAbrirTecladoBase(usuario, "Mensagem para a sala", 300, NULL)) mtImeModo = 2; }
    if (n & ORBIS_PAD_BUTTON_TRIANGLE) { mtMudo = !mtMudo; meetPs4Estado(mtSala, !mtMudo); snprintf(mtAviso, sizeof(mtAviso), mtMudo ? "Microfone desligado" : "Microfone ligado"); }
    if (n & ORBIS_PAD_BUTTON_R1) mtSelPessoa++;
    if (n & ORBIS_PAD_BUTTON_L1) mtSelPessoa--;
    if (n & ORBIS_PAD_BUTTON_R2) {
        if (mtLinkInternet[0]) mtVerInternet = !mtVerInternet;
        else if (!mtUpnpRodando && remotoCriarThread(mtThreadUpnp, NULL)) mtUpnpRodando = true;
    }
    if (n & ORBIS_PAD_BUTTON_OPTIONS) {
        int ids[MEET_MAX_PESSOAS], q = 0;
        pthread_mutex_lock(&meetTrava);
        for (int k = 0; k < MEET_MAX_PESSOAS; k++) if (meetSalas[mtSala].pessoas[k]) ids[q++] = meetSalas[mtSala].pessoas[k]->id;
        pthread_mutex_unlock(&meetTrava);
        if (q) { int alvo = ids[((mtSelPessoa % q) + q) % q]; meetExpulsar(mtSala, alvo); snprintf(mtAviso, sizeof(mtAviso), "Participante removido."); }
    }
    return false;
}

static void mtDesenharImagem(uint32_t* p, const uint8_t* rgba, int iw, int ih, int x, int y, int w, int h) {
    // cobre o quadro (corta as sobras), amostragem simples
    float esc = fmaxf((float)w / iw, (float)h / ih);
    int ox = (int)((iw * esc - w) / 2), oy = (int)((ih * esc - h) / 2);
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy; if (py < 0 || py >= 1080) continue;
        int sy = (int)((yy + oy) / esc); if (sy >= ih) sy = ih - 1;
        const uint8_t* linha = rgba + (size_t)sy * iw * 4;
        uint32_t* d = p + py * 1920;
        for (int xx = 0; xx < w; xx++) {
            int px = x + xx; if (px < 0 || px >= 1920) continue;
            int sx = (int)((xx + ox) / esc); if (sx >= iw) sx = iw - 1;
            const uint8_t* c = linha + sx * 4;
            d[px] = 0xFF000000u | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2];
        }
    }
}
void meetRenderizar(uint32_t* p) {
    telaFundo(p, 0xFF0F1216);
    desenharTexto(p, "Hyper Neiva Meet", 40, 50, 30, 0xFFFFFFFF);
    if (mtSala < 0) {
        desenharTexto(p, "Chamada de video servida pelo PS4: chat, audio e camera pelo navegador do celular ou do PC.", 22, 50, 88, 0xFF8A96A3);
        const char* itens[MT_ITENS] = { "Criar sala aberta (quem tem o link entra)", "Criar sala privada (link com codigo)", NULL, NULL, NULL };
        char ent[120]; snprintf(ent, sizeof(ent), "Entrada nas salas novas: %s", mtAprovacao ? "pedir aprovacao (voce aceita cada pessoa)" : "livre");
        char nome[80]; snprintf(nome, sizeof(nome), "Seu nome: %s", mtMeuNome);
        char net[120]; snprintf(net, sizeof(net), "Abrir para a internet (UPnP no roteador): %s", mtInternet ? "sim" : "nao");
        itens[2] = ent; itens[3] = nome; itens[4] = net;
        int y = 150;
        int extras = 0;
        pthread_mutex_lock(&meetTrava);
        char salas[MEET_MAX_SALAS][120];
        for (int i = 0; i < MEET_MAX_SALAS; i++) if (meetSalas[i].usada) {
            int pe = meetPessoasNaSala(i);
            char agora[40] = ""; if (pe) snprintf(agora, sizeof(agora), "  -  %d na chamada agora", pe);
            snprintf(salas[extras++], 120, "Sala %s  -  %s, %s%s", meetSalas[i].nome, meetSalas[i].privada ? "privada" : "aberta", meetSalas[i].aprovacao ? "com aprovacao" : "entrada livre", agora);
        }
        pthread_mutex_unlock(&meetTrava);
        for (int i = 0; i < MT_ITENS + extras; i++) {
            if (i == MT_ITENS) { desenharTexto(p, "Salas salvas (o link de cada uma e permanente)", 22, 60, y + 4, 0xFF8FA6BF); y += 44; }
            if (i == mtSelMenu) telaRet(p, 40, y - 8, 1200, 52, 0xFF24456B);
            desenharTexto(p, i < MT_ITENS ? itens[i] : salas[i - MT_ITENS], 28, 60, y, i == mtSelMenu ? 0xFFFFFFFF : 0xFFC0CCD8);
            y += 58;
        }
        desenharTexto(p, "Quem entra precisa aceitar o aviso de certificado do navegador uma vez (o PS4 assina o proprio certificado).", 22, 50, 900, 0xFFFFE08A);
        if (mtAviso[0]) desenharTexto(p, mtAviso, 24, 50, 950, 0xFFFF8A80);
        desenharTexto(p, mtSelMenu >= MT_ITENS ? "X entra na sala   Triangulo apaga a sala   O volta" : "X escolhe   O volta", 22, 50, 1020, 0xFF8A96A3);
        return;
    }
    // ---- na sala
    char t[300];
    pthread_mutex_lock(&meetTrava);
    snprintf(t, sizeof(t), "Sala %s  -  %s  -  %d pessoa(s)", meetSalas[mtSala].nome, meetSalas[mtSala].privada ? "privada" : "aberta", meetPessoasNaSala(mtSala));
    int ids[MEET_MAX_PESSOAS + 1]; char nomes[MEET_MAX_PESSOAS + 1][24]; bool cams[MEET_MAX_PESSOAS + 1], mics[MEET_MAX_PESSOAS + 1]; int q = 0;
    ids[q] = 0; snprintf(nomes[q], 24, "%s", mtMeuNome); cams[q] = false; mics[q] = !mtMudo; q++;
    for (int k = 0; k < MEET_MAX_PESSOAS; k++) { MeetCliente* c = meetSalas[mtSala].pessoas[k]; if (c) { ids[q] = c->id; snprintf(nomes[q], 24, "%s", c->nome); cams[q] = c->camOn; mics[q] = c->micOn; q++; } }
    pthread_mutex_unlock(&meetTrava);
    desenharTexto(p, t, 24, 50, 82, 0xFF8FA6BF);
    int cols = q <= 1 ? 1 : q <= 4 ? 2 : 3, linhas = (q + cols - 1) / cols;
    int gx = 40, gy = 130, gw = 1260, gh = 860;
    int tw = (gw - (cols - 1) * 12) / cols, th = (gh - (linhas - 1) * 12) / linhas;
    if (th > tw * 3 / 4) th = tw * 3 / 4; else tw = th * 4 / 3;
    int sel = q > 1 ? ((mtSelPessoa % (q - 1)) + (q - 1)) % (q - 1) + 1 : -1;
    uint64_t agora = remotoAgoraMs();
    pthread_mutex_lock(&mtTrava);
    for (int i = 0; i < q; i++) {
        int x = gx + (i % cols) * (tw + 12), y = gy + (i / cols) * (th + 12);
        MtPessoa* pe = ids[i] ? mtPessoaDe(ids[i], false) : NULL;
        bool fala = pe && agora - pe->falouMs < 400;
        telaRet(p, x - 3, y - 3, tw + 6, th + 6, i == sel ? 0xFFFFD54F : fala ? 0xFF29B6F6 : 0xFF2A313A);
        if (pe && pe->rgba && cams[i]) mtDesenharImagem(p, pe->rgba, pe->w, pe->h, x, y, tw, th);
        else {
            telaRet(p, x, y, tw, th, 0xFF24303C);
            char ini[8]; snprintf(ini, sizeof(ini), "%.1s", nomes[i]);
            desenharTexto(p, ini, th / 3, x + tw / 2 - th / 10, y + th / 3, 0xFFFFFFFF);
        }
        telaRet(p, x + 8, y + th - 44, medirLarguraTexto(nomes[i], 22) + 20, 36, 0xC0000000);
        desenharTexto(p, nomes[i], 22, x + 18, y + th - 40, 0xFFFFFFFF);
        if (!mics[i]) { telaRet(p, x + tw - 86, y + 8, 78, 32, 0xC0000000); desenharTexto(p, "mudo", 20, x + tw - 76, y + 12, 0xFFFF8A80); }
    }
    pthread_mutex_unlock(&mtTrava);
    // lado direito: QR, link, chat
    int rx = 1330;
    telaRet(p, rx - 10, 70, 590, 1000, 0xFF171C22);
    const char* link = mtVerInternet && mtLinkInternet[0] ? mtLinkInternet : mtLinkLocal;
    remotoDesenharQrTexto(p, link, rx + 10, 90, 5);
    desenharTexto(p, mtVerInternet && mtLinkInternet[0] ? "Link da internet" : "Link da sua rede (Wi-Fi de casa)", 20, rx + 260, 100, 0xFF8FA6BF);
    char l1[120], l2[120]; snprintf(l1, sizeof(l1), "%.40s", link); snprintf(l2, sizeof(l2), "%.80s", strlen(link) > 40 ? link + 40 : "");
    desenharTexto(p, l1, 18, rx + 260, 130, 0xFF8FC8FF); desenharTexto(p, l2, 18, rx + 260, 156, 0xFF8FC8FF);
    if (mtUpnpRodando) desenharTexto(p, "Abrindo a porta no roteador...", 18, rx + 260, 196, 0xFFFFE08A);
    else if (mtAvisoInternet[0]) {
        char a1[60], a2[60], a3[60]; snprintf(a1, 60, "%.30s", mtAvisoInternet); snprintf(a2, 60, "%.30s", strlen(mtAvisoInternet) > 30 ? mtAvisoInternet + 30 : ""); snprintf(a3, 60, "%.30s", strlen(mtAvisoInternet) > 60 ? mtAvisoInternet + 60 : "");
        desenharTexto(p, a1, 17, rx + 260, 196, 0xFFFFE08A); desenharTexto(p, a2, 17, rx + 260, 218, 0xFFFFE08A); desenharTexto(p, a3, 17, rx + 260, 240, 0xFFFFE08A);
    }
    desenharTexto(p, "Chat", 24, rx, 330, 0xFFFFFFFF);
    pthread_mutex_lock(&mtTrava);
    int ini = mtChatN > 22 ? mtChatN - 22 : 0;
    for (int i = ini; i < mtChatN; i++) { char lc[80]; snprintf(lc, sizeof(lc), "%.44s", mtChat[i]); desenharTexto(p, lc, 20, rx, 370 + (i - ini) * 28, mtChat[i][0] == '*' ? 0xFF8A96A3 : 0xFFE0E6EC); }
    pthread_mutex_unlock(&mtTrava);
    char quem[24]; int pedido = meetPrimeiroPedido(mtSala, quem, sizeof(quem));
    if (pedido >= 0) {
        int np = meetQuantosPedidos(mtSala);
        char tp[160]; snprintf(tp, sizeof(tp), "%s quer entrar na sala   X aceita   O recusa%s", quem, np > 1 ? "   (mais pedidos esperando)" : "");
        telaRet(p, 40, 990, 1260, 44, 0xFF5A3A00);
        desenharTexto(p, tp, 24, 56, 998, 0xFFFFFFFF);
    }
    else if (mtAviso[0]) desenharTexto(p, mtAviso, 22, 50, 1000, 0xFFFFE08A);
    desenharTexto(p, "Quadrado escreve   Triangulo mudo   R2 link da internet   L1/R1 escolhe pessoa   Options remove   O fecha a sala", 20, 50, 1040, 0xFF8A96A3);
}
