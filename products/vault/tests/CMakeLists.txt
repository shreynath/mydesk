add_executable(mydesk_vault_tests test_vault.cpp)
target_link_libraries(mydesk_vault_tests PRIVATE mydesk::vault_engine)
add_test(NAME mydesk_vault_tests COMMAND mydesk_vault_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME vault_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/vault-vault-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_vault.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
