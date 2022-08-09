#include "material.hpp"

#include "hittable.hpp"
#include "vec.hpp"

auto diffuse_light::scatter(const ray& /*r_in*/,
                            const hit_record&  /*rec*/,
                            color&  /*attenuation*/,
                            color& emitted,
                            ray&  /*scattered*/,
                            std::mt19937&  /*prng*/) const -> bool
{
  emitted = m_color;
  return false;
}

auto lambertian::scatter(const ray& /*r_in*/,
                         const hit_record& rec,
                         color& attenuation,
                         color&  /*emitted*/,
                         ray& scattered,
                         std::mt19937& prng) const -> bool
{
  auto scatter_direction = rec.normal + vec3::random_unit_vector(prng);
  if (scatter_direction.near_zero()) {
    scatter_direction = rec.normal;
  }
  scattered = ray(rec.p, scatter_direction);
  attenuation = m_albedo;
  return true;
}

auto metal::scatter(const ray& r_in,
                    const hit_record& rec,
                    color& attenuation,
                    color& /*emitted*/,
                    ray& scattered,
                    std::mt19937& prng) const -> bool
{
  vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
  scattered =
      ray(rec.p, reflected + m_fuzz * vec3::random_in_unit_sphere(prng));
  attenuation = m_albedo;
  return (dot(scattered.direction(), rec.normal) > 0);
}

static auto reflectance(double cosine, double ref_idx) -> double
{
  // Use Schlick's approximation for reflectance.
  auto r0 = (1 - ref_idx) / (1 + ref_idx);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow((1 - cosine), 5);
}

auto dielectric::scatter(const ray& r_in,
                         const hit_record& rec,
                         color& attenuation,
                         color& /*emitted*/,
                         ray& scattered,
                         std::mt19937& prng) const -> bool
{
  attenuation = color(1.0, 1.0, 1.0);
  double refraction_ratio = rec.front_face ? (1.0 / m_ir) : m_ir;

  vec3 unit_direction = unit_vector(r_in.direction());
  double cos_theta = std::min(dot(-unit_direction, rec.normal), 1.0);
  double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

  bool cannot_refract = refraction_ratio * sin_theta > 1.0;
  vec3 direction;

  std::uniform_real_distribution<double> distribution(0, 1);
  if (cannot_refract
      || reflectance(cos_theta, refraction_ratio) > distribution(prng))
  {
    direction = reflect(unit_direction, rec.normal);
  } else {
    direction = refract(unit_direction, rec.normal, refraction_ratio);
  }

  scattered = ray(rec.p, direction);
  return true;
}
