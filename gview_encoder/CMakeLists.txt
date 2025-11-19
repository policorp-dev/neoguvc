set(CMAKE_C_STANDARD 11)

if(NOT DEFINED APP_MAIN_DOMAIN)
  set(APP_MAIN_DOMAIN "neoguvc")
endif()

set(LIBSOVERSION "2")
set(LIBVERSION  "2.2.1")
set(LIBOUTPUT "${APP_MAIN_DOMAIN}encoder")
set(PC_OUTPUT "lib${LIBOUTPUT}.pc")

add_library(gviewencoder SHARED
  audio_codecs.c
  avi.c
  encoder.c
  file_io.c
  libav_encoder.c
  packet.c
  matroska.c
  muxer.c
  stream_io.c
  video_codecs.c
)

set_target_properties(
  gviewencoder PROPERTIES
  OUTPUT_NAME ${LIBOUTPUT}
  VERSION ${LIBVERSION}
  SOVERSION ${LIBSOVERSION}
)

pkg_check_modules(FFMPEG REQUIRED libavcodec libavutil)

target_link_libraries(gviewencoder ${FFMPEG_LIBRARIES})
add_definitions(${FFMPEG_CFLAGS} ${FFMPEG_CFLAGS_OTHER})

include_directories(${CMAKE_SOURCE_DIR}/includes)

if(INSTALL_DEVKIT)
  configure_file(libgviewencoder.pc.in ${PC_OUTPUT} @ONLY)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/neoguvcencoder.h TYPE INCLUDE)

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PC_OUTPUT}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/pkgconfig
  )
endif()

install(TARGETS gviewencoder)
