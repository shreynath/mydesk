# MyDesk Fusion application target
add_executable(MyDeskFusion main.cpp)
target_link_libraries(MyDeskFusion PRIVATE mydesk::fusion_engine)

set_target_properties(MyDeskFusion PROPERTIES
  OUTPUT_NAME "MyDeskFusion"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Fusion"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.fusion"
)

install(TARGETS MyDeskFusion BUNDLE DESTINATION . RUNTIME DESTINATION bin)
