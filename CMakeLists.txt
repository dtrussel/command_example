cmake_minimum_required(VERSION 3.12)
project(command_pattern)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(lightbulb_server src/lightbulb_server.cpp)
target_include_directories(lightbulb_server PRIVATE src/)
target_link_libraries(lightbulb_server ${CONAN_LIBS})
target_compile_features(lightbulb_server PUBLIC cxx_std_17)

add_executable(lightbulb_client src/lightbulb_client.cpp)
target_link_libraries(lightbulb_client ${CONAN_LIBS})
target_compile_features(lightbulb_client PUBLIC cxx_std_17)
