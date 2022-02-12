cmake_minimum_required(VERSION 3.21)
project(ulang-vm)

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 -D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 -D_CRT_SECURE_NO_WARNINGS")

    if (CMAKE_BUILD_TYPE MATCHES Release)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif ()

    if (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /PROFILE")
    endif ()

    message("MSVC CXX Flags: ${CMAKE_CXX_FLAGS}")
    message("MSVC CXX Flags Release: ${CMAKE_CXX_FLAGS_RELEASE}")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c99 -Wno-missing-field-initializers")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11 -Wno-missing-field-initializers")

    if (CMAKE_BUILD_TYPE MATCHES Release)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fno-exceptions -fno-rtti")
    endif ()
    message("Clang CXX Flags: ${CMAKE_CXX_FLAGS}")
    message("Clang CXX Flags Release: ${CMAKE_CXX_FLAGS_RELEASE}")
endif ()

set(CMAKE_CXX_STANDARD 11)

include_directories(src)
file(GLOB INCLUDES "src/*.h")
file(GLOB SOURCES "src/*.c")
add_library(ulang-lib ${INCLUDE} ${SOURCES})
set_target_properties(ulang-lib PROPERTIES OUTPUT_NAME "ulang-lib")

add_executable(ulang ${INCLUDES} "src/apps/ulang.cpp")
target_link_libraries(ulang LINK_PUBLIC ulang-lib)

add_executable(test ${INCLUDES} "src/apps/test.cpp")
target_link_libraries(test LINK_PUBLIC ulang-lib)