#include <span>

#include "./serial.hpp"

#include "./common.hpp"

auto floorplan(serial_args args) -> std::tuple<int, result_data>
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
  int number_of_tasks =
      add_cell(result, 1, footprint, board, std::span<cell> {args.fp.cells});

  return {number_of_tasks, result};
}