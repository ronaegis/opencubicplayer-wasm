# Modland.com Plugin Configuration (filesystem driver)

set(MODLAND_SOURCES
    modland-com-stepper.c
    # Note: modland-com-stepper.c includes modland-com.c with renamed functions
    #       and provides non-blocking stepper wrappers for WASM builds
    # All other .c files are #included by modland-com.c (single compilation unit)
    # WASM-specific download wrapper is in main module (shared infrastructure)
)

# Create modland side module
add_executable(modland-module
    ${MODLAND_SOURCES}
)

# Set output name for side module
set_target_properties(modland-module PROPERTIES
    OUTPUT_NAME "modland"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Preprocessor definitions for side module (modland)
target_compile_definitions(modland-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
    OCP_WASM_FILESEL_STEPPER=1
)
