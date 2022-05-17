#pragma once

#include <atomic>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

#include <argparse/argparse.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

#include "./common.hpp"

using dispatcher = cppless::dispatcher::aws_lambda_dispatcher;

class dispatcher_args
{
public:
  unsigned int size;
  unsigned int prefix_length;
};

inline auto nqueens(dispatcher_args args) -> unsigned int
{
  auto size = args.size;
  auto prefix_length = args.prefix_length;

  cppless::aws::lambda::client lambda_client;
  auto key = lambda_client.create_derived_key_from_env();
  dispatcher aws {"", lambda_client, key};
  dispatcher::instance instance = aws.create_instance();

  auto prefixes = std::vector<unsigned char>();
  prefixes.reserve(pow(size, prefix_length));
  auto scratchpad = std::vector<unsigned char>(size);

  nqueens_prefixes(0,
                   prefix_length,
                   0,
                   size,
                   std::span<unsigned char> {scratchpad},
                   prefixes);

  std::vector<cppless::shared_future<unsigned int>> futures;

  for (unsigned int i = 0; i < prefixes.size(); i += prefix_length) {
    std::vector<unsigned int> prefix(prefixes.begin() + i,
                                     prefixes.begin() + i + prefix_length);
    dispatcher::task::sendable task = [prefix, size]
    {
      auto scratchpad = std::vector<unsigned char>(size);
      std::copy(prefix.begin(), prefix.end(), scratchpad.begin());
      return nqueens_serial(prefix.size(), scratchpad);
    };
    instance.dispatch(task, futures.emplace_back(), {});
  }
  for ([[maybe_unused]] auto& f : futures) {
    instance.wait_one();
  }
  unsigned int res = 0;
  for (auto& f : futures) {
    res += f.get_value();
  }

  return res;
}