cmake_minimum_required(VERSION 3.9)
project(examples)
link_libraries(usefull_macros)
include_directories(../)

#target_link_libraries(hello -lm)
add_executable(hello helloworld.c)
