#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>

#include "bvh.hpp"
#include "camera.hpp"
#include "color.hpp"
#include "common.hpp"
#include "hittable.hpp"
#include "hittable_list.hpp"
#include "image.hpp"
#include "material.hpp"
#include "ray.hpp"
#include "renderer.hpp"
#include "sphere.hpp"
#include "tile.hpp"
#include "vec.hpp"

hittable_list random_scene(std::mt19937& prng)
{
  std::uniform_real_distribution<double> distribution(0, 1);

  hittable_list world;

  auto ground_material = std::make_shared<lambertian>(color(0.5, 0.5, 0.5));
  world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

  for (int a = -11; a < 11; a++) {
    for (int b = -11; b < 11; b++) {
      auto choose_mat = distribution(prng);
      point3 center(
          a + 0.9 * distribution(prng), 0.2, b + 0.9 * distribution(prng));

      if ((center - point3(4, 0.2, 0)).length() > 0.9) {
        std::shared_ptr<material> sphere_material;

        if (choose_mat < 0.8) {
          // diffuse
          auto albedo = color::random(prng) * color::random(prng);
          sphere_material = std::make_shared<lambertian>(albedo);
          world.add(make_shared<sphere>(center, 0.2, sphere_material));
        } else if (choose_mat < 0.95) {
          // metal
          auto albedo = color::random(0.5, 1, prng);
          auto fuzz = distribution(prng) / 2;
          sphere_material = std::make_shared<metal>(albedo, fuzz);
          world.add(make_shared<sphere>(center, 0.2, sphere_material));
        } else {
          // glass
          sphere_material = std::make_shared<dielectric>(1.5);
          world.add(make_shared<sphere>(center, 0.2, sphere_material));
        }
      }
    }
  }

  auto material1 = std::make_shared<dielectric>(1.5);
  world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

  auto material2 = std::make_shared<lambertian>(color(0.4, 0.2, 0.1));
  world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

  auto material3 = std::make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
  world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

  return world;
}

__attribute((weak)) int main(int argc, char* argv[])
{
  // Image
  const auto aspect_ratio = 3.0 / 2.0;
  const int image_width = 900;
  const int image_height = static_cast<int>(image_width / aspect_ratio);
  const int samples_per_pixel = 120;
  const int max_depth = 50;

  // World
  std::mt19937 generator(42);
  auto world = random_scene(generator);

  point3 lookfrom(13, 2, 3);
  point3 lookat(0, 0, 0);
  vec3 vup(0, 1, 0);
  auto dist_to_focus = 10.0;
  auto aperture = 0.1;

  camera cam(lookfrom, lookat, vup, 20, aspect_ratio, aperture, dist_to_focus);

  // Render

  image img(image_width, image_height, samples_per_pixel);
  double progress = 0;
  bool finished = false;

  std::mutex mu;
  std::condition_variable cv;

  std::string arg(argv[1]);

  std::unique_ptr<renderer> r;
  if (arg == "aws_lambda_renderer") {
    r = std::make_unique<aws_lambda_renderer>();
  } else if (arg == "single_threaded_renderer") {
    r = std::make_unique<single_threaded_renderer>();
  } else if (arg == "multi_threaded_renderer") {
    r = std::make_unique<multi_threaded_renderer>(8);
  }
  r->start(
      {
          .world = world,
          .cam = cam,
          .samples_per_pixel = samples_per_pixel,
          .max_depth = max_depth,
      },
      img,
      mu,
      progress,
      finished,
      cv);

  while (true) {
    std::unique_lock lk(mu);

    double current_progress = progress;
    bool current_finished = finished;

    std::cerr << std::setw(5) << std::setfill('0') << std::fixed
              << std::setprecision(2) << "\rProgress: " << progress * 100 << "%"
              << std::flush;
    if (current_finished)
      break;
    cv.wait(lk, [&]() { return progress != current_progress; });
  }

  r->join();

  std::cout << img;

  std::cerr << "\nDone.\n";
}