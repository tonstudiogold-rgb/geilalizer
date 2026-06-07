cmake_minimum_required(VERSION 3.22)

project(Geilalizer
    VERSION 0.1.0
    DESCRIPTION "24-track standalone tape-machine-style audio app"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(Geilalizer
    PRODUCT_NAME "24-Track Standalone Geilalizer"
    COMPANY_NAME "Geilalizer"
    BUNDLE_ID "com.geilalizer.standalone"
    VERSION "${PROJECT_VERSION}"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "24-Track Standalone Geilalizer needs audio input access for armed tracks."
    DOCUMENT_EXTENSIONS wav aiff aif flac mp3
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

target_sources(Geilalizer
    PRIVATE
        src/App.cpp
        src/App.h
        src/MainComponent.cpp
        src/MainComponent.h
        src/core/AudioConstants.h
        src/core/AudioEngine.cpp
        src/core/AudioEngine.h
        src/core/ChannelState.h
        src/core/RenderEngine.cpp
        src/core/RenderEngine.h
        src/core/SessionState.h
        src/dsp/AudioBlock.h
        src/dsp/HiddenIrAdapter.cpp
        src/dsp/HiddenIrAdapter.h
        src/dsp/HiddenNamAdapter.h
        src/dsp/MachineProcessor.cpp
        src/dsp/MachineProcessor.h
        src/dsp/SafetyLimiter.h)

target_compile_definitions(Geilalizer
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Geilalizer,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)

target_link_libraries(Geilalizer
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(APPLE)
    set_target_properties(Geilalizer PROPERTIES
        XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20"
        XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "12.0")
endif()

add_executable(CoreSmoke
    tests/CoreSmoke.cpp
    src/core/AudioEngine.cpp
    src/core/RenderEngine.cpp
    src/dsp/HiddenIrAdapter.cpp
    src/dsp/MachineProcessor.cpp)

target_include_directories(CoreSmoke PRIVATE src)
target_compile_features(CoreSmoke PRIVATE cxx_std_20)

enable_testing()
add_test(NAME CoreSmoke COMMAND CoreSmoke)
