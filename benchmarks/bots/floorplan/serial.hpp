#pragma once

#include "./common.hpp"

class serial_args
{
public:
  floorplan_data fp;
};

auto floorplan(serial_args args) -> std::tuple<int, result_data>;