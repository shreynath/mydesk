# MyDesk Surface application target
set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_surface_desktop STATIC
  main_window.cpp
  main_window.hpp
  surface_viewport.cpp
  surface_viewport.hpp)
target_include_directories(mydesk_surface_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_surface_desktop PUBLIC mydesk::surface_engine Qt6::Widgets Qt6::Gui)

qt_add_executable(MyDeskSurface main.cpp)
target_link_libraries(MyDeskSurface PRIVATE mydesk_surface_desktop)

set_target_properties(MyDeskSurface PROPERTIES
  OUTPUT_NAME "MyDeskSurface"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Surface"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.surface"
)

install(TARGETS MyDeskSurface BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(MYDESK_BUILD_TESTS)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  qt_add_executable(mydesk_surface_ui_tests tests/test_surface_ui.cpp)
  target_link_libraries(mydesk_surface_ui_tests PRIVATE mydesk_surface_desktop Qt6::Test)
  add_test(NAME test_surface_ui_workflow COMMAND mydesk_surface_ui_tests)
  set_tests_properties(test_surface_ui_workflow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(APPLE)
  qt_generate_deploy_app_script(
    TARGET MyDeskSurface
    OUTPUT_SCRIPT surface_deploy_script
    NO_PLUGINS
    NO_TRANSLATIONS)
  install(SCRIPT ${surface_deploy_script})
  get_target_property(_mydesk_surface_qtpaths Qt6::qtpaths IMPORTED_LOCATION)
  execute_process(
    COMMAND "${_mydesk_surface_qtpaths}" --query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE _mydesk_surface_qt_plugins
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  file(REAL_PATH "${_mydesk_surface_qt_plugins}/platforms/libqcocoa.dylib"
    _mydesk_surface_cocoa_plugin)
  file(REAL_PATH "${_mydesk_surface_qt_plugins}/styles/libqmacstyle.dylib"
    _mydesk_surface_mac_style_plugin)
  install(FILES "${_mydesk_surface_cocoa_plugin}"
    RENAME libqcocoa.dylib
    DESTINATION "MyDeskSurface.app/Contents/PlugIns/platforms")
  install(FILES "${_mydesk_surface_mac_style_plugin}"
    RENAME libqmacstyle.dylib
    DESTINATION "MyDeskSurface.app/Contents/PlugIns/styles")
  install(CODE [[
    foreach(_plugin "platforms/libqcocoa.dylib" "styles/libqmacstyle.dylib")
      set(_plugin_path
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/MyDeskSurface.app/Contents/PlugIns/${_plugin}")
      execute_process(
        COMMAND /usr/bin/install_name_tool -add_rpath "@loader_path/../../Frameworks" "${_plugin_path}"
        RESULT_VARIABLE _patch_result
        ERROR_VARIABLE _patch_error)
      if(NOT _patch_result EQUAL 0)
        message(FATAL_ERROR "Could not make Qt plugin relocatable: ${_patch_error}")
      endif()
    endforeach()
  ]])
else()
  qt_generate_deploy_app_script(
    TARGET MyDeskSurface
    OUTPUT_SCRIPT surface_deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${surface_deploy_script})
endif()
