# PlayWAV Plugin Configuration

set(PLAYWAV_SOURCES
    ../playwav/wavplay.c
    ../playwav/wavpplay.c
    ../playwav/wavtype.c
)

add_executable(playwav-module
    ${PLAYWAV_SOURCES}
)

set_target_properties(playwav-module PROPERTIES
    OUTPUT_NAME "playwav"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_include_directories(playwav-module BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../playwav
)

target_compile_definitions(playwav-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    OCP_MINOR_VERSION=0
    OCP_PATCH_VERSION=1
    VERSION="3.0.1-WASM"
)
