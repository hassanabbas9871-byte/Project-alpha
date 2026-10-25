# This is a separate CMake project. Building FLUX does not build Afterlight
# and does not require either version of SFML.
cmake_minimum_required(VERSION 3.20)
project(Flux VERSION 1.0.0 LANGUAGES CXX)

# Turn this off to build only the pure C++ physics checks on a machine that
# does not have raylib or a display. Normal users can leave it on.
option(FLUX_BUILD_APP "Build the interactive raylib application" ON)
include(CTest)

# A small helper applies the same language requirements to both executables.
# C++23 is required: CMake must not silently select an older language version.
function(flux_compile_settings target_name)
    target_include_directories(${target_name} PRIVATE include)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(FLUX_BUILD_APP)
    # raylib owns the graphics window, drawing, keyboard, and mouse access.
    # The Windows helper pins this prefix to /ucrt64 so another installed
    # toolchain cannot accidentally supply incompatible headers or libraries.
    set(FLUX_RAYLIB_ROOT "" CACHE PATH "Exact raylib installation prefix (optional)")
    if(FLUX_RAYLIB_ROOT)
        set(raylib_DIR "${FLUX_RAYLIB_ROOT}/lib/cmake/raylib")
        find_package(raylib 6.2 CONFIG REQUIRED PATHS "${raylib_DIR}" NO_DEFAULT_PATH)
    else()
        find_package(raylib 6.2 CONFIG REQUIRED)
    endif()
    if(DEFINED raylib_VERSION AND raylib_VERSION VERSION_LESS 6.2)
        message(FATAL_ERROR "FLUX needs raylib 6.2 or newer; your UCRT64 raylib 6.2 is supported.")
    endif()

    add_executable(flux src/main.cpp src/Simulation.cpp)
    flux_compile_settings(flux)
    target_link_libraries(flux PRIVATE raylib)

    # Reuse the font already distributed with this repository. It is copied
    # beside the executable so starting from Explorer still finds the asset.
    # The source path is only a fallback for unusual development directories.
    target_compile_definitions(flux PRIVATE
        FLUX_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
    )
    add_custom_command(TARGET flux POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:flux>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/LICENSE-font.txt"
            "$<TARGET_FILE_DIR:flux>/assets"
        COMMENT "Copying the font and its redistribution license beside FLUX"
    )
endif()

if(BUILD_TESTING)
    # This executable does not link raylib at all. These are real simulation
    # scenarios, and they can run without a monitor, GPU context, or speakers.
    add_executable(flux-physics-tests src/physics_tests.cpp src/Simulation.cpp)
    flux_compile_settings(flux-physics-tests)
    add_test(NAME flux_physics COMMAND flux-physics-tests)
endif()
