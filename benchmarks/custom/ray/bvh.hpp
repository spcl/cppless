#pragma once

#include <memory>
#include <random>
#include <variant>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/variant.hpp>

#include "hittable.hpp"
#include "hittable_list.hpp"
#include "vec.hpp"

class bvh_node : public hittable
{
public:
  bvh_node() = default;

  bvh_node(const hittable_list& list, std::mt19937& prng)
      : bvh_node(list.objects(), 0, list.objects().size(), prng)
  {
  }

  bvh_node(const std::vector<std::shared_ptr<hittable>>& src_objects,
           size_t start,
           size_t end,
           std::mt19937& prng);

  auto hit(const ray& r, double t_min, double t_max, hit_record& rec) const
      -> bool override;

  auto bounding_box(aabb& output_box) const -> bool override;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(cereal::base_class<hittable>(this), left, right, box);
  }

public:
  std::variant<std::shared_ptr<bvh_node>, std::shared_ptr<hittable>> left;
  std::variant<std::shared_ptr<bvh_node>, std::shared_ptr<hittable>> right;
  aabb box;
};

CEREAL_REGISTER_TYPE(bvh_node)  // NOLINT