#include <iostream>
#include <ostream>

#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <netinet/in.h>

namespace cppless::tcpunch
{
namespace asio = boost::asio;

class client
{
public:
  explicit client(asio::io_service& ioc)
      : m_incoming_socket(ioc)
      , m_timer(ioc)
      , m_socket(ioc)
      , m_acceptor(ioc)
      , m_peer_socket(ioc)
  {
    using reuse_addr = asio::socket_base::reuse_address;
    using reuse_port =
        asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT>;

    m_socket.open(asio::ip::tcp::v4());
    m_socket.set_option(asio::ip::tcp::socket::reuse_address(/*v=*/true));
    m_socket.set_option(reuse_port(/*v=*/true));

    m_acceptor.open(asio::ip::tcp::v4());
    m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(/*v=*/true));
    m_acceptor.set_option(reuse_port(/*v=*/true));

    m_peer_socket.open(asio::ip::tcp::v4());
    m_peer_socket.set_option(asio::ip::tcp::socket::reuse_address(/*v=*/true));
    m_peer_socket.set_option(reuse_port(/*v=*/true));
  }

  auto async_connect(const asio::ip::tcp::resolver::results_type& endpoints,
                     std::string connection_key,
                     std::function<void(const boost::system::error_code&,
                                        asio::ip::tcp::socket&)> callback)

  {
    m_connection_key = std::move(connection_key);
    m_callback = std::move(callback);
    asio::async_connect(m_socket,
                        endpoints,
                        boost::bind(&client::handle_connect,
                                    this,
                                    asio::placeholders::error,
                                    asio::placeholders::endpoint));
  }

private:
  static constexpr unsigned int connect_retry_delay = 100;  // ms
  static constexpr unsigned int peer_connection_data_alignment = 8;
  struct peer_connection_data
  {
    asio::ip::address_v4 ip;
    // port in network byte order
    in_port_t port {};
  } __attribute__((aligned(peer_connection_data_alignment)));

  auto handle_connect(boost::system::error_code /*ec*/,
                      const asio::ip::tcp::endpoint& /*unused*/) -> void
  {
    auto buf = asio::buffer(m_connection_key);
    asio::async_write(
        m_socket, buf, boost::bind(&client::handle_pairing_name_send, this));
  }

  auto handle_pairing_name_send() -> void
  {
    asio::async_read(m_socket,
                     asio::buffer(&m_public_info, sizeof(peer_connection_data)),
                     asio::transfer_exactly(sizeof(peer_connection_data)),
                     boost::bind(&client::handle_pub_info_rcv, this));
  }

  auto handle_pub_info_rcv() -> void
  {
    asio::async_read(m_socket,
                     asio::buffer(&m_peer_conn, sizeof(peer_connection_data)),
                     asio::transfer_exactly(sizeof(peer_connection_data)),
                     boost::bind(&client::handle_peer_conn_recv, this));
  }

  auto handle_peer_conn_recv() -> void
  {
    std::cout << "exchanged connection data" << std::endl;
    asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::any(),
                                     ntohs(m_public_info.port));
    m_peer_socket.bind(endpoint);

    start_peer_listen();
    try_connect();
  }

  auto start_peer_listen() -> void
  {
    asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::any(),
                                     ntohs(m_public_info.port));  // NOLINT
    std::cout << "listening on: " << endpoint << std::endl;
    m_acceptor.bind(endpoint);

    m_acceptor.listen();
    accept_peer();
  }

  auto accept_peer() -> void
  {
    if (m_connection_established) {
      return;
    }
    m_acceptor.async_accept(
        m_incoming_socket,
        boost::bind(
            &client::handle_peer_accept, this, asio::placeholders::error));
  }

  auto handle_peer_accept(boost::system::error_code ec) -> void
  {
    if (m_connection_established) {
      return;
    }
    if (ec.failed()) {
      std::cout << "accept failed: " << ec.message() << std::endl;
      accept_peer();
      return;
    }
    std::cout << "peer connected" << std::endl;
    m_timer.cancel();
    if (not m_connection_established) {
      m_connection_established = true;
      m_callback({}, m_incoming_socket);
    }
  }

  auto try_connect() -> void
  {
    asio::ip::tcp::endpoint endpoint;
    endpoint.address(m_peer_conn.ip);
    endpoint.port(ntohs(m_peer_conn.port));  // NOLINT

    std::cout << "connecting from " << m_peer_socket.local_endpoint() << " to "
              << endpoint << std::endl;

    if (not m_connection_established) {
      m_peer_socket.async_connect(
          endpoint,
          boost::bind(
              &client::handle_peer_connect, this, asio::placeholders::error));
    }
  }

  auto handle_peer_connect(boost::system::error_code ec) -> void
  {
    if (ec.failed()) {
      std::cout << "connect failed: " << ec.message() << std::endl;
      m_timer.expires_from_now(
          boost::posix_time::millisec(connect_retry_delay));
      m_timer.async_wait(boost::bind(&client::try_connect, this));
    } else if (not m_connection_established) {
      std::cout << "connected to peer" << std::endl;
      m_connection_established = true;
      m_callback({}, m_peer_socket);
      m_acceptor.cancel();
    }
  }

public:
  std::string m_connection_key;
  peer_connection_data m_public_info;
  peer_connection_data m_peer_conn;
  asio::ip::tcp::socket m_incoming_socket;

  asio::ip::tcp::socket m_socket;

  asio::deadline_timer m_timer;
  asio::ip::tcp::socket m_peer_socket;
  asio::ip::tcp::acceptor m_acceptor;

  std::function<void(boost::system::error_code ec, asio::ip::tcp::socket&)>
      m_callback;

  bool m_connection_established = false;
};
}  // namespace cppless::tcpunch