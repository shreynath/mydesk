# MyDesk Studio application target
add_executable(MyDeskStudio main.cpp)
target_link_libraries(MyDeskStudio PRIVATE mydesk::studio_engine)

set_target_properties(MyDeskStudio PROPERTIES
  OUTPUT_NAME "MyDeskStudio"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Studio"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.studio"
)

install(TARGETS MyDeskStudio BUNDLE DESTINATION . RUNTIME DESTINATION bin)
