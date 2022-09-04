# Copyright (c) 2022  Zubax Robotics  <info@zubax.com>
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>

cmake_minimum_required(VERSION 3.20)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

project(crc_collider CXX)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
-fno-rtti -fno-exceptions \
-Werror -Wall -Wextra -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef -Wconversion -Wtype-limits \
-Wsign-conversion -Wcast-align -Wmissing-declarations -Wzero-as-null-pointer-constant -Wnon-virtual-dtor \
-Woverloaded-virtual -Wsign-promo -Wold-style-cast \
-Wno-error=attributes \
")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native -fomit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -march=native -mtune=native")
set(CMAKE_VERBOSE_MAKEFILE ON)
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

add_executable(crc_collider crc_collider.cpp)
target_link_libraries(crc_collider pthread)

add_executable(composer composer.cpp)
