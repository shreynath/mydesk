set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_mill_desktop STATIC
  main_window.cpp
  main_window.hpp
  mill_viewport.cpp
  mill_viewport.hpp)
target_include_directories(mydesk_mill_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_mill_desktop
  PUBLIC mydesk::mill_engine Qt6::Widgets Qt6::Gui)

qt_add_executable(MyDeskMill main.cpp)
target_link_libraries(MyDeskMill PRIVATE mydesk_mill_desktop)
set_target_properties(MyDeskMill PROPERTIES
  OUTPUT_NAME "MyDeskMill"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Mill"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.mill")

install(TARGETS MyDeskMill BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(MYDESK_BUILD_TESTS)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  qt_add_executable(mydesk_mill_ui_tests tests/test_mill_ui.cpp)
  target_link_libraries(mydesk_mill_ui_tests PRIVATE mydesk_mill_desktop Qt6::Test)
  add_test(NAME test_mill_ui_workflow COMMAND mydesk_mill_ui_tests)
  set_tests_properties(test_mill_ui_workflow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET MyDeskMill
    OUTPUT_SCRIPT mill_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${mill_deploy_script})
endif()
