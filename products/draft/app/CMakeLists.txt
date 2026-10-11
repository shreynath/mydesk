set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(MYDESK_DRAFT_APP_SOURCES
  src/main.cpp
  src/main_window.cpp
  src/canvas_widget.cpp
  src/command_line_widget.cpp
  src/layers_panel.cpp
  src/properties_panel.cpp
)

set(MYDESK_DRAFT_APP_HEADERS
  src/main_window.hpp
  src/canvas_widget.hpp
  src/command_line_widget.hpp
  src/layers_panel.hpp
  src/properties_panel.hpp
)

qt_add_executable(mydesk_draft_app
  ${MYDESK_DRAFT_APP_SOURCES}
  ${MYDESK_DRAFT_APP_HEADERS}
)

target_include_directories(mydesk_draft_app PRIVATE src)

target_link_libraries(mydesk_draft_app
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

set_target_properties(mydesk_draft_app PROPERTIES
  OUTPUT_NAME "MyDeskDraft"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Draft"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.draft"
)

install(TARGETS mydesk_draft_app BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET mydesk_draft_app
    OUTPUT_SCRIPT draft_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${draft_deploy_script})
endif()
