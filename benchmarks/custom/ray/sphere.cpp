#include "sphere.hpp"

#include "hittable.hpp"
#include "vec.hpp"

auto sphere::hit(const ray& r,
                 double t_min,
                 double t_max,
                 hit_record& rec) const -> bool
{
  vec3 oc = r.origin() - m_center;
  auto a = r.direction().length_squared();
  auto half_b = dot(oc, r.direction());
  auto c = oc.length_squared() - m_radius * m_radius;

  auto discriminant = half_b * half_b - a * c;
  if (discriminant < 0) {
    return false;
  }
  auto sqrtd = std::sqrt(discriminant);

  // Find the nearest root that lies in the acceptable range.
  auto root = (-half_b - sqrtd) / a;
  if (root < t_min || t_max < root) {
    root = (-half_b + sqrtd) / a;
    if (root < t_min || t_max < root) {
      return false;
    }
  }

  rec.t = root;
  rec.p = r.at(rec.t);
  vec3 outward_normal = (rec.p - m_center) / m_radius;
  rec.set_face_normal(r, outward_normal);
  rec.mat_ptr = m_material;

  return true;
}

auto sphere::bounding_box(aabb& output_box) const -> bool
{
  output_box = aabb(m_center - vec3(m_radius, m_radius, m_radius),
                    m_center + vec3(m_radius, m_radius, m_radius));
  return true;
}