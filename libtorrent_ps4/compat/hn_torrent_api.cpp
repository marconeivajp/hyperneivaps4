// Interface em C entre o app e a libtorrent. O app compila com outras flags e não enxerga Boost:
// só as funções daqui. Nada aqui deixa exceção escapar para o app.
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/posix_disk_io.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "hn_torrent_api.h"

namespace lt = libtorrent;

namespace {

std::unique_ptr<lt::session> sessao;
std::mutex trava;
std::string pastaDados;
std::string pastaDestino;
std::vector<lt::torrent_handle> lista;
std::vector<lt::torrent_status> situacao;
int resumosPendentes = 0;

void registrar(char const* formato, ...) {
    std::string caminho = pastaDados + "/torrent_log.txt";
    FILE* f = std::fopen(caminho.c_str(), "a");
    if (!f) return;
    va_list a;
    va_start(a, formato);
    std::vfprintf(f, formato, a);
    va_end(a);
    std::fputc('\n', f);
    std::fclose(f);
}

}  // namespace

extern "C" void hn_marca(const char* texto) { registrar("MARCA %s", texto); }

namespace {

std::string arquivoResumo(lt::torrent_handle const& h) {
    std::ostringstream nome;
    nome << h.info_hashes().get_best();
    return pastaDados + "/" + nome.str() + ".resume";
}

std::string arquivoResumo(lt::info_hash_t const& hash) {
    std::ostringstream nome;
    nome << hash.get_best();
    return pastaDados + "/" + nome.str() + ".resume";
}

void gravarResumo(lt::save_resume_data_alert const* a) {
    std::vector<char> dados = lt::write_resume_data_buf(a->params);
    std::string destino = arquivoResumo(a->params.info_hashes);
    std::string temporario = destino + ".tmp";
    std::ofstream saida(temporario, std::ios::binary);
    saida.write(dados.data(), (std::streamsize)dados.size());
    saida.close();
    if (saida.good()) std::rename(temporario.c_str(), destino.c_str());
}

void pedirResumo(lt::torrent_handle const& h) {
    if (!h.is_valid()) return;
    h.save_resume_data(lt::torrent_handle::save_info_dict | lt::torrent_handle::only_if_modified);
    resumosPendentes++;
}

void tratarAlertas() {
    std::vector<lt::alert*> alertas;
    sessao->pop_alerts(&alertas);
    for (lt::alert* a : alertas) {
        if (auto const* r = lt::alert_cast<lt::save_resume_data_alert>(a)) {
            gravarResumo(r);
            resumosPendentes--;
        }
        else if (lt::alert_cast<lt::save_resume_data_failed_alert>(a)) resumosPendentes--;
        else if (auto const* s = lt::alert_cast<lt::state_update_alert>(a)) {
            for (lt::torrent_status const& novo : s->status) {
                for (lt::torrent_status& antigo : situacao) {
                    if (antigo.handle == novo.handle) { antigo = novo; break; }
                }
            }
        }
        else if (auto const* c = lt::alert_cast<lt::torrent_finished_alert>(a)) {
            registrar("CONCLUIDO %s", c->torrent_name());
            pedirResumo(c->handle);
        }
        else if (auto const* m = lt::alert_cast<lt::metadata_received_alert>(a)) {
            registrar("METADADOS %s", m->torrent_name());
            pedirResumo(m->handle);
        }
        else if (a->category() & lt::alert_category::error) registrar("ERRO %s", a->message().c_str());
        else if (lt::alert_cast<lt::listen_succeeded_alert>(a) || lt::alert_cast<lt::listen_failed_alert>(a)
            || lt::alert_cast<lt::tracker_reply_alert>(a) || lt::alert_cast<lt::dht_bootstrap_alert>(a)) registrar("%s", a->message().c_str());
    }
}

void guardar(lt::torrent_handle const& h) {
    for (lt::torrent_handle const& existente : lista) if (existente == h) return;
    lista.push_back(h);
    situacao.push_back(h.status());
}

void carregarResumos() {
    DIR* d = opendir(pastaDados.c_str());
    if (!d) return;
    while (dirent* item = readdir(d)) {
        std::string nome = item->d_name;
        if (nome.size() < 8 || nome.compare(nome.size() - 7, 7, ".resume") != 0) continue;
        std::ifstream entrada(pastaDados + "/" + nome, std::ios::binary);
        std::vector<char> dados((std::istreambuf_iterator<char>(entrada)), std::istreambuf_iterator<char>());
        lt::error_code erro;
        lt::add_torrent_params p = lt::read_resume_data(dados, erro);
        if (erro) { registrar("RESUMO invalido %s: %s", nome.c_str(), erro.message().c_str()); continue; }
        lt::torrent_handle h = sessao->add_torrent(std::move(p), erro);
        if (erro) registrar("RESUMO recusado %s: %s", nome.c_str(), erro.message().c_str());
        else guardar(h);
    }
    closedir(d);
}

}  // namespace

extern "C" int hn_torrent_iniciar(const char* dados, const char* destino) {
    std::lock_guard<std::mutex> l(trava);
    if (sessao) return 0;
    try {
        pastaDados = dados;
        pastaDestino = destino;
        mkdir(dados, 0777);
        mkdir(destino, 0777);

        // Se exceção não funcionar no console, o log para aqui e mostra a causa do fechamento.
        registrar("TESTE excecao");
        try { throw std::runtime_error("ok"); }
        catch (std::exception const& e) { registrar("TESTE excecao %s", e.what()); }

        lt::settings_pack s;
        s.set_int(lt::settings_pack::alert_mask, lt::alert_category::error | lt::alert_category::status | lt::alert_category::storage | lt::alert_category::tracker | lt::alert_category::dht);
        s.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881");
        s.set_str(lt::settings_pack::user_agent, "HyperNeiva/1.0");
        s.set_bool(lt::settings_pack::enable_dht, true);
        s.set_bool(lt::settings_pack::enable_lsd, false);
        s.set_bool(lt::settings_pack::enable_upnp, false);
        s.set_bool(lt::settings_pack::enable_natpmp, false);
        s.set_int(lt::settings_pack::connections_limit, 80);
        s.set_int(lt::settings_pack::active_downloads, 2);
        s.set_int(lt::settings_pack::active_seeds, 0);
        s.set_str(lt::settings_pack::dht_bootstrap_nodes, "router.bittorrent.com:6881,dht.transmissionbt.com:6881,router.utorrent.com:6881");

        lt::session_params p(std::move(s));
        p.disk_io_constructor = lt::posix_disk_io_constructor;
        sessao = std::make_unique<lt::session>(std::move(p));
        registrar("SESSAO iniciada");
        carregarResumos();
        return 0;
    }
    catch (std::exception const& e) { registrar("FALHA iniciar: %s", e.what()); }
    catch (...) { registrar("FALHA iniciar"); }
    sessao.reset();
    return -1;
}

extern "C" int hn_torrent_adicionar(const char* origem, char* erroTexto, int tamanhoErro) {
    std::lock_guard<std::mutex> l(trava);
    if (erroTexto && tamanhoErro > 0) erroTexto[0] = 0;
    if (!sessao) return -1;
    try {
        lt::error_code erro;
        lt::add_torrent_params p;
        if (std::strncmp(origem, "magnet:", 7) == 0) p = lt::parse_magnet_uri(origem, erro);
        else {
            auto info = std::make_shared<lt::torrent_info>(std::string(origem), erro);
            if (!erro) p.ti = info;
        }
        if (!erro) {
            p.save_path = pastaDestino;
            lt::torrent_handle h = sessao->add_torrent(std::move(p), erro);
            if (!erro) {
                guardar(h);
                registrar("ADICIONADO %s", h.status(lt::torrent_handle::query_name).name.c_str());
                return (int)lista.size() - 1;
            }
        }
        registrar("RECUSADO %s: %s", origem, erro.message().c_str());
        if (erroTexto) std::snprintf(erroTexto, (size_t)tamanhoErro, "%s", erro.message().c_str());
    }
    catch (std::exception const& e) {
        registrar("FALHA adicionar: %s", e.what());
        if (erroTexto) std::snprintf(erroTexto, (size_t)tamanhoErro, "%s", e.what());
    }
    catch (...) { registrar("FALHA adicionar"); }
    return -1;
}

extern "C" void hn_torrent_atualizar(void) {
    std::lock_guard<std::mutex> l(trava);
    if (!sessao) return;
    try {
        sessao->post_torrent_updates();
        tratarAlertas();
        // O PS4 pode fechar o app sem aviso: guarda o progresso de tempos em tempos.
        static time_t ultimoResumo = time(nullptr);
        if (time(nullptr) - ultimoResumo >= 60) {
            ultimoResumo = time(nullptr);
            for (lt::torrent_handle const& h : lista) pedirResumo(h);
        }
    }
    catch (...) { registrar("FALHA atualizar"); }
}

extern "C" int hn_torrent_total(void) {
    std::lock_guard<std::mutex> l(trava);
    return (int)situacao.size();
}

extern "C" int hn_torrent_info(int indice, HnTorrentInfo* saida) {
    std::lock_guard<std::mutex> l(trava);
    if (indice < 0 || indice >= (int)situacao.size()) return -1;
    lt::torrent_status const& s = situacao[(size_t)indice];
    std::memset(saida, 0, sizeof(*saida));
    std::snprintf(saida->nome, sizeof(saida->nome), "%s", s.name.empty() ? "(buscando dados do torrent)" : s.name.c_str());
    saida->progresso = s.progress;
    saida->baixado = s.total_wanted_done;
    saida->total = s.total_wanted;
    saida->taxa = s.download_payload_rate;
    saida->fontes = s.num_peers;
    saida->pausado = (s.flags & lt::torrent_flags::paused) ? 1 : 0;
    saida->concluido = s.is_finished ? 1 : 0;
    saida->estado = (int)s.state;
    if (s.errc) std::snprintf(saida->erro, sizeof(saida->erro), "%s", s.errc.message().c_str());
    return 0;
}

extern "C" void hn_torrent_alternar_pausa(int indice) {
    std::lock_guard<std::mutex> l(trava);
    if (indice < 0 || indice >= (int)lista.size()) return;
    try {
        lt::torrent_handle& h = lista[(size_t)indice];
        if (h.flags() & lt::torrent_flags::paused) {
            h.set_flags(lt::torrent_flags::auto_managed);
            h.resume();
        }
        else {
            h.unset_flags(lt::torrent_flags::auto_managed);
            h.pause();
            pedirResumo(h);
        }
    }
    catch (...) { registrar("FALHA pausa"); }
}

extern "C" void hn_torrent_remover(int indice, int apagarArquivos) {
    std::lock_guard<std::mutex> l(trava);
    if (!sessao || indice < 0 || indice >= (int)lista.size()) return;
    try {
        lt::torrent_handle h = lista[(size_t)indice];
        std::remove(arquivoResumo(h).c_str());
        sessao->remove_torrent(h, apagarArquivos ? lt::session::delete_files : lt::remove_flags_t{});
        lista.erase(lista.begin() + indice);
        situacao.erase(situacao.begin() + indice);
    }
    catch (...) { registrar("FALHA remover"); }
}

extern "C" void hn_torrent_encerrar(void) {
    std::lock_guard<std::mutex> l(trava);
    if (!sessao) return;
    try {
        sessao->pause();
        for (lt::torrent_handle const& h : lista) pedirResumo(h);
        for (int i = 0; i < 100 && resumosPendentes > 0; i++) {
            sessao->wait_for_alert(lt::milliseconds(50));
            tratarAlertas();
        }
    }
    catch (...) { registrar("FALHA encerrar"); }
    lista.clear();
    situacao.clear();
    sessao.reset();
}
