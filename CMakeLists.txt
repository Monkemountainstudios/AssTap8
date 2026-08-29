cmake_minimum_required(VERSION 3.22)

project(ASSTAP_8 VERSION 1.0.0 LANGUAGES C CXX)

set(ASSTAP8_JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE 9.0.1 checkout")

if (ASSTAP8_JUCE_SOURCE_DIR)
    add_subdirectory("${ASSTAP8_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(ASSTAP_8
    VERSION 1.0.0
    PRODUCT_NAME "ASSTAP_8"
    COMPANY_NAME "MonkeMountainStudios"
    COMPANY_WEBSITE "https://monkemountainstudios.neocities.org"
    BUNDLE_ID "com.monkemountainstudios.asstap8"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/AT8.png.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Source/AT8.png.png"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
)

juce_generate_juce_header(ASSTAP_8)

target_sources(ASSTAP_8 PRIVATE
    Source/AssTapTrack.cpp
    Source/AssTapTrack.h
    Source/KnobImageData.h
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/MetalImageData.h
    Source/SonicOnionImageData.h
)

target_compile_features(ASSTAP_8 PRIVATE cxx_std_17)

target_compile_definitions(ASSTAP_8 PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if (WIN32)
    target_compile_definitions(ASSTAP_8 PRIVATE
        JUCE_ASIO=1
        JUCE_DIRECTSOUND=1
        JUCE_WASAPI=1
    )
endif()

target_link_libraries(ASSTAP_8 PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_processors_headless
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
