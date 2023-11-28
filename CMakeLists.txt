cmake_minimum_required(VERSION 3.12)

project(wg2nat LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(wireguard)
add_subdirectory(src)
