cmake_minimum_required(VERSION 3.22)

project(OniriqueWeight VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Universal binary, and the oldest macOS the JUCE 9 runtime supports.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE is looked for next to this file first, then in the PitchBox checkout
# beside this one, so the plugins of the range can share a single copy.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    set(WEIGHT_JUCE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
else()
    set(WEIGHT_JUCE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../PitchBox/JUCE")
endif()

set(WEIGHT_JUCE_DIR "${WEIGHT_JUCE_DEFAULT}" CACHE PATH "Path to JUCE")

if(NOT EXISTS "${WEIGHT_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${WEIGHT_JUCE_DIR}.\n"
        "Run:  git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git")
endif()

add_subdirectory("${WEIGHT_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# The DSP, built on its own so the offline tests can link it without dragging
# in JUCE. Everything that decides how the plugin sounds lives here.
add_library(weight_core STATIC
    Source/dsp/Weight.cpp)

target_include_directories(weight_core PUBLIC Source)
set_target_properties(weight_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # MSVC's <cmath> only declares M_PI when asked. PUBLIC so the tests get it too.
    target_compile_definitions(weight_core PUBLIC _USE_MATH_DEFINES)
    target_compile_options(weight_core PRIVATE /W4)
else()
    target_compile_options(weight_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# The plugin.
set(WEIGHT_FORMATS VST3)

if(APPLE)
    list(APPEND WEIGHT_FORMATS AU)
endif()

# The system VST3 folder on Windows is under Program Files, which a normal build
# cannot write to, so installing after the build is opt-in there.
if(WIN32)
    option(WEIGHT_COPY_AFTER_BUILD "Install the plugin after building" OFF)
else()
    option(WEIGHT_COPY_AFTER_BUILD "Install the plugin after building" ON)
endif()

juce_add_plugin(OniriqueWeight
    COMPANY_NAME "Onirique"
    BUNDLE_ID "com.onirique.weight"
    PLUGIN_MANUFACTURER_CODE Onrq
    PLUGIN_CODE Wght
    FORMATS ${WEIGHT_FORMATS}
    # "Weight" alone is a common word: the file on disk carries the house's name too.
    PRODUCT_NAME "OniriqueWeight"
    PLUGIN_NAME "ONIRIQUE Weight"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD ${WEIGHT_COPY_AFTER_BUILD})

# The Onirique sigil, and the photographs the front is made of (tools/photo.cpp).
juce_add_binary_data(WeightAssets SOURCES
    Resources/sigil.png
    Resources/photo/front-off.jpg
    Resources/photo/front-on.jpg
    Resources/photo/dial.png
    Resources/photo/lowfreq.png
    Resources/photo/highfreq.png
    Resources/photo/attensel.png
    Resources/photo/eqin.png
    Resources/photo/power.png)

target_sources(OniriqueWeight PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Theme.cpp)

# The licence every Onirique plugin carries: the folder is copied as it is from
# plugin-license/ in the ONIRIQUE CENTRAL repository, and is only ever changed there.
include(License/OniriqueLicense.cmake)
onirique_add_license(OniriqueWeight)

target_compile_definitions(OniriqueWeight PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

# The descriptions are in French: the sources are UTF-8, and MSVC has to be told.
if(MSVC)
    target_compile_options(OniriqueWeight PRIVATE /utf-8)
endif()

target_link_libraries(OniriqueWeight
    PRIVATE
        weight_core
        WeightAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline test: the whole audio path without a host.
enable_testing()

add_executable(test_weight tests/test_weight.cpp)
target_link_libraries(test_weight PRIVATE weight_core)
target_include_directories(test_weight PRIVATE tests)
add_test(NAME weight COMMAND test_weight)

# A development tool, not part of the product: renders the editor to a PNG
# without a host. Built only when asked for by name.
add_executable(weight_snapshot EXCLUDE_FROM_ALL tools/snapshot.cpp)

target_include_directories(weight_snapshot PRIVATE
    $<TARGET_PROPERTY:OniriqueWeight,INCLUDE_DIRECTORIES>)

target_compile_definitions(weight_snapshot PRIVATE
    $<TARGET_PROPERTY:OniriqueWeight,COMPILE_DEFINITIONS>)

if(MSVC)
    target_compile_options(weight_snapshot PRIVATE /utf-8)
endif()

target_link_libraries(weight_snapshot PRIVATE OniriqueWeight weight_core WeightAssets)

# A development tool, not part of the product: path-traces the photographs of
# the front and of every position of its controls into Resources/photo. It
# stands apart from the plugin, whose build needs those photographs.
juce_add_console_app(weight_photo PRODUCT_NAME "weight_photo")
set_target_properties(weight_photo PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(weight_photo PRIVATE tools/photo.cpp)

juce_add_binary_data(WeightPhotoData HEADER_NAME PhotoData.h NAMESPACE PhotoData SOURCES
    Resources/sigil.png
    License/Fonts/HankenGrotesk600.ttf)
set_target_properties(WeightPhotoData PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_compile_definitions(weight_photo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

if(MSVC)
    target_compile_options(weight_photo PRIVATE /utf-8 /fp:fast)
endif()

target_link_libraries(weight_photo PRIVATE WeightPhotoData juce::juce_gui_basics juce::juce_recommended_config_flags)
