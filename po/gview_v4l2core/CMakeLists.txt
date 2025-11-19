if(NOT DEFINED APP_V4L2_DOMAIN)
  set(APP_V4L2_DOMAIN "neoguvc_v4l2core")
endif()

set(_potFile ${APP_V4L2_DOMAIN}.pot)
set(_xgettext_option_list
  "--language=C"
  "--keyword=_"
  "--keyword=N_"
  "--keyword=C_:1c,2"
  "--keyword=NC_:1c,2 -s"
  "--package-name=${APP_V4L2_DOMAIN}"
)

file(GLOB pofiles "${CMAKE_SOURCE_DIR}/po/gview_v4l2core/*.po")
file(GLOB _src_list
  "${CMAKE_SOURCE_DIR}/gview_v4l2core/*.c"
)

gettext_create_translations(${_potFile} ALL ${pofiles})

find_program(XGETTEXT_EXECUTABLE xgettext)

add_custom_command(
  OUTPUT ${_potFile}
  COMMAND ${XGETTEXT_EXECUTABLE}
    ${_xgettext_option_list} -o ${_potFile} ${_src_list}
  DEPENDS ${_src_list}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Extract translatable messages to ${_potFile}"
)

add_custom_target(
  update-v4l2core-pot
  DEPENDS ${_potFile}
  COMMENT "Updating ${_potFile} from sources"
)
