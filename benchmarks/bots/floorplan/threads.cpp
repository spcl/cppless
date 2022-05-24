#include <span>

#include "./threads.hpp"

#include "./common.hpp"

auto add_cell_threads(result_data& result,
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
        nn2 += add_cell(result, cells[id].next, footprint, board, cells);
        /* if area is greater than or equal to best area, prune search */
      } else {
      }
    }
  }
  return nn2;
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
  int number_of_tasks = add_cell_threads(
      result, 1, footprint, board, std::span<cell> {args.fp.cells});

  return {number_of_tasks, result};
}