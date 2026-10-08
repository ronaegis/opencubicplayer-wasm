cmake_minimum_required(VERSION 3.10)
project(OpenCubicPlayer-WASM)

# Set emscripten-specific options
set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_CROSSCOMPILING TRUE)

# Use the current binary directory for output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Ensure main module links in standard library objects so side modules can
# resolve C++ runtime symbols (e.g. RTTI, exception types).
set(ENV{EMCC_FORCE_STDLIBS} 1)

# Compiler flags for WASM - add debug symbols and PIC for dynamic linking
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(FATAL_ERROR "The WASM build must be configured with emcc/em++ (use emcmake cmake) ")
endif()

option(OCP_WASM_DEBUG_LOGGING "Enable verbose WASM logging and SDL2 debug output" OFF)
option(WASM_PRODUCTION_BUILD "Optimize for production (disable debug symbols, enable assertions=0, use -O3)" ON)
option(WASM_INCLUDE_SAMPLES "Include sample/demo music files in build (adds ~8MB)" OFF)
option(WASM_INCLUDE_HELP "Include help database (ocp.hlp, adds ~45KB)" ON)
option(WASM_INCLUDE_ADPLUG_DB "Include AdPlug metadata database (adds ~8KB)" ON)

# Set compiler flags based on build type
if(WASM_PRODUCTION_BUILD)
    set(WASM_COMMON_FLAGS "-O3 -fPIC -DWASM_BUILD -sUSE_SDL=2 -sUSE_FREETYPE=1")
    set(WASM_DEBUG_FLAGS "")
    set(WASM_ASSERTIONS_FLAG "-s ASSERTIONS=0")
    message(STATUS "WASM Production Build: O3 optimizations enabled, debug symbols disabled")
else()
    set(WASM_COMMON_FLAGS "-O1 -fPIC -DWASM_BUILD -sUSE_SDL=2 -sUSE_FREETYPE=1")
    set(WASM_DEBUG_FLAGS "-g")
    set(WASM_ASSERTIONS_FLAG "-s ASSERTIONS=2")
    message(STATUS "WASM Development Build: Debug symbols enabled, moderate optimizations")
endif()
list(APPEND WASM_EXTRA_DEFINES OCP_WASM_FILESEL_STEPPER OCP_WASM_DIALOG_STEPPER)

if(OCP_WASM_DEBUG_LOGGING)
    list(APPEND WASM_EXTRA_DEFINES OCP_WASM_DEBUG_LOGGING SDL2_DEBUG)
endif()

if(WASM_EXTRA_DEFINES)
    add_compile_definitions(${WASM_EXTRA_DEFINES})
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WASM_COMMON_FLAGS} ${WASM_DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_COMMON_FLAGS} ${WASM_DEBUG_FLAGS} -frtti")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main module linker flags (core OCP with dynamic linking support)
# Use MAIN_MODULE=1 to export all symbols including C++ stdlib for side modules
# Memory: 16MB initial, grows as needed via ALLOW_MEMORY_GROWTH
# dynCall_* names in EXPORTED_RUNTIME_METHODS must also appear in the
# signatures array in index.html. wasm/tests/dyncall-list.test.js checks that.
set(MAIN_MODULE_LINKER_FLAGS "\
    ${WASM_DEBUG_FLAGS} \
    -s WASM=1 \
    -s USE_SDL=2 \
    -s FETCH=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=16777216 \
    -s MAIN_MODULE=1 \
    -s ALLOW_TABLE_GROWTH=1 \
    -s RESERVED_FUNCTION_POINTERS=20 \
    -sEXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"FS\",\"loadDynamicLibrary\",\"dynCall\",\"dynCall_i\",\"dynCall_ii\",\"dynCall_iii\",\"dynCall_iiii\",\"dynCall_iiiii\",\"dynCall_v\",\"dynCall_vi\",\"dynCall_vii\",\"dynCall_viii\",\"dynCall_viiii\",\"wasmTable\",\"addFunction\",\"removeFunction\"]' \
    -s DISABLE_EXCEPTION_CATCHING=0 \
    -s NO_EXIT_RUNTIME=1 \
    -lidbfs.js \
    ${WASM_ASSERTIONS_FLAG} \
    -s SAFE_HEAP=0 \
    -sEXPORTED_FUNCTIONS='[\"_wasm_start_ocp\",\"_wasm_startup_is_complete\",\"_wasm_main_loop\",\"_load_module_file_interface\",\"_wasm_get_virtual_device_stepper_active\",\"_wasm_set_virtual_device_stepper_active\",\"_wasm_sync_download_to_file\",\"_idbfs_sync_complete_callback\",\"_idbfs_flush\",\"_wasm_set_virtual_path\",\"_wasm_get_virtual_path\",\"_wasm_set_no_sleep_mode\",\"_wasm_get_no_sleep_mode\",\"_wasm_get_frame_count\",\"_wasm_force_frame_render\",\"_wasm_get_paths_json\",\"_main\",\"_malloc\",\"_free\"]' \
    --preload-file ${CMAKE_SOURCE_DIR}/unifont-16.0.04.ttf@unifont-16.0.04.ttf \
    --preload-file ${CMAKE_SOURCE_DIR}/ocp-default.ini@/assets/ocp.ini")

# Conditionally add help database (adds ~45KB)
if(WASM_INCLUDE_HELP)
    set(MAIN_MODULE_LINKER_FLAGS "${MAIN_MODULE_LINKER_FLAGS} \
    --preload-file ${CMAKE_SOURCE_DIR}/../ocp.hlp@/assets/ocp.hlp")
    message(STATUS "Including help database (~45KB)")
else()
    message(STATUS "Excluding help database (use -DWASM_INCLUDE_HELP=ON to include)")
endif()

# Conditionally add AdPlug metadata database (adds ~8KB)
if(WASM_INCLUDE_ADPLUG_DB)
    set(MAIN_MODULE_LINKER_FLAGS "${MAIN_MODULE_LINKER_FLAGS} \
    --preload-file ${CMAKE_SOURCE_DIR}/../playopl/adplugdb-git/adplug.db@/assets/adplug.db")
    message(STATUS "Including AdPlug database (~8KB)")
else()
    message(STATUS "Excluding AdPlug database (use -DWASM_INCLUDE_ADPLUG_DB=ON to include)")
endif()

# Catalog produced by wasm/build.sh before configure. Parsing allmods.txt
# happens in that host tool, not in the browser.
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/CPMDLAND.DAT")
    set(MAIN_MODULE_LINKER_FLAGS "${MAIN_MODULE_LINKER_FLAGS} \
    --preload-file ${CMAKE_CURRENT_BINARY_DIR}/CPMDLAND.DAT@/modland/CPMDLAND.DAT")
    message(STATUS "Including prebuilt Modland catalog")
else()
    message(STATUS "No prebuilt Modland catalog (run wasm/build.sh to create CPMDLAND.DAT)")
endif()

# Sample files are now staged as external HTTP assets by the build script.
if(WASM_INCLUDE_SAMPLES)
    message(STATUS "Sample music files will be served lazily over HTTP")
else()
    message(STATUS "Sample music files disabled (use -DWASM_INCLUDE_SAMPLES=ON to stage them)")
endif()

set(SIDE_MODULE_LINKER_FLAGS "\
    ${WASM_DEBUG_FLAGS} \
    -s WASM=1 \
    -s SIDE_MODULE=1")

# Include directories - include original source directories
include_directories(
    ${CMAKE_SOURCE_DIR}  # For config.h, iconv.h, and wasm headers (FIRST for header precedence)
    ${CMAKE_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/../boot
    ${CMAKE_SOURCE_DIR}/../stuff
    ${CMAKE_SOURCE_DIR}/../cpiface
    ${CMAKE_SOURCE_DIR}/../playxm
    ${CMAKE_SOURCE_DIR}/../playsid
    ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src
    ${CMAKE_SOURCE_DIR}/../dev
    ${CMAKE_SOURCE_DIR}/../devp
    ${CMAKE_SOURCE_DIR}/../devw
    ${CMAKE_SOURCE_DIR}/../filesel
)

# Define sources - using original files where possible
set(BOOT_SOURCES
    ../boot/console.c
    ../boot/compdate.c
    ../boot/pmain.c
    ../boot/psetting.c
    ../boot/plinkman.c  # Plugin loader - supports dynamic linking in WASM
)

set(STUFF_SOURCES
    # Re-enable font system components for debugging
    ../stuff/pfonts.c
    ../stuff/poutput-fontengine.c
    ../stuff/poutput-swtext.c
    ../stuff/cp437.c
    ../stuff/ttf.c
    ../stuff/poutput.c
    # Enable SDL2 display system for interface UI (wrapped for WASM safety)
    poutput-sdl2-wrapper.c
    ../stuff/poutput-keyboard.c
    # Use platform-aware framelock instead of stub
    framelock-wasm.c
    framelock-platform.c
    ../stuff/poll.c
    ../stuff/err.c
    ../stuff/compat.c
    ../stuff/latin1.c
    ../stuff/utf-8.c
    # Exclude sets.c due to dllextinfo conflict
    # ../stuff/sets.c
)

set(CPIFACE_SOURCES
    cpiface-wrapper.c
    ../cpiface/cpitext.c
    ../cpiface/cpichan.c
    ../cpiface/cpiinst.c
    ../cpiface/cpitrack.c
    ../cpiface/cpimsg.c
    ../cpiface/mcpedit.c
    ../cpiface/volctrl.c
    # Add missing cpiface dependencies
    ../cpiface/fft.c
    ../cpiface/cpianal.c
    ../cpiface/cpigraph.c
    # ../cpiface/cpikube.c  # Excluded - causes directory access issues in WASM
    ../cpiface/cpilinks.c
    ../cpiface/cpimvol.c
    ../cpiface/cpiscope.c
    ../cpiface/cpidots.c
    ../cpiface/cpipic.c
    ../cpiface/cpiphase.c
    ../cpiface/cpiptype.c
    ../cpiface/cpikeyhelp.c
)

# Plugin configurations are in separate files
# (PLAYXM_SOURCES and related configs moved to playxm-plugin.cmake)

# Fix naming conflict for volctrl.c
set_source_files_properties(../cpiface/volctrl.c PROPERTIES
    COMPILE_FLAGS "-DMCP_NO_MATH_H"
)

# NOTE: For side module, we do NOT rename dllextinfo - it needs to be exported as-is
# The main module will have renamed dllextinfo symbols for built-in components

# Fix dllextinfo conflict for cpiface.c (so playxm plugin can export dllextinfo)
set_source_files_properties(../cpiface/cpiface.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=cpiface_dllextinfo"
)

# Fix dllextinfo conflict for deviplay.c
set_source_files_properties(../dev/deviplay.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=deviplay_dllextinfo"
)

# Fix dllextinfo conflict for mchasm.c
set_source_files_properties(../dev/mchasm.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=mchasm_dllextinfo"
)

# Fix dllextinfo conflict for deviwave.c
set_source_files_properties(../dev/deviwave.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=deviwave_dllextinfo"
)

# Fix dllextinfo conflict for cphelper.c
set_source_files_properties(../help/cphelper.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=cphelper_dllextinfo"
)

# Fix dllextinfo conflict for cphlpif.c
set_source_files_properties(../help/cphlpif.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=cphlpif_dllextinfo"
)

# Fix dllextinfo conflict for pfilesel.c
set_source_files_properties(../filesel/pfilesel.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=pfilesel_dllextinfo"
)

# Fix dllextinfo conflict for fsptype.c
set_source_files_properties(../filesel/fsptype.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=fsptype_dllextinfo"
)

# Fix dllextinfo conflict for pfsmain.c
set_source_files_properties(../filesel/pfsmain.c PROPERTIES
    COMPILE_DEFINITIONS "dllextinfo=pfsmain_dllextinfo"
)

set(DEV_SOURCES
    ../dev/ringbuffer.c
    # Include deviplay.c for real audio playback
    ../dev/deviplay.c
    # Include plrasm.c for plrConvertBufferFromStereo16BitSigned
    ../dev/plrasm.c
    # Include mchasm.c for mixAddAbs16SS and mixGetMasterSample functions
    ../dev/mchasm.c
    # Include mixasm.c for mixer assembly functions
    ../dev/mixasm.c
    # Include deviwave.c for MCP driver system
    ../dev/deviwave.c
    ../dev/player.c
    ../dev/mix.c
    # Exclude mixclip.c due to dllextinfo conflict
    # ../dev/mixclip.c
    ../dev/postproc.c
)

set(FILESEL_SOURCES
    # Full file selector system for WASM
    ../filesel/dirdb.c
    # Use WASM wrapper instead of original mdb.c (skips 0-byte placeholder scans)
    wasm-mdb-wrapper.c
    pfilesel-stepper.c
    fsSetup-stepper.c
    fsHelp-stepper.c
    ../filesel/pfsmain.c
    ../filesel/modlist.c
    ../filesel/filesystem.c
    # Use WASM wrapper instead of original filesystem-unix.c
    wasm-filesystem-unix-wrapper.c
    ../filesel/filesystem-drive.c
    ../filesel/filesystem-dir-mem.c
    ../filesel/filesystem-setup.c
    ../filesel/filesystem-filehandle-cache.c
    ../filesel/filesystem-playlist.c
    ../filesel/filesystem-playlist-m3u.c
    ../filesel/filesystem-playlist-pls.c
    ../filesel/filesystem-file-dev.c
    ../filesel/filesystem-textfile.c
    ../filesel/filesystem-zip.c
    ../filesel/charsets.c
    ../filesel/adbmeta.c
    ../filesel/fsptype.c
    # musicbrainz.c excluded - requires cJSON library, not needed for WASM
    # WASM-specific download wrapper using emscripten_fetch (shared infrastructure for modland and future plugins)
    download-web.c
)

set(HELP_SOURCES
    # Include full help system with cphelper.c to load ocp.hlp
    ../help/cphelper.c
    ../help/cphlpif.c
)

# WASM-specific sources and platform implementations
set(WASM_SOURCES
    wasm-original-main.c
    wasm-stubs.c
    wasm-fileio.c
    sync-download.c
    idbfs-persistence.c
    psetting-wasm.c
    psetting-platform.c
    wasm-platform-config-api.c
    generic-dialog-stepper.c
    loading-progress.c
    modland-seed.c
)

# Plugin configurations (moved to playsid-plugin.cmake)

# ---------------------------------------------------------------------------
# Main executable (core module) and side modules
# ---------------------------------------------------------------------------

# Include plugin configurations
include(playxm-plugin.cmake)
include(playsid-plugin.cmake)
include(playym-plugin.cmake)
include(playgme-plugin.cmake)
include(playflac-plugin.cmake)
include(playwav-plugin.cmake)
include(playogg-plugin.cmake)
include(playopl-plugin.cmake)
include(playgmd-plugin.cmake)
include(playit-plugin.cmake)
include(playay-plugin.cmake)
include(playhvl-plugin.cmake)
include(playmp2-plugin.cmake)
include(playtimidity-plugin.cmake)
include(devpsdl2-plugin.cmake)
include(devpnone-plugin.cmake)
include(devwmix-plugin.cmake)
include(devwmixf-plugin.cmake)
include(devwnone-plugin.cmake)
include(freverb-plugin.cmake)
include(ireverb-plugin.cmake)
include(modland-plugin.cmake)

# Create main executable (without playxm - will be loaded as side module)
add_executable(ocp-wasm
    ${BOOT_SOURCES}
    ${STUFF_SOURCES}
    ${CPIFACE_SOURCES}
    # PLAYXM_SOURCES excluded - will be in side module
    ${DEV_SOURCES}
    ${FILESEL_SOURCES}
    ${HELP_SOURCES}
    ${WASM_SOURCES}
)

# Set output name for main module
set_target_properties(ocp-wasm PROPERTIES
    OUTPUT_NAME "ocp"
    SUFFIX ".js"
    LINK_FLAGS "${MAIN_MODULE_LINKER_FLAGS}"
)

# Plugin side modules are created in their respective .cmake files

# Preprocessor definitions for main module
target_compile_definitions(ocp-wasm PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    OCP_MAJOR_VERSION=3
    VERSION="3.5.0+wasm.0.1.0"
    OCP_WASM_HAS_ZIP=1
    OCP_NO_BZIP2=1
)

target_compile_options(ocp-wasm PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/include/sdl_audio_shim.h")

# Warnings for the port's own sources. Several of them #include an upstream
# .c file to wrap it, so the classes upstream trips on stay off:
# sign-compare, #warning notes, and the 60-byte database signatures.
file(GLOB OCP_WASM_WARNING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
set_source_files_properties(${OCP_WASM_WARNING_SOURCES} PROPERTIES
    COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-cpp -Wno-unterminated-string-initialization")

set(OCP_PRELOAD_DEPS
    "${CMAKE_SOURCE_DIR}/unifont-16.0.04.ttf"
    "${CMAKE_SOURCE_DIR}/ocp-default.ini")
if(WASM_INCLUDE_HELP)
    list(APPEND OCP_PRELOAD_DEPS "${CMAKE_SOURCE_DIR}/../ocp.hlp")
endif()
if(WASM_INCLUDE_ADPLUG_DB)
    list(APPEND OCP_PRELOAD_DEPS "${CMAKE_SOURCE_DIR}/../playopl/adplugdb-git/adplug.db")
endif()
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/CPMDLAND.DAT")
    list(APPEND OCP_PRELOAD_DEPS "${CMAKE_CURRENT_BINARY_DIR}/CPMDLAND.DAT")
endif()
set_property(TARGET ocp-wasm PROPERTY LINK_DEPENDS "${OCP_PRELOAD_DEPS}")

# Plugin target definitions are in their respective .cmake files
