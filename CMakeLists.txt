cmake_minimum_required(VERSION 3.21.1)
project(ulang-vm)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

include(FetchContent)
FetchContent_Declare(minifb GIT_REPOSITORY https://github.com/emoon/minifb)
FetchContent_MakeAvailable(minifb)

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 -D_CRT_SECURE_NO_WARNINGS")

    if (CMAKE_BUILD_TYPE MATCHES Release)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    endif ()

    if (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /PROFILE")
    endif ()

    message("MSVC C Flags: ${CMAKE_C_FLAGS}")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wno-c11-extensions -Wno-missing-field-initializers")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")

    if (CMAKE_BUILD_TYPE MATCHES Release)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    endif ()
    message("Clang C Flags: ${CMAKE_C_FLAGS}")
    message("Clang C Flags Release: ${CMAKE_C_FLAGS_RELEASE}")
endif ()

include_directories(src)
file(GLOB INCLUDES "src/*.h")
file(GLOB SOURCES "src/*.c")
add_library(ulang-lib ${INCLUDE} ${SOURCES})
set_target_properties(ulang-lib PROPERTIES OUTPUT_NAME "ulang-lib")

add_executable(ulang-vm ${INCLUDES} "src/apps/ulang-vm.c")
target_link_libraries(ulang-vm LINK_PUBLIC ulang-lib minifb)

add_executable(test ${INCLUDES} "src/apps/test.c")
target_link_libraries(test LINK_PUBLIC ulang-lib)
