#pragma once

#include <memory>

#include "aabb.hpp"
#include "ray.hpp"
#include "vec.hpp"

class material;

struct hit_record
{
  point3 p;
  vec3 normal;
  double t = 0;
  std::shared_ptr<material> mat_ptr;
  bool front_face;

  inline void set_face_normal(const ray& r, const vec3& outward_normal)
  {
    front_face = dot(r.direction(), outward_normal) < 0;
    normal = front_face ? outward_normal : -outward_normal;
  }

} __attribute__((aligned(128)));

class hittable
{
public:
  virtual bool hit(const ray& r,
                   double t_min,
                   double t_max,
                   hit_record& rec) const = 0;
  virtual auto bounding_box(aabb& output_box) const -> bool = 0;
  virtual ~hittable() = default;

  template<class Archive>
  void serialize(Archive& /*ar*/)
  {
  }
};
