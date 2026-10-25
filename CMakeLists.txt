# CMake is a build organizer: it tells your compiler which files make the game,
# which C++ language version to use, and which external libraries to link.
cmake_minimum_required(VERSION 3.20)
project(Afterlight VERSION 2.0.0 LANGUAGES CXX)

# SFML handles the operating-system details of graphics, windows, and sound.
# This source uses SFML 3.1: capitalized component names and SFML:: targets
# are part of its CMake interface. An older installed SFML will fail clearly.
find_package(SFML 3.1 COMPONENTS Graphics Window System Audio REQUIRED)
if(SFML_VERSION VERSION_GREATER_EQUAL 4)
    message(FATAL_ERROR "Afterlight supports SFML 3.1 and later 3.x releases, not SFML 4.")
endif()

# Each .cpp file is compiled separately, then the linker combines them into
# one executable. Headers in include/ describe the pieces they share.
add_executable(afterlight
    src/main.cpp
    src/World.cpp
    src/Renderer.cpp
    src/Audio.cpp
)
target_include_directories(afterlight PRIVATE include)

# C++23 is required, rather than silently accepting an older C++ version.
# Disabling extensions keeps the code portable between standard compilers.
target_compile_features(afterlight PRIVATE cxx_std_23)
set_target_properties(afterlight PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(afterlight PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio)

# Warnings point out suspicious code while learning; they are not runtime
# errors. Different compiler families spell their warning switches differently.
if(MSVC)
    target_compile_options(afterlight PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(afterlight PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Keep a complete assets folder beside the program. The absolute source path
# is a fallback for developers who start the program from another directory.
target_compile_definitions(afterlight PRIVATE
    AFTERLIGHT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
add_custom_command(TARGET afterlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:afterlight>/assets"
    COMMENT "Copying the bundled font and its license beside Afterlight"
)

# Windows UCRT64 users run through scripts/run-ucrt64.bat, which supplies
# the matching SFML and GCC runtime DLLs through PATH. Copying only SFML DLLs
# would miss transitive DLL dependencies, so we do not pretend to package them.

# CTest runs the game's own physics checks. These do not open a window, so
# they can also run on a server with no monitor or graphics session.
include(CTest)
if(BUILD_TESTING)
    add_test(NAME parkour_physics COMMAND afterlight --self-test)
endif()
