# MyDesk Infra application target
add_executable(MyDeskInfra main.cpp)
target_link_libraries(MyDeskInfra PRIVATE mydesk::infra_engine)

set_target_properties(MyDeskInfra PROPERTIES
  OUTPUT_NAME "MyDeskInfra"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Infra"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.infra"
)

install(TARGETS MyDeskInfra BUNDLE DESTINATION . RUNTIME DESTINATION bin)
