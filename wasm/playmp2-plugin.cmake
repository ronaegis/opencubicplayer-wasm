# PlayMP2 Plugin Configuration

# libmad sources (MPEG audio decoder library)
set(LIBMAD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/version.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/fixed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/bit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/frame.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/synth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/layer12.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/layer3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad/huffman.c
)

set(PLAYMP2_SOURCES
    ../playmp2/cpiid3info.c
    ../playmp2/cpiid3pic.c
    ../playmp2/mppplay.c
    ../playmp2/mpplay.c
    ../playmp2/mptype.c
    ../playmp2/id3.c
    ${LIBMAD_SOURCES}
)

# Create playmp2 side module
add_executable(playmp2-module
    ${PLAYMP2_SOURCES}
)

# Set output name for side module
set_target_properties(playmp2-module PROPERTIES
    OUTPUT_NAME "playmp2"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playmp2
target_include_directories(playmp2-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playmp2
    ${CMAKE_CURRENT_SOURCE_DIR}/libmad
)

# Preprocessor definitions for side module (playmp2)
target_compile_definitions(playmp2-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
