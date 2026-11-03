cmake_minimum_required(VERSION 3.22)

project(Grinder VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GRINDER_BUILD_TESTS "Build the headless DSP unit tests" ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap-juce-extensions EXCLUDE_FROM_ALL)

juce_add_plugin(Grinder
    COMPANY_NAME "Grinder Audio"
    PLUGIN_MANUFACTURER_CODE Gnda
    PLUGIN_CODE Grnd
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Grinder"
    BUNDLE_ID "com.bonystoev.Grinder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

clap_juce_extensions_plugin(
    TARGET Grinder
    CLAP_ID "com.bonystoev.Grinder"
    CLAP_FEATURES audio-effect distortion
)

target_sources(Grinder PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/GrinderLookAndFeel.cpp
    src/GrinderComponents.cpp
)

target_include_directories(Grinder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp
)

target_compile_definitions(Grinder PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Grinder PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_dsp
    clap_juce_extensions
)

if(GRINDER_BUILD_TESTS)
    add_executable(grinder_tests tests/test_dsp.cpp)
    target_include_directories(grinder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dsp)
    target_compile_options(grinder_tests PRIVATE -O2 -Wall -Wextra)
    enable_testing()
    add_test(NAME grinder_dsp COMMAND grinder_tests)
endif()
