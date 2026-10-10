add_executable(mydesk_tests
  unit/test_geometry.cpp
  unit/test_document.cpp
  unit/test_commands.cpp
  unit/test_layers.cpp
  unit/test_dims_props_snaps.cpp
  unit/test_blocks.cpp
  unit/test_text_hatch.cpp
  unit/test_coordinates.cpp
  unit/test_explode.cpp
  unit/test_linetypes.cpp
  unit/test_attributes.cpp
  unit/test_layouts.cpp
  unit/test_snaps.cpp
  unit/test_transforms.cpp
  unit/test_modify.cpp
  integration/test_roundtrip.cpp
  integration/test_dxf_roundtrip.cpp
  test_main.cpp
)

target_link_libraries(mydesk_tests
  PRIVATE
    mydesk::geometry
    mydesk::document
    mydesk::commands
    mydesk::exchange
    mydesk::draft
    mydesk::modify
    mydesk::units
)

add_test(NAME mydesk_tests COMMAND mydesk_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME draft_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/draft-autocad-2025.json
            --tests ${CMAKE_SOURCE_DIR}/tests/test_main.cpp
            --root ${CMAKE_SOURCE_DIR})
  add_test(
    NAME draft_lt_capability_manifest
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/compatibility-runner/check_manifest.py
            --manifest ${CMAKE_SOURCE_DIR}/compatibility/target-manifests/draft-lt-autocad-lt-2025.json
            --tests ${CMAKE_SOURCE_DIR}/tests/test_main.cpp
            --root ${CMAKE_SOURCE_DIR})
endif()

# Convenience target used by scripts/ci
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                  DEPENDS mydesk_tests)
