cmake_minimum_required(VERSION 2.6)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-std=c++1y -fPIC -O3 -Werror -Wall -Wpedantic)

project(fmtlib)
file(GLOB SOURCES "ext/fmtlib/*.cc")
add_library(fmt STATIC ${SOURCES})

project(gmock)
include_directories(ext/gmock)
include_directories(ext/gtest)
add_library(gmock STATIC ext/gtest/src/gtest-all.cc
                         ext/gmock/src/gmock-all.cc
                         ext/gmock/src/gmock_main.cc)

project(static_lib)
include_directories(ext)
include_directories(include)
file(GLOB SOURCES "src/*.cc")
add_library(yam_http_static STATIC ${SOURCES})
target_link_libraries(yam_http_static fmt curl pthread unwind)

project(shared_lib)
add_library(yam_http SHARED "")
target_link_libraries(yam_http yam_http_static)

project(example_hello_world)
file(GLOB SOURCES "examples/*.cc")
add_executable(hello_world ${SOURCES})
target_link_libraries(hello_world yam_http)

project(sandbox_echo)
add_executable(sandbox_echo "sandbox/echo.cc")
target_link_libraries(sandbox_echo yam_http)

project(tests)
include_directories(test)
file(GLOB SOURCES "test/*.cc")
add_executable(tests ${SOURCES})
target_link_libraries(tests yam_http gmock)
