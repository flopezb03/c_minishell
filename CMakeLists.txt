cmake_minimum_required(VERSION 3.10)
project(MiniShell C)

set(CMAKE_C_STANDARD 11)

include_directories(lib)

set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(MiniShell main.c)

# Enlaza directamente el archivo .a como argumento
target_link_libraries(MiniShell ${CMAKE_SOURCE_DIR}/lib/libparser.a)
