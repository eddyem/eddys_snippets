cmake_minimum_required(VERSION 3.9)
project(myapp)
add_executable(myapp helloworld.c)
include_directories(../)
target_link_libraries(myapp usefull_macros)

