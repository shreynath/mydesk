add_executable(mydesk_simulation_tests test_simulation.cpp)
target_link_libraries(mydesk_simulation_tests PRIVATE mydesk::simulation_engine)
add_test(NAME mydesk_simulation_tests COMMAND mydesk_simulation_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME simulation_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/simulation-nastran-2025.json
            --tests ${CMAKE_CURRENT_SOURCE_DIR}/test_simulation.cpp
                    ${CMAKE_SOURCE_DIR}/products/simulation/app/tests/test_simulation_ui.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()
