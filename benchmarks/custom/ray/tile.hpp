#pragma once

#include <vector>
struct tile
{
  int x;
  int y;
  int width;
  int height;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(x, y, width, height);
  }
};

inline auto quantize_image(int image_width,
                           int image_height,
                           int tile_width,
                           int tile_height) -> std::vector<tile>
{
  const int tiles_x = image_width % tile_width == 0
      ? image_width / tile_width
      : image_width / tile_width + 1;
  const int tiles_y = image_height % tile_height == 0
      ? image_height / tile_height
      : image_height / tile_height + 1;

  std::vector<tile> tiles;
  tiles.reserve(static_cast<long>(tiles_x) * tiles_y);
  for (int x = 0; x < tiles_x - 1; x++) {
    for (int y = 0; y < tiles_y - 1; y++) {
      tiles.push_back({
          .x = tile_width * x,
          .y = tile_height * y,
          .width = tile_width,
          .height = tile_height,
      });
    }
  }

  for (int x = 0; x < tiles_x - 1; x++) {
    int y = tiles_y - 1;
    tiles.push_back({.x = tile_width * x,
                     .y = tile_height * y,
                     .width = tile_width,
                     .height = image_height - tile_height * y});
  }
  for (int y = 0; y < tiles_y - 1; y++) {
    int x = tiles_x - 1;
    tiles.push_back({.x = tile_width * x,
                     .y = tile_height * y,
                     .width = image_width - tile_width * x,
                     .height = tile_height});
  }

  {
    int x = tiles_x - 1;
    int y = tiles_y - 1;

    tiles.push_back({.x = tile_width * x,
                     .y = tile_height * y,
                     .width = image_width - tile_width * x,
                     .height = image_height - tile_height * y});
  }
  return tiles;
}