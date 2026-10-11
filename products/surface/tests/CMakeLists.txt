add_executable(mydesk_surface_tests test_surface.cpp)
target_link_libraries(mydesk_surface_tests PRIVATE mydesk::surface_engine)
add_test(NAME mydesk_surface_tests COMMAND mydesk_surface_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME surface_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/surface-alias-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_surface.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
