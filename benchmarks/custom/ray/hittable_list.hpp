#pragma once

#include <memory>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>

#include "hittable.hpp"

class hittable_list : public hittable
{
public:
  hittable_list() = default;

  void clear()
  {
    m_objects.clear();
  }

  void add(std::shared_ptr<hittable> object)
  {
    m_objects.push_back(std::move(object));
  }

  [[nodiscard]] auto objects() -> std::vector<std::shared_ptr<hittable>>&
  {
    return m_objects;
  }

  [[nodiscard]] auto objects() const
      -> const std::vector<std::shared_ptr<hittable>>&
  {
    return m_objects;
  }

  auto hit(const ray& r, double t_min, double t_max, hit_record& rec) const
      -> bool override;

  auto bounding_box(aabb& output_box) const -> bool override;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::base_class<hittable>(this), m_objects);
  }

private:
  std::vector<std::shared_ptr<hittable>> m_objects;
};

CEREAL_REGISTER_TYPE(hittable_list)  // NOLINT