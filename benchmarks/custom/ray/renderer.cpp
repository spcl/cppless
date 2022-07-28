#include <mutex>
#include <random>
#include <vector>

#include "renderer.hpp"

#include "bvh.hpp"
#include "camera.hpp"
#include "common.hpp"
#include "cppless/dispatcher/common.hpp"
#include "hittable.hpp"
#include "image.hpp"
#include "material.hpp"
#include "ray.hpp"
#include "tile.hpp"
#include "vec.hpp"

static auto ray_color(const ray& r,
                      const hittable& world,
                      int depth,
                      std::mt19937& prng) -> color
{
  hit_record rec;

  // If we've exceeded the ray bounce limit, no more light is gathered.
  if (depth <= 0) {
    return color(0, 0, 0);
  }

  if (world.hit(r, 0.001, infinity, rec)) {
    ray scattered;
    color attenuation;
    if (rec.mat_ptr->scatter(r, rec, attenuation, scattered, prng)) {
      return attenuation * ray_color(scattered, world, depth - 1, prng);
    }
    return color(0, 0, 0);
  }

  vec3 unit_direction = unit_vector(r.direction());
  auto t = 0.5 * (unit_direction.y() + 1.0);
  return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

void single_threaded_renderer::start(scene sc,
                                     image& target,
                                     std::mutex& mut,
                                     double& progress,
                                     bool& finished,
                                     std::condition_variable& cv)
{
  std::mt19937 generator(42);
  m_bvh_root = bvh_node(sc.world, generator);
  auto fn = [this, &target, &mut, &progress, &finished, &cv, sc]()
  {
    std::mt19937 generator(42);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    for (int x = 0; x < target.width(); x++) {
      for (int y = 0; y < target.height(); y++) {
        color c;
        for (int s = 0; s < sc.samples_per_pixel; ++s) {
          auto u = (x + distribution(generator)) / (target.width() - 1);
          auto v = (y + distribution(generator)) / (target.height() - 1);
          ray r = sc.cam.get_ray(u, v, generator);
          c += ray_color(r, m_bvh_root, sc.max_depth, generator);
        }

        {
          std::scoped_lock lk(mut);
          target(x, y) = c;
        }
      }

      {
        std::scoped_lock lk(mut);
        progress = static_cast<double>(x) / target.width();
      }

      cv.notify_one();
    }
    {
      std::scoped_lock lk(mut);
      finished = true;
    }
  };

  m_worker.emplace(fn);
}

void single_threaded_renderer::join()
{
  m_worker->join();
}

void multi_threaded_renderer::start(scene sc,
                                    image& target,
                                    std::mutex& mut,
                                    double& progress,
                                    bool& finished,
                                    std::condition_variable& cv)
{
  std::mt19937 generator(42);

  m_bvh_root = bvh_node(sc.world, generator);

  m_tiles = quantize_image(target.width(), target.height(), 128, 128);
  std::cerr << "number_of_tiles: " << m_tiles.size() << std::endl;

  m_workers.reserve(m_num_workers);
  for (int i = 0; i < m_num_workers; i++) {
    auto fn = [&]()
    {
      std::mt19937 generator(42);
      std::uniform_real_distribution<double> distribution(0.0, 1.0);

      unsigned long current_tile = m_tile_index++;
      while (current_tile < m_tiles.size()) {
        tile t = m_tiles[current_tile];

        image tile_img(t.width, t.height, sc.samples_per_pixel);
        for (int x = t.x; x < t.width + t.x; x++) {
          for (int y = t.y; y < t.height + t.y; y++) {
            for (int s = 0; s < sc.samples_per_pixel; ++s) {
              auto u = (x + distribution(generator)) / (target.width() - 1);
              auto v = (y + distribution(generator)) / (target.height() - 1);
              ray r = sc.cam.get_ray(u, v, generator);
              tile_img(x - t.x, y - t.y) +=
                  ray_color(r, m_bvh_root, sc.max_depth, generator);
            }
          }
        }

        {
          std::scoped_lock lk(mut);
          target.insert(t.x, t.y, tile_img);
          m_finished_tiles++;
          progress = static_cast<double>(m_finished_tiles) / m_tiles.size();
          finished = m_finished_tiles == m_tiles.size();
        }
        cv.notify_one();

        current_tile = m_tile_index++;
      }
    };
    m_workers.emplace_back(fn);
  }
}

void multi_threaded_renderer::join()
{
  for (auto& thread : m_workers) {
    thread.join();
  }
}

void aws_lambda_renderer::start(scene sc,
                                image& target,
                                std::mutex& mut,
                                double& progress,
                                bool& finished,
                                std::condition_variable& cv)
{
  auto start = [sc, &target, &mut, &progress, &finished, &cv]()
  {
    dispatcher aws;
    auto instance = aws.create_instance();
    std::mt19937 generator(42);
    auto bvh_root = bvh_node(sc.world, generator);

    camera cam = sc.cam;
    unsigned int width = target.width();
    unsigned int height = target.height();
    int samples_per_pixel = sc.samples_per_pixel;
    int max_depth = sc.max_depth;
    auto t = [cam, width, height, samples_per_pixel, max_depth](tile t,
                                                                bvh_node world)
    {
      std::mt19937 generator(42);
      std::uniform_real_distribution<double> distribution(0.0, 1.0);
      image tile_img(t.width, t.height, samples_per_pixel);
      for (int x = t.x; x < t.width + t.x; x++) {
        for (int y = t.y; y < t.height + t.y; y++) {
          for (int s = 0; s < samples_per_pixel; ++s) {
            auto u = (x + distribution(generator)) / (width - 1);
            auto v = (y + distribution(generator)) / (height - 1);
            ray r = cam.get_ray(u, v, generator);
            tile_img(x - t.x, y - t.y) +=
                ray_color(r, world, max_depth, generator);
          }
        }
      }
      return tile_img;
    };

    auto tiles = quantize_image(target.width(), target.height(), 64, 64);
    std::vector<image> images(tiles.size());

    for (int i = 0; i < tiles.size(); i++) {
      cppless::dispatch(
          instance, t, images[i], std::make_tuple(tiles[i], bvh_root));
    }

    for (int i = 0; i < images.size(); i++) {
      auto f = instance.wait_one();
      {
        std::scoped_lock lk(mut);
        tile t = tiles[f];
        target.insert(t.x, t.y, images[f]);
        progress = static_cast<double>(i) / images.size();
        cv.notify_one();
      }
    }
    {
      std::scoped_lock lk(mut);
      finished = true;
    }
    cv.notify_one();
  };
  m_worker.emplace(start);
}

void aws_lambda_renderer::join()
{
  m_worker->join();
}
