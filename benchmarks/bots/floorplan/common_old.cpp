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

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "./common.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* compute all possible locations for nw corner for cell */
static auto starts(int id,
                   int shape,
                   std::span<coord> nws,
                   std::span<cell> cells) -> int
{
  int n = 0;
  int top = 0;
  int bot = 0;
  int lhs = 0;
  int rhs = 0;

  /* size of cell */
  int rows = cells[id].alt[shape][0];
  int cols = cells[id].alt[shape][1];

  /* the cells to the left and above */
  int left = cells[id].left;
  int above = cells[id].above;

  /* if there is a vertical and horizontal dependence */
  if ((left >= 0) && (above >= 0)) {
    top = cells[above].bot + 1;
    lhs = cells[left].rhs + 1;
    bot = top + rows;
    rhs = lhs + cols;

    /* if footprint of cell touches the cells to the left and above */
    if ((top <= cells[left].bot) && (bot >= cells[left].top)
        && (lhs <= cells[above].rhs) && (rhs >= cells[above].lhs))
    {
      n = 1;
      nws[0][0] = top;
      nws[0][1] = lhs;
    } else {
      n = 0;
    }

    /* if there is only a horizontal dependence */
  } else if (left >= 0) {
    /* highest initial row is top of cell to the left - rows */
    top = std::max(cells[left].top - rows + 1, 0);
    /* lowest initial row is bottom of cell to the left */
    bot = std::min(cells[left].bot, rows);
    n = bot - top + 1;

    for (int i = 0; i < n; i++) {
      nws[i][0] = i + top;
      nws[i][1] = cells[left].rhs + 1;
    }

  } else {
    /* leftmost initial col is lhs of cell above - cols */
    lhs = std::max(cells[above].lhs - cols + 1, 0);
    /* rightmost initial col is rhs of cell above */
    rhs = std::min(cells[above].rhs, cols);
    n = rhs - lhs + 1;

    for (int i = 0; i < n; i++) {
      nws[i][0] = cells[above].bot + 1;
      nws[i][1] = i + lhs;
    }
  }

  return (n);
}

/* lay the cell down on the board in the rectangular space defined
   by the cells top, bottom, left, and right edges. If the cell can
   not be layed down, return 0; else 1.
*/
static auto lay_down(int id, board_array& board, std::span<cell> cells) -> bool
{
  int top = cells[id].top;
  int bot = cells[id].bot;
  int lhs = cells[id].lhs;
  int rhs = cells[id].rhs;

  for (int i = top; i <= bot; i++) {
    for (int j = lhs; j <= rhs; j++) {
      if (board[i][j] == 0) {
        board[i][j] = static_cast<char>(id);
      } else {
        return false;
      }
    }
  }

  return true;
}
static auto read_inputs(std::fstream& f) -> floorplan_data
{
  floorplan_data fd;

  unsigned long n = 0;

  f >> n;

  fd.cells.resize(n + 1);

  fd.cells[0].alt = std::vector<coord> {};
  fd.cells[0].top = 0;
  fd.cells[0].bot = 0;
  fd.cells[0].lhs = -1;
  fd.cells[0].rhs = -1;
  fd.cells[0].left = 0;
  fd.cells[0].above = 0;
  fd.cells[0].next = 0;

  for (unsigned int i = 1; i < n + 1; i++) {
    int cn = 0;
    f >> cn;
    fd.cells[i].alt.resize(cn);

    for (auto& j : fd.cells[i].alt) {
      f >> j[0];
      f >> j[1];
    }

    f >> fd.cells[i].left;
    f >> fd.cells[i].above;
    f >> fd.cells[i].next;
  }

  if (!f.eof()) {
    f >> fd.solution;
  }

  return fd;
}

void write_outputs(result_data& result)
{
  std::cout << "Minimum area = " << result.min_area << std::endl;

  for (int i = 0; i < result.min_footprint[0]; i++) {
    for (int j = 0; j < result.min_footprint[1]; j++) {
      if (result.best_board[i][j] == 0) {
        std::cout << " ";
      } else {
        std::cout << static_cast<char>('A' + result.best_board[i][j] - 1);
      }
    }
    std::cout << std::endl;
  }
}

auto add_cell(result_data& result,
              int id,
              coord prev_footprint,
              board_array& prev_board,
              std::span<cell> cells) -> int
{
  int nn = 0;
  int nn2 = 0;
  int area = 0;

  board_array board;
  coord footprint;
  std::array<coord, dmax> nws {};

  nn2 = 0;
  /* for each possible shape */
  for (int i = 0; i < cells[id].alt.size(); i++) {
    /* compute all possible locations for nw corner */
    nn = starts(id, i, nws, cells);
    nn2 += nn;
    /* for all possible locations */
    for (int j = 0; j < nn; j++) {
      /* extent of shape */
      cells[id].top = nws[j][0];
      cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
      cells[id].lhs = nws[j][1];
      cells[id].rhs = cells[id].lhs + cells[id].alt[i][1] - 1;

      board = prev_board;

      /* if the cell cannot be layed down, prune search */
      if (!lay_down(id, board, cells)) {
        continue;
      }

      /* calculate new footprint of board and area of footprint */
      footprint[0] = std::max(prev_footprint[0], cells[id].bot + 1);
      footprint[1] = std::max(prev_footprint[1], cells[id].rhs + 1);
      area = footprint[0] * footprint[1];

      /* if last cell */
      if (cells[id].next == 0) {
        /* if area is minimum, update global values */
        if (area < result.min_area) {
          result.min_area = area;
          result.min_footprint = footprint;
          result.best_board = board;
          std::cout << "N  " << area << std::endl;
        }

        /* if area is less than best area */
      } else if (area < result.min_area) {
        nn2 += add_cell(result, cells[id].next, footprint, board, cells);
        /* if area is greater than or equal to best area, prune search */
      } else {
      }
    }
  }
  return nn2;
}

auto floorplan_init(const std::string& filename) -> floorplan_data
{
  std::fstream input_file(filename, std::fstream::in);
  /* read input file and initialize global minimum area */
  auto fp = read_inputs(input_file);
  return fp;
}
