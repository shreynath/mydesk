add_library(mydesk_surface_engine
  src/surface_document.cpp
)
add_library(mydesk::surface_engine ALIAS mydesk_surface_engine)

target_include_directories(mydesk_surface_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_surface_engine PUBLIC cxx_std_20)
