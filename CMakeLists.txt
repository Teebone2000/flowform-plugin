cmake_minimum_required(VERSION 3.15)
project(FlowForm VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_ROOT "$ENV{HOME}/Library/JUCE" CACHE PATH "Path to JUCE")
if(NOT EXISTS ${JUCE_ROOT})
    message(FATAL_ERROR "JUCE not found at ${JUCE_ROOT}")
endif()

add_subdirectory(${JUCE_ROOT} JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(FlowForm
    VERSION 1.0.0
    COMPANY_NAME "DigitalCapers"
    BUNDLE_ID "com.digitalcapers.flowform"
    PLUGIN_MANUFACTURER_CODE Dcap
    PLUGIN_CODE FlwF
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "FlowForm"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(FlowForm)

target_sources(FlowForm PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ScopeComponent.cpp
    Source/DSP/CompressorProcessor.cpp
    Source/DSP/DriveEngine.cpp
    Source/DSP/HarmonicProcessor.cpp
    Source/DSP/LimiterProcessor.cpp
    Source/DSP/ClipperProcessor.cpp
    Source/DSP/ScaleSystem.cpp
    Source/DSP/MeterProcessor.cpp
)

target_compile_definitions(FlowForm PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(FlowForm PRIVATE
    "${CMAKE_SOURCE_DIR}/Source"
    "/Users/invisible/Shared"
)

target_link_libraries(FlowForm PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
