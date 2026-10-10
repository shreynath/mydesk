add_executable(mydesk_coordinate_tests test_coordinate.cpp)
target_link_libraries(mydesk_coordinate_tests PRIVATE mydesk::coordinate_engine)
add_test(NAME mydesk_coordinate_tests COMMAND mydesk_coordinate_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME coordinate_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/coordinate-navisworks-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_coordinate.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
