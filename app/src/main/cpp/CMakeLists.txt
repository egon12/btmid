cmake_minimum_required(VERSION 3.22.1)
project(btmid)

find_package(oboe REQUIRED CONFIG)

add_subdirectory(opus)

add_library(btmid SHARED
	MidiParser.cpp
	LoopRecorder.cpp
	MidiEngine.cpp
	outputs/OboeEngine.cpp
	outputs/WifiEngine.cpp
	InstrumentRepository.cpp
	AudioGraph.cpp
	instruments/Piano.cpp
	instruments/NoiseDrum.cpp
	instruments/FmDrum.cpp
	instruments/SampleDrum.cpp
	instruments/PianoSinTable.cpp
	instruments/PolyOscillator.cpp
	instruments/MonoOscillator.cpp
	PianoBenchmark.cpp
	jni_bridge.cpp
)

target_include_directories(btmid PRIVATE
	opus/include
)


target_link_libraries(btmid
	oboe::oboe
	opus
	log
	android
	amidi
)
