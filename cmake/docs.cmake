# ---- Dependencies ----

include(FetchContent)
FetchContent_Declare(
    mcss URL
    https://github.com/mosra/m.css/archive/a0d292ec311b97fefd21e93cdefb60f88d19ede6.zip
    URL_MD5 d41d8cd98f00b204e9800998ecf8427e
    SOURCE_DIR "${PROJECT_BINARY_DIR}/mcss"
    UPDATE_DISCONNECTED YES
)
FetchContent_MakeAvailable(mcss)

find_package(Python3 3.6 REQUIRED)

# ---- Declare documentation target ----

set(
    DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/docs"
    CACHE PATH "Path for the generated Doxygen documentation"
)

set(working_dir "${PROJECT_BINARY_DIR}/docs")

foreach(file IN ITEMS Doxyfile conf.py)
  configure_file("docs/${file}.in" "${working_dir}/${file}" @ONLY)
endforeach()

set(mcss_script "${mcss_SOURCE_DIR}/documentation/doxygen.py")
set(config "${working_dir}/conf.py")

add_custom_target(
    docs
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
    "${DOXYGEN_OUTPUT_DIRECTORY}/html"
    "${DOXYGEN_OUTPUT_DIRECTORY}/xml"
    COMMAND "${Python3_EXECUTABLE}" "${mcss_script}" "${config}"
    COMMENT "Building documentation using Doxygen and m.css"
    WORKING_DIRECTORY "${working_dir}"
    VERBATIM
)
