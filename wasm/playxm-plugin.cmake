# PlayXM Plugin Configuration

set(PLAYXM_SOURCES
    ../playxm/xmplay.c
    ../playxm/xmload.c
    ../playxm/xmlmod.c
    ../playxm/xmlmxm.c
    ../playxm/xmtime.c
    ../playxm/xmtype.c
    ../playxm/xmpplay.c
    ../playxm/xmptrak.c
    ../playxm/xmpinst.c
    ../playxm/xmchan.c
    ../playxm/xmrtns.c
)

# Fix naming conflict for xmtime.c
set_source_files_properties(../playxm/xmtime.c PROPERTIES
    COMPILE_DEFINITIONS "sync=xm_sync_var"
)

# Create playxm side module
add_executable(playxm-module
    ${PLAYXM_SOURCES}
)

# Set output name for side module
set_target_properties(playxm-module PROPERTIES
    OUTPUT_NAME "playxm"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_compile_options(playxm-module PRIVATE
    "-include" "${CMAKE_SOURCE_DIR}/playxm-stdio-shim.h"
)

# Preprocessor definitions for side module (playxm)
target_compile_definitions(playxm-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)
