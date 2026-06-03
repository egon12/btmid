cmake_minimum_required(VERSION 3.16)
project(btmid_renderer_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp)

add_executable(renderer_test
    renderer_test.cpp
    ${ENGINE_SRC}/SineTestRenderer.cpp
    ${ENGINE_SRC}/renderers/NoiseDrumRenderer.cpp
)

target_include_directories(renderer_test PRIVATE ${ENGINE_SRC})
