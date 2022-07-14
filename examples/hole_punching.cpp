#include <array>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/write.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>
#include <cppless/utils/tcpunch.hpp>

namespace asio = boost::asio;

auto exchange_number(std::string host_name, std::string connection_key) -> int
{
  asio::io_service ioc;

  cppless::tcpunch::client tcpunch(ioc);
  asio::ip::tcp::resolver resolver(ioc);

  long long seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::mt19937 rng(seed);
  std::uniform_int_distribution<> dist(0, 256);
  auto random_number = dist(rng);
  auto other_number = 0;

  int x = 0;
  int sum = 0;

  auto handle_update = [&](asio::ip::tcp::socket& socket)
  {
    x++;
    if (x >= 2) {
      sum = other_number + random_number;
      socket.close();
    }
  };

  auto handle_connect =
      [&](const boost::system::error_code& ec, asio::ip::tcp::socket& socket)
  {
    auto handle_send =
        [&](const boost::system::error_code& ec, unsigned int /*num_bytes*/)
    {
      if (ec.failed()) {
        std::cout << ec.what() << std::endl;
        socket.close();
        return;
      }
      handle_update(socket);
    };

    auto handle_rcv =
        [&](const boost::system::error_code& ec, unsigned int /*num_bytes*/)
    {
      if (ec.failed()) {
        std::cout << ec.what() << std::endl;
        socket.close();
        return;
      }
      handle_update(socket);
    };

    asio::async_write(socket,
                      asio::buffer(&random_number, sizeof(random_number)),
                      handle_send);
    asio::async_read(
        socket, asio::buffer(&other_number, sizeof(other_number)), handle_rcv);
  };

  auto handle_resolve =
      [&](const boost::system::error_code& ec,
          const asio::ip::tcp::resolver::results_type& endpoint)
  {
    if (ec.failed()) {
      std::cout << ec.what() << std::endl;
      return;
    }
    tcpunch.async_connect(endpoint, connection_key, handle_connect);
  };
  resolver.async_resolve(host_name, "10000", handle_resolve);

  ioc.run();
  return sum;
}

using dispatcher = cppless::aws_lambda_beast_dispatcher<>::from_env;
using task = cppless::task<dispatcher>;

auto main() -> int
{
  dispatcher aws;
  auto instance = aws.create_instance();

  cppless::shared_future<int> t0_result;
  cppless::shared_future<int> t1_result;

  std::string host_name =
      "ec2-18-185-121-125.eu-central-1.compute.amazonaws.com";
  task::sendable t0 = [&]() { return exchange_number(host_name, "t0"); };
}

// "ec2-18-185-121-125.eu-central-1.compute.amazonaws.com"