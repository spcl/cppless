#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, const std::string& what)
{
  std::cerr << what << ": " << ec.message() << "\n";
}

class resolver_session
{
  std::string m_host;
  tcp::resolver m_resolver;
  std::optional<tcp::resolver::results_type> m_results;
  std::vector<std::function<void(tcp::resolver::results_type)>> m_callbacks;

public:
  explicit resolver_session(net::io_context& ioc)
      : m_resolver(net::make_strand(ioc))
  {
  }

  void run(const std::string& host, const std::string& port)
  {
    m_host = host;
    m_resolver.async_resolve(
        host,
        port,
        beast::bind_front_handler(&resolver_session::on_resolve, this));
  }

  void on_resolve(beast::error_code ec,
                  const tcp::resolver::results_type& results)
  {
    if (ec) {
      return fail(ec, "resolve");
    }

    m_results = results;
    for (auto& cb : m_callbacks) {
      cb(results);
    }
  }

  void add_callback(std::function<void(tcp::resolver::results_type)> cb)
  {
    m_callbacks.push_back(std::move(cb));
  }

  [[nodiscard]] auto host() const -> const std::string&
  {
    return m_host;
  }

  [[nodiscard]] auto results() const
      -> const std::optional<tcp::resolver::results_type>&
  {
    return m_results;
  }
};

// Performs an HTTP GET and prints the response
class http_request_session
{
  beast::ssl_stream<beast::tcp_stream> m_stream;
  beast::flat_buffer m_buffer;
  http::request<http::string_body> m_req;
  http::response<http::string_body> m_res;

public:
  // Objects are constructed with a strand to
  // ensure that handlers do not execute concurrently.
  explicit http_request_session(net::io_context& ioc,
                                boost::asio::ssl::context& ctx)
      : m_stream(net::make_strand(ioc), ctx)
  {
  }

  // Start the asynchronous operation
  void run(resolver_session& resolver_session,
           const std::string& target,
           int version)
  {
    if (!SSL_set_tlsext_host_name(m_stream.native_handle(),
                                  resolver_session.host().data()))
    {
      beast::error_code ec {static_cast<int>(::ERR_error()),
                            net::error::ssl_category()};

      return fail(ec, "ssl_set_hostname");
    }

    // Set up an HTTP GET request message
    m_req.version(version);
    m_req.method(http::verb::get);
    m_req.target(target);
    m_req.set(http::field::host, resolver_session.host());
    m_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    auto results = resolver_session.results();
    if (results) {
      on_resolve(*results);
    } else {
      resolver_session.add_callback(
          beast::bind_front_handler(&http_request_session::on_resolve, this));
    }
  }

  void on_resolve(const tcp::resolver::results_type& results)
  {
    beast::lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    beast::lowest_layer(m_stream).async_connect(
        results,
        beast::bind_front_handler(&http_request_session::on_connect, this));
  }

  void on_connect(beast::error_code ec,
                  const tcp::resolver::results_type::endpoint_type& /*unused*/)
  {
    if (ec) {
      return fail(ec, "connect");
    }

    // Set a timeout on the operation
    beast::lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    // Send the HTTP request to the remote host
    http::async_write(
        m_stream,
        m_req,
        beast::bind_front_handler(&http_request_session::on_write, this));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
      return fail(ec, "write");
    }

    http::async_read(
        m_stream,
        m_buffer,
        m_res,
        beast::bind_front_handler(&http_request_session::on_read, this));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
      return fail(ec, "read");
    }

    std::cout << m_res.body() << std::endl;

    m_stream.async_shutdown(
        beast::bind_front_handler(&http_request_session::on_shutdown, this));
  }

  void on_shutdown(beast::error_code ec)
  {
    if (ec == net::error::eof) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
    }
    if (ec) {
      return fail(ec, "shutdown");
    }

    // If we get here then the connection is closed gracefully
  }
};

//------------------------------------------------------------------------------

auto main(int argc, char** argv) -> int
{
  // Check command line arguments.
  if (argc != 4 && argc != 5) {
    std::cerr << "Usage: http-client-async <host> <port> <target> [<HTTP "
                 "version: 1.0 or 1.1(default)>]\n"
              << "Example:\n"
              << "    http-client-async www.example.com 80 /\n"
              << "    http-client-async www.example.com 80 / 1.0\n";
    return EXIT_FAILURE;
  }
  auto* const host = argv[1];
  auto* const port = argv[2];
  auto* const target = argv[3];
  int version = argc == 5 && (std::strcmp("1.0", argv[4]) == 0) ? 10 : 11;

  // The io_context is required for all I/O
  net::io_context ioc;

  // Launch the asynchronous operation
  resolver_session resolver(ioc);
  resolver.run(host, port);
  http_request_session request(ioc);
  request.run(resolver, target, version);

  // Run the I/O service. The call will return when
  // the get operation is complete.
  ioc.run();

  return EXIT_SUCCESS;
}