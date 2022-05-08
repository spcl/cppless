#include <cstdlib>
#include <iostream>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/provider/aws/lambda.hpp>
#include <nghttp2/asio_http2_client.h>

auto main(int argc, char* argv[]) -> int
{
  std::string function_name =
      "08bc8655b2ed3089d721cbc0dea657c80c41cde71f8c69131d06b5d299ec8044";
  std::string qualifier = "$LATEST";
  std::string payload = R"({"value0":{"context":{"value0":-1},"args":{}}})";

  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();

  boost::system::error_code ec;
  boost::asio::io_service io_service;

  boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
  tls.set_default_verify_paths();
  nghttp2::asio_http2::client::configure_tls_context(ec, tls);

  nghttp2::asio_http2::client::session sess(
      io_service, tls, lambda_client.get_hostname(), "443");

  bool connected = false;
  sess.on_connect([&](const auto&) { connected = true; });
  sess.on_error([](const boost::system::error_code& ec)
                { std::cerr << "Error: " << ec.message() << std::endl; });

  while (!connected) {
    io_service.run_one();
  }

  cppless::aws::lambda::invocation_request req {
      function_name,
      qualifier,
      std::vector<unsigned char> {payload.begin(), payload.end()},
  };

  req.on_result(
      [](const std::vector<unsigned char>& data) {
        std::cout << std::string {data.begin(), data.end()};
      });

  const auto* sess_req = req.submit(sess, lambda_client, key);
  sess_req->on_close([&sess](uint32_t /*error_code*/) { sess.shutdown(); });

  io_service.run();
}