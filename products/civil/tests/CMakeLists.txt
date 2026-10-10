add_executable(mydesk_civil_tests test_civil.cpp)
target_link_libraries(mydesk_civil_tests PRIVATE mydesk::civil_engine)
add_test(NAME mydesk_civil_tests COMMAND mydesk_civil_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME civil_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/civil-civil3d-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_civil.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
