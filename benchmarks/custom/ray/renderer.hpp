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
  explicit multi_threaded_renderer(
                      int num_workers,
                      unsigned int tile_width,
                      unsigned int tile_height
  ): m_num_workers(num_workers),
     m_tile_width(tile_width),
     m_tile_height(tile_height)
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
  unsigned int m_tile_width;
  unsigned int m_tile_height;
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
  aws_lambda_renderer(unsigned int tile_width,
                      unsigned int tile_height,
                      int repetitions,
                      std::string output_location,
                      std::string img_location)
                      //cppless::tracing_span_ref span_ref)
      : m_tile_width(tile_width),
        m_tile_height(tile_height),
        m_repetitions(repetitions),
        m_output_location(output_location),
        m_img_location(img_location)
      {};
      //, m_span_ref(span_ref) {};
  void start(scene sc,
             image& target,
             std::mutex& mut,
             double& progress,
             bool& finished,
             std::condition_variable& cv
  ) override;

  void join() override;

private:

  int m_repetitions;
  std::string m_output_location;
  std::string m_img_location;

  unsigned int m_tile_width;
  unsigned int m_tile_height;
  //cppless::tracing_span_ref m_span_ref;
  std::optional<std::thread> m_worker;
};
