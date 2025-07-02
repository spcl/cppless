cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 14)

project(cpplessBenchmarksSeBSThumbnailer CXX)

find_package(OpenCV COMPONENTS imgcodecs imgproc REQUIRED)
#find_package(PkgConfig)
#pkg_check_modules(OpenCV REQUIRED OpenCV)

add_executable("benchmark_sebs_thumbnailer" dispatcher.cpp)
target_link_libraries("benchmark_sebs_thumbnailer" PRIVATE cppless::cppless)
aws_lambda_target("benchmark_sebs_thumbnailer")
aws_lambda_serverless_target("benchmark_sebs_thumbnailer")

target_include_directories("benchmark_sebs_thumbnailer" PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries("benchmark_sebs_thumbnailer" PUBLIC ${OpenCV_LIBS})
set_property(TARGET "benchmark_sebs_thumbnailer" PROPERTY CXX_STANDARD 14)

