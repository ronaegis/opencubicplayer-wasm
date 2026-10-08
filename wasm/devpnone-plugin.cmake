# Null output device plugin (side module)

set(DEVPNONE_SOURCES
    ../devp/devpnone.c
)

add_executable(devpnone-module
    ${DEVPNONE_SOURCES}
)

set_target_properties(devpnone-module PROPERTIES
    OUTPUT_NAME "devpnone"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(devpnone-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
