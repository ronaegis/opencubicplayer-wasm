# Integer mixer device plugin (side module)

set(DEVWMIX_SOURCES
    ../devw/devwmix.c
    ../devw/dwmixa.c
    ../devw/dwmixqa.c
)

add_executable(devwmix-module
    ${DEVWMIX_SOURCES}
)

set_target_properties(devwmix-module PROPERTIES
    OUTPUT_NAME "devwmix"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_definitions(devwmix-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
