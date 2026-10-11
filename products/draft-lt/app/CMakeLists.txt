# MyDesk Draft LT independent desktop application target.
set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_draft_lt_ui STATIC
  main_window.cpp
  main_window.hpp
  canvas_widget.cpp
  canvas_widget.hpp
)
target_include_directories(mydesk_draft_lt_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_draft_lt_ui
  PUBLIC
    Qt6::Widgets
    Qt6::Gui
    mydesk::draft
    mydesk::commands
    mydesk::document
    mydesk::exchange
    mydesk::geometry
    mydesk::units
)

qt_add_executable(MyDeskDraftLT main.cpp)
target_link_libraries(MyDeskDraftLT
  PRIVATE
    mydesk_draft_lt_ui
)

set_target_properties(MyDeskDraftLT PROPERTIES
  OUTPUT_NAME "MyDeskDraftLT"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Draft LT"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.draftlt"
)

install(TARGETS MyDeskDraftLT BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(APPLE)
  qt_generate_deploy_app_script(
    TARGET MyDeskDraftLT
    OUTPUT_SCRIPT mydesk_draft_lt_deploy_script
    NO_PLUGINS
    NO_TRANSLATIONS)
  install(SCRIPT ${mydesk_draft_lt_deploy_script})
  get_target_property(_mydesk_draft_lt_qtpaths Qt6::qtpaths IMPORTED_LOCATION)
  execute_process(
    COMMAND "${_mydesk_draft_lt_qtpaths}" --query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE _mydesk_draft_lt_qt_plugins
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  file(REAL_PATH "${_mydesk_draft_lt_qt_plugins}/platforms/libqcocoa.dylib"
    _mydesk_draft_lt_cocoa_plugin)
  file(REAL_PATH "${_mydesk_draft_lt_qt_plugins}/styles/libqmacstyle.dylib"
    _mydesk_draft_lt_mac_style_plugin)
  install(FILES "${_mydesk_draft_lt_cocoa_plugin}"
    RENAME libqcocoa.dylib
    DESTINATION "MyDeskDraftLT.app/Contents/PlugIns/platforms")
  install(FILES "${_mydesk_draft_lt_mac_style_plugin}"
    RENAME libqmacstyle.dylib
    DESTINATION "MyDeskDraftLT.app/Contents/PlugIns/styles")
  install(CODE [[
    foreach(_plugin "platforms/libqcocoa.dylib" "styles/libqmacstyle.dylib")
      set(_plugin_path
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/MyDeskDraftLT.app/Contents/PlugIns/${_plugin}")
      execute_process(
        COMMAND /usr/bin/install_name_tool -add_rpath "@loader_path/../../Frameworks" "${_plugin_path}"
        RESULT_VARIABLE _patch_result
        ERROR_VARIABLE _patch_error)
      if(NOT _patch_result EQUAL 0)
        message(FATAL_ERROR "Could not make Draft LT Qt plugin relocatable: ${_patch_error}")
      endif()
    endforeach()
  ]])
else()
  qt_generate_deploy_app_script(
    TARGET MyDeskDraftLT
    OUTPUT_SCRIPT mydesk_draft_lt_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${mydesk_draft_lt_deploy_script})
endif()
