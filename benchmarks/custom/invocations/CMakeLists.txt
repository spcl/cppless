cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksCustomRay CXX)

find_package(MPI)

add_executable("benchmark_custom_invocations" dispatcher.cpp)
target_compile_options("benchmark_custom_invocations" PRIVATE "-ffast-math")
target_link_libraries("benchmark_custom_invocations" PRIVATE cppless::cppless)
target_compile_features("benchmark_custom_invocations" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_custom_invocations")
aws_lambda_serverless_target("benchmark_custom_invocations")

#message("${MPI_FOUND}")
#if(MPI_FOUND)

add_executable("benchmark_custom_invocations_mpi" dispatcher_mpi.cpp)
target_compile_options("benchmark_custom_invocations_mpi" PRIVATE "-ffast-math")
target_link_libraries("benchmark_custom_invocations_mpi" PRIVATE cppless::cppless)
target_compile_features("benchmark_custom_invocations_mpi" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_custom_invocations_mpi")
aws_lambda_serverless_target("benchmark_custom_invocations_mpi")

if(MPI_FOUND)
target_compile_definitions(benchmark_custom_invocations_mpi PUBLIC USE_MPI)
target_link_libraries("benchmark_custom_invocations_mpi" PUBLIC MPI::MPI_CXX)
endif()
#endif()
