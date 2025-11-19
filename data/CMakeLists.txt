option(USE_DEBIANMENU "Install a debian menu entry" OFF)
set(DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")

configure_file(neoguvc.appdata.xml.in neoguvc.appdata.xml @ONLY)
configure_file(neoguvc.desktop.in neoguvc.desktop @ONLY)

file(ARCHIVE_CREATE
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/neoguvc.1.gz
  PATHS ${CMAKE_CURRENT_SOURCE_DIR}/neoguvc.1
  FORMAT raw
  COMPRESSION GZip
)

if(USE_DEBIANMENU)
  configure_file(neoguvc.in neoguvc @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/neoguvc DESTINATION ${DATADIR}/menu)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/neoguvc.appdata.xml DESTINATION ${DATADIR}/appdata)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/neoguvc.desktop DESTINATION ${DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/neoguvc.1.gz DESTINATION ${DATADIR}/man/man1)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/neoguvc.png DESTINATION ${DATADIR}/pixmaps)
