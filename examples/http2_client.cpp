#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <variant>
#include <vector>

#include <argparse/argparse.hpp>
#include <cppless/provider/aws/lambda.hpp>
#include <nghttp2/asio_http2_client.h>
#include <nlohmann/json.hpp>

auto main(int argc, char* argv[]) -> int
{
  std::string function_name = "helloLambdaWorld";
  std::string qualifier = "$LATEST";

  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();

  boost::system::error_code ec;
  boost::asio::io_service io_service;

  boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
  tls.set_default_verify_paths();
  nghttp2::asio_http2::client::configure_tls_context(ec, tls);

  int num_sessions = 16;
  std::vector<nghttp2::asio_http2::client::session> sessions;
  for (int i = 0; i < num_sessions; ++i) {
    sessions.emplace_back(io_service, tls, lambda_client.hostname(), "443");
  }

  int connected = 0;
  bool error = false;
  for (auto& session : sessions) {
    session.on_connect([&](const auto&) { connected++; });
    session.on_error(
        [&](const boost::system::error_code& ec)
        {
          error = true;
          std::cerr << "Error: " << ec.message() << std::endl;
        });
  }

  while (connected < num_sessions && !error) {
    io_service.run_one();
  }

  int n = std::stoi(argv[1]);
  int p = std::stoi(argv[2]);

  std::vector<cppless::aws::lambda::nghttp2_invocation_request> reqs;
  reqs.reserve(n);
  for (int i = 0; i < n; i++) {
    // Construct payload using nlohmann::json.
    nlohmann::json payload;
    payload["key1"] = "value1";
    payload["key2"] = "value2";
    payload["key3"] = "value3";

    reqs.emplace_back(function_name, qualifier, payload.dump());
  };

  int started = 0;
  int completed = 0;

  std::vector<int> resend_queue;

  auto submit_req =
      [&, n](cppless::aws::lambda::nghttp2_invocation_request& req, int id)
  {
    auto& session = sessions[id % sessions.size()];
    const auto* session_req =
        req.submit(session, lambda_client, key, std::nullopt);

    session_req->on_close(
        [&, n](uint32_t /*error_code*/)
        {
          if (completed == n) {
            for (auto& session : sessions) {
              session.shutdown();
            }
          }
        });
  };
  std::function<void()> schedule_request = [&, n]()
  {
    int id = started++;

    auto cb = [&, n](const cppless::aws::lambda::invocation_response& res)
    {
      if (res.body != "\"value1\"") {
        throw std::runtime_error(
            "Unexpected response, expected \"value1\", "
            "got: "
            + res.body);
      }

      completed++;

      if (!resend_queue.empty()) {
        int resend_id = resend_queue.back();
        submit_req(reqs[resend_id], resend_id);
        resend_queue.pop_back();
      } else if (started < n) {
        schedule_request();
      }
    };

    auto err_cb = [&, id](const cppless::aws::lambda::invocation_error& err)
    {
      if (std::holds_alternative<
              cppless::aws::lambda::invocation_error_too_many_requests>(err))
      {
        if (started > completed) {
          resend_queue.push_back(id);
        } else {
          submit_req(reqs[id], id);
        }
      } else {
        std::cerr << "Error." << std::endl;
      }
    };

    auto& req = reqs[id];
    req.on_result(cb);
    req.on_error(err_cb);

    submit_req(req, id);
  };

  for (int j = 0; j < p; j++) {
    schedule_request();
  }

  io_service.run();

  std::cout << "Started: " << started << std::endl;
  std::cout << "Completed: " << completed << std::endl;
}
