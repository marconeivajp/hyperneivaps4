#ifndef TORRENT_ENGINE_H
#define TORRENT_ENGINE_H

#include <string>
#include <memory>

namespace libtorrent {
    class session;
}

struct TorrentStatus {
    std::string name;
    float progress;
    int download_rate;
    int num_peers;
    int state; // -1: Erro, 0: Baixando, 1: Concluido
};

class TorrentEngine {
public:
    TorrentEngine();
    ~TorrentEngine();

    bool init();
    bool addTorrentFile(const std::string& torrent_path, const std::string& save_path);
    void update();
    TorrentStatus getStatus();
    void shutdown();

private:
    std::unique_ptr<libtorrent::session> m_session;
    bool m_initialized;
    TorrentStatus m_current_status;
};

extern TorrentEngine g_torrentApp;

extern "C" {
    void iniciarDownloadTorrent(const char* torrent_path);
}

#endif // TORRENT_ENGINE_H