# Floating-point reverb plugin (side module)

set(FREVERB_SOURCES
    ../plugins/freverb.c
)

add_executable(freverb-module
    ${FREVERB_SOURCES}
)

set_target_properties(freverb-module PROPERTIES
    OUTPUT_NAME "freverb"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(freverb-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)

target_link_options(freverb-module PRIVATE "-lm")
