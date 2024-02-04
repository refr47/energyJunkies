cmake_minimum_required(VERSION 3.16)
project(energy_logger C)

set(CMAKE_C_STANDARD 11)

add_executable(energy_logger energy_logger.c)

target_link_libraries(energy_logger m modbus json-c)
