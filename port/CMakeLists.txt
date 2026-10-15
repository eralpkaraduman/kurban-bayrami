cmake_minimum_required(VERSION 3.16)
project(kurban C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# raylib is vendored, so builds need no network.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
endif()
add_subdirectory(vendor/raylib)

add_executable(kurban src/main.c src/game.c src/actors.c src/flx.c)
target_link_libraries(kurban raylib)

if(EMSCRIPTEN)
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
        --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
    # cmake --build build-web --target itch  ->  build-web/kurban-itch.zip
    add_custom_target(itch
        COMMAND ${CMAKE_COMMAND} -E tar cf kurban-itch.zip --format=zip
            index.html index.js index.wasm index.data
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS kurban)
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
