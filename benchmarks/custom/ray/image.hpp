#pragma once

#include <iostream>
#include <vector>

#include "color.hpp"
#include "vec.hpp"

class image
{
public:
  image() = default;
  image(unsigned int width, unsigned int height, int samples_per_pixel)
      : m_width(width)
      , m_height(height)
      , m_samples_per_pixel(samples_per_pixel)
      , m_data(static_cast<unsigned long>(width) * height)
  {
  }

  auto operator()(unsigned int x, unsigned int y) -> color&
  {
    return m_data[y * m_width + x];
  }

  auto operator()(unsigned int x, unsigned int y) const -> const color&
  {
    return m_data[y * m_width + x];
  }

  auto insert(unsigned int offset_x, unsigned int offset_y, const image& other)
  {
    unsigned int x_max = std::min(m_width, offset_x + other.width());
    unsigned int y_max = std::min(m_height, offset_y + other.height());
    for (unsigned int x = offset_x; x < x_max; x++) {
      for (unsigned int y = offset_y; y < y_max; y++) {
        (*this)(x, y) = other(x - offset_x, y - offset_y);
      }
    }
  }

  [[nodiscard]] auto width() const -> unsigned int
  {
    return m_width;
  }

  [[nodiscard]] auto height() const -> unsigned int
  {
    return m_height;
  }

  [[nodiscard]] auto samples_per_pixel() const -> int
  {
    return m_samples_per_pixel;
  }

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(m_width, m_height, m_samples_per_pixel, m_data);
  }

private:
  unsigned int m_width;
  unsigned int m_height;
  int m_samples_per_pixel;
  std::vector<color> m_data;
};

inline auto operator<<(std::ostream& os, const image& img) -> std::ostream&
{
  os << "P3\n" << img.width() << " " << img.height() << "\n255\n";
  for (int y = img.height() - 1; y >= 0; y--) {
    for (int x = 0; x < img.width(); x++) {
      write_color(os, img(x, y), img.samples_per_pixel());
    }
  }
  return os;
}