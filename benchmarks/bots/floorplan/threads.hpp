#pragma once

#include "./common.hpp"

class threads_args
{
public:
  floorplan_data fp;
};

auto floorplan(threads_args args) -> std::tuple<int, result_data>;