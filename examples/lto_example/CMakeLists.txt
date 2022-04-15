project(cpplessExamplesLto)

cmake_minimum_required(VERSION 3.9.4)

#include(CheckIPOSupported)
#check_ipo_supported(RESULT supported OUTPUT error)

add_compile_options(-cppless -falt-entry "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
add_link_options(-cppless -falt-entry)

add_executable("lto_example" "main.cpp" "lib.cpp")
target_link_libraries("lto_example" PRIVATE cppless::cppless)
target_compile_features("lto_example" PRIVATE cxx_std_20)
add_custom_target("run_lto_example" COMMAND "./lto_example" VERBATIM)
add_dependencies("run_lto_example" "lto_example")

add_dependencies(run-examples "run_lto_example")

set_property(TARGET "lto_example" PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
