cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 14)

project(cpplessBenchmarksSeBSThumbnailer CXX)

message("Module path ${CMAKE_PREFIX_PATH}")

find_package(OpenCV COMPONENTS imgcodecs imgproc REQUIRED)
find_package(Torch REQUIRED)
find_package(TorchVision REQUIRED)
#find_package(PkgConfig)
#pkg_check_modules(OpenCV REQUIRED OpenCV)

add_executable("benchmark_sebs_image_recognition" dispatcher.cpp)
target_link_libraries("benchmark_sebs_image_recognition" PRIVATE cppless::cppless)
aws_lambda_target("benchmark_sebs_image_recognition")
aws_lambda_serverless_target("benchmark_sebs_image_recognition")

target_include_directories("benchmark_sebs_image_recognition" PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries("benchmark_sebs_image_recognition" PUBLIC ${OpenCV_LIBS})
target_link_libraries("benchmark_sebs_image_recognition" PUBLIC TorchVision::TorchVision "${TORCH_LIBRARIES}" ${OpenCV_LIBS})
set_property(TARGET "benchmark_sebs_image_recognition" PROPERTY CXX_STANDARD 14)

add_executable("benchmark_sebs_image_recognition_local" local.cpp)

target_include_directories("benchmark_sebs_image_recognition_local" PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries("benchmark_sebs_image_recognition_local" PUBLIC ${OpenCV_LIBS})
target_link_libraries("benchmark_sebs_image_recognition_local" PUBLIC TorchVision::TorchVision "${TORCH_LIBRARIES}" ${OpenCV_LIBS})
set_property(TARGET "benchmark_sebs_image_recognition_local" PROPERTY CXX_STANDARD 14)
