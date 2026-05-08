// === BLINDAGEM DO COMPILADOR PARA O PS4 ===
#define __CLANG_MAX_ALIGN_T_DEFINED 1
#include <stddef.h>
#define BOOST_ASIO_DISABLE_KQUEUE 1
#define BOOST_ASIO_DISABLE_EPOLL 1
#ifndef FIONBIO
#define FIONBIO 0x8004667e
#endif
#ifndef FIONREAD
#define FIONREAD 0x4004667f
#endif
#ifndef SO_NOSIGPIPE
#define SO_NOSIGPIPE 0x0800
#endif
// ==========================================

#include "torrent_engine.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

// === HEADERS DO PS4 PARA O BYPASS DE REDE ===
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
// ============================================

// Headers da libtorrent
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/bdecode.hpp>

namespace lt = libtorrent;

TorrentEngine g_torrentApp;

// =========================================================================
// FUNCAO DE LOG PARA DEBUG NO PS4
// =========================================================================
void EscreverLog(const std::string& mensagem) {
    std::ofstream logFile("/data/HyperNeiva/torrent_log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << "[TORRENT] " << mensagem << "\n";
        logFile.close();
    }
}
// =========================================================================

struct __jmp_buf_tag; 
namespace sig {
    namespace detail {
        int once = 0;
        void setup_handler() {}
        struct scoped_jmpbuf {
            scoped_jmpbuf(struct __jmp_buf_tag (*j)[1]);
            ~scoped_jmpbuf();
        };
        scoped_jmpbuf::scoped_jmpbuf(struct __jmp_buf_tag (*j)[1]) { (void)j; }
        scoped_jmpbuf::~scoped_jmpbuf() {}
    }
}

// Vacina para threads
extern "C" int __cxa_thread_atexit_impl(void (*func)(void*), void* arg, void* dso_handle) { return 0; }

TorrentEngine::TorrentEngine() : m_initialized(false) {
    m_current_status = {"Aguardando...", 0.0f, 0, 0, -1};
}

TorrentEngine::~TorrentEngine() {
    shutdown();
}

bool TorrentEngine::init() {
    if (m_initialized) return true;

    EscreverLog("Iniciando motor libtorrent. Tentativa de Bypass com SceNet (OpenOrbis Puro)...");

    // =========================================================================
    // BYPASS DE MEMÓRIA DO KERNEL (SCENET)
    // Inicializa a rede nativa do PS4 sem parametros adicionais (Padrao OpenOrbis)
    // =========================================================================
    EscreverLog("Carregando modulo de rede nativo do PS4 (libSceNet)...");
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    
    int netRes = sceNetInit();
    // 0 = Sucesso | 0x80410107 = O módulo já estava inicializado
    if (netRes == 0 || netRes == (int)0x80410107) { 
        EscreverLog("SceNet inicializado. Criando Pool de Sockets de 4MB...");
        sceNetPoolCreate("HyperNeivaTorrentPool", 4 * 1024 * 1024, 0);
    } else {
        EscreverLog("AVISO: sceNetInit retornou erro: " + std::to_string(netRes));
    }
    // =========================================================================

    try {
        lt::settings_pack pack;
        pack.set_int(lt::settings_pack::connections_limit, 100);    
        pack.set_int(lt::settings_pack::active_downloads, 1);      
        
        pack.set_bool(lt::settings_pack::enable_ip_notifier, false);
        pack.set_bool(lt::settings_pack::enable_upnp, false);
        pack.set_bool(lt::settings_pack::enable_natpmp, false);
        pack.set_bool(lt::settings_pack::enable_lsd, false);

        pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881,0.0.0.0:0");
        pack.set_str(lt::settings_pack::outgoing_interfaces, "");

        pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
        pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
        pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);

        pack.set_bool(lt::settings_pack::enable_dht, true);        
        pack.set_bool(lt::settings_pack::announce_to_all_trackers, true);
        pack.set_bool(lt::settings_pack::announce_to_all_tiers, true);
        
        pack.set_int(lt::settings_pack::alert_mask, 0xffffffff);

        m_session = std::make_unique<lt::session>(pack);
        m_initialized = true;
        EscreverLog("Sessao libtorrent gerada. Placa de rede sob Bypass.");
        
        return true;
    } catch (const std::exception& e) {
        EscreverLog(std::string("Erro fatal no init: ") + e.what());
        return false;
    }
}

bool TorrentEngine::addTorrentFile(const std::string& torrent_path, const std::string& save_path) {
    EscreverLog("Carregando arquivo: " + torrent_path);

    if (!m_initialized) init();
    if (!m_initialized || !m_session) return false;

    // === GARANTIA DE SANDBOX ===
    mkdir("/data/HyperNeiva", 0777);
    mkdir(save_path.c_str(), 0777);
    // ===========================

    try {
        lt::error_code ec;
        std::ifstream ifs(torrent_path, std::ios_base::binary);
        if (!ifs) {
            EscreverLog("ERRO FATAL: Nao consegui ler o arquivo .torrent!");
            return false;
        }

        std::vector<char> buf(std::istreambuf_iterator<char>(ifs), (std::istreambuf_iterator<char>()));
        EscreverLog("Lidos " + std::to_string(buf.size()) + " bytes do arquivo torrent.");

        lt::bdecode_node e = lt::bdecode(buf, ec);
        if (ec) {
            EscreverLog("ERRO NO BDECODE: " + ec.message());
            return false;
        }

        auto info = std::make_shared<lt::torrent_info>(e, ec);
        if (ec) {
            EscreverLog("ERRO NO TORRENT_INFO: " + ec.message());
            return false;
        }

        EscreverLog("Torrent reconhecido: " + info->name());

        lt::add_torrent_params atp;
        atp.ti = info;
        atp.save_path = save_path;

        atp.trackers.push_back("http://tracker.opentrackr.org:1337/announce");
        atp.trackers.push_back("udp://tracker.opentrackr.org:1337/announce");
        atp.trackers.push_back("udp://tracker.openbittorrent.com:80/announce");
        atp.trackers.push_back("udp://exodus.desync.com:6969/announce");
        
        lt::error_code add_ec;
        m_session->add_torrent(atp, add_ec);
        
        if (add_ec) {
            EscreverLog("ERRO AO ADICIONAR NA SESSAO: " + add_ec.message());
        } else {
            EscreverLog("Motor engatilhado. Tentando arrombar a conexao com os Peers...");
        }
        
        m_current_status.state = 0; 
        m_current_status.name = info->name();
        m_current_status.progress = 0.0f;
        
        return true;
        
    } catch (const std::exception& e) {
        EscreverLog(std::string("Excecao na criacao: ") + e.what());
        return false;
    }
}

void TorrentEngine::update() {
    if (!m_initialized || !m_session) return;

    std::vector<lt::alert*> alerts;
    m_session->pop_alerts(&alerts);

    static float progresso_impresso = -1.0f;

    for (lt::alert const* a : alerts) {
        if (a->type() != lt::session_stats_alert::alert_type && a->type() != lt::state_update_alert::alert_type) {
             EscreverLog(std::string("[EVENTO] ") + a->message());
        }

        if (auto st = lt::alert_cast<lt::state_update_alert>(a)) {
            if (st->status.empty()) continue;
            lt::torrent_status const& ts = st->status[0];
            
            m_current_status.progress = (ts.progress_ppm / 10000.f) * 100.0f; 
            m_current_status.download_rate = ts.download_rate;       
            m_current_status.num_peers = ts.num_peers;
            
            if (m_current_status.progress >= 0.0f && (m_current_status.progress != progresso_impresso || ts.num_peers > 0)) {
                EscreverLog("==> [DOWNLOAD] Progresso: " + std::to_string(m_current_status.progress) + "% | Velocidade: " + std::to_string(ts.download_rate / 1024) + " KB/s | Peers: " + std::to_string(ts.num_peers));
                progresso_impresso = m_current_status.progress;
            }

            if (ts.state == lt::torrent_status::finished || ts.state == lt::torrent_status::seeding) {
                m_current_status.state = 1; 
                m_current_status.progress = 100.0f;
                EscreverLog("==> [SUCESSO] DOWNLOAD 100% CONCLUIDO!");
            }
        }
        else if (auto err = lt::alert_cast<lt::torrent_error_alert>(a)) {
            m_current_status.state = -1;
            m_current_status.name = "Erro no Torrent";
            EscreverLog("!!! [ERRO FATAL NO TORRENT] !!! -> " + err->message());
        }
    }
}

TorrentStatus TorrentEngine::getStatus() {
    return m_current_status;
}

void TorrentEngine::shutdown() {
    EscreverLog("Encerrando motor e fechando conexoes.");
    if (m_session) m_session.reset(); 
    if (m_initialized) {
        m_initialized = false;
    }
}

extern "C" {
    void iniciarDownloadTorrent(const char* torrent_path) {
        std::ofstream logFile("/data/HyperNeiva/torrent_log.txt", std::ios_base::trunc);
        if (logFile.is_open()) {
            logFile << "[TORRENT] === TESTE DE BYPASS SCENET (ORBIS SDK) ===\n";
            logFile.close();
        }
        g_torrentApp.addTorrentFile(torrent_path, "/data/HyperNeiva/Torrents");
    }
}