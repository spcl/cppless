cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksCustomRay CXX)

add_executable("benchmark_custom_ray" main.cpp camera.cpp color.cpp hittable_list.cpp material.cpp sphere.cpp renderer.cpp bvh.cpp aabb.cpp)
target_compile_options("benchmark_custom_ray" PRIVATE "-ffast-math" "-march=native")
target_link_libraries("benchmark_custom_ray" PRIVATE cppless::cppless)
target_compile_features("benchmark_custom_ray" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_custom_ray")
aws_lambda_serverless_target("benchmark_custom_ray")