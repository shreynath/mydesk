add_library(mydesk_geometry
  src/vec2.cpp
  src/bbox.cpp
  src/segment.cpp
)
add_library(mydesk::geometry ALIAS mydesk_geometry)

target_include_directories(mydesk_geometry
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(mydesk_geometry PUBLIC cxx_std_20)
