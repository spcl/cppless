#include "hittable.hpp"

#include "hittable_list.hpp"
#include "ray.hpp"

auto hittable_list::hit(const ray& r,
                        double t_min,
                        double t_max,
                        hit_record& rec) const -> bool
{
  hit_record temp_rec;
  bool hit_anything = false;
  auto closest_so_far = t_max;

  for (const auto& object : m_objects) {
    if (object->hit(r, t_min, closest_so_far, temp_rec)) {
      hit_anything = true;
      closest_so_far = temp_rec.t;
      rec = temp_rec;
    }
  }

  return hit_anything;
}

auto hittable_list::bounding_box(aabb& output_box) const -> bool
{
  if (m_objects.empty()) {
    return false;
  }

  aabb temp_box;
  bool first_box = true;

  for (const auto& object : m_objects) {
    if (!object->bounding_box(temp_box)) {
      return false;
    }
    output_box = first_box ? temp_box : surrounding_box(output_box, temp_box);
    first_box = false;
  }

  return true;
}