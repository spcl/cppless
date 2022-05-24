/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de
 * Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify */
/*  it under the terms of the GNU General Public License as published by */
/*  the Free Software Foundation; either version 2 of the License, or */
/*  (at your option) any later version. */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful, */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/*  GNU General Public License for more details. */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License */
/*  along with this program; if not, write to the Free Software */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA            */
/**********************************************************************************************/

/* Original code from the Application Kernel Matrix by Cray */

#pragma once

#include <algorithm>
#include <array>
#include <fstream>
#include <span>
#include <tuple>
#include <vector>

constexpr auto rows = 64;
constexpr auto cols = 64;
constexpr auto dmax = 64;

using coord = std::array<int, 2>;
using board_array = std::array<std::array<char, rows>, cols>;

constexpr auto cell_alignment = 64;
class cell
{
public:
  std::vector<coord> alt;
  int top;
  int bot;
  int lhs;
  int rhs;
  int left;
  int above;
  int next;
} __attribute__((aligned(cell_alignment)));

class floorplan_data
{
public:
  std::vector<cell> cells;
  int solution;
};

class result_data
{
public:
  int min_area;
  board_array best_board;
  coord min_footprint;
};

auto floorplan_init(const std::string& filename) -> floorplan_data;
auto add_cell(result_data& result,
              int id,
              coord prev_footprint,
              board_array& prev_board,
              std::span<cell> cells) -> int;
auto starts(int id, int shape, std::span<coord> nws, std::span<cell> cells)
    -> int;
auto lay_down(int id, board_array& board, std::span<cell> cells) -> bool;
void write_outputs(result_data& result);