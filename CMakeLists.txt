cmake_minimum_required(VERSION 3.22)
project(PocketTracer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

juce_add_plugin(PocketTracer
    VERSION                     "1.0.0"
    COMPANY_NAME                "Khyle Audio"
    BUNDLE_ID                   "com.khyleaudio.pockettracer"
    PLUGIN_MANUFACTURER_CODE    KhAu
    PLUGIN_CODE                 PkTr
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Pocket Tracer"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx|Delay"
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

juce_generate_juce_header(PocketTracer)

juce_add_binary_data(PocketTracerFonts
    SOURCES
        Assets/Fonts/OpenDyslexic-Regular.otf
        Assets/Fonts/OpenDyslexic-Bold.otf
)

target_sources(PocketTracer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/DelayEngine.cpp
    Source/dsp/DelayEngine.h
    Source/dsp/TiltFilter.cpp
    Source/dsp/TiltFilter.h
    Source/dsp/TempoSync.cpp
    Source/dsp/TempoSync.h
    Source/gui/PocketTracerLookAndFeel.cpp
    Source/gui/PocketTracerLookAndFeel.h
)

target_include_directories(PocketTracer PRIVATE Source)

target_compile_definitions(PocketTracer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PocketTracer
    PRIVATE
        PocketTracerFonts
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
