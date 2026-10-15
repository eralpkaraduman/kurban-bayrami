# Renders one web template the same way the game builds do.
#   cmake -DTEMPLATE=<file.in> -DOUT=<file> [-DSITE_CONTENT_FILE=<file>] [-D<NAME>=<value> ...] -P render.cmake
# Placeholder values arrive as -D variables. SITE_CONTENT is read from a file,
# as the games' CMakeLists.txt do.
if(DEFINED SITE_CONTENT_FILE)
    file(READ "${SITE_CONTENT_FILE}" SITE_CONTENT)
endif()
configure_file("${TEMPLATE}" "${OUT}" @ONLY)
