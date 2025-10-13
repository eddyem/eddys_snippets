cmake_minimum_required(VERSION 3.9)

include_directories(..)
link_libraries(i2csensorsPTH usefull_macros -lm)

add_executable(single single_sensor.c)
add_executable(log logmany.c)
#target_link_libraries(single i2csensorsPTH usefull_macros)
