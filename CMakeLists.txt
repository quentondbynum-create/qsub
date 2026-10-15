cmake_minimum_required(VERSION 3.22)
project(QSub VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pull JUCE automatically — first build downloads it (~150MB), then it's cached.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(QSub
    COMPANY_NAME "Headroom Audio"
    PLUGIN_NAME "QSub"
    DESCRIPTION "808 sub-bass generator: pitch-tracking sub synth for kicks and bass"
    PLUGIN_MANUFACTURER_CODE Hdrm
    PLUGIN_CODE Qsub
    BUNDLE_ID com.headroomaudio.qsub
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "QSub"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
)

juce_add_binary_data(QSubAssets SOURCES Source/Assets/headroom-logo.png)

target_sources(QSub PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(QSub PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(QSub PRIVATE
    QSubAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
