set(CMAKE_C_STANDARD 11)

if(NOT DEFINED APP_MAIN_DOMAIN)
  set(APP_MAIN_DOMAIN "neoguvc")
endif()

set(LIBSOVERSION "2")
set(LIBVERSION  "2.2.0")
set(LIBOUTPUT "${APP_MAIN_DOMAIN}audio")
set(PC_OUTPUT "lib${LIBOUTPUT}.pc")

option(USE_PULSE "Enable Pulseaudio API" ON)

add_library(gviewaudio SHARED
  audio.c
  audio_fx.c
  audio_portaudio.c
  core_time.c
)

set_target_properties(
  gviewaudio PROPERTIES
  OUTPUT_NAME ${LIBOUTPUT}
  VERSION ${LIBVERSION}
  SOVERSION ${LIBSOVERSION}
)

pkg_check_modules(PORTAUDIO2 REQUIRED portaudio-2.0)

target_link_libraries(gviewaudio ${PORTAUDIO2_LIBRARIES})
add_definitions(${PORTAUDIO2_CFLAGS} ${PORTAUDIO2_CFLAGS_OTHER})

if(USE_PULSE)
  pkg_check_modules(PULSEAUDIO libpulse)
  if(PULSEAUDIO_FOUND)
    message(STATUS "pulseaudio is ON")
    target_sources(gviewaudio PRIVATE audio_pulseaudio.c)
    add_compile_definitions(HAS_PULSEAUDIO)
    target_link_libraries(gviewaudio ${PULSEAUDIO_LIBRARIES})
    add_definitions(${PULSEAUDIO_CFLAGS} ${PULSEAUDIO_CFLAGS_OTHER})
  endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/includes)

if(INSTALL_DEVKIT)
  configure_file(libgviewaudio.pc.in ${PC_OUTPUT} @ONLY)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/neoguvcaudio.h TYPE INCLUDE)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PC_OUTPUT}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/pkgconfig
  )
endif()

install(TARGETS gviewaudio)
