add_executable(mydesk_mill_tests test_mill.cpp)
target_link_libraries(mydesk_mill_tests PRIVATE mydesk::mill_engine)
add_test(NAME mydesk_mill_tests COMMAND mydesk_mill_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME mill_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/mill-powermill-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_mill.cpp
                    ${CMAKE_SOURCE_DIR}/products/mill/app/tests/test_mill_ui.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
