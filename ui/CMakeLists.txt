cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

add_executable(guvcview
  main.cpp
  MainWindow.cpp
  ImageControlsWindow.cpp
)

target_include_directories(guvcview PRIVATE
  ${GTKMM_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/includes
  ${CMAKE_SOURCE_DIR}/gview_v4l2core
  ${CMAKE_SOURCE_DIR}/gview_audio
  ${CMAKE_SOURCE_DIR}/gview_encoder
)

add_definitions(${GTKMM_CFLAGS_OTHER})

target_link_libraries(guvcview
  gviewv4l2core
  gviewaudio
  gviewencoder
  gviewrender
  ${GTKMM_LIBRARIES}
  pthread
)

install(TARGETS guvcview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
