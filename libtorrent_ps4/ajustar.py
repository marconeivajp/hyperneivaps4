"""Ajustes da libtorrent e da Boost para o PS4. Pode rodar de novo sem duplicar nada."""
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
LT = os.path.join(AQUI, "src", "libtorrent-rasterbar-2.1.2")
BOOST = os.path.join(AQUI, "src", "boost-1.92.0", "boost")


def trocar(base, rel, antigo, novo, todas=False):
    c = os.path.join(base, rel)
    d = open(c, "rb").read().decode("utf-8", "surrogateescape")
    nl = "\r\n" if "\r\n" in d else "\n"
    a, b = antigo.replace("\n", nl), novo.replace("\n", nl)
    if b in d:
        return
    assert a in d, (rel, antigo[:60])
    d = d.replace(a, b) if todas else d.replace(a, b, 1)
    open(c, "wb").write(d.encode("utf-8", "surrogateescape"))


# O PS4 é FreeBSD só no kernel: sem getrandom, mmap de disco, sysctl de rede nem ifaddrs.
trocar(LT, "include/libtorrent/config.hpp", """// non-Apple BSD
#define TORRENT_USE_GETRANDOM 1
#define TORRENT_HAS_PTHREAD_SET_NAME 1
#define TORRENT_USE_PWRITEV 1
""", """// non-Apple BSD
#define TORRENT_USE_GETRANDOM 0
""")
trocar(LT, "include/libtorrent/config.hpp", """#define TORRENT_HAS_SYMLINK 1

#ifndef TORRENT_HAVE_MMAP
#define TORRENT_HAVE_MMAP 1
#endif
#define TORRENT_USE_MADVISE 1

#define TORRENT_HAS_FALLOCATE 0

#define TORRENT_USE_IFADDRS 1
#define TORRENT_USE_SYSCTL 1
#define TORRENT_USE_IFCONF 1
""", """#define TORRENT_HAS_SYMLINK 0
#define TORRENT_HAS_FALLOCATE 0
#define TORRENT_USE_IFADDRS 0
#define TORRENT_USE_SYSCTL 0
#define TORRENT_USE_IFCONF 0
""")

# Sem API de rede no SDK: uma interface só, com o IP que sai para a internet.
trocar(LT, "src/enum_net.cpp", """#error "don't know how to enumerate network routes on this platform"
""", """		unsigned int const ip = hn_ip_local();
		if (ip != 0)
		{
			ip_route r;
			r.destination = address_v4::any();
			r.netmask = address_v4::any();
			r.gateway = address_v4((ip & 0xffffff00u) | 1u);
			r.source_hint = address_v4(ip);
			std::strncpy(r.name, "eth0", sizeof(r.name) - 1);
			r.mtu = 1500;
			ret.push_back(r);
		}
""")

trocar(LT, "src/enum_net.cpp", """#error "Don't know how to enumerate network interfaces on this platform"
""", """		unsigned int const ip = hn_ip_local();
		if (ip != 0)
		{
			ip_interface iface;
			iface.interface_address = address_v4(ip);
			iface.netmask = address_v4(0xffffff00u);
			std::strncpy(iface.name, "eth0", sizeof(iface.name) - 1);
			ret.push_back(iface);
		}
""")

# pipe() não foi testado no console; socket já é provado pelo Telegram.
for rel in ("asio/detail/select_interrupter.hpp", "asio/detail/socket_select_interrupter.hpp", "asio/detail/impl/socket_select_interrupter.ipp"):
    trocar(BOOST, rel, "defined(__SYMBIAN32__)", "defined(__SYMBIAN32__) || defined(HN_PS4)", True)

for rel in ("asio/ip/detail/impl/endpoint.ipp", "asio/detail/impl/socket_ops.ipp"):
    trocar(BOOST, rel, "sin6_family = BOOST_ASIO_OS_DEF(AF_INET6)", "sin6_family = HN_SIN6_FAMILY", True)
# pthread_condattr_t do SDK tem 4 bytes, mas o sistema guarda um ponteiro de 8 ali e corrompe a pilha.
trocar(BOOST, "asio/detail/impl/posix_event.ipp", """  ::pthread_condattr_t attr;
""", """  void* hn_attr[2] = { 0, 0 };
  ::pthread_condattr_t& attr = *reinterpret_cast<::pthread_condattr_t*>(hn_attr);
""")

# Marcas no log: o console não dá depurador, e cada teste custa tempo do usuário.
trocar(LT, "src/session.cpp", """			m_io_service = std::make_shared<io_context>(1);
			ios = m_io_service.get();
""", """			hn_marca("io_context");
			m_io_service = std::make_shared<io_context>(1);
			ios = m_io_service.get();
			hn_marca("io_context ok");
""")
trocar(LT, "src/session.cpp", """		m_impl = std::make_shared<aux::session_impl>(std::ref(*ios)
""", """		hn_marca("session_impl");
		m_impl = std::make_shared<aux::session_impl>(std::ref(*ios)
""")
trocar(LT, "src/session.cpp", """		m_impl->start_session();
""", """		hn_marca("start_session");
		m_impl->start_session();
		hn_marca("start_session ok");
""")
trocar(LT, "src/session_impl.cpp", """	void session_impl::init()
	{
""", """	void session_impl::init()
	{
		hn_marca("init rede");
""")
trocar(LT, "src/session_impl.cpp", """	void session_impl::reopen_listen_sockets(bool const map_ports)
	{
""", """	void session_impl::reopen_listen_sockets(bool const map_ports)
	{
		hn_marca("listen");
""")
trocar(LT, "src/session_impl.cpp", """	void session_impl::start_dht()
	{
""", """	void session_impl::start_dht()
	{
		hn_marca("dht");
""")
print("ajustes ok")
