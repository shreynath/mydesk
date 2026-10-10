add_executable(mydesk_tests
  unit/test_geometry.cpp
  unit/test_document.cpp
  unit/test_commands.cpp
  unit/test_layers.cpp
  unit/test_dims_props_snaps.cpp
  unit/test_blocks.cpp
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

# Convenience target used by scripts/ci
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                  DEPENDS mydesk_tests)
