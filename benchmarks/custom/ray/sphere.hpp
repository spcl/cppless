#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>

#include "hittable.hpp"
#include "vec.hpp"

class sphere : public hittable
{
public:
  sphere() = default;
  ~sphere() override = default;
  sphere(point3 center, double radius, std::shared_ptr<material> m)
      : m_center(center)
      , m_radius(radius)
      , m_material(std::move(m)) {};

  auto hit(const ray& r, double t_min, double t_max, hit_record& rec) const
      -> bool override;

  auto bounding_box(aabb& output_box) const -> bool override;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::base_class<hittable>(this), m_center, m_radius, m_material);
  }

private:
  point3 m_center;
  double m_radius {};
  std::shared_ptr<material> m_material;
};

#include "material.hpp"

CEREAL_REGISTER_TYPE(sphere)  // NOLINT