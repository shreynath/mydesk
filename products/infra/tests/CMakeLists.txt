add_executable(mydesk_infra_tests test_infra.cpp)
target_link_libraries(mydesk_infra_tests PRIVATE mydesk::infra_engine)
add_test(NAME mydesk_infra_tests COMMAND mydesk_infra_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME infra_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/infra-infraworks-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_infra.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
