#pragma once

#include <random>

#include <cereal/archives/binary.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

#include "ray.hpp"
#include "vec.hpp"

struct hit_record;

class material
{
public:
  material() = default;
  virtual auto scatter(const ray& r_in,
                       const hit_record& rec,
                       color& attenuation,
                       ray& scattered,
                       std::mt19937& prng) const -> bool = 0;
  virtual ~material() = default;
  template<class Archive>
  void serialize(Archive& /*ar*/)
  {
  }
};

class lambertian : public material
{
public:
  lambertian() = default;
  explicit lambertian(const color& a)
      : m_albedo(a)
  {
  }

  auto scatter(const ray& r_in,
               const hit_record& rec,
               color& attenuation,
               ray& scattered,
               std::mt19937& prng) const -> bool override;
  ~lambertian() override = default;
  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::base_class<material>(this), m_albedo);
  }

private:
  color m_albedo;
};

CEREAL_REGISTER_TYPE(lambertian)  // NOLINT

class metal : public material
{
public:
  metal() = default;
  explicit metal(const color& a, double fuzz)
      : m_albedo(a)
      , m_fuzz(fuzz)
  {
  }

  auto scatter(const ray& r_in,
               const hit_record& rec,
               color& attenuation,
               ray& scattered,
               std::mt19937& prng) const -> bool override;
  ~metal() override = default;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::base_class<material>(this), m_albedo, m_fuzz);
  }

private:
  color m_albedo;
  double m_fuzz;
};

CEREAL_REGISTER_TYPE(metal)  // NOLINT

class dielectric : public material
{
public:
  dielectric() = default;
  explicit dielectric(double index_of_refraction)
      : m_ir(index_of_refraction)
  {
  }

  auto scatter(const ray& r_in,
               const hit_record& rec,
               color& attenuation,
               ray& scattered,
               std::mt19937& prng) const -> bool override;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::base_class<material>(this), m_ir);
  }

private:
  double m_ir;  // Index of Refraction
};

CEREAL_REGISTER_TYPE(dielectric)  // NOLINT