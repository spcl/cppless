#include "aabb.hpp"

auto surrounding_box(aabb box0, aabb box1) -> aabb
{
  point3 small(std::min(box0.min().x(), box1.min().x()),
               std::min(box0.min().y(), box1.min().y()),
               std::min(box0.min().z(), box1.min().z()));

  point3 big(std::max(box0.max().x(), box1.max().x()),
             std::max(box0.max().y(), box1.max().y()),
             std::max(box0.max().z(), box1.max().z()));

  return {small, big};
}
