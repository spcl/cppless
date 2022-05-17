#pragma once

#include <chrono>

#include <nlohmann/json.hpp>

namespace std::chrono
{
using json = nlohmann::json;

inline void to_json(json& j,
                    const std::chrono::high_resolution_clock::time_point& p)
{
  j = std::chrono::duration_cast<std::chrono::nanoseconds>(p.time_since_epoch())
          .count();
}
}  // namespace std::chrono