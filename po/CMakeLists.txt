include(FindGettext)

if(NOT DEFINED APP_MAIN_DOMAIN)
  set(APP_MAIN_DOMAIN "neoguvc")
endif()
if(NOT DEFINED APP_V4L2_DOMAIN)
  set(APP_V4L2_DOMAIN "${APP_MAIN_DOMAIN}_v4l2core")
endif()

add_subdirectory(gview_v4l2core)

set(_text_domain ${APP_MAIN_DOMAIN})
set(_potFile ${_text_domain}.pot)
set(_xgettext_option_list
  "--language=C"
  "--keyword=_"
  "--keyword=N_"
  "--keyword=C_:1c,2"
  "--keyword=NC_:1c,2 -s"
  "--package-name=${_text_domain}"
  "--package-version=${PRJ_VER}"
)

file(GLOB pofiles "${CMAKE_SOURCE_DIR}/po/*.po")
file(GLOB _src_list
  "${CMAKE_SOURCE_DIR}/ui/*.cpp"
  "${CMAKE_SOURCE_DIR}/ui/*.hpp"
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
  update-pot
  DEPENDS ${_potFile}
  COMMENT "Updating ${_potFile} from sources"
)
