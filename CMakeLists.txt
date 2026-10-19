cmake_minimum_required(VERSION 3.22)

project(MultiSqueeze VERSION 0.1.11)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS target" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(MultiSqueezeAssets SOURCES
    Assets/UI-v2/Raster/background-v2.png
    Assets/UI-v2/Raster/knob-v2.png
    Assets/bar-off.png
    Assets/bar-on.png
    Assets/rect-off.png
    Assets/rect-on.png
    Assets/moth_logo.png
    Assets/Fonts/Oxanium-Bold.ttf
    Assets/Fonts/BarlowCondensed-Light.ttf
    Assets/Fonts/BarlowCondensed-Medium.ttf
    Assets/Fonts/Rajdhani-Medium.ttf
)

juce_add_plugin(MultiSqueeze
    COMPANY_NAME "Moth Production"
    BUNDLE_ID "com.mothproduction.multisqueeze"
    PLUGIN_MANUFACTURER_CODE MthP
    PLUGIN_CODE MSq1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "MultiSqueeze"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
)

# Keep JUCE default output paths for consistent single/multi-config builds.
target_sources(MultiSqueeze PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BandSplitter.cpp
    Source/CompressorBand.cpp
    Source/SpectrumAnalyzer.cpp
    Source/MultiSqueezeLookAndFeel.cpp
    Source/MultiSqueezeFonts.cpp
)

target_compile_definitions(MultiSqueeze PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(MultiSqueeze
    PRIVATE juce::juce_audio_utils juce::juce_dsp MultiSqueezeAssets
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags
)

option(MULTISQUEEZE_BUILD_TESTS "Build DSP unit tests" ON)
if(MULTISQUEEZE_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(MultiSqueezeTests PRODUCT_NAME "MultiSqueeze Tests")
    target_sources(MultiSqueezeTests PRIVATE Tests/DspTests.cpp Source/CompressorBand.cpp Source/BandSplitter.cpp)
    target_link_libraries(MultiSqueezeTests PRIVATE juce::juce_dsp juce::juce_audio_basics
                          PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME MultiSqueezeDSP COMMAND MultiSqueezeTests)
    juce_add_console_app(MultiSqueezeProcessorTests PRODUCT_NAME "MultiSqueeze Processor Tests")
    target_sources(MultiSqueezeProcessorTests PRIVATE Tests/ProcessorTests.cpp)
    target_link_libraries(MultiSqueezeProcessorTests PRIVATE MultiSqueeze)
    target_include_directories(MultiSqueezeProcessorTests PRIVATE $<TARGET_PROPERTY:MultiSqueeze,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MultiSqueezeProcessorTests PRIVATE $<TARGET_PROPERTY:MultiSqueeze,COMPILE_DEFINITIONS>)
    add_test(NAME MultiSqueezeProcessor COMMAND MultiSqueezeProcessorTests)
endif()

option(MULTISQUEEZE_LAYOUT_TUNER "Enable the interactive JUCE UI layout tuner" OFF)
if(MULTISQUEEZE_LAYOUT_TUNER)
    target_compile_definitions(MultiSqueeze PRIVATE JUCE_LAYOUT_TUNER=1)
endif()
