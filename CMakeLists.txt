cmake_minimum_required(VERSION 3.21)
project(favorite-levels VERSION 1.0.0)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

find_package(Geode REQUIRED)
setup_geode_mod(${PROJECT_NAME})
