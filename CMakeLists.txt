cmake_minimum_required(VERSION 2.6)

get_directory_property(hasParent PARENT_DIRECTORY)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (hasParent)
    set(http_INCLUDE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ext
        ${CMAKE_CURRENT_SOURCE_DIR}/ext/std
        PARENT_SCOPE)
endif()

add_compile_options(-std=c++1z -fPIC -O3 -Werror -Wall -Wpedantic -isystem ext/std)

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
add_library(chili_static STATIC ${SOURCES})
target_link_libraries(chili_static fmt curl pthread unwind)

project(shared_lib)
add_library(chili SHARED "")
target_link_libraries(chili chili_static)

project(example_hello_world)
file(GLOB SOURCES "examples/hello_world.cc")
add_executable(hello_world ${SOURCES})
target_link_libraries(hello_world chili)

project(example_streamer)
file(GLOB SOURCES "examples/streamer.cc")
add_executable(streamer ${SOURCES})
target_link_libraries(streamer chili)

project(example_routed)
file(GLOB SOURCES "examples/routed.cc")
add_executable(routed ${SOURCES})
target_link_libraries(routed chili)

project(sandbox_echo)
add_executable(sandbox_echo "sandbox/echo.cc")
target_link_libraries(sandbox_echo chili)

project(tests)
include_directories(test)
file(GLOB SOURCES "test/*.cc")
add_executable(tests ${SOURCES})
target_link_libraries(tests chili gmock)
