# PlayAY Plugin Configuration

set(PLAYAY_SOURCES
    ../playay/z80.c
    ../playay/sound.c
    ../playay/aychan.c
    ../playay/aypplay.c
    ../playay/ayplay.c
    ../playay/aytype.c
)

# Create playay side module
add_executable(playay-module
    ${PLAYAY_SOURCES}
)

# Set output name for side module
set_target_properties(playay-module PROPERTIES
    OUTPUT_NAME "playay"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playay
target_include_directories(playay-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playay
)

# Preprocessor definitions for side module (playay)
target_compile_definitions(playay-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
