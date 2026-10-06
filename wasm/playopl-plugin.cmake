# PlayOPL Plugin Configuration

set(PLAYOPL_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/playopl-generated)
set(PLAYOPL_BINIO_GENERATED_DIR ${PLAYOPL_GENERATED_DIR}/libbinio-git/src)
set(PLAYOPL_ADPLUG_INCLUDE_DIR ${PLAYOPL_GENERATED_DIR}/adplug/include)

file(MAKE_DIRECTORY "${PLAYOPL_BINIO_GENERATED_DIR}")
file(MAKE_DIRECTORY "${PLAYOPL_ADPLUG_INCLUDE_DIR}/adplug")

set(ENABLE_STRING 1)
set(ENABLE_IOSTREAM 1)
set(ISO_STDLIB 1)
set(WITH_MATH 1)
set(TYPE_INT "long long")
set(TYPE_FLOAT "long double")
configure_file(
    ${CMAKE_SOURCE_DIR}/../playopl/libbinio-git/src/binio.h.in
    ${PLAYOPL_BINIO_GENERATED_DIR}/binio.h
    @ONLY
)
unset(ENABLE_STRING)
unset(ENABLE_IOSTREAM)
unset(ISO_STDLIB)
unset(WITH_MATH)
unset(TYPE_INT)
unset(TYPE_FLOAT)

set(ADPLUG_VERSION_VALUE "2.3.3")
set(VERSION "${ADPLUG_VERSION_VALUE}")
configure_file(
    ${CMAKE_SOURCE_DIR}/../playopl/adplug-git/src/version.h.in
    ${PLAYOPL_ADPLUG_INCLUDE_DIR}/adplug/version.h
    @ONLY
)
unset(VERSION)

set(LIBBINIO_SOURCES
    ../playopl/libbinio-git/src/binfile.cpp
    ../playopl/libbinio-git/src/binio.cpp
    ../playopl/libbinio-git/src/binstr.cpp
    ../playopl/libbinio-git/src/binwrap.cpp
)

set(ADPLUG_SOURCES
    ../playopl/adplug-git/src/adlibemu.c
    ../playopl/adplug-git/src/debug.c
    ../playopl/adplug-git/src/depack.c
    ../playopl/adplug-git/src/fmopl.c
    ../playopl/adplug-git/src/nukedopl.c
    ../playopl/adplug-git/src/ungzip.c
    ../playopl/adplug-git/src/unlzh.c
    ../playopl/adplug-git/src/unlzss.c
    ../playopl/adplug-git/src/unlzw.c
    ../playopl/adplug-git/src/sixdepack.cpp
    ../playopl/adplug-git/src/a2m.cpp
    ../playopl/adplug-git/src/a2m-v2.cpp
    ../playopl/adplug-git/src/adl.cpp
    ../playopl/adplug-git/src/adplug.cpp
    ../playopl/adplug-git/src/adtrack.cpp
    ../playopl/adplug-git/src/amd.cpp
    ../playopl/adplug-git/src/analopl.cpp
    ../playopl/adplug-git/src/bam.cpp
    ../playopl/adplug-git/src/bmf.cpp
    ../playopl/adplug-git/src/cff.cpp
    ../playopl/adplug-git/src/cmf.cpp
    ../playopl/adplug-git/src/cmfmcsop.cpp
    ../playopl/adplug-git/src/coktel.cpp
    ../playopl/adplug-git/src/composer.cpp
    ../playopl/adplug-git/src/d00.cpp
    ../playopl/adplug-git/src/database.cpp
    ../playopl/adplug-git/src/dfm.cpp
    ../playopl/adplug-git/src/diskopl.cpp
    ../playopl/adplug-git/src/dmo.cpp
    ../playopl/adplug-git/src/dro2.cpp
    ../playopl/adplug-git/src/dro.cpp
    ../playopl/adplug-git/src/dtm.cpp
    ../playopl/adplug-git/src/emuopl.cpp
    ../playopl/adplug-git/src/flash.cpp
    ../playopl/adplug-git/src/fmc.cpp
    ../playopl/adplug-git/src/fprovide.cpp
    ../playopl/adplug-git/src/got.cpp
    ../playopl/adplug-git/src/herad.cpp
    ../playopl/adplug-git/src/hsc.cpp
    ../playopl/adplug-git/src/hsp.cpp
    ../playopl/adplug-git/src/hybrid.cpp
    ../playopl/adplug-git/src/hyp.cpp
    ../playopl/adplug-git/src/imf.cpp
    ../playopl/adplug-git/src/jbm.cpp
    ../playopl/adplug-git/src/kemuopl.cpp
    ../playopl/adplug-git/src/ksm.cpp
    ../playopl/adplug-git/src/lds.cpp
    ../playopl/adplug-git/src/mad.cpp
    ../playopl/adplug-git/src/mdi.cpp
    ../playopl/adplug-git/src/mid.cpp
    ../playopl/adplug-git/src/mkj.cpp
    ../playopl/adplug-git/src/msc.cpp
    ../playopl/adplug-git/src/mtk.cpp
    ../playopl/adplug-git/src/mtr.cpp
    ../playopl/adplug-git/src/mus.cpp
    ../playopl/adplug-git/src/nemuopl.cpp
    ../playopl/adplug-git/src/pis.cpp
    ../playopl/adplug-git/src/player.cpp
    ../playopl/adplug-git/src/players.cpp
    ../playopl/adplug-git/src/plx.cpp
    ../playopl/adplug-git/src/protrack.cpp
    ../playopl/adplug-git/src/psi.cpp
    ../playopl/adplug-git/src/rad2.cpp
    ../playopl/adplug-git/src/rat.cpp
    ../playopl/adplug-git/src/raw.cpp
    ../playopl/adplug-git/src/realopl.cpp
    ../playopl/adplug-git/src/rix.cpp
    ../playopl/adplug-git/src/rol.cpp
    ../playopl/adplug-git/src/s3m.cpp
    ../playopl/adplug-git/src/sa2.cpp
    ../playopl/adplug-git/src/sng.cpp
    ../playopl/adplug-git/src/sop.cpp
    ../playopl/adplug-git/src/surroundopl.cpp
    ../playopl/adplug-git/src/temuopl.cpp
    ../playopl/adplug-git/src/u6m.cpp
    ../playopl/adplug-git/src/vgm.cpp
    ../playopl/adplug-git/src/wm.cpp
    ../playopl/adplug-git/src/woodyopl.cpp
    ../playopl/adplug-git/src/xad.cpp
    ../playopl/adplug-git/src/xmi.cpp
    ../playopl/adplug-git/src/xsm.cpp
)

set(PLAYOPL_SOURCES
    ../playopl/ocpemu.cpp
    ../playopl/oplchan.cpp
    ../playopl/oplKen.cpp
    ../playopl/oplNuked.cpp
    playopl-wrapper.cpp
    ../playopl/oplpplay.cpp
    ../playopl/oplptrak.cpp
    ../playopl/oplSatoh.cpp
    ../playopl/opltype.cpp
    ../playopl/oplWoody.cpp
    playopl-config-stub.cpp
    playopl-retrowave-stub.cpp
    ${LIBBINIO_SOURCES}
    ${ADPLUG_SOURCES}
)

add_executable(playopl-module
    ${PLAYOPL_SOURCES}
)

set_target_properties(playopl-module PROPERTIES
    OUTPUT_NAME "playopl"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_include_directories(playopl-module BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/../playopl
    ${CMAKE_SOURCE_DIR}/../playopl/adplug-git/src
    ${CMAKE_SOURCE_DIR}/../playopl/libbinio-git/src
    ${PLAYOPL_GENERATED_DIR}
    ${PLAYOPL_BINIO_GENERATED_DIR}
    ${PLAYOPL_ADPLUG_INCLUDE_DIR}
    ${PLAYOPL_ADPLUG_INCLUDE_DIR}/adplug
)

target_compile_definitions(playopl-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    HAVE_MKSTEMP=1
    HAVE_STRCASECMP=1
    stricmp=strcasecmp
    OCP_MAJOR_VERSION=3
    OCP_MINOR_VERSION=0
    OCP_PATCH_VERSION=1
    VERSION=\"${ADPLUG_VERSION_VALUE}-WASM\"
)
