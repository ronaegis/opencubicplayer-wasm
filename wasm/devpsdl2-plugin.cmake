# SDL2 device plugin (side module)
# Uses WASM-specific wrapper to optimize buffer sizes

set(DEVPSDL2_SOURCES
    devpsdl2-wrapper.c
)

add_executable(devpsdl2-module
    ${DEVPSDL2_SOURCES}
)

set_target_properties(devpsdl2-module PROPERTIES
    OUTPUT_NAME "devpsdl2"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(devpsdl2-module PRIVATE
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
