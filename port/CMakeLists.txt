cmake_minimum_required(VERSION 3.16)
project(renfs C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)    # for editors: port/build/compile_commands.json
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O2 -fwrapv)

# SDL2, linked dynamically (the MiSTer's hybrid framework is built around SDL2)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(RENFS_SOURCES
  src/main.c
  src/files.c
  src/unpack.c
  src/fsh.c
  src/video.c
  src/fe.c
  src/audio.c
  src/sndlib.c
  src/racesnd.c src/racekeys.c src/pause.c src/session.c src/standings.c src/racemenu.c
  src/replay.c
  src/ff.c src/ffdev.c
  src/eacs.c
  src/tgv.c
  src/movie.c
  src/sound.c
  src/config.c
  src/font.c
  src/menu.c
  src/mainmenu.c src/showcase.c src/trackrec.c
  src/options.c src/audioopt.c src/gfxopt.c src/ctrlopt.c src/loadsave.c src/credits.c src/joystick.c src/smooth.c
  src/raster.c
  src/poly.c
  src/road.c src/roaddraw.c src/sky.c src/skid.c src/view.c src/car.c src/camera.c src/dash.c src/hud.c src/race.c
  src/model.c src/cardraw.c src/polyfill.c src/drive.c src/crash.c src/smoke.c src/timing.c src/opponent.c src/collide.c
  src/track.c
  src/exe.c
  src/trackinfo.c
  src/racepal.c
  src/trackfam.c
)

# MISTER_HYBRID (branch mister): the MiSTer hybrid core (~/MiSTer-ReNFS). SDL is the
# framework's, linked statically with its "mister" drivers; src/mister.c is what the
# core adds (the OSD's options, the picture handed to the core, the pad, the cheats).
# The sums stay exact (no -ffast-math): the races and replays must be the PC's.
option(MISTER_HYBRID "Build for the MiSTer hybrid core (needs SDL2 with the MiSTer drivers)" OFF)
if(MISTER_HYBRID)
  add_executable(renfs ${RENFS_SOURCES} src/mister.c src/mister_files.c)
  target_compile_definitions(renfs PRIVATE MISTER_HYBRID=1)
  target_include_directories(renfs PRIVATE ${SDL2_STATIC_INCLUDE_DIRS})
  target_compile_options(renfs PRIVATE ${SDL2_STATIC_CFLAGS_OTHER})
  target_link_libraries(renfs PRIVATE ${SDL2_STATIC_LDFLAGS} m)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    target_compile_options(renfs PRIVATE -O3 -marm)
  endif()
else()
  add_executable(renfs ${RENFS_SOURCES})
  target_include_directories(renfs PRIVATE ${SDL2_INCLUDE_DIRS})
  target_compile_options(renfs PRIVATE ${SDL2_CFLAGS_OTHER})
  target_link_libraries(renfs PRIVATE ${SDL2_LDFLAGS})
endif()

# the version (../VERSION + commits since, see tools/version.sh), looked up at every build
add_custom_target(renfs_version
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/renfs_version.h
          -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/renfs_version.h)
add_dependencies(renfs renfs_version)
target_include_directories(renfs PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
