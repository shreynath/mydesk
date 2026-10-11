# MyDesk Surface application target
add_executable(MyDeskSurface main.cpp)
target_link_libraries(MyDeskSurface PRIVATE mydesk::surface_engine)

set_target_properties(MyDeskSurface PROPERTIES
  OUTPUT_NAME "MyDeskSurface"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Surface"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.surface"
)

install(TARGETS MyDeskSurface BUNDLE DESTINATION . RUNTIME DESTINATION bin)
