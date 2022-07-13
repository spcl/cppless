#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/provider/aws/lambda.hpp>
#include <cppless/utils/beast/http_request_session.hpp>
#include <cppless/utils/tracing.hpp>
#include <nlohmann/json.hpp>

auto main(int argc, char* argv[]) -> int
{
  std::string function_name = "echo";
  std::string qualifier = "$LATEST";
  std::string payload = R"({"test":42})";

  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();

  boost::asio::io_context ioc;

  boost::system::error_code ec;
  boost::asio::ssl::context tls(boost::asio::ssl::context::tlsv12_client);
  tls.set_default_verify_paths();

  cppless::beast::resolver_session resolver(ioc);
  resolver.run(lambda_client.hostname(), "443");

  cppless::aws::lambda::beast_invocation_request req {
      function_name,
      qualifier,
      payload,
  };

  auto cb = [](const cppless::aws::lambda::invocation_response& res)
  {
    std::cout << "request_id: " << res.request_id << std::endl;
    std::cout << res.body << std::endl;
  };
  req.on_result(cb);

  cppless::tracing_span_container span_container;
  auto root = span_container.create_root("lambda_invocation");

  req.submit(resolver, ioc, tls, lambda_client, key, root);

  ioc.run();

  nlohmann::json j = span_container;
  std::cout << j.dump(2) << std::endl;
}