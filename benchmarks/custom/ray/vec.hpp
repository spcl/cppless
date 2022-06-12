#pragma once

#include <array>
#include <cmath>
#include <iostream>
#include <random>

using base_vec3 = double __attribute__((ext_vector_type(3)));

class vec3
{
public:
  vec3()
      : m_base {0, 0, 0}
  {
  }

  vec3(base_vec3 v)
      : m_base(v)
  {
  }

  vec3(const double e0, const double e1, const double e2)
      : m_base {e0, e1, e2}
  {
  }

  [[nodiscard]] inline auto base() const -> const base_vec3&
  {
    return m_base;
  }

  [[nodiscard]] inline auto x() const -> double
  {
    return m_base[0];
  }
  [[nodiscard]] inline auto y() const -> double
  {
    return m_base[1];
  }
  [[nodiscard]] inline auto z() const -> double
  {
    return m_base[2];
  }

  [[nodiscard]] inline auto operator-() const -> vec3
  {
    return {-m_base[0], -m_base[1], -m_base[2]};
  }
  [[nodiscard]] inline auto operator[](const int i) const -> double
  {
    return m_base[i];  // NOLINT
  }

  inline auto operator+=(const vec3 v) -> vec3&
  {
    m_base = base() + v.base();
    return *this;
  }

  inline auto operator*=(const double t) -> vec3&
  {
    m_base = base() * t;
    return *this;
  }

  inline auto operator/=(const double t) -> vec3&
  {
    return *this *= 1 / t;
  }

  [[nodiscard]] inline auto length() const -> double
  {
    return std::sqrt(length_squared());
  }

  [[nodiscard]] inline auto length_squared() const -> double
  {
    return m_base.x * m_base.x + m_base.y * m_base.y + m_base.z * m_base.z;
  }

  [[nodiscard]] inline auto near_zero() const -> bool
  {
    // Return true if the vector is close to zero in all dimensions.
    const auto s = 1e-8;
    return (std::abs(m_base.x) < s) && (std::abs(m_base.y) < s)
        && (std::abs(m_base.z) < s);
  }

  [[nodiscard]] inline static auto random(std::mt19937& prng) -> vec3
  {
    std::uniform_real_distribution<double> distribution(0, 1);
    return {distribution(prng), distribution(prng), distribution(prng)};
  }

  [[nodiscard]] inline static auto random(double min,
                                          double max,
                                          std::mt19937& prng) -> vec3
  {
    std::uniform_real_distribution<double> distribution(min, max);
    return {distribution(prng), distribution(prng), distribution(prng)};
  }

  [[nodiscard]] inline static auto random_in_unit_sphere(std::mt19937& prng)
      -> vec3
  {
    while (true) {
      auto p = vec3::random(-1, 1, prng);
      if (p.length_squared() < 1) {
        return p;
      }
    }
  }

  [[nodiscard]] inline static auto random_unit_vector(std::mt19937& prng)
      -> vec3;

  [[nodiscard]] inline static auto random_in_unit_disk(std::mt19937& prng)
      -> vec3
  {
    while (true) {
      std::uniform_real_distribution<double> distribution(0, 1);
      vec3 p = {distribution(prng), distribution(prng), distribution(prng)};
      if (p.length_squared() >= 1) {
        continue;
      }
      return p;
    }
  }

  [[nodiscard]] inline static auto random_in_hemisphere(const vec3& normal,
                                                        std::mt19937& prng)
      -> vec3;

  template<class Archive>
  void serialize(Archive& ar)
  {
    double x = m_base.x;
    double y = m_base.y;
    double z = m_base.z;
    ar(x, y, z);
    m_base.x = x;
    m_base.y = y;
    m_base.z = z;
  }

private:
  base_vec3 m_base;
};

using point3 = vec3;  // 3D point
using color = vec3;  // RGB color

inline auto operator<<(std::ostream& out, const vec3 v) -> std::ostream&
{
  return out << v[0] << ' ' << v[1] << ' ' << v[2];
}

[[nodiscard]] inline auto operator+(const vec3& u, const vec3& v) -> vec3
{
  return u.base() + v.base();
}

[[nodiscard]] inline auto operator-(const vec3& u, const vec3& v) -> vec3
{
  return u.base() - v.base();
}

[[nodiscard]] inline auto operator*(const vec3& u, const vec3& v) -> vec3
{
  return u.base() * v.base();
}

[[nodiscard]] inline auto operator*(const double t, const vec3& v) -> vec3
{
  return v * t;
}

[[nodiscard]] inline auto operator*(const vec3& v, const double& t) -> vec3
{
  return t * v;
}

[[nodiscard]] inline auto operator/(const vec3& v, const double& t) -> vec3
{
  return (1 / t) * v;
}

[[nodiscard]] inline auto dot(const vec3 u, const vec3 v) -> double
{
  return u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
}

[[nodiscard]] inline auto cross(const vec3 u, const vec3 v) -> vec3
{
  return {u[1] * v[2] - u[2] * v[1],
          u[2] * v[0] - u[0] * v[2],
          u[0] * v[1] - u[1] * v[0]};
}

[[nodiscard]] inline auto reflect(const vec3& v, const vec3& n) -> vec3
{
  return v - 2 * dot(v, n) * n;
}

[[nodiscard]] inline auto unit_vector(const vec3& v) -> vec3
{
  return v / v.length();
}

[[nodiscard]] inline auto refract(const vec3& uv,
                                  const vec3& n,
                                  double etai_over_etat) -> vec3
{
  auto cos_theta = std::min(dot(-uv, n), 1.0);
  vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
  vec3 r_out_parallel =
      -std::sqrt(std::abs(1.0 - r_out_perp.length_squared())) * n;
  return r_out_perp + r_out_parallel;
}

[[nodiscard]] inline auto vec3::random_unit_vector(std::mt19937& prng) -> vec3
{
  return unit_vector(vec3::random_in_unit_sphere(prng));
}

[[nodiscard]] inline auto vec3::random_in_hemisphere(const vec3& normal,
                                                     std::mt19937& prng) -> vec3
{
  vec3 in_unit_sphere = vec3::random_in_unit_sphere(prng);
  if (dot(in_unit_sphere, normal)
      > 0.0)  // In the same hemisphere as the normal
    return in_unit_sphere;
  return -in_unit_sphere;
}