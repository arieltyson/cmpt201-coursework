cmake_minimum_required(VERSION 3.22)

project(
  Lab8
  VERSION 1.0
  DESCRIPTION "Threaded word count with uthash"
  LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
# Note: ASan and TSan are incompatible
# Address sanitization:
add_compile_options(-fsanitize=thread -g -O1)
add_link_options(-fsanitize=thread -g -O1)
# --------- Task 3 ---------
# Enable the thread sanitizer
add_compile_options(-pthread)
add_link_options(-pthread)
add_executable(main src/lab8.c)
target_include_directories(main PRIVATE include)
