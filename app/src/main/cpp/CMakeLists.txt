cmake_minimum_required(VERSION 3.22.1)
project(btmid)

find_package(oboe REQUIRED CONFIG)

add_library(btmid SHARED
        NativeEngine.cpp
        SineTestRenderer.cpp
        renderers/NoiseDrumRenderer.cpp
        jni_bridge.cpp
)

target_link_libraries(btmid
        oboe::oboe
        log
        android
)
