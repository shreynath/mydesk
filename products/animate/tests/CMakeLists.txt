add_executable(mydesk_animate_tests test_animate.cpp)
target_link_libraries(mydesk_animate_tests PRIVATE mydesk::animate_engine)
add_test(NAME mydesk_animate_tests COMMAND mydesk_animate_tests)

if(TARGET mydesk_animate_ui)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  add_executable(mydesk_animate_ui_tests test_animate_ui.cpp)
  target_link_libraries(mydesk_animate_ui_tests PRIVATE mydesk_animate_ui Qt6::Test)
  add_test(NAME mydesk_animate_ui_tests COMMAND mydesk_animate_ui_tests)
  set_tests_properties(mydesk_animate_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME animate_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/animate-maya-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_animate.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
