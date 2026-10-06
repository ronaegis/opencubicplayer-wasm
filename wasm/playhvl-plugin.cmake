# PlayHVL Plugin Configuration

set(PLAYHVL_SOURCES
    ../playhvl/loader.c
    ../playhvl/player.c
    ../playhvl/hvlpchan.c
    ../playhvl/hvlpdots.c
    ../playhvl/hvlpinst.c
    ../playhvl/hvlpplay.c
    ../playhvl/hvlplay.c
    ../playhvl/hvlptrak.c
    ../playhvl/hvltype.c
)

# Create playhvl side module
add_executable(playhvl-module
    ${PLAYHVL_SOURCES}
)

# Set output name for side module
set_target_properties(playhvl-module PROPERTIES
    OUTPUT_NAME "playhvl"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playhvl
target_include_directories(playhvl-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playhvl
)

# Preprocessor definitions for side module (playhvl)
target_compile_definitions(playhvl-module PRIVATE
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
