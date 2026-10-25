# This is a standalone project: building Veilbound does not build Afterlight
# or FLUX, and it does not need either SFML installation on your computer.
cmake_minimum_required(VERSION 3.20)
project(Veilbound VERSION 1.0.0 LANGUAGES CXX)

# A server can build and exercise the story/combat/save code without raylib.
# Leave this ON for the normal graphical game.
option(VEIL_BUILD_APP "Build the interactive RPG" ON)
include(CTest)

# Apply the same language and warning rules to the game and its tests.
# Headers describe shared data; source files supply the implementation.
function(veil_compile_settings target_name)
    target_include_directories(${target_name} PRIVATE include)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(VEIL_BUILD_APP)
    # The Windows helper pins the installation prefix, so CMake cannot silently
    # find a library built for a different compiler elsewhere on Windows.
    set(VEIL_RAYLIB_ROOT "" CACHE PATH "Exact raylib installation prefix (optional)")
    if(VEIL_RAYLIB_ROOT)
        set(raylib_DIR "${VEIL_RAYLIB_ROOT}/lib/cmake/raylib")
        find_package(raylib 6.2 CONFIG REQUIRED PATHS "${raylib_DIR}" NO_DEFAULT_PATH)
    else()
        find_package(raylib 6.2 CONFIG REQUIRED)
    endif()

    # raylib draws the game and reads input. The other three implementation
    # files contain plain C++ systems which can also be tested without a window.
    add_executable(veilbound
        src/main.cpp src/Renderer.cpp src/Typography.cpp src/Content.cpp src/Battle.cpp src/Game.cpp src/Audio.cpp
    )
    veil_compile_settings(veilbound)
    target_link_libraries(veilbound PRIVATE raylib)

    # The font already belongs to this repository; copying its license alongside
    # it preserves its redistribution terms. The compiled path is a development
    # fallback, while assets beside the executable support Explorer launches.
    target_compile_definitions(veilbound PRIVATE
        VEIL_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
    )
    add_custom_command(TARGET veilbound POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:veilbound>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/LICENSE-font.txt"
            "$<TARGET_FILE_DIR:veilbound>/assets"
        COMMENT "Copying the bundled font and its license beside Veilbound"
    )
endif()

if(BUILD_TESTING)
    # The test runner uses the actual campaign, battle, inventory and save code.
    # It deliberately links no graphics library and opens no window.
    add_executable(veilbound-tests
        src/tests.cpp src/Content.cpp src/Battle.cpp src/Game.cpp
    )
    veil_compile_settings(veilbound-tests)
    add_test(NAME veilbound_systems COMMAND veilbound-tests)
endif()
