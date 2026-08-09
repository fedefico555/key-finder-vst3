cmake_minimum_required(VERSION 3.22)

project(KeyFinderVST3 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE: viene scaricato automaticamente da GitHub al primo "cmake --build".
# Se preferisci usare una copia locale di JUCE, commenta il blocco
# FetchContent e usa invece: add_subdirectory(percorso/a/JUCE JUCE)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(KeyFinderVST3
    COMPANY_NAME "TuoNome"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Tuon
    PLUGIN_CODE Kfvs
    FORMATS VST3 Standalone
    PRODUCT_NAME "Key Finder"
)

juce_generate_juce_header(KeyFinderVST3)

target_sources(KeyFinderVST3
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/KeyDetector.cpp
        Source/CamelotWheel.cpp
)

target_compile_definitions(KeyFinderVST3
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(KeyFinderVST3
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
