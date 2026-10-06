# PlayGMD Plugin Configuration

set(PLAYGMD_SOURCES
    ../playgmd/gmdpchan.c
    ../playgmd/gmdpdots.c
    ../playgmd/gmdpinst.c
    ../playgmd/gmdplay.c
    ../playgmd/gmdpplay.c
    ../playgmd/gmdptrak.c
    ../playgmd/gmdrtns.c
    ../playgmd/gmdtime.c
    ../playgmd/gmdtype.c
    ../playgmd/gmdl669.c
    ../playgmd/gmdlams.c
    ../playgmd/gmdldmf.c
    ../playgmd/gmdlmdl.c
    ../playgmd/gmdlmtm.c
    ../playgmd/gmdlokt.c
    ../playgmd/gmdlptm.c
    ../playgmd/gmdls3m.c
    ../playgmd/gmdlstm.c
    ../playgmd/gmdlult.c
)

# Create playgmd side module
add_executable(playgmd-module
    ${PLAYGMD_SOURCES}
)

# Set output name for side module
set_target_properties(playgmd-module PROPERTIES
    OUTPUT_NAME "playgmd"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playgmd
target_include_directories(playgmd-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playgmd
)

# Preprocessor definitions for side module (playgmd)
target_compile_definitions(playgmd-module PRIVATE
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
