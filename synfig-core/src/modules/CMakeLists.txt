##
## Includes
##

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SYNFIG_BUILD_ROOT}/lib/synfig/modules)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SYNFIG_BUILD_ROOT}/lib/synfig/modules)

# for multi-config builds (e.g. msvc)
foreach(CONFIG_TYPE  ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
endforeach(CONFIG_TYPE CMAKE_CONFIGURATION_TYPES)

##
## Modules
##

## TODO: make configurable
## TODO: write some macros

set(MODS_ENABLED
    lyr_freetype
    lyr_std
    mod_bmp
    mod_dv
    mod_example
    mod_ffmpeg
    mod_filter
    mod_geometry
    mod_gif
    mod_gradient
    mod_imagemagick
    mod_jpeg
#    mod_magickpp # - made optional
    mod_noise
    mod_openexr
    mod_particle
    mod_png
    mod_ppm
    mod_svg
    mod_yuv420p
#    mptr_mplayer # - "This code has vulnerabilities"
)

if(LIBMNG_FOUND)
  list(APPEND MODS_ENABLED mod_mng)
else()
  message(STATUS "mod_mng: Disabled")
endif()

if (LIBAVFORMAT_FOUND)
	list(APPEND MODS_ENABLED mod_libavcodec)
endif()

## Magick++ support
option(WITH_MAGICKPP "Enable Magick++ module (output format support)" OFF)
option(WITHOUT_MAGICKPP "Disable Magick++ module (output format support)" OFF)

if (${WITH_MAGICKPP} AND ${WITHOUT_MAGICKPP})
    message(FATAL_ERROR "You cannot request both WITH_MAGICKPP and WITHOUT_MAGICKPP!")
endif()

if (NOT ${WITHOUT_MAGICKPP} AND TARGET PkgConfig::MAGICKPP)
    list(APPEND MODS_ENABLED mod_magickpp)
elseif (${WITH_MAGICKPP} AND NOT TARGET PkgConfig::MAGICKPP)
    message(SEND_ERROR "Magick++ support is requested, but the package cannot be located.")
endif()
## Magick++ support (end)

## Process selected modules
set(SYNFIG_MODULES_CONTENT "")

message("-- Synfig modules:")
foreach(MOD IN ITEMS ${MODS_ENABLED})
    message("--   ${MOD}")
    set(SYNFIG_MODULES_CONTENT "${SYNFIG_MODULES_CONTENT}\n${MOD}")
    add_subdirectory(${MOD})
    add_dependencies(synfig_bin ${MOD})
endforeach(MOD)

file(WRITE ${SYNFIG_BUILD_ROOT}/etc/synfig_modules.cfg ${SYNFIG_MODULES_CONTENT})

install(
    FILES ${SYNFIG_BUILD_ROOT}/etc/synfig_modules.cfg
    DESTINATION etc
)
