# Floating-point mixer device plugin (side module)

set(DEVWMIXF_SOURCES
    ../devw/devwmixf.c
    ../devw/dwmixfa.c
)

add_executable(devwmixf-module
    ${DEVWMIXF_SOURCES}
)

set_target_properties(devwmixf-module PROPERTIES
    OUTPUT_NAME "devwmixf"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(devwmixf-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
