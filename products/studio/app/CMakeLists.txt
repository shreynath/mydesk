# MyDesk Studio application target
set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_studio_desktop STATIC
  main_window.cpp
  main_window.hpp
  studio_viewport.cpp
  studio_viewport.hpp)
target_include_directories(mydesk_studio_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_studio_desktop
  PUBLIC mydesk::studio_engine Qt6::Widgets Qt6::Gui)

qt_add_executable(MyDeskStudio main.cpp)
target_link_libraries(MyDeskStudio PRIVATE mydesk_studio_desktop)

set_target_properties(MyDeskStudio PROPERTIES
  OUTPUT_NAME "MyDeskStudio"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Studio"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.studio"
)

install(TARGETS MyDeskStudio BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(MYDESK_BUILD_TESTS)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  qt_add_executable(mydesk_studio_ui_tests tests/test_studio_ui.cpp)
  target_link_libraries(mydesk_studio_ui_tests PRIVATE mydesk_studio_desktop Qt6::Test)
  add_test(NAME test_studio_ui_workflow COMMAND mydesk_studio_ui_tests)
  set_tests_properties(test_studio_ui_workflow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET MyDeskStudio
    OUTPUT_SCRIPT studio_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${studio_deploy_script})
endif()
