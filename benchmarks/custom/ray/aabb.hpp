#pragma once

#include "ray.hpp"
#include "vec.hpp"

class aabb
{
public:
  aabb() = default;
  aabb(const point3& a, const point3& b)
      : m_minimum(a)
      , m_maximum(b)
  {
  }

  [[nodiscard]] auto min() const -> point3
  {
    return m_minimum;
  }
  [[nodiscard]] auto max() const -> point3
  {
    return m_maximum;
  }

  [[nodiscard]] auto hit(const ray& r, double t_min, double t_max) const -> bool
  {
    const auto& direction = r.direction().base();
    const auto& origin = r.origin().base();
    const auto& aabb_min = min().base();
    const auto& aabb_max = max().base();

    auto inv_d = 1.0F / direction;
    auto t0 = (aabb_min - origin) * inv_d;
    auto t1 = (aabb_max - origin) * inv_d;

    auto t_smaller = __builtin_elementwise_min(t0, t1);
    auto t_bigger = __builtin_elementwise_max(t0, t1);

    t_min = std::max(t_min, __builtin_reduce_max(t_smaller));
    t_max = std::min(t_max, __builtin_reduce_min(t_bigger));

    return (t_min < t_max);
  }

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(m_minimum, m_maximum);
  }

private:
  point3 m_minimum {};
  point3 m_maximum {};
};

auto surrounding_box(aabb box0, aabb box1) -> aabb;