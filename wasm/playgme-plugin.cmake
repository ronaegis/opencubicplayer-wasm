# PlayGME Plugin Configuration

set(LIBGME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/gme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ay_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ay_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ay_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Blip_Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Classic_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Data_Reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Dual_Resampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Effects_Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Fir_Resampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Gb_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Gb_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Gb_Oscs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Gbs_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Gme_File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Gym_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Hes_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Hes_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Hes_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Kss_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Kss_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Kss_Scc_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/M3u_Playlist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Multi_Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Music_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Fds_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Fme7_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Namco_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Oscs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Vrc6_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nes_Vrc7_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nsf_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Nsfe_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Sap_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Sap_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Sap_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Sms_Apu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Snes_Spc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Spc_Cpu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Spc_Dsp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Spc_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Spc_Filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Vgm_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Vgm_Emu_Impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ym2413_Emu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ym2612_GENS.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ym2612_MAME.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/Ym2612_Nuked.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/ext/emu2413.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme/ext/panning.c
)

set(PLAYGME_SOURCES
    ../playgme/cpiinfo.c
    ../playgme/gmeplay.c
    ../playgme/gmepplay.c
    ../playgme/gmetype.c
    ${LIBGME_SOURCES}
)

add_executable(playgme-module
    ${PLAYGME_SOURCES}
)

set_target_properties(playgme-module PROPERTIES
    OUTPUT_NAME "playgme"
    SUFFIX ".wasm"
    LINK_FLAGS "${SIDE_MODULE_LINKER_FLAGS}"
)

target_include_directories(playgme-module PRIVATE
    ${CMAKE_SOURCE_DIR}/../playgme
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme
    ${CMAKE_CURRENT_SOURCE_DIR}/libgme/gme
)

target_compile_definitions(playgme-module PRIVATE
    WASM_BUILD=1
    HAVE_CONFIG_H=1
    OPENCUBICPLAYER_SYSCONFDIR="/assets"
    OPENCUBICPLAYER_DOCDIR="/assets"
    OPENCUBICPLAYER_DATADIR="/assets"
    HAVE_SDL2=1
    HAVE_ZLIB_H=1
    BLARGG_LITTLE_ENDIAN=1
    OCP_MAJOR_VERSION=3
    VERSION="3.0.1-WASM"
)

target_link_options(playgme-module PRIVATE
    "-sUSE_ZLIB=1"
)
