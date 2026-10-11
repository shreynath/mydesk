set(CMAKE_AUTOMOC ON)

add_library(mydesk_animate_ui
  main_window.cpp main_window.hpp
  animate_viewport.cpp animate_viewport.hpp)
target_include_directories(mydesk_animate_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_animate_ui PUBLIC mydesk::animate_engine Qt6::Widgets Qt6::Gui)

qt_add_executable(MyDeskAnimate main.cpp)
target_link_libraries(MyDeskAnimate PRIVATE mydesk_animate_ui)
set_target_properties(MyDeskAnimate PROPERTIES
  MACOSX_BUNDLE TRUE WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Animate"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.animate")
install(TARGETS MyDeskAnimate BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET MyDeskAnimate OUTPUT_SCRIPT animate_deploy_script
    NO_TRANSLATIONS NO_UNSUPPORTED_PLATFORM_ERROR
    INCLUDE_PLUGIN_TYPES platforms styles)
  install(SCRIPT ${animate_deploy_script})
endif()
