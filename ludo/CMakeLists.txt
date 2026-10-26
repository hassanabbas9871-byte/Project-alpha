# This is a standalone game. Building it does not build the repository's
# other games, and it needs raylib rather than SFML.
cmake_minimum_required(VERSION 3.20)
project(Ludo VERSION 1.0.0 LANGUAGES CXX)

# Automated machines can exercise the actual rules without a graphics library.
# Leave this option ON when you want the playable desktop application.
option(LUDO_BUILD_APP "Build the graphical Ludo game" ON)
# This test opens a graphics context. Enable it on a desktop or under Xvfb;
# ordinary headless rules builds and Windows users do not need to enable it.
option(LUDO_BUILD_DISPLAY_TESTS "Build the optional desktop rendering regression" OFF)
include(CTest)

# Both programs compile the same rules with the same C++23 language setting.
function(ludo_compile_settings target_name)
    target_include_directories(${target_name} PRIVATE include)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(LUDO_BUILD_APP)
    # The Windows helper supplies an exact UCRT64 prefix. This prevents CMake
    # from silently choosing a raylib installation for a different compiler.
    set(LUDO_RAYLIB_ROOT "" CACHE PATH "Exact raylib installation prefix (optional)")
    if(LUDO_RAYLIB_ROOT)
        set(raylib_DIR "${LUDO_RAYLIB_ROOT}/lib/cmake/raylib")
        find_package(raylib 6.2 CONFIG REQUIRED PATHS "${raylib_DIR}" NO_DEFAULT_PATH)
    else()
        find_package(raylib 6.2 CONFIG REQUIRED)
    endif()

    # main.cpp owns the window and input, Typography.cpp draws readable text,
    # and Ludo.cpp owns the board-game rules.
    add_executable(ludo src/main.cpp src/Typography.cpp src/Display.cpp src/Ludo.cpp)
    ludo_compile_settings(ludo)
    target_link_libraries(ludo PRIVATE raylib)

    # Draw real TrueType text rather than stretching a tiny default bitmap.
    # Copy the font and its license beside the EXE for File Explorer launches.
    target_compile_definitions(ludo PRIVATE
        LUDO_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
    )
    add_custom_command(TARGET ludo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ludo>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/LICENSE-font.txt"
            "$<TARGET_FILE_DIR:ludo>/assets"
        COMMENT "Copying Ludo's readable font and its license"
    )

    if(BUILD_TESTING AND LUDO_BUILD_DISPLAY_TESTS)
        add_executable(ludo-display-tests
            src/display-tests.cpp src/Typography.cpp src/Display.cpp src/Ludo.cpp
        )
        ludo_compile_settings(ludo-display-tests)
        target_link_libraries(ludo-display-tests PRIVATE raylib)
        target_compile_definitions(ludo-display-tests PRIVATE
            LUDO_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../assets/DejaVuSans.ttf"
        )
        add_test(NAME ludo_display COMMAND ludo-display-tests)
        set_tests_properties(ludo_display PROPERTIES LABELS graphics)
    endif()
endif()

if(BUILD_TESTING)
    # Tests use the real rules implementation, with no window or raylib link.
    add_executable(ludo-tests src/tests.cpp src/Ludo.cpp)
    ludo_compile_settings(ludo-tests)
    add_test(NAME ludo_rules COMMAND ludo-tests)
endif()
