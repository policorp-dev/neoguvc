set(CMAKE_C_STANDARD 11)

option(USE_GTK3 "Enable Gtk3 gui" ON)

add_compile_definitions(GETTEXT_PACKAGE="guvcview")
add_compile_definitions(PACKAGE_DATA_DIR="${CMAKE_INSTALL_DATADIR}")
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

add_executable(guvcview
  config.c
  core_io.c
  gui.c
  guvcview.c
  options.c
  video_capture.c
)

if(USE_GTK3)
  pkg_check_modules(GTK3 gtk+-3.0)
  if(GTK3_FOUND)
    message(STATUS "Gtk3 is ON")
    add_compile_definitions(ENABLE_GTK3)
    target_link_libraries(guvcview ${GTK3_LIBRARIES})
    add_definitions(${GTK3_CFLAGS} ${GTK3_CFLAGS_OTHER})
    target_sources(guvcview PRIVATE
      gui_gtk3_audioctrls.c
      gui_gtk3.c
      gui_gtk3_callbacks.c
      gui_gtk3_h264ctrls.c
      gui_gtk3_menu.c
      gui_gtk3_v4l2ctrls.c
      gui_gtk3_videoctrls.c)
  endif()
endif()

if(GL_FOUND)
  message(STATUS "linking guvcview to ${GL_LIBRARIES}")
  target_link_libraries(guvcview ${GL_LIBRARIES})
endif()

include_directories(
  ${CMAKE_SOURCE_DIR}/includes
  ${CMAKE_SOURCE_DIR}/gview_audio
  ${CMAKE_SOURCE_DIR}/gview_encoder
  ${CMAKE_SOURCE_DIR}/gview_render
  ${CMAKE_SOURCE_DIR}/gview_v4l2core)

target_link_libraries(guvcview
  gviewaudio
  gviewencoder
  gviewrender
  gviewv4l2core)

install(TARGETS guvcview)
