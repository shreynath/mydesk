add_executable(mydesk_studio_tests test_studio.cpp)
target_link_libraries(mydesk_studio_tests PRIVATE mydesk::studio_engine)
add_test(NAME mydesk_studio_tests COMMAND mydesk_studio_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME studio_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/studio-3dsmax-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_studio.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
