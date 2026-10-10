add_library(mydesk_units
  src/units.cpp
)
add_library(mydesk::units ALIAS mydesk_units)

target_include_directories(mydesk_units
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(mydesk_units PUBLIC cxx_std_20)
