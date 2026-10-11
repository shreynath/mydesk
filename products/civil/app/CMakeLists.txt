set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_civil_desktop STATIC
  src/main_window.cpp
  src/main_window.hpp
  src/civil_viewport.cpp
  src/civil_viewport.hpp)
target_include_directories(mydesk_civil_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mydesk_civil_desktop
  PUBLIC mydesk::civil_engine Qt6::Widgets Qt6::Gui)
target_compile_features(mydesk_civil_desktop PUBLIC cxx_std_20)

qt_add_executable(MyDeskCivil src/main.cpp)
target_link_libraries(MyDeskCivil PRIVATE mydesk_civil_desktop)
set_target_properties(MyDeskCivil PROPERTIES
  OUTPUT_NAME "MyDeskCivil"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Civil"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.civil")

install(TARGETS MyDeskCivil BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(MYDESK_BUILD_TESTS)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  qt_add_executable(mydesk_civil_ui_tests tests/test_civil_ui.cpp)
  target_link_libraries(mydesk_civil_ui_tests PRIVATE mydesk_civil_desktop Qt6::Test)
  add_test(NAME test_civil_ui_workflow COMMAND mydesk_civil_ui_tests)
  set_tests_properties(test_civil_ui_workflow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET MyDeskCivil
    OUTPUT_SCRIPT civil_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${civil_deploy_script})
endif()
