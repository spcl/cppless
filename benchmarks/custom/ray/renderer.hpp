#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <cppless/dispatcher/aws-lambda.hpp>
#include <cppless/dispatcher/sendable.hpp>

#include "bvh.hpp"
#include "camera.hpp"
#include "cppless/dispatcher/aws-lambda.hpp"
#include "hittable_list.hpp"
#include "image.hpp"
#include "tile.hpp"
#include "vec.hpp"

struct scene
{
  const hittable_list& world;
  const camera& cam;
  int samples_per_pixel;
  int max_depth;
};

class renderer
{
public:
  virtual void start(scene sc,
                     image& target,
                     std::mutex& mut,
                     double& progress,
                     bool& finished,
                     std::condition_variable& cv) = 0;
  virtual void join() = 0;
  virtual ~renderer() = default;
};

class single_threaded_renderer : public renderer
{
public:
  void start(scene sc,
             image& target,
             std::mutex& mut,
             double& progress,
             bool& finished,
             std::condition_variable& cv) override;
  void join() override;
  ~single_threaded_renderer() override = default;

private:
  bvh_node m_bvh_root;
  std::optional<std::thread> m_worker;
};

class multi_threaded_renderer : public renderer
{
public:
  explicit multi_threaded_renderer(int num_workers)
      : m_num_workers(num_workers)
  {
  }
  void start(scene sc,
             image& target,
             std::mutex& mut,
             double& progress,
             bool& finished,
             std::condition_variable& cv) override;
  void join() override;

private:
  int m_num_workers;
  bvh_node m_bvh_root;
  std::vector<std::thread> m_workers;
  std::atomic<unsigned long> m_tile_index = 0;
  int m_finished_tiles = 0;
  std::vector<tile> m_tiles;
};

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;
constexpr unsigned int memory_limit = 2048;
constexpr unsigned int ephemeral_storage = 64;
namespace lambda = cppless::aws;
using task =
    dispatcher::task<lambda::with_memory<memory_limit>,
                     lambda::with_ephemeral_storage<ephemeral_storage>>;

class aws_lambda_renderer : public renderer
{
public:
  aws_lambda_renderer() = default;
  void start(scene sc,
             image& target,
             std::mutex& mut,
             double& progress,
             bool& finished,
             std::condition_variable& cv) override;

  void join() override;

private:
  std::optional<std::thread> m_worker;
};
