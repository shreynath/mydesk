add_library(mydesk_simulation_engine
  src/simulation_document.cpp
)
add_library(mydesk::simulation_engine ALIAS mydesk_simulation_engine)

target_include_directories(mydesk_simulation_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_simulation_engine PUBLIC cxx_std_20)
