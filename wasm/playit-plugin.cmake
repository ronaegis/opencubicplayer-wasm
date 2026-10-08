# PlayIT Plugin Configuration

set(PLAYIT_SOURCES
    ../playit/itload.c
    ../playit/itchan.c
    ../playit/itpinst.c
    ../playit/itplay.c
    ../playit/itpplay.c
    ../playit/itptrack.c
    ../playit/itrtns.c
    ../playit/itsex.c
    ../playit/ittime.c
    ../playit/ittype.c
)

# Create playit side module
add_executable(playit-module
    ${PLAYIT_SOURCES}
)

# Set output name for side module
set_target_properties(playit-module PROPERTIES
    OUTPUT_NAME "playit"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playit
target_include_directories(playit-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playit
)

# Preprocessor definitions for side module (playit)
target_compile_definitions(playit-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
