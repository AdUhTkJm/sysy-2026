cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_COMPILER clang++)
project(hcc LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  src/*.cpp)

add_executable(hcc ${SRC_FILES})

target_compile_options(hcc PRIVATE
  -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct -Wno-nested-anon-types
  -Wno-variadic-macro-arguments-omitted -Wno-zero-length-array
  -fno-exceptions -fno-rtti -fdiagnostics-color=always -fsanitize=undefined,address
  -g -std=c++17
)
target_link_options(hcc PRIVATE
  -fsanitize=undefined,address
)
