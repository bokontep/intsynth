cmake_minimum_required(VERSION 3.10)

project(intsynth)
find_library(JACK_LIB jack)


add_executable(intsynth src/client.cpp src/ADSR.cpp src/Util.cpp)

target_link_libraries(intsynth jack pthread)