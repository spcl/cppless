#include <iostream>

#include <nghttp2/asio_http2_client.h>

using boost::asio::ip::tcp;

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::client;

int main(int argc, char* argv[])
{
  boost::system::error_code ec;
  boost::asio::io_service io_service;

  // connect to localhost:3000
  boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
  tls.set_default_verify_paths();
  configure_tls_context(ec, tls);
  session sess(io_service, tls, "google.com", "443");

  sess.on_connect(
      [&sess](tcp::resolver::iterator endpoint_it)
      {
        boost::system::error_code ec;
        std::cout << "Connected" << std::endl;

        auto req = sess.submit(ec, "GET", "https://google.com:443/");

        req->on_response(
            [](const response& res)
            {
              // print status code and response header fields.
              std::cout << "HTTP/2 " << res.status_code() << std::endl;
              for (auto& kv : res.header()) {
                std::cout << kv.first << ": " << kv.second.value << "\n";
              }
              std::cout << std::endl;

              res.on_data(
                  [](const uint8_t* data, std::size_t len)
                  {
                    std::cout.write(reinterpret_cast<const char*>(data), len);
                    std::cout << std::endl;
                  });
            });

        req->on_close(
            [&sess](uint32_t error_code)
            {
              std::cout << "Connection closed with error code: " << error_code
                        << std::endl;
              // shutdown session after first request was done.
              sess.shutdown();
            });
      });

  sess.on_error([](const boost::system::error_code& ec)
                { std::cout << "error: " << ec.message() << std::endl; });

  do {
    io_service.run_one();
    std::cout << "." << std::flush;
  } while (!io_service.stopped());
}