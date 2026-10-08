# PlayTimidity Plugin Configuration

# TiMidity++ libarc sources
# Note: url_pipe.c excluded - WASM wrapper provides correct signature
set(TIMIDITY_LIBARC_SOURCES
    ../playtimidity/timidity-git/libarc/arc.c
    ../playtimidity/timidity-git/libarc/arc_mime.c
    ../playtimidity/timidity-git/libarc/arc_lzh.c
    ../playtimidity/timidity-git/libarc/arc_tar.c
    ../playtimidity/timidity-git/libarc/arc_zip.c
    ../playtimidity/timidity-git/libarc/deflate.c
    ../playtimidity/timidity-git/libarc/explode.c
    ../playtimidity/timidity-git/libarc/inflate.c
    ../playtimidity/timidity-git/libarc/unlzh.c
    ../playtimidity/timidity-git/libarc/url.c
    ../playtimidity/timidity-git/libarc/url_b64decode.c
    ../playtimidity/timidity-git/libarc/url_cache.c
    ../playtimidity/timidity-git/libarc/url_dir.c
    ../playtimidity/timidity-git/libarc/url_file.c
    ../playtimidity/timidity-git/libarc/url_hqxdecode.c
    ../playtimidity/timidity-git/libarc/url_inflate.c
    ../playtimidity/timidity-git/libarc/url_mem.c
    ../playtimidity/timidity-git/libarc/url_qsdecode.c
    ../playtimidity/timidity-git/libarc/url_uudecode.c
)

# TiMidity++ interface sources
set(TIMIDITY_INTERFACE_SOURCES
    ../playtimidity/timidity-git/interface/wrdt_dumb.c
)

# TiMidity++ core sources
set(TIMIDITY_CORE_SOURCES
    ../playtimidity/timidity-git/timidity/aq.c
    ../playtimidity/timidity-git/timidity/audio_cnv.c
    ../playtimidity/timidity-git/timidity/common.c
    ../playtimidity/timidity-git/timidity/effect.c
    ../playtimidity/timidity-git/timidity/filter.c
    ../playtimidity/timidity-git/timidity/freq.c
    ../playtimidity/timidity-git/timidity/instrum.c
    ../playtimidity/timidity-git/timidity/loadtab.c
    ../playtimidity/timidity-git/timidity/mfi.c
    ../playtimidity/timidity-git/timidity/miditrace.c
    ../playtimidity/timidity-git/timidity/mix.c
    ../playtimidity/timidity-git/timidity/mt19937ar.c
    ../playtimidity/timidity-git/timidity/optcode.c
    ../playtimidity/timidity-git/timidity/quantity.c
    ../playtimidity/timidity-git/timidity/rcp.c
    ../playtimidity/timidity-git/timidity/readmidi.c
    ../playtimidity/timidity-git/timidity/recache.c
    ../playtimidity/timidity-git/timidity/resample.c
    ../playtimidity/timidity-git/timidity/reverb.c
    ../playtimidity/timidity-git/timidity/sbkconv.c
    ../playtimidity/timidity-git/timidity/sffile.c
    ../playtimidity/timidity-git/timidity/sfitem.c
    ../playtimidity/timidity-git/timidity/smfconv.c
    ../playtimidity/timidity-git/timidity/smplfile.c
    ../playtimidity/timidity-git/timidity/sndfont.c
    ../playtimidity/timidity-git/timidity/tables.c
    ../playtimidity/timidity-git/timidity/timidity.c
    ../playtimidity/timidity-git/timidity/version.c
    ../playtimidity/timidity-git/timidity/wrd_read.c
    ../playtimidity/timidity-git/timidity/wrdt.c
)

# TiMidity++ utils sources
set(TIMIDITY_UTILS_SOURCES
    ../playtimidity/timidity-git/utils/mblock.c
    ../playtimidity/timidity-git/utils/fft4g.c
    ../playtimidity/timidity-git/utils/memb.c
    ../playtimidity/timidity-git/utils/nkflib.c
    ../playtimidity/timidity-git/utils/strtab.c
    ../playtimidity/timidity-git/utils/timer.c
)

# OCP wrapper sources
set(PLAYTIMIDITY_OCP_SOURCES
    ../playtimidity/cpikaraoke.c
    ../playtimidity/cpitimiditysetup.c
    ../playtimidity/timiditytype.c
    ../playtimidity/ocp-output.c
    ../playtimidity/timidityconfig.c
    ../playtimidity/timidityplay.c
    ../playtimidity/timiditypplay.c
    ../playtimidity/timiditypchan.c
    ../playtimidity/timiditypdots.c
)

set(PLAYTIMIDITY_SOURCES
    ${TIMIDITY_LIBARC_SOURCES}
    ${TIMIDITY_INTERFACE_SOURCES}
    ${TIMIDITY_CORE_SOURCES}
    ${TIMIDITY_UTILS_SOURCES}
    ${PLAYTIMIDITY_OCP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/timidity-url-pipe-wrapper.c
)

# Generate newton_table.c
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/newton_table.c
    COMMAND ${CMAKE_COMMAND} -E echo "/* Generated newton_table.c for WASM */" > ${CMAKE_CURRENT_BINARY_DIR}/newton_table.c
    COMMAND ${CMAKE_COMMAND} -E echo "/* Placeholder - table generation skipped for WASM build */" >> ${CMAKE_CURRENT_BINARY_DIR}/newton_table.c
    COMMENT "Generating placeholder newton_table.c"
)

# Create playtimidity side module
add_executable(playtimidity-module
    ${PLAYTIMIDITY_SOURCES}
    ${CMAKE_CURRENT_BINARY_DIR}/newton_table.c
)

# Set output name for side module
set_target_properties(playtimidity-module PROPERTIES
    OUTPUT_NAME "playtimidity"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

# Include directories for playtimidity
target_include_directories(playtimidity-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playtimidity
    ${CMAKE_SOURCE_DIR}/../playtimidity/timidity-git/interface
    ${CMAKE_SOURCE_DIR}/../playtimidity/timidity-git/libarc
    ${CMAKE_SOURCE_DIR}/../playtimidity/timidity-git/libunimod
    ${CMAKE_SOURCE_DIR}/../playtimidity/timidity-git/timidity
    ${CMAKE_SOURCE_DIR}/../playtimidity/timidity-git/utils
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Preprocessor definitions for side module (playtimidity)
target_compile_definitions(playtimidity-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    NO_CURSES=1
    ANOTHER_MAIN=1
    DISABLE_WRDT=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    DEFAULT_PATH="/assets/timidity"
    PKGDATADIR="/assets/timidity"
    HAVE_SDL2=1
    HAVE_ERRNO_H=1
    HAVE_LIMITS_H=1
    HAVE_STDINT_H=1
    HAVE_STRING_H=1
    HAVE_STRINGS_H=1
    HAVE_DIRENT_H=1
    HAVE_UNISTD_H=1
    HAVE_SYS_STAT_H=1
    HAVE_SYS_TYPES_H=1
    HAVE_SYS_TIME_H=1
    HAVE_GETTIMEOFDAY=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)

# Special compile flags for specific files
set_source_files_properties(
    ../playtimidity/timidity-git/timidity/timidity.c
    PROPERTIES COMPILE_FLAGS "-Dmain=timidity_main"
)

set_source_files_properties(
    ../playtimidity/timidity-git/timidity/wrdt.c
    PROPERTIES COMPILE_FLAGS "-Dtty_wrdt_mode=dumb_wrdt_mode"
)

set_source_files_properties(
    ../playtimidity/timidityplay.c
    PROPERTIES COMPILE_FLAGS "-Dmain=timidity_main"
)
