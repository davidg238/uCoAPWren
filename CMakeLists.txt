cmake_minimum_required(VERSION 3.17)
project(uCoAPWren C)

set(CMAKE_C_STANDARD 99)

add_executable(uCoAPWren main.c endpoints.c coap.c coap.h wrenAll.c wren.h)
target_link_libraries(uCoAPWren m)