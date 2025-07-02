#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>

#include <random>

#include <thread>

#include <argparse/argparse.hpp>

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
        } else if (choose_mat < 0.65) {
          // metal
          auto albedo = color::random(0.5, 1, prng);
          auto fuzz = distribution(prng) / 2;
          sphere_material = std::make_shared<metal>(albedo, fuzz);
          world.add(make_shared<sphere>(center, 0.4, sphere_material));
        } else if (choose_mat < 0.95) {
          // emissive
          auto light_color = color::random(5, 10, prng);
          sphere_material = std::make_shared<diffuse_light>(light_color);
          world.add(make_shared<sphere>(center, 0.1, sphere_material));
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

  auto material4 = std::make_shared<diffuse_light>(color(0.7, 0.4, 0.0));
  world.add(make_shared<sphere>(point3(-8, 1, 0), 1.0, material4));

  return world;
}

__attribute((weak)) int main(int argc, char* argv[])
{
  argparse::ArgumentParser program("ray_bench");

  program.add_argument("-w", "--width").scan<'d', int>();
  program.add_argument("-a", "--aspect").scan<'g', double>();
  program.add_argument("-s", "--samples").scan<'d', int>();
  program.add_argument("-d", "--depth").scan<'d', int>();

  program.add_argument("--dispatcher")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--dispatcher-tile-width")
      .help("Prefix length value when using the dispatcher implementation")
      .default_value(64)
      .scan<'i', unsigned int>();
  program.add_argument("--dispatcher-tile-height")
      .help("Prefix length value when using the dispatcher implementation")
      .default_value(64)
      .scan<'i', unsigned int>();
  program.add_argument("--dispatcher-trace-output")
      .default_value(std::string {""});
  program.add_argument("--path")
      .default_value(std::string {""});
  program.add_argument("--serial").default_value(false).implicit_value(true);
  program.add_argument("--threads").default_value(-1).scan<'d', int>();
  program.add_argument("-r")
      .help("number of repetitions")
      .default_value(1)
      .scan<'i', int>();
  program.add_argument("-o")
      .default_value(std::string(""))
      .help("location to write output statistics");
  program.add_argument("-i")
      .default_value(std::string(""))
      .help("location to write output image");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  // Image
  const double aspect_ratio = program.get<double>("-a");
  const int image_width = program.get<int>("-w");
  const int image_height = static_cast<int>(image_width / aspect_ratio);
  const int samples_per_pixel = program.get<int>("-s");
  const int max_depth = program.get<int>("-d");

  // Dispatcher benchmarking
  int repetitions = program.get<int>("-r");
  std::string output_location = program.get<std::string>("-o");
  auto img_location = program.get<std::string>("--path");

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
  //cppless::tracing_span_container spans;
  //auto root = spans.create_root("root").start();
  if (program["--dispatcher"] == true) {
    auto tile_width = program.get<unsigned int>("--dispatcher-tile-width");
    auto tile_height = program.get<unsigned int>("--dispatcher-tile-height");
    r = std::make_unique<aws_lambda_renderer>(tile_width, tile_height, repetitions, output_location, img_location);
  } else if (program["--serial"] == true) {
    r = std::make_unique<single_threaded_renderer>(repetitions, output_location, img_location);
  } else if (program["--threads"] != -1) {
    auto tile_width = program.get<unsigned int>("--dispatcher-tile-width");
    auto tile_height = program.get<unsigned int>("--dispatcher-tile-height");
    r = std::make_unique<multi_threaded_renderer>(program.get<int>("--threads"), tile_width, tile_height, repetitions, output_location, img_location);
  }
  auto start = std::chrono::high_resolution_clock::now();
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

 // while (true) {
 //   std::unique_lock lk(mu);

 //   double current_progress = progress;
 //   bool current_finished = finished;

 //   //std::cerr << std::setw(5) << std::setfill('0') << std::fixed
 //   //          << std::setprecision(2) << "\rProgress: " << progress * 100 << "%"
 //   //          << std::flush;
 //   if (current_finished) {
 //     break;
 // }
 //   cv.wait(lk,
 //           [&]() {
 //             return progress != current_progress
 //                 || current_finished != finished;
 //           });
 // }

  r->join();
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();

  std::cout << "Time: " << duration << std::endl;

  if(!img_location.empty() && program["--dispatcher"] == false) {

    std::ofstream file{img_location, std::ios::out};
    file << img;
  }

  std::cerr << "\nDone.\n";

  // If using dispatcher and option is set, write trace
  //if (program["--dispatcher"] == true) {
  //  auto trace_location = program.get<std::string>("--dispatcher-trace-output");
  //  if (!trace_location.empty()) {
  //    std::ofstream trace_file(trace_location);
  //    nlohmann::json spans_json = spans;
  //    trace_file << spans_json.dump(2);
  //  }
  //}
}
