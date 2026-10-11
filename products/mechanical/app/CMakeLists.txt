set(CMAKE_AUTOMOC ON)

add_library(mydesk_mechanical_desktop
  src/mechanical_session.cpp
  src/mechanical_session.hpp
  src/shape_viewport.cpp
  src/shape_viewport.hpp
  src/main_window.cpp
  src/main_window.hpp)
target_include_directories(mydesk_mechanical_desktop PUBLIC src)
target_link_libraries(mydesk_mechanical_desktop
  PUBLIC mydesk::mechanical_engine Qt6::Widgets Qt6::Gui)

qt_add_executable(mydesk_mechanical_app src/main.cpp)
target_link_libraries(mydesk_mechanical_app PRIVATE mydesk_mechanical_desktop)
set_target_properties(mydesk_mechanical_app PROPERTIES
  OUTPUT_NAME "MyDeskMechanical"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Mechanical"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.mechanical")
install(TARGETS mydesk_mechanical_app BUNDLE DESTINATION . RUNTIME DESTINATION bin)
