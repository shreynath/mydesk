# MyDesk Vault application target
add_executable(MyDeskVault main.cpp)
target_link_libraries(MyDeskVault PRIVATE mydesk::vault_engine)

set_target_properties(MyDeskVault PROPERTIES
  OUTPUT_NAME "MyDeskVault"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Vault"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.vault"
)

install(TARGETS MyDeskVault BUNDLE DESTINATION . RUNTIME DESTINATION bin)
