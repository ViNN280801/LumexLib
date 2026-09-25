cmake_minimum_required(VERSION 3.16)
project(LumexLibTestPackage LANGUAGES CXX)

find_package(LumexLib REQUIRED CONFIG)

add_executable(lumex_consumer main.cpp)
target_link_libraries(lumex_consumer PRIVATE lumex::fmt lumex::string lumex::xml)
target_compile_features(lumex_consumer PRIVATE cxx_std_17)
