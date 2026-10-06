# PlayFLAC Plugin Configuration

set(LIBFLAC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/bitmath.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/bitreader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/bitwriter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/cpu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/fixed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/format.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/lpc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/metadata_iterators.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/metadata_object.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/stream_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/src/window.c
)

set(PLAYFLAC_SOURCES
    ../playflac/cpiflacinfo.c
    ../playflac/cpiflacpic.c
    ../playflac/flacplay.c
    ../playflac/flacpplay.c
    ../playflac/flactype.c
    ${LIBFLAC_SOURCES}
)

add_executable(playflac-module
    ${PLAYFLAC_SOURCES}
)

set_target_properties(playflac-module PROPERTIES
    OUTPUT_NAME "playflac"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_include_directories(playflac-module BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../playflac
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/include/FLAC
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/include/share
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/include/private
    ${CMAKE_CURRENT_SOURCE_DIR}/libflac/include/protected
)

target_compile_definitions(playflac-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    FLAC__NO_MD5=0
    FLAC__HAS_OGG=0
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    OCP_MINOR_VERSION=0
    OCP_PATCH_VERSION=1
    VERSION="3.0.1-WASM"
)

target_compile_options(playflac-module PRIVATE
    "-UNDEBUG"
    "-include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libflac/assert_shim.h"
)
