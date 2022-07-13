#include <iostream>
#include <string>

#include <cppless/provider/aws/s3.hpp>

auto main(int argc, char* argv[]) -> int
{
  std::string bucket_name = "cppless-data-lmoeller";
  std::string object_key = "floorplan.15";

  cppless::aws::s3::client s3_client {bucket_name};
  auto key = s3_client.create_derived_key_from_env();

  boost::asio::io_context ioc;

  boost::system::error_code ec;
  boost::asio::ssl::context tls(boost::asio::ssl::context::tlsv12_client);
  tls.set_default_verify_paths();

  cppless::beast::resolver_session resolver(ioc);
  resolver.run(s3_client.hostname(), "443");

  cppless::aws::s3::beast_get_object_request req {object_key};

  req.on_result([](const std::string& data)
                { std::cout << data << std::endl; });

  req.submit(resolver, ioc, tls, s3_client, key, std::nullopt);

  ioc.run();
}