cmake_minimum_required(VERSION 3.14...3.28)
project(presence_cpp_ffi)

add_library(rust_to_c rust_to_c.cpp)
install(TARGETS rust_to_c)