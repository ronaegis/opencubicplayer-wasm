# Integer reverb plugin (side module)

set(IREVERB_SOURCES
    ../plugins/ireverb.c
)

add_executable(ireverb-module
    ${IREVERB_SOURCES}
)

set_target_properties(ireverb-module PROPERTIES
    OUTPUT_NAME "ireverb"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(ireverb-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)

target_link_options(ireverb-module PRIVATE "-lm")
