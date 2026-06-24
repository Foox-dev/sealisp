cmake_minimum_required(VERSION 3.16)
project(sealisp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

add_executable(sealisp
  src/main.c
  src/value.c
  src/env.c
  src/reader.c
  src/printer.c
  src/eval.c
  src/builtins.c
)
