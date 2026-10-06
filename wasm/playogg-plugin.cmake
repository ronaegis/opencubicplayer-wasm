# PlayOGG Plugin Configuration

set(LIBOGG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libogg/src/bitwise.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libogg/src/framing.c
)

set(LIBVORBIS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/analysis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/bitrate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/block.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/codebook.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/envelope.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/floor0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/floor1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/lookup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/lpc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/lsp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/mapping0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/mdct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/psy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/res0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/sharedbook.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/smallft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/synthesis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/vorbisfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib/window.c
)

set(PLAYOGG_SOURCES
    ../playogg/cpiogginfo.c
    ../playogg/cpioggpic.c
    ../playogg/oggplay.c
    ../playogg/oggpplay.c
    ../playogg/oggtype.c
    ${LIBOGG_SOURCES}
    ${LIBVORBIS_SOURCES}
)

add_executable(playogg-module
    ${PLAYOGG_SOURCES}
)

set_target_properties(playogg-module PROPERTIES
    OUTPUT_NAME "playogg"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_include_directories(playogg-module BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/../playogg
    ${CMAKE_CURRENT_SOURCE_DIR}/libogg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis/lib
)

target_compile_definitions(playogg-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=0
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    OCP_MINOR_VERSION=0
    OCP_PATCH_VERSION=1
    VERSION="3.0.1-WASM"
)
