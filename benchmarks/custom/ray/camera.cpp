#include <random>

#include "camera.hpp"

#include "common.hpp"
#include "ray.hpp"
#include "vec.hpp"

camera::camera(point3 lookfrom,
               point3 lookat,
               vec3 vup,
               double vfov,  // vertical field-of-view in degrees
               double aspect_ratio,
               double aperture,
               double focus_dist)
    : m_origin(lookfrom)
    , m_lens_radius(aperture / 2)
{
  auto theta = degrees_to_radians(vfov);
  auto h = tan(theta / 2);
  auto viewport_height = 2.0 * h;
  auto viewport_width = aspect_ratio * viewport_height;

  m_w = unit_vector(lookfrom - lookat);
  m_u = unit_vector(cross(vup, m_w));
  m_v = cross(m_w, m_u);

  m_horizontal = focus_dist * viewport_width * m_u;
  m_vertical = focus_dist * viewport_height * m_v;
  m_lower_left_corner =
      m_origin - m_horizontal / 2 - m_vertical / 2 - focus_dist * m_w;
}

auto camera::get_ray(double s, double t, std::mt19937& prng) const -> ray
{
  vec3 rd = m_lens_radius * vec3::random_in_unit_disk(prng);
  vec3 offset = m_u * rd.x() + m_v * rd.y();

  return {m_origin + offset,
          m_lower_left_corner + s * m_horizontal + t * m_vertical - m_origin
              - offset};
}