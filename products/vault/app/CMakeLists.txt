set(CMAKE_AUTOMOC ON)

qt_add_library(mydesk_vault_ui STATIC
  main_window.cpp
  main_window.hpp)
add_library(mydesk::vault_ui ALIAS mydesk_vault_ui)
target_include_directories(mydesk_vault_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mydesk_vault_ui PUBLIC mydesk::vault_engine Qt6::Widgets Qt6::Gui)
target_compile_features(mydesk_vault_ui PUBLIC cxx_std_20)

qt_add_executable(MyDeskVault main.cpp)
target_link_libraries(MyDeskVault PRIVATE mydesk::vault_ui)
set_target_properties(MyDeskVault PROPERTIES
  OUTPUT_NAME "MyDeskVault"
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "MyDesk Vault"
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.mydesk.vault")

install(TARGETS MyDeskVault BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET MyDeskVault
    OUTPUT_SCRIPT vault_deploy_script
    INCLUDE_PLUGIN_TYPES platforms styles
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${vault_deploy_script})
endif()
