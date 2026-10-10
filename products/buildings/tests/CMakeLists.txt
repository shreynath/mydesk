add_executable(mydesk_buildings_tests test_buildings.cpp)
target_link_libraries(mydesk_buildings_tests PRIVATE mydesk::buildings_engine)
add_test(NAME mydesk_buildings_tests COMMAND mydesk_buildings_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME buildings_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/buildings-revit-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_buildings.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
