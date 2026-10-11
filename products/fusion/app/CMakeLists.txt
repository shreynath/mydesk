set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_fusion_desktop STATIC
  main_window.cpp
  main_window.hpp
  fusion_viewport.cpp
  fusion_viewport.hpp)
target_include_directories(mydesk_fusion_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_fusion_desktop PUBLIC
  mydesk::fusion_engine
  Qt6::Widgets
  Qt6::Gui)

qt_add_executable(MyDeskFusion main.cpp)
target_link_libraries(MyDeskFusion PRIVATE mydesk_fusion_desktop)

set_target_properties(MyDeskFusion PROPERTIES
  OUTPUT_NAME "MyDeskFusion"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Fusion"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.fusion"
)

install(TARGETS MyDeskFusion BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(MYDESK_BUILD_TESTS)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  qt_add_executable(mydesk_fusion_ui_tests tests/test_fusion_ui.cpp)
  target_link_libraries(mydesk_fusion_ui_tests PRIVATE mydesk_fusion_desktop Qt6::Test)
  add_test(NAME mydesk_fusion_ui_tests COMMAND mydesk_fusion_ui_tests)
  set_tests_properties(mydesk_fusion_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET MyDeskFusion
    OUTPUT_SCRIPT fusion_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${fusion_deploy_script})
endif()
