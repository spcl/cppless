project(cpplessServer CXX)

add_executable("cppless-server" "main.cpp")
target_link_libraries("cppless-server" PRIVATE cppless::cppless)
target_compile_features("cppless-server" PRIVATE cxx_std_20)
add_custom_target("run_cppless-server" COMMAND "./cppless-server" VERBATIM)
add_dependencies("run_cppless-server" "cppless-server")