#pragma once

#include "vec.hpp"

class ray
{
public:
  ray() = default;
  ray(point3 origin, vec3 direction)
      : m_orig(origin)
      , m_dir(direction)
  {
  }

  [[nodiscard]] inline auto origin() const -> point3
  {
    return m_orig;
  }

  [[nodiscard]] inline auto direction() const -> vec3
  {
    return m_dir;
  }

  [[nodiscard]] inline auto at(double t) const -> point3
  {
    return m_orig + t * m_dir;
  }

private:
  point3 m_orig;
  vec3 m_dir;
};