add_executable(mydesk_draft_lt_tests test_draft_lt.cpp)
target_link_libraries(mydesk_draft_lt_tests
  PRIVATE
    mydesk::draft
    mydesk::commands
    mydesk::document
    mydesk::exchange
    mydesk::geometry
    mydesk::units
)
add_test(NAME mydesk_draft_lt_tests COMMAND mydesk_draft_lt_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME draft_lt_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/draft-lt-autocad-lt-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_draft_lt.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
