cmake_minimum_required(VERSION 3.25)
project(DesktopAudioAnalyzer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DAA_BUILD_APP "Build the Windows/Qt application" ON)
option(DAA_BUILD_TESTS "Build deterministic DSP tests" ON)

add_library(daa_dsp
    src/dsp/analysis.cpp
    src/dsp/fft.cpp
    src/dsp/wav_reader.cpp
)
target_include_directories(daa_dsp PUBLIC src)
if(MSVC)
    target_compile_options(daa_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(daa_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DAA_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    add_executable(daa_dsp_tests tests/dsp_tests.cpp)
    target_link_libraries(daa_dsp_tests PRIVATE daa_dsp Threads::Threads)
    add_test(NAME dsp COMMAND daa_dsp_tests)
endif()

if(DAA_BUILD_APP)
    if(NOT WIN32)
        message(FATAL_ERROR "The initial capture backend requires Windows. Use -DDAA_BUILD_APP=OFF for DSP tests.")
    endif()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.8)
    qt_add_executable(desktop_audio_analyzer
        src/app/main.cpp
        src/app/audio_engine.cpp src/app/audio_engine.h
        src/app/plot_item.cpp src/app/plot_item.h
        src/app/workspace.cpp src/app/workspace.h
        src/capture/wasapi_capture.cpp src/capture/wasapi_capture.h
    )
    qt_add_resources(desktop_audio_analyzer qml PREFIX "/" FILES
        qml/Main.qml qml/AnalyzerWindow.qml)
    target_compile_definitions(desktop_audio_analyzer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(desktop_audio_analyzer PRIVATE
        daa_dsp Qt6::Quick Qt6::QuickControls2 ole32 uuid avrt)
    set_target_properties(desktop_audio_analyzer PROPERTIES
        WIN32_EXECUTABLE TRUE OUTPUT_NAME "DesktopAudioAnalyzer")
    install(TARGETS desktop_audio_analyzer RUNTIME DESTINATION .)
endif()
