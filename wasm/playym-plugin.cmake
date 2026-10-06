# PlayYM Plugin Configuration

set(PLAYYM_SOURCES
    ../playym/ymplay.cpp
    ../playym/ympplay.cpp
    ../playym/ymtype.cpp
    ../playym/stsoundlib/digidrum.cpp
    ../playym/stsoundlib/Ym2149Ex.cpp
    ../playym/stsoundlib/Ymload.cpp
    ../playym/stsoundlib/YmMusic.cpp
    ../playym/stsoundlib/YmUserInterface.cpp
)

# LZH support is required for YM files
set(PLAYYM_LZH_SOURCES
    ../playym/lzh/lzhlib.cpp
)
list(APPEND PLAYYM_SOURCES ${PLAYYM_LZH_SOURCES})
set(HAVE_LZH 1)

# Create playym side module
add_executable(playym-module
    ${PLAYYM_SOURCES}
)

# Set output name for side module
set_target_properties(playym-module PROPERTIES
    OUTPUT_NAME "playym"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playym
target_include_directories(playym-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playym
    ${CMAKE_SOURCE_DIR}/../playym/stsoundlib
    ${CMAKE_SOURCE_DIR}/../playym/lzh
)

# Preprocessor definitions for side module (playym)
target_compile_definitions(playym-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    HAVE_MKSTEMP=1
    OCP_MAJOR_VERSION=3
    OCP_MINOR_VERSION=0
    OCP_PATCH_VERSION=1
    VERSION="3.0.1-WASM"
    HAVE_LZH=${HAVE_LZH}
)
