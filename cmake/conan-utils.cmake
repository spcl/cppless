
macro(conan_utils_host_system_name result)
    #handle -s os setting
    if(CMAKE_HOST_SYSTEM_NAME AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Generic")
        #use default conan os setting if CMAKE_SYSTEM_NAME is not defined
        set(CONAN_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})
        if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
            set(CONAN_SYSTEM_NAME Macos)
        endif()
        if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "QNX")
            set(CONAN_SYSTEM_NAME Neutrino)
        endif()
        set(CONAN_SUPPORTED_PLATFORMS Windows Linux Macos Android iOS FreeBSD WindowsStore WindowsCE watchOS tvOS FreeBSD SunOS AIX Arduino Emscripten Neutrino)
        list (FIND CONAN_SUPPORTED_PLATFORMS "${CONAN_SYSTEM_NAME}" _index)
        if (${_index} GREATER -1)
            #check if the cmake system is a conan supported one
            set(${result} ${CONAN_SYSTEM_NAME})
        else()
            message(FATAL_ERROR "cmake system ${CONAN_SYSTEM_NAME} is not supported by conan. Use one of ${CONAN_SUPPORTED_PLATFORMS}")
        endif()
    endif()
endmacro()