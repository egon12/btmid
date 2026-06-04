cmake_minimum_required(VERSION 3.22.1)
project(btmid)

find_package(oboe REQUIRED CONFIG)

add_library(btmid SHARED
        MidiParser.cpp
        NativeEngine.cpp
        instruments/Piano.cpp
        instruments/NoiseDrum.cpp
        instruments/FmDrum.cpp
        instruments/SampleDrum.cpp
        instruments/PianoSinTable.cpp
        jni_bridge.cpp
)

target_link_libraries(btmid
        oboe::oboe
        log
        android
        amidi
)
