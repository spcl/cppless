#pragma once

#include "./common.hpp"

class dispatcher_args
{
public:
  floorplan_data fp;
};

auto floorplan(dispatcher_args args) -> std::tuple<int, result_data>;