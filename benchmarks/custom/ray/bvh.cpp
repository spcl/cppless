#include <memory>
#include <random>
#include <vector>

#include "bvh.hpp"

#include "ray.hpp"
#include "vec.hpp"

auto bvh_node::bounding_box(aabb& output_box) const -> bool
{
  output_box = box;
  return true;
}

auto bvh_node::hit(const ray& r,
                   double t_min,
                   double t_max,
                   hit_record& rec) const -> bool
{
  if (!box.hit(r, t_min, t_max)) {
    return false;
  }

  bool hit_left;
  if (std::holds_alternative<std::shared_ptr<bvh_node>>(left)) {
    hit_left = std::get<std::shared_ptr<bvh_node>>(left)->bvh_node::hit(
        r, t_min, t_max, rec);
  } else {
    hit_left =
        std::get<std::shared_ptr<hittable>>(left)->hit(r, t_min, t_max, rec);
  }

  bool hit_right;
  if (std::holds_alternative<std::shared_ptr<bvh_node>>(right)) {
    hit_right = std::get<std::shared_ptr<bvh_node>>(right)->bvh_node::hit(
        r, t_min, hit_left ? rec.t : t_max, rec);
  } else {
    hit_right = std::get<std::shared_ptr<hittable>>(right)->hit(
        r, t_min, hit_left ? rec.t : t_max, rec);
  }

  return hit_left || hit_right;
}

inline auto box_compare(const std::shared_ptr<hittable>& a,
                        const std::shared_ptr<hittable>& b,
                        int axis) -> bool
{
  aabb box_a;
  aabb box_b;

  if (!a->bounding_box(box_a) || !b->bounding_box(box_b)) {
    std::cerr << "No bounding box in bvh_node constructor.\n";
  }

  return box_a.min()[axis] < box_b.min()[axis];
}

auto box_x_compare(const std::shared_ptr<hittable> a,
                   const std::shared_ptr<hittable> b) -> bool
{
  return box_compare(a, b, 0);
}

auto box_y_compare(const std::shared_ptr<hittable> a,
                   const std::shared_ptr<hittable> b) -> bool
{
  return box_compare(a, b, 1);
}

auto box_z_compare(const std::shared_ptr<hittable> a,
                   const std::shared_ptr<hittable> b) -> bool
{
  return box_compare(a, b, 2);
}

bvh_node::bvh_node(const std::vector<std::shared_ptr<hittable>>& src_objects,
                   size_t start,
                   size_t end,
                   std::mt19937& prng)
{
  std::uniform_int_distribution axis_distribution(0, 2);

  auto objects =
      src_objects;  // Create a modifiable array of the source scene objects

  int axis = axis_distribution(prng);

  auto comparator = (axis == 0) ? box_x_compare
      : (axis == 1)             ? box_y_compare
                                : box_z_compare;

  size_t object_span = end - start;

  if (object_span == 1) {
    left = objects[start];
    right = objects[start];
  } else if (object_span == 2) {
    if (comparator(objects[start], objects[start + 1])) {
      left = objects[start];
      right = objects[start + 1];
    } else {
      left = objects[start + 1];
      right = objects[start];
    }
  } else {
    std::sort(objects.begin() + start, objects.begin() + end, comparator);

    auto mid = start + object_span / 2;
    left = make_shared<bvh_node>(objects, start, mid, prng);
    right = make_shared<bvh_node>(objects, mid, end, prng);
  }

  aabb box_left;
  bool has_box_left;

  if (std::holds_alternative<std::shared_ptr<bvh_node>>(left)) {
    has_box_left =
        std::get<std::shared_ptr<bvh_node>>(left)->bvh_node::bounding_box(
            box_left);
  } else {
    has_box_left =
        std::get<std::shared_ptr<hittable>>(left)->bounding_box(box_left);
  }

  aabb box_right;
  bool has_box_right;

  if (std::holds_alternative<std::shared_ptr<bvh_node>>(right)) {
    has_box_right =
        std::get<std::shared_ptr<bvh_node>>(right)->bvh_node::bounding_box(
            box_right);
  } else {
    has_box_right =
        std::get<std::shared_ptr<hittable>>(right)->bounding_box(box_right);
  }

  if (!has_box_left || !has_box_right) {
    std::cerr << "No bounding box in bvh_node constructor.\n";
  }

  box = surrounding_box(box_left, box_right);
}