cmake_minimum_required(VERSION 3.22)

# Windows: link the C++ runtime statically so the plugin runs without
# installing the Visual C++ Redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# macOS: build for both Apple Silicon and Intel.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(ChannelStrip VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ChannelStrip
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE Chst
    FORMATS VST3 Standalone
    PRODUCT_NAME "My Channel Strip"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(ChannelStrip PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(ChannelStrip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ChannelStrip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Build:
#   sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev \
#     libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
#     libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
# The VST3 ends up in build/ChannelStrip_artefacts/Release/VST3/
# Copy it to ~/.vst3/ for your DAW to find it.
