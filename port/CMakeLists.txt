cmake_minimum_required(VERSION 3.24)
project(kurban C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# raylib 6.0 is downloaded on the first configure and checked against its
# SHA-256 hash. It builds as a static library, so it ends up inside the game
# binary or WebAssembly file, with no separate library to ship.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
endif()
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(raylib)

add_executable(kurban src/main.c src/game.c src/actors.c src/flx.c)
target_link_libraries(kurban raylib)

if(EMSCRIPTEN)
    # The web page around the game: a shared template, filled in per game.
    # Key codes are raylib's: arrows 262-265, X 88, C 67.
    set(WEB_DIR "${CMAKE_SOURCE_DIR}/web")
    set(WEB_TITLE "Kurban Bayrami: The Gathering")
    set(WEB_TOUCH_DPAD "{ up: 265, down: 264, left: 263, right: 262 }")
    set(WEB_TOUCH_BUTTONS "[{ label: 'X', caption: 'Jump', key: 88 }, { label: 'C', caption: 'Carry', key: 67 }]")
    configure_file("${WEB_DIR}/shell.html.in" "${CMAKE_BINARY_DIR}/shell.html" @ONLY)

    # The GitHub Pages cover page: the same kind of shared template. The
    # page text comes from web/site-content.html.
    set(SITE_TAGLINE "Herd sheep across town to the pit before the feast. My Ludum Dare 21 warm-up from 2011, rebuilt in C.")
    set(SITE_URL "https://eralpkaraduman.github.io/kurban-bayrami/")
    set(SITE_THEME "--bg: #12171c; --surface: #1b232b; --text: #e6edf2; --muted: #9aa8b4; --accent: #73add2; --accent-strong: #9fd06b; --border: #2b3742;")
    set(SITE_GAME_RATIO "3 / 1")
    # On touch screens the game frame holds the game at full width plus the
    # control bar. The frame is wider than tall, so the bar sizes its buttons
    # by the frame height, which keeps the D-pad at its 44 px minimum:
    # 3 cells plus 40 px of padding.
    set(SITE_TOUCH_FRAME_HEIGHT "calc(100vw / 3 + 3 * 44px + 42px)")
    set(SITE_HINT_KEYBOARD "Click the game, then press <kbd>X</kbd> to start. You need a keyboard.")
    set(SITE_HINT_TOUCH "Tap <b>X</b> to start. Play with the buttons under the game.")
    file(READ "${CMAKE_SOURCE_DIR}/web/site-content.html" SITE_CONTENT)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/web/site-content.html")
    configure_file("${WEB_DIR}/site.html.in" "${CMAKE_BINARY_DIR}/site.html" @ONLY)

    target_compile_definitions(kurban PRIVATE PLATFORM_WEB)
    # itch.io wants index.html at the root of the uploaded zip.
    set_target_properties(kurban PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
    target_link_options(kurban PRIVATE
        # The raylib wiki advises a fixed heap over memory growth.
        # The game needs only a few MB, so 32 MB leaves plenty of room.
        # emscripten_set_main_loop drives frames, so ASYNCIFY is not needed.
        -sUSE_GLFW=3
        -sINITIAL_MEMORY=33554432
        -sENVIRONMENT=web
        --preload-file ${CMAKE_SOURCE_DIR}/assets@assets
        --shell-file ${CMAKE_BINARY_DIR}/shell.html)
    # Relink when the page changes, and put the touch controls beside it.
    set_target_properties(kurban PROPERTIES LINK_DEPENDS "${CMAKE_BINARY_DIR}/shell.html")
    add_custom_command(TARGET kurban POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${WEB_DIR}/touch-controls.js" "${WEB_DIR}/touch-controls.css"
            "${CMAKE_BINARY_DIR}"
        VERBATIM)
    set(WEB_FILES index.html index.js index.wasm index.data touch-controls.js touch-controls.css)
    # cmake --build build-web --target itch  ->  build-web/kurban-itch.zip
    add_custom_target(itch
        COMMAND ${CMAKE_COMMAND} -E tar cf kurban-itch.zip --format=zip ${WEB_FILES}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS kurban)
    # cmake --build build-web --target pages  ->  build-web/pages/, the GitHub
    # Pages site: a cover page at the root and the game itself under play/.
    set(PAGES_DIR "${CMAKE_BINARY_DIR}/pages")
    add_custom_target(pages
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${PAGES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PAGES_DIR}/play" "${PAGES_DIR}/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/site.html" "${PAGES_DIR}/index.html"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/kurban-cover-630x500.png" "${PAGES_DIR}/cover.png"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/screenshots" "${PAGES_DIR}/screenshots"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/system.ttf" "${PAGES_DIR}/fonts/system.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy ${WEB_FILES} "${PAGES_DIR}/play"
        COMMAND ${CMAKE_COMMAND} -E touch "${PAGES_DIR}/.nojekyll"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS kurban
        VERBATIM)
else()
    if(NOT MSVC)
        target_link_libraries(kurban m)
    endif()
    # Copy assets next to the binary. main.c changes into that folder.
    add_custom_command(TARGET kurban POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:kurban>/assets)
endif()

# Logic checks, desktop only: ./build/kurban_check from this folder.
if(NOT EMSCRIPTEN)
    add_executable(kurban_check tests/check.c src/game.c src/actors.c src/flx.c)
    target_link_libraries(kurban_check raylib)
    if(NOT MSVC)
        target_link_libraries(kurban_check m)
    endif()
endif()
