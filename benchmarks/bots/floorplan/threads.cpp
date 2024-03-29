/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

#include <future>
#include <span>
#include <thread>
#include <vector>

#include "./threads.hpp"

#include "./common.hpp"

auto add_cell_threads(std::vector<std::thread>& threads,
                      std::vector<std::future<result_data>>& futures,
                      int cutoff,
                      result_data& result,
                      int id,
                      coord prev_footprint,
                      board_array& prev_board,
                      std::span<cell> cells) -> void
{
  int nn = 0;
  int nn2 = 0;
  int area = 0;

  board_array board;
  coord footprint;
  std::array<coord, dmax> nws {};

  /* for each possible shape */
  for (int i = 0; i < cells[id].alt.size(); i++) {
    /* compute all possible locations for nw corner */
    nn = starts(id, i, nws, cells);
    nn2 += nn;
    /* for all possible locations */
    std::span<coord> possible_nws {nws.data(), static_cast<std::size_t>(nn)};
    for (auto nw : possible_nws) {
      /* extent of shape */
      cells[id].top = nw[0];
      cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
      cells[id].lhs = nw[1];
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
        }

        /* if area is less than best area */
      } else if (area < result.min_area) {
        if (cutoff == 0) {
          std::vector<cell> cell_vector {cells.begin(), cells.end()};
          std::packaged_task<result_data()> task(
              [min_area = result.min_area,
               cell_vector,
               footprint,
               parent_board = board,
               id]() mutable
              {
                board_array board = parent_board;
                std::span<cell> cells {cell_vector};

                result_data result {};
                result.min_area = min_area;

                add_cell(result, cells[id].next, footprint, board, cells);

                return result;
              });
          futures.push_back(task.get_future());
          threads.emplace_back(std::move(task));
        } else {
          add_cell_threads(threads,
                           futures,
                           cutoff - 1,
                           result,
                           cells[id].next,
                           footprint,
                           board,
                           cells);
        }
        /* if area is greater than or equal to best area, prune search */
      } else {
      }
    }
  }
}

auto floorplan(threads_args args) -> std::tuple<int, result_data>
{
  coord footprint;
  /* footprint of initial board is zero */
  footprint[0] = 0;
  footprint[1] = 0;
  board_array board {};
  for (int i = 0; i < cols; i++) {
    for (int j = 0; j < rows; j++) {
      board[i][j] = 0;
    }
  }
  result_data result {};
  result.min_area = rows * cols;

  std::vector<std::thread> threads;
  std::vector<std::future<result_data>> futures;

  add_cell_threads(threads,
                   futures,
                   args.cutoff,
                   result,
                   1,
                   footprint,
                   board,
                   std::span<cell> {args.fp.cells});

  for (auto& thread : threads) {
    thread.join();
  }
  for (auto& future : futures) {
    result = combine(result, future.get());
  }

  return {static_cast<int>(0), result};
}