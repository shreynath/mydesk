set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(MYDESK_DESKTOP_SOURCES
  src/main.cpp
  src/main_window.cpp
  src/canvas_widget.cpp
  src/command_line_widget.cpp
  src/layers_panel.cpp
  src/properties_panel.cpp
)

set(MYDESK_DESKTOP_HEADERS
  src/main_window.hpp
  src/canvas_widget.hpp
  src/command_line_widget.hpp
  src/layers_panel.hpp
  src/properties_panel.hpp
)

qt_add_executable(mydesk_desktop
  ${MYDESK_DESKTOP_SOURCES}
  ${MYDESK_DESKTOP_HEADERS}
)

target_include_directories(mydesk_desktop PRIVATE src)

target_link_libraries(mydesk_desktop
  PRIVATE
    Qt6::Widgets
    Qt6::Gui
    mydesk::draft
    mydesk::commands
    mydesk::document
    mydesk::exchange
    mydesk::geometry
    mydesk::units
)

set_target_properties(mydesk_desktop PROPERTIES
  OUTPUT_NAME "MyDesk"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
)

install(TARGETS mydesk_desktop BUNDLE DESTINATION . RUNTIME DESTINATION bin)
