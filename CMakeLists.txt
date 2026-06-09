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
option(GEILALIZER_ENABLE_NAMCORE "Build hidden NAM stages with NeuralAmpModelerCore" ON)
option(GEILALIZER_BUILD_JUCE_APP "Build the JUCE standalone GUI app" ON)

if(GEILALIZER_BUILD_JUCE_APP)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(GEILALIZER_ENABLE_NAMCORE)
    FetchContent_Declare(
        NeuralAmpModelerCore
        GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(NeuralAmpModelerCore)

    file(GLOB GEILALIZER_NAMCORE_SOURCES
        "${neuralampmodelercore_SOURCE_DIR}/NAM/*.cpp"
        "${neuralampmodelercore_SOURCE_DIR}/NAM/*.c"
        "${neuralampmodelercore_SOURCE_DIR}/NAM/*/*.cpp")

    add_library(GeilalizerNamCore STATIC ${GEILALIZER_NAMCORE_SOURCES})
    target_compile_features(GeilalizerNamCore PUBLIC cxx_std_20)
    target_compile_definitions(GeilalizerNamCore PUBLIC NAM_SAMPLE_FLOAT)
    target_include_directories(GeilalizerNamCore
        PUBLIC
            "${neuralampmodelercore_SOURCE_DIR}"
            "${neuralampmodelercore_SOURCE_DIR}/NAM"
            "${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen"
            "${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann")
endif()

if(GEILALIZER_BUILD_JUCE_APP)
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
        src/core/ImportPlanner.h
        src/core/LevelMeter.h
        src/core/LinearResampler.h
        src/core/RenderEngine.cpp
        src/core/RenderEngine.h
        src/core/SessionState.h
        src/dsp/AssetPath.h
        src/dsp/AudioBlock.h
        src/dsp/HiddenIrAdapter.cpp
        src/dsp/HiddenIrAdapter.h
        src/dsp/HiddenMixbusIrAdapter.cpp
        src/dsp/HiddenMixbusIrAdapter.h
        src/dsp/HiddenNamAdapter.cpp
        src/dsp/HiddenNamAdapter.h
        src/dsp/HiddenPostFaderIrAdapter.cpp
        src/dsp/HiddenPostFaderIrAdapter.h
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

    if(EXISTS "${CMAKE_SOURCE_DIR}/private_nam_models")
        if(APPLE)
            add_custom_command(TARGET Geilalizer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/private_nam_models"
                    "$<TARGET_BUNDLE_DIR:Geilalizer>/Contents/Resources/private_nam_models")
        else()
            add_custom_command(TARGET Geilalizer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/private_nam_models"
                    "$<TARGET_FILE_DIR:Geilalizer>/private_nam_models")
        endif()
    endif()
endif()

add_executable(CoreSmoke
    tests/CoreSmoke.cpp
    src/core/AudioEngine.cpp
    src/core/RenderEngine.cpp
    src/dsp/HiddenIrAdapter.cpp
    src/dsp/HiddenMixbusIrAdapter.cpp
    src/dsp/HiddenNamAdapter.cpp
    src/dsp/HiddenPostFaderIrAdapter.cpp
    src/dsp/MachineProcessor.cpp)

add_executable(NamAdapterSmoke
    tests/NamAdapterSmoke.cpp
    src/dsp/HiddenNamAdapter.cpp)

add_executable(ProductE2E
    tests/ProductE2E.cpp
    src/core/AudioEngine.cpp
    src/core/RenderEngine.cpp
    src/dsp/HiddenIrAdapter.cpp
    src/dsp/HiddenMixbusIrAdapter.cpp
    src/dsp/HiddenNamAdapter.cpp
    src/dsp/HiddenPostFaderIrAdapter.cpp
    src/dsp/MachineProcessor.cpp)

add_executable(FunctionalSmoke
    tests/FunctionalSmoke.cpp
    src/core/AudioEngine.cpp
    src/core/RenderEngine.cpp
    src/dsp/HiddenIrAdapter.cpp
    src/dsp/HiddenMixbusIrAdapter.cpp
    src/dsp/HiddenNamAdapter.cpp
    src/dsp/HiddenPostFaderIrAdapter.cpp
    src/dsp/MachineProcessor.cpp)

if(GEILALIZER_BUILD_JUCE_APP)
    add_executable(OfflineRenderSlice
        tests/OfflineRenderSlice.cpp
        src/core/RenderEngine.cpp
        src/dsp/HiddenIrAdapter.cpp
        src/dsp/HiddenMixbusIrAdapter.cpp
        src/dsp/HiddenNamAdapter.cpp
        src/dsp/HiddenPostFaderIrAdapter.cpp
        src/dsp/MachineProcessor.cpp)
endif()

target_include_directories(CoreSmoke PRIVATE src)
target_compile_features(CoreSmoke PRIVATE cxx_std_20)
target_include_directories(NamAdapterSmoke PRIVATE src)
target_compile_features(NamAdapterSmoke PRIVATE cxx_std_20)
target_include_directories(ProductE2E PRIVATE src)
target_compile_features(ProductE2E PRIVATE cxx_std_20)
target_include_directories(FunctionalSmoke PRIVATE src)
target_compile_features(FunctionalSmoke PRIVATE cxx_std_20)
if(GEILALIZER_BUILD_JUCE_APP)
    target_include_directories(OfflineRenderSlice PRIVATE src)
    target_compile_features(OfflineRenderSlice PRIVATE cxx_std_20)
    target_compile_definitions(OfflineRenderSlice PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(OfflineRenderSlice
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

if(GEILALIZER_ENABLE_NAMCORE)
    # NeuralAmpModelerCore registers architecture parsers from translation-unit static
    # initializers (WaveNet/LSTM/ConvNet/etc.).  A normal static-archive link can omit
    # those objects because no symbol is referenced directly, leaving NAM files to fail
    # at runtime with "No config parser registered".  Force-load the archive for every
    # target that embeds hidden NAM models so the registry is populated.
    set(GEILALIZER_NAMCORE_LINK "$<LINK_LIBRARY:WHOLE_ARCHIVE,GeilalizerNamCore>")

    if(GEILALIZER_BUILD_JUCE_APP)
        target_link_libraries(Geilalizer PRIVATE ${GEILALIZER_NAMCORE_LINK})
        target_compile_definitions(Geilalizer PRIVATE GEILALIZER_USE_NAMCORE)
    endif()
    target_link_libraries(CoreSmoke PRIVATE ${GEILALIZER_NAMCORE_LINK})
    target_link_libraries(NamAdapterSmoke PRIVATE ${GEILALIZER_NAMCORE_LINK})
    target_link_libraries(ProductE2E PRIVATE ${GEILALIZER_NAMCORE_LINK})
    target_link_libraries(FunctionalSmoke PRIVATE ${GEILALIZER_NAMCORE_LINK})
    if(GEILALIZER_BUILD_JUCE_APP)
        target_link_libraries(OfflineRenderSlice PRIVATE ${GEILALIZER_NAMCORE_LINK})
        target_compile_definitions(OfflineRenderSlice PRIVATE GEILALIZER_USE_NAMCORE)
    endif()
    target_compile_definitions(CoreSmoke PRIVATE GEILALIZER_USE_NAMCORE)
    target_compile_definitions(NamAdapterSmoke PRIVATE GEILALIZER_USE_NAMCORE)
    target_compile_definitions(ProductE2E PRIVATE GEILALIZER_USE_NAMCORE)
    target_compile_definitions(FunctionalSmoke PRIVATE GEILALIZER_USE_NAMCORE)
endif()

enable_testing()
add_test(NAME CoreSmoke COMMAND CoreSmoke)
add_test(NAME NamAdapterSmoke COMMAND NamAdapterSmoke)
add_test(NAME ProductE2E COMMAND ProductE2E)
add_test(NAME FunctionalSmoke COMMAND FunctionalSmoke)
if(GEILALIZER_BUILD_JUCE_APP)
    add_test(NAME OfflineRenderSlice COMMAND OfflineRenderSlice)
endif()
set_tests_properties(CoreSmoke ProductE2E FunctionalSmoke PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "GEILALIZER_DISABLE_REAL_NAMCORE=1")
if(GEILALIZER_BUILD_JUCE_APP)
    set_tests_properties(OfflineRenderSlice PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "GEILALIZER_DISABLE_REAL_NAMCORE=1")
endif()
set_tests_properties(NamAdapterSmoke PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

