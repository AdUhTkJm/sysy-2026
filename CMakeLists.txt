cmake_minimum_required(VERSION 3.20)
project(hcc LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  src/*.cpp)

add_executable(hcc ${SRC_FILES})

target_compile_options(hcc PRIVATE
  -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -g
)
