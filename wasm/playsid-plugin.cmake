# PlaySID Plugin Configuration

# Header bridging for config macros is now handled in source files via
# WASM_BUILD guards; no global config.h rewrite is needed here anymore.

# ---------------------------------------------------------------------------
# Toolchain detection for playsid side module asset generation
# ---------------------------------------------------------------------------
find_program(XA_EXECUTABLE xa)
if (NOT XA_EXECUTABLE)
    message(FATAL_ERROR "The 'xa' 6502 assembler is required to build the playsid WASM side module")
endif()

find_program(PERL_EXECUTABLE perl)
if (NOT PERL_EXECUTABLE)
    message(FATAL_ERROR "Perl is required to build the playsid WASM side module")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(PLAYSID_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/playsid-generated)
set(PLAYSID_RESID_OUTPUT_DIR ${PLAYSID_GENERATED_DIR}/resid)
set(PLAYSID_SIDTUNE_OUTPUT_DIR ${PLAYSID_GENERATED_DIR}/sidtune)
set(PLAYSID_RESIDFP_OUTPUT_DIR ${PLAYSID_GENERATED_DIR}/residfp)

set(PLAYSID_GENERATED_FILES)

# libresidfp and libsidplayfp now live in separate trees. Generate the
# headers beside their .in files so quoted includes resolve on a fresh checkout.
set(HAVE_BUILTIN_EXPECT 1)
set(RESIDFP_BRANCH_HINTS 1)
set(RESIDFP_INLINING 1)
set(RESIDFP_INLINE inline)
set(PACKAGE_VERSION "1.2.1")

set(LIBRESIDFP_DIR ${CMAKE_SOURCE_DIR}/../playsid/libresidfp-git)
set(LIBSIDPLAYFP_DIR ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git)

configure_file(
    ${LIBRESIDFP_DIR}/src/siddefs-fp.h.in
    ${LIBRESIDFP_DIR}/src/siddefs-fp.h
    @ONLY
)

set(LIB_MAJOR 1)
set(LIB_MINOR 2)
set(LIB_LEVEL 1)
configure_file(
    ${LIBRESIDFP_DIR}/src/residfp/sidversion.h.in
    ${LIBRESIDFP_DIR}/src/residfp/sidversion.h
    @ONLY
)

set(LIB_MAJOR 3)
set(LIB_MINOR 1)
set(LIB_LEVEL 0)
configure_file(
    ${LIBSIDPLAYFP_DIR}/src/sidplayfp/sidversion.h.in
    ${LIBSIDPLAYFP_DIR}/src/sidplayfp/sidversion.h
    @ONLY
)

configure_file(
    ${LIBSIDPLAYFP_DIR}/src/builders/sidlite-builder/sidlite/sl_defs.h.in
    ${LIBSIDPLAYFP_DIR}/src/builders/sidlite-builder/sidlite/sl_defs.h
    @ONLY
)

list(APPEND PLAYSID_GENERATED_FILES
    ${LIBRESIDFP_DIR}/src/siddefs-fp.h
    ${LIBRESIDFP_DIR}/src/residfp/sidversion.h
    ${LIBSIDPLAYFP_DIR}/src/sidplayfp/sidversion.h
    ${LIBSIDPLAYFP_DIR}/src/builders/sidlite-builder/sidlite/sl_defs.h
)

set(PLAYSID_SIDTUNE_SOURCES
    sidplayer1
    sidplayer2
)

foreach(sid_src ${PLAYSID_SIDTUNE_SOURCES})
    set(asm_path ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/sidtune/${sid_src}.a65)
    set(bin_path ${PLAYSID_SIDTUNE_OUTPUT_DIR}/${sid_src}.bin)
    add_custom_command(
        OUTPUT ${bin_path}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLAYSID_SIDTUNE_OUTPUT_DIR}
        COMMAND ${XA_EXECUTABLE} -R -G ${asm_path} -o ${sid_src}.o65
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/o65_to_hex_array.py ${CMAKE_CURRENT_BINARY_DIR}/${sid_src}.o65 ${bin_path}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/${sid_src}.o65
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${asm_path} ${CMAKE_SOURCE_DIR}/tools/o65_to_hex_array.py
        COMMENT "Assembling ${sid_src}.a65 to ${sid_src}.bin"
    )
    list(APPEND PLAYSID_GENERATED_FILES ${bin_path})
endforeach()

add_custom_command(
    OUTPUT ${PLAYSID_SIDTUNE_OUTPUT_DIR}/psiddrv.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLAYSID_SIDTUNE_OUTPUT_DIR}
    COMMAND ${XA_EXECUTABLE} -R -G ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/psiddrv.a65 -o psiddrv.o65
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/o65_to_hex_array.py ${CMAKE_CURRENT_BINARY_DIR}/psiddrv.o65 ${PLAYSID_SIDTUNE_OUTPUT_DIR}/psiddrv.bin
    COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/psiddrv.o65
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/psiddrv.a65 ${CMAKE_SOURCE_DIR}/tools/o65_to_hex_array.py
    COMMENT "Assembling psiddrv.a65 to psiddrv.bin"
)

list(APPEND PLAYSID_GENERATED_FILES ${PLAYSID_SIDTUNE_OUTPUT_DIR}/psiddrv.bin)

add_custom_target(playsid-generated DEPENDS ${PLAYSID_GENERATED_FILES})

set(PLAYSID_SOURCES
    ../playsid/cpiinfo.cpp
    ../playsid/cpisidsetup.cpp
    ../playsid/sidplay.cpp
    ../playsid/sidpplay.cpp
    ../playsid/sidconfig.c
    ../playsid/sidtype.c
    ../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite-builder.cpp
    ../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite-emu.cpp
    ../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite/ADSR.cpp
    ../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite/Filter.cpp
    ../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite/SID.cpp
    ../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite/WavGen.cpp
    ../playsid/libsidplayfp-git/src/builders/residfp-builder/residfp-builder.cpp
    ../playsid/libsidplayfp-git/src/builders/residfp-builder/residfp-emu.cpp
    ../playsid/libresidfp-git/src/Dac.cpp
    ../playsid/libresidfp-git/src/EnvelopeGenerator.cpp
    ../playsid/libresidfp-git/src/ExternalFilter.cpp
    ../playsid/libresidfp-git/src/Filter.cpp
    ../playsid/libresidfp-git/src/Filter6581.cpp
    ../playsid/libresidfp-git/src/Filter8580.cpp
    ../playsid/libresidfp-git/src/FilterModelConfig.cpp
    ../playsid/libresidfp-git/src/FilterModelConfig6581.cpp
    ../playsid/libresidfp-git/src/FilterModelConfig8580.cpp
    ../playsid/libresidfp-git/src/Integrator6581.cpp
    ../playsid/libresidfp-git/src/Integrator8580.cpp
    ../playsid/libresidfp-git/src/OpAmp.cpp
    ../playsid/libresidfp-git/src/residfp/residfp.cpp
    ../playsid/libresidfp-git/src/SID.cpp
    ../playsid/libresidfp-git/src/Spline.cpp
    ../playsid/libresidfp-git/src/State.cpp
    ../playsid/libresidfp-git/src/version.cc
    ../playsid/libresidfp-git/src/WaveformCalculator.cpp
    ../playsid/libresidfp-git/src/WaveformGenerator.cpp
    ../playsid/libresidfp-git/src/resample/SincResampler.cpp
    ../playsid/libsidplayfp-git/src/c64/c64.cpp
    ../playsid/libsidplayfp-git/src/c64/CIA/interrupt.cpp
    ../playsid/libsidplayfp-git/src/c64/CIA/mos652x.cpp
    ../playsid/libsidplayfp-git/src/c64/CIA/SerialPort.cpp
    ../playsid/libsidplayfp-git/src/c64/CIA/timer.cpp
    ../playsid/libsidplayfp-git/src/c64/CPU/mos6510.cpp
    ../playsid/libsidplayfp-git/src/c64/CPU/mos6510debug.cpp
    ../playsid/libsidplayfp-git/src/c64/CIA/tod.cpp
    ../playsid/libsidplayfp-git/src/c64/mmu.cpp
    ../playsid/libsidplayfp-git/src/c64/VIC_II/mos656x.cpp
    ../playsid/libsidplayfp-git/src/EventScheduler.cpp
    ../playsid/libsidplayfp-git/src/simpleMixer.cpp
    ../playsid/libsidplayfp-git/src/player.cpp
    ../playsid/libsidplayfp-git/src/psiddrv.cpp
    ../playsid/libsidplayfp-git/src/reloc65.cpp
    ../playsid/libsidplayfp-git/src/sidemu.cpp
    ../playsid/libsidplayfp-git/src/sidplayfp/sidbuilder.cpp
    ../playsid/libsidplayfp-git/src/sidplayfp/SidConfig.cpp
    ../playsid/libsidplayfp-git/src/sidplayfp/SidInfo.cpp
    ../playsid/libsidplayfp-git/src/sidplayfp/SidTune.cpp
    ../playsid/libsidplayfp-git/src/sidplayfp/SidTuneInfo.cpp
    ../playsid/libsidplayfp-git/src/sidtune/p00.cpp
    ../playsid/libsidplayfp-git/src/sidtune/prg.cpp
    ../playsid/libsidplayfp-git/src/sidtune/MUS.cpp
    ../playsid/libsidplayfp-git/src/sidtune/PSID.cpp
    ../playsid/libsidplayfp-git/src/sidtune/SidTuneBase.cpp
    ../playsid/libsidplayfp-git/src/sidtune/SidTuneTools.cpp
    ../playsid/libsidplayfp-api.cpp
)

# Create playsid side module
add_executable(playsid-module
    ${PLAYSID_SOURCES}
)

set_target_properties(playsid-module PROPERTIES
    OUTPUT_NAME "playsid"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

add_dependencies(playsid-module playsid-generated)

# Include thread shim to provide sequential std::thread implementation for WASM
# Note: The generated root config.h handles wasm/config.h includes
target_compile_options(playsid-module PRIVATE
    "-include" "${CMAKE_SOURCE_DIR}/playsid-thread-shim.h"
)

target_include_directories(playsid-module BEFORE PRIVATE
    ${PLAYSID_GENERATED_DIR}
    ${PLAYSID_SIDTUNE_OUTPUT_DIR}
    ${CMAKE_SOURCE_DIR}/sidplayfp-config
    ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/sidplayfp
    ${CMAKE_SOURCE_DIR}/../playsid
    ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src
    ${CMAKE_SOURCE_DIR}/../playsid/libresidfp-git/src
    ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/builders/residfp-builder
    ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/builders/sidlite-builder
    ${CMAKE_SOURCE_DIR}/../playsid/libsidplayfp-git/src/builders/sidlite-builder/sidlite
)

target_compile_definitions(playsid-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    HAVE_MKSTEMP=1
    OCP_MAJOR_VERSION=3
    OCP_MINOR_VERSION=5
    OCP_PATCH_VERSION=0
    VERSION="3.5.0+wasm.0.1.0"
    PACKAGE="libsidplayfp"
    PACKAGE_NAME="libsidplayfp"
    PACKAGE_TARNAME="libsidplayfp"
    PACKAGE_VERSION="0.0.0-wasm"
    PACKAGE_BUGREPORT=""
    PACKAGE_URL="https://github.com/OpenCubicPlayer"
)
