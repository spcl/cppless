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
#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "./dispatcher.hpp"

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/common.hpp>

#include "./common.hpp"

template<class Dispatcher>
auto add_cell_dispatcher(typename Dispatcher::instance& instance,
                         std::vector<std::unique_ptr<result_data>>& futures,
                         int cutoff,
                         result_data& result,
                         int id,
                         coord prev_footprint,
                         board_array& prev_board,
                         std::span<cell> cells) -> void
{
  int nn = 0;
  int area = 0;

  board_array board;
  coord footprint;
  std::array<coord, dmax> nws {};

  /* for each possible shape */
  for (int i = 0; i < cells[id].alt.size(); i++) {
    /* compute all possible locations for nw corner */
    nn = starts(id, i, nws, cells);
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
          auto task =
              [min_area = result.min_area, footprint, parent_board = board, id](
                  std::vector<cell> cell_vector)
          {
            board_array board = parent_board;
            std::span<cell> cells {cell_vector};

            result_data result {};
            result.min_area = min_area;

            add_cell(result, cells[id].next, footprint, board, cells);
            return result;
          };
          auto& future = *futures.emplace_back(std::make_unique<result_data>());
          cppless::dispatch(
              instance, task, future, {{cells.begin(), cells.end()}});
          // future = task({cells.begin(), cells.end()});
        } else {
          add_cell_dispatcher<Dispatcher>(instance,
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
        // No pruning here
      }
    }
  }
}

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;

auto floorplan(dispatcher_args args) -> std::tuple<int, result_data>
{
  dispatcher aws;
  dispatcher::instance instance = aws.create_instance();

  std::vector<std::unique_ptr<result_data>> futures;

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

  add_cell_dispatcher<dispatcher>(instance,
                                  futures,
                                  args.cutoff,
                                  result,
                                  1,
                                  footprint,
                                  board,
                                  std::span<cell> {args.fp.cells});
  cppless::wait(instance, futures.size());
  for (auto& future : futures) {
    result = combine(result, *future);
  }

  return {futures.size(), result};
}