project(cpplessServer CXX)

add_executable("cppless_server" "main.cpp")
target_link_libraries("cppless_server" PRIVATE cppless::cppless)
target_compile_features("cppless_server" PRIVATE cxx_std_20)
add_custom_target("run_cppless_server" COMMAND "./cppless_server" VERBATIM)
add_dependencies("run_cppless_server" "cppless_server")