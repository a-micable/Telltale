cmake_minimum_required(VERSION 3.15)
project(telltale VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -pedantic")

file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/telltale/*.hpp")

add_library(telltale STATIC ${SOURCES} ${HEADERS})
target_include_directories(telltale PUBLIC include)

add_executable(telltale-cli src/main.cpp)
target_link_libraries(telltale-cli telltale)

enable_testing()
file(GLOB TEST_SOURCES "tests/test_*.cpp")
foreach(test ${TEST_SOURCES})
    get_filename_component(test_name ${test} NAME_WE)
    add_executable(${test_name} ${test})
    target_link_libraries(${test_name} telltale gtest gtest_main)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
