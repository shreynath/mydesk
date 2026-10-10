add_library(mydesk_civil_engine
  src/civil_document.cpp
)
add_library(mydesk::civil_engine ALIAS mydesk_civil_engine)

target_include_directories(mydesk_civil_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(mydesk_civil_engine PUBLIC cxx_std_20)
