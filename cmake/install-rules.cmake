if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_INSTALL_INCLUDEDIR include/cppless CACHE PATH "")
endif()

# Project is configured with no languages, so tell GNUInstallDirs the lib dir
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "")

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package cppless)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT cppless_Development
)

install(
    TARGETS cppless_cppless
    EXPORT cpplessTargets
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

# Allow package maintainers to freely override the path for the configs
set(
    cppless_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(cppless_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${cppless_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT cppless_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${cppless_INSTALL_CMAKEDIR}"
    COMPONENT cppless_Development
)

install(
    EXPORT cpplessTargets
    NAMESPACE cppless::
    DESTINATION "${cppless_INSTALL_CMAKEDIR}"
    COMPONENT cppless_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
