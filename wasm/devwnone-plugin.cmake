# Wavetable no-output device plugin (side module)

set(DEVWNONE_SOURCES
    ../devw/devwnone.c
)

add_executable(devwnone-module
    ${DEVWNONE_SOURCES}
)

set_target_properties(devwnone-module PROPERTIES
    OUTPUT_NAME "devwnone"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(devwnone-module PRIVATE
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
