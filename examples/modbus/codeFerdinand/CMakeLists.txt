cmake_minimum_required(VERSION 3.16)
project(energy_mgr C)

set(CMAKE_C_STANDARD 11)

add_executable(energy_mgr energy-mgr.c)

target_link_libraries(energy_mgr m modbus)
