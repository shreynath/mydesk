set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(mydesk_coordinate_ui STATIC
  src/main_window.cpp
  src/main_window.hpp
  src/coordination_viewport.cpp
  src/coordination_viewport.hpp
)

target_include_directories(mydesk_coordinate_ui PUBLIC src)
target_link_libraries(mydesk_coordinate_ui
  PUBLIC
    Qt6::Widgets
    Qt6::Gui
    mydesk::coordinate_engine
)
target_compile_features(mydesk_coordinate_ui PUBLIC cxx_std_20)

qt_add_executable(mydesk_coordinate_app src/main.cpp)
target_link_libraries(mydesk_coordinate_app PRIVATE mydesk_coordinate_ui Qt6::Widgets)
target_compile_features(mydesk_coordinate_app PUBLIC cxx_std_20)

set_target_properties(mydesk_coordinate_app PROPERTIES
  OUTPUT_NAME "MyDeskCoordinate"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Coordinate"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.coordinate"
)

install(TARGETS mydesk_coordinate_app BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET mydesk_coordinate_app
    OUTPUT_SCRIPT coordinate_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
    INCLUDE_PLUGIN_TYPES platforms styles
  )
  install(SCRIPT ${coordinate_deploy_script})
endif()
