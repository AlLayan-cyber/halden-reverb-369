cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build Universal Binary on macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

project(HaldenReverb369 VERSION 1.2.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HALDEN_BUILD_TESTS "Build the offline DSP test runner" OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HaldenReverb369
    COMPANY_NAME "Moon Music Label"
    BUNDLE_ID "com.moonmusiclabel.haldenreverb369"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MoMu
    PLUGIN_CODE Hr69
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Halden Reverb 369"
)

juce_generate_juce_header(HaldenReverb369)

target_sources(HaldenReverb369
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP.h
)

target_compile_definitions(HaldenReverb369
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(HaldenReverb369
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(HALDEN_BUILD_TESTS)
    juce_add_console_app(HaldenTests PRODUCT_NAME "HaldenTests")
    target_sources(HaldenTests PRIVATE Tests/TestMain.cpp)
    target_include_directories(HaldenTests PRIVATE Source ${CMAKE_CURRENT_BINARY_DIR}/HaldenReverb369_artefacts/JuceLibraryCode)
    target_compile_definitions(HaldenTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(HaldenTests PRIVATE HaldenReverb369 juce::juce_audio_utils juce::juce_dsp
                          juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)


    juce_add_console_app(FxLab PRODUCT_NAME "FxLab")
    target_sources(FxLab PRIVATE Tests/FxLab.cpp)
    target_include_directories(FxLab PRIVATE Source ${CMAKE_CURRENT_BINARY_DIR}/HaldenReverb369_artefacts/JuceLibraryCode)
    target_compile_definitions(FxLab PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FxLab PRIVATE HaldenReverb369 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

endif()
