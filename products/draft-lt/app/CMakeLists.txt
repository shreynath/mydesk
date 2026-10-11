# MyDesk Draft LT application target
add_executable(MyDeskDraftLT
  main.cpp
)

target_link_libraries(MyDeskDraftLT
  PRIVATE
    mydesk::draft
    mydesk::commands
    mydesk::document
    mydesk::exchange
    mydesk::geometry
    mydesk::units
)

set_target_properties(MyDeskDraftLT PROPERTIES
  OUTPUT_NAME "MyDeskDraftLT"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Draft LT"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.draftlt"
)

install(TARGETS MyDeskDraftLT BUNDLE DESTINATION . RUNTIME DESTINATION bin)
