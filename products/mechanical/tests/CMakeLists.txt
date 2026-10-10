add_executable(mydesk_mechanical_tests test_mechanical.cpp)
target_link_libraries(mydesk_mechanical_tests PRIVATE mydesk::mechanical_engine)
add_test(NAME mydesk_mechanical_tests COMMAND mydesk_mechanical_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME mechanical_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/mechanical-inventor-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_mechanical.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
