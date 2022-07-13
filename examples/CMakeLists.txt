cmake_minimum_required(VERSION 3.14)

project(cpplessExamples CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(cppless REQUIRED)
endif()

add_custom_target(run-examples)

function(add_example NAME)
  add_executable("${NAME}_example" "${NAME}.cpp")
  target_compile_options("${NAME}_example" PRIVATE -cppless -falt-entry "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
  target_link_options("${NAME}_example" PRIVATE -cppless -falt-entry)
  target_link_libraries("${NAME}_example" PRIVATE cppless::cppless)
  target_compile_features("${NAME}_example" PRIVATE cxx_std_20)
  add_custom_target("run_${NAME}_example" COMMAND "./${NAME}_example" VERBATIM)
  add_dependencies("run_${NAME}_example" "${NAME}_example")

  add_dependencies(run-examples "run_${NAME}_example")
endfunction()

add_example(alt_entry)
add_example(dispatcher)
add_example(graph)
add_example(http_client)
add_example(http2_client)
add_example(lambda_reflection)
add_example(merge_sort)
add_example(get_object)
add_example(hole_punching)

add_folders(Example)

add_subdirectory(aws)
add_subdirectory(lto_example)