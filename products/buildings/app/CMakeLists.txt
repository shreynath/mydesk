add_library(mydesk_buildings_controller src/document_controller.cpp)
add_library(mydesk::buildings_controller ALIAS mydesk_buildings_controller)
target_include_directories(mydesk_buildings_controller PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(mydesk_buildings_controller PUBLIC mydesk::buildings_engine)
target_compile_features(mydesk_buildings_controller PUBLIC cxx_std_20)

add_library(mydesk_buildings_ui
  src/main_window.cpp
  src/bim_viewport.cpp)
add_library(mydesk::buildings_ui ALIAS mydesk_buildings_ui)
target_include_directories(mydesk_buildings_ui PUBLIC include)
target_link_libraries(mydesk_buildings_ui PUBLIC
  mydesk::buildings_controller Qt6::Widgets Qt6::Gui)

qt_add_executable(mydesk_buildings_app src/main.cpp)
target_link_libraries(mydesk_buildings_app PRIVATE mydesk::buildings_ui)
set_target_properties(mydesk_buildings_app PROPERTIES
  OUTPUT_NAME "MyDeskBuildings"
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Buildings"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.mydesk.buildings")
install(TARGETS mydesk_buildings_app
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)

# Bundle this product's Qt runtime and platform plugins without introducing a
# dependency on another MyDesk application.
if(APPLE)
  # macdeployqt cannot filter plugin types. Deploying every plugin drags in
  # unrelated PDF, SVG, and virtual-keyboard frameworks, so deploy the linked
  # runtime first and add only the Cocoa platform and native style plugins.
  qt_generate_deploy_app_script(
    TARGET mydesk_buildings_app
    OUTPUT_SCRIPT mydesk_buildings_deploy_script
    NO_PLUGINS
    NO_TRANSLATIONS)
  install(SCRIPT ${mydesk_buildings_deploy_script})
  get_target_property(_mydesk_qtpaths Qt6::qtpaths IMPORTED_LOCATION)
  execute_process(
    COMMAND "${_mydesk_qtpaths}" --query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE _mydesk_qt_plugins
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  file(REAL_PATH "${_mydesk_qt_plugins}/platforms/libqcocoa.dylib"
    _mydesk_cocoa_plugin)
  file(REAL_PATH "${_mydesk_qt_plugins}/styles/libqmacstyle.dylib"
    _mydesk_mac_style_plugin)
  install(FILES "${_mydesk_cocoa_plugin}"
    RENAME libqcocoa.dylib
    DESTINATION "MyDeskBuildings.app/Contents/PlugIns/platforms")
  install(FILES "${_mydesk_mac_style_plugin}"
    RENAME libqmacstyle.dylib
    DESTINATION "MyDeskBuildings.app/Contents/PlugIns/styles")
  install(CODE [[
    foreach(_plugin
        "platforms/libqcocoa.dylib"
        "styles/libqmacstyle.dylib")
      set(_plugin_path
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/MyDeskBuildings.app/Contents/PlugIns/${_plugin}")
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
    TARGET mydesk_buildings_app
    OUTPUT_SCRIPT mydesk_buildings_deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${mydesk_buildings_deploy_script})
endif()
