#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

// YYYYMMDD'T'HHMMSS'Z'
inline auto format_aws_date(const std::chrono::system_clock::time_point& tp)
    -> std::string
{
  std::time_t time = std::chrono::system_clock::to_time_t(tp);
  tm gm_tm = *gmtime(&time);  // NOLINT

  std::stringstream ss;
  const auto tm_start_year = 1900;
  ss << std::setw(4) << std::setfill('0') << gm_tm.tm_year + tm_start_year;
  ss << std::setw(2) << std::setfill('0') << gm_tm.tm_mon + 1;
  ss << std::setw(2) << std::setfill('0') << gm_tm.tm_mday;
  ss << 'T';
  ss << std::setw(2) << std::setfill('0') << gm_tm.tm_hour;
  ss << std::setw(2) << std::setfill('0') << gm_tm.tm_min;
  ss << std::setw(2) << std::setfill('0') << gm_tm.tm_sec;
  ss << 'Z';

  return ss.str();
}