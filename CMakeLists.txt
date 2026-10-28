cmake_minimum_required(VERSION 3.22)

set(MALON_VERSION 1.0.0)
project(MalonFrequencyAnalyzer VERSION ${MALON_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(APPLE)
    # universal binary: Apple Silicon + Intel
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

option(MALON_BUILD_TESTS "Build the DSP unit tests and the headless UI tools" ON)

# ---- JUCE 8 (AGPLv3 / open source use). Local checkout if present, otherwise fetched. ----
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory(third_party/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                              GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- 1. DSP core: no JUCE, no platform code ----
add_library(MalonDsp STATIC Source/dsp/FilterBank.cpp)
target_include_directories(MalonDsp PUBLIC Source)
find_package(Threads REQUIRED)
target_link_libraries(MalonDsp PUBLIC Threads::Threads)

# ---- 2. Shared plugin code (processor, editor, UI) ----
add_library(MalonCore STATIC
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/SpectrumView.cpp
    Source/ui/SettingsPanel.cpp)
target_include_directories(MalonCore PUBLIC Source)
target_compile_definitions(MalonCore PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    MALON_VERSION_STRING="${MALON_VERSION}")
target_link_libraries(MalonCore
    PUBLIC  MalonDsp juce::juce_audio_processors juce::juce_gui_basics juce::juce_graphics
    PRIVATE juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(MalonCore PRIVATE /W4 /wd4100)
endif()

# ---- 3. The plugin: VST3 everywhere, AU on macOS ----
if(APPLE)
    set(MALON_FORMATS VST3 AU)
else()
    set(MALON_FORMATS VST3)
endif()

juce_add_plugin(MalonFrequencyAnalyzer
    COMPANY_NAME "MUSICA MALON"
    COMPANY_WEBSITE "https://manymalon.com"
    PLUGIN_MANUFACTURER_CODE Mlon
    PLUGIN_CODE Mfaz
    FORMATS ${MALON_FORMATS}
    PRODUCT_NAME "Malon Frequency Analyzer"
    BUNDLE_ID com.manymalon.frequencyanalyzer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    VERSION ${MALON_VERSION})

target_sources(MalonFrequencyAnalyzer PRIVATE Source/PluginEntry.cpp)
target_compile_definitions(MalonFrequencyAnalyzer PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(MalonFrequencyAnalyzer
    PRIVATE MalonCore juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags)

# ---- 4. Tests and headless tools ----
if(MALON_BUILD_TESTS)
    enable_testing()

    add_executable(MalonDspTests Tests/DspTests.cpp)
    target_link_libraries(MalonDspTests PRIVATE MalonDsp)
    add_test(NAME dsp COMMAND MalonDspTests)

    # processor tests + headless screenshots: console app that links the same code as the plugin
    juce_add_console_app(MalonPluginTests PRODUCT_NAME "MalonPluginTests")
    target_sources(MalonPluginTests PRIVATE Tests/PluginTests.cpp)
    target_compile_definitions(MalonPluginTests PRIVATE
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=1
        MALON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(MalonPluginTests PRIVATE MalonCore juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_test(NAME plugin COMMAND MalonPluginTests --out ${CMAKE_BINARY_DIR}/screenshots)
endif()
