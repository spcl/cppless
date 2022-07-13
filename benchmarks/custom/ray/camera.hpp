#pragma once

#include <random>

#include "ray.hpp"
#include "vec.hpp"

class camera
{
public:
  camera() = default;
  camera(point3 lookfrom,
         point3 lookat,
         vec3 vup,
         double vfov,  // vertical field-of-view in degrees
         double aspect_ratio,
         double aperture,
         double focus_dist);

  [[nodiscard]] auto get_ray(double u, double v, std::mt19937& prng) const
      -> ray;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(m_origin,
       m_lower_left_corner,
       m_horizontal,
       m_vertical,
       m_u,
       m_v,
       m_w,
       m_lens_radius);
  }

private:
  point3 m_origin;
  point3 m_lower_left_corner;
  vec3 m_horizontal;
  vec3 m_vertical;
  vec3 m_u, m_v, m_w;
  double m_lens_radius;
};