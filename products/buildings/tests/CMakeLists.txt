add_executable(mydesk_buildings_tests test_buildings.cpp)
target_link_libraries(mydesk_buildings_tests PRIVATE mydesk::buildings_engine)
add_test(NAME mydesk_buildings_tests COMMAND mydesk_buildings_tests)

if(TARGET mydesk_buildings_controller)
  add_executable(mydesk_buildings_controller_tests test_document_controller.cpp)
  target_link_libraries(mydesk_buildings_controller_tests PRIVATE mydesk::buildings_controller)
  add_test(NAME mydesk_buildings_controller_tests COMMAND mydesk_buildings_controller_tests)
endif()

if(TARGET mydesk_buildings_ui)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  add_executable(mydesk_buildings_ui_tests test_buildings_ui.cpp)
  target_link_libraries(mydesk_buildings_ui_tests PRIVATE
    mydesk::buildings_ui Qt6::Test)
  add_test(NAME mydesk_buildings_ui_tests COMMAND mydesk_buildings_ui_tests)
  set_tests_properties(mydesk_buildings_ui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

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
