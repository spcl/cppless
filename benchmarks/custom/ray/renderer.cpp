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

#include "../../include/measurement.hpp"

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
    color emitted;
    bool did_scatter =
        rec.mat_ptr->scatter(r, rec, attenuation, emitted, scattered, prng);
    if (!did_scatter) {
      return emitted;
    }
    return emitted + attenuation * ray_color(scattered, world, depth - 1, prng);
  }

  vec3 unit_direction = unit_vector(r.direction());
  auto t = 0.5 * (unit_direction.y() + 1.0);
  return (1.0 - t) * color(0.3, 0.3, 0.3) + t * color(0.2, 0.3, 0.4);
}

void single_threaded_renderer::start(scene sc,
                                     image& target,
                                     std::mutex& mut,
                                     double& progress,
                                     bool& finished,
                                     std::condition_variable& cv)
{
  measurements benchmarker;

  for(int rep = 0; rep < m_repetitions; ++rep) {

    target.clean();

    benchmarker.start_repetition(rep);
    auto start = std::chrono::high_resolution_clock::now();

    std::mt19937 generator(42);
    bvh_node m_bvh_root = bvh_node(sc.world, generator);

    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    for (int y = 0; y < target.height(); y++) {
      for (int x = 0; x < target.width(); x++) {
        color c;
        for (int s = 0; s < sc.samples_per_pixel; ++s) {
          auto u = (x + distribution(generator)) / (target.width() - 1);
          auto v = (y + distribution(generator)) / (target.height() - 1);
          ray r = sc.cam.get_ray(u, v, generator);
          c += ray_color(r, m_bvh_root, sc.max_depth, generator);
        }

        {
          target(x, y) = c;
        }
      }
    }

    auto end = std::chrono::high_resolution_clock::now();

    benchmarker.add_result(start, end, "total");

    if(!m_img_location.empty()) {
      std::ofstream file{m_img_location + "_" + std::to_string(rep), std::ios::out};
      file << target;
    }

  }

  benchmarker.write(m_output_location);
}

void single_threaded_renderer::join()
{
  //m_worker->join();
}

void multi_threaded_renderer::start(scene sc,
                                    image& target,
                                    std::mutex& mut,
                                    double& progress,
                                    bool& finished,
                                    std::condition_variable& cv)
{
  measurements benchmarker;
  m_workers.reserve(m_num_workers);

  for(int rep = 0; rep < m_repetitions; ++rep) {

    target.clean();
    m_workers.clear();
    m_tile_index = 0;
    m_finished_tiles = 0;

    benchmarker.start_repetition(rep);
    auto start = std::chrono::high_resolution_clock::now();

    std::mt19937 generator(42);

    bvh_node m_bvh_root = bvh_node(sc.world, generator);

    m_tiles = quantize_image(
        target.width(), target.height(), m_tile_width, m_tile_height);
    std::cerr << "number_of_tiles: " << m_tiles.size() << std::endl;

    for (int i = 0; i < m_num_workers; i++) {
      auto worker_fn = [this, &m_bvh_root, &target, &progress, &finished, &cv, &mut, sc]()
      {
        std::mt19937 generator(42);  // NOLINT
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        double width = static_cast<double>(target.width()) - 1;
        double height = static_cast<double>(target.height()) - 1;

        unsigned long current_tile_index = m_tile_index++;
        while (current_tile_index < m_tiles.size()) {
          tile current_tile = m_tiles[current_tile_index];

          for (int x = current_tile.x; x < current_tile.width + current_tile.x;
               x++) {
            for (int y = current_tile.y; y < current_tile.height + current_tile.y;
                 y++) {
              for (int s = 0; s < sc.samples_per_pixel; s++) {
                auto u = (x + distribution(generator)) / width;
                auto v = (y + distribution(generator)) / height;
                ray r = sc.cam.get_ray(u, v, generator);
                target(x, y) += ray_color(r, m_bvh_root, sc.max_depth, generator);
              }
            }
          }

          {
            std::scoped_lock lk(mut);
            m_finished_tiles++;
            progress = static_cast<double>(m_finished_tiles)
                / static_cast<double>(m_tiles.size());
            finished = m_finished_tiles == m_tiles.size();
            cv.notify_one();
          }

          current_tile_index = m_tile_index++;
        }
      };
      m_workers.emplace_back(worker_fn);
    }

    for (auto& thread : m_workers) {
      thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    benchmarker.add_result(start, end, "total");

    if(!m_img_location.empty()) {
      std::ofstream file{m_img_location + "_" + std::to_string(rep), std::ios::out};
      file << target;
    }
  }

  benchmarker.write(m_output_location);
}

void multi_threaded_renderer::join()
{
  //for (auto& thread : m_workers) {
  //  thread.join();
  //}
}

void aws_lambda_renderer::start(scene sc,
                                image& target,
                                std::mutex& mut,
                                double& progress,
                                bool& finished,
                                std::condition_variable& cv)
{

  auto start = [sc, &target, &mut, &progress, &finished, &cv, this]()
  {

    std::vector<std::tuple<int, int, uint64_t, std::string, bool>> time_results;
    dispatcher aws;
    auto instance = aws.create_instance();

    for(int rep = 0; rep < m_repetitions; ++rep) {

      auto tile_start = std::chrono::high_resolution_clock::now();
      auto tiles = quantize_image(
          target.width(), target.height(), m_tile_width, m_tile_height);
      auto tile_end = std::chrono::high_resolution_clock::now();

      auto bhv_start = std::chrono::high_resolution_clock::now();
      std::mt19937 generator(42);
      auto bvh_root = bvh_node(sc.world, generator);
      auto bhv_end = std::chrono::high_resolution_clock::now();

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

      std::vector<image> images(tiles.size());
      int start_position_vec = time_results.size();
      int first_id = -1;

      auto start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < tiles.size(); i++) {
        auto start_func = std::chrono::high_resolution_clock::now();
        auto id = cppless::dispatch(instance,
                          t,
                          images[i],
                          {tiles[i], bvh_root});
                          //m_span_ref.create_child("lambda_invocation"));
        if(first_id == -1)
          first_id = id;
        auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(start_func).time_since_epoch().count();
        time_results.emplace_back(rep, id, ts, "", false);
      }
      auto dispatch_end = std::chrono::high_resolution_clock::now();

      for (int i = 0; i < images.size(); i++) {

        auto f = instance.wait_one();

        auto end = std::chrono::high_resolution_clock::now(); 
        auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch().count();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

        int idx = std::get<0>(f) - first_id;
        {
          std::scoped_lock lk(mut);
          tile t = tiles[idx];
          target.insert(t.x, t.y, images[idx]);
          progress = static_cast<double>(i) / images.size();
          //cv.notify_one();
        }

        int pos = std::get<0>(f);
        int pos_shift = pos - first_id;
        int pos_in_vector = start_position_vec + pos_shift;
        std::get<2>(time_results[pos_in_vector]) = ts - std::get<2>(time_results[pos_in_vector]);
        std::get<3>(time_results[pos_in_vector]) = std::get<1>(f).invocation_id;
        std::get<4>(time_results[pos_in_vector]) = std::get<1>(f).is_cold;
      }

      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
      time_results.emplace_back(rep, -1, duration, "compute_total", false);

      {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-tile_start).count();
        time_results.emplace_back(rep, -1, duration, "total", false);
      }
      {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(bhv_end-bhv_start).count();
        time_results.emplace_back(rep, -1, duration, "bhv", false);
      }
      {
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(tile_end-tile_start).count();
        time_results.emplace_back(rep, -1, duration, "tile", false);
      }
      {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(dispatch_end-tile_start).count();
        time_results.emplace_back(rep, -1, duration, "dispatch_from_start", false);
      }
      {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(dispatch_end-start).count();
        time_results.emplace_back(rep, -1, duration, "dispatch", false);
      }

      if(!m_img_location.empty()) {
        std::ofstream file{m_img_location + "_" + std::to_string(rep), std::ios::out};
        file << target;
      }

      if(rep == 0)
        std::clog << "number_of_tiles: " << tiles.size() << std::endl;
    }

    {
      std::ofstream output_file{m_output_location, std::ios::out};
      output_file << "repetition,sample,time,request_id,is_cold" << std::endl;
      for(auto & res : time_results)
        output_file << std::get<0>(res) << "," << std::get<1>(res) << "," << std::get<2>(res) << "," << std::get<3>(res) << "," << std::get<4>(res) << std::endl;
    }
    {
      std::scoped_lock lk(mut);
      finished = true;
      cv.notify_one();
    }
  };
  m_worker.emplace(start);
}

void aws_lambda_renderer::join()
{
  m_worker->join();
}
