add_executable(mydesk_fusion_tests test_fusion.cpp)
target_link_libraries(mydesk_fusion_tests PRIVATE mydesk::fusion_engine)
add_test(NAME mydesk_fusion_tests COMMAND mydesk_fusion_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME fusion_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/fusion-fusion360-current.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_fusion.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
