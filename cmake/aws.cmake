set(AWS_LAMBDA_PACKAGING_SCRIPT "${CMAKE_SOURCE_DIR}/tools/packagerpy/packager.py" CACHE FILEPATH "")
function(aws_lambda_package_target target)
    if (NOT CPPLESS_SERVERLESS)
        return()
    endif()
    set(options OPTIONAL NO_LIBC)
    cmake_parse_arguments(PACKAGER "${options}" "" "" ${ARGN})
    if (${PACKAGER_NO_LIBC})
        set (PACKAGER_NO_LIBC "--no-libc")
    else()
        set (PACKAGER_NO_LIBC "--libc")
    endif()

    if (DEFINED SYSROOT)
      set (PACKAGER_SYROOT "--sysroot" ${SYSROOT})
    else()
      set (PACKAGER_SYROOT)
    endif()

    if (${DOCKER_IMAGE})
        set (PACKAGER_IMAGE "--image" ${DOCKER_IMAGE})
    else()
        set (PACKAGER_IMAGE)
    endif()

    if("${CPPLESS_TOOLCHAIN}" STREQUAL "aarch64")
      add_custom_target("aws_lambda_package_${target}"
          COMMAND ${AWS_LAMBDA_PACKAGING_SCRIPT} ${PACKAGER_NO_LIBC}
          "--project" ${CMAKE_BINARY_DIR}
          "--project-source" ${CMAKE_SOURCE_DIR}
          ${PACKAGER_SYROOT}
          ${PACKAGER_IMAGE}
          "--target-name" ${target}
          "--deploy"
          "--architecture" "aarch64"
          $<TARGET_FILE:${target}>
          DEPENDS ${target})
    else()
      add_custom_target("aws_lambda_package_${target}"
          COMMAND ${AWS_LAMBDA_PACKAGING_SCRIPT} ${PACKAGER_NO_LIBC}
          "--project" ${CMAKE_BINARY_DIR}
          ${PACKAGER_SYROOT}
          ${PACKAGER_IMAGE}
          "--target-name" ${target}
          "--strip"
          "--deploy"
          $<TARGET_FILE:${target}>
          DEPENDS ${target})
    endif()
endfunction()

function(aws_lambda_target NAME)
    # Cppless options
    if (CPPLESS_SERVERLESS)
        target_compile_options("${NAME}" PRIVATE -cppless -falt-entry "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
        target_link_options("${NAME}" PRIVATE -cppless -falt-entry)
    else()
        target_compile_options("${NAME}" PRIVATE -cppless "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
        target_link_options("${NAME}" PRIVATE -cppless)
    endif()

    target_compile_features("${NAME}" PRIVATE cxx_std_20)
    target_compile_options("${NAME}" PRIVATE "-Wall" "-Wextra")
    
    # Link libraries
    fetchContent_MakeAvailable(aws-lambda-cpp)
    target_link_libraries("${NAME}" PRIVATE aws-lambda-runtime)
    target_link_libraries("${NAME}" PRIVATE cppless::cppless)

    if (DEFINED CPPLESS_STATIC_LINKAGE)
        target_link_libraries("${NAME}" PRIVATE -static-libgcc)
    endif()

    find_package(CURL REQUIRED)
    target_link_libraries("${NAME}" PRIVATE CURL::libcurl)

    target_compile_definitions("${NAME}" PRIVATE "TARGET_NAME=\"${NAME}\"")
endfunction()


function(aws_lambda_serverless_target NAME)
    if(DEFINED CPPLESS_SERVERLESS)
        aws_lambda_package_target("${NAME}")
    else()
        include(ExternalProject)
        ExternalProject_Add(serverless-${NAME}
            SOURCE_DIR "${CMAKE_SOURCE_DIR}"
            BUILD_ALWAYS ON
            CMAKE_ARGS
                -DCPPLESS_SERVERLESS=ON
                -DCPPLESS_TOOLCHAIN=${CPPLESS_TOOLCHAIN}
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchains/${CPPLESS_TOOLCHAIN}/toolchain.cmake
                -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            INSTALL_COMMAND true
            BUILD_COMMAND cmake --build . --target aws_lambda_package_${NAME}
            )
        # Add custom target which depends on "$NAME" and "${PROJECT_NAME}-aws-lambda-serverless"
        add_custom_target("serverless_${NAME}" COMMAND "true")
        add_dependencies("serverless_${NAME}" serverless-${NAME})
        add_dependencies("serverless_${NAME}" ${NAME})


        add_custom_target("run_${NAME}" COMMAND "./${NAME}" VERBATIM)
        add_dependencies("run_${NAME}" "${NAME}")

        add_custom_target("run_serverless_${NAME}" COMMAND "./${NAME}" VERBATIM)
        add_dependencies("run_serverless_${NAME}" "serverless_${NAME}")
    endif()
endfunction()
