cmake_minimum_required(VERSION 3.22.1)
project(btmid)

find_package(oboe REQUIRED CONFIG)

# TinySoundFont (MIT) — single-header SoundFont synth, fetched at configure time
# rather than vendored into the repo.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tsf.h")
	file(DOWNLOAD
		"https://raw.githubusercontent.com/schellingb/TinySoundFont/master/tsf.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/tsf.h"
		SHOW_PROGRESS
	)
endif()

add_subdirectory(opus)

add_library(btmid SHARED
	MidiParser.cpp
	LoopRecorder.cpp
	Metronome.cpp
	MidiEngine.cpp
	UICallback.cpp
	outputs/OboeOutput.cpp
	outputs/WifiOutput.cpp
	InstrumentRepository.cpp
	AudioGraph.cpp
	instruments/Piano.cpp
	instruments/NoiseDrum.cpp
	instruments/FmDrum.cpp
	instruments/SampleDrum.cpp
	instruments/PianoSinTable.cpp
	instruments/PolyOscillator.cpp
	instruments/MonoOscillator.cpp
	instruments/SfInstrument.cpp
	PianoBenchmark.cpp
	jni_bridge.cpp
)

target_include_directories(btmid PRIVATE
	opus/include
	${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(btmid
	oboe::oboe
	opus
	log
	android
	amidi
)
