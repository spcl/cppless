#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <cppless/utils/tracing.hpp>

namespace cppless::beast
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

//------------------------------------------------------------------------------

// Report a failure
inline void fail(beast::error_code ec, const std::string& what)
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

class http_request_tracing_data
{
public:
  explicit http_request_tracing_data(tracing_span_ref parent)
      : m_parent(parent)
      , m_resolve_span(parent.create_child("resolve").start())
      , m_connect_span(parent.create_child("connect"))
      , m_handshake_span(parent.create_child("handshake"))
      , m_write_span(parent.create_child("write"))
      , m_read_span(parent.create_child("read"))
      , m_shutdown_span(parent.create_child("shutdown"))
  {
    parent.start();
  }

  void on_resolve()
  {
    m_resolve_span.end();
    m_connect_span.start();
  }

  void on_connect()
  {
    m_connect_span.end();
    m_handshake_span.start();
  }

  void on_handshake()
  {
    m_handshake_span.end();
    m_write_span.start();
  }

  void on_write()
  {
    m_write_span.end();
    m_read_span.start();
  }

  void on_read()
  {
    m_read_span.end();
    m_shutdown_span.start();
  }

  void on_shutdown()
  {
    m_shutdown_span.end();
    m_parent.end();
  }

private:
  tracing_span_ref m_parent;

  tracing_span_ref m_resolve_span;
  tracing_span_ref m_connect_span;
  tracing_span_ref m_handshake_span;
  tracing_span_ref m_write_span;
  tracing_span_ref m_read_span;
  tracing_span_ref m_shutdown_span;
};

// Performs an HTTP GET and prints the response
class http_request_session
    : public std::enable_shared_from_this<http_request_session>
{
  beast::ssl_stream<beast::tcp_stream> m_stream;
  beast::flat_buffer m_buffer;
  http::request<http::string_body> m_req;
  http::response<http::string_body> m_res;

  std::function<void(http::response<http::string_body>&)> m_callback;

  std::optional<http_request_tracing_data> m_tracing_data;

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
           std::optional<tracing_span_ref> span = std::nullopt)
  {
    if (span) {
      m_tracing_data.emplace(*span);
    }

    if (!SSL_set_tlsext_host_name(m_stream.native_handle(),
                                  resolver_session.host().data()))
    {
      beast::error_code ec {static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category()};

      return fail(ec, "ssl_set_hostname");
    }

    // Set up an HTTP GET request message
    auto results = resolver_session.results();
    if (results) {
      on_resolve(*results);
    } else {
      resolver_session.add_callback(beast::bind_front_handler(
          &http_request_session::on_resolve, shared_from_this()));
    }
  }

  void on_resolve(const tcp::resolver::results_type& results)
  {
    if (m_tracing_data) {
      m_tracing_data->on_resolve();
    }

    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    beast::get_lowest_layer(m_stream).async_connect(
        results,
        beast::bind_front_handler(&http_request_session::on_connect,
                                  shared_from_this()));
  }

  void on_connect(beast::error_code ec,
                  const tcp::resolver::results_type::endpoint_type& /*unused*/)
  {
    if (m_tracing_data) {
      m_tracing_data->on_connect();
    }

    if (ec) {
      return fail(ec, "connect");
    }

    m_stream.async_handshake(
        boost::asio::ssl::stream_base::client,
        beast::bind_front_handler(&http_request_session::on_handshake,
                                  shared_from_this()));
  }

  void on_handshake(beast::error_code ec)
  {
    if (m_tracing_data) {
      m_tracing_data->on_handshake();
    }

    if (ec) {
      return fail(ec, "handshake");
    }

    // Set a timeout on the operation
    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

    // Send the HTTP request to the remote host
    http::async_write(m_stream,
                      m_req,
                      beast::bind_front_handler(&http_request_session::on_write,
                                                shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (m_tracing_data) {
      m_tracing_data->on_write();
    }

    if (ec) {
      return fail(ec, "write");
    }

    http::async_read(m_stream,
                     m_buffer,
                     m_res,
                     beast::bind_front_handler(&http_request_session::on_read,
                                               shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (m_tracing_data) {
      m_tracing_data->on_read();
    }

    if (ec) {
      return fail(ec, "read");
    }

    m_callback(m_res);

    m_stream.async_shutdown(beast::bind_front_handler(
        &http_request_session::on_shutdown, shared_from_this()));
  }

  void on_shutdown(beast::error_code ec)  // NOLINT
  {
    if (m_tracing_data) {
      m_tracing_data->on_shutdown();
    }

    if (ec == net::error::eof) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
    } else if (ec == boost::asio::ssl::error::stream_truncated) {
      // Rationale:
      // Some servers fail to shutdown the TLS session correctly.
      ec = {};
    }

    if (ec) {
      return fail(ec, "shutdown");
    }

    // If we get here then the connection is closed gracefully
  }

  void set_callback(std::function<void(http::response<http::string_body>&)> cb)
  {
    m_callback = std::move(cb);
  }

  auto request() -> http::request<http::string_body>&
  {
    return m_req;
  }
};
}  // namespace cppless::beast