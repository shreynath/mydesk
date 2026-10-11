add_executable(mydesk_vault_tests test_vault.cpp)
target_link_libraries(mydesk_vault_tests PRIVATE mydesk::vault_engine)
add_test(NAME mydesk_vault_tests COMMAND mydesk_vault_tests)

if(TARGET mydesk_vault_ui)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  qt_add_executable(mydesk_vault_ui_tests test_vault_ui.cpp)
  target_link_libraries(mydesk_vault_ui_tests PRIVATE mydesk::vault_ui Qt6::Test)
  add_test(NAME mydesk_vault_ui_tests COMMAND mydesk_vault_ui_tests)
  set_tests_properties(mydesk_vault_ui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME vault_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/vault-vault-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_vault.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/test_vault_ui.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
